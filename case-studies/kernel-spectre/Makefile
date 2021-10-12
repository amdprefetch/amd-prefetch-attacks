CFLAGS ?= -O3 -Wall -g -fno-strict-aliasing -lm
LDFLAGS ?= -lpthread
#
# Detect if AMD CPU (ugly
NOT_INTEL ?= $(shell cat /proc/cpuinfo | grep -q Intel 2> /dev/null; echo $$?)
NOT_RDPRU ?= $(shell cat /proc/cpuinfo | grep -q rdpru 2> /dev/null; echo $$?)
WITH_AMD := 0

ifeq ($(NOT_INTEL), 1)
ifeq ($(NOT_RDPRU), 0)
WITH_AMD := 1
endif
endif

CPPFLAGS += -DWITH_AMD=${WITH_AMD}

all: profile profile-optimized

profile: main.c cacheutils.h libtlb.h
	@echo [CC] $@
	@gcc ${CPPFLAGS} ${CFLAGS} -DRECORD_POWER=0 main.c ${LDFLAGS} -o profile

profile-optimized: optimized.c cacheutils.h libtlb.h
	@echo [CC] $@
	@gcc ${CPPFLAGS} ${CFLAGS} -DRECORD_POWER=0 optimized.c ${LDFLAGS} -o profile-optimized

clean:
	@rm -rf profile profile-optimized

dist:
	@tar cfv dist.tar \
		--transform 's,^,kernel_spectre/,' \
		Makefile \
		cacheutils.h \
		libtlb.h \
		main.c \
		optimized.c \
		module/Makefile \
		module/kernel_spectre.c \
		module/kernel_spectre.h
