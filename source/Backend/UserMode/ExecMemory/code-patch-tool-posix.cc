
#include "dobby_internal.h"
#include "core/arch/Cpu.h"

#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#if !defined(__APPLE__)
PUBLIC MemoryOperationError DobbyCodePatch(void *address, uint8_t *buffer, uint32_t buffer_size) {
#if defined(__ANDROID__) || defined(__linux__)
  if (address == nullptr || buffer == nullptr || buffer_size == 0) {
    return kMemoryOperationError;
  }

  int page_size = (int)sysconf(_SC_PAGESIZE);
  uintptr_t patch_page = ALIGN_FLOOR(address, page_size);
  uintptr_t patch_end_page = ALIGN_FLOOR((uintptr_t)address + buffer_size - 1, page_size);

  auto protect_pages = [page_size, patch_page, patch_end_page](int prot) -> bool {
    for (uintptr_t page = patch_page; page <= patch_end_page; page += page_size) {
      if (mprotect((void *)page, page_size, prot) != 0) {
        ERROR_LOG("mprotect(%p, 0x%x, 0x%x) failed: errno=%d", (void *)page, page_size, prot, errno);
        return false;
      }
    }
    return true;
  };

  // Android 16 system_server may reject RWX transitions. Patch through a
  // writable non-executable window and restore executable permission after.
  if (!protect_pages(PROT_READ | PROT_WRITE)) {
    return kMemoryOperationError;
  }

  // patch buffer
  memcpy(address, buffer, buffer_size);

  // restore page permission
  if (!protect_pages(PROT_READ | PROT_EXEC)) {
    return kMemoryOperationError;
  }

  addr_t clear_start_ = (addr_t)address;
  ClearCache((void *)clear_start_, (void *)(clear_start_ + buffer_size));
#endif
  return kMemoryOperationSuccess;
}

#endif
