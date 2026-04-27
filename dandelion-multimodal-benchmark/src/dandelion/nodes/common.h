#pragma once
/*
 * Shared helpers for all Dandelion compute nodes.
 * No libc dependency — only dandelion/runtime.h + dandelion/crt.h.
 *
 * Inter-node wire format:
 *   IoBuffer.ident     = request_id  (e.g. "t001")
 *   IoBuffer.ident_len = strlen(request_id)
 *   IoBuffer.data      = payload bytes (text = NUL-terminated string,
 *                                        audio/image = file path string)
 *   IoBuffer.data_len  = payload byte count
 *   IoBuffer.key       = modality  (0=text, 1=audio, 2=image)
 *
 * Entry point pattern for every node:
 *   #include "common.h"
 *   int main(void) { ... return 0; }
 *   DANDELION_ENTRY(main)
 *
 * Real SDK API (dandelion/runtime.h):
 *   dandelion_input_buffer_count(set_idx)       → size_t
 *   dandelion_get_input(set_idx, buf_idx)        → IoBuffer*
 *   dandelion_add_output(set_idx, IoBuffer buf)  → void (by value!)
 *   dandelion_alloc(size, alignment)             → void*
 *   dandelion_free(ptr)                          → void
 */

#include <dandelion/runtime.h>
#include <dandelion/crt.h>

/* ---------- convenience aliases for the real SDK API ---------- */

/* Number of input buffers in set `s` (0 if set doesn't exist). */
static inline size_t dn_input_count(size_t s) {
    return dandelion_input_buffer_count(s);
}

/* Get input buffer pointer (NULL if out-of-range). */
static inline IoBuffer *dn_get_input(size_t s, size_t i) {
    return dandelion_get_input(s, i);
}

/*
 * Allocate `data_cap` bytes, build an IoBuffer with a copy of ident,
 * add it to output set `s`, and return the data pointer for filling.
 * Returns NULL on allocation failure.
 */
static inline void *dn_add_output(size_t s,
                                   const char *ident, size_t ident_len,
                                   size_t data_cap, size_t key) {
    void *data = dandelion_alloc(data_cap, 1);
    if (!data) return (void *)0;

    char *id_copy = (char *)dandelion_alloc(ident_len + 1, 1);
    if (!id_copy) return (void *)0;
    for (size_t i = 0; i < ident_len; i++) id_copy[i] = ident[i];
    id_copy[ident_len] = '\0';

    IoBuffer out;
    out.ident     = id_copy;
    out.ident_len = ident_len;
    out.data      = data;
    out.data_len  = data_cap;   /* caller should set correct length after fill */
    out.key       = key;
    dandelion_add_output(s, out);
    return data;
}

/*
 * Like dn_add_output but copies a known payload immediately.
 * data_len is the actual payload length; data_cap = data_len+1 for NUL.
 */
static inline int dn_emit(size_t s,
                           const char *ident, size_t ident_len,
                           const void *payload, size_t payload_len,
                           size_t key) {
    char *data = (char *)dandelion_alloc(payload_len + 1, 1);
    if (!data) return -1;
    for (size_t i = 0; i < payload_len; i++) data[i] = ((const char*)payload)[i];
    data[payload_len] = '\0';

    char *id_copy = (char *)dandelion_alloc(ident_len + 1, 1);
    if (!id_copy) return -1;
    for (size_t i = 0; i < ident_len; i++) id_copy[i] = ident[i];
    id_copy[ident_len] = '\0';

    IoBuffer out;
    out.ident     = id_copy;
    out.ident_len = ident_len;
    out.data      = data;
    out.data_len  = payload_len;
    out.key       = key;
    dandelion_add_output(s, out);
    return 0;
}

/* ---------- minimal string helpers (no libc) ---------- */

static inline size_t dn_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static inline void dn_memcpy(void *dst, const void *src, size_t n) {
    char *d = (char *)dst;
    const char *s = (const char *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

static inline int dn_memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *p = (const unsigned char *)a;
    const unsigned char *q = (const unsigned char *)b;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != q[i]) return (int)p[i] - (int)q[i];
    }
    return 0;
}

static inline void dn_memset(void *dst, int c, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    for (size_t i = 0; i < n; i++) d[i] = (unsigned char)c;
}

static inline char dn_tolower(char c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

static inline const char *dn_memmem(const char *hay, size_t hl,
                                     const char *needle, size_t nl) {
    if (nl == 0) return hay;
    if (nl > hl) return (void *)0;
    for (size_t i = 0; i + nl <= hl; i++) {
        if (dn_memcmp(hay + i, needle, nl) == 0) return hay + i;
    }
    return (void *)0;
}

/* Case-insensitive substring search. */
static inline int dn_contains(const char *text, size_t tlen, const char *kw) {
    size_t klen = dn_strlen(kw);
    if (klen > tlen) return 0;
    for (size_t i = 0; i + klen <= tlen; i++) {
        int match = 1;
        for (size_t j = 0; j < klen; j++) {
            if (dn_tolower(text[i+j]) != dn_tolower(kw[j])) { match = 0; break; }
        }
        if (match) return 1;
    }
    return 0;
}

/*
 * Extract value of first "key":"value" pair from json[0..json_len).
 * Writes to out[0..out_max), NUL-terminates, sets *out_len.
 * Returns 0 on success, -1 if not found.
 */
static inline int dn_json_get(const char *json, size_t json_len,
                               const char *key,
                               char *out, size_t out_max, size_t *out_len) {
    char needle[128];
    size_t klen = dn_strlen(key);
    if (klen + 4 >= sizeof(needle)) return -1;
    needle[0] = '"';
    dn_memcpy(needle + 1, key, klen);
    needle[1 + klen] = '"';
    needle[2 + klen] = ':';
    needle[3 + klen] = '"';
    needle[4 + klen] = '\0';
    size_t nlen = 4 + klen;

    const char *pos = dn_memmem(json, json_len, needle, nlen);
    if (!pos) return -1;
    pos += nlen;
    size_t remaining = json_len - (size_t)(pos - json);
    size_t ei = 0;
    while (ei < remaining && pos[ei] != '"') ei++;
    size_t vlen = ei;
    if (vlen >= out_max) vlen = out_max - 1;
    dn_memcpy(out, pos, vlen);
    out[vlen] = '\0';
    *out_len = vlen;
    return 0;
}

/* ---------- uint → ASCII ---------- */
static inline size_t dn_uitoa(unsigned long v, char *buf, size_t buf_max) {
    if (buf_max == 0) return 0;
    char tmp[24]; int i = 0;
    if (v == 0) { tmp[i++] = '0'; }
    while (v > 0) { tmp[i++] = '0' + (v % 10); v /= 10; }
    size_t len = (size_t)i;
    if (len >= buf_max) len = buf_max - 1;
    for (size_t j = 0; j < len; j++) buf[j] = tmp[len - 1 - j];
    buf[len] = '\0';
    return len;
}

/* ---------- append string to growing char buffer ---------- */
static inline size_t dn_append(char *dst, size_t pos, size_t cap,
                                const char *src, size_t slen) {
    if (pos >= cap) return pos;
    size_t room = cap - pos - 1;
    if (slen > room) slen = room;
    dn_memcpy(dst + pos, src, slen);
    dst[pos + slen] = '\0';
    return pos + slen;
}
