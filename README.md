# capwap-acdisc — CAPWAP AC discovery from DHCP option 43 / 138

A tiny, dependency-free C library that **any CAPWAP WTP implementation** can
compile in and call to turn DHCP option **43** (vendor-specific) and **138**
(RFC 5417 CAPWAP-AC) into the **AC IP address** to connect to.

- **Pure C** — `<stdint.h>`, `<string.h>`, `<stdio.h>` only. No message bus, no
  sockets, no OS calls, no daemon, no threads.
- **You call a function, you get an IP string back.** That's the whole API.
- **Caller supplies the option data.** The library does not fetch from the
  network — you pass in whatever your DHCP client already exposed (hex string or
  raw bytes). This is what keeps it portable across every platform.
- **Verified on real hardware** — decoders pass 26/26 tests on x86 (under
  ASan/UBSan) and on an armv7l OpenWrt AP.

> License: **BSD-2-Clause** — usable by any WTP, open or closed. See `LICENSE`.
> Architecture: see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

---

## TL;DR — which function gives me the AC IP?

```c
#include "capwap_acdisc.h"

char ac[16];
if (capwap_acdisc_ip(opt138_hex, opt43_hex, ac, sizeof ac) == 0)
    /* ac now holds "192.168.1.1" — the AC address to connect to */ ;
```

**`capwap_acdisc_ip()`** is the one to call — it returns the preferred AC IP as a
string (`0` = success, `-1` = none found). Pass it the hex strings your DHCP
client exposed for options 138 and 43 (either may be `NULL`). Want the full list
instead of just the best one? Use `capwap_acdisc_list()`. Have raw bytes instead
of hex? Use `capwap_acdisc_ip_bytes()`.

To also make the DHCP client *ask* for the options, call
**`capwap_acdisc_request_options("wan")`** once at startup.

---

## Files to add to your build

**Portable core** (zero dependencies — compiles into any WTP, any OS):

| File | Purpose |
|---|---|
| `capwap_acdisc.h` / `capwap_acdisc.c` | **The decode API.** option data → AC IP. |
| `capwap_dhcp_opt.h` / `capwap_dhcp_opt.c` | Low-level byte decoders + validation (option 138 list, option 43 TLV sub-opt `0xF1`). |

**Request backend** (pick exactly ONE — this is the platform-specific seam):

| File | Use on | Needs |
|---|---|---|
| `capwap_acdisc_request_uci.c` | **OpenWrt / QSDK** — sets `network.<iface>.reqopts` via libuci | `-luci` (DEPENDS += libuci) |
| `capwap_acdisc_request_null.c` | platforms whose DHCP client already requests; host tests | nothing |
| `capwap_acdisc_request.h` | the generic prototype both implement | — |

The core is dependency-free. Only the **request backend** is platform-specific,
and it's isolated to one swappable file. A new platform drops in its own backend
(e.g. a dhclient.conf editor) implementing the one prototype.

---

## The API

```c
#include "capwap_acdisc_request.h"   /* step 1: ask for the options */
#include "capwap_acdisc.h"           /* step 2: decode them to an IP */

/* --- requesting (call once, e.g. at daemon init) --- */
/* Ensure the DHCP client requests opt 43 + 138 on iface. Idempotent. 0/-1. */
int  capwap_acdisc_request_options(const char *iface);

/* --- decoding (call when you need the AC) --- */
/* Preferred single AC IP (138 wins, 43 is fallback). 0 = ok, -1 = none. */
int  capwap_acdisc_ip(const char *opt138_hex, const char *opt43_hex,
                      char *ip_out, size_t ip_out_sz);     /* ip_out >= 16 */

/* Full ordered list (138 first, 43 appended, de-duped). Returns count / -1. */
int  capwap_acdisc_list(const char *opt138_hex, const char *opt43_hex,
                        uint32_t *ips_host, unsigned max);

/* Same as _ip() but from raw option value bytes instead of hex. */
int  capwap_acdisc_ip_bytes(const uint8_t *opt138, size_t n138,
                            const uint8_t *opt43,  size_t n43,
                            char *ip_out, size_t ip_out_sz);

/* host-order IPv4 -> "a.b.c.d" */
char *capwap_ac_ip_to_str(uint32_t ip_host, char *out, size_t sz);
```

