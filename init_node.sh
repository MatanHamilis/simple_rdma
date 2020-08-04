#!/usr/bin/env bash
if [ "$#" -lt 1 ]; then
	echo "Not enough params!"
	echo "Usage: $0 <num>"
	exit
fi
for i in $(seq $1); do
	ADDR_TO_INIT="cp$i"
	ssh "$ADDR_TO_INIT" << EOF &
	sudo apt-get update
	sudo apt-get -y install rdma-core libibverbs1 ibverbs-utils librdmacm1 rdmacm-utils ibsim-utils ibutils libcxgb3-1 libibmad5 libibumad3 libmlx4-1 libmthca1 libnes1 infiniband-diags mstflint opensm libopensm5a perftest srptools libibverbs-dev librdmacm-dev mbw cmake

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

	echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
EOF
done
