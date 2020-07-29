#ifndef __MEMUTILS_H__
#define __MEMUTILS_H__

#include <sys/mman.h>
#include <stdint.h>

// Allocates the requested number of bytes at the requested address.
// If the allocation is impossible (due to overlap with existing mapping) - returns NULL.
// Otherwise - returns the requested address.
// If the requested number of bytes is not a multiple of page size, additional bytes will be allocated to fit a whole page.
void* allocate_at_addr(void* addr, uint32_t size_in_bytes);

// Frees an address allocated using the previous function.
void free_at_addr(void* ptr, uint32_t size_in_bytes);

#endif