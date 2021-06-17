#ifndef __HISTOGRAM_H__
#define __HISTOGRAM_H__

#include <stdlib.h>
// For memset
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>

#include <jansson.h>

#ifndef data_type
#define data_type double
#endif

#ifndef counter_type
#define counter_type uint64_t
#endif

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Histogram declaration                                                      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
struct histogram_t
{
    unsigned int verbosity;

    unsigned int bins;
    data_type min;
    data_type max;

    double bin_width;

    counter_type *histo;
};

enum histogram_error_t
{
    HISTOGRAM_OK,
    HISTOGRAM_ERROR_MALLOC = -1,
    HISTOGRAM_ERROR_EMPTY_HISTO = -2,
    HISTOGRAM_ERROR_EMPTY_HISTO_ARRAY = -3,
    HISTOGRAM_ERROR_EMPTY_CONFIG = -4,
    HISTOGRAM_ERROR_JSON_MALLOC = -5,
    HISTOGRAM_ERROR_DIFFERENT_SIZE = -6,
    HISTOGRAM_ERROR_NULL_WIDTH = -7,
    HISTOGRAM_ERROR_GENERIC = -99
};

typedef struct histogram_t histogram_t;
typedef enum histogram_error_t histogram_error_t;

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Histogram creation and destruction                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
extern inline histogram_t *histogram_create(unsigned int bins,
                                            data_type min,
                                            data_type max,
                                            unsigned int verbosity)
{
    histogram_t *new_histo = (histogram_t*)malloc(sizeof(histogram_t));

    if (new_histo == NULL)
    {
        return NULL;
    }
    else
    {
        new_histo->verbosity = verbosity;
        new_histo->bins = bins;
        new_histo->min = min;
        new_histo->max = max;
        new_histo->bin_width = (max - min) / bins;

        new_histo->histo = (counter_type*)calloc(sizeof(counter_type), bins);

        if (new_histo->histo == NULL)
        {
            free(new_histo);

            return NULL;
        }
    }

    return new_histo;
}

