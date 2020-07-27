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

#include "verbs_wrappers.h"
#include "logging.h"
#include "cm.h"

const int CQE_SIZE = 100;
const size_t BUF_SIZE = 0x1001;

int do_server(uint16_t port_no);
int do_client(char* server_addr, uint16_t port_no);
void setup_qp(ConnectionInfoExchange* info, struct ibv_qp* qp);
void print_help(char* prog_name);

int main(int argc, char** argv)
{
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
	struct ibv_context* dev_ctx = get_dev_context();
	struct ibv_pd* pd = alloc_pd(dev_ctx);
	struct ibv_comp_channel* ch = create_comp_channel(dev_ctx);
	struct ibv_cq* cq_with_ch = create_cq(dev_ctx, CQE_SIZE, NULL, ch, 0);
	struct ibv_qp_init_attr qp_attrs = create_qp_init_attr(cq_with_ch);
	struct ibv_qp* qp = create_qp(pd, &qp_attrs);
	int client_sock = do_connect_client(port, server_addr);
	ConnectionInfoExchange my_info = prepare_exchange_info(qp, dev_ctx, NULL);
	ConnectionInfoExchange peer_info = exchange_info_with_peer(client_sock, my_info);
	void* buf = alloc_mr(BUF_SIZE);
	struct ibv_mr* mr = register_mr(pd, buf, BUF_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ);
	setup_qp(&peer_info, qp);
	do_sync(client_sock);
	do_rdma_read((void*)peer_info.remote_addr, buf, peer_info.rkey, mr->lkey, BUF_SIZE, qp);
	log_msg("The first int in the buffer is: %d", ((int*)buf)[0]);
	do_sync(client_sock);

	dereg_mr(mr);
	free(buf);
	destroy_qp(qp);
	destroy_cq(cq_with_ch);
	destroy_comp_channel(ch);
	dealloc_pd(pd);
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

	void* buf = alloc_mr(BUF_SIZE);
	((int*)buf)[0] = 0x12345678;

	struct ibv_pd* pd = alloc_pd(dev_ctx);

	struct ibv_mr* mr = register_mr(pd, buf, BUF_SIZE, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ);

	struct ibv_comp_channel* ch = create_comp_channel(dev_ctx);
	struct ibv_cq* cq_with_ch = create_cq(dev_ctx, CQE_SIZE, NULL, ch, 0);
	struct ibv_cq* cq_no_ch = create_cq(dev_ctx, CQE_SIZE, NULL, NULL, 0);

	struct ibv_qp_init_attr qp_attrs = create_qp_init_attr(cq_with_ch);
	struct ibv_qp* qp = create_qp(pd, &qp_attrs);

	int server_sock = do_connect_server(port_no);
	ConnectionInfoExchange my_info = prepare_exchange_info(qp, dev_ctx, mr);
	ConnectionInfoExchange peer_info = exchange_info_with_peer(server_sock, my_info);
	setup_qp(&peer_info, qp);
	do_sync(server_sock);
	do_sync(server_sock);
	destroy_qp(qp);

	destroy_cq(cq_no_ch);
	destroy_cq(cq_with_ch);
	destroy_comp_channel(ch);

	dereg_mr(mr);

	dealloc_pd(pd);

	free(buf);

	do_close_device(dev_ctx);
}

void setup_qp(ConnectionInfoExchange* info, struct ibv_qp* qp)
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
	attr.ah_attr.dlid = info->port_lid;
	attr.ah_attr.is_global = 0;
	attr.ah_attr.sl = 0;
	attr.ah_attr.port_num = 1;
	attr.dest_qp_num = info->qp_num;
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