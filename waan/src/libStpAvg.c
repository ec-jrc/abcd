/*! \brief Determination of the energy information from an exponentially
 *         decaying pulse, by compensating its decay and calculating the pulse
 *         height with simple averages.
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the first N samples.
 *  2. The pulse is offset by the baseline to center it around zero.
 *  3. The decay is compensated with a recursive filter, obtaining a step
 *     function.
 *  4. The topline is determined by averaging the N samples after the rise time.
 *  5. The energy information is obtained by calculating the difference between
 *     the averages of the baseline and of the topline.
 *  6. The risetime is calculated between the low and the high levels.
 *     The risetime is stored in the baseline entry
 *
 * In the event_PSD structure the energy information is stored in the qlong,
 * while the qshort stores the value of the risetime.
 *
 * The configuration parameters that are searched in a `json_t` object are:
 *
 * - `baseline_samples`: the number of samples to average to determine the
 *   baseline. The average starts from the beginning of the waveform.
 * - `rise_samples`: the number of samples to skip after the end of the baseline
 *   before starting the averaging window of the topline.
 * - `pulse_polarity`: a string describing the expected pulse polarity, it
 *   can be `positive` or `negative`.
 * - `decay_time`: the pulse decay time in terms of clock samples, for the
 *   compensation.
 * - `smooth_samples`: the number of samples to be averaged in the running
 *   mean, rounded to the next odd number. Optional, default value: 1
 * - `low_level`: the low level to calculate the risetime, relative to the pulse
 *   height. Optional, default value: 0.1
 * - `high_level`: the high level to calculate the risetime, relative to the
 *   pulse height. Optional, default value: 0.9
 * - `height_scaling`: a scaling factor multiplied to both the integrals.
 *   Optional, default value: 1
 * - `energy_threshold`: pulses with an energy lower than the threshold are
 *   discared. Optional, default value: 0
 *
 * This function determines only one event_PSD and will discard the others.
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
    uint32_t smooth_samples;
    double low_level;
    double high_level;
    double height_scaling;
    double energy_threshold;

    bool is_error;

    uint32_t previous_samples_number;

    double *curve_samples;
    double *curve_compensated;
    double *curve_offset;
    double *curve_smoothed;
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

        if (json_is_number(json_object_get(json_config, "smooth_samples"))) {
            const unsigned int W = json_number_value(json_object_get(json_config, "smooth_samples"));
            // Rounding it to the next greater odd number
            config->smooth_samples = floor(W / 2) * 2 + 1;
        } else {
            config->smooth_samples = 1;
        }


        if (json_is_number(json_object_get(json_config, "low_level"))) {
            config->low_level = json_number_value(json_object_get(json_config, "low_level"));
        } else {
            config->low_level = 0.1;
        }

        if (json_is_number(json_object_get(json_config, "high_level"))) {
            config->high_level = json_number_value(json_object_get(json_config, "high_level"));
        } else {
            config->high_level = 0.9;
        }

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
        config->curve_smoothed = NULL;

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
    if (config->curve_smoothed) {
        free(config->curve_smoothed);
    }

    if (user_config) {
        free(user_config);
    }
}

/*! \brief Function that determines the energy information.
 */
void energy_analysis(const uint16_t *samples,
                     uint32_t samples_number,
                     struct event_waveform *waveform,
                     uint32_t **trigger_positions,
                     struct event_PSD **events_buffer,
                     size_t *events_number,
                     void *user_config)
{
    UNUSED(trigger_positions);

    struct StpAvg_config *config = (struct StpAvg_config*)user_config;

    reallocate_curves(samples_number, &config);

    bool is_error = false;

    if ((*events_number) != 1) {
        printf("WARNING: libStpAvg energy_analysis(): Reallocating buffers, from events number: %zu\n", (*events_number));

        // Assuring that there is one event_PSD and discarding others
        is_error = !reallocate_buffers(trigger_positions, events_buffer, events_number, 1);

        if (is_error) {
            printf("ERROR: libStpAvg energy_analysis(): Unable to reallocate buffers\n");
        }
    }

    if (config->is_error) {
        printf("ERROR: libStpAvg energy_analysis(): Error status detected\n");

        return;
    }

    to_double(samples, samples_number, &config->curve_samples);

    // Preventing segfaults by checking the boundaries
    const uint32_t bottomline_start = 0;
    const uint32_t bottomline_end = (config->baseline_samples < samples_number) ? config->baseline_samples : samples_number;
    const uint32_t topline_start = ((config->baseline_samples + config->rise_samples) < samples_number) ? config->baseline_samples + config->rise_samples : samples_number;
    const uint32_t topline_end = ((topline_start + config->baseline_samples) < samples_number) ? topline_start + config->baseline_samples : samples_number;

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

    const double level_low = config->low_level * (topline - bottomline) + bottomline;
    const double level_high = config->high_level * (topline - bottomline) + bottomline;
    size_t index_low = 0;
    size_t index_high = 0;

    // We use the running mean only for the risetime calculation.
    // For the rest we use the compensated curve.
    running_mean(config->curve_compensated, samples_number, config->smooth_samples, &config->curve_smoothed);

    risetime(config->curve_smoothed, 0, samples_number,
             level_low, level_high,
             &index_low, &index_high);

    size_t risetime_samples = index_high - index_low;

    const double delta = (topline - bottomline) * config->height_scaling;

    const uint64_t long_delta = (uint64_t)round(delta);

    // We convert the 64 bit integers to 16 bit to simulate the digitizer data
    uint16_t int_delta = long_delta & UINT16_MAX;

    if (long_delta > UINT16_MAX)
    {
        int_delta = UINT16_MAX;
    }
    if (risetime_samples > UINT16_MAX)
    {
        risetime_samples = UINT16_MAX;
    }

    uint64_t int_baseline = ((uint64_t)round(baseline)) & UINT16_MAX;

    const uint8_t group_counter = 0;

    if (delta < config->energy_threshold) {
        // Discard the event
        reallocate_buffers(trigger_positions, events_buffer, events_number, 0);
    } else {
        // Output
        // We have to assume that this was taken care earlier
        //(*events_buffer)[0].timestamp = waveform->timestamp;
        (*events_buffer)[0].qshort = risetime_samples;
        (*events_buffer)[0].qlong = int_delta;
        (*events_buffer)[0].baseline = int_baseline;
        (*events_buffer)[0].channel = waveform->channel;
        (*events_buffer)[0].group_counter = group_counter;

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
    }
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
        double *new_curve_smoothed = realloc(config->curve_smoothed,
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
        if (!new_curve_smoothed) {
            printf("ERROR: libGrid reallocate_curves(): Unable to allocate curve_smoothed memory\n");

            config->is_error = true;
        } else {
            config->curve_smoothed = new_curve_smoothed;
        }
    }
}
