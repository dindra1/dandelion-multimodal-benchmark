/*
 * image_ocr — two-phase: send HTTP request to /ocr, then normalize result.
 *
 * Phase A: Input set 0 = image file path → Output set 0 = HTTP descriptor
 * Phase B: Input set 1 = HTTP response {"text":"..."} → Output set 0 = normalized text
 */

#include "common.h"

#define OCR_URL  "http://127.0.0.1:8765/ocr"
#define OCR_ULEN 26

int main(void) {
    if (dn_input_count(1) == 0) {
        /* Phase A */
        if (dn_input_count(0) == 0) return 0;
        IoBuffer *in = dn_get_input(0, 0);
        if (!in || in->data_len == 0) return 0;

        size_t body_len = in->data_len;
        size_t req_len  = OCR_ULEN + 1 + body_len;

        char *data = (char *)dn_add_output(0, in->ident, in->ident_len,
                                            req_len + 1, 2);
        if (!data) return 0;
        dn_memcpy(data, OCR_URL, OCR_ULEN);
        data[OCR_ULEN] = '\n';
        dn_memcpy(data + OCR_ULEN + 1, in->data, body_len);
        data[req_len] = '\0';
        return 0;
    }

    /* Phase B */
    IoBuffer *resp = dn_get_input(1, 0);
    IoBuffer *orig = dn_get_input(0, 0);
    if (!resp || resp->data_len == 0) return 0;

    char ocr_text[4096]; size_t olen = 0;
    dn_json_get((const char *)resp->data, resp->data_len,
                "text", ocr_text, sizeof(ocr_text), &olen);

    char norm[4096]; size_t npos = 0;
    for (size_t i = 0; i < olen && npos < sizeof(norm) - 1; i++) {
        char c = ocr_text[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ') {
            norm[npos++] = c;
        } else if (c >= 'A' && c <= 'Z') {
            norm[npos++] = c + 32;
        } else {
            if (npos > 0 && norm[npos-1] != ' ') norm[npos++] = ' ';
        }
    }
    while (npos > 0 && norm[npos-1] == ' ') npos--;
    norm[npos] = '\0';

    const char *ident     = orig ? orig->ident     : resp->ident;
    size_t      ident_len = orig ? orig->ident_len : resp->ident_len;
    dn_emit(0, ident, ident_len, norm, npos, 0);
    return 0;
}
DANDELION_ENTRY(main)
