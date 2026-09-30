//main.c
#include "app_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>

extern void *producer_thread_main(void *arg);
extern void *consumer_thread_main(void *arg);
extern void *logger_thread_main(void *arg);

static app_state_t g_app;

static void request_shutdown(void)
{
    g_app.running = 0;
    circbuf_shutdown(&g_app.queue); /* ξυπνά τον Consumer αν κοιμάται */
}

static void handle_signal(int sig)
{
    (void)sig;
    request_shutdown();
}

int main(int argc, char *argv[])
{
   // timer για την εκτέλεση,αυτοματοποιήση της διαδικασίας και δοκιμαστική λειοτυργία
    long duration_sec = 86400;
    if (argc > 1) {
        long v = atol(argv[1]);
        if (v > 0) duration_sec = v;
    }

    app_state_init(&g_app);

    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    pthread_t producer_tid, consumer_tid, logger_tid;

    if (pthread_create(&producer_tid, NULL, producer_thread_main, &g_app) != 0) {
        fprintf(stderr, "Failed to create producer thread\n");
        return 1;
    }
    if (pthread_create(&consumer_tid, NULL, consumer_thread_main, &g_app) != 0) {
        fprintf(stderr, "Failed to create consumer thread\n");
        return 1;
    }
    if (pthread_create(&logger_tid, NULL, logger_thread_main, &g_app) != 0) {
        fprintf(stderr, "Failed to create logger thread\n");
        return 1;
    }

    struct timespec wall_now;
    clock_gettime(CLOCK_REALTIME, &wall_now);
    printf("Firehose real-time monitor running. Duration: %ld s (~%.2f h).\n",
           duration_sec, duration_sec / 3600.0);
    printf("Start (epoch): %ld | Planned stop (epoch): %ld\n",
           (long)wall_now.tv_sec, (long)wall_now.tv_sec + duration_sec);
    printf("Metrics -> metrics_log.txt | Reconnect events -> reconnect_log.txt\n");
    printf("Ctrl+C τερματίζει νωρίτερα αν χρειαστεί.\n");

    
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec += duration_sec;

    int rc;
    do {
        rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
    } while (rc == EINTR && g_app.running);

    if (g_app.running) {
        printf("Ολοκληρώθηκε η προγραμματισμένη διάρκεια (%ld s) -- τερματισμός.\n",
               duration_sec);
        request_shutdown();
    }

    pthread_join(producer_tid, NULL);
    pthread_join(consumer_tid, NULL);
    pthread_join(logger_tid, NULL);

    app_state_destroy(&g_app);
    return 0;
}
