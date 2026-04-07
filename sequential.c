#include <stdio.h>

#define N 100000

int arr[N];

int main() {
    // Initialize (write)
    for (int i = 0; i < N; i++) {
        arr[i] = i;
    }

    // Read + compute
    long sum = 0;
    for (int i = 0; i < N; i++) {
        sum += arr[i];
    }

    printf("Sum = %ld\n", sum);
    return 0;
}