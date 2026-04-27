/*
 * normalize — lowercase + strip punctuation from text.
 *
 * Input  set 0, buf 0: raw text (key=0, ident=request_id)
 * Output set 0, buf 0: normalized text
 */

#include "common.h"

int main(void) {
    if (dn_input_count(0) == 0) return 0;
    IoBuffer *in = dn_get_input(0, 0);
    if (!in || in->data_len == 0) return 0;

    const char *src  = (const char *)in->data;
    size_t      slen = in->data_len;

    char norm[4096]; size_t npos = 0;
    for (size_t i = 0; i < slen && npos < sizeof(norm) - 1; i++) {
        char c = src[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ') {
            norm[npos++] = c;
        } else if (c >= 'A' && c <= 'Z') {
            norm[npos++] = c + 32;
        } else {
            if (npos > 0 && norm[npos - 1] != ' ') norm[npos++] = ' ';
        }
    }
    while (npos > 0 && norm[npos - 1] == ' ') npos--;
    norm[npos] = '\0';

    dn_emit(0, in->ident, in->ident_len, norm, npos, 0);
    return 0;
}
DANDELION_ENTRY(main)
