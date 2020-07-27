/* 
In this file I'm going to implement simple RDMA program, testing the latency of various RDMA operations.
The program is based on the libibverbs
*/

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <infiniband/verbs.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>

const int CQE_SIZE = 100;
const size_t BUF_SIZE = 0x1001;
const void* QP_CONTEXT = (void*)0x12345678;
const uint8_t IB_PORT_NUMBER = 1;

typedef struct 
{
	uint32_t qp_num;
	uint16_t port_lid;
	uint64_t remote_addr;
	uint32_t rkey;
} ConnectionInfoExchange;

ConnectionInfoExchange prepare_exchange_info(struct ibv_qp*, struct ibv_context*, struct ibv_mr*);

int log_msg(const char* format, ...);
struct ibv_device** get_device_list();
struct ibv_context* get_dev_context();

struct ibv_mr* register_mr(struct ibv_pd* pd, void* buf, size_t buf_len, enum ibv_access_flags access);
void* alloc_mr(unsigned int size);
void dereg_mr(struct ibv_mr* mr);

void dealloc_pd(struct ibv_pd*);
struct ibv_pd* alloc_pd(struct ibv_context* ctx);

struct ibv_comp_channel* create_comp_channel(struct ibv_context* ctx);
void destroy_comp_channel(struct ibv_comp_channel* ch);

struct ibv_cq* create_cq(struct ibv_context*, int cqe, void* cq_context, struct ibv_comp_channel* ch, int comp_vector);
void destroy_cq(struct ibv_cq* cq);

void destroy_qp(struct ibv_qp* qp);
struct ibv_qp* create_qp(struct ibv_pd* pd, struct ibv_qp_init_attr* attr);
struct ibv_qp_init_attr create_qp_init_attr(struct ibv_cq* cq);
int do_server(uint16_t port_no);
int do_client(char* server_addr, uint16_t port_no);

int do_connect_server(int16_t listen_port);
int do_connect_client(uint16_t portno, char* ipv4_addr);
ConnectionInfoExchange exchange_info_with_peer(int peer_sock, ConnectionInfoExchange my_info);
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
	struct ibv_sge sge_entry = {
		.addr = mr->addr,
		.length = 4,
		.lkey = mr->lkey
	};
	struct ibv_send_wr* bad_wr = NULL;
	struct ibv_send_wr wr = {
		.wr_id = 1,
		.next = NULL,
		.sg_list = &sge_entry,
		.num_sge = 1,
		.opcode = IBV_WR_RDMA_READ,
		.send_flags = 0,
		.wr.rdma.remote_addr = peer_info.remote_addr,
		.wr.rdma.rkey = peer_info.rkey
	};

	ans = ibv_req_notify_cq(cq_with_ch, 0);
	if (0 != ans)
	{
		log_msg("Failed to req_notify_cq! errno = %s (%d)", strerror(ans), ans);
		exit(-1);
	}
	void* ev_ctx = NULL;
	ans = ibv_post_send(qp, &wr, &bad_wr);
	if (0 != ans)
	{
		log_msg("Failed to post_send! errno = %s (%d)", strerror(ans), ans);
		exit(-1);
	}
	int ret = ibv_get_cq_event(ch, &cq_with_ch, &ev_ctx);
	if (ret)
	{
		log_msg("Failed to fetch event from the CQ.");
		exit(-1);
	}

	ibv_ack_cq_events(cq_with_ch, 1);
	while ( 1 )
	{
		struct ibv_wc wc;
		int ne = ibv_poll_cq(cq_with_ch, 1, &wc);
		if (!ne)  break;
		if (ne < 0)
		{
			log_msg("Error in ibv_poll_cq! Value returned = %d", ne);
			exit(-1);
		}
		if (wc.status != IBV_WC_SUCCESS)
		{
			log_msg("Received WQE but the WR failed! Status = %s (%d)", ibv_wc_status_str(wc.status), wc.status);
			exit(-1);
		}

		log_msg("WR id %d finished successfully!", wc.wr_id);
	}

	log_msg("The first int in the buffer is: %d", ((int*)buf)[0]);
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
	sleep(1);
	destroy_qp(qp);

	destroy_cq(cq_no_ch);
	destroy_cq(cq_with_ch);
	destroy_comp_channel(ch);

	dereg_mr(mr);

	dealloc_pd(pd);

	free(buf);

	// Free resources
	ibv_close_device(dev_ctx);
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
	attr.max_dest_rd_atomic = 0;
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
	attr.max_rd_atomic = 0;

	if (0 != ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC))
	{
		log_msg("Failed to change state: RTR -> RTS");
		exit(-1);
	}

	log_msg("RNR -> RTR QP Set Successfully");
}
void print_connection_info(ConnectionInfoExchange* info)
{
	log_msg("[QP Info] LID\t=\t%hu", info->port_lid);
	log_msg("[QP Info] QPN\t=\t%u",info->qp_num);
	log_msg("[QP Info] MR\t=\t%llu",info->remote_addr);
	log_msg("[QP Info] rkey\t=\t%u",info->rkey);
}
// Exchanges local info with peer. Returns the peer info.
ConnectionInfoExchange exchange_info_with_peer(int peer_sock, ConnectionInfoExchange my_info)
{
	int total_sent = 0;
	char* to_send = &my_info;
	while (total_sent < sizeof(my_info))
	{
		int current_sent = send(peer_sock, to_send + total_sent, sizeof(my_info) - total_sent, 0);
		if (-1 == current_sent)
		{
			log_msg("Failed to send! errno = %s", strerror(errno));
			exit(-1);
		}
		total_sent += current_sent;
	}

	ConnectionInfoExchange peer_info;
	int total_received = 0;
	char* to_recv = &peer_info;
	while (total_received < sizeof(peer_info))
	{
		int current_received = recv(peer_sock, to_recv + total_received, sizeof(peer_info) - total_received, 0);
		if (-1 == current_received)
		{
			log_msg("Failed to receive msg. errno = %s", strerror(errno));
			exit(-1);
		}
		total_received += current_received;
	}

	close(peer_sock);
	print_connection_info(&peer_info);
	return peer_info;

}

