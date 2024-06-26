/*! \brief Determination of the energy information from a short pulse, by
 *         applying an (RC)^4 filter to the waveforms.
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the first N samples.
 *  2. The pulse is offset by the baseline to center it around zero.
 *  3. A recursive low-pass filter (RC^4 filter) is applied.
 *  4. The energy information is obtained by determining the absolute maximum
 *     of the resulting waveform.
 *  5. A recursive high-pass filter (CR filter) is applied.
 *  6. The information about the tail is obtained by determining the absolute
 *     minimum of the resulting waveform.
 *
 * In the event_PSD structure the energy information is stored in the qlong,
 * while the qshort stores the value of the minimum of the differentiated
 * signal.
 * The PSD parameter used in the PSD selection is calculated with the formula:
 *
 *          qlong - qshort
 *   PSD = ----------------
 *              qlong
 *
 * The algorithm may be run over a subset of the waveform, by setting the
 * parameters: `analysis_start` and `analysis_end`
 *
 * This function determines only one event_PSD and will discard the other
 * trigger positions determined in the timestamp analysis step.
 *
 * The configuration parameters that are searched in a `json_t` object are:
 *
 * - `baseline_samples`: the number of samples to average to determine the
 *   baseline. The average starts from the beginning of the analysis.
 * - `pulse_polarity`: a string describing the expected pulse polarity, it
 *   can be `positive` or `negative`.
 * - `lowpass_time`: the decay time of the low-pass filter (RC^4 filter), in
 *   terms of clock samples.
 * - `highpass_time`: the decay time of the high-pass filter (CR filter), in
 *   terms of clock samples. Optional, default value: same as `lowpass_time`.
 * - `height_scaling`: a scaling factor multiplied to both extrema.
 *   Optional, default value: 1
 * - `energy_threshold`: pulses with an energy lower than the threshold are
 *   discared. Optional, default value: 0
 * - `PSD_min`: minimum value accepted for the PSD parameter. Default value: -0.1
 * - `PSD_max`: maximum value accepted for the PSD parameter. Default value: 1.1
 * - `analysis_start`: the first bin of the analysis relative to the trigger
 *   position. It can be negative. Optional, default value: 0
 * - `analysis_end`: the last bin of the analysis relative to the trigger
 *   position. Optional, default value: `samples_number`
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
struct RC4_config
{
    uint32_t baseline_samples;
    int64_t analysis_start;
    int64_t analysis_end;
    double lowpass_time;
    double highpass_time;
    enum pulse_polarity_t pulse_polarity;
    double height_scaling;
    double energy_threshold;
    double PSD_min;
    double PSD_max;

    bool enable_warnings;

    bool is_error;

    uint32_t previous_samples_number;

    double *curve_samples;
    double *curve_offset;
    double *curve_RC;
    double *curve_CR;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct RC4_config **user_config);

/*! \brief Function that reads the json_t configuration for the `energy_analysis()` function.
 *
 * This function parses a JSON object determining the configuration for the
 * `energy_analysis()` function. The configuration is returned as an
 * allocated `struct RC4_config`.
 */
