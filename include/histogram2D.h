#ifndef __HISTOGRAM2D_H__
#define __HISTOGRAM2D_H__

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
struct histogram2D_t
{
    unsigned int verbosity;

    unsigned int bins_x;
    data_type min_x;
    data_type max_x;
    unsigned int bins_y;
    data_type min_y;
    data_type max_y;

    double bin_width_x;
    double bin_width_y;

    counter_type *histo;
};

enum histogram2D_error_t
{
    HISTOGRAM2D_OK,
    HISTOGRAM2D_ERROR_MALLOC = -1,
    HISTOGRAM2D_ERROR_EMPTY_HISTO = -2,
    HISTOGRAM2D_ERROR_EMPTY_HISTO_ARRAY = -3,
    HISTOGRAM2D_ERROR_EMPTY_CONFIG = -4,
    HISTOGRAM2D_ERROR_JSON_MALLOC = -5,
    HISTOGRAM2D_ERROR_DIFFERENT_SIZE = -6,
    HISTOGRAM2D_ERROR_NULL_WIDTH = -7,
    HISTOGRAM2D_ERROR_GENERIC = -99
};

typedef struct histogram2D_t histogram2D_t;
typedef enum histogram2D_error_t histogram2D_error_t;

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Histogram creation and destruction                                         //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
extern inline histogram2D_t *histogram2D_create(unsigned int bins_x,
                                            data_type min_x,
                                            data_type max_x,
                                            unsigned int bins_y,
                                            data_type min_y,
                                            data_type max_y,
                                            unsigned int verbosity)
{
    histogram2D_t *new_histo = (histogram2D_t*)malloc(sizeof(histogram2D_t));

    if (new_histo == NULL)
    {
        return NULL;
    }
    else
    {
        new_histo->verbosity = verbosity;
        new_histo->bins_x = bins_x;
        new_histo->min_x = min_x;
        new_histo->max_x = max_x;
        new_histo->bin_width_x = (max_x - min_x) / bins_x;
        new_histo->bins_y = bins_y;
        new_histo->min_y = min_y;
        new_histo->max_y = max_y;
        new_histo->bin_width_y = (max_y - min_y) / bins_y;

        new_histo->histo = (counter_type*)calloc(sizeof(counter_type), bins_x * bins_y);

        if (new_histo->histo == NULL)
        {
            free(new_histo);

            return NULL;
        }
    }

    return new_histo;
}

