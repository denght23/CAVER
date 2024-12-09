#!/bin/bash

cecho(){  # source: https://stackoverflow.com/a/53463162/2886168
    RED="\033[0;31m"
    GREEN="\033[0;32m"
    YELLOW="\033[0;33m"
    # ... ADD MORE COLORS
    NC="\033[0m" # No Color

    printf "${!1}${2} ${NC}\n"
}

cecho "GREEN" "Running RDMA Network Load Balancing Simulations (leaf-spine topology)"

TOPOLOGY="fat_k8_100G_OS2" # or, fat_k8_100G_OS2
NETLOAD="60" # network load 50%
RUNTIME="0.1" # 0.1 second (traffic generation)

cecho "YELLOW" "\n----------------------------------"
cecho "YELLOW" "TOPOLOGY: ${TOPOLOGY}" 
cecho "YELLOW" "NETWORK LOAD: ${NETLOAD}" 
cecho "YELLOW" "TIME: ${RUNTIME}" 
cecho "YELLOW" "----------------------------------\n"

# # Lossless RDMA
# cecho "GREEN" "Run Lossless RDMA experiments..."
# python3 run.py --lb fecmp --pfc 1 --irn 0 --simul_time ${RUNTIME} --netload ${NETLOAD} --topo ${TOPOLOGY} 2>&1 > /dev/null & 
# sleep 10
# # conga only for non-bond topo;
# python3 run.py --lb conga --pfc 1 --irn 0 --simul_time ${RUNTIME} --netload ${NETLOAD} --topo ${TOPOLOGY} 2>&1 > /dev/null &
# sleep 10
# python3 run.py --lb conweave --pfc 1 --irn 0 --simul_time ${RUNTIME} --netload ${NETLOAD} --topo ${TOPOLOGY} 2>&1 > /dev/null &
# sleep 10
# python3 run.py --lb dv --pfc 1 --irn 0 --simul_time ${RUNTIME} --netload ${NETLOAD} --topo ${TOPOLOGY} 2>&1 > /dev/null &
# sleep 10

# Lossless RDMA
cecho "GREEN" "Run Lossless RDMA experiments..."
python3 run.py --lb fecmp --pfc 1 --irn 0 --simul_time ${RUNTIME} --netload 80 --topo ${TOPOLOGY} 2>&1 > /dev/null & 
sleep 10
# conga only for non-bond topo;
python3 run.py --lb fecmp --pfc 1 --irn 0 --simul_time ${RUNTIME} --netload 40 --topo ${TOPOLOGY} 2>&1 > /dev/null &
sleep 10
python3 run.py --lb fecmp --pfc 1 --irn 0 --simul_time ${RUNTIME} --netload 90 --topo ${TOPOLOGY} 2>&1 > /dev/null &
sleep 10
python3 run.py --lb fecmp --pfc 1 --irn 0 --simul_time ${RUNTIME} --netload 70 --topo ${TOPOLOGY} 2>&1 > /dev/null &
sleep 10
python3 run.py --lb fecmp --pfc 1 --irn 0 --simul_time ${RUNTIME} --netload 30 --topo ${TOPOLOGY} 2>&1 > /dev/null &
sleep 10


cecho "GREEN" "Runing all in parallel. Check the processors running on background!"
