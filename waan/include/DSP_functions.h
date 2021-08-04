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

/*! \brief Function that converts the integer samples to doubles.
 *
 * \param[in] samples an array with the input samples.
 * \param[in] samples_number the number of samples in the array.
 *
 * \param[out] double_samples a pointer to an allocated double array with n samples.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int to_double(const uint16_t *samples, size_t samples_number, \
                            double **double_samples)
{
    if (!samples || !double_samples)
    {
        return EXIT_FAILURE;
    }
    if (!(*double_samples))
    {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < samples_number; i++) {
        (*double_samples)[i] = samples[i];
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that determines the extrema of an array.
 *
 * \param[in] samples an array with the input samples.
 * \param[in] start starting index for the search (included).
 * \param[in] end ending index for the search (not included).
 *
 * \param[out] index_min the index of the minimum of the samples.
 * \param[out] index_max the index of the maximum of the samples.
 * \param[out] min the minimum of the samples.
 * \param[out] max the maximum of the samples.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
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

    for (size_t i = start; i < end; ++i)
    {
        if ((*minimum) > samples[i])
        {
            (*minimum) = samples[i];
            (*index_min) = i;
        }
        if ((*maximum) < samples[i])
        {
            (*maximum) = samples[i];
            (*index_max) = i;
        }
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that calculates the sum of all the given samples.
 *
 * \param[in] samples an array with the input samples.
 * \param[in] start starting index for the search (included).
 * \param[in] end ending index for the search (not included).
 *
 * \param[out] sum the resulting sum.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int calculate_sum(const double *samples, size_t start, size_t end, \
                                double *sum)
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

/*! \brief Function that calculates the average of the given samples.
 *
 * \param[in] samples an array with the input samples.
 * \param[in] start starting index for the search (included).
 * \param[in] end ending index for the search (not included).
 *
 * \param[out] average the resulting average.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
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

/*! \brief Function that adds a constant to the samples and multiplies them by another constant.
 *
 * \param[in] samples an array with the input samples.
 * \param[in] samples_number the number of samples in the array.
 * \param[in] adding the addition constant.
 * \param[in] multiplying the multiplication constant.
 *
 * \param[out] output_samples a pointer to an allocated double array with n samples.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int add_and_multiply_constant(const double *samples, size_t samples_number, \
                                            double adding, double multiplying,
                                            double **output_samples)
{
    if (!samples || !output_samples)
    {
        return EXIT_FAILURE;
    }
    if (!(*output_samples))
    {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < samples_number; i++) {
        (*output_samples)[i] = multiplying * (samples[i] + adding);
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that applies a recursive running mean, as described in the DSP book, chapter 15, eq. (15-3)
 *
 * \param[in] samples an array with the input samples.
 * \param[in] samples_number the number of samples in the array.
 * \param[in] smooth_samples the number of samples of the averaging window.
 *
 * \param[out] smoothed_samples a pointer to an allocated double array with n samples.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int running_mean(const uint16_t *samples, size_t samples_number, \
                               unsigned int smooth_samples, \
                               double **smoothed_samples)
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

    for (uint32_t i = (P + 1); i < (samples_number - P); i++) {
        (*smoothed_samples)[i] = (*smoothed_samples)[i - 1]
                                 + (((int32_t)samples[i + P]) - ((int32_t)samples[i - (P + 1)])) / M;
    }

    // The last (P + 1) numbers will be all identical.
    for (uint32_t i = (samples_number - P); i < samples_number; i++) {
        (*smoothed_samples)[i] = (*smoothed_samples)[samples_number - P - 1];
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that calculates the CFD signal also known as monitor signal.
 *
 * \param[in] samples an array with the input samples.
 * \param[in] samples_number the number of samples in the array.
 * \param[in] delay the integer number of steps that the signal will be delayed.
 * \param[in] fraction the multiplication factor for the delayed signal.
 *
 * \param[out] monitor_samples a pointer to an allocated double array with n samples.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int CFD_signal(const double *samples, size_t samples_number, \
                             int delay, double fraction, \
                             double **monitor_samples)
{
    if (!samples || !monitor_samples)
    {
        return EXIT_FAILURE;
    }
    if (!(*monitor_samples))
    {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < samples_number; ++i)
    {
        const int new_i = i - delay;

        double delayed_sample;

        if (new_i < 0)
        {
            delayed_sample = samples[0];
        }
        else if (new_i >= (int)samples_number)
        {
            delayed_sample = samples[samples_number - 1];
        }
        else
        {
            delayed_sample = samples[new_i];
        }

        (*monitor_samples)[i] = fraction * samples[i] - delayed_sample;
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that determines the zero crossing index using the bisection method, assuming that L < R.
 *
 * \param[in] samples an array with the input samples.
 * \param[in] L starting index for the search (included).
 * \param[in] R ending index for the search (included).
 *
 * \param[out] zero_crossing_index the index of the zero crossing.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int find_zero_crossing(const double *samples, size_t L, size_t R, \
                                     size_t *zero_crossing_index)
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

/*! \brief Function that linearly interpolates the samples around the zero crossing index.
 *
 * \param[in] samples an array with the input samples.
 * \param[in] samples_number the number of samples in the array.
 * \param[in] zero_crossing_index the index of the zero crossing.
 * \param[in] zero_crossing_samples the number of points to use in the interpolation.
 *
 * \param[out] the position of the zero crossing with better resolution.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int find_fine_zero_crossing(const double *samples, size_t samples_number, \
                                          unsigned int zero_crossing_index, unsigned int zero_crossing_samples, \
                                          double *fine_zero_crossing)
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

    if (((int)zero_crossing_index - (int)half_W) < 0 || (zero_crossing_index + half_W +1) > samples_number)
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

/*! \brief Function that calculates the cumulative sum of all the samples.
 *
 * \param[in] samples an array with the input samples.
 * \param[in] samples_number the number of samples in the array.
 *
 * \param[out] integral_samples a pointer to an allocated uint64_t array with n samples.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int cumulative_sum(const uint16_t *samples, size_t samples_number, \
                                 uint64_t **integral_samples)
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

    for (size_t i = 0; i < samples_number; ++i)
    {
        total_sum += (uint64_t)samples[i];
        (*integral_samples)[i] = total_sum;
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that subtracts the baseline from the integral of the pulse.
 *
 * \param[in] integral_samples an array with the integral of the input samples.
 * \param[in] samples_number the number of samples in the array.
 * \param[in] baseline the baseline to be subtracted.
 *
 * \param[out] integral_curve the integral of the input signal, baseline subtracted.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int integral_baseline_subtract(const uint64_t *integral_samples, size_t samples_number, \
                                             double baseline, \
                                             double **integral_curve)
{
    if (!integral_samples || !integral_curve)
    {
        return EXIT_FAILURE;
    }

    if (!(*integral_curve))
    {
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < samples_number; ++i)
    {
        // We add one to 'i' otherwise the first bin would not be subtracted
        (*integral_curve)[i] = (double)integral_samples[i] - (i + 1) * baseline;
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that compensates the exponential decay of a pulse.
 *
 * \param[in] samples an array with the input samples, the baseline shall already be subtracted.
 * \param[in] samples_number the number of samples in the array.
 * \param[in] decay_time the decay time of the exponential, in terms of clock steps.
 *
 * \param[out] filtered_samples the resulting pulse.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int decay_compensation(const double *samples, int samples_number, \
                                     double decay_time, \
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

    // Just renaming arrays
    const double *x = samples;
    double *y = (*filtered_samples);

    y[0] = 0;

    // Loop through all the samples
    for (int i = 0; i < samples_number; ++i)
    {
        const double y_i_minus_one = (i - 1) >= 0 ? y[i - 1] : y[0];
        const double x_i = x[i];
        const double x_i_minus_one = (i - 1) >= 0 ? x[i - 1] : x[0];

        y[i] = y_i_minus_one + 2.0 / (1.0 + factor) * (x_i - factor * x_i_minus_one);
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that applies a trapezoidal filter
 *
 * \param[in] samples an array with the input samples, the baseline shall already be subtracted.
 * \param[in] samples_number the number of samples in the array.
 * \param[in] risetime the risetime of the trapezoid in terms of clock samples.
 * \param[in] flattop the width of the top of the trapezoid.
 *
 * \param[out] filtered_samples the resulting pulse.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int trapezoidal_filter(const double *samples, int samples_number, \
                                     unsigned int risetime, unsigned int flattop, \
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

    const int K = risetime;
    const int L = risetime + flattop;

    const double *x = samples;
    double *y = (*filtered_samples);

    y[0] = 0;

    // Loop through all the samples
    for (int i = 0; i < samples_number; ++i)
    {
        //const double y_i_minus_one = (i - 1) >= 0 ? y[i - 1] : y[0];
        //const double x_i = x[i];
        //const double x_i_minus_K = (i - K) >= 0 ? x[i - K] : x[0];
        //const double x_i_minus_L = (i - L) >= 0 ? x[i - L] : x[0];
        //const double x_i_minus_KL =(i - K - L) >= 0 ? x[i - K - L] : x[0];
        const double y_i_minus_one = (i - 1) >= 0 ? y[i - 1] : 0;
        const double x_i = x[i];
        const double x_i_minus_K = (i - K) >= 0 ? x[i - K] : 0;
        const double x_i_minus_L = (i - L) >= 0 ? x[i - L] : 0;
        const double x_i_minus_KL =(i - K - L) >= 0 ? x[i - K - L] : 0;

        y[i] = y_i_minus_one + (x_i - x_i_minus_K) - (x_i_minus_L - x_i_minus_KL);
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that applies a single pole high-pass filter (CR filter)
 *
 * \param[in] samples an array with the input samples, the baseline shall already be subtracted.
 * \param[in] samples_number the number of samples in the array.
 * \param[in] decay_time the decay time of the filter.
 *
 * \param[out] filtered_samples the resulting pulse.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int CR_filter(const double *samples, int samples_number, \
                            double decay_time, \
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

    const double factor = exp(-1 / decay_time);

    const double a0 = (1.0 + factor) / 2.0;
    const double a1 = -(1.0 + factor) / 2.0;
    const double b1 = factor;

    const double *x = samples;
    double *y = (*filtered_samples);

    y[0] = 0;

    // Loop through all the samples
    for (int i = 0; i < samples_number; ++i)
    {
        const double x_i = x[i];
        const double x_i_minus_one = (i - 1) >= 0 ? x[i - 1] : x[0];
        const double y_i_minus_one = (i - 1) >= 0 ? y[i - 1] : y[0];

        y[i] = a0 * x_i + a1 * x_i_minus_one + b1 * y_i_minus_one;
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that applies a single pole low-pass filters (RC filter)
 *
 * \param[in] samples an array with the input samples, the baseline shall already be subtracted.
 * \param[in] samples_number the number of samples in the array.
 * \param[in] decay_time the decay time of the filter.
 *
 * \param[out] filtered_samples the resulting pulse.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int RC_filter(const double *samples, int samples_number, \
                            double decay_time, \
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

    const double factor = exp(-1 / decay_time);

    const double a0 = (1.0 - factor);
    const double b1 = factor;

    const double *x = samples;
    double *y = (*filtered_samples);

    y[0] = 0;

    // Loop through all the samples
    for (int i = 0; i < samples_number; ++i)
    {
        const double x_i = x[i];
        const double y_i_minus_one = (i - 1) >= 0 ? y[i - 1] : y[0];

        y[i] = a0 * x_i + b1 * y_i_minus_one;
    }

    return EXIT_SUCCESS;
}

/*! \brief Function that applies four single pole low-pass filters (RC^4 filter)
 *
 * \param[in] samples an array with the input samples, the baseline shall already be subtracted.
 * \param[in] samples_number the number of samples in the array.
 * \param[in] decay_time the decay time of the filter.
 *
 * \param[out] filtered_samples the resulting pulse.
 *
 * \return EXIT_SUCCESS if it was able to find the extrema, EXIT_FAILURE otherwise.
 */
