#ifndef __COMMON_ANALYSIS_H__
#define __COMMON_ANALYSIS_H__

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

enum pulse_polarity_t {
    POLARITY_NEGATIVE = -1,
    POLARITY_POSITIVE = 1
};

typedef enum pulse_polarity_t pulse_polarity_t;

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

#endif
