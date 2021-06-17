#ifndef __WAPH_DATA_ANALYSIS_H__
#define __WAPH_DATA_ANALYSIS_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

/******************************************************************************/
/* pole_zero_correction(): compensate the exponential decay                   */
/*                                                                            */
/*  Input:  samples[]: array with all the samples                             */
/*          N:         the samples number                                     */
/*          decay_time:the signal slow decay time                             */
/*                                                                            */
/*  Output: filtered_samples[]: the array containing the output               */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the filter                 */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int pole_zero_correction(const uint16_t *samples, int N, unsigned int decay_time, int pulse_polarity, double **filtered_samples)
{
    if (!samples || !filtered_samples)
    {
        return EXIT_FAILURE;
    }

    if (!(*filtered_samples))
    {
        return EXIT_FAILURE;
    }

    const double factor = exp(-1.0 / decay_time);

    const uint16_t *x = samples;
    double *y = (*filtered_samples);

    y[0] = 0;

    if (pulse_polarity == POLARITY_POSITIVE)
    {
        // Loop through all the samples
        for (int n = 0; n < N; ++n)
        {
            const double prev_y = (n - 1) >= 0 ? y[n - 1] : y[0];
            const double x_n = x[n];
            const double x_n_minus_one = (n - 1) >= 0 ? x[n - 1] : x[0];

            const double this_y = prev_y + (x_n - x_n_minus_one * factor);

            y[n] = this_y;
        }
    }
    else
    {
        // Loop through all the samples
        for (int n = 0; n < N; ++n)
        {
            const double prev_y = (n - 1) >= 0 ? y[n - 1] : y[0];
            const double x_n = x[n];
            const double x_n_minus_one = (n - 1) >= 0 ? x[n - 1] : x[0];

            const double this_y = prev_y + ((INT16_MAX - x_n) - (INT16_MAX - x_n_minus_one) * factor);

            y[n] = this_y;
        }
    }

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* trapezoidal_filter(): applied a trapezoidal filter to the given samples    */
/*                                                                            */
/*  Input:  samples[]: array with all the samples                             */
/*          N:         the samples number                                     */
/*          risetime:  trapezoidal risetime                                   */
/*          flattop:   trapezoidal flattop                                    */
/*                                                                            */
/*  Output: filtered_samples[]: the array containing the output               */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the filter                 */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int trapezoidal_filter(const double *samples, int N, unsigned int risetime, unsigned int flattop, unsigned int pulse_polarity, double **filtered_samples)
{
    if (!samples || !filtered_samples)
    {
        return EXIT_FAILURE;
    }

    if (!(*filtered_samples))
    {
        return EXIT_FAILURE;
    }

    const int k = risetime;
    const int l = risetime + flattop;

    const double *x = samples;
    double *y = (*filtered_samples);

    y[0] = 0;

    if (pulse_polarity == POLARITY_POSITIVE)
    {
        // Loop through all the samples
        for (int n = 0; n < N; ++n)
        {
            const double prev_y = (n - 1) >= 0 ? y[n - 1] : y[0];
            const double x_n = x[n];
            const double x_n_minus_k = (n - k) >= 0 ? x[n - k] : x[0];
            const double x_n_minus_l = (n - l) >= 0 ? x[n - l] : x[0];
            const double x_n_minus_kl = (n - k - l) >= 0 ? x[n - k - l] : x[0];

            const double this_y = prev_y + (x_n - x_n_minus_k) - (x_n_minus_l - x_n_minus_kl);

            y[n] = this_y;
        }
    }
    else
    {
        // Loop through all the samples
        for (int n = 0; n < N; ++n)
        {
            const double prev_y = (n - 1) >= 0 ? y[n - 1] : y[0];
            const double x_n = x[n];
            const double x_n_minus_k = (n - k) >= 0 ? x[n - k] : x[0];
            const double x_n_minus_l = (n - l) >= 0 ? x[n - l] : x[0];
            const double x_n_minus_kl = (n - k - l) >= 0 ? x[n - k - l] : x[0];

            const double this_y = prev_y + (- x_n + x_n_minus_k) - (- x_n_minus_l + x_n_minus_kl);

            y[n] = this_y;
        }
    }

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* sum(): calculates the sum of the given samples                             */
/*                                                                            */
/*  Input:  samples[]: array with all the samples                             */
/*          N:         the samples number                                     */
/*                                                                            */
/*  Output: sum: the sum up to the end of the array                           */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int sum(const double *samples, size_t N, double *sum)
{
    if (!samples || !sum)
    {
        return EXIT_FAILURE;
    }

    *sum = 0;

    // Loop through all the samples
    for (size_t i = 0; i < N; ++i)
    {
        *sum += samples[i];
    }
    
    return EXIT_SUCCESS;
}

#endif
