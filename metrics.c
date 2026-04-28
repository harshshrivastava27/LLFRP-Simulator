#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "metrics.h"

/* --------------------------------------------------------------- */
/* lifecycle                                                       */
/* --------------------------------------------------------------- */

void metrics_init(Metrics *m, int num_tasks, long long hyperperiod) {
    memset(m, 0, sizeof(*m));
    m->num_tasks   = num_tasks;
    m->hyperperiod = hyperperiod;

    m->trace_cap = hyperperiod > 0 ? hyperperiod : 64;
    m->trace     = malloc(sizeof(ScheduleEntry) * m->trace_cap);
    m->trace_len = 0;

    m->jobs_cap  = 64;
    m->jobs      = malloc(sizeof(JobRecord) * m->jobs_cap);
    m->jobs_len  = 0;
}

void metrics_destroy(Metrics *m) {
    free(m->trace);
    free(m->jobs);
    memset(m, 0, sizeof(*m));
}

/* --------------------------------------------------------------- */
/* counters                                                        */
/* --------------------------------------------------------------- */

void metrics_inc_decision      (Metrics *m) { m->decision_points++;          }
void metrics_inc_voluntary_cs  (Metrics *m) { m->ctx_switches_voluntary++;   }
void metrics_inc_involuntary_cs(Metrics *m) { m->ctx_switches_involuntary++; }
void metrics_inc_preemption    (Metrics *m) { m->preemptions++;              }
void metrics_inc_cache_impact  (Metrics *m) { m->cache_impacts++;            }

void metrics_log_tick(Metrics *m, int t, const Job *running) {
    if (m->trace_len >= m->trace_cap) {
        long long nc = m->trace_cap * 2;
        ScheduleEntry *n = realloc(m->trace, sizeof(ScheduleEntry) * nc);
        if (!n) return;
        m->trace = n;
        m->trace_cap = nc;
    }
    ScheduleEntry *e = &m->trace[m->trace_len++];
    e->t_start = t;
    e->t_end   = t + 1;
    if (running) {
        e->task_id = running->task_id;
        e->job_id  = running->job_id;
        m->busy_ticks++;
    } else {
        e->task_id = 0;
        e->job_id  = 0;
        m->idle_ticks++;
    }
}

void metrics_record_job(Metrics *m, const Job *j) {
    if (m->jobs_len >= m->jobs_cap) {
        int nc = m->jobs_cap * 2;
        JobRecord *n = realloc(m->jobs, sizeof(JobRecord) * nc);
        if (!n) return;
        m->jobs = n;
        m->jobs_cap = nc;
    }
    JobRecord *r = &m->jobs[m->jobs_len++];
    r->job_id        = j->job_id;
    r->task_id       = j->task_id;
    r->instance      = j->instance;
    r->release       = j->release;
    r->abs_deadline  = j->abs_deadline;
    r->first_start   = j->first_start;
    r->completion    = j->completion;
    r->preempt_count = j->preempt_count;
    r->wcet          = j->wcet;
    r->completed     = j->completed;
}

/* --------------------------------------------------------------- */
/* schedule pretty-print                                           */
/* --------------------------------------------------------------- */

void metrics_print_schedule(const Metrics *m) {
    printf("\n=== Schedule (per tick) ===\n");
    printf(" %-6s  %-6s  %-12s\n", "start", "end", "running");

    /* Compress consecutive ticks with same job for readability. */
    long long i = 0;
    while (i < m->trace_len) {
        long long j = i + 1;
        while (j < m->trace_len &&
               m->trace[j].task_id == m->trace[i].task_id &&
               m->trace[j].job_id  == m->trace[i].job_id)
            j++;

        int s = m->trace[i].t_start;
        int e = m->trace[j-1].t_end;
        if (m->trace[i].task_id == 0)
            printf(" %-6d  %-6d  IDLE\n", s, e);
        else
            printf(" %-6d  %-6d  T%d/J%d\n",
                   s, e, m->trace[i].task_id, m->trace[i].job_id);
        i = j;
    }
}

/* --------------------------------------------------------------- */
/* helpers for per-task statistics                                 */
/* --------------------------------------------------------------- */

