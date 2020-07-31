
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cm.h"
#include "logging.h"

const uint8_t IB_PORT_NUMBER = 1;

void do_send(int sock, char* buf, int size)
{
	int total_sent = 0;
	int ans = 0;
	while (total_sent < size)
	{
		ans = send(sock, buf+total_sent, size - total_sent, 0);
		if (ans < 0)
		{
			log_msg("Failed to send! errno = %s (%d)", strerror(ans), ans);
			exit(-1);
		}
		total_sent += ans;
	}
}

void do_recv(int sock, char* buf, int size)
{
	int total_recv = 0;
	int ans = 0;
	while (total_recv < size)
	{
		ans = recv(sock, buf+total_recv, size - total_recv, 0);
		if (ans < 0)
		{
			log_msg("Failed to recv! errno = %s (%d)", strerror(ans), ans);
			exit(-1);
		}
		total_recv += ans;
	}
}

void do_sync(int sock)
{
	int ans = 0;
	char buf = 'a';
	do_send(sock, &buf, 1);
	do_recv(sock, &buf, 1);
}

void send_buf_to_peer(int peer_sock, char* buf, uint64_t buf_size)
{
	do_send(peer_sock, (char*)&buf_size, sizeof(buf_size));
	do_send(peer_sock, buf, buf_size);
}

uint64_t recv_buf_from_peer(int peer_sock, void** ptr)
{
	uint64_t buf_size;
	do_recv(peer_sock, &buf_size, sizeof(buf_size));
	*ptr = do_malloc(buf_size);
	do_recv(peer_sock, *ptr, buf_size);
	return buf_size;
}

void send_info_to_peer(int peer_sock, struct ibv_qp* qp, struct ibv_context* dev_ctx, struct ibv_mr** mrs, uint32_t number_of_mrs)
{
	ConnectionInfoExchange* peer_info = NULL;
	uint32_t total_bytes_for_struct = sizeof(peer_info->header) + number_of_mrs * sizeof(MrEntry);
	ConnectionInfoExchange* my_info = malloc(total_bytes_for_struct);
	if (NULL == my_info)
	{
		log_msg("Failed to malloc! leaving...");
		exit(-1);
	}

	struct ibv_port_attr port_attrs;
	if (0 != ibv_query_port(dev_ctx, IB_PORT_NUMBER, &port_attrs))
	{
		log_msg("Failed to fetch port attributes!");
		exit(-1);
	}

	my_info->header.number_of_mrs = number_of_mrs;
	my_info->header.port_lid = port_attrs.lid;
	my_info->header.qp_num = qp->qp_num;

	for (uint32_t i = 0 ; i < number_of_mrs ; ++i)
	{
		my_info->mrs[i].remote_addr = mrs[i]->addr;
		my_info->mrs[i].rkey = mrs[i]->rkey;
		my_info->mrs[i].size_in_bytes = mrs[i]->length;
	}

	send_buf_to_peer(peer_sock, (char*)(my_info), total_bytes_for_struct);
	free(my_info);
}

ConnectionInfoExchange* receive_info_from_peer(int peer_sock)
{
	ConnectionInfoExchange* peer_info = NULL;
	recv_buf_from_peer(peer_sock, &peer_info);
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
	if (0 != bind(sock, (struct sockaddr*)&addr, sizeof(addr)))
	{
		log_msg("Failed to bind to interface! port = %hu, errno = %s", listen_port, strerror(errno));
		exit(-1);
	}

	if (0 != listen(sock, 1))
	{
		log_msg("Failed to start listening on socket. errno = %s", strerror(errno));
		exit(-1);
	}

	log_msg("Waiting for client to connect...");
	int client_sock = accept(sock, (struct sockaddr*)&addr, &addr_size);
	log_msg("Done! Client has connected");
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

	if (0 != connect(sock, (struct sockaddr*)&addr, sizeof(addr)))
	{
		log_msg("Failed to connect! Errno = %s", strerror(errno));
		exit(-1);
	}

	return sock;
}

void print_connection_info(ConnectionInfoExchange* info)
{

	log_msg("[QP Info] LID\t=\t%hu", info->header.port_lid);
	log_msg("[QP Info] Number of MRs\t=\t%u",info->header.number_of_mrs);
	for (uint32_t i = 0 ; i < info->header.number_of_mrs ; ++i)
	{
		log_msg("[QP Info] \t[% 6u]", i);
		log_msg("[QP Info] \t\tMR\t=\t%llu",info->mrs[i].remote_addr);
		log_msg("[QP Info] \t\trkey\t=\t%u",info->mrs[i].rkey);
		log_msg("[QP Info] \t\tsize\t=\t%x", info->mrs[i].size_in_bytes);
	}
}