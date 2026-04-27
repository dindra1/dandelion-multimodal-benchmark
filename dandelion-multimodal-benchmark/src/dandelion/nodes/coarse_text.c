/*
 * coarse_text — normalize + classify text in one node (coarse-grained DAG).
 *
 * Input  set 0, buf 0: raw text (key=0, ident=request_id)
 * Output set 0, buf 0: {"id":"...","intent":"...","modality":"text"}
 */

#include "common.h"

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
    if (dn_input_count(0) == 0) return 0;
    IoBuffer *in = dn_get_input(0, 0);
    if (!in || in->data_len == 0) return 0;

    /* normalize */
    char norm[4096]; size_t npos = 0;
    const char *src = (const char *)in->data;
    for (size_t i = 0; i < in->data_len && npos < sizeof(norm)-1; i++) {
        char c = src[i];
        if ((c>='a'&&c<='z')||(c>='0'&&c<='9')||c==' ') { norm[npos++]=c; }
        else if (c>='A'&&c<='Z') { norm[npos++]=c+32; }
        else { if (npos>0&&norm[npos-1]!=' ') norm[npos++]=' '; }
    }
    while (npos>0&&norm[npos-1]==' ') npos--;
    norm[npos]='\0';

    const char *intent = classify(norm, npos);

    char result[256]; size_t rpos = 0;
    rpos = dn_append(result, rpos, sizeof(result), "{\"id\":\"",       7);
    rpos = dn_append(result, rpos, sizeof(result), in->ident,          in->ident_len);
    rpos = dn_append(result, rpos, sizeof(result), "\",\"intent\":\"",  12);
    rpos = dn_append(result, rpos, sizeof(result), intent,             dn_strlen(intent));
    rpos = dn_append(result, rpos, sizeof(result), "\",\"modality\":\"text\"}", 20);

    dn_emit(0, in->ident, in->ident_len, result, rpos, 0);
    return 0;
}
DANDELION_ENTRY(main)
