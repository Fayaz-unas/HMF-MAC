#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N (1 << 20)   // 1M elements

int arr[N];
int idx[N];

int main() {
    // Initialize array
    for (int i = 0; i < N; i++) {
        arr[i] = i;
        idx[i] = i;
    }

    // Shuffle indices (Fisher-Yates)
    srand(0);
    for (int i = N - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = idx[i];
        idx[i] = idx[j];
        idx[j] = temp;
    }

    // Random access pattern
    long long sum = 0;
    for (int i = 0; i < N; i++) {
        sum += arr[idx[i]];
    }

    printf("%lld\n", sum);
    return 0;
}