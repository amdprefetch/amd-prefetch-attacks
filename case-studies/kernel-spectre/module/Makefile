MODULE_FILE := kernel_spectre.ko

# Default target
all: local

# Local
LOCAL_ARCH := x86
LOCAL_KDIR ?= /lib/modules/$(shell uname -r)/build

local : ARCH := $(LOCAL_ARCH)
local : KDIR := $(LOCAL_KDIR)
local-clean : ARCH := $(LOCAL_ARCH)

local: build
local-clean: clean

# Kernel module
OBJECT_FILES_NON_STANDARD_kernel_spectre.o := y
obj-m := kernel_spectre.o

build:
	@make \
		ARCH=$(ARCH) \
		CROSS_COMPILE=$(CCPATH) \
		-C $(KDIR) \
		M=$(PWD) \
		modules

clean:
	@rm -rf \
		*.o \
		*.ko \
		*.mod \
		*.mod.c \
		*.o.ur-safe \
		.*.cmd \
		modules.order \
		.tmp_versions \
		.cache.mk \
		Module.symvers
