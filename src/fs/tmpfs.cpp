#include "LTOS/fs/tmpfs.hpp"
#include "LTOS/fs/vfs.hpp"
#include "LTOS/mm/heap.hpp"

using namespace fs::vfs;

namespace fs::tmpfs {

Node *create_file(const char *name) {
  Node *node = (Node *)heap::kmalloc(sizeof(Node));

  TmpFile *tmp = (TmpFile *)heap::kmalloc(sizeof(TmpFile));

  tmp->data = nullptr;
  tmp->size = 0;

  node->name = name;
  node->directory = false;
  node->file = (File *)heap::kmalloc(sizeof(File));

  node->file->private_data = tmp;

  node->next = root->children;
  root->children = node;

  return node;
}

int write(File *file, const uint8_t *data, size_t size) {
  TmpFile *tmp = (TmpFile *)file->private_data;

  tmp->data = (uint8_t *)heap::kmalloc(size);

  for (size_t i = 0; i < size; i++)
    tmp->data[i] = data[i];

  tmp->size = size;

  return size;
}

char *read(File *file) {
  TmpFile *tmp = (TmpFile *)file->private_data;

  return (char *)tmp->data;
}

} // namespace fs::tmpfs
