#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "task.h"

/* Read one logical line, skipping blanks and comments. Returns 0 on EOF. */
static int next_line(FILE *fp, char *buf, int sz) {
    while (fgets(buf, sz, fp)) {
        char *p = buf;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '\0' || *p == '#') continue;   /* blank / comment */
        return 1;
    }
    return 0;
}

Task* task_load(const char *filename, int *n) {
    if (!filename || !n) return NULL;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "task_load: cannot open '%s'\n", filename);
        return NULL;
    }

    int  cap   = 8;
    int  count = 0;
    Task *tasks = malloc(sizeof(Task) * cap);
    if (!tasks) { fclose(fp); return NULL; }

    char line[256];
    int  lineno = 0;

    while (next_line(fp, line, sizeof(line))) {
        lineno++;
        int a, p, c, d;
        if (sscanf(line, "%d %d %d %d", &a, &p, &c, &d) != 4) {
            fprintf(stderr,
                    "task_load: line %d malformed (expected 4 ints): %s",
                    lineno, line);
            continue;
        }
        if (p <= 0 || c <= 0 || d <= 0 || c > d) {
            fprintf(stderr,
                    "task_load: line %d has invalid values "
                    "(period=%d, wcet=%d, dl=%d)\n", lineno, p, c, d);
            continue;
        }

        if (count >= cap) {
            cap *= 2;
            Task *nt = realloc(tasks, sizeof(Task) * cap);
            if (!nt) { free(tasks); fclose(fp); return NULL; }
            tasks = nt;
        }

        tasks[count].task_id  = count + 1;
        tasks[count].phase    = a;
        tasks[count].period   = p;
        tasks[count].wcet     = c;
        tasks[count].deadline = d;
        count++;
    }

    fclose(fp);

    if (count == 0) {
        free(tasks);
        *n = 0;
        return NULL;
    }
    *n = count;
    return tasks;
}

void task_free(Task *tasks) { free(tasks); }

void task_print_all(const Task *tasks, int n) {
    printf("\n=== Task Set (%d tasks) ===\n", n);
    printf("%-6s %-8s %-8s %-8s %-10s %-8s\n",
           "ID", "Phase", "Period", "WCET", "Deadline", "U_i");
    for (int i = 0; i < n; i++) {
        double u = (double)tasks[i].wcet / (double)tasks[i].period;
        printf("T%-5d %-8d %-8d %-8d %-10d %-8.4f\n",
               tasks[i].task_id, tasks[i].phase, tasks[i].period,
               tasks[i].wcet, tasks[i].deadline, u);
    }
    printf("Total Utilization U = %.4f\n", task_utilization(tasks, n));
}

double task_utilization(const Task *tasks, int n) {
    double u = 0.0;
    for (int i = 0; i < n; i++)
        u += (double)tasks[i].wcet / (double)tasks[i].period;
    return u;
}
