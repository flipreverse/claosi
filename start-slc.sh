#\!/bin/bash
USE_TP=${1:-1}; shift
USE_PROT_SPECIFIC=${1:-0}; shift
sudo insmod build/kern/slc-core.ko
sudo insmod build/kern/slc-net.ko useTracepoints=${USE_TP} useProtSpecific=${USE_PROT_SPECIFIC}
sudo insmod build/kern/slc-process.ko
./build/user/slc-core ../slc-input
sleep 1
sudo rmmod slc-process
sleep 1
sudo rmmod slc-net
sleep 1
sudo rmmod slc-core
