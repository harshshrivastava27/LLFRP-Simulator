#include <stdlib.h>
#include "deadline_queue.h"

void dq_init(DeadlineQueue *dq) {
    dq->items = NULL;
    dq->count = 0;
    dq->cap   = 0;
}

void dq_clear(DeadlineQueue *dq) { dq->count = 0; }

void dq_destroy(DeadlineQueue *dq) {
    free(dq->items);
    dq->items = NULL;
    dq->count = dq->cap = 0;
}

static void grow(DeadlineQueue *dq) {
    int nc = dq->cap == 0 ? 8 : dq->cap * 2;
    Job **n = realloc(dq->items, sizeof(Job*) * nc);
    if (!n) return;          /* on OOM we simply stop growing */
    dq->items = n;
    dq->cap   = nc;
}

void dq_insert(DeadlineQueue *dq, Job *j) {
    if (dq->count >= dq->cap) grow(dq);
    if (dq->count >= dq->cap) return;

    /* find first index with deadline > j->deadline (or task_id tie) */
    int pos = dq->count;
    for (int i = 0; i < dq->count; i++) {
        Job *x = dq->items[i];
        if (x->abs_deadline > j->abs_deadline ||
            (x->abs_deadline == j->abs_deadline && x->task_id > j->task_id)) {
            pos = i;
            break;
        }
    }
    /* shift right */
    for (int i = dq->count; i > pos; i--)
        dq->items[i] = dq->items[i-1];
    dq->items[pos] = j;
    dq->count++;
}
