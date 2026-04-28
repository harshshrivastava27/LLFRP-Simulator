#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "scheduler_llfrp.h"
#include "ready_queue.h"
#include "deadline_queue.h"
#include "utils.h"
#include "job.h"

/* --------------------------------------------------------------- */
/*  extension_time()                                               */
/* --------------------------------------------------------------- */
/*
 * Given the currently running job `cur` at time t and the ready queue
 * `rq`, compute the maximum number of additional ticks `cur` may
 * execute right now without causing any equal-or-higher-priority
 * (earlier-deadline) ready job to miss its deadline.
 *
 * Algorithm (paper, with the minor typo fixed):
 *
 *   Build a list j1..jm = ready jobs whose absolute deadline is
 *   <= deadline(cur), sorted by ASCENDING absolute deadline. Then
 *
 *       extension = min over i of [ slack(j_i, t)
 *                                   - sum_{k < i} ( remaining(j_k, t)
 *                                     + ceil((dl(j_i)-dl(j_k))/period(j_k))
 *                                       * exec(j_k) ) ]
 *
 *   That is: for each higher-priority ready job j_i, the slack it
 *   currently has, minus the cumulative work of the still-earlier-
 *   deadline jobs (their current remaining + the future instances of
 *   their tasks that must also fit in before j_i's deadline).
 *
 *   The minimum across all such j_i bounds how long we may delay cur.
 *
 * The function returns INT_MAX if no ready job has deadline <= cur's.
 * Negative values are clamped to 0.
 */
static int extension_time(const Job *cur, const ReadyQueue *rq, int t) {
    DeadlineQueue dq;
    dq_init(&dq);

    /* Walk the ready queue and pick jobs with deadline <= cur's. */
    for (Job *j = rq->head; j; j = j->next) {
        if (j->abs_deadline <= cur->abs_deadline)
            dq_insert(&dq, j);
    }

    if (dq.count == 0) {
        dq_destroy(&dq);
        return INT_MAX;
    }

    long long min_ext = LLONG_MAX;

    for (int i = 0; i < dq.count; i++) {
        Job *ji = dq.items[i];
        long long slack_i = ji->abs_deadline - t - ji->remaining;

        long long cum_work = 0;
        for (int k = 0; k < i; k++) {
            Job *jk = dq.items[k];
            long long extra =
                ceil_div(ji->abs_deadline - jk->abs_deadline,
                         jk->period) * (long long)jk->wcet;
            cum_work += jk->remaining + extra;
        }

        long long candidate = slack_i - cum_work;
        if (candidate < min_ext) min_ext = candidate;
    }

    dq_destroy(&dq);

    if (min_ext < 0)            min_ext = 0;
    if (min_ext > INT_MAX)      min_ext = INT_MAX;
    return (int)min_ext;
}

/* --------------------------------------------------------------- */
/*  Main scheduling loop                                           */
/* --------------------------------------------------------------- */

