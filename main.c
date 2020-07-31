/* 
In this file I'm going to implement simple RDMA program, testing the latency of various RDMA operations.
The program is based on the libibverbs
*/

#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <infiniband/verbs.h>
#include <errno.h>
#include <getopt.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "memutils.h"
#include "cache_exhauster.h"
#include "verbs_wrappers.h"
#include "logging.h"
#include "cm.h"

const unsigned int CQE_SIZE = 10;
const unsigned int CLIENT_BUF_SIZE = 1;

#define SERVER_BUFFER_SIZE (PAGE_SIZE * PREFETCH_GROUP_SIZE)


void release_memlock_limits();
int do_server(uint16_t port_no);
int do_client(char* server_addr, uint16_t port_no);
void setup_qp(uint32_t qp_num, uint16_t port_lid, struct ibv_qp* qp);
void print_help(char* prog_name);

int main(int argc, char** argv)
{
	release_memlock_limits();
	uint16_t port = 12345;
	char* server_addr = NULL;
	int c;
	while ((c = getopt(argc,argv,"p:a:h")) != -1) 
	{
		switch(c)
		{
			case 'h':
				print_help(argv[0]);
				exit(-1);
				break;
			case 'a':
				server_addr	= optarg;
				break;
			case 'p':
				port = strtol(optarg, NULL, 10);
				break;
			default:
				print_help(argv[0]);
				exit(-1);
				break;
		}
	}
	if (optind != argc)
	{
		print_help(argv[0]);
		exit(-1);
	}	
	if (server_addr == NULL)
	{
		log_msg("I'm a server! Listening on port: %hu", port);
		return do_server(port);
	}
	log_msg("I'm a client. Connectiong to: %s:%hu", server_addr, port);
	return do_client(server_addr, port);
}

void print_help(char* prog_name)
{
	log_msg("Usage: %s [-a server_addr] [-p port] [-h]", prog_name);
	log_msg("\t -h - print this help and exit");
	log_msg("\t -a - set to client mode and specify the server's IP address, otherwise - server mode.");
	log_msg("\t -p - specify the port number to connect to (default: 12345)");
}

int do_client(char* server_addr, uint16_t port)
{
	int ans = 0;
	int client_sock = do_connect_client(port, server_addr);
	ConnectionInfoExchange* peer_info = receive_info_from_peer(client_sock);
	struct ibv_context* dev_ctx = get_dev_context();
	struct ibv_comp_channel* ch = create_comp_channel(dev_ctx);
	struct ibv_cq* cq_with_ch = create_cq(dev_ctx, CQE_SIZE, NULL, ch, 0);
	struct ibv_qp_init_attr qp_attrs = create_qp_init_attr(cq_with_ch);

	void* buf = alloc_mr(CLIENT_BUF_SIZE);
	struct ibv_pd* pd = alloc_pd(dev_ctx);
	struct ibv_qp* qp = create_qp(pd, &qp_attrs);
	struct ibv_mr* mr = register_mr(pd, buf, CLIENT_BUF_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ);
	send_info_to_peer(client_sock, qp, dev_ctx, &mr, 1);
	setup_qp(peer_info->header.qp_num, peer_info->header.port_lid, qp);

	do_sync(client_sock);
	logic_attacker(qp, peer_info, buf, mr->lkey);
	do_sync(client_sock);

	dereg_mr(mr);
	destroy_qp(qp);
	dealloc_pd(pd);

	free(peer_info);
	free(buf);
	destroy_cq(cq_with_ch);
	destroy_comp_channel(ch);
	do_close_device(dev_ctx);
	return 0;
}

