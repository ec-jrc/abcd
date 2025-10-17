/*! \brief Determination of the energy information from multiple pulses per
 *         waveform by integrating the whole pulse. Pulse Shape Discrimination
 *         is obtained through the double integration method.
 *
 * Calculation procedure:
 *  1. For each detected pulse by a timestamp analysis library, the baseline is
 *     determined averaging the samples right before the integration gates.
 *  2. The local pulse is offset by the baseline to center it around zero.
 *  3. The pulse is integrated over a short and a long integration domains.
 *
 * In the event_PSD structure the energy information is stored in the qlong,
 * while the qshort stores the value of the shorter integral.
 * The PSD parameter used in the PSD selection is calculated with the formula:
 *
 *          qlong - qshort
 *   PSD = ----------------
 *              qlong
 *
 * The configuration parameters that are searched in a `json_t` object are:
 *
 * - `baseline_samples`: the number of samples to average to determine the
 *   baseline. The average window ends at the begin of the first integration
 *   gate.
 * - `pulse_polarity`: a string describing the expected pulse polarity, it
 *   can be `positive` or `negative`.
 * - `pregate`: the number of samples before the `trigger_position` that define
 *   the beginning of the integration windows. If this setting is found both
 *   integration windows will start from this point.
 * - `short_pregate`: the number of samples before the `trigger_position` that
 *   define the beginning of the short integration window. This setting has the
 *   precedence over `pregate`.
 * - `long_pregate`: the number of samples before the `trigger_position` that
 *   define the beginning of the long integration window. This setting has the
 *   precedence over `pregate`.
 * - `short_gate`: the number of samples of the width of the short integration
 *   window.
 * - `long_gate`: the number of samples of the width of the long integration
 *   window.
 * - `integrals_scaling`: a scaling factor multiplied to both the integrals.
 *   Optional, default value: 1
 * - `energy_threshold`: pulses with an energy lower than the threshold are
 *   discared. Optional, default value: 0
 * - `PSD_min`: minimum value accepted for the PSD parameter. Default value: -0.1
 * - `PSD_max`: maximum value accepted for the PSD parameter. Default value: 1.1
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

#define min(x,y) ((x) < (y)) ? (x) : (y)
#define max(x,y) ((x) > (y)) ? (x) : (y)

/*! \brief Sctructure that holds the configuration for the `energy_analysis()` function.
 */
struct PSD_config
{
    uint32_t baseline_samples;
    int64_t short_pregate;
    int64_t long_pregate;
    int64_t short_gate;
    int64_t long_gate;
    enum pulse_polarity_t pulse_polarity;
    double integrals_scaling;
    double energy_threshold;
    double PSD_min;
    double PSD_max;

    bool is_error;

    uint32_t previous_samples_number;

    double *curve_doubles;
    double *curve_samples;
    double *curve_integrals;

    struct event_PSD *events_buffer;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct PSD_config **user_config);

/*! \brief Function that reads the json_t configuration for the `energy_analysis()` function.
 *
 * This function parses a JSON object determining the configuration for the
 * `energy_analysis()` function. The configuration is returned as an
 * allocated `struct PSD_config`.
 */
