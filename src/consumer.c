//consumer.c παρόμοιας λογικής με παράδειγα από την ροτεινόμενη βιβλιογραφία

#include "app_state.h"
#include <cjson/cJSON.h>
#include <string.h>
#include <stdlib.h>

static msg_kind_t kind_from_string(const char *s)
{
    if (strcmp(s, "commit")   == 0) return KIND_COMMIT;
    if (strcmp(s, "identity") == 0) return KIND_IDENTITY;
    if (strcmp(s, "account")  == 0) return KIND_ACCOUNT;
    return KIND_INFO;
}

void *consumer_thread_main(void *arg)
{
    app_state_t *app = (app_state_t *)arg;

    while (app->running) {
        char *msg = circbuf_pop(&app->queue);
        if (msg == NULL) {
            
            break;
        }

        cJSON *root = cJSON_Parse(msg);
        if (root) {
            cJSON *kind_item = cJSON_GetObjectItemCaseSensitive(root, "kind");
            if (cJSON_IsString(kind_item) && kind_item->valuestring != NULL) {
                metrics_increment(&app->metrics, kind_from_string(kind_item->valuestring));
            } else {
                metrics_increment(&app->metrics, KIND_INFO);
            }
            cJSON_Delete(root);
        } else {
            // Malformed JSON - καταμετράται ως info/σφάλμα 
            metrics_increment(&app->metrics, KIND_INFO);
        }

        free(msg);
    }
    return NULL;
}
