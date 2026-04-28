#include <stdlib.h>
#include "ready_queue.h"

void rq_init(ReadyQueue *rq) { rq->head = NULL; rq->size = 0; }

int  rq_empty(const ReadyQueue *rq) { return rq->head == NULL; }
int  rq_size (const ReadyQueue *rq) { return rq->size; }

Job* rq_peek (const ReadyQueue *rq) { return rq->head; }

Job* rq_peek_second(const ReadyQueue *rq) {
    return rq->head ? rq->head->next : NULL;
}

/* Compare priority: a has higher priority than b at time t iff
 * laxity(a,t) < laxity(b,t), with ties broken by lower task_id then
 * lower job_id (deterministic). */
static int higher_prio(const Job *a, const Job *b, int t) {
    int la = job_laxity(a, t);
    int lb = job_laxity(b, t);
    if (la != lb) return la < lb;
    if (a->task_id != b->task_id) return a->task_id < b->task_id;
    return a->job_id < b->job_id;
}

void rq_insert(ReadyQueue *rq, Job *job, int t) {
    job->next = NULL;

    if (!rq->head || higher_prio(job, rq->head, t)) {
        job->next = rq->head;
        rq->head = job;
        rq->size++;
        return;
    }

    Job *cur = rq->head;
    while (cur->next && higher_prio(cur->next, job, t))
        cur = cur->next;

    job->next = cur->next;
    cur->next = job;
    rq->size++;
}

Job* rq_pop(ReadyQueue *rq) {
    if (!rq->head) return NULL;
    Job *j = rq->head;
    rq->head = j->next;
    j->next  = NULL;
    rq->size--;
    return j;
}

int rq_remove(ReadyQueue *rq, Job *job) {
    if (!rq->head) return 0;
    if (rq->head == job) {
        rq->head = job->next;
        job->next = NULL;
        rq->size--;
        return 1;
    }
    Job *cur = rq->head;
    while (cur->next && cur->next != job) cur = cur->next;
    if (!cur->next) return 0;
    cur->next = job->next;
    job->next = NULL;
    rq->size--;
    return 1;
}

void rq_destroy(ReadyQueue *rq) {
    Job *cur = rq->head;
    while (cur) {
        Job *nx = cur->next;
        job_free(cur);
        cur = nx;
    }
    rq->head = NULL;
    rq->size = 0;
}
