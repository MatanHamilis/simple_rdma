#ifndef __LATENCY_MEASURE_H__
#define __LATENCY_MEASURE_H__

#include "cm.h"
#include "logging.h"
#include "verbs_wrappers.h"

void logic_latency(struct ibv_qp* qp, ConnectionInfoExchange* peer_info, void* local_buf, uint32_t lkey);

#endif