int do_connect_server(int16_t listen_port)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		log_msg("Failed to create socket, errno = %s", strerror(errno));
		exit(-1);
	}

	struct sockaddr_in addr;
	addr.sin_port = listen_port;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	socklen_t addr_size = sizeof(addr);
	if (0 != bind(sock, &addr, sizeof(addr)))
	{
		log_msg("Failed to bind to interface! port = %hu, errno = %s", listen_port, strerror(errno));
		exit(-1);
	}

	if (0 != listen(sock, 1))
	{
		log_msg("Failed to start listening on socket. errno = %s", strerror(errno));
		exit(-1);
	}

	int client_sock = accept(sock, &addr, &addr_size);
	if (-1 == client_sock)
	{
		log_msg("Failed to accept connection! errno = %s", strerror(errno));
		exit(-1);
	}

	close(sock);
	return client_sock;
}

int do_connect_client(uint16_t portno, char* ipv4_addr)
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0)
	{
		log_msg("Failed to create socket, errno = %s", strerror(errno));
		exit(-1);
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = portno;
	if (1 != inet_aton(ipv4_addr, &addr.sin_addr))
	{
		log_msg("Failed to parse ipv4 address! Address got: %s", ipv4_addr);
		exit(-1);
	}

	if (0 != connect(sock, &addr, sizeof(addr)))
	{
		log_msg("Failed to connect! Errno = %s", strerror(errno));
		exit(-1);
	}

	return sock;
}

ConnectionInfoExchange prepare_exchange_info(struct ibv_qp* qp, struct ibv_context* ctx, struct ibv_mr* mr)
{
	ConnectionInfoExchange cie;
	cie.qp_num = qp->qp_num;
	
	struct ibv_port_attr port_attr;
	if (0 != ibv_query_port(ctx, IB_PORT_NUMBER, &port_attr))
	{
		log_msg("Failed to fetch port attributes!");
		exit(-1);
	}
	cie.port_lid = port_attr.lid;
	if (NULL != mr)
	{
		cie.rkey = mr->rkey;
		cie.remote_addr = mr->addr;
	} 
	else
	{
		cie.rkey = 0;
		cie.remote_addr = 0;
	}
	return cie;
}

struct ibv_qp_init_attr create_qp_init_attr(struct ibv_cq* cq)
{
	enum ibv_qp_type qptype = IBV_QPT_RC;

	struct ibv_qp_init_attr attr = {
		.qp_context = QP_CONTEXT,
		.send_cq = cq,
		.recv_cq = cq,
		.srq = NULL,
		.qp_type = qptype,
		.sq_sig_all = 1
	};

	attr.cap.max_send_wr = 10;
	attr.cap.max_recv_wr = 1;
	attr.cap.max_send_sge = 10;
	attr.cap.max_recv_sge = 10;
	attr.cap.max_inline_data = 32;
	return attr;
}

void destroy_qp(struct ibv_qp* qp)
{
	log_msg("Destroying QP now");
	if (0 != ibv_destroy_qp(qp))
	{
		log_msg("Failed to destroy QP!");
		exit(-1);
	}
	log_msg("QP Destroyed successfully!");
}

struct ibv_qp* create_qp(struct ibv_pd* pd, struct ibv_qp_init_attr* attr)
{
	log_msg("Creating QP!\n\tpd = %p\n\tattr = %p", pd, attr);
	struct ibv_qp* qp = ibv_create_qp(pd, attr);
	if (NULL == qp)
	{
		log_msg("Failed to create QP!");
		exit(-1);
	}
	log_msg("QP created successfully!");
	log_msg("Setting QP Properties...");
	return qp;
}

