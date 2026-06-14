# capwap-acdisc - portable CAPWAP AC-discovery from DHCP option 43/138
#
#   make            build the static library (portable decode core)
#   make test       build + run the unit tests (ASan/UBSan)
#   make clean      remove build artifacts
#
# Cross-compiling for a target (e.g. OpenWrt/QSDK):
#   make CC=arm-openwrt-linux-muslgnueabi-gcc
#
# The UCI request backend (capwap_acdisc_request_uci.c) is NOT built here: it
# needs libuci and is meant to be compiled on-target (link with -luci). The
# portable core and the no-op backend build anywhere.

CC      ?= cc
CFLAGS  ?= -Wall -Wextra -Werror -O2 -std=c99
SAN     ?= -fsanitize=address,undefined
AR      ?= ar

# Portable decode core (zero dependencies) — this is the reusable library.
CORE     = capwap_dhcp_opt.c capwap_acdisc.c
LIB      = libcapwapacdisc.a

# Tests link the core + the no-op request backend.
TEST_DEPS = $(CORE) capwap_acdisc_request_null.c

.PHONY: all test clean

all: $(LIB)

$(LIB): $(CORE:.c=.o)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test:
	$(CC) $(CFLAGS) $(SAN) $(TEST_DEPS) test_capwap_dhcp_opt.c -o test_opt
	$(CC) $(CFLAGS) $(SAN) $(TEST_DEPS) test_capwap_acdisc.c  -o test_acdisc
	./test_opt
	./test_acdisc

clean:
	rm -f *.o $(LIB) test_opt test_acdisc
