/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * capwap_acdisc_request.h - "make the DHCP client ask for opt 43/138" API.
 *
 * Requesting an option is inherently platform-specific (it means configuring
 * the platform's DHCP client to put the option codes in its Parameter Request
 * List, DHCP option 55). This header is the GENERIC interface; the actual work
 * is done by a compile-time backend:
 *
 *   capwap_acdisc_request_uci.c   - OpenWrt / QSDK: writes UCI network.<iface>.reqopts
 *                                   via libuci (needs -luci).  <-- ship this on QSDK
 *   capwap_acdisc_request_null.c  - no-op (returns 0): platforms whose DHCP
 *                                   client already requests, or host unit tests.
 *
 * A different platform just drops in its own .c implementing this prototype
 * (e.g. editing dhclient.conf's "request" line) — the rest of the library and
 * the CAPWAP caller are unchanged. That is the plug-and-play seam.
 */
#ifndef _CAPWAP_ACDISC_REQUEST_H_
#define _CAPWAP_ACDISC_REQUEST_H_

/* CAPWAP AC-discovery DHCP option codes (RFC 5417 / vendor). */
#define CAPWAP_DHCP_OPT_AC_V4   138   /* RFC 5417 CAPWAP Access Controller */
#define CAPWAP_DHCP_OPT_VENDOR   43   /* vendor-specific (sub-opt 0xF1)    */

/*
 * Ensure the DHCP client on `iface` (e.g. "wan") requests options 43 and 138.
 * Idempotent: adding when already present is a no-op. Safe to call at every
 * daemon start.
 *
 * Returns:
 *    0  success (already requested, or added successfully)
 *   -1  error (backend failure)
 *
 * NOTE: the change takes effect on the next DHCP lease/renew. To apply it
 * immediately the caller (or platform) must reload the interface; see the
 * backend's notes. Passing iface == NULL applies to a backend default
 * ("wan" on the UCI backend).
 */
int capwap_acdisc_request_options(const char *iface);

#endif /* _CAPWAP_ACDISC_REQUEST_H_ */
