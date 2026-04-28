#ifndef READY_QUEUE_H
#define READY_QUEUE_H

#include "job.h"

/*
 * Ready Queue (sorted singly-linked list)
 *
 * Ordered by laxity at time of insertion (LLF priority). Tie-breaker:
 * smaller task_id wins, then smaller job_id. The 'next' field of Job
 * is used.
 *
 * Note: while a job sits in the ready queue, real-time t advances but
 * its remaining time does not, so its laxity decreases by 1 per tick.
 * That means relative ordering between two ready jobs is invariant
 * (both decrease at the same rate), so we only need to re-sort when a
 * NEW job is inserted (its absolute laxity at time t may slot in
 * anywhere). We do this with O(N) linear insertion.
 */

typedef struct {
    Job *head;
    int  size;
} ReadyQueue;

void rq_init(ReadyQueue *rq);

/* Insert job using laxity computed at time t. */
void rq_insert(ReadyQueue *rq, Job *job, int t);

/* Pop highest-priority (lowest-laxity) job. Returns NULL if empty. */
Job* rq_pop(ReadyQueue *rq);

/* Peek without removing. */
Job* rq_peek(const ReadyQueue *rq);

/* Peek the second element (used to detect laxity ties). */
Job* rq_peek_second(const ReadyQueue *rq);

int  rq_empty(const ReadyQueue *rq);
int  rq_size(const ReadyQueue *rq);

/* Remove a specific job (by pointer). Returns 1 if removed. */
int  rq_remove(ReadyQueue *rq, Job *job);

/* Free all remaining jobs (used at shutdown). */
void rq_destroy(ReadyQueue *rq);

#endif