void run_llf_rcs(const Task *tasks, int n, int threshold, Metrics *m) {

    if (threshold < 1) threshold = 1;

    /* Hyperperiod: compute inline to avoid an unnecessary allocation. */
    long long H = tasks[0].period;
    for (int i = 1; i < n; i++) H = lcm_ll(H, tasks[i].period);

    if (H <= 0) {
        fprintf(stderr, "Hyperperiod is zero; nothing to schedule.\n");
        return;
    }
    m->hyperperiod = H;

    ReadyQueue rq;
    rq_init(&rq);

    /* Per-task: when is the next release, and the running instance #. */
    int *next_release = malloc(sizeof(int) * n);
    int *instance_no  = malloc(sizeof(int) * n);
    for (int i = 0; i < n; i++) {
        next_release[i] = tasks[i].phase;
        instance_no[i]  = 0;
    }

    Job *current        = NULL;
    int  prev_task_id   = -1;       /* for cache-impact detection      */
    int  ext_remaining  =  0;       /* ticks left in active extension  */
    int  in_extension   =  0;       /* are we currently extending?     */
    int  next_global_jid = 0;

    long long missed_deadlines = 0;

    for (long long t = 0; t < H; t++) {

        /* (1) Process arrivals --------------------------------------- */
        int had_arrival = 0;
        for (int i = 0; i < n; i++) {
            if (next_release[i] == (int)t) {
                instance_no[i]++;
                ++next_global_jid;
                Job *j = job_create(
                    next_global_jid,
                    tasks[i].task_id,
                    instance_no[i],
                    (int)t,
                    (int)t + tasks[i].deadline,
                    tasks[i].period,
                    tasks[i].wcet);
                rq_insert(&rq, j, (int)t);
                next_release[i] += tasks[i].period;
                had_arrival = 1;
            }
        }

        /* (2) Detect a "departure" decision point: current finished
         *     in the previous tick. (Handled below where we picked
         *     a new current; nothing to do here.) */

        /* (3) Detect "laxity tie with head of readyQ" or "highest
         *     priority changed". This requires that a current job
         *     exists. */
        int dec_point     = (had_arrival || current == NULL);
        int laxity_tie    = 0;
        int head_promoted = 0;
        if (current && !rq_empty(&rq)) {
            Job *top = rq_peek(&rq);
            int la_cur = job_laxity(current, (int)t);
            int la_top = job_laxity(top,     (int)t);
            if (la_top <  la_cur) head_promoted = 1;
            if (la_top == la_cur) laxity_tie    = 1;
            if (head_promoted || laxity_tie)   dec_point = 1;
        }

        /* (4) Deferred switch fires? */
        int deferred_fire = (in_extension && ext_remaining == 0);
        if (deferred_fire) dec_point = 1;

        if (dec_point) metrics_inc_decision(m);

        /* (5) LLFRP scheduling decision ------------------------------ */
        if (current == NULL) {
            /* CPU idle: pick highest-priority ready job, if any. */
            if (!rq_empty(&rq)) {
                current = rq_pop(&rq);
                if (current->first_start < 0) current->first_start = (int)t;
                if (prev_task_id != -1 && prev_task_id != current->task_id)
                    metrics_inc_cache_impact(m);
                metrics_inc_voluntary_cs(m);
                in_extension = 0;
                ext_remaining = 0;
            }
        } else if (head_promoted || laxity_tie || deferred_fire) {
            /*
             * A higher-priority (or tied) job became available, OR
             * an existing extension expired. Decide whether to
             * preempt.
             */
            int ext = extension_time(current, &rq, (int)t);

            /* Special LLFRP rule: if the running job currently has the
             * highest priority (no head_promoted, only laxity tie or
             * fresh arrival of equal-priority job), the paper says to
             * keep running. We already covered head_promoted; for a
             * pure tie we still call extension_time but use the same
             * threshold rule. */

            if (head_promoted && ext < threshold) {
                /* Preempt: the running job cannot safely defer. */
                Job *preempted = current;
                preempted->preempt_count++;
                rq_insert(&rq, preempted, (int)t);
                metrics_inc_preemption(m);
                metrics_inc_involuntary_cs(m);

                current = rq_pop(&rq);
                if (current->first_start < 0) current->first_start = (int)t;
                if (prev_task_id != -1 && prev_task_id != current->task_id)
                    metrics_inc_cache_impact(m);
                in_extension  = 0;
                ext_remaining = 0;
            } else {
                /* Defer the switch: keep current. */
                in_extension  = 1;
                /* The extension is bounded by ext, but capped by the
                 * job's own remaining time so we don't pretend to
                 * extend a job that's about to finish. */
                int cap = current->remaining;
                ext_remaining = ext < cap ? ext : cap;
                if (ext_remaining < 1) ext_remaining = 1;  /* run >=1 */
            }
        }

        /* (6) Record schedule, run one tick ------------------------- */
        if (current) {
            metrics_log_tick(m, (int)t, current);
            current->remaining--;
            if (in_extension && ext_remaining > 0) ext_remaining--;
            prev_task_id = current->task_id;

            if (current->remaining == 0) {
                /* Voluntary departure. */
                current->completion = (int)t + 1;
                current->completed  = 1;
                if (current->completion > current->abs_deadline)
                    missed_deadlines++;
                metrics_record_job(m, current);
                /* Cache impact: next dispatched job will be different
                 * task with high probability; we count it when we pick
                 * the next job (above) so we don't double-count here. */
                job_free(current);
                current = NULL;
                in_extension = 0;
                ext_remaining = 0;
                /* On the next loop iteration the "departure" branch
                 * takes effect by virtue of current==NULL. */
            }
        } else {
            metrics_log_tick(m, (int)t, NULL);   /* idle */
        }
    }

    /* Anything still left at hyperperiod end is incomplete. ----------- */
    if (current) {
        current->completed = 0;
        metrics_record_job(m, current);
        job_free(current);
        current = NULL;
    }
    while (!rq_empty(&rq)) {
        Job *j = rq_pop(&rq);
        j->completed = 0;
        metrics_record_job(m, j);
        job_free(j);
    }

    free(next_release);
    free(instance_no);

    if (missed_deadlines > 0)
        fprintf(stderr,
                "[LLFRP] WARNING: %lld deadline miss(es) detected.\n",
                missed_deadlines);
}
