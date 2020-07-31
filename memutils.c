#include <memutils.h>
#include <stdlib.h>
#include <logging.h>

void* allocate_at_addr(void* addr, uint32_t size_in_bytes)
{
    log_msg("Allocating memory at: %p, of size: %u", addr, size_in_bytes);
    void* allocated_addr = mmap(addr, size_in_bytes, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (allocated_addr == MAP_FAILED)
    {
        log_msg("Failed to allocate at specified address!");
        return NULL;
    }

    if (allocated_addr != addr) 
    {
        if (-1 == munmap(allocated_addr, size_in_bytes))
        {
            log_msg("Catastrophic error happened with memory management - leaving!");
            exit(-1);
        }
        return NULL;
    }
    return allocated_addr;
}

void free_at_addr(void* ptr, uint32_t size_in_bytes)
{
    if (-1 == munmap(ptr, size_in_bytes))
    {
        log_msg("Catastrophic error happened with memory management - leaving!");
        exit(-1);
    }

    return 0;
}

void* do_malloc(uint64_t bytes)
{
    void* buf = malloc(bytes);
    if (NULL == buf)
    {
        log_msg("Failed to malloc %llu bytes", bytes);
        exit(-1);
    }
    return buf;
}