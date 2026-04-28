#include <stdlib.h>
#include "job.h"

Job* job_create(int job_id, int task_id, int instance,
                int release, int abs_deadline,
                int period, int wcet)
{
    Job *j = malloc(sizeof(Job));
    if (!j) return NULL;

    j->job_id       = job_id;
    j->task_id      = task_id;
    j->instance     = instance;
    j->release      = release;
    j->abs_deadline = abs_deadline;
    j->period       = period;
    j->wcet         = wcet;
    j->remaining    = wcet;

    j->first_start   = -1;
    j->completion    = -1;
    j->preempt_count = 0;
    j->completed     = 0;
    j->next          = NULL;
    return j;
}

void job_free(Job *j) { free(j); }

int job_laxity(const Job *j, int t) {
    return j->abs_deadline - t - j->remaining;
}
