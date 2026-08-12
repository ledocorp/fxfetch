// fxfetch_lib — HTTPS GET under NetCap (--cli).
module fxfetch_lib;

using core;
import std/io;
import std/string;
import std/strutil;

extern "c" {
    fn fx_cli_argc() -> i32;
    fn fx_cli_arg(i: i32) -> string;
    effects { alloc } fn fx_guest_begin(root: string, arena_bytes: i64) -> i64;
    effects { alloc } fn fx_guest_end(ctx_handle: i64) -> i32;
    effects { alloc } fn fx_guest_mint_fscap(ctx_handle: i64, root: string) -> i64;
    effects { alloc, io } fn fx_fetch_https_get(
        allow_host: string,
        url: string,
        ca_file: string,
        guest_handle: i64,
        fs_handle: i64,
        out_path: string,
        headers_only: i32
    ) -> i32;
}

fn eq(a: string, b: string) -> bool {
    return string.compare(a, b);
}

fn usage() -> i32 effects { io } {
    let _u = io.write_err(
        "usage: fxfetch --allow-host <host[:port]> --ca <pem> [--headers-only] [--out <file> --allow <dir>] <https-url>"
    );
    return 1;
}

fn map_st(st: i32) -> i32 effects { io } {
    if (st == 0) {
        return 0;
    }
    if (st == -1) {
        let _u = io.write_err("fxfetch: bad flags / url (https:// only)");
        return 1;
    }
    if (st == -2) {
        let _d = io.write_err("fxfetch: host denied / allow mismatch / path deny");
        return 2;
    }
    if (st == -3) {
        let _t = io.write_err("fxfetch: TLS / network failure");
        return 3;
    }
    if (st == -4) {
        let _p = io.write_err("fxfetch: HTTP parse failure");
        return 4;
    }
    let _i = io.write_err("fxfetch: I/O failure");
    return 3;
}

fn path_has_dotdot(s: string) -> bool {
    return strutil.contains(s, "..");
}

fn resolve_under(allow: string, rel: string) -> Result<string, core_Err> effects { alloc } {
    let al = string.len(allow);
    let dl = string.len(rel);
    if (dl > al) {
        if (strutil.starts_with(rel, allow) == true) {
            let c = string.byte_at(rel, al);
            if (c == 47) {
                return Ok(rel);
            }
            if (c == 92) {
                return Ok(rel);
            }
        }
    }
    let mid = string.concat(allow, "/")?;
    return string.concat(mid, rel);
}

fn cli_main() -> Result<i32, core_Err> effects { alloc, io } {
    let allow_host = "";
    let ca = "";
    let url = "";
    let out_path = "";
    let allow_dir = "";
    let headers_only: i32 = 0;
    let argc = fx_cli_argc();
    let i: i32 = 1;

    while (i < argc) {
        let a = fx_cli_arg(i);
        if (eq(a, "--help") == true) {
            return Ok(usage());
        }
        if (eq(a, "-h") == true) {
            return Ok(usage());
        }
        if (eq(a, "--headers-only") == true) {
            headers_only = 1;
            i = i + 1;
        } else {
            if (eq(a, "--allow-host") == true) {
                i = i + 1;
                if (i >= argc) {
                    let _m = io.write_err("fxfetch: --allow-host requires a value");
                    return Ok(1);
                }
                allow_host = fx_cli_arg(i);
                i = i + 1;
            } else {
                if (eq(a, "--ca") == true) {
                    i = i + 1;
                    if (i >= argc) {
                        let _m = io.write_err("fxfetch: --ca requires a PEM path");
                        return Ok(1);
                    }
                    ca = fx_cli_arg(i);
                    i = i + 1;
                } else {
                    if (eq(a, "--out") == true) {
                        i = i + 1;
                        if (i >= argc) {
                            let _m = io.write_err("fxfetch: --out requires a path");
                            return Ok(1);
                        }
                        out_path = fx_cli_arg(i);
                        i = i + 1;
                    } else {
                        if (eq(a, "--allow") == true) {
                            i = i + 1;
                            if (i >= argc) {
                                let _m = io.write_err("fxfetch: --allow requires a directory");
                                return Ok(1);
                            }
                            allow_dir = fx_cli_arg(i);
                            i = i + 1;
                        } else {
                            if (string.len(url) != 0) {
                                let _m = io.write_err("fxfetch: unexpected extra argument");
                                return Ok(1);
                            }
                            url = a;
                            i = i + 1;
                        }
                    }
                }
            }
        }
    }

    if (string.len(allow_host) == 0) {
        let _m = io.write_err("fxfetch: --allow-host is required");
        return Ok(1);
    }
    if (string.len(ca) == 0) {
        let _m = io.write_err("fxfetch: --ca is required");
        return Ok(1);
    }
    if (string.len(url) == 0) {
        return Ok(usage());
    }
    if (strutil.starts_with(url, "https://") == false) {
        let _m = io.write_err("fxfetch: url must be https://");
        return Ok(1);
    }
    if (string.len(out_path) != 0) {
        if (string.len(allow_dir) == 0) {
            let _m = io.write_err("fxfetch: --out requires --allow <dir>");
            return Ok(1);
        }
        if (path_has_dotdot(out_path) == true) {
            let _m = io.write_err("fxfetch: --out path must not contain ..");
            return Ok(1);
        }
    }

    let g_root = "";
    if (string.len(allow_dir) != 0) {
        g_root = allow_dir;
    }
    let g = fx_guest_begin(g_root, 65536);
    if (g == 0) {
        let _g = io.write_err("fxfetch: guest begin failed");
        return Ok(2);
    }

    let fs: i64 = 0;
    let op = "";
    if (string.len(out_path) != 0) {
        fs = fx_guest_mint_fscap(g, "");
        if (fs == 0) {
            let _e0 = fx_guest_end(g);
            let _f = io.write_err("fxfetch: mint_fs failed");
            return Ok(2);
        }
        let resolved = resolve_under(allow_dir, out_path)?;
        op = resolved;
    }

    let st = fx_fetch_https_get(allow_host, url, ca, g, fs, op, headers_only);
    let _en = fx_guest_end(g);
    return Ok(map_st(st));
}