---

## Integration: two call sites in your WTP

**① Once at startup — make the DHCP client ask for the options:**

```c
capwap_acdisc_request_options("wan");   /* UCI backend writes reqopts=43,138 */
```

On QSDK this is the C equivalent of your `99-capwap-reqopts` script — the library
now owns it. Idempotent, so calling it every boot is fine. Takes effect on the
next DHCP lease/renew.

**② When you need the AC — decode whatever the DHCP client received:**

```c
char ac[16];

if (capwap_acdisc_ip(getenv("opt138"), getenv("opt43"), ac, sizeof ac) == 0)
    capwap_start_discovery(ac);       /* AC IP learned from DHCP */
else
    capwap_start_discovery(conf_ac);  /* fall back to static config */
```

`opt138` / `opt43` here are whatever your platform's DHCP client exposed for
those option codes. Either may be `NULL` or `""` (absent). The fallback branch
is your safety net: if there is no DHCP option (or it is unparseable) the WTP
behaves exactly as it did before.

### Where do `opt138` / `opt43` come from?

That part is platform-specific and **outside** this library (so the library
stays portable). You feed it whatever your DHCP client gives you, for example:

| Platform / DHCP client | Where the option appears | How you pass it |
|---|---|---|
| busybox `udhcpc` (OpenWrt/QSDK) | env vars `opt43` / `opt138` (lowercase hex) in the notify script's environment | `getenv("opt138")` |
| ISC dhclient | a dhclient-script hook / lease file | read the value, pass the hex |
| Raw DHCP parsing in your own code | you already have the bytes | use `capwap_acdisc_ip_bytes()` |

> Confirmed for busybox 1.35.0 (from source `networking/udhcp/dhcpc.c`):
> requested-but-unnamed options are exported as `opt<CODE>=<lowercase-hex>`, so
> option 43 → `opt43`, option 138 → `opt138`. The codes must be in the client's
> request list (`reqopts`).

---

## Behaviour contract

- **Preference:** option 138 (standard) is used first; option 43 (vendor) only
  supplements / falls back.
- **Validation:** rejects `0.0.0.0`, `127/8`, `224/4`+ (multicast/reserved),
  `255.255.255.255`. De-dupes. Preserves first-seen order = priority.
- **Bounds-safe:** option 43's TLV length fields are attacker-controllable;
  the walk never reads past the buffer (verified under ASan/UBSan).
- **Return values:** `_ip()` returns `0` and writes a dotted-quad on success;
  `-1` if no valid AC (caller should fall back to static config).
- **Security note:** DHCP is unauthenticated — a discovered AC IP is only a
  *pointer*. Trust must still be established by the CAPWAP control channel
  (DTLS/X.509). This library only decodes; it does not authenticate.

---

## Build & test (host)

```bash
make test      # builds + runs both suites under AddressSanitizer + UBSan
make           # builds the portable static lib (libcapwapacdisc.a)
make clean
```

Both suites are warning-clean under `-Werror` and pass under
AddressSanitizer + UBSan. The UCI request backend (`capwap_acdisc_request_uci.c`)
is compiled on-target against the platform's `libuci` (link with `-luci`).

## QSDK build wiring

In the `freewtp-km` package (or your WTP's package):

1. Add the core `.c` files + `capwap_acdisc_request_uci.c` to the build sources.
2. Add libuci to deps: `DEPENDS += +libuci`.
3. Call `capwap_acdisc_request_options("wan")` at daemon init and
   `capwap_acdisc_ip(...)` at the discovery seam.
4. You can then retire the `99-capwap-reqopts` / `40-capwap-ac` /
   `capwap-dhcp-ac` shell scripts — the library replaces all three.
