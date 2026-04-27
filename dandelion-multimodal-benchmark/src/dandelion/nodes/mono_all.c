/*
 * mono_all — single Dandelion compute function handling all three modalities.
 *
 * Input  set 0, buf 0: JSON request {"id":"...","modality":"...","input":"..."}
 * Output set 0, buf 0: {"id":"...","intent":"...","modality":"..."}
 *
 * For text: inline normalize + classify.
 * For audio/image: two-phase reqwest_io.
 *   Phase A: set 1 empty  → emit HTTP request descriptor
 *   Phase B: set 1 filled → parse response, classify, emit result
 *
 * Modality is encoded in the key field of the Phase A output:
 *   key=1 → audio (use "transcript" field)
 *   key=2 → image (use "text" field)
 */

#include "common.h"

#define ASR_URL  "http://127.0.0.1:8765/asr"
#define ASR_ULEN 26
#define OCR_URL  "http://127.0.0.1:8765/ocr"
#define OCR_ULEN 26

static const char *classify(const char *t, size_t n) {
    if (dn_contains(t,n,"cancel")||dn_contains(t,n,"annul")||dn_contains(t,n,"abort"))
        return "cancel_reservation";
    if (dn_contains(t,n,"change")||dn_contains(t,n,"modif")||dn_contains(t,n,"update")||
        dn_contains(t,n,"reschedul")||dn_contains(t,n,"move")||dn_contains(t,n,"switch"))
        return "modify_reservation";
    if (dn_contains(t,n,"book")||dn_contains(t,n,"reserv")||dn_contains(t,n,"table")||
        dn_contains(t,n,"seat")||dn_contains(t,n,"spot")||dn_contains(t,n,"place"))
        return "book_reservation";
    if (dn_contains(t,n,"menu")||dn_contains(t,n,"food")||dn_contains(t,n,"dish")||
        dn_contains(t,n,"eat")||dn_contains(t,n,"meal")||dn_contains(t,n,"vegetarian")||
        dn_contains(t,n,"vegan")||dn_contains(t,n,"allergen")||dn_contains(t,n,"option"))
        return "ask_about_menu";
    return "unknown";
}

static void normalize(const char *src, size_t slen, char *dst, size_t *out_len, size_t cap) {
    size_t pos = 0;
    for (size_t i = 0; i < slen && pos < cap-1; i++) {
        char c = src[i];
        if ((c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' ') { dst[pos++]=c; }
        else if (c>='A'&&c<='Z') { dst[pos++]=c+32; }
        else { if (pos>0&&dst[pos-1]!=' ') dst[pos++]=' '; }
    }
    while (pos>0&&dst[pos-1]==' ') pos--;
    dst[pos]='\0';
    *out_len=pos;
}

static void emit_result(const char *ident, size_t ident_len,
                         const char *intent, const char *modality) {
    char result[256]; size_t rpos = 0;
    rpos = dn_append(result, rpos, sizeof(result), "{\"id\":\"",        7);
    rpos = dn_append(result, rpos, sizeof(result), ident,              ident_len);
    rpos = dn_append(result, rpos, sizeof(result), "\",\"intent\":\"",  12);
    rpos = dn_append(result, rpos, sizeof(result), intent,             dn_strlen(intent));
    rpos = dn_append(result, rpos, sizeof(result), "\",\"modality\":\"", 14);
    rpos = dn_append(result, rpos, sizeof(result), modality,           dn_strlen(modality));
    rpos = dn_append(result, rpos, sizeof(result), "\"}",              2);
    dn_emit(0, ident, ident_len, result, rpos, 0);
}

int main(void) {
    if (dn_input_count(0) == 0) return 0;
    IoBuffer *in = dn_get_input(0, 0);
    if (!in || in->data_len == 0) return 0;

    if (dn_input_count(1) == 0) {
        /* Phase A: parse original JSON request */
        const char *json = (const char *)in->data;
        size_t      jlen = in->data_len;

        char id[64];        size_t id_len  = 0;
        char modality[16];  size_t mod_len = 0;
        char payload[4096]; size_t pay_len = 0;

        if (dn_json_get(json, jlen, "id",       id,       sizeof(id),       &id_len)  != 0) return 0;
        if (dn_json_get(json, jlen, "modality", modality, sizeof(modality), &mod_len) != 0) return 0;
        if (dn_json_get(json, jlen, "input",    payload,  sizeof(payload),  &pay_len) != 0) return 0;

        if (dn_memcmp(modality, "text", 4) == 0 && mod_len == 4) {
            /* Inline text path */
            char norm[4096]; size_t nlen = 0;
            normalize(payload, pay_len, norm, &nlen, sizeof(norm));
            emit_result(id, id_len, classify(norm, nlen), "text");
            return 0;
        }

        /* audio or image: emit HTTP request (key encodes modality for Phase B) */
        const char *url;
        size_t      ulen;
        size_t      key;
        if (dn_memcmp(modality, "audio", 5) == 0 && mod_len == 5) {
            url = ASR_URL; ulen = ASR_ULEN; key = 1;
        } else {
            url = OCR_URL; ulen = OCR_ULEN; key = 2;
        }

        size_t req_len = ulen + 1 + pay_len;
        char *data = (char *)dn_add_output(0, id, id_len, req_len + 1, key);
        if (!data) return 0;
        dn_memcpy(data, url, ulen);
        data[ulen] = '\n';
        dn_memcpy(data + ulen + 1, payload, pay_len);
        data[req_len] = '\0';
        return 0;
    }

    /* Phase B: HTTP response in set 1, Phase A output in set 0 */
    IoBuffer *resp = dn_get_input(1, 0);
    if (!resp || resp->data_len == 0) return 0;

    /* Recover request_id and modality from Phase A output (in->ident and in->key) */
    const char *ident     = in->ident;
    size_t      ident_len = in->ident_len;
    size_t      orig_key  = in->key; /* 1=audio, 2=image */

    const char *resp_json = (const char *)resp->data;
    size_t      resp_jlen = resp->data_len;

    char raw_text[4096]; size_t rtext_len = 0;
    if (orig_key == 1) {
        dn_json_get(resp_json, resp_jlen, "transcript", raw_text, sizeof(raw_text), &rtext_len);
    } else {
        dn_json_get(resp_json, resp_jlen, "text",       raw_text, sizeof(raw_text), &rtext_len);
    }

    char norm[4096]; size_t nlen = 0;
    normalize(raw_text, rtext_len, norm, &nlen, sizeof(norm));

    const char *mod_str = (orig_key == 1) ? "audio" : "image";
    emit_result(ident, ident_len, classify(norm, nlen), mod_str);
    return 0;
}
DANDELION_ENTRY(main)
