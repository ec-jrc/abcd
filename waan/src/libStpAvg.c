/*! \brief Determination of the energy information from an exponentially
 *         decaying pulse, by compensating its decay and calculating the pulse
 *         height with simple averages.
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the first N samples.
 *  2. The pulse is offset by the baseline to center it around zero.
 *  3. The decay is compensated with a recursive filter, obtaining a step
 *     function.
 *  4. The energy information is obtained by calculating the difference between
 *     the averages of the baseline and of the topline.
 *
 * In the event_PSD structure the energy information is stored in the qlong,
 * while the qshort stores the value of the topline only.
 *
 * The configuration parameters that are searched in a `json_t` object are:
 *
 * - `baseline_samples`: the number of samples to average to determine the
 *   baseline. The average starts from the beginning of the waveform.
 * - `rise_samples`: the number of samples to skip after the end of the baseline
 *   before starting the averaging window of the topline.
 * - `pulse_polarity`: a string describing the expected pulse polarity, it
 *   can be "positive` or "negative".
 * - `decay_time`: the pulse decay time in terms of clock samples, for the
 *   compensation.
 * - `height_scaling`: a scaling factor multiplied to both the integrals.
 *   Optional, default value: 1
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
struct StpAvg_config
{
    double decay_time;
    uint32_t baseline_samples;
    uint32_t rise_samples;
    enum pulse_polarity_t pulse_polarity;
    double height_scaling;

    bool is_error;

    uint32_t previous_samples_number;

    double *curve_samples;
    double *curve_compensated;
    double *curve_offset;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct StpAvg_config **user_config);

/*! \brief Function that reads the json_t configuration for the `energy_analysis()` function.
 *
 * This function parses a JSON object determining the configuration for the
 * `energy_analysis()` function. The configuration is returned as an
 * allocated `struct StpAvg_config`.
 */
