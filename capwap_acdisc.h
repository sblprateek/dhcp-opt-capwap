/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * capwap_acdisc.h - portable CAPWAP AC-discovery API.
 *
 * Drop-in for ANY CAPWAP WTP implementation. Pure C (stdint/string/stdio only):
 * no message bus, no OS calls, no daemon. The caller passes whatever DHCP
 * option data its own DHCP client already exposed (option 138 and/or 43, as
 * hex strings or raw bytes); the library decodes, validates, de-dupes, and
 * returns the AC IPv4 address(es) to use.
 *
 *   Preference: option 138 (RFC 5417) wins; option 43 (vendor) is the fallback.
 *
 * Typical call site (in the WTP, where it needs the AC address):
 *
 *     char ac[16];
 *     if (capwap_acdisc_ip(getenv("opt138"), getenv("opt43"),
 *                          ac, sizeof ac) == 0)
 *         capwap_start_discovery(ac);     // got AC IP from DHCP
 *     else
 *         capwap_start_discovery(conf_ac); // fall back to static config
 *
 * See capwap_dhcp_opt.h for the low-level byte decoders this sits on.
 */
#ifndef _CAPWAP_ACDISC_H_
#define _CAPWAP_ACDISC_H_

#include <stdint.h>
#include <stddef.h>

/* ---- hex-string inputs (the common case: DHCP clients expose opt as hex) ---- */

/* Resolve the single preferred AC IPv4 from option 138 and/or 43 hex strings.
 * Either argument may be NULL or "" (absent). Writes a dotted-quad string
 * ("192.168.1.1") into ip_out (needs >= 16 bytes).
 * Returns 0 on success, -1 if no valid AC was found. */
int capwap_acdisc_ip(const char *opt138_hex, const char *opt43_hex,
		     char *ip_out, size_t ip_out_sz);

/* Resolve the full ordered AC list (138 first, then 43 fallbacks) from hex.
 * Fills ips_host[] (host byte order) up to max entries.
 * Returns the count written (>= 0), or -1 on a malformed hex input. */
int capwap_acdisc_list(const char *opt138_hex, const char *opt43_hex,
		       uint32_t *ips_host, unsigned max);

/* ---- raw-byte inputs (if the caller holds bytes, not hex) ---- */

/* Same as capwap_acdisc_ip() but from raw option value bytes. NULL/0 = absent. */
int capwap_acdisc_ip_bytes(const uint8_t *opt138, size_t n138,
			   const uint8_t *opt43,  size_t n43,
			   char *ip_out, size_t ip_out_sz);

/* ---- small helper ---- */

/* Format a host-order IPv4 as dotted-quad. Returns ip_out, or NULL on overflow. */
char *capwap_ac_ip_to_str(uint32_t ip_host, char *ip_out, size_t ip_out_sz);

#endif /* _CAPWAP_ACDISC_H_ */
