#ifndef APP_STATE_H
#define APP_STATE_H

#include "circbuf.h"
#include "metrics.h"
#include <pthread.h>
#include <stdint.h>

#define JETSTREAM_HOST "jetstream1.us-east.bsky.network"
#define JETSTREAM_PATH "/subscribe?wantedCollections=app.bsky.feed.post"
#define JETSTREAM_PORT 443

// κύριο host δεν απαντά μετά από αρκετές αποτυχημένες προσπάθειες. 
extern const char *JETSTREAM_HOSTS[];
#define JETSTREAM_HOSTS_COUNT 4

typedef enum {
    CONN_DISCONNECTED = 0,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_RECONNECTING
} conn_state_t;

typedef struct {
    pthread_mutex_t mutex;
    conn_state_t    state;
    unsigned long   reconnect_attempts;   /* consecutive failed attempts */
    unsigned long   total_reconnects;     /* lifetime successful reconnects */
    int64_t         last_cursor_us;       /* time_us του τελευταίου μηνύματος (Jetstream cursor) */
    struct timespec last_disconnect_ts;
} conn_status_t;

typedef struct {
    circbuf_t      queue;
    metrics_t      metrics;
    conn_status_t  conn;
    volatile int   running; // flag
} app_state_t;

void app_state_init(app_state_t *app);
void app_state_destroy(app_state_t *app);

#endif 
