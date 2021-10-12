WITH_TSX ?= 0
CFLAGS ?= -Os -Wall -g -fno-strict-aliasing
LDFLAGS ?= -lm

CPPFLAGS += -DWITH_TSX=${WITH_TSX}

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

all: profile profile-power

header_files: cacheutils.h libpowertrace.h ptedit_header.h

profile: main.c header_files
	@echo [CC] $@
	@gcc ${CPPFLAGS} ${CFLAGS} -DRECORD_POWER=0 main.c -o profile ${LDFLAGS}

profile-power: main.c header_files
	@echo [CC] $@
	@gcc ${CPPFLAGS} ${CFLAGS} -DRECORD_POWER=1 main.c -o profile-power ${LDFLAGS}

clean:
	@rm -rf profile profile-power

tar:
dist:
	@tar cfv dist.tar \
		--transform 's,^,profile/,' \
		Makefile \
		cacheutils.h \
		libpowertrace.h \
		main.c \
		ptedit_header.h \
