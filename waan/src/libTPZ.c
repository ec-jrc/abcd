/*! \brief Determination of the energy information from an exponentially
 *         decaying pulse, by compensating its decay and then applying a
 *         trapezoidal filter to the waveforms.
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the first N samples.
 *  2. The pulse is offset by the baseline to center it around zero.
 *  3. The decay is compensated with a recursive filter, obtaining a step
 *     function.
 *  4. A recursive trapezoidal filter is applied.
 *  5. The energy information is obtained by determining the absolute maximum
 *     of the resulting waveform.
 *
 * In the event_PSD structure the energy information is stored in the qlong,
 * while the qshort stores the value of the shorter integral.
 *
 * The configuration parameters that are searched in a `json_t` object are:
 *
 * - `baseline_samples`: the number of samples to average to determine the
 *   baseline. The average starts from the beginning of the waveform.
 * - `pulse_polarity`: a string describing the expected pulse polarity, it
 *   can be `positive` or `negative`.
 * - `decay_time`: the pulse decay time in terms of clock samples, for the
 *   compensation.
 * - `trapezoid_risetime`: the risetime of the resulting trapezoid.
 * - `trapezoid_flattop`: the width of the trapezoid top.
 * - `peak_position`: a string describing the determation method of the
 *   trapezoid height, can be `maximum` or `peaking_time`, default: `maximum`.
 * - `peaking_time`: the position of the trapezoid top relative to the
 *   trigger_position, the trapezoid height is stored in the qshort entry of
 *   the event_PSD structure.
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

#define PEAK_POSITION_MAXIMUM 0
#define PEAK_POSITION_PEAKING_TIME 1

/*! \brief Sctructure that holds the configuration for the `energy_analysis` function.
 */
struct TPZ_config
{
    double decay_time;
    uint32_t trapezoid_risetime;
    uint32_t trapezoid_flattop;
    int64_t peaking_time;
    uint32_t baseline_samples;
    enum pulse_polarity_t pulse_polarity;
    int peak_position;
    double height_scaling;
    double energy_threshold;

    bool is_error;

    uint32_t previous_samples_number;

    double *curve_samples;
    double *curve_compensated;
    double *curve_offset;
    double *curve_trapezoid;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct TPZ_config **user_config);

/*! \brief Function that reads the json_t configuration for the `energy_analysis()` function.
 *
 * This function parses a JSON object determining the configuration for the
 * `energy_analysis()` function. The configuration is returned as an
 * allocated `struct TPZ_config`.
 *
 */
