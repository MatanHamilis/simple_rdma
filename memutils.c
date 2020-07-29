#include <memutils.h>
#include <stdlib.h>
#include <logging.h>

void* allocate_at_addr(void* addr, uint32_t size_in_bytes)
{
    void* allocated_addr = mmap(addr, size_in_bytes, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
    if (allocated_addr == MAP_FAILED)
    {
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