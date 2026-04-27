/*
 * format_output — passthrough: copies the classified result to the final output set.
 *
 * Input  set 0, buf 0: {"id":"...","intent":"...","modality":"..."}
 * Output set 0, buf 0: same content
 */

#include "common.h"

int main(void) {
    if (dn_input_count(0) == 0) return 0;
    IoBuffer *in = dn_get_input(0, 0);
    if (!in || in->data_len == 0) return 0;

    dn_emit(0, in->ident, in->ident_len, in->data, in->data_len, in->key);
    return 0;
}
DANDELION_ENTRY(main)
