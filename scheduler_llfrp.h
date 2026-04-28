#ifndef SCHEDULER_LLFRP_H
#define SCHEDULER_LLFRP_H

#include "task.h"
#include "metrics.h"

/*
 * LLFRP scheduler (Least Laxity First with Reduced Pre-emptions),
 * Baid, Subramanya & Raveendran, GSTF JoC, 2014.
 *
 * Implementation faithful to Section III of the paper:
 *
 *   - Each tick is a candidate scheduling instant.
 *   - Decision points: arrival, departure, laxity tie with head of
 *     readyQ, or a deferred-switch deadline arriving.
 *   - On every preemption decision point, extension_time() is computed
 *     over the deadline-ordered queue. If it is >= Threshold, the
 *     currently running job is allowed to continue; otherwise we
 *     preempt and run the highest-priority ready job.
 *   - Strict-LLF behaviour (preempt every tick on a laxity tie) is
 *     therefore replaced by a controlled extension whenever it is safe.
 *
 * The function is named `run_llf_rcs` to keep continuity with the
 * existing project (LLFRP == LLF with Reduced Context Switches).
 */

void run_llf_rcs(const Task *tasks, int n,
                 int threshold,        /* >= 1; paper suggests small  */
                 Metrics *m);

#endif
