#ifndef __DSP_FUNCTIONS_H__
#define __DSP_FUNCTIONS_H__

#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <math.h>

enum pulse_polarity_t {
    POLARITY_NEGATIVE = -1,
    POLARITY_POSITIVE = 1
};

typedef enum pulse_polarity_t pulse_polarity_t;

inline extern int to_double(const uint16_t *samples, size_t n, double **double_samples)
{
    if (!samples || !double_samples)
    {
        return EXIT_FAILURE;
    }
    if (!(*double_samples))
    {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < n; i++) {
        (*double_samples)[i] = samples[i];
    }

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* find_extrema(): find the extrema of a series of samples                    */
/*                                                                            */
/*  Input:  samples[]: array with all the samples                             */
/*          start:     starting index for the search (included)               */
/*          end:       ending index for the search (not included)             */
/*                                                                            */
/*  Output: index_min: the index of the minimum of the samples                */
/*          index_max: the index of the maximum                               */
/*          minimum:   the minimum of the samples                             */
/*          maximum:   the maximum                                            */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to find the extrema                   */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int find_extrema(const double *samples, size_t start, size_t end, \
                               size_t *index_min, size_t *index_max, \
                               double *minimum,   double *maximum)
{
    if ((start > end) || !samples || !index_min || !index_max || !maximum || !minimum)
    {
        return EXIT_FAILURE;
    }

    (*index_min) = start;
    (*index_max) = start;
    (*minimum) = samples[start];
    (*maximum) = samples[start];

    for (size_t n = start; n < end; ++n)
    {
        if ((*minimum) > samples[n])
        {
            (*minimum) = samples[n];
            (*index_min) = n;
        }
        if ((*maximum) < samples[n])
        {
            (*maximum) = samples[n];
            (*index_max) = n;
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
inline extern int calculate_sum(const double *samples, size_t start, size_t end, double *sum)
{
    if ((start > end) || !samples || !sum)
    {
        return EXIT_FAILURE;
    }

    double accumulator = 0;

    for (size_t i = start; i < end; ++i)
    {
        accumulator += samples[i];
    }

    (*sum) = accumulator;
    
    return EXIT_SUCCESS;
}

inline extern int calculate_average(const double *samples, size_t start, size_t end, \
                                    double *average)
{
    if ((start > end) || !samples || !average)
    {
        return EXIT_FAILURE;
    }

    if (start == end)
    {
        (*average) = 0;
    } else {
        const size_t delta = end - start;

        double accumulator = 0;
        calculate_sum(samples, start, end, &accumulator);

        (*average) = accumulator / delta;
    }

    return EXIT_SUCCESS;
}

inline extern int add_constant(const double *samples, size_t n, \
                               double constant, double **added_samples)
{
    if (!samples || !added_samples)
    {
        return EXIT_FAILURE;
    }
    if (!(*added_samples))
    {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < n; i++) {
        (*added_samples)[i] = samples[i] + constant;
    }

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* running_mean():                                                            */
    // Recursive running mean as described in the DSP book, chapter 15, eq. (15-3)
/*                                                                            */
/*  Input:  samples[]: array with all the samples                             */
/*          n:         the samples number                                     */
/*          delay:     delay to be used for the shifted signal                */
/*          fraction:  fraction to be multiplied to the shifted signal        */
/*                                                                            */
/*  Output: monitor_samples[]: the array containing the resulting signal      */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int running_mean(const uint16_t *samples, size_t n, unsigned int smooth_samples, double **smoothed_samples)
{
    if (!samples || !smoothed_samples)
    {
        return EXIT_FAILURE;
    }
    if (!(*smoothed_samples))
    {
        return EXIT_FAILURE;
    }

    const double M = smooth_samples;
    const unsigned int P = (smooth_samples - 1) / 2;

    // Calculating the average of the first M numbers and then storing that on
    // the first P numbers.
    uint32_t accumulator = 0;
    for (uint32_t i = 0; i < smooth_samples; i++) {
        accumulator += samples[i];
    }

    const double beginning_average = accumulator / M;
    for (uint32_t i = 0; i < (P + 1); i++) {
        (*smoothed_samples)[i] = beginning_average;
    }

    for (uint32_t i = (P + 1); i < (n - P); i++) {
        (*smoothed_samples)[i] = (*smoothed_samples)[i - 1]
                                 + (((int32_t)samples[i + P]) - ((int32_t)samples[i - (P + 1)])) / M;
    }

    // The last (P + 1) numbers will be all identical.
    for (uint32_t i = (n - P); i < n; i++) {
        (*smoothed_samples)[i] = (*smoothed_samples)[n - P - 1];
    }

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* CFD_signal(): calculate the CFD signal also known as monitor signal        */
/*                                                                            */
/*  Input:  samples[]: array with all the samples                             */
/*          n:         the samples number                                     */
/*          delay:     delay to be used for the shifted signal                */
/*          fraction:  fraction to be multiplied to the shifted signal        */
/*                                                                            */
/*  Output: monitor_samples[]: the array containing the resulting signal      */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int CFD_signal(const double *samples, size_t n, int delay, double fraction, double **monitor_samples)
{
    if (!samples || !monitor_samples)
    {
        return EXIT_FAILURE;
    }
    if (!(*monitor_samples))
    {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < n; ++i)
    {
        const int new_i = i - delay;

        double delayed_sample;

        if (new_i < 0)
        {
            delayed_sample = samples[0];
        }
        else if (new_i >= (int)n)
        {
            delayed_sample = samples[n - 1];
        }
        else
        {
            delayed_sample = samples[new_i];
        }

        (*monitor_samples)[i] = fraction * delayed_sample - samples[i];
    }

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* find_zero_crossing(): determine the zero crossing index using the bisection*/
/*                       method, assuming that L < R.                         */
/*                                                                            */
/*  Input:  samples[]: array with all the samples                             */
/*          L:         left index for the search                              */
/*          R:         right index for the search                             */
/*                                                                            */
/*  Output: zero_crossing_index: the index of the zero crossing               */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int find_zero_crossing(const double *samples, size_t L, size_t R, size_t *zero_crossing_index)
{
    if (!samples || !zero_crossing_index)
    {
        return EXIT_FAILURE;
    }

    const size_t M = (R + L) / 2;
    const double mid_sample = samples[M];

    const size_t D = (R - L);

    if (mid_sample == 0 || D <= 1)
    {
        (*zero_crossing_index) = M;

        return EXIT_SUCCESS;
    }
    else
    {
        const double left_sample = samples[L];

        if (left_sample * mid_sample > 0)
        {
            find_zero_crossing(samples, M, R, zero_crossing_index);
        }
        else
        {
            find_zero_crossing(samples, L, M, zero_crossing_index);
        }
    }

    return EXIT_FAILURE;
}

/******************************************************************************/
/* find_fine_zero_crossing(): given the zero crossing interpolates around it  */
/*                                                                            */
/*  Input:  samples[]: array with all the samples                             */
/*          n:         the samples number                                     */
/*          zero_crossing_index:   the index of the zero crossing             */
/*          zero_crossing_samples: the number of samples for the interpolation*/
/*                                                                            */
/*  Output: fine_zero_crossing: the interpolated position of the zero crossing*/
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int find_fine_zero_crossing(const double *samples, size_t n, unsigned int zero_crossing_index, unsigned int zero_crossing_samples, double *fine_zero_crossing)
{
    if (!samples)
    {
        return EXIT_FAILURE;
    }

    if (zero_crossing_samples < 2)
    {
        *fine_zero_crossing = zero_crossing_index;

        return EXIT_SUCCESS;
    }

    unsigned int W = (zero_crossing_samples / 2) * 2 + 1;
    unsigned int half_W = W / 2;

    if (((int)zero_crossing_index - (int)half_W) < 0 || (zero_crossing_index + half_W +1) > n)
    {
        return EXIT_FAILURE;
    }

    unsigned int sum_x = 0;
    unsigned int sum_xx = 0;
    double sum_y = 0;
    double sum_xy = 0;
    unsigned int N = 0;

    for (size_t i = (zero_crossing_index - half_W); i < (zero_crossing_index + half_W + 1); ++i)
    {
        N += 1;
        sum_x += i;
        sum_xx += i * i;
        sum_y += samples[i];
        sum_xy += i * samples[i];
    }

    const double Delta = W * sum_xx - sum_x * sum_x;
    const double q = 1.0 / Delta * (sum_xx * sum_y - sum_x * sum_xy);
    const double m = 1.0 / Delta * (W * sum_xy - sum_x * sum_y);

    //printf("N: %u, sum_x: %u, sum_xx: %u, sum_xy: %f, sum_y: %f\n", N, sum_x, sum_xx, sum_xy, sum_y);
    //printf("Delta: %f, m: %f, q: %f\n", Delta, m, q);

    *fine_zero_crossing = - q / m;

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* cumulative_sum(): calculates the cumulative sum for the given samples      */
/*                                                                            */
/*  Input:  samples[]: array with all the samples                             */
/*          n:         the samples number                                     */
/*                                                                            */
/*  Output: integral_samples[]: the array containing the cumulative sum       */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int cumulative_sum(const uint16_t *samples, size_t n, uint64_t **integral_samples)
{
    if (!samples || !integral_samples)
    {
        return EXIT_FAILURE;
    }

    if (!(*integral_samples))
    {
        return EXIT_FAILURE;
    }

    int64_t total_sum = 0;

    for (size_t i = 0; i < n; ++i)
    {
        total_sum += (uint64_t)samples[i];
        (*integral_samples)[i] = total_sum;
    }

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* integral_baseline_subtrac(): correct the cumulative sum by subtracting the */
/*                              constant component of the baseline.           */
/*                                                                            */
/*  Input:  integral_samples[]: array with the cumulative sum                 */
/*          n:                  the samples number                            */
/*          baseline:           the constant component to be subtracted       */
/*                                                                            */
/*  Output: integral_curve[]:   the array containing the corrected integral   */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int integral_baseline_subtract(const uint64_t *integral_samples, size_t n, double baseline, double **integral_curve)
{
    if (!integral_samples || !integral_curve)
    {
        return EXIT_FAILURE;
    }

    if (!(*integral_curve))
    {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < n; ++i)
    {
        // We add one to 'i' otherwise the first bin would not be subtracted
        (*integral_curve)[i] = (double)integral_samples[i] - (i + 1) * baseline;
    }

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* calculate_var(): calculates the Root Mean Square of the signal samples.    */
/*                                                                            */
/*  Input:  samples[]: array with all the samples, with the baseline          */
/*          n:         the samples number                                     */
/*          baseline:  the signal baseline                                    */
/*                                                                            */
/*  Output: signal_var: the calculated variance.                              */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int calculate_var(const uint16_t *samples, size_t n, double baseline, double *signal_var)
{
    if (!samples || !signal_var)
    {
        return EXIT_FAILURE;
    }

    double sum = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double difference = (double)samples[i] - baseline;
        sum += difference * difference;
    }

    *signal_var = sum / (n - 1);

    return EXIT_SUCCESS;
}

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
inline extern int pole_zero_correction(const double *samples, int N, \
                                       double decay_time, int pulse_polarity, \
                                       double **filtered_samples)
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

#endif
