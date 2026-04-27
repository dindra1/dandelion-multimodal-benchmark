/*
 * audio_asr — two-phase: send HTTP request to /asr, then classify transcript.
 *
 * Phase A (set 1 empty):
 *   Input  set 0: wav file path (key=1)
 *   Output set 0: HTTP request descriptor "http://127.0.0.1:8765/asr\n<path>"
 *
 * Phase B (set 1 populated with HTTP response):
 *   Input  set 0: original HTTP request (key=1, ident=request_id)
 *   Input  set 1: HTTP response {"transcript":"..."}
 *   Output set 0: normalized transcript text (key=0)
 */

#include "common.h"

#define ASR_URL  "http://127.0.0.1:8765/asr"
#define ASR_ULEN 26

int main(void) {
    if (dn_input_count(1) == 0) {
        /* Phase A: build HTTP request */
        if (dn_input_count(0) == 0) return 0;
        IoBuffer *in = dn_get_input(0, 0);
        if (!in || in->data_len == 0) return 0;

        size_t body_len = in->data_len;
        size_t req_len  = ASR_ULEN + 1 + body_len;

        char *data = (char *)dn_add_output(0, in->ident, in->ident_len,
                                            req_len + 1, 1);
        if (!data) return 0;
        dn_memcpy(data, ASR_URL, ASR_ULEN);
        data[ASR_ULEN] = '\n';
        dn_memcpy(data + ASR_ULEN + 1, in->data, body_len);
        data[req_len] = '\0';
        return 0;
    }

    /* Phase B: transcript in set 1 */
    IoBuffer *resp = dn_get_input(1, 0);
    IoBuffer *orig = dn_get_input(0, 0);
    if (!resp || resp->data_len == 0) return 0;

    char transcript[2048]; size_t tlen = 0;
    dn_json_get((const char *)resp->data, resp->data_len,
                "transcript", transcript, sizeof(transcript), &tlen);

    /* normalize inline */
    char norm[2048]; size_t npos = 0;
    for (size_t i = 0; i < tlen && npos < sizeof(norm) - 1; i++) {
        char c = transcript[i];
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
