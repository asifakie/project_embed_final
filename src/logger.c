// logger.c

#include "app_state.h"
#include <stdio.h>
#include <time.h>
#include <errno.h>

#define NSEC_PER_SEC 1000000000L

static void ts_add_seconds(struct timespec *ts, long seconds)
{
    ts->tv_sec += seconds;
}

void *logger_thread_main(void *arg)
{
    app_state_t *app = (app_state_t *)arg;

    FILE *log = fopen("metrics_log.txt", "a");
    if (!log) {
        perror("logger: fopen metrics_log.txt");
        return NULL;
    }
    setvbuf(log, NULL, _IOLBF, 0); /* line-buffered: ανθεκτικό σε κρας */

    cpu_stat_t cpu;
    cpu_stat_init(&cpu);
    /* Πρώτη ανάγνωση για baseline delta (δεν καταγράφεται ως γραμμή) */
    cpu_stat_get_usage_pct(&cpu);

    struct timespec next;
    clock_gettime(CLOCK_MONOTONIC, &next);
    ts_add_seconds(&next, 1);

    while (app->running) {
        int rc;
        do {
            rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        } while (rc == EINTR); /* αγνόησε signals, ξαναπερίμενε μέχρι το target */

        if (!app->running) break;

        /* --- Ατομική λήψη + μηδενισμός μετρητών του τελευταίου δευτερολέπτου --- */
        unsigned long counts[KIND_COUNT];
        metrics_snapshot_and_reset(&app->metrics, counts);

        double buf_pct  = circbuf_occupancy_pct(&app->queue);
        double cpu_pct  = cpu_stat_get_usage_pct(&cpu);
        double temp_c   = cpu_stat_get_temp_c(); /* -1.0 αν μη διαθέσιμο */

        struct timespec wall;
        clock_gettime(CLOCK_REALTIME, &wall);

        fprintf(log, "%ld,%ld,%lu,%lu,%lu,%lu,%.4f,%.4f,%.3f\n",
                (long)wall.tv_sec, wall.tv_nsec,
                counts[KIND_COMMIT], counts[KIND_IDENTITY],
                counts[KIND_ACCOUNT], counts[KIND_INFO],
                buf_pct, cpu_pct, temp_c);

        /* Επόμενη απόλυτη προθεσμία: next += 1s (όχι "now + 1s") ώστε να
         * μη μεταφέρεται καθυστέρηση/jitter από τον τρέχοντα κύκλο στον
         * επόμενο -> μηδενικό συσσωρευτικό drift στις 24 ώρες. */
        ts_add_seconds(&next, 1);
    }

    fclose(log);
    return NULL;
}
