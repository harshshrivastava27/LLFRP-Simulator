# LLFRP Scheduler Simulator

C implementation of the LLF-RP (Least Laxity First with Reduced
Pre-emptions) scheduling algorithm from:

> Baid, Subramanya & Raveendran. "LLFRP: An Energy Efficient Variant
> of LLF with Reduced Pre-emptions for Real-Time Systems."
> GSTF International Journal on Computing, Vol. 3 No. 4, April 2014.

## Build

```
make
```

Produces a `scheduler` binary.

## Run

```
./scheduler task.txt              # paper's Table I example
./scheduler task2.txt -q          # quiet mode (suppress per-tick trace)
./scheduler task3.txt -t 2        # threshold = 2
./scheduler -h                    # help
```

## Task file format

One task per line:

```
<phase> <period> <wcet> <deadline>
```

Lines beginning with `#` and blank lines are skipped.

## Modules

| File | Role |
|------|------|
| `task.{h,c}` | Task ADT + file loader |
| `job.{h,c}` | Job ADT (with metric bookkeeping fields) |
| `ready_queue.{h,c}` | Sorted-by-laxity priority queue |
| `deadline_queue.{h,c}` | Sorted-by-deadline view used by `extension_time()` |
| `metrics.{h,c}` | All metric counters + summary printer |
| `energy.{h,c}` | Energy model (paper eqs. 4–8) |
| `utils.{h,c}` | gcd / lcm / hyperperiod / ceil_div |
| `scheduler_llfrp.{h,c}` | The LLFRP algorithm proper |
| `main.c` | CLI entry point |

## Metrics produced

A. # Decision points
B. # Context switches (voluntary + involuntary)
C. # Preemptions
D. # Cache impact points
E. Response time (min / avg / max, per task and overall)
F. Waiting time (min / avg / max, per task and overall)
G. Jitter — arrival-time jitter, response-time jitter (absolute & relative)
H. CPU utilization
I. Latency (max per task)
J. Lateness (max per task)

Plus full energy breakdown (E_JOBS, E_DECISION, E_PREEMPT, E_CACHE,
E_TOTAL_DYN, % unproductive) per the paper's formulae.

## Validation

Running `./scheduler task.txt` reproduces **exactly** the schedule
shown in Figure 1 of the paper, including the **single preemption**
that LLFRP achieves on that task set.
