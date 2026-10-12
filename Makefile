all:

TARGET = ge_patch
OBJS = main.o gu.o exports.o

CFLAGS = -O3 -Os -G0 -Wall -fshort-wchar -fno-pic -mno-check-zero-division
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PRX_EXPORTS = exports.exp

USE_KERNEL_LIBC = 1
USE_KERNEL_LIBS = 1

LIBS = -lpspsystemctrl_kernel

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build_prx.mak

HOST_CC ?= cc
.PHONY: test
test:
	@set -e; binary=$$(mktemp /tmp/gepatch-test.XXXXXX); trap 'rm -f "$$binary"' EXIT; \
	$(HOST_CC) -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -fno-pie -no-pie -Itests/shim tests/presentation.c -o "$$binary"; \
	"$$binary"
