/*
 * classify — keyword intent classifier.
 *
 * Input  set 0, buf 0: normalized text (key=0)
 * Output set 0, buf 0: {"id":"...","intent":"...","modality":"text"}
 */

#include "common.h"

static const char *classify(const char *text, size_t tlen) {
    if (dn_contains(text, tlen, "cancel") || dn_contains(text, tlen, "annul") ||
        dn_contains(text, tlen, "abort"))  return "cancel_reservation";
    if (dn_contains(text, tlen, "change") || dn_contains(text, tlen, "modif") ||
        dn_contains(text, tlen, "update") || dn_contains(text, tlen, "reschedul") ||
        dn_contains(text, tlen, "move")   || dn_contains(text, tlen, "switch"))
        return "modify_reservation";
    if (dn_contains(text, tlen, "book") || dn_contains(text, tlen, "reserv") ||
        dn_contains(text, tlen, "table") || dn_contains(text, tlen, "seat") ||
        dn_contains(text, tlen, "spot")  || dn_contains(text, tlen, "place"))
        return "book_reservation";
    if (dn_contains(text, tlen, "menu") || dn_contains(text, tlen, "food") ||
        dn_contains(text, tlen, "dish")  || dn_contains(text, tlen, "eat") ||
        dn_contains(text, tlen, "meal")  || dn_contains(text, tlen, "vegetarian") ||
        dn_contains(text, tlen, "vegan") || dn_contains(text, tlen, "allergen") ||
        dn_contains(text, tlen, "option")) return "ask_about_menu";
    return "unknown";
}

int main(void) {
    if (dn_input_count(0) == 0) return 0;
    IoBuffer *in = dn_get_input(0, 0);
    if (!in || in->data_len == 0) return 0;

    const char *intent = classify((const char *)in->data, in->data_len);

    char result[256]; size_t rpos = 0;
    rpos = dn_append(result, rpos, sizeof(result), "{\"id\":\"",        7);
    rpos = dn_append(result, rpos, sizeof(result), in->ident,           in->ident_len);
    rpos = dn_append(result, rpos, sizeof(result), "\",\"intent\":\"",   12);
    rpos = dn_append(result, rpos, sizeof(result), intent,              dn_strlen(intent));
    rpos = dn_append(result, rpos, sizeof(result), "\",\"modality\":\"text\"}", 20);

    dn_emit(0, in->ident, in->ident_len, result, rpos, 0);
    return 0;
}
DANDELION_ENTRY(main)