void energy_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config)) {
        printf("ERROR: libTPZ energy_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
	struct TPZ_config *config = malloc(1 * sizeof(struct TPZ_config));

        if (!config) {
            printf("ERROR: libTPZ energy_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }

        config->decay_time = json_integer_value(json_object_get(json_config, "decay_time"));
        config->trapezoid_risetime = json_integer_value(json_object_get(json_config, "trapezoid_risetime"));
        config->trapezoid_flattop = json_integer_value(json_object_get(json_config, "trapezoid_flattop"));
        config->peaking_time = json_integer_value(json_object_get(json_config, "peaking_time"));
        config->baseline_samples = json_integer_value(json_object_get(json_config, "baseline_samples"));

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

        config->peak_position = PEAK_POSITION_MAXIMUM;

        if (json_is_string(json_object_get(json_config, "peak_position"))) {
            const char *pulse_polarity = json_string_value(json_object_get(json_config, "peak_position"));

            if (strstr(pulse_polarity, "maximum"))
            {
                config->peak_position = PEAK_POSITION_MAXIMUM;
            }
            else if (strstr(pulse_polarity, "peaking_time"))
            {
                config->peak_position = PEAK_POSITION_PEAKING_TIME;
            }
        }

        config->is_error = false;
        config->previous_samples_number = 0;

        config->curve_samples = NULL;
        config->curve_compensated = NULL;
        config->curve_offset = NULL;
        config->curve_trapezoid = NULL;

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
    struct TPZ_config *config = (struct TPZ_config*)user_config;

    if (config->curve_samples) {
        free(config->curve_samples);
    }
    if (config->curve_compensated) {
        free(config->curve_compensated);
    }
    if (config->curve_offset) {
        free(config->curve_offset);
    }
    if (config->curve_trapezoid) {
        free(config->curve_trapezoid);
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
    struct TPZ_config *config = (struct TPZ_config*)user_config;

    reallocate_curves(samples_number, &config);

    bool is_error = false;

    if ((*events_number) != 1) {
        //printf("WARNING: libTPZ energy_analysis(): Reallocating buffers, from events number: %zu\n", (*events_number));

        // Assuring that there is one event_PSD and discarding others
        is_error = !reallocate_buffers(trigger_positions, events_buffer, events_number, 1);

        if (is_error) {
            printf("ERROR: libTPZ energy_analysis(): Unable to reallocate buffers\n");
        }
    }

    if (is_error || config->is_error) {
        printf("ERROR: libTPZ energy_analysis(): Error status detected\n");

        return;
    }

    to_double(samples, samples_number, &config->curve_samples);

    double baseline = 0;

    calculate_average(config->curve_samples, 0, config->baseline_samples, &baseline);

    if (config->pulse_polarity == POLARITY_POSITIVE) {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, 1.0, &config->curve_offset);
    } else {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, -1.0, &config->curve_offset);
    }

    decay_compensation(config->curve_offset, samples_number, \
                       config->decay_time, \
                       &config->curve_compensated);

    trapezoidal_filter(config->curve_compensated, samples_number,
                       config->trapezoid_risetime, config->trapezoid_flattop,
                       &config->curve_trapezoid);

    double trapezoid_min = 0;
    double trapezoid_max = 0;
    size_t trapezoid_index_min = 0;
    size_t trapezoid_index_max = 0;

    find_extrema(config->curve_trapezoid, 0, samples_number,
                 &trapezoid_index_min, &trapezoid_index_max,
                 &trapezoid_min, &trapezoid_max);

    const double energy_maximum = trapezoid_max * config->height_scaling / (config->trapezoid_risetime + config->trapezoid_flattop);
    const double energy_at_peaking = config->curve_trapezoid[config->peaking_time + (*trigger_positions)[0]] * config->height_scaling / (config->trapezoid_risetime + config->trapezoid_flattop);

    const uint64_t long_maximum = (uint64_t)round(energy_maximum);
    const uint64_t long_at_peaking = (uint64_t)round(energy_at_peaking);

    // We convert the 64 bit integers to 16 bit to simulate the digitizer data
    uint16_t int_maximum = long_maximum & UINT16_MAX;
    uint16_t int_at_peaking = long_at_peaking & UINT16_MAX;

    if (long_maximum > UINT16_MAX)
    {
        int_maximum = UINT16_MAX;
    }
    if (long_at_peaking > UINT16_MAX)
    {
        int_at_peaking = UINT16_MAX;
    }

    uint64_t int_baseline = ((uint64_t)round(baseline)) & UINT16_MAX;

    const uint8_t group_counter = 0;

    if (energy_maximum < config->energy_threshold) {
        // Discard the event
        reallocate_buffers(trigger_positions, events_buffer, events_number, 0);
    } else {
        // Output
        // We have to assume that this was taken care earlier
        //(*events_buffer)[0].timestamp = waveform->timestamp;
        if (config->peak_position == PEAK_POSITION_PEAKING_TIME) {
            (*events_buffer)[0].qshort = int_maximum;
            (*events_buffer)[0].qlong = int_at_peaking;
        } else {
            (*events_buffer)[0].qshort = int_at_peaking;
            (*events_buffer)[0].qlong = int_maximum;
        }
        (*events_buffer)[0].baseline = int_baseline;
        (*events_buffer)[0].channel = waveform->channel;
        (*events_buffer)[0].group_counter = group_counter;

        const uint8_t initial_additional_number = waveform_additional_get_number(waveform);
        const uint8_t new_additional_number = initial_additional_number + 3;

        waveform_additional_set_number(waveform, new_additional_number);

        uint8_t *additional_compensated = waveform_additional_get(waveform, initial_additional_number + 0);
        uint8_t *additional_trapezoid = waveform_additional_get(waveform, initial_additional_number + 1);
        uint8_t *additional_positions = waveform_additional_get(waveform, initial_additional_number + 2);

        double compensated_min = 0;
        double compensated_max = 0;
        size_t compensated_index_min = 0;
        size_t compensated_index_max = 0;

        find_extrema(config->curve_compensated, 0, samples_number,
                     &compensated_index_min, &compensated_index_max,
                     &compensated_min, &compensated_max);

        const double compensated_abs_max = (fabs(compensated_max) > fabs(compensated_min)) ? fabs(compensated_max) : fabs(compensated_min);
        const double trapezoid_abs_max = (fabs(trapezoid_max) > fabs(trapezoid_min)) ? fabs(trapezoid_max) : fabs(trapezoid_min);

        const uint8_t ZERO = UINT8_MAX / 2;
        const uint8_t MAX = UINT8_MAX / 2;

        for (uint32_t i = 0; i < samples_number; i++) {
            additional_compensated[i] = (config->curve_compensated[i] / compensated_abs_max) * MAX + ZERO;
            additional_trapezoid[i] = (config->curve_trapezoid[i] / trapezoid_abs_max) * MAX + ZERO;

            if (i == trapezoid_index_max) {
                additional_positions[i] = MAX + ZERO;
            } else if (i == (config->peaking_time + (*trigger_positions)[0])) {
                additional_positions[i] = MAX / 2 + ZERO;
            } else {
                additional_positions[i] = ZERO;
            }
        }
    }
}

void reallocate_curves(uint32_t samples_number, struct TPZ_config **user_config)
{
    struct TPZ_config *config = (*user_config);

    if (samples_number != config->previous_samples_number) {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_compensated = realloc(config->curve_compensated,
                                                samples_number * sizeof(double));
        double *new_curve_offset = realloc(config->curve_offset,
                                           samples_number * sizeof(double));
        double *new_curve_trapezoid = realloc(config->curve_trapezoid,
                                              samples_number * sizeof(double));

        if (!new_curve_samples) {
            printf("ERROR: libTPZ reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        } else {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_compensated) {
            printf("ERROR: libTPZ reallocate_curves(): Unable to allocate curve_compensated memory\n");

            config->is_error = true;
        } else {
            config->curve_compensated = new_curve_compensated;
        }
        if (!new_curve_offset) {
            printf("ERROR: libTPZ reallocate_curves(): Unable to allocate curve_offset memory\n");

            config->is_error = true;
        } else {
            config->curve_offset = new_curve_offset;
        }
        if (!new_curve_trapezoid) {
            printf("ERROR: libTPZ reallocate_curves(): Unable to allocate curve_trapezoid memory\n");

            config->is_error = true;
        } else {
            config->curve_trapezoid = new_curve_trapezoid;
        }
    }
}
