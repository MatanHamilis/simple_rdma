#!/usr/bin/env bash

if [ "$#" -lt 8 ]; then
	echo "Usage: $0 USERNAME CLIENT_ADDR SERVER_ADDR BANDWIDTH TOOL_NAME PORT DURATION QPS"
	exit
fi
USERNAME=$1
CLIENT_ADDR=$2
SERVER_ADDR=$3
BANDWIDTH=$4
TOOL_NAME=$5
PORT=$6
DURATION=$7
QPS=$8

CMD_LINE_SRV="\
$TOOL_NAME \
	--port=$PORT \
	--size=$((2**17)) \
	--iters=1000 \
	--duration $DURATION \
	--force-link=IB \
	--perform_warm_up \
	--connection=UD \
	--report_gbits \
	--burst_size=$((2**4)) \
	--rate_limit=$BANDWIDTH \
	--rate_units=g \
	--rate_limit_type=SW"
CMD_LINE_CLIENT="$CMD_LINE_SRV --qp=$QPS $SERVER_ADDR"
ssh "$USERNAME"@"$SERVER_ADDR" "$CMD_LINE_SRV" 1>&- 2>&- & 
sleep 5
ssh "$USERNAME"@"$CLIENT_ADDR" "$CMD_LINE_CLIENT" 

