/* fxfetch — HTTPS GET helper under NetCap (+ optional FsCap for --out). */
#include "fx_cap_runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    FX_FETCH_OK = 0,
    FX_FETCH_USAGE = -1,
    FX_FETCH_DENY = -2,
    FX_FETCH_TLS = -3,
    FX_FETCH_PARSE = -4,
    FX_FETCH_IO = -5
};

#define FX_FETCH_MAX_RESP (16 * 1024 * 1024)
#define FX_FETCH_MAX_REQ 4096

static int parse_allow_host(const char *s, char *host, size_t host_sz, int *port) {
    const char *colon;
    size_t n;
    if (s == NULL || s[0] == '\0' || host == NULL || port == NULL) {
        return 0;
    }
    *port = 443;
    colon = strrchr(s, ':');
    if (colon != NULL && colon != s) {
        /* IPv6 not supported in v1 — single colon = port. */
        if (strchr(s, ':') == colon) {
            n = (size_t)(colon - s);
            if (n == 0 || n >= host_sz) {
                return 0;
            }
            memcpy(host, s, n);
            host[n] = '\0';
            *port = atoi(colon + 1);
            if (*port <= 0 || *port > 65535) {
                return 0;
            }
            return 1;
        }
    }
    n = strlen(s);
    if (n >= host_sz) {
        return 0;
    }
    memcpy(host, s, n + 1);
    return 1;
}

/* Parse https://host[:port]/path] → host/port/path. Returns 1 ok. */
static int parse_https_url(const char *url, char *host, size_t host_sz, int *port, char *path,
                           size_t path_sz) {
    const char *p;
    const char *slash;
    const char *colon;
    size_t hn;
    if (url == NULL || host == NULL || port == NULL || path == NULL) {
        return 0;
    }
    if (strncmp(url, "https://", 8) != 0) {
        return 0;
    }
    p = url + 8;
    if (*p == '\0') {
        return 0;
    }
    slash = strchr(p, '/');
    colon = strchr(p, ':');
    *port = 443;
    if (colon != NULL && (slash == NULL || colon < slash)) {
        hn = (size_t)(colon - p);
        if (hn == 0 || hn >= host_sz) {
            return 0;
        }
        memcpy(host, p, hn);
        host[hn] = '\0';
        *port = atoi(colon + 1);
        if (*port <= 0 || *port > 65535) {
            return 0;
        }
    } else {
        hn = slash ? (size_t)(slash - p) : strlen(p);
        if (hn == 0 || hn >= host_sz) {
            return 0;
        }
        memcpy(host, p, hn);
        host[hn] = '\0';
    }
    if (slash == NULL || slash[0] == '\0') {
        if (path_sz < 2) {
            return 0;
        }
        path[0] = '/';
        path[1] = '\0';
        return 1;
    }
    if (strlen(slash) >= path_sz) {
        return 0;
    }
    memcpy(path, slash, strlen(slash) + 1);
    return 1;
}

