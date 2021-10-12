CFLAGS ?= -Os -Wall -g -fno-strict-aliasing
LDFLAGS ?= -lm -lpthread
WITH_TLB_EVICT ?= 0

# Detect if AMD CPU (ugly
NOT_INTEL ?= $(shell cat /proc/cpuinfo | grep -q Intel 2> /dev/null; echo $$?)
NOT_RDPRU ?= $(shell cat /proc/cpuinfo | grep -q rdpru 2> /dev/null; echo $$?)
WITH_AMD := 0

ifeq ($(NOT_INTEL), 1)
ifeq ($(NOT_RDPRU), 0)
WITH_AMD := 1
endif
endif

CPPFLAGS += -DWITH_AMD=${WITH_AMD} -DWITH_TLB_EVICT=${WITH_TLB_EVICT}

all: kaslr kaslr-power

kaslr: main.c cacheutils.h libpowertrace.h libtlb.h statistics.h
	@echo [CC] $@
	@gcc ${CPPFLAGS} ${CFLAGS} -DRECORD_POWER=0 main.c -o kaslr ${LDFLAGS}

kaslr-power: main.c cacheutils.h libpowertrace.h libtlb.h statistics.h
	@echo [CC] $@
	@gcc ${CPPFLAGS} ${CFLAGS} -DRECORD_POWER=1 main.c -o kaslr-power ${LDFLAGS}

clean:
	@rm -rf kaslr kaslr-power log.csv

tar:
dist:
	@tar cfv dist.tar \
		--transform 's,^,kaslr/,' \
		Makefile \
		libtlb.h \
		cacheutils.h \
		libpowertrace.h \
		main.c
