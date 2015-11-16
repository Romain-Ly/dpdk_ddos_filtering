ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc
RTE_OUTPUT = bin
RTE_TOOLCHAIN = colorgcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = bridge


#LDLIBS += -L$(RTE_SRCDIR)/../../bpf/test/linux-ebpf-jit
#LDLIBS += -l/$(RTE_SRCDIR)/../../lib/linux-ebpf-jit-dpdk/linux-ebpf-jit-dpdk
LDLIBS += -lyaml
LDLIBS += -lpcap

#LDPATH += -L../../lib/linux-ebpf-jit
LDLIBS += -L$(RTE_SRCDIR)/../linux_ebpf_jit/ -llinux_ebpf_jit


# all source are stored in SRCS-y
SRCS-y := src/main.c
SRCS-y += src/bridge.c
SRCS-y += src/bridge_optimized.c
#SRCS-y += src/aton.c
SRCS-y += src/flib.c
SRCS-y += src/ipc.c
SRCS-y += src/parser.c
SRCS-y += src/fdir.c
SRCS-y += src/dpdk_mp.c
SRCS-y += src/debug.c
SRCS-y += src/engine_bpf.c
SRCS-y += src/engine_bpf_pcap.c
SRCS-y += src/engine_misc.c
SRCS-y += src/engine_hash.c


SRCS-y += src/fast_strstr.c
CFLAGS += $(shell pkg-config --cflags glib-2.0)
LDLIBS += $(shell pkg-config --libs glib-2.0)

SRCS-y += $(RTE_SRCDIR)/../dpdk_getflags_name/src/getflags_name.c
CFLAGS += -I$(RTE_SRCDIR)/../dpdk_getflags_name/include

SRCS-y += $(RTE_SRCDIR)/../dpdk_getport_capabilities/src/getport_capabilities.c
CFLAGS += -I$(RTE_SRCDIR)/../dpdk_getport_capabilities/include

SRCS-y += $(RTE_SRCDIR)/../dpdk_fdir_parser/src/fdir_yaml_parser.c
CFLAGS += -I$(RTE_SRCDIR)/../dpdk_fdir_parser/include


CFLAGS += -I$(RTE_SRCDIR)/../linux_ebpf_jit

CFLAGS += -I$(RTE_SRCDIR)/include



CFLAGS += -O3
#CFLAGS += -Wall
#CFLAGS += -g 
#CFLAGS += -fstack-protector-all


CFLAGS += -std=gnu11
#CFLAGS += $(WERROR_FLAGS)


include $(RTE_SDK)/mk/rte.extapp.mk

