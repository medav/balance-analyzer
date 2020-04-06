
#include "softbrain.h"

// DFG:
// Input: A, B, reset
// Output: C
//
// T = A * B
// B = Acc(A, B, reset)
//

#define WORD_SIZE 8
#define ARR_SIZE 100

int main() {
    // Dummy values to just make this compilable
    const int A = 1;
    const int B = 2;
    const int reset = 3;
    const int C = 4;

    // Data is fed into the CGRA in 8-byte "words"
    uint64_t arr1[ARR_SIZE] = {1};
    uint64_t arr2[ARR_SIZE] = {2};
    uint64_t out = 0;

    SB_CONFIG();

    // Push all ARR_SIZE elements of arr1 and arr2 to ports A and B,
    // respectively.
    SB_MEM_PORT_STREAM(arr1, WORD_SIZE, WORD_SIZE, ARR_SIZE, A);
    SB_MEM_PORT_STREAM(arr2, WORD_SIZE, WORD_SIZE, ARR_SIZE, B);

    // The reset port should get all zeros except for the last one. On the last
    // element we reset the accumulator by supplying a "1"
    SB_CONSTANT(reset, 0, ARR_SIZE - 1);
    SB_CONSTANT(reset, 1, 1);

    // All outputs are discarded except the very last one which contains the
    // final sum. This final value is written to &out.
    SB_DISCARD(C, ARR_SIZE - 1);
    SB_PORT_MEM_STREAM(C, WORD_SIZE, WORD_SIZE, 1, &out);

    SB_WAIT();

    return 0;
}