## author Romain Ly

DDoS filtering application  on DPDK
Comments under progressed

## dependencies (ubuntu 14.10 package)
- libgtk2.0-devbgtk2
- libyaml-dev
- libpcap-dev
- dpdk-2-1.0
- https://github.com/Romain-Ly/linux_ebpf_jit
- https://github.com/Romain-Ly/dpdk_getflags_name
- https://github.com/Romain-Ly/dpdk_fdir_parser
- https://github.com/Romain-Ly/dpdk_getport_capabilities

## How to 
make clean && make

## environments :
LD_LIBRARY_PATH="../linux_ebpf_jit"
export LD_LIBRARY_PATH

