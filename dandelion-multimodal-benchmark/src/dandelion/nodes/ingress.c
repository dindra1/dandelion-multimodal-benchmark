/*
 * ingress — parses the JSON request and routes to the correct modality output set.
 *
 * Input  set 0, buf 0: raw JSON  {"id":"t001","modality":"text","input":"..."}
 * Output set 0: text  payload  (key=0)
 * Output set 1: audio payload  (key=1)
 * Output set 2: image payload  (key=2)
 */

#include "common.h"

int main(void) {
    if (dn_input_count(0) == 0) return 0;
    IoBuffer *in = dn_get_input(0, 0);
    if (!in || in->data_len == 0) return 0;

    const char *json = (const char *)in->data;
    size_t      jlen = in->data_len;

    char id[64];        size_t id_len  = 0;
    char modality[16];  size_t mod_len = 0;
    char payload[4096]; size_t pay_len = 0;

    if (dn_json_get(json, jlen, "id",       id,       sizeof(id),       &id_len)  != 0) return 0;
    if (dn_json_get(json, jlen, "modality", modality, sizeof(modality), &mod_len) != 0) return 0;
    if (dn_json_get(json, jlen, "input",    payload,  sizeof(payload),  &pay_len) != 0) return 0;

    size_t out_set;
    size_t key;
    if (dn_memcmp(modality, "text",  4) == 0 && mod_len == 4) {
        out_set = 0; key = 0;
    } else if (dn_memcmp(modality, "audio", 5) == 0 && mod_len == 5) {
        out_set = 1; key = 1;
    } else if (dn_memcmp(modality, "image", 5) == 0 && mod_len == 5) {
        out_set = 2; key = 2;
    } else {
        return 0; /* unknown modality — drop */
    }

    dn_emit(out_set, id, id_len, payload, pay_len, key);
    return 0;
}
DANDELION_ENTRY(main)
