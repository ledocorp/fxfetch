# fxfetch — user design summary

fxfetch is a **narrow HTTPS GET CLI** under **NetCap**: allowlisted host:port, required CA file, dual-path bins.

## Why fxfetch

| Keep | Refuse (v1) |
|------|-------------|
| One-shot GET honesty | curl flag empire |
| Required `--allow-host` + `--ca` | Ambient network trust |
| Status/headers on stderr; body stdout or `--out` | Cookie jars / redirects-by-default |
| Dual-path emit-C + IR | Optional-IR theater |

## Exit codes

| Code | Meaning |
|------|---------|
| 0 | ok |
| 1 | usage / bad URL |
| 2 | host or path deny |
| 3 | TLS / network failure |
| 4 | other I/O |

## Rebuild

See root README. Needs fx 0.9.6+ with `--cli`, `host/cap` TLS dial, and Mbed TLS 2.28.x.

## Non-goals

POST · cookies · redirects · HTTP/2/3 · macOS claim
