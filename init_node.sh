#!/usr/bin/env bash
if [ "$#" -lt 1 ]; then
	echo "Not enough params!"
	echo "Usage: $0 <num>"
	exit
fi
for i in $(seq $1); do
	ADDR_TO_INIT="c6220-$i"
	ssh "$ADDR_TO_INIT" << EOF &
	sudo apt-get update
	sudo apt-get -y install rdma-core libibverbs1 ibverbs-utils librdmacm1 rdmacm-utils ibsim-utils ibutils libcxgb3-1 libibmad5 libibumad3 libmlx4-1 libmthca1 libnes1 infiniband-diags mstflint opensm libopensm5a perftest srptools libibverbs-dev librdmacm-dev mbw libibmad-dev

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

git clone https://github.com/Mellanox/mstflint.git
cd mstflint
./autogen.sh
configure
make -j 32
sudo make install
cd ..

wget 'http://www.mellanox.com/downloads/firmware/fw-ConnectX3-rel-2_42_5000-0T483W-FlexBoot-3.4.752.bin.zip'
unzip 'http://www.mellanox.com/downloads/firmware/fw-ConnectX3-rel-2_42_5000-0T483W-FlexBoot-3.4.752.bin.zip'
sudo mstflint --latest_fw -d mlx4_0 -i './fw-ConnectX3-rel-2_42_5000-0T483W-FlexBoot-3.4.752.bin' burn

git clone https://github.com/Mellanox/ibdump.git
cd ibdump
make WITH_MSTFLINT=yes
sudo make WITH_MSTFLINT=yes install
cd ..

wget 'https://www.mellanox.com/downloads/ofed/MLNX_OFED-4.9-0.1.7.0/MLNX_OFED_LINUX-4.9-0.1.7.0-ubuntu18.04-x86_64.tgz'