extern inline histogram2D_error_t histogram2D_destroy(histogram2D_t *histo)
{
    if (histo == NULL) {
        return HISTOGRAM2D_ERROR_EMPTY_HISTO;
    }
    if (histo->histo == NULL) {
        return HISTOGRAM2D_ERROR_EMPTY_HISTO_ARRAY;
    }

    free(histo->histo);
    free(histo);

    return HISTOGRAM2D_OK;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Parameters calculations and retrieval                                      //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
extern inline unsigned int histogram2D_get_index(const histogram2D_t *histo,
                                                 unsigned int i_x,
                                                 unsigned int i_y)
{
    return i_x + histo->bins_x * i_y;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// Operations on the histogram2Ds                                               //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
extern inline histogram2D_error_t histogram2D_reset(histogram2D_t *histo)
{
    if (histo == NULL) {
        return HISTOGRAM2D_ERROR_EMPTY_HISTO;
    }
    if (histo->histo == NULL) {
        return HISTOGRAM2D_ERROR_EMPTY_HISTO_ARRAY;
    }

    if (histo->verbosity > 0)
    {
        printf("histogram2D_reset()\n");
    }

    memset(histo->histo, 0, sizeof(counter_type) * histo->bins_x * histo->bins_y);

    return HISTOGRAM2D_OK;
}

extern inline histogram2D_error_t histogram2D_fill(histogram2D_t *histo,
                                                   data_type value_x,
                                                   data_type value_y)
{
    if (histo == NULL) {
        return HISTOGRAM2D_ERROR_EMPTY_HISTO;
    }
    if (histo->histo == NULL) {
        return HISTOGRAM2D_ERROR_EMPTY_HISTO_ARRAY;
    }

    const data_type offset_value_x = value_x - histo->min_x;
    const data_type offset_value_y = value_y - histo->min_y;

    const double norm_value_x = (double)(offset_value_x / histo->bin_width_x);
    const long int bin_x = floor(norm_value_x);
    const double norm_value_y = (double)(offset_value_y / histo->bin_width_y);
    const long int bin_y = floor(norm_value_y);

    if (0 <= bin_x && bin_x < histo->bins_x && 0 <= bin_y && bin_y < histo->bins_y)
    {
        const unsigned int index = histogram2D_get_index(histo, bin_x, bin_y);
        histo->histo[index] += 1;
    }

    if (histo->verbosity > 1)
    {
        printf("histogram2D_fill(): value_x: %f, bin_x: %li, value_y: %f, bin_y: %li\n", value_x,
                                                                                         bin_x,
                                                                                         value_y,
                                                                                         bin_y);
    }

    return HISTOGRAM2D_OK;
}

////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// JSON interface                                                             //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////
extern inline histogram2D_error_t histogram2D_reconfigure_json(histogram2D_t *histo, json_t *new_config)
{
    if (histo == NULL) {
        return HISTOGRAM2D_ERROR_EMPTY_HISTO;
    }
    if (histo->histo == NULL) {
        return HISTOGRAM2D_ERROR_EMPTY_HISTO_ARRAY;
    }
    if (new_config == NULL) {
        return HISTOGRAM2D_ERROR_EMPTY_CONFIG;
    }

    if (histo->verbosity > 0)
    {
        printf("histogram2D_reconfigure_json()\n");
    }

    json_t *json_verbosity = json_object_get(new_config, "verbosity");
    if (json_verbosity != NULL && json_is_integer(json_verbosity))
    {
        histo->verbosity = json_integer_value(json_verbosity);

        if (histo->verbosity > 0)
        {
            printf("histogram2D_reconfigure_json(): verbosity: %d\n", histo->verbosity);
        }
    }

    json_t *json_bins_x = json_object_get(new_config, "bins_x");
    if (json_bins_x != NULL && json_is_integer(json_bins_x))
    {
        histo->bins_x = json_integer_value(json_bins_x);

        if (histo->verbosity > 0)
        {
            printf("histogram2D_reconfigure_json(): bins_x: %d\n", histo->bins_x);
        }
    }

    json_t *json_min_x = json_object_get(new_config, "min_x");
    if (json_min_x != NULL && json_is_number(json_min_x))
    {
        histo->min_x = json_number_value(json_min_x);

        if (histo->verbosity > 0)
        {
            printf("histogram2D_reconfigure_json(): min_x: %f\n", histo->min_x);
        }
    }

    json_t *json_max_x = json_object_get(new_config, "max_x");
    if (json_max_x != NULL && json_is_integer(json_max_x))
    {
        histo->max_x = json_number_value(json_max_x);

        if (histo->verbosity > 0)
        {
            printf("histogram2D_reconfigure_json(): max_x: %f\n", histo->max_x);
        }
    }

    json_t *json_bins_y = json_object_get(new_config, "bins_y");
    if (json_bins_y != NULL && json_is_integer(json_bins_y))
    {
        histo->bins_y = json_integer_value(json_bins_y);

        if (histo->verbosity > 0)
        {
            printf("histogram2D_reconfigure_json(): bins_y: %d\n", histo->bins_y);
        }
    }

    json_t *json_min_y = json_object_get(new_config, "min_y");
    if (json_min_y != NULL && json_is_number(json_min_y))
    {
        histo->min_y = json_number_value(json_min_y);

        if (histo->verbosity > 0)
        {
            printf("histogram2D_reconfigure_json(): min_y: %f\n", histo->min_y);
        }
    }

    json_t *json_max_y = json_object_get(new_config, "max_y");
    if (json_max_y != NULL && json_is_number(json_max_y))
    {
        histo->max_y = json_number_value(json_max_y);

        if (histo->verbosity > 0)
        {
            printf("histogram2D_reconfigure_json(): max_y: %f\n", histo->max_y);
        }
    }

    histo->bin_width_x = (histo->max_x - histo->min_x) / histo->bins_x;
    histo->bin_width_y = (histo->max_y - histo->min_y) / histo->bins_y;

    free(histo->histo);

    histo->histo = (counter_type*)calloc(sizeof(counter_type), histo->bins_x * histo->bins_y);

    if (histo->histo == NULL)
    {
        return HISTOGRAM2D_ERROR_MALLOC;
    }

    return HISTOGRAM2D_OK;
}

extern inline json_t *histogram2D_config_to_json(histogram2D_t *histo)
{
    json_t *json_config = json_object();
    if (json_config == NULL)
    {
        return NULL;
    }

    json_object_set_new_nocheck(json_config, "verbosity", json_integer(histo->verbosity));
    json_object_set_new_nocheck(json_config, "bins_x", json_integer(histo->bins_x));
    json_object_set_new_nocheck(json_config, "min_x", json_real(histo->min_x));
    json_object_set_new_nocheck(json_config, "max_x", json_real(histo->max_x));
    json_object_set_new_nocheck(json_config, "bins_y", json_integer(histo->bins_y));
    json_object_set_new_nocheck(json_config, "min_y", json_real(histo->min_y));
    json_object_set_new_nocheck(json_config, "max_y", json_real(histo->max_y));

    return json_config;
}

extern inline json_t *histogram2D_data_to_json(histogram2D_t *histo)
{
    json_t *json_histo = json_array();
    if (json_histo == NULL)
    {
        return NULL;
    }

    for (unsigned int i = 0; i < (histo->bins_x * histo->bins_y); i++)
    {
        json_array_append_new(json_histo, json_real(histo->histo[i]));
    }

    return json_histo;
}

extern inline json_t *histogram2D_to_json(histogram2D_t *histo)
{
    json_t *json_histo = json_object();
    if (json_histo == NULL)
    {
        return NULL;
    }

    json_t *json_histo_config = histogram2D_config_to_json(histo);
    json_t *json_histo_histo = histogram2D_data_to_json(histo);

    json_object_set_new_nocheck(json_histo, "data", json_histo_histo);
    json_object_set_new_nocheck(json_histo, "config", json_histo_config);

    return json_histo;
}

#endif
