#ifndef DEADLINE_QUEUE_H
#define DEADLINE_QUEUE_H

#include "job.h"

/*
 * Deadline-ordered queue
 *
 * Used inside extension_time() of LLFRP. Holds the same set of jobs
 * as the ready queue but ordered by absolute deadline (ascending,
 * earliest deadline first).
 *
 * IMPORTANT: a Job has only one 'next' pointer, which is used by the
 * ReadyQueue while it is ready. To avoid two queues fighting over
 * the same pointer, the deadline queue is built ON DEMAND by the
 * scheduler each time extension_time() is called - it walks the
 * ready queue and produces a *separate* sorted array. This module
 * therefore exposes only a tiny array-based snapshot type.
 *
 * That keeps the data structures simple and matches how the paper
 * describes deadlineQ as a logical view of the ready set.
 */

typedef struct {
    Job **items;   /* dynamically allocated; ascending by deadline    */
    int   count;
    int   cap;
} DeadlineQueue;

void dq_init(DeadlineQueue *dq);
void dq_clear(DeadlineQueue *dq);
void dq_destroy(DeadlineQueue *dq);

/* Insert maintaining ascending-deadline order (tie-break: task_id). */
void dq_insert(DeadlineQueue *dq, Job *j);

#endif
