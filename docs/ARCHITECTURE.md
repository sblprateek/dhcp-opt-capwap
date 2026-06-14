# Architecture — capwap-acdisc

How this library fits into a CAPWAP WTP, and why it is split the way it is.

---

## 1. The problem

A CAPWAP WTP (access point) needs the **AC's IP address** before it can start a
control channel. The zero-touch way to get it: the DHCP exchange that gives the
WTP its own IP can also carry the AC address inside a DHCP option —
**option 138** (RFC 5417, the standard) or **option 43** (vendor-specific).

So three things must happen:

1. **REQUEST** — the WTP's DHCP client must *ask* the server to include
   option 43/138 (via the DHCP Parameter Request List, option 55).
2. **RECEIVE** — the DHCP client gets the reply and exposes the raw option
   bytes (e.g. busybox `udhcpc` exports them as env vars `opt43` / `opt138`).
3. **DECODE** — those bytes must be parsed into a usable AC IP address.

This library covers **DECODE** (always) and **REQUEST** (via an optional,
swappable backend). RECEIVE belongs to the platform's DHCP client and is never
touched.

---

## 2. The three-job split

```
 ┌───────────────┐     ┌───────────────┐     ┌─────────────────────┐
 │ ① REQUEST     │ ──► │ ② RECEIVE     │ ──► │ ③ DECODE            │
 │ ask for 43/138│     │ DHCP client   │     │ bytes/hex → AC IP   │
 └───────────────┘     └───────────────┘     └─────────────────────┘
   library (optional      platform DHCP         library (always)
   UCI/other backend)     client — untouched    portable, zero-dep
```

- **③ DECODE is the heart of the library** and is 100% portable — pure C, no
  network, no OS calls. Same source drops into any WTP on any OS.
- **① REQUEST is platform-specific** (it means configuring the DHCP client), so
  it lives behind a generic interface with a per-platform backend. Ship the one
  that matches your platform; the rest of the library is unchanged.
- **② RECEIVE is not the library's job.** The caller hands in whatever bytes its
  DHCP client produced.

---

## 3. Component map

```
                       your CAPWAP WTP daemon
                                 │
          ┌──────────────────────┼───────────────────────┐
          │ at startup           │ when it needs the AC   │
          ▼                      ▼                        │
  capwap_acdisc_request_options()   capwap_acdisc_ip()    │
          │                      │                        │
   ┌──────┴──────┐               │                        │
   ▼             ▼               ▼                        │
 _request_uci.c  _request_null.c  capwap_acdisc.c  ◄──────┘  high-level API
 (libuci, QSDK)  (no-op)              │
                                      ▼
                              capwap_dhcp_opt.c            low-level decoders
                              (opt138 list / opt43 TLV,
                               validation, de-dupe)
```

| Layer | Files | Depends on |
|---|---|---|
| High-level API | `capwap_acdisc.{c,h}` | the decoders only |
| Low-level decoders | `capwap_dhcp_opt.{c,h}` | nothing (libc) |
| Request interface | `capwap_acdisc_request.h` | — |
| Request backend (pick 1) | `capwap_acdisc_request_uci.c` **or** `capwap_acdisc_request_null.c` | libuci / nothing |

---

## 4. Data flow on an OpenWrt/QSDK WTP (concrete)

```
 DHCP server  ──ACK { ..., opt138 = c0a80101 }──►  busybox udhcpc
   ▲                                                     │
   │ (server includes 138 because the client asked)      │ exports env:
   │                                                      │   opt138=c0a80101
 ① capwap_acdisc_request_options("wan")                  ▼
   → UCI network.wan.reqopts += 43,138                 WTP daemon, at discovery:
   → netifd runs udhcpc -O 43 -O 138                  ③ capwap_acdisc_ip(
   → codes land in DHCP option 55                          getenv("opt138"),
                                                            getenv("opt43"),
                                                            ac, sizeof ac)
                                                         → ac = "192.168.1.1"
                                                         → start CAPWAP to ac
```

Steps ① and ③ are library calls. Everything between them is the stock platform
DHCP path.

---

## 5. Decode rules (what `capwap_dhcp_opt.c` enforces)

- **Option 138**: value is `N × 4` bytes; each 4-byte group is one AC IPv4.
  Length must be a non-zero multiple of 4.
- **Option 43**: walk TLV sub-options `[code][len][value…]`; harvest only
  sub-option **`0xF1`** (AC IPv4 list). All other sub-options are skipped.
  Truncated TLVs stop the walk — never an over-read (verified under ASan/UBSan).
- **Preference**: option 138 wins; option 43 supplements/falls back.
- **Validation**: reject `0.0.0.0`, `127/8`, `224/4`+ , `255.255.255.255`;
  de-dupe; preserve first-seen order = priority.

---

## 6. Security model

DHCP is unauthenticated — option 43/138 can be spoofed by any DHCP server on the
segment. **A discovered AC IP is only a pointer, never a trust decision.** Trust
must be established by the CAPWAP control channel itself (DTLS with X.509
certificates). A spoofed AC fails the certificate exchange and is rejected.

This library decodes; it does not authenticate. The bounds-checked parsers
prevent a malformed option from harming the WTP, but choosing whether to *trust*
the resulting AC is the control plane's responsibility.

---

## 7. Why requesting is a swappable backend, not built in

Requesting an option means touching the platform's DHCP client — there is no
portable, OS-agnostic way to do it. Hard-coding one method (UCI) would tie the
whole library to OpenWrt. Instead `capwap_acdisc_request.h` defines one
prototype and each platform supplies a backend `.c`:

- `capwap_acdisc_request_uci.c` — OpenWrt/QSDK, via `libuci`.
- `capwap_acdisc_request_null.c` — platforms whose DHCP client already requests.
- *(your platform)* — e.g. an ISC `dhclient.conf` editor implementing the same
  one-function prototype.

The decode core never changes. That is the plug-and-play seam.
