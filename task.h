#ifndef TASK_H
#define TASK_H

/*
 * Task ADT
 *
 * A periodic real-time task is described by:
 *   - phase (first arrival time, a_i)
 *   - period (T_i)
 *   - WCET / execution time (C_i)
 *   - relative deadline (D_i)
 *
 * Input file format (whitespace separated, one task per line):
 *   <phase> <period> <wcet> <deadline>
 *
 * Lines beginning with '#' or empty lines are skipped.
 */

typedef struct {
    int task_id;     /* 1-based, assigned in load order */
    int phase;       /* first release time           */
    int period;
    int wcet;        /* worst-case execution time    */
    int deadline;    /* relative deadline            */
} Task;

/* Load tasks from a file. Returns NULL on error. *n receives count. */
Task* task_load(const char *filename, int *n);

void task_free(Task *tasks);

/* Pretty-print loaded tasks. */
void task_print_all(const Task *tasks, int n);

/* Total task-set utilization = sum(C_i / T_i). */
double task_utilization(const Task *tasks, int n);

#endif