void energy_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config)) {
        printf("ERROR: libStpAvg energy_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
	struct StpAvg_config *config = malloc(1 * sizeof(struct StpAvg_config));

        if (!config) {
            printf("ERROR: libStpAvg energy_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }

        config->decay_time = json_integer_value(json_object_get(json_config, "decay_time"));
        config->baseline_samples = json_integer_value(json_object_get(json_config, "baseline_samples"));
        config->rise_samples = json_integer_value(json_object_get(json_config, "rise_samples"));

        if (json_is_number(json_object_get(json_config, "height_scaling"))) {
            config->height_scaling = json_number_value(json_object_get(json_config, "height_scaling"));
        } else {
            config->height_scaling = 1;
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

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
    struct StpAvg_config *config = (struct StpAvg_config*)user_config;

    if (config->curve_samples) {
        free(config->curve_samples);
    }
    if (config->curve_compensated) {
        free(config->curve_compensated);
    }
    if (config->curve_offset) {
        free(config->curve_offset);
    }

    if (user_config) {
        free(user_config);
    }
}

/*! \brief Function that determines the energy and PSD information.
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

    struct StpAvg_config *config = (struct StpAvg_config*)user_config;

    reallocate_curves(samples_number, &config);

    if (config->is_error) {
        printf("ERROR: libStpAvg energy_analysis(): Error status detected\n");

        return;
    }

    to_double(samples, samples_number, &config->curve_samples);

    // Preventing segfaults by checking the boundaries
    const uint32_t bottomline_start = 0;
    const uint32_t bottomline_end = (config->baseline_samples < samples_number) ? config->baseline_samples : samples_number;
    const uint32_t topline_start = ((config->baseline_samples + config->rise_samples) < samples_number) ? config->baseline_samples + config->rise_samples : samples_number;
    const uint32_t topline_end = samples_number;

    double baseline = 0;
    calculate_average(config->curve_samples, bottomline_start, bottomline_end, &baseline);

    if (config->pulse_polarity == POLARITY_POSITIVE) {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, 1.0, &config->curve_offset);
    } else {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, -1.0, &config->curve_offset);
    }

    decay_compensation(config->curve_offset, samples_number, \
                       config->decay_time, \
                       &config->curve_compensated);

    // The curve after the compensation sometimes does not have a zero baseline
    // so we correct again for the new baseline.
    double bottomline = 0;
    calculate_average(config->curve_compensated, bottomline_start, bottomline_end, &bottomline);

    double topline = 0;
    calculate_average(config->curve_compensated, topline_start, topline_end, &topline);

    const uint64_t long_delta = (uint64_t)round((topline - bottomline) * config->height_scaling);
    const uint64_t long_topline = (uint64_t)round(topline * config->height_scaling);

    // We convert the 64 bit integers to 16 bit to simulate the digitizer data
    uint16_t int_delta = long_delta & UINT16_MAX;
    uint16_t int_topline = long_topline & UINT16_MAX;

    if (long_delta > UINT16_MAX)
    {
        int_delta = UINT16_MAX;
    }
    if (long_topline > UINT16_MAX)
    {
        int_topline = UINT16_MAX;
    }

    uint64_t int_baseline = ((uint64_t)round(baseline)) & UINT16_MAX;

    const bool PUR = false;

    // Output
    event->timestamp = waveform->timestamp;
    event->qshort = int_topline;
    event->qlong = int_delta;
    event->baseline = int_baseline;
    event->channel = waveform->channel;
    event->pur = PUR;

    // We determine some parameters to generate the additional pulses
    double compensated_min = 0;
    double compensated_max = 0;
    size_t compensated_index_min = 0;
    size_t compensated_index_max = 0;

    find_extrema(config->curve_compensated, 0, samples_number,
                 &compensated_index_min, &compensated_index_max,
                 &compensated_min, &compensated_max);

    const double compensated_abs_max = (fabs(compensated_max) > fabs(compensated_min)) ? fabs(compensated_max) : fabs(compensated_min);

    const uint8_t initial_additional_number = waveform_additional_get_number(waveform);
    const uint8_t new_additional_number = initial_additional_number + 2;

    waveform_additional_set_number(waveform, new_additional_number);

    uint8_t *additional_compensated = waveform_additional_get(waveform, initial_additional_number + 0);
    uint8_t *additional_levels = waveform_additional_get(waveform, initial_additional_number + 1);

    const uint8_t ZERO = UINT8_MAX / 2;
    const uint8_t MAX = UINT8_MAX / 2;

    for (uint32_t i = 0; i < samples_number; i++) {
        additional_compensated[i] = (config->curve_compensated[i] / compensated_abs_max) * MAX + ZERO;

        if (i < config->baseline_samples) {
            additional_levels[i] = bottomline / compensated_abs_max * MAX + ZERO;
        } else if (config->baseline_samples <= i && i < config->baseline_samples + config->rise_samples) {
            // Creates a visual ramp that goes from bottomline to topline
            additional_levels[i] = ((double)i - config->baseline_samples) / config->rise_samples * topline / compensated_abs_max * MAX + ZERO;
        } else {
            additional_levels[i] = topline / compensated_abs_max * MAX + ZERO;
        }
    }

    (*select_event) = SELECT_TRUE;
}

void reallocate_curves(uint32_t samples_number, struct StpAvg_config **user_config)
{
    struct StpAvg_config *config = (*user_config);

    if (samples_number != config->previous_samples_number) {
	config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_compensated = realloc(config->curve_compensated,
                                                samples_number * sizeof(double));
        double *new_curve_offset = realloc(config->curve_offset,
                                           samples_number * sizeof(double));

        if (!new_curve_samples) {
            printf("ERROR: libStpAvg reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        } else {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_compensated) {
            printf("ERROR: libStpAvg reallocate_curves(): Unable to allocate curve_compensated memory\n");

            config->is_error = true;
        } else {
            config->curve_compensated = new_curve_compensated;
        }
        if (!new_curve_offset) {
            printf("ERROR: libStpAvg reallocate_curves(): Unable to allocate curve_offset memory\n");

            config->is_error = true;
        } else {
            config->curve_offset = new_curve_offset;
        }
    }
}
