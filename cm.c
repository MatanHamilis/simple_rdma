
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cm.h"
#include "logging.h"

const uint8_t IB_PORT_NUMBER = 1;

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
		cie.remote_addr = (uint64_t)mr->addr;
	} 
	else
	{
		cie.rkey = 0;
		cie.remote_addr = 0;
	}
	return cie;
}

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

ConnectionInfoExchange exchange_info_with_peer(int peer_sock, ConnectionInfoExchange my_info)
{
	int total_sent = 0;
    do_send(peer_sock, (char*)&my_info, sizeof(my_info));

	ConnectionInfoExchange peer_info;
	int total_received = 0;
    do_recv(peer_sock, (char*)&peer_info, sizeof(peer_info));

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

	int client_sock = accept(sock, (struct sockaddr*)&addr, &addr_size);
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
	log_msg("[QP Info] LID\t=\t%hu", info->port_lid);
	log_msg("[QP Info] QPN\t=\t%u",info->qp_num);
	log_msg("[QP Info] MR\t=\t%llu",info->remote_addr);
	log_msg("[QP Info] rkey\t=\t%u",info->rkey);
}