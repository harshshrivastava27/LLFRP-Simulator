#ifndef JOB_H
#define JOB_H

/*
 * Job ADT
 *
 * A Job is a single instance (release) of a Task. It carries timing
 * fields used by the scheduler and bookkeeping fields used by the
 * metrics module.
 *
 * The 'next' pointer is generic and may be re-used by either of the
 * priority queues (ready_queue, deadline_queue), but never by both at
 * the same time. A Job is in at most one queue at any moment.
 */

typedef struct Job {
    /* identity ------------------------------------------------------- */
    int job_id;            /* unique global id, 1-based                */
    int task_id;           /* parent task                              */
    int instance;          /* which release of the task (1, 2, ...)    */

    /* schedule fields ------------------------------------------------ */
    int release;           /* arrival time                             */
    int abs_deadline;      /* absolute deadline = release + D_i        */
    int period;            /* parent task's period (used by ext_time)  */
    int wcet;              /* total execution time required            */
    int remaining;         /* remaining execution time                 */

    /* bookkeeping for metrics --------------------------------------- */
    int  first_start;      /* time of first dispatch (-1 if none)      */
    int  completion;       /* completion time      (-1 if none)        */
    int  preempt_count;    /* # times this job was preempted           */
    int  completed;        /* 1 once job has finished                  */

    struct Job *next;      /* used by whichever queue holds the job    */
} Job;

/* Allocate and fully initialize a Job. Caller owns the pointer. */
Job* job_create(int job_id, int task_id, int instance,
                int release, int abs_deadline,
                int period, int wcet);

void job_free(Job *j);

/* Slack / laxity at time t. While a job runs, slack is invariant
 * (deadline-t-rem stays the same as t and rem move together). */
int  job_laxity(const Job *j, int t);

#endif