static int host_eq(const char *a, const char *b) {
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static void write_status_line(const char *headers, size_t hdr_len) {
    const char *nl;
    size_t n;
    nl = (const char *)memchr(headers, '\n', hdr_len);
    if (nl == NULL) {
        n = hdr_len;
    } else {
        n = (size_t)(nl - headers);
        if (n > 0 && headers[n - 1] == '\r') {
            n--;
        }
    }
    fprintf(stderr, "%.*s\n", (int)n, headers);
}

/*
 * fx_fetch_https_get — product helper.
 * allow_host: --allow-host value (host or host:port)
 * url: https://...
 * ca_file: PEM CA (required)
 * fs_handle: FsCap for --out (0 if unused)
 * out_path: NULL → body to stdout; else path under FsCap
 * headers_only: nonzero → skip body
 *
 * Returns FX_FETCH_* (0 ok; negative codes as above).
 */
int32_t fx_fetch_https_get(const char *allow_host, const char *url, const char *ca_file,
                           int64_t guest_handle, int64_t fs_handle, const char *out_path,
                           int32_t headers_only) {
    char allow_h[256];
    char url_h[256];
    char path[2048];
    char req[FX_FETCH_MAX_REQ];
    char *resp = NULL;
    int allow_port = 443;
    int url_port = 443;
    int64_t net = 0;
    int64_t sock = 0;
    int32_t n;
    int32_t total = 0;
    size_t hdr_end = 0;
    size_t i;
    FILE *outf = NULL;
    int rc = FX_FETCH_IO;

    if (allow_host == NULL || url == NULL || ca_file == NULL || ca_file[0] == '\0') {
        return FX_FETCH_USAGE;
    }
    if (!parse_allow_host(allow_host, allow_h, sizeof(allow_h), &allow_port)) {
        return FX_FETCH_USAGE;
    }
    if (!parse_https_url(url, url_h, sizeof(url_h), &url_port, path, sizeof(path))) {
        return FX_FETCH_USAGE;
    }
    if (!host_eq(allow_h, url_h) || allow_port != url_port) {
        return FX_FETCH_DENY;
    }
    if (out_path != NULL && out_path[0] != '\0') {
        FxFsCap *fs;
        if (fs_handle == 0) {
            return FX_FETCH_DENY;
        }
        fs = fx_fscap_from_handle(fs_handle);
        if (!fx_fscap_path_allowed(fs, out_path)) {
            return FX_FETCH_DENY;
        }
    }

    net = fx_guest_mint_netcap(guest_handle, allow_h, allow_port, allow_port);
    if (net == 0) {
        return FX_FETCH_DENY;
    }
    if (fx_netcap_set_ca_file(ca_file) != 0) {
        return FX_FETCH_USAGE;
    }

    sock = fx_netcap_dial(net, url_h, (int32_t)url_port, 1);
    if (sock == 0) {
        int le = fx_netcap_last_error();
        return le == 5 ? FX_FETCH_DENY : FX_FETCH_TLS;
    }

    n = snprintf(req, sizeof(req),
                 "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\nUser-Agent: fxfetch/0.1\r\n\r\n",
                 path, url_h);
    if (n <= 0 || n >= (int)sizeof(req)) {
        fx_netcap_close(sock);
        return FX_FETCH_IO;
    }
    if (fx_netcap_write(sock, req, n) != n) {
        fx_netcap_close(sock);
        return FX_FETCH_TLS;
    }

    resp = (char *)malloc(FX_FETCH_MAX_RESP);
    if (resp == NULL) {
        fx_netcap_close(sock);
        return FX_FETCH_IO;
    }
    total = 0;
    for (;;) {
        if (total >= FX_FETCH_MAX_RESP - 1) {
            free(resp);
            fx_netcap_close(sock);
            return FX_FETCH_IO;
        }
        n = fx_netcap_read(sock, resp + total, FX_FETCH_MAX_RESP - 1 - total);
        if (n < 0) {
            free(resp);
            fx_netcap_close(sock);
            return FX_FETCH_TLS;
        }
        if (n == 0) {
            break;
        }
        total += n;
    }
    fx_netcap_close(sock);
    sock = 0;
    resp[total] = '\0';

    /* Find header/body split. */
    hdr_end = 0;
    for (i = 0; i + 3 < (size_t)total; i++) {
        if (resp[i] == '\r' && resp[i + 1] == '\n' && resp[i + 2] == '\r' && resp[i + 3] == '\n') {
            hdr_end = i + 4;
            break;
        }
    }
    if (hdr_end == 0) {
        free(resp);
        return FX_FETCH_PARSE;
    }
    if (strncmp(resp, "HTTP/", 5) != 0) {
        free(resp);
        return FX_FETCH_PARSE;
    }

    write_status_line(resp, hdr_end);
    /* Headers block including final blank line → stderr */
    fwrite(resp, 1, hdr_end, stderr);

    if (headers_only == 0) {
        const char *body = resp + hdr_end;
        size_t body_len = (size_t)total - hdr_end;
        if (out_path != NULL && out_path[0] != '\0') {
            outf = fopen(out_path, "wb");
            if (outf == NULL) {
                free(resp);
                return FX_FETCH_IO;
            }
            if (body_len > 0 && fwrite(body, 1, body_len, outf) != body_len) {
                fclose(outf);
                free(resp);
                return FX_FETCH_IO;
            }
            fclose(outf);
        } else if (body_len > 0) {
            if (fwrite(body, 1, body_len, stdout) != body_len) {
                free(resp);
                return FX_FETCH_IO;
            }
        }
    }

    free(resp);
    return FX_FETCH_OK;
}

/* Expose parsers for smoke/unit without full GET (optional). */
int32_t fx_fetch_parse_allow_host(const char *s, char *host_out, int32_t host_cap, int32_t *port_out) {
    char tmp[256];
    int port = 443;
    if (host_out == NULL || port_out == NULL || host_cap < 2) {
        return 0;
    }
    if (!parse_allow_host(s, tmp, sizeof(tmp), &port)) {
        return 0;
    }
    if ((int)strlen(tmp) >= host_cap) {
        return 0;
    }
    memcpy(host_out, tmp, strlen(tmp) + 1);
    *port_out = port;
    return 1;
}
