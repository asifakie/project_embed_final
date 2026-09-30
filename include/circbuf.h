#ifndef CIRCBUF_H
#define CIRCBUF_H

#include <pthread.h>
#include <stddef.h>

// Χωρητικότητα (πλήθος JSON μηνυμάτων) του κυκλικού buffer Bounded queue
#define CIRCBUF_CAPACITY 4096

typedef struct {
    char   *slots[CIRCBUF_CAPACITY]; /* malloc'd NUL-terminated strings */
    size_t  head;                    /*  index pop */
    size_t  tail;                    /* index  push */
    size_t  count;                   /* μετρητης για το στοιχειο που τρεχει εκινη την στιγμη */

    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
    int             shutting_down;
} circbuf_t;

void   circbuf_init(circbuf_t *cb);
void   circbuf_destroy(circbuf_t *cb);

/* Παίρνει ιδιοκτησία του msg . Μπλοκάρει αν είναι
 * γεμάτο */
int    circbuf_push(circbuf_t *cb, char *msg);

/*Κάνει free() το επιστρεφόμενο string.
 * Μπλοκάρει αν είναι άδειο. Επιστρέφει NULL αν έγινε shutdown και είναι άδειο. */
char  *circbuf_pop(circbuf_t *cb);

// Ποσοσσγτό κατάληψης
double circbuf_occupancy_pct(circbuf_t *cb);

// Ξυπνά όλα τα νήματα που περιμένουν
void   circbuf_shutdown(circbuf_t *cb);

#endif 
