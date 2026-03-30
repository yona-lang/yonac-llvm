#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
int safe(int queen, int *placed, int n, int col) {
    for (int i = 0; i < n; i++) {
        if (queen == placed[i]) return 0;
        if (abs(queen - placed[i]) == col + i) return 0;
    }
    return 1;
}
int64_t solve(int n, int row, int *placed, int nplaced, int col) {
    if (row > n) return 1;
    if (col > n) return 0;
    int64_t count = 0;
    if (safe(col, placed, nplaced, 1)) {
        placed[nplaced] = col;
        count += solve(n, row + 1, placed, nplaced + 1, 1);
    }
    count += solve(n, row, placed, nplaced, col + 1);
    return count;
}
int main(void) {
    int placed[20];
    printf("%ld\n", solve(10, 1, placed, 0, 1));
    return 0;
}
