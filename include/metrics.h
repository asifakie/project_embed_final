#ifndef METRICS_H
#define METRICS_H

#include <pthread.h>

typedef enum {
    KIND_COMMIT = 0,
    KIND_IDENTITY,
    KIND_ACCOUNT,
    KIND_INFO,
    KIND_COUNT
} msg_kind_t;

typedef struct {
    unsigned long counts[KIND_COUNT];
    pthread_mutex_t mutex;
} metrics_t;

void metrics_init(metrics_t *m);
void metrics_destroy(metrics_t *m);
void metrics_increment(metrics_t *m, msg_kind_t k);

/* Ατομική (atomic) λήψη-και-μηδενισμός: ο Logger παίρνει τους μετρητές
 * του τελευταίου δευτερολέπτου και τους μηδενίζει μέσα στο ΙΔΙΟ lock,
 * ώστε να μη χαθεί/διπλομετρηθεί κανένα μήνυμα (race condition). */
void metrics_snapshot_and_reset(metrics_t *m, unsigned long out[KIND_COUNT]);

/* --- CPU usage μέσω /proc/stat (jiffies delta) --- */
typedef struct {
    unsigned long long prev_total;
    unsigned long long prev_idle;
    int initialized;
} cpu_stat_t;

void   cpu_stat_init(cpu_stat_t *c);
// Επιστρέφει ποσοστό (0-100) χρήσης CPU από την προηγούμενη κλήση.
 
double cpu_stat_get_usage_pct(cpu_stat_t *c);

// Θερμοκρασία CPU (SoC)
double cpu_stat_get_temp_c(void);

#endif /* METRICS_H */
//Κεφαλίδ για το αρχείο που θα παίνει μετρήσεις
