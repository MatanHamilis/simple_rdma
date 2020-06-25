/* 
In this file I'm going to implement simple RDMA program, testing the latency of various RDMA operations.
The program is based on the libibverbs
*/

#include <stdlib.h>
#include <stddef.h>

const size_t SIZE = 1024;

int main(int argc, char** argv)
{

}

// The client is the one connecting to the server and reading from / writing to a buffer residing in the server's memory.

int do_client()
{

}


// The server is the one allocating a buffer and waiting for messages from the client requesting perform read/write.
int do_server()
{
	char* const buffer = (char*)malloc(SIZE * sizeof(char));
	struct ibv_mr * mr;
}