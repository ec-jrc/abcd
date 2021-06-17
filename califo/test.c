////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// This test program fits a simulated peak using the functions from the GNU   //
// Scientific Library                                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#define counter_type uint64_t

#include "histogram.h"
#include "fit_functions.h"

int main()
{
    struct histogram_t *histo = histogram_create(1024, 123.4, 246.8, 1);

    const double min = histo->min;
    const double max = histo->max;
    const double bin_width = histo->bin_width;

    const double A = 3000;
    const double mu = (max + min) / 2;
    const double sigma = (max - min) / 8;
    const double B = 12400;
    const double alpha = 0.02;
    const double rand_width = A / 3;

    printf("# True values: A = %.4f, mu = %.4f, sigma = %.4f, B = %.4f, alpha = %.4f\n", A, mu, sigma, B, alpha);

    gsl_rng_env_setup();
    const gsl_rng_type *T = gsl_rng_default;
    gsl_rng *rng = gsl_rng_alloc(T);

    for (unsigned int i = 0; i < histo->bins; i++)
    {
        const double r = gsl_ran_gaussian(rng, rand_width);

        const double t = min + bin_width * i;
        const double val = peak(A, mu, sigma, B, alpha, t) + r;
        histo->histo[i] = (val >= 0) ? (counter_type)val : 0;
    }
 
    struct peak_parameters p0 = {A * 1.2, mu * 0.9, sigma * 1.1, B * 0.8, alpha * 2, 0};

    struct peak_parameters p = fit_peak(histo, p0);
    printf("# True values: A = %.4f, mu = %.4f, sigma = %.4f, B = %.4f, alpha = %.4f\n", A, mu, sigma, B, alpha);

    printf("# Values...\n");
    printf("# t\tvalues\tfit\n");

    for (unsigned int i = 0; i < histo->bins; i++)
    {
        const double t = min + bin_width * i;

        printf("%.2f\t%" PRIu64 "\t%f\n", t, histo->histo[i], peak(p.A, p.mu, p.sigma, p.B, p.alpha, t));
    }

    gsl_rng_free (rng);

    return (p.fit_result == GSL_SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
}
