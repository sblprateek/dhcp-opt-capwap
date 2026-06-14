// SPDX-License-Identifier: BSD-2-Clause
/*
 * capwap_acdisc.c - portable CAPWAP AC-discovery API (see capwap_acdisc.h).
 */
#include <stdio.h>
#include "capwap_acdisc.h"
#include "capwap_dhcp_opt.h"

char *capwap_ac_ip_to_str(uint32_t ip, char *out, size_t sz)
{
	int n = snprintf(out, sz, "%u.%u.%u.%u",
			 (ip >> 24) & 0xff, (ip >> 16) & 0xff,
			 (ip >> 8) & 0xff, ip & 0xff);
	if (n < 0 || (size_t)n >= sz)
		return NULL;
	return out;
}

/* Decode both options into one ordered, validated, de-duped list (138 first).
 * Returns count, or -1 if a supplied hex string is malformed. */
static int resolve_list(const char *o138_hex, const char *o43_hex,
			struct ac_list *acs)
{
	uint8_t buf[256];
	int n;

	if (o138_hex && o138_hex[0]) {
		n = hexdecode(o138_hex, buf, sizeof buf);
		if (n < 0)
			return -1;
		decode_opt138(buf, (size_t)n, acs);   /* invalid opt138 -> 0 added */
	}
	if (o43_hex && o43_hex[0]) {
		n = hexdecode(o43_hex, buf, sizeof buf);
		if (n < 0)
			return -1;
		decode_opt43(buf, (size_t)n, acs);     /* appended after 138 */
	}
	return (int)acs->count;
}

int capwap_acdisc_list(const char *o138_hex, const char *o43_hex,
		       uint32_t *ips, unsigned max)
{
	struct ac_list acs = {0};
	int rc = resolve_list(o138_hex, o43_hex, &acs);
	if (rc < 0)
		return -1;
	for (unsigned i = 0; i < acs.count && i < max; i++)
		ips[i] = acs.ip[i];
	return (int)(acs.count < max ? acs.count : max);
}

int capwap_acdisc_ip(const char *o138_hex, const char *o43_hex,
		     char *ip_out, size_t ip_out_sz)
{
	struct ac_list acs = {0};
	if (resolve_list(o138_hex, o43_hex, &acs) <= 0)
		return -1;
	return capwap_ac_ip_to_str(acs.ip[0], ip_out, ip_out_sz) ? 0 : -1;
}

int capwap_acdisc_ip_bytes(const uint8_t *o138, size_t n138,
			   const uint8_t *o43, size_t n43,
			   char *ip_out, size_t ip_out_sz)
{
	struct ac_list acs = {0};

	if (o138 && n138)
		decode_opt138(o138, n138, &acs);
	if (acs.count == 0 && o43 && n43)
		decode_opt43(o43, n43, &acs);

	if (acs.count == 0)
		return -1;
	return capwap_ac_ip_to_str(acs.ip[0], ip_out, ip_out_sz) ? 0 : -1;
}