/* For one task, return:
 *   *cnt   = how many jobs of this task we have records for
 *   *rt_min, *rt_max, *rt_sum  = response-time stats
 *   *wt_min, *wt_max, *wt_sum  = waiting-time stats
 *   *abs_jitter = max(rt) - min(rt)
 *   *rel_jitter = max( |rt[i+1] - rt[i]| )
 *   *arr_jitter = max( |actual_inter_arrival - period| )
 *      (since we model fully periodic releases, this is always 0,
 *       but we keep the framework.)
 *   *latency_max = max( completion - first_start ) over completed jobs
 *   *lateness_max = max( completion - abs_deadline ) -- can be negative
 *   *missed     = how many jobs missed their deadline
 *   *completed  = how many jobs completed
 *
 * Returns 1 if task has at least one job, 0 otherwise. */
static int task_stats(const JobRecord *jobs, int n, int task_id,
                      int *cnt,
                      double *rt_min, double *rt_max, double *rt_sum,
                      double *wt_min, double *wt_max, double *wt_sum,
                      double *abs_jit, double *rel_jit, double *arr_jit,
                      double *lat_max,
                      int    *latn_max,
                      int    *missed, int *completed,
                      int     period)
{
    int found = 0;
    *rt_min = *wt_min = 1e18;
    *rt_max = *wt_max = -1e18;
    *rt_sum = *wt_sum = 0.0;
    *abs_jit = *rel_jit = *arr_jit = 0.0;
    *lat_max = -1e18;
    *latn_max = INT_MIN;
    *missed = 0;
    *completed = 0;
    *cnt = 0;

    /* gather rt[] in instance order */
    double rts[1024];
    int    rels[1024];
    int    k = 0;

    for (int i = 0; i < n && k < 1024; i++) {
        if (jobs[i].task_id != task_id) continue;
        if (!jobs[i].completed) continue;

        double rt = jobs[i].completion - jobs[i].release;
        double wt = rt - jobs[i].wcet;
        rts[k]  = rt;
        rels[k] = jobs[i].release;
        k++;

        *rt_sum += rt; *wt_sum += wt;
        if (rt < *rt_min) *rt_min = rt;
        if (rt > *rt_max) *rt_max = rt;
        if (wt < *wt_min) *wt_min = wt;
        if (wt > *wt_max) *wt_max = wt;

        double lat = jobs[i].completion - jobs[i].first_start;
        if (lat > *lat_max) *lat_max = lat;

        int late = jobs[i].completion - jobs[i].abs_deadline;
        if (late > *latn_max) *latn_max = late;
        if (late > 0) (*missed)++;
        (*completed)++;
        found = 1;
    }
    *cnt = k;

    if (k > 0) {
        *abs_jit = *rt_max - *rt_min;
        for (int i = 1; i < k; i++) {
            double d = rts[i] - rts[i-1]; if (d < 0) d = -d;
            if (d > *rel_jit) *rel_jit = d;
            int actual_iat = rels[i] - rels[i-1];
            int diff = actual_iat - period; if (diff < 0) diff = -diff;
            if (diff > *arr_jit) *arr_jit = diff;
        }
    } else {
        *rt_min = *rt_max = *wt_min = *wt_max = 0;
        *lat_max = 0;
        *latn_max = 0;
    }

    /* also count jobs that didn't even complete as "missed" */
    for (int i = 0; i < n; i++) {
        if (jobs[i].task_id != task_id) continue;
        if (!jobs[i].completed) {
            (*missed)++;
            if (0 - jobs[i].abs_deadline > *latn_max) {
                /* we don't know completion, skip lateness for incomplete */
            }
        }
    }

    return found;
}

/* --------------------------------------------------------------- */
/* full report                                                     */
/* --------------------------------------------------------------- */