extern inline histogram_error_t histogram_destroy(histogram_t *histo)
{
    if (histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO;
    }
    if (histo->histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO_ARRAY;
    }

    free(histo->histo);
    free(histo);

    return HISTOGRAM_OK;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Parameters calculations and retrieval                                      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
extern inline data_type histogram_get_mean(const histogram_t *histo)
{
    const unsigned int N = histo->bins;
    const data_type min = histo->min;
    const data_type bin_width = histo->bin_width;

    data_type mean = 0.0;
    counter_type sum = 0;

    for (unsigned int i = 0; i < N; i++)
    {
        const data_type x = min + bin_width * i;
        mean += x * histo->histo[i];
        sum += histo->histo[i];
    }

    mean /= sum;

    return mean;
}

extern inline data_type histogram_get_variance(const histogram_t *histo)
{
    const unsigned int N = histo->bins;
    const data_type min = histo->min;
    const data_type bin_width = histo->bin_width;

    data_type mean = 0.0;
    data_type sqr_mean = 0.0;
    counter_type sum = 0;

    for (unsigned int i = 0; i < N; i++)
    {
        const data_type x = min + bin_width * i;
        mean += x * histo->histo[i];
        sqr_mean += x * x * histo->histo[i];
        sum += histo->histo[i];
    }

    mean /= sum;
    sqr_mean /= sum;

    return sqr_mean  - mean * mean;
}

extern inline data_type histogram_get_stddev(const histogram_t *histo)
{
    return sqrt(histogram_get_variance(histo));
}

extern inline data_type histogram_get_integral(const histogram_t *histo)
{
    const data_type bin_width = histo->bin_width;

    counter_type sum = 0;

    for (unsigned int i = 0; i < histo->bins; i++)
    {
        sum += histo->histo[i];
    }

    return sum * bin_width;
}

extern inline counter_type histogram_get_max(const histogram_t *histo)
{
    counter_type max = histo->histo[0];

    for (unsigned int i = 1; i < histo->bins; i++)
    {
        if (max < histo->histo[i])
        {
            max = histo->histo[i];
        }
    }

    return max;
}

extern inline data_type histogram_get_mean_interval(const histogram_t *histo, data_type left_edge, data_type right_edge)
{
    const unsigned int N = histo->bins;
    const data_type min = histo->min;
    const data_type bin_width = histo->bin_width;

    data_type mean = 0.0;
    counter_type sum = 0;

    const data_type left_offset_value = left_edge - min;
    const data_type right_offset_value = right_edge - min;

    const double left_norm_value = (double)(left_offset_value / bin_width);
    const double right_norm_value = (double)(right_offset_value / bin_width);

    const long int left_edge_bin = (left_norm_value > 0) ? floor(left_norm_value) : 0;
    const long int right_edge_bin = floor(right_norm_value);

    for (unsigned int i = left_edge_bin; (i < right_edge_bin) && (i < N); i++)
    {
        const data_type x = min + bin_width * i;
        mean += x * histo->histo[i];
        sum += histo->histo[i];
    }

    mean /= sum;

    return mean;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Operations on the histograms                                               //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
extern inline histogram_error_t histogram_reset(histogram_t *histo)
{
    if (histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO;
    }
    if (histo->histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO_ARRAY;
    }

    if (histo->verbosity > 0)
    {
        printf("histogram_reset()\n");
    }

    memset(histo->histo, 0, sizeof(counter_type) * histo->bins);

    return HISTOGRAM_OK;
}

extern inline histogram_error_t histogram_fill(histogram_t *histo, data_type value)
{
    if (histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO;
    }
    if (histo->histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO_ARRAY;
    }

    const data_type offset_value = value - histo->min;

    const double norm_value = (double)(offset_value / histo->bin_width);
    const long int bin = floor(norm_value);

    if (0 <= bin && bin < histo->bins)
    {
        histo->histo[bin] += 1;
    }

    if (histo->verbosity > 1)
    {
        printf("histogram_fill(): value: %f, bin: %li\n", value, bin);
    }

    return HISTOGRAM_OK;
}

extern inline histogram_error_t histogram_add_to(histogram_t *output_histo, const histogram_t* input_histo)
{
    if (output_histo == NULL || input_histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO;
    }
    if (output_histo->histo == NULL || input_histo->histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO_ARRAY;
    }
    if (output_histo->bins != input_histo->bins) {
        return HISTOGRAM_ERROR_DIFFERENT_SIZE;
    }

    if (output_histo->verbosity > 0)
    {
        printf("histogram_add_to()\n");
    }

    for (unsigned int i = 0; i < output_histo->bins; i++)
    {
        output_histo->histo[i] += input_histo->histo[i];
    }

    return HISTOGRAM_OK;
}

extern inline histogram_error_t histogram_subtract_from(histogram_t *output_histo, const histogram_t* input_histo)
{
    if (output_histo == NULL || input_histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO;
    }
    if (output_histo->histo == NULL || input_histo->histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO_ARRAY;
    }
    if (output_histo->bins != input_histo->bins) {
        return HISTOGRAM_ERROR_DIFFERENT_SIZE;
    }

    if (output_histo->verbosity > 0)
    {
        printf("histogram_subtract_from()\n");
    }

    for (unsigned int i = 0; i < output_histo->bins; i++)
    {
        output_histo->histo[i] -= input_histo->histo[i];
    }

    return HISTOGRAM_OK;
}

//! The smoothing window is centered on each bin. The width shall be an odd number, if it is even
//! it will be transformed to the next odd number.
extern inline histogram_error_t histogram_box_smooth(histogram_t *histo, unsigned int width)
{
    if (histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO;
    }
    if (histo->histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO_ARRAY;
    }
    if (width == 0) {
        return HISTOGRAM_ERROR_NULL_WIDTH;
    }

    if (histo->verbosity > 0)
    {
        printf("histogram_smooth()\n");
    }

    const unsigned int half_width = width / 2;
    const unsigned int full_width = half_width * 2 + 1;
    const unsigned int bins = histo->bins;

    // Calculating the cumulative sum of the bins
    counter_type *cumulative_sum = (counter_type*)malloc(sizeof(counter_type) * (bins + 2 * half_width));

    if (!cumulative_sum) {
        return HISTOGRAM_ERROR_MALLOC;
    }

    counter_type total_sum = 0;

    for (unsigned int i = 0; i < half_width; i++)
    {
        total_sum += histo->histo[0];
        cumulative_sum[i] = total_sum;
    }

    for (unsigned int i = 0; i < bins; i++)
    {
        total_sum += histo->histo[i];
        cumulative_sum[i + half_width] = total_sum;
    }

    for (unsigned int i = 0; i < half_width; i++)
    {
        total_sum += histo->histo[bins - 1];
        cumulative_sum[i + half_width + bins] = total_sum;
    }

    // Calculating the average bins
    for (unsigned int i = 0; i < bins; i++)
    {
        histo->histo[i] = (cumulative_sum[i + half_width + half_width] -
                           cumulative_sum[i - half_width + half_width]) / full_width;
    }

    free(cumulative_sum);

    return HISTOGRAM_OK;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// JSON interface                                                             //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
extern inline histogram_error_t histogram_reconfigure_json(histogram_t *histo, json_t *new_config)
{
    if (histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO;
    }
    if (histo->histo == NULL) {
        return HISTOGRAM_ERROR_EMPTY_HISTO_ARRAY;
    }
    if (new_config == NULL) {
        return HISTOGRAM_ERROR_EMPTY_CONFIG;
    }

    if (histo->verbosity > 0)
    {
        printf("histogram_reconfigure_json()\n");
    }

    json_t *json_verbosity = json_object_get(new_config, "verbosity");
    if (json_verbosity != NULL && json_is_integer(json_verbosity))
    {
        histo->verbosity = json_integer_value(json_verbosity);

        if (histo->verbosity > 0)
        {
            printf("histogram_reconfigure_json(): verbosity: %d\n", histo->verbosity);
        }
    }

    json_t *json_bins = json_object_get(new_config, "bins");
    if (json_bins != NULL && json_is_integer(json_bins))
    {
        histo->bins = json_integer_value(json_bins);

        if (histo->verbosity > 0)
        {
            printf("histogram_reconfigure_json(): bins: %d\n", histo->bins);
        }
    }

    json_t *json_min = json_object_get(new_config, "min");
    if (json_min != NULL && json_is_number(json_min))
    {
        histo->min = json_number_value(json_min);

        if (histo->verbosity > 0)
        {
            printf("histogram_reconfigure_json(): min: %f\n", histo->min);
        }
    }

    json_t *json_max = json_object_get(new_config, "max");
    if (json_max != NULL && json_is_number(json_max))
    {
        histo->max = json_number_value(json_max);

        if (histo->verbosity > 0)
        {
            printf("histogram_reconfigure_json(): max: %f\n", histo->max);
        }
    }

    histo->bin_width = (histo->max - histo->min) / histo->bins;

    free(histo->histo);

    histo->histo = (counter_type*)calloc(sizeof(counter_type), histo->bins);

    if (histo->histo == NULL)
    {
        return HISTOGRAM_ERROR_MALLOC;
    }

    return HISTOGRAM_OK;
}

extern inline json_t *histogram_config_to_json(histogram_t *histo)
{
    json_t *json_config = json_object();
    if (json_config == NULL)
    {
        return NULL;
    }

    json_object_set_new_nocheck(json_config, "verbosity", json_integer(histo->verbosity));
    json_object_set_new_nocheck(json_config, "bins", json_integer(histo->bins));
    json_object_set_new_nocheck(json_config, "min", json_real(histo->min));
    json_object_set_new_nocheck(json_config, "max", json_real(histo->max));

    return json_config;
}

extern inline json_t *histogram_data_to_json(histogram_t *histo)
{
    json_t *json_histo = json_array();
    if (json_histo == NULL)
    {
        return NULL;
    }

    for (unsigned int i = 0; i < histo->bins; i++)
    {
        json_array_append_new(json_histo, json_real(histo->histo[i]));
    }

    return json_histo;
}

extern inline json_t *histogram_to_json(histogram_t *histo)
{
    json_t *json_histo = json_object();
    if (json_histo == NULL)
    {
        return NULL;
    }

    json_t *json_histo_config = histogram_config_to_json(histo);
    json_t *json_histo_histo = histogram_data_to_json(histo);

    json_object_set_new_nocheck(json_histo, "data", json_histo_histo);
    json_object_set_new_nocheck(json_histo, "config", json_histo_config);

    return json_histo;
}

#endif
