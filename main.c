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

const size_t BUF_SIZE = 0x100000;

int log_msg(const char* format, ...);
struct ibv_device** get_device_list();
struct ibv_context* get_dev_context();
struct ibv_mr* register_mr(struct ibv_pd* pd, void* buf, size_t buf_len, enum ibv_access_flags access);
void* alloc_mr(unsigned int size);
struct ibv_pd* alloc_pd(struct ibv_context* ctx);

int main(int argc, char** argv)
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

	// Free resources
	ibv_close_device(dev_ctx);
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
	char* dev_name = ibv_get_device_name(dev);
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