inline extern int RC4_filter(const double *samples, int samples_number, \
                             double decay_time, \
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

    const double factor = exp(-1 / decay_time);

    const double a0 = (1.0 - factor) * (1.0 - factor) * (1.0 - factor) * (1.0 - factor);
    const double b1 =  4. * factor;
    const double b2 = -6. * factor * factor;
    const double b3 =  4. * factor * factor * factor;
    const double b4 = -1. * factor * factor * factor * factor;

    const double *x = samples;
    double *y = (*filtered_samples);

    y[0] = 0;

    // Loop through all the samples
    for (int i = 0; i < samples_number; ++i)
    {
        const double x_i = x[i];
        const double y_i_minus_one = (i - 1) >= 0 ? y[i - 1] : y[0];
        const double y_i_minus_two = (i - 2) >= 0 ? y[i - 2] : y[0];
        const double y_i_minus_three = (i - 3) >= 0 ? y[i - 3] : y[0];
        const double y_i_minus_four = (i - 4) >= 0 ? y[i - 4] : y[0];

        y[i] = a0 * x_i \
             + b1 * y_i_minus_one \
             + b2 * y_i_minus_two \
             + b3 * y_i_minus_three \
             + b4 * y_i_minus_four;
    }

    return EXIT_SUCCESS;
}

#endif
