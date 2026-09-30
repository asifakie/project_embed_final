#include "app_state.h"
#include <string.h>

const char *JETSTREAM_HOSTS[JETSTREAM_HOSTS_COUNT] = {
    "jetstream1.us-east.bsky.network",
    "jetstream2.us-east.bsky.network",
    "jetstream1.us-west.bsky.network",
    "jetstream2.us-west.bsky.network"
};

void app_state_init(app_state_t *app)
{
    circbuf_init(&app->queue);
    metrics_init(&app->metrics);

    memset(&app->conn, 0, sizeof(app->conn));
    pthread_mutex_init(&app->conn.mutex, NULL);
    app->conn.state = CONN_DISCONNECTED;
    app->conn.last_cursor_us = -1; /* -1 => χωρίς cursor (πρώτη σύνδεση, live tail) */

    app->running = 1;
}

void app_state_destroy(app_state_t *app)
{
    circbuf_destroy(&app->queue);
    metrics_destroy(&app->metrics);
    pthread_mutex_destroy(&app->conn.mutex);
}