void metrics_print_summary(const Metrics *m, const Task *tasks, int n) {
    printf("\n=========================================================\n");
    printf("  LLFRP Schedule Metrics\n");
    printf("=========================================================\n");
    printf("Hyperperiod (sim length) : %lld\n", m->hyperperiod);
    printf("Total jobs released      : %d\n",   m->jobs_len);

    int on_time = 0;
    for (int i = 0; i < m->jobs_len; i++)
        if (m->jobs[i].completed &&
            m->jobs[i].completion <= m->jobs[i].abs_deadline) on_time++;
    int deadline_misses = m->jobs_len - on_time;
    printf("Jobs on time             : %d\n", on_time);
    printf("Deadline misses          : %d\n", deadline_misses);

    /* ---------- A: decision points ------------------------------ */
    printf("\nA. Decision points          : %lld\n", m->decision_points);

    /* ---------- B: context switches ----------------------------- */
    long long total_cs = m->ctx_switches_voluntary
                       + m->ctx_switches_involuntary;
    printf("B. Context switches         : %lld\n", total_cs);
    printf("     - voluntary  (compl.)  : %lld\n", m->ctx_switches_voluntary);
    printf("     - involuntary(preempt) : %lld\n", m->ctx_switches_involuntary);

    /* ---------- C: preemptions ---------------------------------- */
    printf("C. Preemptions              : %lld\n", m->preemptions);

    /* ---------- D: cache impact points -------------------------- */
    printf("D. Cache impact points      : %lld\n", m->cache_impacts);

    /* ---------- E,F,G,I,J : per task & overall ------------------ */
    printf("\nPer-task timing:\n");
    printf("%-4s  %4s  %4s  %4s  %4s  %4s  %4s  %4s  %4s  %4s  %4s  %4s\n",
           "Tsk","#Job","RTmn","RTmx","RTav","WTmn","WTmx","WTav",
           "ARJ","RRJ","Lat","Late");

    /* Overall accumulators */
    double O_rt_min = 1e18, O_rt_max = -1e18, O_rt_sum = 0;
    double O_wt_min = 1e18, O_wt_max = -1e18, O_wt_sum = 0;
    int    O_cnt = 0;
    double O_abs_jit = 0, O_rel_jit = 0, O_arr_jit = 0;
    double O_lat = 0;
    int    O_late = INT_MIN;

    for (int i = 0; i < n; i++) {
        int cnt, missed, comp;
        double rtmn, rtmx, rtsum, wtmn, wtmx, wtsum;
        double abj, rej, arj, latmx;
        int latn;
        task_stats(m->jobs, m->jobs_len, tasks[i].task_id,
                   &cnt, &rtmn, &rtmx, &rtsum, &wtmn, &wtmx, &wtsum,
                   &abj, &rej, &arj, &latmx, &latn,
                   &missed, &comp, tasks[i].period);

        double rtav = cnt ? rtsum / cnt : 0.0;
        double wtav = cnt ? wtsum / cnt : 0.0;
        printf("T%-3d  %4d  %4.0f  %4.0f  %4.1f  %4.0f  %4.0f  %4.1f"
               "  %4.0f  %4.0f  %4.0f  %4d\n",
               tasks[i].task_id, cnt,
               rtmn, rtmx, rtav,
               wtmn, wtmx, wtav,
               abj, rej, latmx, latn);

        if (cnt > 0) {
            O_rt_sum += rtsum; O_wt_sum += wtsum; O_cnt += cnt;
            if (rtmn < O_rt_min) O_rt_min = rtmn;
            if (rtmx > O_rt_max) O_rt_max = rtmx;
            if (wtmn < O_wt_min) O_wt_min = wtmn;
            if (wtmx > O_wt_max) O_wt_max = wtmx;
            if (abj  > O_abs_jit) O_abs_jit = abj;
            if (rej  > O_rel_jit) O_rel_jit = rej;
            if (arj  > O_arr_jit) O_arr_jit = arj;
            if (latmx > O_lat) O_lat = latmx;
            if (latn  > O_late) O_late = latn;
        }
    }
    if (O_cnt == 0) { O_rt_min = O_rt_max = O_wt_min = O_wt_max = 0; O_late = 0; }

    printf("\nE. Response time   min/avg/max : %.0f / %.2f / %.0f\n",
           O_rt_min, O_cnt ? O_rt_sum/O_cnt : 0, O_rt_max);
    printf("F. Waiting  time   min/avg/max : %.0f / %.2f / %.0f\n",
           O_wt_min, O_cnt ? O_wt_sum/O_cnt : 0, O_wt_max);
    printf("G. Jitter  (max over tasks):\n");
    printf("     Arrival-time jitter        : %.0f\n", O_arr_jit);
    printf("     Response-time abs. jitter  : %.0f\n", O_abs_jit);
    printf("     Response-time rel. jitter  : %.0f\n", O_rel_jit);

    /* ---------- H: CPU utilization ------------------------------ */
    double util = m->hyperperiod
                ? (double)m->busy_ticks / (double)m->hyperperiod
                : 0.0;
    printf("H. CPU utilization             : %.4f  (%lld busy / %lld total ticks)\n",
           util, m->busy_ticks, m->hyperperiod);

    /* ---------- I: latency -------------------------------------- */
    printf("I. Max latency (any task)      : %.0f\n", O_lat);

    /* ---------- J: lateness ------------------------------------- */
    printf("J. Max lateness (any task)     : %d\n",
           (O_cnt == 0 ? 0 : O_late));
}
