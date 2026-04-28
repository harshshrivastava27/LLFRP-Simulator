#ifndef METRICS_H
#define METRICS_H

#include "task.h"
#include "job.h"

/*
 * Metrics module
 *
 * Records:
 *   - the per-tick schedule (one entry per time unit), and
 *   - per-job timing data for every job that completes.
 *
 * From these, prints all of the metrics the user requested:
 *   A. # decision points
 *   B. # context switches  (voluntary + involuntary)
 *   C. # preemptions
 *   D. # cache impact points
 *   E. response time   (min/max/avg)
 *   F. waiting  time   (min/max/avg)
 *   G. jitter          (arrival-time jitter, response-time jitter
 *                       both absolute and relative, per-task & overall)
 *   H. CPU utilization
 *   I. latency         (max latency per task, overall max)
 *   J. lateness        (max lateness per task, overall max)
 *
 * All energy-related accounting lives in energy.c, but it READS the
 * counters maintained here.
 */

/* One entry per simulated time tick. */
typedef struct {
    int t_start;       /* tick start (== tick number)         */
    int t_end;         /* tick end                            */
    int task_id;       /* 0 if idle, else task id             */
    int job_id;        /* 0 if idle, else job id              */
} ScheduleEntry;

/* Per-job record (filled at completion / hyperperiod end). */
typedef struct {
    int  job_id;
    int  task_id;
    int  instance;
    int  release;
    int  abs_deadline;
    int  first_start;     /* -1 if never started               */
    int  completion;      /* -1 if not completed before sim end */
    int  preempt_count;
    int  wcet;
    int  completed;
} JobRecord;

typedef struct {
    /* counters --------------------------------------------------- */
    long long decision_points;
    long long ctx_switches_voluntary;     /* job completed, picked next */
    long long ctx_switches_involuntary;   /* preemption                 */
    long long preemptions;
    long long cache_impacts;
    long long idle_ticks;
    long long busy_ticks;

    /* schedule trace (one slot per tick) ------------------------- */
    ScheduleEntry *trace;
    long long trace_len;
    long long trace_cap;

    /* completed job records (for response/jitter/lateness) ------- */
    JobRecord *jobs;
    int        jobs_len;
    int        jobs_cap;

    /* simulation envelope --------------------------------------- */
    long long hyperperiod;
    int       num_tasks;
} Metrics;

/* lifecycle */
void metrics_init    (Metrics *m, int num_tasks, long long hyperperiod);
void metrics_destroy (Metrics *m);

/* event recorders (called from the scheduler) -------------------- */
void metrics_log_tick           (Metrics *m, int t, const Job *running);
void metrics_inc_decision       (Metrics *m);
void metrics_inc_voluntary_cs   (Metrics *m);
void metrics_inc_involuntary_cs (Metrics *m);
void metrics_inc_preemption     (Metrics *m);
void metrics_inc_cache_impact   (Metrics *m);

/* called when a job completes (or at end of sim if not completed) */
void metrics_record_job (Metrics *m, const Job *j);

/* reporting ----------------------------------------------------- */
void metrics_print_schedule (const Metrics *m);
void metrics_print_summary  (const Metrics *m, const Task *tasks, int n);

#endif
