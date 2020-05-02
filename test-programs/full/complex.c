
#include "softbrain.h"

#define NULL (void*)0

// DFG:
// Input: A, B, reset
// Output: C
//
// T = A * B
// B = Acc(A, B, reset)
//

#define WORD_SIZE 8

int main() {
    // Dummy values to just make this compilable
    const int A = 1;
    const int B = 2;
    const int C = 3;
    const int D = 4;

    int c1, c2, c3, c4, c5, c6, c7;
    int i, j, k, l;

    SB_CONFIG();

    SB_MEM_PORT_STREAM(NULL, 8, 8, 99, A);

    if (c1) {
        SB_MEM_PORT_STREAM(NULL, 8, 8, 100, B);
        SB_MEM_PORT_STREAM(NULL, 8, 8, 2, A);
    }

    if (c3) {
        SB_MEM_PORT_STREAM(NULL, 8, 8, 100, B);
    }

    if (c4) {
        if (c2) {
            for (i = 0; i < 100; i++) {
                SB_PORT_MEM_STREAM(C, 8, 8, 1, NULL);
            }
            SB_MEM_PORT_STREAM(NULL, 8, 8, 1, A);
        }

        for (j = 1; j < 1000; j++) {
            SB_PORT_MEM_STREAM(D, 8, 8, 50, NULL);
            SB_PORT_MEM_STREAM(D, 8, 8, 50, NULL);

            if (c3) {
                SB_PORT_MEM_STREAM(D, 8, 8, 50, NULL);
            }
        }
    }
    else {
        if (c1) {
            SB_PORT_MEM_STREAM(C, 8, 8, 1, NULL);
        }
    }

    SB_WAIT();

    return 0;
}