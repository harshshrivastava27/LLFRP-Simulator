#include <stdio.h>
#include "energy.h"

EnergyModel energy_default_model(void) {
    EnergyModel m = {
        .e_per_tick     = 1.00,
        .e_per_decision = 0.05,
        .e_per_preempt  = 0.50,
        .e_per_cache    = 0.20,
    };
    return m;
}

EnergyResult energy_compute(const Metrics    *m,
                            const Task       *tasks,
                            int               n,
                            const EnergyModel*mdl)
{
    EnergyResult r = {0};

    /* E_JOBS (eq. 5) -- sum over tasks: exec_time * (H / period)
     * which equals total useful work across the hyperperiod. */
    long long total_work_ticks = 0;
    for (int i = 0; i < n; i++) {
        if (tasks[i].period == 0) continue;
        long long n_inst = m->hyperperiod / tasks[i].period;
        total_work_ticks += (long long)tasks[i].wcet * n_inst;
    }
    r.e_jobs    = total_work_ticks    * mdl->e_per_tick;

    /* E_PREEMPT (eq. 6, 7) */
    r.e_preempt = m->preemptions      * mdl->e_per_preempt;

    /* E_DEC (eq. 8) */
    r.e_decision= m->decision_points  * mdl->e_per_decision;

    /* Cache impact (treated separately; the paper folds it into eq. 7
     * but we make it visible in the breakdown). */
    r.e_cache   = m->cache_impacts    * mdl->e_per_cache;

    r.e_total_dyn    = r.e_jobs + r.e_decision + r.e_preempt + r.e_cache;
    r.e_unproductive = r.e_decision + r.e_preempt + r.e_cache;
    r.pct_unproductive = r.e_total_dyn > 0
                       ? (100.0 * r.e_unproductive / r.e_total_dyn)
                       : 0.0;
    return r;
}

void energy_print(const EnergyResult *r, const EnergyModel *mdl) {
    printf("\n=== Energy Consumption (paper eqs. 4-8) ===\n");
    printf("Model coefficients: tick=%.2f decision=%.2f preempt=%.2f cache=%.2f\n",
           mdl->e_per_tick, mdl->e_per_decision,
           mdl->e_per_preempt, mdl->e_per_cache);
    printf("E_JOBS      (useful)     : %12.3f\n", r->e_jobs);
    printf("E_DECISION  (overhead)   : %12.3f\n", r->e_decision);
    printf("E_PREEMPT   (overhead)   : %12.3f\n", r->e_preempt);
    printf("E_CACHE     (overhead)   : %12.3f\n", r->e_cache);
    printf("--------------------------------------------\n");
    printf("E_TOTAL_DYN              : %12.3f\n", r->e_total_dyn);
    printf("E_UNPRODUCTIVE           : %12.3f\n", r->e_unproductive);
    printf("%% Unproductive energy    : %12.3f%%\n", r->pct_unproductive);
}
