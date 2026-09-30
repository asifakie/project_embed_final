//producer.c                      

#include "app_state.h"
#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define BASE_DELAY_MS      500
#define MAX_DELAY_MS       30000
#define FAILOVER_THRESHOLD 4
#define RECV_BUF_CHUNK     65536

typedef struct {
    app_state_t *app;
    struct lws  *wsi;
    char        *rx_accum;      /* συσσώρευση fragmented frames */
    size_t       rx_len;
    size_t       rx_cap;
    int          host_idx;
} producer_ctx_t;

static void log_reconnect_event(const char *fmt, ...)
{
    //Ξεχωριστό αρχείο από το metrics_log.txt 
    FILE *f = fopen("reconnect_log.txt", "a");
    if (!f) return;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    fprintf(f, "[%ld.%09ld] ", (long)ts.tv_sec, ts.tv_nsec);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}


static int64_t extract_time_us(const char *json, size_t len)
{
    const char *key = "\"time_us\"";
    const char *p = memmem(json, len, key, strlen(key));
    if (!p) return -1;
    p += strlen(key);
    while (*p == ' ' || *p == ':') p++;
    return strtoll(p, NULL, 10);
}

static uint64_t backoff_delay_ms(unsigned long attempts)
{
    uint64_t delay = (uint64_t)BASE_DELAY_MS << (attempts > 10 ? 10 : attempts);
    if (delay > MAX_DELAY_MS) delay = MAX_DELAY_MS;
    
    long jitter = (rand() % (int)(delay / 5 + 1)) - (long)(delay / 10);
    long result = (long)delay + jitter;
    return result > 0 ? (uint64_t)result : BASE_DELAY_MS;
}

static void build_ws_path(char *out, size_t out_sz, int64_t cursor_us)
{
    if (cursor_us >= 0)
        snprintf(out, out_sz,
                 "/subscribe?wantedCollections=app.bsky.feed.post&cursor=%lld",
                 (long long)cursor_us);
    else
        snprintf(out, out_sz, "%s", JETSTREAM_PATH);
}

