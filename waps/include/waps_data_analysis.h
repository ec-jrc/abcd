#ifndef __WAPS_DATA_ANALYSIS_H__
#define __WAPS_DATA_ANALYSIS_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

/******************************************************************************/
/* cumulative_sum(): calculates the cumulative sum for the given samples      */
/*                                                                            */
/*  Input:  samples[]: array with all the samples                             */
/*          n:         the samples number                                     */
/*                                                                            */
/*  Output: samples_integral[]: the array containing the cumulative sum       */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int cumulative_sum(const uint16_t *samples, size_t n, uint64_t **samples_integral)
{
    if (!samples || !samples_integral)
    {
        return EXIT_FAILURE;
    }

    if (!(*samples_integral))
    {
        return EXIT_FAILURE;
    }

    int64_t total_sum = 0;

    for (size_t i = 0; i < n; ++i)
    {
        total_sum += (uint64_t)samples[i];
        (*samples_integral)[i] = total_sum;
    }

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* running_mean(): smooth the signal samples using the cumulative sum         */
/*                                                                            */
/*  Input:  samples_integral[]: array with all the samples integral, also     */
/*                              known as the cumulative sum                   */
/*          n:                  the samples number                            */
/*                                                                            */
/*  Output: smoothed_samples[]: the array containing the smoothed samples     */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int running_mean(const double *samples_integral, size_t n, unsigned int W, double **smoothed_samples)
{
    if (!samples_integral || !smoothed_samples)
    {
        return EXIT_FAILURE;
    }

    if (!(*smoothed_samples))
    {
        return EXIT_FAILURE;
    }

    const double W_d = floor(W / 2) * 2 + 1;
    const unsigned int half_W = floor(W / 2);
    const double half_W_d = half_W;

    for (size_t i = 0; i <= half_W; ++i)
    {
        const double new_sample = samples_integral[i + half_W] / (i + half_W_d + 1.0);
        (*smoothed_samples)[i] = new_sample;
    }
    for (size_t i = half_W + 1; i <= (n - half_W - 1); ++i)
    {
        const double new_sample = (samples_integral[i + half_W] - samples_integral[i - half_W - 1]) / W_d;
        (*smoothed_samples)[i] = new_sample;
    }
    for (size_t i = n - half_W; i < n; ++i)
    {
        const double new_sample = (samples_integral[n - 1] - samples_integral[i - half_W - 1]) / (n - i + half_W_d);
        (*smoothed_samples)[i] = new_sample;
    }

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* integral_baseline_subtrac(): correct the cumulative sum by subtracting the */
/*                              constant component of the baseline.           */
/*                                                                            */
/*  Input:  samples_integral[]: array with the cumulative sum                 */
/*          n:                  the samples number                            */
/*          baseline:           the constant component to be subtracted       */
/*                                                                            */
/*  Output: curve_integral[]:   the array containing the corrected integral   */
/*                                                                            */
/*  Return: EXIT_SUCCESS if it was able to compute the sum                    */
/*          EXIT_FAILURE otherwise                                            */
/*                                                                            */
/******************************************************************************/
inline extern int integral_baseline_subtract(const uint64_t *samples_integral, size_t n, double baseline, double **curve_integral)
{
    if (!samples_integral || !curve_integral)
    {
        return EXIT_FAILURE;
    }

    if (!(*curve_integral))
    {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < n; ++i)
    {
        // We add one to 'i' otherwise the first bin would not be subtracted
        (*curve_integral)[i] = (double)samples_integral[i] - (i + 1) * baseline;
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

#endif
