#ifndef ENERGY_H
#define ENERGY_H

#include "metrics.h"
#include "task.h"

/*
 * Energy model (LLFRP paper, eqs. 4-8)
 *
 *   E_TOT_DYN = E_JOBS + E_PREEMPT + E_DEC                            (4)
 *   E_JOBS    = sum_i  exec_time(T_i) * (Hyperperiod / Period(T_i))   (5)
 *   E_PREEMPT = #preemptions * preemption_time                        (6)
 *   preemption_time = ctx_switch_time + avg_cache_miss_time           (7)
 *   E_DEC     = #decisions * per_decision_time                        (8)
 *
 * The numbers below are NORMALIZED defaults expressed in "energy units
 * per nominal CPU tick". They can be tuned to match a real platform.
 *
 *   - 1 useful execution tick           : 1.0 unit
 *   - 1 scheduler decision              : E_PER_DECISION (small)
 *   - 1 preemption (ctx + cache fills)  : E_PER_PREEMPT  (larger)
 *   - 1 cache impact (refill on resume) : E_PER_CACHE
 *
 * The paper reports "% unproductive energy" = (E_PREEMPT + E_DEC + cache)
 *                                             / E_TOTAL.
 */

typedef struct {
    double e_per_tick;       /* E_JOBS per useful tick           */
    double e_per_decision;   /* per-decision overhead            */
    double e_per_preempt;    /* per-preemption overhead          */
    double e_per_cache;      /* per cache-impact point overhead  */
} EnergyModel;

/* Sensible defaults: 1 / 0.05 / 0.5 / 0.2  (paper-style normalised). */
EnergyModel energy_default_model(void);

typedef struct {
    double e_jobs;
    double e_decision;
    double e_preempt;
    double e_cache;
    double e_total_dyn;       /* sum                                  */
    double e_unproductive;    /* e_decision + e_preempt + e_cache      */
    double pct_unproductive;  /* 100 * unproductive / total            */
} EnergyResult;

/* Compute energy from the metrics counters and the task set. */
EnergyResult energy_compute(const Metrics    *m,
                            const Task       *tasks,
                            int               num_tasks,
                            const EnergyModel*model);

void energy_print(const EnergyResult *r, const EnergyModel *model);

#endif