void energy_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config)) {
        printf("ERROR: libRC4 energy_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
        struct RC4_config *config = malloc(1 * sizeof(struct RC4_config));

        if (!config) {
            printf("ERROR: libRC4 energy_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }

        config->lowpass_time = json_number_value(json_object_get(json_config, "lowpass_time"));

        if (json_is_number(json_object_get(json_config, "highpass_time"))) {
            config->highpass_time = json_number_value(json_object_get(json_config, "highpass_time"));
        } else {
            config->highpass_time = config->lowpass_time;
        }

        config->baseline_samples = json_integer_value(json_object_get(json_config, "baseline_samples"));

        if (json_is_number(json_object_get(json_config, "analysis_start"))) {
            config->analysis_start = json_integer_value(json_object_get(json_config, "analysis_start"));
        } else {
            config->analysis_start = 0;
        }
        if (json_is_number(json_object_get(json_config, "analysis_end"))) {
            config->analysis_end = json_integer_value(json_object_get(json_config, "analysis_end"));
        } else {
            config->analysis_end = UINT32_MAX;
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

        if (json_is_number(json_object_get(json_config, "PSD_min"))) {
            config->PSD_min = json_number_value(json_object_get(json_config, "PSD_min"));
        } else {
            config->PSD_min = -0.1;
        }

        if (json_is_number(json_object_get(json_config, "PSD_max"))) {
            config->PSD_max = json_number_value(json_object_get(json_config, "PSD_max"));
        } else {
            config->PSD_max = 1.1;
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

        if (json_is_true(json_object_get(json_config, "enable_warnings"))) {
            config->enable_warnings = true;
        } else {
            config->enable_warnings = false;
        }

        config->is_error = false;
        config->previous_samples_number = 0;

        config->curve_samples = NULL;
        config->curve_offset = NULL;
        config->curve_RC = NULL;
        config->curve_CR = NULL;

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
    struct RC4_config *config = (struct RC4_config*)user_config;

    if (config->curve_samples) {
        free(config->curve_samples);
    }
    if (config->curve_offset) {
        free(config->curve_offset);
    }
    if (config->curve_RC) {
        free(config->curve_RC);
    }
    if (config->curve_CR) {
        free(config->curve_CR);
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
    struct RC4_config *config = (struct RC4_config*)user_config;

    reallocate_curves(samples_number, &config);

    bool is_error = false;

    if ((*events_number) != 1) {
        if (config->enable_warnings) {
            printf("WARNING: libRC4 energy_analysis(): Reallocating buffers, from events number: %zu\n", (*events_number));
        }

        // Assuring that there is one event_PSD and discarding others
        is_error = !reallocate_buffers(trigger_positions, events_buffer, events_number, 1);

        if (is_error) {
            printf("ERROR: libRC4 energy_analysis(): Unable to reallocate buffers\n");
        }
    }

    if (is_error || config->is_error) {
        printf("ERROR: libRC4 energy_analysis(): Error status detected\n");

        return;
    }

    to_double(samples, samples_number, &config->curve_samples);

    // Preventing segfaults by checking the boundaries
    const int64_t raw_analysis_start = config->analysis_start + (*trigger_positions)[0];
    const int64_t raw_analysis_end = config->analysis_end + (*trigger_positions)[0];
    uint32_t analysis_start = raw_analysis_start;
    if (raw_analysis_start < 0) {
        analysis_start = 0;
    } else if (raw_analysis_start > samples_number) {
        analysis_start = samples_number;
    }
    const uint32_t analysis_end = (raw_analysis_end < samples_number) ? raw_analysis_end : samples_number;
    const uint32_t analysis_width = analysis_end - analysis_start;

    const uint32_t baseline_start = analysis_start;
    const uint32_t baseline_end = ((baseline_start + config->baseline_samples) < samples_number) ? (baseline_start + config->baseline_samples) : samples_number;

    double baseline = 0;
    calculate_average(config->curve_samples, baseline_start, baseline_end, &baseline);

    if (config->pulse_polarity == POLARITY_POSITIVE) {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, 1.0, &config->curve_offset);
    } else {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, -1.0, &config->curve_offset);
    }

    RC4_filter(config->curve_offset + analysis_start, analysis_width, \
               config->lowpass_time, \
               &config->curve_RC);

    double RC_min = 0;
    double RC_max = 0;
    size_t RC_index_min = 0;
    size_t RC_index_max = 0;

    find_extrema(config->curve_RC, 0, analysis_width,
                 &RC_index_min, &RC_index_max,
                 &RC_min, &RC_max);

    CR_filter(config->curve_RC, analysis_width, \
              config->highpass_time, \
              &config->curve_CR);

    double CR_min = 0;
    double CR_max = 0;
    size_t CR_index_min = 0;
    size_t CR_index_max = 0;

    find_extrema(config->curve_CR, 0, analysis_width,
                 &CR_index_min, &CR_index_max,
                 &CR_min, &CR_max);

    const double energy = RC_max * config->height_scaling * config->lowpass_time;
    const uint64_t long_energy = (uint64_t)round(energy);

    // We convert the 64 bit integers to 16 bit to simulate the digitizer data
    uint16_t int_energy = long_energy & UINT16_MAX;

    if (long_energy > UINT16_MAX)
    {
        int_energy = UINT16_MAX;
    }

    const double tail_maximum = fabs(CR_min) * config->height_scaling * config->lowpass_time;
    const uint64_t long_tail_maximum = (uint64_t)round(tail_maximum);
    uint16_t int_tail_maximum = long_tail_maximum & UINT16_MAX;
    if (long_tail_maximum > UINT16_MAX)
    {
        int_tail_maximum = UINT16_MAX;
    }

    const double PSD = (energy != 0) ? ((energy - tail_maximum) / energy) : (config->PSD_min - 1.);

    uint64_t int_baseline = ((uint64_t)round(baseline)) & UINT16_MAX;

    const uint8_t group_counter = 0;

    if (energy < config->energy_threshold || PSD < config->PSD_min || PSD > config->PSD_max) {
        // Discard the event
        reallocate_buffers(trigger_positions, events_buffer, events_number, 0);
    } else {
        // Output
        // We have to assume that this was taken care earlier
        //(*events_buffer)[0].timestamp = waveform->timestamp;
        (*events_buffer)[0].qshort = int_tail_maximum;
        (*events_buffer)[0].qlong = int_energy;
        (*events_buffer)[0].baseline = int_baseline;
        (*events_buffer)[0].channel = waveform->channel;
        (*events_buffer)[0].group_counter = group_counter;

        const uint8_t initial_additional_number = waveform_additional_get_number(waveform);
        const uint8_t new_additional_number = initial_additional_number + 4;

        waveform_additional_set_number(waveform, new_additional_number);

        uint8_t *additional_gate_analysis = waveform_additional_get(waveform, initial_additional_number + 0);
        uint8_t *additional_gate_baseline = waveform_additional_get(waveform, initial_additional_number + 1);
        uint8_t *additional_RC = waveform_additional_get(waveform, initial_additional_number + 2);
        uint8_t *additional_CR = waveform_additional_get(waveform, initial_additional_number + 3);

        const double RC_abs_max = (fabs(RC_max) > fabs(RC_min)) ? fabs(RC_max) : fabs(RC_min);
        const double CR_abs_max = (fabs(CR_max) > fabs(CR_min)) ? fabs(CR_max) : fabs(CR_min);
        const double abs_max = (fabs(RC_abs_max) > fabs(CR_abs_max)) ? fabs(RC_abs_max) : fabs(CR_abs_max);

        const uint8_t ZERO = UINT8_MAX / 2;
        const uint8_t MAX = UINT8_MAX / 2;

        for (uint32_t i = 0; i < samples_number; i++) {
            if (baseline_start <= i && i < baseline_end) {
                additional_gate_baseline[i] = ZERO + MAX / 2;
            } else {
                additional_gate_baseline[i] = ZERO;
            }

            if (analysis_start <= i && i < analysis_end) {
                additional_gate_analysis[i] = ZERO + MAX;
                additional_RC[i] = (config->curve_RC[i - analysis_start] / abs_max) * MAX + ZERO;
                additional_CR[i] = (config->curve_CR[i - analysis_start] / abs_max) * MAX + ZERO;
            } else {
                additional_gate_analysis[i] = ZERO;
                additional_RC[i] = ZERO;
                additional_CR[i] = ZERO;
            }
        }
    }
}

void reallocate_curves(uint32_t samples_number, struct RC4_config **user_config)
{
    struct RC4_config *config = (*user_config);

    if (samples_number != config->previous_samples_number) {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_offset = realloc(config->curve_offset,
                                           samples_number * sizeof(double));
        double *new_curve_RC = realloc(config->curve_RC,
                                       samples_number * sizeof(double));
        double *new_curve_CR = realloc(config->curve_CR,
                                       samples_number * sizeof(double));

        if (!new_curve_samples) {
            printf("ERROR: libRC4 reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        } else {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_offset) {
            printf("ERROR: libRC4 reallocate_curves(): Unable to allocate curve_offset memory\n");

            config->is_error = true;
        } else {
            config->curve_offset = new_curve_offset;
        }
        if (!new_curve_RC) {
            printf("ERROR: libRC4 reallocate_curves(): Unable to allocate curve_RC memory\n");

            config->is_error = true;
        } else {
            config->curve_RC = new_curve_RC;
        }
        if (!new_curve_CR) {
            printf("ERROR: libRC4 reallocate_curves(): Unable to allocate curve_CR memory\n");

            config->is_error = true;
        } else {
            config->curve_CR = new_curve_CR;
        }
    }
}
