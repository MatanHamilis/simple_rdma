#ifndef __CACHE_EXHAUSTER_H__
#define __CACHE_EXHAUSTER_H__

#include "verbs_wrappers.h"
#include "logging.h"
#include "cm.h"

const unsigned int PAGE_SIZE;
const unsigned int PREFETCH_GROUP_SIZE;
const unsigned int LOWER_INDEX_FIRST_BIT;
const unsigned int LOWER_INDEX_LAST_BIT;
const unsigned int UPPER_INDEX_FIRST_BIT;
const unsigned int UPPER_INDEX_LAST_BIT;
#define SERVER_NUMBER_OF_MRS \
	((1<<(LOWER_INDEX_LAST_BIT + 1 - LOWER_INDEX_FIRST_BIT)) + \
	(1<<(UPPER_INDEX_LAST_BIT + 1 - UPPER_INDEX_FIRST_BIT)) - 1)

void logic_attacker(struct ibv_qp* qp, ConnectionInfoExchange* peer_info, void* local_buf, uint32_t lkey);
#endif