void energy_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config)) {
        printf("ERROR: libPSD energy_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
        struct PSD_config *config = malloc(1 * sizeof(struct PSD_config));

        if (!config) {
            printf("ERROR: libPSD energy_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }

        config->baseline_samples = json_integer_value(json_object_get(json_config, "baseline_samples"));

        config->short_pregate = json_integer_value(json_object_get(json_config, "pregate"));
        config->long_pregate = json_integer_value(json_object_get(json_config, "pregate"));

        if (json_is_number(json_object_get(json_config, "short_pregate"))) {
            config->short_pregate = json_integer_value(json_object_get(json_config, "short_pregate"));
        }
        if (json_is_number(json_object_get(json_config, "long_pregate"))) {
            config->long_pregate = json_integer_value(json_object_get(json_config, "long_pregate"));
        }

        config->short_gate = json_integer_value(json_object_get(json_config, "short_gate"));
        config->long_gate = json_integer_value(json_object_get(json_config, "long_gate"));

        if (json_is_number(json_object_get(json_config, "integrals_scaling"))) {
            config->integrals_scaling = json_number_value(json_object_get(json_config, "integrals_scaling"));
        } else {
            config->integrals_scaling = 1;
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

        config->is_error = false;
        config->previous_samples_number = 0;

        config->curve_doubles = NULL;
        config->curve_samples = NULL;
        config->curve_integrals = NULL;
        config->events_buffer = NULL;

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
    struct PSD_config *config = (struct PSD_config*)user_config;

    if (config->curve_doubles) {
        free(config->curve_doubles);
    }
    if (config->curve_samples) {
        free(config->curve_samples);
    }
    if (config->curve_integrals) {
        free(config->curve_integrals);
    }
    if (config->events_buffer) {
        free(config->events_buffer);
    }

    if (user_config) {
        free(user_config);
    }
}

/*! \brief Function that determines the energy and PSD information with the double integration method.
 */
void energy_analysis(const uint16_t *samples,
                     uint32_t samples_number,
                     struct event_waveform *waveform,
                     uint32_t **trigger_positions,
                     struct event_PSD **events_buffer,
                     size_t *events_number,
                     void *user_config)
{
    struct PSD_config *config = (struct PSD_config*)user_config;

    reallocate_curves(samples_number, &config);

    // Reset the integrals to zero, as the integrals from each pulse will be
    // summed on top of each other
    for (uint32_t index = 0; index < samples_number; index += 1) {
        config->curve_integrals[index] = 0;
    }

    to_double(samples, samples_number, &config->curve_doubles);

    // A quick baseline calculation to have the curve roughly around zero.
    // The baseline is calculated anyways for each pulse.
    double rough_baseline = 0;

    calculate_average(config->curve_doubles, 0, config->baseline_samples, &rough_baseline);

    if (config->pulse_polarity == POLARITY_POSITIVE) {
        add_and_multiply_constant(config->curve_doubles, samples_number, -rough_baseline, 1.0, &config->curve_samples);
    } else {
        add_and_multiply_constant(config->curve_doubles, samples_number, -rough_baseline, -1.0, &config->curve_samples);
    }

    // Additionals are created early on because they are built while pulses are
    // analysed
    const uint8_t initial_additional_number = waveform_additional_get_number(waveform);
    const uint8_t new_additional_number = initial_additional_number + 4;

    waveform_additional_set_number(waveform, new_additional_number);

    uint8_t *additional_gate_short = waveform_additional_get(waveform, initial_additional_number + 0);
    uint8_t *additional_gate_long = waveform_additional_get(waveform, initial_additional_number + 1);
    uint8_t *additional_gate_baseline = waveform_additional_get(waveform, initial_additional_number + 2);
    uint8_t *additional_integrals = waveform_additional_get(waveform, initial_additional_number + 3);

    const uint8_t ZERO = UINT8_MAX / 2;
    const uint8_t MAX = UINT8_MAX / 2;
    // This means that we expect no more than 4 overlapping gates
    const uint8_t STEP = MAX / 4;

    for (uint32_t index = 0; index < samples_number; index++) {
        additional_gate_short[index] = ZERO;
        additional_gate_long[index] = ZERO;
        additional_gate_baseline[index] = ZERO;
        additional_integrals[index] = ZERO;
    }

    const int64_t global_pregate = (config->long_pregate > config->short_pregate) ? config->long_pregate : config->short_pregate;

    uint32_t counter_selected_pulses = 0;

    for (size_t events_index = 0; events_index < (*events_number); events_index += 1) {

        // Preventing segfaults by checking the boundaries
        const int64_t raw_baseline_start = (*trigger_positions)[events_index] - global_pregate - config->baseline_samples;
        const uint32_t baseline_start = raw_baseline_start < 0 ? 0 : raw_baseline_start;

        const uint32_t baseline_end = ((baseline_start + config->baseline_samples) < samples_number) ? (baseline_start + config->baseline_samples) : samples_number;

        double baseline = 0;
        calculate_average(config->curve_samples, baseline_start, baseline_end, &baseline);

        const uint32_t local_trigger_position = (*trigger_positions)[events_index];

        int64_t short_gate_start = (local_trigger_position - config->short_pregate) < 0 ? 0 : (local_trigger_position - config->short_pregate);
        int64_t long_gate_start =  (local_trigger_position - config->long_pregate) < 0 ? 0 : (local_trigger_position - config->long_pregate);

        int64_t short_gate_end = (short_gate_start + config->short_gate) < samples_number ? (short_gate_start + config->short_gate) : samples_number;
        int64_t long_gate_end =  (long_gate_start + config->long_gate) < samples_number ? (long_gate_start + config->long_gate) : samples_number;

        uint64_t gates_end = (short_gate_end > long_gate_end) ? short_gate_end : long_gate_end;

        double qshort = 0;
        double qlong = 0;

        double integral = 0;

        for (uint32_t index = baseline_start; index < gates_end; index += 1) {
            integral += (config->curve_samples[index] - baseline);

            config->curve_integrals[index] += integral;

            if (baseline_start <= index && index < baseline_end) {
                additional_gate_baseline[index] += STEP / 2;
            }
            if (short_gate_start <= index && index < short_gate_end) {
                qshort += (config->curve_samples[index] - baseline);

                additional_gate_short[index] += STEP;
            }
            if (long_gate_start <= index && index < long_gate_end) {
                qlong += (config->curve_samples[index] - baseline);

                additional_gate_long[index] += STEP;
            }
        
        }

        const double PSD = (qlong != 0) ? (qlong - qshort) / qlong : (config->PSD_min - 1);

        const double scaled_qshort = qshort * config->integrals_scaling;
        const double scaled_qlong  = qlong * config->integrals_scaling;

        const uint64_t long_qshort = (uint64_t)round(scaled_qshort);
        const uint64_t long_qlong = (uint64_t)round(scaled_qlong);

        // We convert the 64 bit integers to 16 bit to simulate the digitizer data
        uint16_t int_qshort = long_qshort & UINT16_MAX;
        uint16_t int_qlong = long_qlong & UINT16_MAX;

        if (long_qshort > UINT16_MAX)
        {
            int_qshort = UINT16_MAX;
        }
        if (long_qlong > UINT16_MAX)
        {
            int_qlong = UINT16_MAX;
        }

        uint64_t int_baseline = ((uint64_t)round(baseline)) & UINT16_MAX;

        const uint8_t group_counter = 0;

        if (config->energy_threshold <= scaled_qlong && (config->PSD_min <= PSD && PSD < config->PSD_max)) {
            // Output
            // We have to assume that this was taken care earlier
            //config->events_buffer[counter_selected_pulses].timestamp = waveform->timestamp;
            config->events_buffer[counter_selected_pulses].qshort = int_qshort;
            config->events_buffer[counter_selected_pulses].qlong = int_qlong;
            config->events_buffer[counter_selected_pulses].baseline = int_baseline;
            config->events_buffer[counter_selected_pulses].channel = waveform->channel;
            config->events_buffer[counter_selected_pulses].group_counter = group_counter;

            counter_selected_pulses += 1;
        }

    }

    if ((*events_number) != counter_selected_pulses) {
        reallocate_buffers(trigger_positions, events_buffer, events_number, counter_selected_pulses);
    }

    memcpy((*events_buffer), config->events_buffer, counter_selected_pulses * sizeof(struct event_PSD));

    double integral_min = 0;
    double integral_max = 0;
    size_t integral_index_min = 0;
    size_t integral_index_max = 0;

    find_extrema(config->curve_integrals, 0, samples_number,
                 &integral_index_min, &integral_index_max,
                 &integral_min, &integral_max);

    const double integral_abs_max = (fabs(integral_max) > fabs(integral_min)) ? fabs(integral_max) : fabs(integral_min);

    for (uint32_t index = 0; index < samples_number; index += 1) {
        additional_integrals[index] = config->curve_integrals[index] / integral_abs_max * MAX + ZERO;
    }
}

void reallocate_curves(uint32_t samples_number, struct PSD_config **user_config)
{
    struct PSD_config *config = (*user_config);

    if (samples_number != config->previous_samples_number) {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_doubles = realloc(config->curve_doubles,
                                            samples_number * sizeof(double));
        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_integrals = realloc(config->curve_integrals,
                                            samples_number * sizeof(double));
        struct event_PSD *new_events_buffer = realloc(config->events_buffer,
                                                      samples_number * sizeof(struct event_PSD));

        if (!new_curve_doubles) {
            printf("ERROR: libMultiPSD reallocate_curves(): Unable to allocate curve_doubles memory\n");

            config->is_error = true;
        } else {
            config->curve_doubles = new_curve_doubles;
        }
        if (!new_curve_samples) {
            printf("ERROR: libMultiPSD reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        } else {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_integrals) {
            printf("ERROR: libMultiPSD reallocate_curves(): Unable to allocate curve_integrals memory\n");

            config->is_error = true;
        } else {
            config->curve_integrals = new_curve_integrals;
        }
        if (!new_events_buffer) {
            printf("ERROR: libMultiPSD reallocate_curves(): Unable to allocate events_buffer memory\n");

            config->is_error = true;
        } else {
            config->events_buffer = new_events_buffer;
        }
    }
}
