#include "metrics.h"
#include <stdio.h>
#include <string.h>

void metrics_init(metrics_t *m)
{
    memset(m->counts, 0, sizeof(m->counts));
    pthread_mutex_init(&m->mutex, NULL);
}

void metrics_destroy(metrics_t *m)
{
    pthread_mutex_destroy(&m->mutex);
}

void metrics_increment(metrics_t *m, msg_kind_t k)
{
    pthread_mutex_lock(&m->mutex);
    m->counts[k]++;
    pthread_mutex_unlock(&m->mutex);
}

void metrics_snapshot_and_reset(metrics_t *m, unsigned long out[KIND_COUNT])
{
    pthread_mutex_lock(&m->mutex);
    for (int i = 0; i < KIND_COUNT; i++) {
        out[i] = m->counts[i];
        m->counts[i] = 0;
    }
    pthread_mutex_unlock(&m->mutex);
}

void cpu_stat_init(cpu_stat_t *c)
{
    memset(c, 0, sizeof(*c));
}

double cpu_stat_get_usage_pct(cpu_stat_t *c)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return 0.0;

    char label[8];
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    /* Πρώτη γραμμή: "cpu  user nice system idle iowait irq softirq steal ..." */
    int n = fscanf(f, "%7s %llu %llu %llu %llu %llu %llu %llu %llu",
                   label, &user, &nice, &system, &idle,
                   &iowait, &irq, &softirq, &steal);
    fclose(f);
    if (n < 5) return 0.0;

    unsigned long long idle_all  = idle + iowait;
    unsigned long long total     = user + nice + system + idle_all + irq + softirq + steal;

    double usage = 0.0;
    if (c->initialized) {
        unsigned long long dtotal = total - c->prev_total;
        unsigned long long didle  = idle_all - c->prev_idle;
        if (dtotal > 0)
            usage = 100.0 * (double)(dtotal - didle) / (double)dtotal;
    }

    c->prev_total   = total;
    c->prev_idle    = idle_all;
    c->initialized  = 1;
    return usage;
}

double cpu_stat_get_temp_c(void)
{
    // Στο Raspberry Pi (και στα περισσότερα Linux ARM SBCs), το SoC  σε mCelsius
     
    FILE *f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!f) return -1.0;

    long millideg = 0;
    int n = fscanf(f, "%ld", &millideg);
    fclose(f);
    if (n != 1) return -1.0;

    return (double)millideg / 1000.0;
}
