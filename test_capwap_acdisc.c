// SPDX-License-Identifier: BSD-2-Clause
/*
 * test_capwap_acdisc.c - tests for the high-level portable AC-discovery API.
 * Exercises the call shape a CAPWAP WTP actually uses.
 */
#include <stdio.h>
#include <string.h>
#include "capwap_acdisc.h"

static int g_pass, g_fail;

static void expect_ip(const char *name, const char *o138, const char *o43,
		      const char *want_ip /* NULL = expect failure */)
{
	char ip[16];
	int rc = capwap_acdisc_ip(o138, o43, ip, sizeof ip);
	int ok;

	if (want_ip == NULL)
		ok = (rc == -1);
	else
		ok = (rc == 0 && strcmp(ip, want_ip) == 0);

	printf("[%s] %-38s -> %s\n", ok ? "PASS" : "FAIL", name,
	       rc == 0 ? ip : "(none)");
	ok ? g_pass++ : g_fail++;
}

static void expect_list(const char *name, const char *o138, const char *o43,
			const char *want_csv /* e.g. "192.168.1.1,192.168.1.2" */)
{
	uint32_t ips[8];
	char got[128] = {0};
	int n = capwap_acdisc_list(o138, o43, ips, 8);
	int ok = 1;

	for (int i = 0; i < n; i++) {
		char b[16];
		capwap_ac_ip_to_str(ips[i], b, sizeof b);
		if (i) strncat(got, ",", sizeof got - strlen(got) - 1);
		strncat(got, b, sizeof got - strlen(got) - 1);
	}
	if (n < 0) ok = 0;
	else if (strcmp(got, want_csv) != 0) ok = 0;

	printf("[%s] %-38s -> [%s]\n", ok ? "PASS" : "FAIL", name, got);
	ok ? g_pass++ : g_fail++;
}

int main(void)
{
	puts("=== high-level capwap_acdisc API tests ===\n");

	/* preferred single IP */
	expect_ip("138 only",            "c0a80101", NULL,         "192.168.1.1");
	expect_ip("43 only",             NULL, "f104c0a80101",     "192.168.1.1");
	expect_ip("138 preferred over 43", "0a000001", "f104c0a80101", "10.0.0.1");
	expect_ip("both NULL -> fail",   NULL, NULL,               NULL);
	expect_ip("both empty -> fail",  "", "",                   NULL);
	expect_ip("malformed 138, good 43", "zz", "f104c0a80101",  NULL); /* bad hex -> -1 */
	expect_ip("invalid-only 138 -> fail", "00000000", NULL,    NULL);

	/* mirror the doc call site: env-style absent option is "" or NULL */
	expect_ip("env-style: opt138 set",   "0d3c03dd", "",       "13.60.3.221");

	/* full list, 138 then 43 fallbacks appended + de-duped */
	expect_list("list 138 two",      "c0a80101c0a80102", NULL,
		    "192.168.1.1,192.168.1.2");
	expect_list("list 138 + 43 append", "0a000001", "f104c0a80102",
		    "10.0.0.1,192.168.1.2");
	expect_list("list dedupe across 138/43", "0a000001", "f1040a000001",
		    "10.0.0.1");

	/* raw-bytes variant */
	{
		uint8_t o138[] = { 192,168,1,1 };
		char ip[16];
		int rc = capwap_acdisc_ip_bytes(o138, sizeof o138, NULL, 0,
						ip, sizeof ip);
		int ok = (rc == 0 && strcmp(ip, "192.168.1.1") == 0);
		printf("[%s] %-38s -> %s\n", ok ? "PASS" : "FAIL",
		       "bytes variant 138", rc == 0 ? ip : "(none)");
		ok ? g_pass++ : g_fail++;
	}

	printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
	return g_fail ? 1 : 0;
}
