// SPDX-License-Identifier: BSD-2-Clause
/*
 * capwap_acdisc_request_uci.c - OpenWrt/QSDK backend for capwap_acdisc_request.h
 *
 * Ensures the netifd 'dhcp' proto requests CAPWAP options 43 + 138 by adding
 * them to UCI  network.<iface>.reqopts  (idempotently), then committing.
 * netifd turns each reqopts entry into a "-O <code>" flag for udhcpc, which
 * places the code in the DHCP Parameter Request List (option 55). Equivalent of:
 *
 *     uci add_list network.wan.reqopts=43
 *     uci add_list network.wan.reqopts=138
 *     uci commit network
 *
 * but in C via libuci — no shell. Build needs: -luci   (DEPENDS += libuci).
 *
 * Apply: the new reqopts take effect on the next lease/renew. For immediate
 * effect the caller may reload the network (e.g. ubus "network reload"); this
 * backend deliberately does not, to avoid bouncing a live WAN from a library.
 */
#include <stdio.h>
#include <string.h>
#include <uci.h>
#include "capwap_acdisc_request.h"

#define DEFAULT_IFACE  "wan"

/* Is value `val` already in option list `o`? o may be a single string or list. */
static int list_has(struct uci_option *o, const char *val)
{
	struct uci_element *e;

	if (!o)
		return 0;
	if (o->type == UCI_TYPE_STRING)
		return o->v.string && strcmp(o->v.string, val) == 0;
	if (o->type == UCI_TYPE_LIST) {
		uci_foreach_element(&o->v.list, e)
			if (strcmp(e->name, val) == 0)
				return 1;
	}
	return 0;
}

/* add_list network.<iface>.reqopts=<code> unless already present. */
static int ensure_one(struct uci_context *ctx, const char *iface, const char *code)
{
	struct uci_ptr ptr;
	char tuple[96];

	/* 1) look up the existing reqopts list (read) */
	memset(&ptr, 0, sizeof ptr);
	snprintf(tuple, sizeof tuple, "network.%s.reqopts", iface);
	if (uci_lookup_ptr(ctx, &ptr, tuple, true) != UCI_OK)
		return -1;                       /* section missing / parse error */

	if (list_has(ptr.o, code))
		return 0;                        /* already requested -> nothing to do */

	/* 2) add_list the code (uci_lookup_ptr with =value sets ptr.value) */
	memset(&ptr, 0, sizeof ptr);
	snprintf(tuple, sizeof tuple, "network.%s.reqopts=%s", iface, code);
	if (uci_lookup_ptr(ctx, &ptr, tuple, true) != UCI_OK)
		return -1;
	if (uci_add_list(ctx, &ptr) != UCI_OK)
		return -1;

	return 1;                                /* added */
}

int capwap_acdisc_request_options(const char *iface)
{
	struct uci_context *ctx;
	struct uci_ptr pkg_ptr;
	char codes[2][4];
	char pkgname[] = "network";
	int changed = 0, rc = 0;

	if (!iface)
		iface = DEFAULT_IFACE;

	ctx = uci_alloc_context();
	if (!ctx)
		return -1;

	snprintf(codes[0], sizeof codes[0], "%d", CAPWAP_DHCP_OPT_VENDOR);  /* 43  */
	snprintf(codes[1], sizeof codes[1], "%d", CAPWAP_DHCP_OPT_AC_V4);   /* 138 */

	for (int i = 0; i < 2; i++) {
		int r = ensure_one(ctx, iface, codes[i]);
		if (r < 0) { rc = -1; goto out; }
		changed += r;
	}

	if (changed) {
		/* commit the 'network' package */
		memset(&pkg_ptr, 0, sizeof pkg_ptr);
		if (uci_lookup_ptr(ctx, &pkg_ptr, pkgname, true) != UCI_OK ||
		    uci_commit(ctx, &pkg_ptr.p, false) != UCI_OK)
			rc = -1;
	}

out:
	uci_free_context(ctx);
	return rc;
}
