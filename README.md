# fxfetch

**HTTPS GET under NetCap** for [fx](https://github.com/ledocorp/fxlang) projects.

fxfetch performs one HTTPS GET with an explicit host allowlist and CA file. Status and response headers go to stderr; body to stdout (or `--out` under `--allow`). Product logic is **fx**; rebuild with `fx build … --cli`. Dual-path: emit-C and IR.

| | |
|--|--|
| **Requires** | [fx](https://github.com/ledocorp/fxlang) **0.9.6+** (with `--cli`) · Mbed TLS for rebuild |
| **Platforms** | Windows + Linux **x86_64** |
| **License** | Apache-2.0 (tool) · Apache-2.0 (Mbed TLS) |
| **Org** | [LedoCorp](http://www.ledocorp.org) |

## Install (release binaries)

1. Install [fx 0.9.6+](https://github.com/ledocorp/fxlang/releases/tag/v0.9.6).  
2. Download the asset for your OS from [Releases](https://github.com/ledocorp/fxfetch/releases).  
3. Put `bin/windows/fxfetch.exe` or `bin/linux/fxfetch` on your `PATH`.

```text
# Windows (PowerShell)
Invoke-WebRequest -Uri https://github.com/ledocorp/fxfetch/releases/download/v0.1.0/fxfetch-0.1.0-windows-x86_64.zip -OutFile fxfetch.zip
Expand-Archive fxfetch.zip -DestinationPath .
.\bin\windows\fxfetch.exe --help

# Linux
curl -LO https://github.com/ledocorp/fxfetch/releases/download/v0.1.0/fxfetch-0.1.0-linux-x86_64.tar.gz
tar xzf fxfetch-0.1.0-linux-x86_64.tar.gz
./bin/linux/fxfetch --help
```

Optional: `fxfetch-ir` is the IR dual-path binary (same CLI).

## Quick start

```text
fxfetch --allow-host example.com --ca /path/to/ca-bundle.pem https://example.com/
fxfetch --allow-host example.com --ca ca.pem --headers-only https://example.com/
fxfetch --allow-host example.com --ca ca.pem --out body.bin --allow . https://example.com/file
```

## CLI

| Flag / arg | Behavior |
|------------|----------|
| `--allow-host <host[:port]>` | Required NetCap allow (default port 443) |
| `--ca <pem>` | Required CA trust file |
| `--headers-only` | Skip body |
| `--out <file> --allow <dir>` | Write body under FsCap root |
| `<https-url>` | Must be `https://…` |

Exit codes: `0` ok · `1` usage · `2` host/path deny · `3` TLS/network · `4` other I/O.

## Rebuild from source

Needs fx 0.9.6+ with `--cli`, `host/cap` (incl. dial TLS), this repo’s `fx_fetch_ref.c`, and Mbed TLS 2.28.x libs:

```text
fx build fxfetch_lib.fx -o out --emit-c --cli \
  --link fx_fetch_ref.c \
  --link <host>/cap/fx_net_dial.c \
  --link <host>/cap/fx_cap_runtime.c \
  --link-include <host>/cap \
  --link-include <mbedtls>/include \
  --link-dir <mbedtls>/library \
  --link-lib mbedtls --link-lib mbedx509 --link-lib mbedcrypto
```

On Windows also `--link-lib ws2_32`. Same links with `--backend ir` for the IR binary.

## Non-goals (v1)

POST · forms · cookies · redirects · HTTP/2/3 · browser theater · ambient network · macOS prebuilt claim

## Docs

- [docs/FXFETCH.md](docs/FXFETCH.md) — design summary  
- [docs/releases/](docs/releases/) — release notes  
- Language: [ledocorp/fxlang](https://github.com/ledocorp/fxlang)

## License

Copyright Shawn Londono · LedoCorp · Apache-2.0 — see [LICENSE](LICENSE).