int do_server(uint16_t port_no)
{	
	int retval = ibv_fork_init();
	if (0 != retval)
	{
		log_msg("Failed! ibv_fork_init returned %d", retval);
		exit(-1);
	}

	struct ibv_context* dev_ctx = get_dev_context();
	struct ibv_pd* pd = alloc_pd(dev_ctx);
	struct ibv_mr* mrs[SERVER_NUMBER_OF_MRS];

	int mr_idx = 0;
	const uint64_t BASE_OFFSET_LOWER = ((uint64_t)1)<<(UPPER_INDEX_LAST_BIT + 9);
	const uint64_t BASE_OFFSET_UPPER = ((uint64_t)1)<<(UPPER_INDEX_LAST_BIT + 10);
	for (uint64_t i = 0 ; i < 1<<(LOWER_INDEX_LAST_BIT + 1) ; i += 1<<(LOWER_INDEX_FIRST_BIT))
	{
		log_msg("Registering MR id: %d", mr_idx);
		void* buf = allocate_at_addr((void*)(BASE_OFFSET_LOWER + i), SERVER_BUFFER_SIZE);
		mrs[mr_idx] = register_mr(pd, buf, SERVER_BUFFER_SIZE, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ);
		++mr_idx;
	}

	for (uint64_t i = 0 ; i < ((uint64_t)1)<<(UPPER_INDEX_LAST_BIT + 1) ; i += 1<<(UPPER_INDEX_FIRST_BIT))
	{
		log_msg("Registering MR id: %d", mr_idx);
		void* buf = allocate_at_addr((void*)(BASE_OFFSET_UPPER + i), SERVER_BUFFER_SIZE);
		mrs[mr_idx] = register_mr(pd, buf, SERVER_BUFFER_SIZE, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ);
		++mr_idx;
	}

	struct ibv_comp_channel* ch = create_comp_channel(dev_ctx);
	struct ibv_cq* cq_with_ch = create_cq(dev_ctx, CQE_SIZE, NULL, ch, 0);
	struct ibv_cq* cq_no_ch = create_cq(dev_ctx, CQE_SIZE, NULL, NULL, 0);

	struct ibv_qp_init_attr qp_attrs = create_qp_init_attr(cq_with_ch);
	struct ibv_qp* qp = create_qp(pd, &qp_attrs);

	int server_sock = do_connect_server(port_no);
	send_info_to_peer(server_sock, qp, dev_ctx, mrs, SERVER_NUMBER_OF_MRS);
	ConnectionInfoExchange* peer_info = receive_info_from_peer(server_sock);
	setup_qp(peer_info->header.qp_num, peer_info->header.port_lid, qp);

	do_sync(server_sock);
	log_msg("Waiting for client to finish his attack now...");
	do_sync(server_sock);

	destroy_qp(qp); 
	destroy_cq(cq_no_ch);
	destroy_cq(cq_with_ch);
	destroy_comp_channel(ch);

	for (unsigned int i = 0 ; i < SERVER_NUMBER_OF_MRS ; ++i)
	{
		free(mrs[i]->addr);
		dereg_mr(mrs[i]);
	}

	dealloc_pd(pd);
	free(peer_info);
	do_close_device(dev_ctx);
}

void setup_qp(uint32_t qp_num, uint16_t port_lid, struct ibv_qp* qp)
{
	struct ibv_qp_attr attr;

	//RESET -> INIT 
	attr.qp_state = IBV_QPS_INIT;
	attr.pkey_index = 0;
	attr.port_num = 1;
	attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE |  IBV_ACCESS_REMOTE_READ;
	if (0 != ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS))
	{
		log_msg("Failed to change states: RESET -> INIT");
		exit(-1);
	}

	log_msg("RESET -> INIT QP Set Successfully");
	attr.qp_state = IBV_QPS_RTR;
	attr.path_mtu = IBV_MTU_512;
	attr.ah_attr.dlid = port_lid;
	attr.ah_attr.is_global = 0;
	attr.ah_attr.sl = 0;
	attr.ah_attr.port_num = 1;
	attr.dest_qp_num = qp_num;
	attr.rq_psn = 1;
	attr.max_dest_rd_atomic = 1;
	attr.min_rnr_timer = 12;

	if (0 != ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_PATH_MTU | IBV_QP_AV | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER))
	{
		log_msg("Failed to change states: INIT -> RTR");
		exit(-1);
	}

	log_msg("INIT -> RTR QP Set Successfully");

	attr.qp_state = IBV_QPS_RTS;
	attr.timeout = 14;
	attr.retry_cnt = 7;
	attr.rnr_retry = 7;
	attr.sq_psn = 1;
	attr.max_rd_atomic = 1;

	if (0 != ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC))
	{
		log_msg("Failed to change state: RTR -> RTS");
		exit(-1);
	}

	log_msg("RNR -> RTR QP Set Successfully");
}

void release_memlock_limits()
{
	struct rlimit l;
	l.rlim_cur = RLIM_INFINITY;
	l.rlim_max = RLIM_INFINITY;
	
	if (0 != setrlimit(RLIMIT_MEMLOCK, &l))
	{
		log_msg("Failed to set memlock ulimit - errno = %s (%u)",strerror(errno), errno);
		exit(-1);
	}
}