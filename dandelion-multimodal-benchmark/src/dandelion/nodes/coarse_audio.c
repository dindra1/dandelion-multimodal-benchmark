/*
 * coarse_audio — ASR + normalize + classify in one node (coarse-grained DAG).
 *
 * Phase A: Input set 0 = wav path → Output set 0 = HTTP request to /asr
 * Phase B: Input set 1 = {"transcript":"..."} → Output set 0 = result JSON
 */

#include "common.h"

#define ASR_URL  "http://127.0.0.1:8765/asr"
#define ASR_ULEN 26

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

int main(void) {
    if (dn_input_count(1) == 0) {
        /* Phase A */
        if (dn_input_count(0) == 0) return 0;
        IoBuffer *in = dn_get_input(0, 0);
        if (!in || in->data_len == 0) return 0;
        size_t req_len = ASR_ULEN + 1 + in->data_len;
        char *data = (char *)dn_add_output(0, in->ident, in->ident_len, req_len+1, 1);
        if (!data) return 0;
        dn_memcpy(data, ASR_URL, ASR_ULEN);
        data[ASR_ULEN] = '\n';
        dn_memcpy(data + ASR_ULEN + 1, in->data, in->data_len);
        data[req_len] = '\0';
        return 0;
    }

    /* Phase B */
    IoBuffer *resp = dn_get_input(1, 0);
    IoBuffer *orig = dn_get_input(0, 0);
    if (!resp || resp->data_len == 0) return 0;

    char transcript[2048]; size_t tlen = 0;
    dn_json_get((const char *)resp->data, resp->data_len,
                "transcript", transcript, sizeof(transcript), &tlen);

    char norm[2048]; size_t npos = 0;
    for (size_t i = 0; i < tlen && npos < sizeof(norm)-1; i++) {
        char c = transcript[i];
        if ((c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' ') { norm[npos++]=c; }
        else if (c>='A'&&c<='Z') { norm[npos++]=c+32; }
        else { if (npos>0&&norm[npos-1]!=' ') norm[npos++]=' '; }
    }
    while (npos>0&&norm[npos-1]==' ') npos--;
    norm[npos]='\0';

    const char *intent    = classify(norm, npos);
    const char *ident     = orig ? orig->ident     : resp->ident;
    size_t      ident_len = orig ? orig->ident_len : resp->ident_len;

    char result[256]; size_t rpos = 0;
    rpos = dn_append(result, rpos, sizeof(result), "{\"id\":\"",        7);
    rpos = dn_append(result, rpos, sizeof(result), ident,              ident_len);
    rpos = dn_append(result, rpos, sizeof(result), "\",\"intent\":\"",  12);
    rpos = dn_append(result, rpos, sizeof(result), intent,             dn_strlen(intent));
    rpos = dn_append(result, rpos, sizeof(result), "\",\"modality\":\"audio\"}", 21);

    dn_emit(0, ident, ident_len, result, rpos, 0);
    return 0;
}
DANDELION_ENTRY(main)
