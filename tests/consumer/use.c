// A caller outside the tree, building against the installed library.
#include <ffitt/transform/fft.h>
#include <ffitt/filter/fir.h>
#include <ffitt/util/stats.h>
#include <stdio.h>

int main(void)
{
    real_t block[64];
    cnum_t spectrum[64];

    for(int i = 0; i < 64; i++) { block[i] = (real_t)((i % 8) - 4); }

    fft_t fft = fft_alloc(64);
    fft_forward_real(&fft, block, spectrum);

    fir_t fir = fir_alloc(33);

    if(!fir_design_low_pass(&fir, REAL_C(0.2)))
    {
        printf("the filter was refused\n");
        return 1;
    }

    real_t passing = fir_get_gain(&fir, REAL_C(0.05));
    real_t stopped = fir_get_gain(&fir, REAL_C(0.40));

    printf("width %zu bits, bin8 %.2f, passes %.3f, stops %.3f, rms %.3f\n",
           sizeof(real_t) * 8u, (double)cnum_magnitude(spectrum[8]),
           (double)passing, (double)stopped, (double)stats_rms(block, 64));

    fft_free(&fft);
    fir_free(&fir);

    // The filter must pass what is below its cutoff and stop what is above.
    // A library that installed wrong would fail this rather than print
    // something that merely looks like an answer.
    return ((passing > REAL_C(0.9)) && (stopped < REAL_C(0.1))) ? 0 : 1;
}
