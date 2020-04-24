
#include "softbrain.h"

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
    const int reset = 3;
    const int C = 4;

    int some_condition = 1;

    // Note: These are "dynamic" (I.e. runtime determined)
    const int BS = 100; // batch size
    const int ni = 100; // # of input neurons
    const int nn = 100; // # of output neurons

    uint64_t in[BS][ni];
    uint64_t weights[nn][ni];
    uint64_t out[BS][nn];

    // Initialize in[], and weights[][] here...

    SB_CONFIG();


    for (int b = 0; b < BS; b++) {
        for (int n = 0; n < nn; n++) {
            SB_MEM_PORT_STREAM(in[b], WORD_SIZE, WORD_SIZE, ni, A);
            SB_MEM_PORT_STREAM(weights[n], WORD_SIZE, WORD_SIZE, ni, B);

            // The reset port should get all zeros except for the last one. On the
            // last element we reset the accumulator by supplying a "1"
            SB_CONSTANT(reset, 0, ni - 1);
            SB_CONSTANT(reset, 1, 1);

            // All outputs are discarded except the very last one which contains the
            // final sum. This final value is written to &out.
            SB_DISCARD(C, ni - 1);
            SB_PORT_MEM_STREAM(C, WORD_SIZE, WORD_SIZE, 1, &out[b][n]);
        }

        if (some_condition) {
            SB_MEM_PORT_STREAM(in[0], WORD_SIZE, WORD_SIZE, ni, A);
        }
    }

    SB_WAIT();

    return 0;
}