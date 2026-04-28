#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "task.h"
#include "scheduler_llfrp.h"
#include "metrics.h"
#include "energy.h"

static void usage(const char *prog) {
    printf("Usage: %s <taskfile> [options]\n", prog);
    printf("\nOptions:\n");
    printf("  -t N      LLFRP threshold (default 1)\n");
    printf("  -q        Quiet: skip per-tick schedule trace\n");
    printf("  -h        Show this help\n");
    printf("\nTask file format (one task per line):\n");
    printf("  <phase> <period> <wcet> <deadline>\n");
    printf("Lines starting with '#' are ignored.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *fname     = argv[1];
    int         threshold = 1;
    int         quiet     = 0;

    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i], "-q")) { quiet = 1; }
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) {
            threshold = atoi(argv[++i]);
            if (threshold < 1) threshold = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    int n = 0;
    Task *tasks = task_load(fname, &n);
    if (!tasks || n == 0) {
        fprintf(stderr, "Failed to load tasks from '%s'\n", fname);
        return 1;
    }

    task_print_all(tasks, n);

    if (task_utilization(tasks, n) > 1.0 + 1e-9) {
        printf("\n[Note] U > 1: task set is not feasible under any "
               "uniprocessor scheduler.\n");
    }

    Metrics M;
    metrics_init(&M, n, 0);   /* hyperperiod set inside scheduler */

    printf("\n[Running LLFRP, threshold=%d]\n", threshold);
    run_llf_rcs(tasks, n, threshold, &M);

    if (!quiet) metrics_print_schedule(&M);
    metrics_print_summary(&M, tasks, n);

    EnergyModel  em = energy_default_model();
    EnergyResult er = energy_compute(&M, tasks, n, &em);
    energy_print(&er, &em);

    metrics_destroy(&M);
    task_free(tasks);
    return 0;
}
