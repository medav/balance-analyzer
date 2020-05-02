
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

    // Note: These are "dynamic" (I.e. runtime determined)
    const int ni = 100; // # of input neurons
    const int nn = 100; // # of output neurons

    uint64_t in[ni];
    uint64_t weights[nn][ni];
    uint64_t out[nn];

    // Initialize in[], and weights[][] here...

    SB_CONFIG();

    for (int n = 0; n < nn; n++) {
        SB_MEM_PORT_STREAM(weights[n], WORD_SIZE, WORD_SIZE, ni, B);

        // The reset port should get all zeros except for the last one. On the
        // last element we reset the accumulator by supplying a "1"
        SB_CONSTANT(reset, 0, ni - 1);
        SB_CONSTANT(reset, 1, 1);

        // All outputs are discarded except the very last one which contains the
        // final sum. This final value is written to &out.
        SB_DISCARD(C, ni - 1);
        SB_PORT_MEM_STREAM(C, WORD_SIZE, WORD_SIZE, 1, &out[n]);
    }

    SB_WAIT();

    return 0;
}