/*! \brief Determination of the energy information from an exponentially
 *         decaying pulse, by compensating its decay and then applying a CR-RC4
 *         filter to the waveforms.
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the first N samples.
 *  2. The pulse is offset by the baseline to center it around zero.
 *  3. The decay is compensated with a recursive filter, obtaining a step
 *     function.
 *  4. A recursive high-pass filter (CR filter) is applied.
 *  5. A recursive low-pass filter (RC4 filter) is applied.
 *  6. The energy information is obtained by determining the absolute maximum
 *     of the resulting waveform.
 *
 * In the event_PSD structure the energy information is stored in the qlong,
 * while the qshort stores the value of the absolute maximum of the waveform
 * after the high-pass filter.
 *
 * The configuration parameters that are searched in a `json_t` object are:
 *
 * - `baseline_samples`: the number of samples to average to determine the
 *   baseline. The average starts from the beginning of the waveform.
 * - `pulse_polarity`: a string describing the expected pulse polarity, it
 *   can be `positive` or `negative`.
 * - `decay_time`: the pulse decay time in terms of clock samples, for the
 *   compensation.
 * - `highpass_time`: the decay time of the high-pass filter (CR filter), in
 *   terms of clock samples.
 * - `lowpass_time`: the decay time of the low-pass filter (RC4 filter), in
 *   terms of clock samples.
 * - `height_scaling`: a scaling factor multiplied to both the integrals.
 *   Optional, default value: 1
 * - `energy_threshold`: pulses with an energy lower than the threshold are
 *   discared. Optional, default value: 0
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "events.h"
#include "analysis_functions.h"
#include "DSP_functions.h"

/*! \brief Sctructure that holds the configuration for the `energy_analysis()` function.
 */
struct CRRC4_config
{
    uint32_t baseline_samples;
    double decay_time;
    double highpass_time;
    double lowpass_time;
    enum pulse_polarity_t pulse_polarity;
    double height_scaling;
    double energy_threshold;

    bool is_error;

    uint32_t previous_samples_number;

    double *curve_samples;
    double *curve_compensated;
    double *curve_offset;
    double *curve_CR;
    double *curve_RC;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct CRRC4_config **user_config);

/*! \brief Function that reads the json_t configuration for the `energy_analysis()` function.
 *
 * This function parses a JSON object determining the configuration for the
 * `energy_analysis()` function. The configuration is returned as an
 * allocated `struct CRRC4_config`.
 */
