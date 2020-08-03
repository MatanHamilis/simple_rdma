#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "verbs_wrappers.h"
#include "latency_measure.h"

static void sigint_handler(int value);
static volatile int keep_running = 1;

void logic_latency(struct ibv_qp* qp, ConnectionInfoExchange* peer_info, void* local_buf, uint32_t lkey)
{
    __sighandler_t prev = signal(SIGINT, sigint_handler);
    if (SIG_ERR == prev)
    {
        log_msg("Failed to set signal. Leaving...");
        exit(-1);
    }
    log_msg("Performing the attack infinitely use Ctrl+C (SIGINT) to stop the attack...");
    struct timespec start_time;
    struct timespec end_time;
    uint64_t i = 0;
    while (keep_running)
    {
        clock_gettime(CLOCK_REALTIME, &start_time);
        do_rdma_read(peer_info->mrs[0].remote_addr, local_buf, peer_info->mrs[0].rkey, lkey, 1, qp);
        do_cq_empty(qp, 1);
        clock_gettime(CLOCK_REALTIME, &end_time);
        long diff = (end_time.tv_sec - start_time.tv_sec)*1000000000 + (end_time.tv_nsec - start_time.tv_nsec);
        log_msg("%10llu) %u", i, diff/1000);
        usleep(1000000); // Sleeping to ensure the cache is flushed.
        ++i;
    }
    prev = signal(SIGINT, prev);
    if (SIG_ERR == prev)
    {
        log_msg("Failed to set signal. Leaving...");
        exit(-1);
    }
}

static void sigint_handler(int value)
{
    keep_running = 0;
}