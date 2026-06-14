// SPDX-License-Identifier: BSD-2-Clause
/*
 * test_capwap_dhcp_opt.c - unit tests for the opt 43/138 decoders.
 * Mirrors the vectors in CAPWAP-DHCP-DISCOVERY-ARCHITECTURE.md section 11,
 * plus boundary / malformed / security cases.
 */
#include <stdio.h>
#include <string.h>
#include "capwap_dhcp_opt.h"

static int g_pass, g_fail;

#define IP(a,b,c,d) (((uint32_t)(a)<<24)|((b)<<16)|((c)<<8)|(d))

static void show(const char *label, const struct ac_list *l, int rc)
{
	printf("    %-34s rc=%-3d ips=[", label, rc);
	for (unsigned i = 0; i < l->count; i++) {
		uint32_t x = l->ip[i];
		printf("%s%u.%u.%u.%u", i ? ", " : "",
		       (x>>24)&0xff, (x>>16)&0xff, (x>>8)&0xff, x&0xff);
	}
	printf("]\n");
}

/* expect: decode <hex> as opt<which> -> exact ip set (order-sensitive). */
static void check(const char *name, int which, const char *hex,
		  int want_rc_sign, const uint32_t *want, unsigned want_n)
{
	uint8_t buf[256];
	struct ac_list l = {0};
	int n = hexdecode(hex, buf, sizeof buf);
	int rc;

	if (n < 0) { /* hex itself invalid -> treat as empty input */
		printf("[hex-decode-fail] %s\n", name);
		g_fail++; return;
	}
	rc = (which == 138) ? decode_opt138(buf, n, &l)
			    : decode_opt43(buf, n, &l);

	int ok = 1;
	/* want_rc_sign: -1 expect rc<0, 0 expect rc==0, 1 expect rc>0 */
	if (want_rc_sign < 0 && rc >= 0) ok = 0;
	if (want_rc_sign == 0 && rc != 0) ok = 0;
	if (want_rc_sign > 0 && rc <= 0) ok = 0;
	if (l.count != want_n) ok = 0;
	for (unsigned i = 0; ok && i < want_n; i++)
		if (l.ip[i] != want[i]) ok = 0;

	printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
	if (!ok) show("got", &l, rc);
	ok ? g_pass++ : g_fail++;
}

int main(void)
{
	puts("=== CAPWAP DHCP option 43/138 decoder tests ===\n");

	/* --- option 138 (RFC 5417) --- */
	{ uint32_t w[] = { IP(192,168,1,1) };
	  check("138 single AC", 138, "c0a80101", 1, w, 1); }

	{ uint32_t w[] = { IP(192,168,1,1), IP(192,168,1,2) };
	  check("138 two ACs", 138, "c0a80101c0a80102", 1, w, 2); }

	check("138 not multiple of 4 -> err", 138, "c0a801", -1, NULL, 0);
	check("138 empty -> err",             138, "",        -1, NULL, 0);

	/* --- option 43 (vendor TLV, sub-opt 0xF1) --- */
	{ uint32_t w[] = { IP(192,168,1,1) };
	  check("43 sub-F1 single", 43, "f104c0a80101", 1, w, 1); }

	{ uint32_t w[] = { IP(192,168,1,1), IP(192,168,1,2) };
	  check("43 sub-F1 two ACs", 43, "f108c0a80101c0a80102", 1, w, 2); }

	{ uint32_t w[] = { IP(192,168,1,1) };
	  check("43 skips unknown sub-opt 01", 43, "0102deadf104c0a80101", 1, w, 1); }

	check("43 truncated value -> no overread", 43, "f1ff", 0, NULL, 0);
	check("43 empty -> 0",                      43, "",     0, NULL, 0);

	/* --- validation / security --- */
	check("138 reject 0.0.0.0",        138, "00000000", 0, NULL, 0);
	check("138 reject 127.0.0.1",      138, "7f000001", 0, NULL, 0);
	check("138 reject 224.0.0.1 mcast",138, "e0000001", 0, NULL, 0);

	{ uint32_t w[] = { IP(10,0,0,5) };  /* dedupe: same IP twice -> one */
	  check("138 de-dupes", 138, "0a0000050a000005", 1, w, 1); }

	{ uint32_t w[] = { IP(10,0,0,1) };  /* valid kept, 0.0.0.0 dropped */
	  check("138 drop invalid keep valid", 138, "000000000a000001", 1, w, 1); }

	printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
