// SPDX-License-Identifier: BSD-2-Clause
/*
 * capwap_acdisc_request_null.c - no-op backend for capwap_acdisc_request.h
 *
 * Use on platforms where the DHCP client is already configured to request
 * options 43/138 (so the library needn't touch it), or for host unit tests
 * where there is no DHCP client to configure. Always succeeds.
 *
 * Exactly ONE request backend (_uci.c OR _null.c) is linked per build.
 */
#include "capwap_acdisc_request.h"

int capwap_acdisc_request_options(const char *iface)
{
	(void)iface;
	return 0;
}