static int callback_jetstream(struct lws *wsi, enum lws_callback_reasons reason,
                               void *user, void *in, size_t len)
{
    producer_ctx_t *ctx = (producer_ctx_t *)lws_context_user(lws_get_context(wsi));
    app_state_t *app = ctx->app;

    switch (reason) {

    case LWS_CALLBACK_CLIENT_ESTABLISHED: {
        pthread_mutex_lock(&app->conn.mutex);
        unsigned long prev_attempts = app->conn.reconnect_attempts;
        app->conn.state = CONN_CONNECTED;
        app->conn.reconnect_attempts = 0;
        if (prev_attempts > 0) app->conn.total_reconnects++;
        pthread_mutex_unlock(&app->conn.mutex);
        log_reconnect_event("CONNECTED (host=%s, after %lu attempts)",
                             JETSTREAM_HOSTS[ctx->host_idx], prev_attempts);
        break;
    }

    case LWS_CALLBACK_CLIENT_RECEIVE: {
        /* Συσσώρευση σε περίπτωση fragmented message (πολλαπλά frames) */
        size_t remaining = lws_remaining_packet_payload(wsi);
        int is_final = lws_is_final_fragment(wsi);

        if (ctx->rx_len + len + 1 > ctx->rx_cap) {
            size_t newcap = ctx->rx_cap == 0 ? RECV_BUF_CHUNK : ctx->rx_cap * 2;
            while (newcap < ctx->rx_len + len + 1) newcap *= 2;
            char *nb = realloc(ctx->rx_accum, newcap);
            if (!nb) { ctx->rx_len = 0; break; } 
            ctx->rx_accum = nb;
            ctx->rx_cap = newcap;
        }
        memcpy(ctx->rx_accum + ctx->rx_len, in, len);
        ctx->rx_len += len;

        if (remaining == 0 && is_final) {
            ctx->rx_accum[ctx->rx_len] = '\0';

            
            int64_t t_us = extract_time_us(ctx->rx_accum, ctx->rx_len);
            if (t_us > 0 && pthread_mutex_trylock(&app->conn.mutex) == 0) {
                app->conn.last_cursor_us = t_us;
                pthread_mutex_unlock(&app->conn.mutex);
            }

            char *msg_copy = malloc(ctx->rx_len + 1);
            if (msg_copy) {
                memcpy(msg_copy, ctx->rx_accum, ctx->rx_len + 1);
                // Bounded queue
                circbuf_push(&app->queue, msg_copy);
            }
            ctx->rx_len = 0;
        }
        break;
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    case LWS_CALLBACK_CLIENT_CLOSED: {
        pthread_mutex_lock(&app->conn.mutex);
        app->conn.state = CONN_RECONNECTING;
        app->conn.reconnect_attempts++;
        clock_gettime(CLOCK_REALTIME, &app->conn.last_disconnect_ts);
        unsigned long attempts = app->conn.reconnect_attempts;
        pthread_mutex_unlock(&app->conn.mutex);

        log_reconnect_event("DISCONNECTED (%s), attempt #%lu",
                             reason == LWS_CALLBACK_CLIENT_CONNECTION_ERROR
                                 ? "connect error" : "closed",
                             attempts);

        // Fail-over σε επόμενο host μετά από επαναλαμβανόμενες αποτυχίες 
        if (attempts % FAILOVER_THRESHOLD == 0) {
            ctx->host_idx = (ctx->host_idx + 1) % JETSTREAM_HOSTS_COUNT;
            log_reconnect_event("Failover -> host=%s",
                                 JETSTREAM_HOSTS[ctx->host_idx]);
        }
        ctx->wsi = NULL; 
        break;
    }

    default:
        break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    { "jetstream-protocol", callback_jetstream, 0, RECV_BUF_CHUNK, 0, NULL, 0 },
    LWS_PROTOCOL_LIST_TERM
};

static struct lws *do_connect(struct lws_context *context, producer_ctx_t *ctx,
                               int64_t cursor_us)
{
    char path[256];
    build_ws_path(path, sizeof(path), cursor_us);

    struct lws_client_connect_info ccinfo;
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = JETSTREAM_HOSTS[ctx->host_idx];
    ccinfo.port    = JETSTREAM_PORT;
    ccinfo.path    = path;
    ccinfo.host    = ccinfo.address;
    ccinfo.origin  = ccinfo.address;
    ccinfo.protocol = protocols[0].name;
    ccinfo.ssl_connection = LCCSCF_USE_SSL;
    ccinfo.pwsi = &ctx->wsi;

    pthread_mutex_lock(&ctx->app->conn.mutex);
    ctx->app->conn.state = CONN_CONNECTING;
    pthread_mutex_unlock(&ctx->app->conn.mutex);

    return lws_client_connect_via_info(&ccinfo);
}

void *producer_thread_main(void *arg)
{
    app_state_t *app = (app_state_t *)arg;

    producer_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.app = app;

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.user = &ctx;

    struct lws_context *context = lws_create_context(&info);
    if (!context) {
        fprintf(stderr, "producer: lws_create_context failed\n");
        return NULL;
    }

    do_connect(context, &ctx, app->conn.last_cursor_us);

    while (app->running) {
        lws_service(context, 100 /* ms poll timeout */);

        if (ctx.wsi == NULL) {
            /* Είμαστε σε RECONNECTING (ή αρχική αποτυχία). Εφαρμόζουμε
             * exponential backoff πριν την επόμενη προσπάθεια. */
            pthread_mutex_lock(&app->conn.mutex);
            conn_state_t st = app->conn.state;
            unsigned long attempts = app->conn.reconnect_attempts;
            int64_t cursor = app->conn.last_cursor_us;
            pthread_mutex_unlock(&app->conn.mutex);

            if (st == CONN_RECONNECTING || st == CONN_DISCONNECTED) {
                uint64_t delay_ms = backoff_delay_ms(attempts);
                struct timespec req = {
                    .tv_sec  = delay_ms / 1000,
                    .tv_nsec = (long)(delay_ms % 1000) * 1000000L
                };
                nanosleep(&req, NULL);
                if (!app->running) break;
                do_connect(context, &ctx, cursor);
            }
        }
    }

    lws_context_destroy(context);
    free(ctx.rx_accum);
    return NULL;
}
