#include <stdlib.h>
#include <stdint.h>
#include <infiniband/verbs.h>

#include "logging.h"

extern void* const QP_CONTEXT;

struct ibv_qp_init_attr create_qp_init_attr(struct ibv_cq* cq);
void destroy_qp(struct ibv_qp* qp);
struct ibv_qp* create_qp(struct ibv_pd* pd, struct ibv_qp_init_attr* attr);
struct ibv_comp_channel* create_comp_channel(struct ibv_context* ctx);
void destroy_comp_channel(struct ibv_comp_channel* ch);
struct ibv_cq* create_cq(struct ibv_context* ctx, int cqe, void* cq_context, struct ibv_comp_channel* ch, int comp_vector);
void destroy_cq(struct ibv_cq* cq);
void dealloc_pd(struct ibv_pd* pd);
void dereg_mr(struct ibv_mr* mr);
struct ibv_mr* register_mr(struct ibv_pd* pd, void* buf, size_t buf_len, enum ibv_access_flags access);
void* alloc_mr(unsigned int size);
struct ibv_pd* alloc_pd(struct ibv_context* ctx);
struct ibv_device** get_device_list();
struct ibv_context* get_dev_context();
void do_rdma_read(void* remote_address, void* local_address, uint32_t rkey, uint32_t lkey, uint32_t size, struct ibv_qp* qp);
void do_close_device(struct ibv_context* dev_ctx);