struct ibv_comp_channel* create_comp_channel(struct ibv_context* ctx)
{
	log_msg("Creating completion channel!\n\tctx = %p", ctx);
	struct ibv_comp_channel* cch = ibv_create_comp_channel(ctx);
	if (NULL == cch)
	{
		log_msg("Failed to create completion channel");
		exit(-1);
	}
	log_msg("Completion channel created successfully!");
	return cch;
}

void destroy_comp_channel(struct ibv_comp_channel* ch)
{
	log_msg("Destroying completion channel!\n\tch = %p", ch);
	if (0 != ibv_destroy_comp_channel(ch))
	{	
		log_msg("Failed to destroy completion channel");
		exit(-1);
	}
	log_msg("Destroyed channel successfully!");
}

struct ibv_cq* create_cq(struct ibv_context* ctx, int cqe, void* cq_context, struct ibv_comp_channel* ch, int comp_vector)
{
	log_msg("Creating CQ!\n\tcontext = %p\n\tcqe = %d\n\t private context = %p\n\tcompletion channel = %p\n\tcompletion vector = %d", ctx, cqe, cq_context, ch, comp_vector);
	struct ibv_cq* cq = ibv_create_cq(ctx, cqe, cq_context, ch, comp_vector);
	if (NULL == cq)
	{	
		log_msg("Failed to create CQ!");
		exit(-1);
	}
	log_msg("Completion CQ successfully!");
	return cq;
}

void destroy_cq(struct ibv_cq* cq)
{
	log_msg("Destroying CQ!\n\tcq = %p", cq);
	if (0 != ibv_destroy_cq(cq))	
	{
		log_msg("Failed to destroy CQ");
		exit(-1);
	}
	log_msg("Destoyed CQ successfully!");
}


void dealloc_pd(struct ibv_pd* pd)
{
	log_msg("Deallocating PD:\n\tpd = %p", pd);
	if (0 != ibv_dealloc_pd(pd))
	{
		log_msg("Failed to deallocate pd!");
		exit(-1);
	}
	log_msg("PD Deallocated successfully!");
}

void dereg_mr(struct ibv_mr* mr)
{
	log_msg("Deregistering MR:\n\tmr = %p", mr);
	if (0 != ibv_dereg_mr(mr))
	{
		log_msg("Failed to deregister MR");
		exit(-1);
	}
	log_msg("Deregistered MR successfully!");
}
struct ibv_mr* register_mr(struct ibv_pd* pd, void* buf, size_t buf_len, enum ibv_access_flags access)
{
	log_msg("Trying to register MR:");
	log_msg("\tpd = %p\n\tbuf = %p\n\tbuf_len = %u\n\taccess = %u", pd, buf, buf_len, access);

	struct ibv_mr* ret_val = ibv_reg_mr(pd, buf, buf_len, access);
	if (NULL == ret_val)
	{
		log_msg("Failed to register MR!");
		exit(-1);
	}
	log_msg("MR Register finished successfully!");
	return ret_val;
}

void* alloc_mr(unsigned int size)
{
	log_msg("Allocating MR of size %u", size);
	void* mr = malloc(size);
	if (NULL == mr)
	{
		log_msg("Failed to allocate MR!");
		exit(-1);
	}
	log_msg("Allocated MR successfully!");
	return mr;
}

struct ibv_pd* alloc_pd(struct ibv_context* ctx)
{
	struct ibv_pd* ret_val = ibv_alloc_pd(ctx);
	if (NULL == ret_val)
	{
		log_msg("Failed to alloc pd");
		exit(-1);
	}

	return ret_val;
}

int log_msg(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
}

struct ibv_device** get_device_list()
{
	int num_devices = 0;
	struct ibv_device** devlist = ibv_get_device_list(&num_devices);
	if (NULL == devlist)
	{
		log_msg("Failed to get device list");
		exit(-1);
	}
	log_msg("Got %d devices!", num_devices);
	return devlist;
}

struct ibv_context* get_dev_context()
{
	struct ibv_device** devlist = get_device_list();
	struct ibv_device* dev = devlist[0];
	const char* dev_name = ibv_get_device_name(dev);
	if (NULL == dev_name)
	{
		log_msg("Failed to get device name! leaving");
		exit(-1);
	}


	log_msg("Picked device: %s", dev_name);

	struct ibv_context* dev_ctx = ibv_open_device(dev);
	ibv_free_device_list(devlist);

	if (NULL == dev_ctx)
	{
		log_msg("Failed - ibv_open_device returned NULL");
		exit(-1);
	}
	log_msg("Sucess - context ptr = %p", dev_ctx);
	union ibv_gid gid;
	if (0 != ibv_query_gid(dev_ctx, 1, 0, &gid))
	{
		log_msg("ibv_query_gid failed");
		exit(-1);
	}
	log_msg("Device port 1 gid 0 = %llx : %llx", gid.global.subnet_prefix, gid.global.interface_id);

	return dev_ctx;
}
