#ifndef UTILS_H
#define UTILS_H

/* Greatest common divisor (Euclidean algorithm). */
long long gcd_ll(long long a, long long b);

/* Least common multiple. */
long long lcm_ll(long long a, long long b);

/* Hyperperiod = LCM of all task periods. */
long long hyperperiod(const int *periods, int n);

/* Ceiling division for positive integers: ceil(a / b). */
long long ceil_div(long long a, long long b);

#endif
