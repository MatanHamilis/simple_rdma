/* 
In this file I'm going to implement simple RDMA program, testing the latency of various RDMA operations.
The program is based on the libibverbs
*/

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <infiniband/verbs.h>
#include <errno.h>
#include <stdarg.h>

const size_t SIZE = 1024;

int log_msg(const char* format, ...);
struct ibv_device** get_device_list();
struct ibv_context* get_dev_context(struct ibv_device* dev);

int main(int argc, char** argv)
{	int retval = ibv_fork_init();
	if (0 != retval)
	{
		log_msg("Failed! ibv_fork_init returned %d", retval);
		exit(-1);
	}

	struct ibv_device** devlist = get_device_list();
	struct ibv_device* dev = devlist[0];
	char* dev_name = ibv_get_device_name(dev);
	if (NULL == dev_name)
	{
		log_msg("Failed to get device name! leaving");
		exit(-1);
	}
	log_msg("Picked device: %s", dev_name);

	struct ibv_context* dev_ctx = get_dev_context(dev);
	ibv_free_device_list(devlist);


	// Free resources
	ibv_close_device(dev_ctx);
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

struct ibv_context* get_dev_context(struct ibv_device* dev)
{
	struct ibv_context* dev_ctx = ibv_open_device(dev);
	if (NULL == dev_ctx)
	{
		log_msg("Failed - ibv_open_device returned NULL");
		exit(-1);
	}
	log_msg("Sucess - context ptr = %p", dev_ctx);
	return dev_ctx;
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