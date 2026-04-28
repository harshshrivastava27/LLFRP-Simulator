#include "utils.h"

long long gcd_ll(long long a, long long b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

long long lcm_ll(long long a, long long b) {
    if (a == 0 || b == 0) return 0;
    return (a / gcd_ll(a, b)) * b;
}

long long hyperperiod(const int *periods, int n) {
    if (n <= 0) return 0;
    long long h = periods[0];
    for (int i = 1; i < n; i++) {
        h = lcm_ll(h, periods[i]);
    }
    return h;
}

long long ceil_div(long long a, long long b) {
    if (b <= 0) return 0;
    if (a <= 0) return 0;
    return (a + b - 1) / b;
}