void energy_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config)) {
        printf("ERROR: libCRRC4 energy_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
        struct CRRC4_config *config = malloc(1 * sizeof(struct CRRC4_config));

        if (!config) {
            printf("ERROR: libCRRC4 energy_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }

        config->decay_time = json_integer_value(json_object_get(json_config, "decay_time"));
        config->baseline_samples = json_integer_value(json_object_get(json_config, "baseline_samples"));
        config->highpass_time = json_integer_value(json_object_get(json_config, "highpass_time"));
        config->lowpass_time = json_integer_value(json_object_get(json_config, "lowpass_time"));

        if (json_is_number(json_object_get(json_config, "height_scaling"))) {
            config->height_scaling = json_number_value(json_object_get(json_config, "height_scaling"));
        } else {
            config->height_scaling = 1;
        }

        if (json_is_number(json_object_get(json_config, "energy_threshold"))) {
            config->energy_threshold = json_number_value(json_object_get(json_config, "energy_threshold"));
        } else {
            config->energy_threshold = 0;
        }

        config->pulse_polarity = POLARITY_NEGATIVE;

        if (json_is_string(json_object_get(json_config, "pulse_polarity"))) {
            const char *pulse_polarity = json_string_value(json_object_get(json_config, "pulse_polarity"));

            if (strstr(pulse_polarity, "Negative") ||
                strstr(pulse_polarity, "negative"))
            {
                config->pulse_polarity = POLARITY_NEGATIVE;
            }
            else if (strstr(pulse_polarity, "Positive") ||
                     strstr(pulse_polarity, "positive"))
            {
                config->pulse_polarity = POLARITY_POSITIVE;
            }
        }

        config->is_error = false;
        config->previous_samples_number = 0;

        config->curve_samples = NULL;
        config->curve_compensated = NULL;
        config->curve_offset = NULL;
        config->curve_CR = NULL;
        config->curve_RC = NULL;

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
    struct CRRC4_config *config = (struct CRRC4_config*)user_config;

    if (config->curve_samples) {
        free(config->curve_samples);
    }
    if (config->curve_compensated) {
        free(config->curve_compensated);
    }
    if (config->curve_offset) {
        free(config->curve_offset);
    }
    if (config->curve_CR) {
        free(config->curve_CR);
    }
    if (config->curve_RC) {
        free(config->curve_RC);
    }

    if (user_config) {
        free(user_config);
    }
}

/*! \brief Function that determines the energy and CRRC4 information with the double integration method.
 */
void energy_analysis(const uint16_t *samples,
                     uint32_t samples_number,
                     uint32_t trigger_position,
                     struct event_waveform *waveform,
                     struct event_PSD *event,
                     int8_t *select_event,
                     void *user_config)
{
    UNUSED(trigger_position);

    struct CRRC4_config *config = (struct CRRC4_config*)user_config;

    reallocate_curves(samples_number, &config);

    if (config->is_error) {
        printf("ERROR: libCRRC4 energy_analysis(): Error status detected\n");

        return;
    }

    to_double(samples, samples_number, &config->curve_samples);

    // Preventing segfaults by checking the boundaries
    const uint32_t baseline_start = 0;
    const uint32_t baseline_end = (config->baseline_samples < samples_number) ? config->baseline_samples : samples_number;

    double baseline = 0;
    calculate_average(config->curve_samples, baseline_start, baseline_end, &baseline);

    if (config->pulse_polarity == POLARITY_POSITIVE) {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, 1.0, &config->curve_offset);
    } else {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, -1.0, &config->curve_offset);
    }

    decay_compensation(config->curve_offset, samples_number, \
                       config->decay_time, \
                       &config->curve_compensated);

    CR_filter(config->curve_compensated, samples_number, \
              config->highpass_time, \
              &config->curve_CR);

    RC4_filter(config->curve_CR, samples_number, \
               config->lowpass_time, \
               &config->curve_RC);

    double compensated_min = 0;
    double compensated_max = 0;
    size_t compensated_index_min = 0;
    size_t compensated_index_max = 0;

    find_extrema(config->curve_compensated, 0, samples_number,
                 &compensated_index_min, &compensated_index_max,
                 &compensated_min, &compensated_max);

    double CR_min = 0;
    double CR_max = 0;
    size_t CR_index_min = 0;
    size_t CR_index_max = 0;

    find_extrema(config->curve_CR, 0, samples_number,
                 &CR_index_min, &CR_index_max,
                 &CR_min, &CR_max);

    const double CR_maximum = CR_max / config->highpass_time;
    const uint64_t long_CR_maximum = (uint16_t)round(CR_maximum);

    // We convert the 64 bit integers to 16 bit to simulate the digitizer data
    uint16_t int_CR_maximum = long_CR_maximum & UINT16_MAX;

    if (long_CR_maximum > UINT16_MAX)
    {
        int_CR_maximum = UINT16_MAX;
    }

    double RC_min = 0;
    double RC_max = 0;
    size_t RC_index_min = 0;
    size_t RC_index_max = 0;

    find_extrema(config->curve_RC, 0, samples_number,
                 &RC_index_min, &RC_index_max,
                 &RC_min, &RC_max);

    const double energy = RC_max * config->height_scaling * config->lowpass_time / config->highpass_time;
    const uint64_t long_energy = (uint64_t)round(energy);

    // We convert the 64 bit integers to 16 bit to simulate the digitizer data
    uint16_t int_energy = long_energy & UINT16_MAX;

    if (long_energy > UINT16_MAX)
    {
        int_energy = UINT16_MAX;
    }

    uint64_t int_baseline = ((uint64_t)round(baseline)) & UINT16_MAX;

    const bool PUR = false;

    // Output
    event->timestamp = waveform->timestamp;
    event->qshort = int_CR_maximum;
    event->qlong = int_energy;
    event->baseline = int_baseline;
    event->channel = waveform->channel;
    event->pur = PUR;

    if (energy < config->energy_threshold) {
        (*select_event) = SELECT_FALSE;
    } else {
        const uint8_t initial_additional_number = waveform_additional_get_number(waveform);
        const uint8_t new_additional_number = initial_additional_number + 3;

        waveform_additional_set_number(waveform, new_additional_number);

        uint8_t *additional_compensated = waveform_additional_get(waveform, initial_additional_number + 0);
        uint8_t *additional_CR = waveform_additional_get(waveform, initial_additional_number + 1);
        uint8_t *additional_RC = waveform_additional_get(waveform, initial_additional_number + 2);

        const double compensated_abs_max = (fabs(compensated_max) > fabs(compensated_min)) ? fabs(compensated_max) : fabs(compensated_min);
        const double CR_abs_max = (fabs(CR_max) > fabs(CR_min)) ? fabs(CR_max) : fabs(CR_min);
        const double RC_abs_max = (fabs(RC_max) > fabs(RC_min)) ? fabs(RC_max) : fabs(RC_min);
        //const double CRRC_abs_max = (RC_abs_max > CR_abs_max) ? RC_abs_max : CR_abs_max;

        const uint8_t ZERO = UINT8_MAX / 2;
        const uint8_t MAX = UINT8_MAX / 2;

        for (uint32_t i = 0; i < samples_number; i++) {
            additional_compensated[i] = (config->curve_compensated[i] / compensated_abs_max) * MAX + ZERO;
            additional_CR[i] = (config->curve_CR[i] / CR_abs_max) * MAX + ZERO;
            additional_RC[i] = (config->curve_RC[i] / RC_abs_max) * MAX + ZERO;
        }

        (*select_event) = SELECT_TRUE;
    }
}

void reallocate_curves(uint32_t samples_number, struct CRRC4_config **user_config)
{
    struct CRRC4_config *config = (*user_config);

    if (samples_number != config->previous_samples_number) {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_compensated = realloc(config->curve_compensated,
                                                samples_number * sizeof(double));
        double *new_curve_offset = realloc(config->curve_offset,
                                           samples_number * sizeof(double));
        double *new_curve_CR = realloc(config->curve_CR,
                                       samples_number * sizeof(double));
        double *new_curve_RC = realloc(config->curve_RC,
                                       samples_number * sizeof(double));

        if (!new_curve_samples) {
            printf("ERROR: libCRRC4 reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        } else {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_compensated) {
            printf("ERROR: libCRRC4 reallocate_curves(): Unable to allocate curve_compensated memory\n");

            config->is_error = true;
        } else {
            config->curve_compensated = new_curve_compensated;
        }
        if (!new_curve_offset) {
            printf("ERROR: libCRRC4 reallocate_curves(): Unable to allocate curve_offset memory\n");

            config->is_error = true;
        } else {
            config->curve_offset = new_curve_offset;
        }
        if (!new_curve_CR) {
            printf("ERROR: libCRRC4 reallocate_curves(): Unable to allocate curve_CR memory\n");

            config->is_error = true;
        } else {
            config->curve_CR = new_curve_CR;
        }
        if (!new_curve_RC) {
            printf("ERROR: libCRRC4 reallocate_curves(): Unable to allocate curve_RC memory\n");

            config->is_error = true;
        } else {
            config->curve_RC = new_curve_RC;
        }
    }
}
