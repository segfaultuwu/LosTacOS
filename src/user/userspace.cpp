
#include "LTOS/user/userspace.hpp"
#include "LTOS/arch/x86_64/paging.hpp"
#include <string.h>

namespace user {

extern "C" void enter_userspace(void *entry, void *stack);

uint8_t user_code[] = {
    0xEB, 0xFE // jmp $
};

void run_user() {
  uint64_t *pml4 = paging::create_address_space();

  uint64_t code_pa = (uint64_t)paging::alloc_page();
  memcpy((void *)code_pa, user_code, sizeof(user_code));

  paging::map_page(pml4, 0x400000, code_pa, PAGE_PRESENT | PAGE_USER);

  uint64_t stack_pa = (uint64_t)paging::alloc_page();

  paging::map_page(pml4, 0x800000, stack_pa,
                   PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

  asm volatile("mov %0, %%cr3" ::"r"(pml4));

  enter_userspace((void *)0x400000, (void *)(0x800000 + 0x1000));
}
} // namespace user
