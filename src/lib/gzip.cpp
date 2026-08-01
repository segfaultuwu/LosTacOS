#include "LTOS/lib/gzip.hpp"
#include <string.h>

namespace gzip {

bool is_gzip(const void *data, size_t size) {
  if (!data || size < 10)
    return false;
  const uint8_t *buf = (const uint8_t *)data;
  return buf[0] == 0x1f && buf[1] == 0x8b;
}

// Full DEFLATE Inflater (RFC 1951)
struct State {
  const uint8_t *in;
  size_t in_len;
  size_t in_pos;
  uint32_t bitbuf;
  int bitcnt;
  uint8_t *out;
  size_t out_len;
  size_t out_pos;
};

static uint32_t get_bits(State *s, int n) {
  while (s->bitcnt < n) {
    if (s->in_pos < s->in_len) {
      s->bitbuf |= ((uint32_t)s->in[s->in_pos++]) << s->bitcnt;
    }
    s->bitcnt += 8;
  }
  uint32_t val = s->bitbuf & ((1U << n) - 1);
  s->bitbuf >>= n;
  s->bitcnt -= n;
  return val;
}

struct Huffman {
  int counts[16];
  int symbols[288];
};

static void build_huffman(Huffman *h, const uint8_t *lengths, int num) {
  memset(h->counts, 0, sizeof(h->counts));
  for (int i = 0; i < num; i++) {
    if (lengths[i]) h->counts[lengths[i]]++;
  }
  int offsets[16];
  offsets[1] = 0;
  for (int i = 1; i < 15; i++) {
    offsets[i + 1] = offsets[i] + h->counts[i];
  }
  for (int i = 0; i < num; i++) {
    if (lengths[i]) {
      h->symbols[offsets[lengths[i]]++] = i;
    }
  }
}

static int decode_symbol(State *s, const Huffman *h) {
  int code = 0, count = 0, first = 0, index = 0;
  for (int len = 1; len <= 15; len++) {
    code = (code << 1) | get_bits(s, 1);
    count = h->counts[len];
    if (code - first < count) {
      return h->symbols[index + (code - first)];
    }
    index += count;
    first = (first + count) << 1;
  }
  return -1;
}

static const uint16_t lens[] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t lext[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};

static const uint16_t dists[] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t dext[] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static bool inflate_block(State *s, const Huffman *lh, const Huffman *dh) {
  while (1) {
    int sym = decode_symbol(s, lh);
    if (sym < 0 || sym > 285) return false;
    if (sym < 256) {
      if (s->out_pos >= s->out_len) return false;
      s->out[s->out_pos++] = (uint8_t)sym;
    } else if (sym == 256) {
      break; // End of block
    } else {
      sym -= 257;
      int len = lens[sym] + get_bits(s, lext[sym]);
      int dsym = decode_symbol(s, dh);
      if (dsym < 0 || dsym > 29) return false;
      int dist = dists[dsym] + get_bits(s, dext[dsym]);
      if ((size_t)dist > s->out_pos) return false;
      for (int i = 0; i < len; i++) {
        if (s->out_pos >= s->out_len) return false;
        s->out[s->out_pos] = s->out[s->out_pos - dist];
        s->out_pos++;
      }
    }
  }
  return true;
}

bool decompress(const void *src, size_t src_size, void *dest, size_t *dest_size) {
  if (!is_gzip(src, src_size))
    return false;

  const uint8_t *buf = (const uint8_t *)src;
  uint8_t flags = buf[3];
  size_t offset = 10;

  if (flags & 0x04) { // FEXTRA
    if (offset + 2 > src_size) return false;
    uint16_t xlen = buf[offset] | (buf[offset + 1] << 8);
    offset += 2 + xlen;
  }
  if (flags & 0x08) { // FNAME
    while (offset < src_size && buf[offset] != 0) offset++;
    offset++;
  }
  if (flags & 0x10) { // FCOMMENT
    while (offset < src_size && buf[offset] != 0) offset++;
    offset++;
  }
  if (flags & 0x02) offset += 2; // FHCRC

  if (offset >= src_size) return false;

  State s = {
      .in = buf + offset,
      .in_len = src_size - offset - 8,
      .in_pos = 0,
      .bitbuf = 0,
      .bitcnt = 0,
      .out = (uint8_t *)dest,
      .out_len = dest_size ? *dest_size : 0xFFFFFFFF,
      .out_pos = 0,
  };

  int final_block = 0;
  while (!final_block) {
    final_block = get_bits(&s, 1);
    int btype = get_bits(&s, 2);

    if (btype == 0) { // Stored block
      s.bitbuf = 0;
      s.bitcnt = 0;
      if (s.in_pos + 4 > s.in_len) break;
      uint16_t len = s.in[s.in_pos] | (s.in[s.in_pos + 1] << 8);
      s.in_pos += 4;
      if (s.in_pos + len > s.in_len) break;
      for (uint16_t i = 0; i < len && s.out_pos < s.out_len; i++) {
        s.out[s.out_pos++] = s.in[s.in_pos++];
      }
    } else if (btype == 1) { // Fixed Huffman
      uint8_t lengths[288];
      for (int i = 0; i < 144; i++) lengths[i] = 8;
      for (int i = 144; i < 256; i++) lengths[i] = 9;
      for (int i = 256; i < 280; i++) lengths[i] = 7;
      for (int i = 280; i < 288; i++) lengths[i] = 8;
      Huffman lh, dh;
      build_huffman(&lh, lengths, 288);
      uint8_t dlengths[32];
      for (int i = 0; i < 32; i++) dlengths[i] = 5;
      build_huffman(&dh, dlengths, 32);
      if (!inflate_block(&s, &lh, &dh)) break;
    } else if (btype == 2) { // Dynamic Huffman
      int hlit = get_bits(&s, 5) + 257;
      int hdist = get_bits(&s, 5) + 1;
      int hclen = get_bits(&s, 4) + 4;
      static const uint8_t clorder[] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
      uint8_t clens[19] = {0};
      for (int i = 0; i < hclen; i++) clens[clorder[i]] = (uint8_t)get_bits(&s, 3);
      Huffman ch;
      build_huffman(&ch, clens, 19);
      uint8_t lengths[320];
      int num = 0;
      while (num < hlit + hdist) {
        int sym = decode_symbol(&s, &ch);
        if (sym < 16) {
          lengths[num++] = (uint8_t)sym;
        } else if (sym == 16) {
          int copy = get_bits(&s, 2) + 3;
          uint8_t val = num > 0 ? lengths[num - 1] : 0;
          while (copy--) lengths[num++] = val;
        } else if (sym == 17) {
          int copy = get_bits(&s, 3) + 3;
          while (copy--) lengths[num++] = 0;
        } else if (sym == 18) {
          int copy = get_bits(&s, 7) + 11;
          while (copy--) lengths[num++] = 0;
        }
      }
      Huffman lh, dh;
      build_huffman(&lh, lengths, hlit);
      build_huffman(&dh, lengths + hlit, hdist);
      if (!inflate_block(&s, &lh, &dh)) break;
    } else {
      break;
    }
  }

  if (dest_size)
    *dest_size = s.out_pos;

  return s.out_pos > 0;
}

} // namespace gzip
