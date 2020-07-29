#ifndef __CM_H__
#define __CM_H__
#include <stdint.h>
#include <infiniband/verbs.h>

extern const uint8_t IB_PORT_NUMBER;

typedef struct
{
	uint64_t remote_addr;
	uint32_t rkey;
	uint32_t size_in_bytes;
} MrEntry;

typedef struct
{
	uint32_t qp_num;
	uint16_t port_lid;
	uint32_t number_of_mrs;
} ConnectionInfoHeader;

typedef struct 
{
	ConnectionInfoHeader header;
	MrEntry mrs[0];
} ConnectionInfoExchange;


int do_connect_server(int16_t listen_port);
int do_connect_client(uint16_t portno, char* ipv4_addr);
void do_sync(int sock);
void do_send(int sock, char* buf, int size);
void do_recv(int sock, char* buf, int size);
ConnectionInfoExchange* exchange_info_with_peer(int peer_sock, struct ibv_qp*, struct ibv_context*, struct ibv_mr**, uint32_t number_of_mrs);
void print_connection_info(ConnectionInfoExchange* info);

#endif 