#!/usr/bin/env bash
sudo apt-get update
sudo apt-get -y install libibcm1 libibverbs1 ibverbs-utils librdmacm1 rdmacm-utils ibsim-utils ibutils libcxgb3-1 libibmad5 libibumad3 libmlx4-1 libmthca1 libnes1 rds-tools infiniband-diags libibcommon1 mstflint opensm libopensm5 perftest srptools libibverbs-dev librdmacm-dev

sudo modprobe rdma_cm
sudo modprobe ib_uverbs
sudo modprobe rdma_ucm
sudo modprobe ib_ucm
sudo modprobe ib_umad
sudo modprobe ib_ipoib
sudo modprobe mlx4_ib
sudo modprobe mlx4_en
sudo modprobe iw_cxgb3
sudo modprobe iw_cxgb4
sudo modprobe iw_nes
sudo modprobe iw_c2