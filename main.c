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
#include <stdarg.h>

const int CQE_SIZE = 100;
const size_t BUF_SIZE = 0x100000;
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
		}
	}

	if (server_addr == NULL)
	{
		return do_server(port);
	}
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

	void* buf1 = alloc_mr(BUF_SIZE);
	void* buf2 = alloc_mr(BUF_SIZE);
	void* buf3 = alloc_mr(BUF_SIZE);

	struct ibv_pd* pd = alloc_pd(dev_ctx);

	struct ibv_mr* mr1 = register_mr(pd, buf1, BUF_SIZE, 0);
	struct ibv_mr* mr2 = register_mr(pd, buf1, BUF_SIZE, IBV_ACCESS_LOCAL_WRITE);
	struct ibv_mr* mr3 = register_mr(pd, buf1, BUF_SIZE, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE);

	struct ibv_comp_channel* ch = create_comp_channel(dev_ctx);
	struct ibv_cq* cq_with_ch = create_cq(dev_ctx, CQE_SIZE, NULL, ch, 0);
	struct ibv_cq* cq_no_ch = create_cq(dev_ctx, CQE_SIZE, NULL, NULL, 0);

	struct ibv_qp_init_attr qp_attrs = create_qp_init_attr(cq_with_ch);
	struct ibv_qp* qp = create_qp(pd, &qp_attrs);

	int server_sock = do_connect_server(port_no);
	ConnectionInfoExchange my_info;
	my_info.port_lid = 0;
	my_info.remote_addr = (uint64_t)mr1->addr;
	my_info.rkey = mr1->rkey;
	my_info.qp_num = qp->qp_num;
	ConnectionInfoExchange info = exchange_info_with_peer(server_sock, my_info);


	

	destroy_qp(qp);

	destroy_cq(cq_no_ch);
	destroy_cq(cq_with_ch);
	destroy_comp_channel(ch);

	dereg_mr(mr1);
	dereg_mr(mr2);
	dereg_mr(mr3);

	dealloc_pd(pd);

	free(buf1);
	free(buf2);
	free(buf3);

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
	if (-1 == ibv_modify_qp(qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT))
	{
		log_msg("Failed to change states: RESET -> INIT");
		exit(-1);
	}



}
void print_connection_info(ConnectionInfoExchange* info)
{
	log_msg("[QP Info] LID=\t%hu", info->port_lid);
	log_msg("[QP Info] QP Number=\t%u",info->qp_num);
	log_msg("[QP Info] MR=\t%llu",info->remote_addr);
	log_msg("[QP Info] rkey=\t%u",info->rkey);
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

	int client_sock = accept(sock, &addr, sizeof(addr));
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
	cie.remote_addr = mr->addr;
	cie.rkey = mr->rkey;
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
