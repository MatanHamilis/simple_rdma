#include <stdint.h>
#include <infiniband/verbs.h>

extern const uint8_t IB_PORT_NUMBER;

typedef struct 
{
	uint32_t qp_num;
	uint16_t port_lid;
	uint64_t remote_addr;
	uint32_t rkey;
} ConnectionInfoExchange;

ConnectionInfoExchange prepare_exchange_info(struct ibv_qp*, struct ibv_context*, struct ibv_mr*);
int do_connect_server(int16_t listen_port);
int do_connect_client(uint16_t portno, char* ipv4_addr);
void do_sync(int sock);
void do_send(int sock, char* buf, int size);
void do_recv(int sock, char* buf, int size);
ConnectionInfoExchange exchange_info_with_peer(int peer_sock, ConnectionInfoExchange my_info);
void print_connection_info(ConnectionInfoExchange* info);