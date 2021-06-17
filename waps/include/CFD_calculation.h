#ifndef __CFD_CALCULATION_H__
#define __CFD_CALCULATION_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

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

        (*monitor_samples)[i] = fraction * delayed_sample - (double)samples[i];
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
inline extern int find_zero_crossing(const double *samples, size_t L, size_t R, unsigned int *zero_crossing_index)
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

#endif
