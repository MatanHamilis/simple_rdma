#include "verbs_wrappers.h"

void* const QP_CONTEXT = (void*)0x12345678;

struct ibv_qp_init_attr create_qp_init_attr(struct ibv_cq* cq)
{
	enum ibv_qp_type qptype = IBV_QPT_RC;

	struct ibv_qp_init_attr attr = {
		.qp_context = QP_CONTEXT,
		.send_cq = cq,
		.recv_cq = cq,
		.srq = NULL,
		.qp_type = qptype,
		.sq_sig_all = 1,
		.cap.max_send_sge = 10,
		.cap.max_recv_sge = 10,
		.cap.max_recv_wr = 10,
		.cap.max_send_wr = 2048,
		.cap.max_inline_data = 32
	};

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
		log_msg("Failed to register MR! errno: %s (%u)", strerror(errno), errno);
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

void do_close_device(struct ibv_context* dev_ctx)
{
	if (ibv_close_device(dev_ctx))
	{
		log_msg("Failed to close device!");
		exit(-1);
	}
}

void do_rdma_read(void* remote_address, void* local_address, uint32_t rkey, uint32_t lkey, uint32_t size, struct ibv_qp* qp)
{
	struct ibv_sge sge_entry = {
		.addr = (uint64_t)local_address,
		.length = size,
		.lkey = lkey
	};
	struct ibv_send_wr* bad_wr = NULL;
	struct ibv_send_wr wr = {
		.wr_id = 1,
		.next = NULL,
		.sg_list = &sge_entry,
		.num_sge = 1,
		.opcode = IBV_WR_RDMA_READ,
		.send_flags = 0,
		.wr.rdma.remote_addr = (uint64_t)remote_address,
		.wr.rdma.rkey = rkey
	};
	int	ans = ibv_req_notify_cq(qp->send_cq, 0);
	if (0 != ans)
	{
		log_msg("Failed to req_notify_cq! errno = %s (%d)", strerror(ans), ans);
		exit(-1);
	}
	ans = ibv_post_send(qp, &wr, &bad_wr);
	if (0 != ans)
	{
		log_msg("Failed to post_send! errno = %s (%d)", strerror(ans), ans);
		exit(-1);
	}

}

void do_cq_empty(struct ibv_qp* qp, uint32_t num_events)
{
	uint32_t i = 0 ;
	while ( i < num_events )
	{
		struct ibv_wc wc;
		int ne = ibv_poll_cq(qp->send_cq, 1, &wc);
//		if (!ne)  break;
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
		i += ne;
	}
}