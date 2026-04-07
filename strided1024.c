#include <stdio.h>

#define N 1000000
#define STRIDE 1024   // try 4, 16, 64, 256

int arr[N];

int main() {
    // Initialize
    for (int i = 0; i < N; i++) {
        arr[i] = i;
    }

    long sum = 0;

    // Strided access
    for (int i = 0; i < N; i += STRIDE) {
        sum += arr[i];
    }

    printf("Sum = %ld\n", sum);
    return 0;
}