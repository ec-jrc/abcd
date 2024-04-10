/*! \brief Determination of the energy information from a short pulse by
 *         integrating the whole pulse. Pulse Shape Discrimination is obtained
 *         through the double integration method.
 *         It calculates the average waveform is the selected E vs PSD region.
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the samples right before the
 *     integration gates.
 *  2. The pulse is offset by the baseline to center it around zero.
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
 * The average waveform is calculated by accumulating all selected waveforms in
 * the selection region between [energy_min, energy_max] and [PSD_min, PSD_max]
 * Each waveform is then substituted by the progressively accumulating average
 * waveform.
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
 * - `energy_min`: pulses with an energy lower than this value are discarded
 *   Optional, default value: 0
 * - `energy_max`: pulses with an energy higher than this value are discarded
 *   Optional, default value: UINT16_MAX
 * - `PSD_min`: minimum value accepted for the PSD parameter. Default value: -0.1
 * - `PSD_max`: maximum value accepted for the PSD parameter. Default value: 1.1
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
    double energy_min;
    double energy_max;
    double PSD_min;
    double PSD_max;

    bool enable_warnings;

    bool is_error;

    uint32_t previous_samples_number;

    uint64_t *samples_cumulative;
    double *curve_integral;

    double *curve_cumulative;
    size_t counter_curves;
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

        if (json_is_number(json_object_get(json_config, "energy_min"))) {
            config->energy_min = json_number_value(json_object_get(json_config, "energy_min"));
        } else {
            config->energy_min = 0;
        }
        if (json_is_number(json_object_get(json_config, "energy_max"))) {
            config->energy_max = json_number_value(json_object_get(json_config, "energy_max"));
        } else {
            config->energy_max = UINT16_MAX;
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

        config->samples_cumulative = NULL;
        config->curve_integral = NULL;
        config->curve_cumulative = NULL;
        config->counter_curves = 0;

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
    struct PSD_config *config = (struct PSD_config*)user_config;

    if (config->samples_cumulative) {
        free(config->samples_cumulative);
    }
    if (config->curve_integral) {
        free(config->curve_integral);
    }
    if (config->curve_cumulative) {
        free(config->curve_cumulative);
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

    bool is_error = false;

    if ((*events_number) != 1) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): Reallocating buffers, from events number: %zu\n", (*events_number));
        }

        // Assuring that there is one event_PSD and discarding others
        is_error = !reallocate_buffers(trigger_positions, events_buffer, events_number, 1);

        if (is_error) {
            printf("ERROR: libPSD energy_analysis(): Unable to reallocate buffers\n");
        }
    }

    const int64_t global_pregate = (config->long_pregate > config->short_pregate) ? config->long_pregate : config->short_pregate;

    int64_t local_start = (*trigger_positions)[0] - global_pregate - config->baseline_samples;

    if (local_start < 0) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): local_start is less than zero!\n");
        }

        local_start = 0;
    } else if (local_start > samples_number - 1) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): local_start is greater than samples_number!\n");
        }

        local_start = samples_number - 1;
    }

    int64_t local_trigger_position = (*trigger_positions)[0] - local_start;

    if (local_trigger_position < 0) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): local_trigger_position is less than zero!\n");
        }

        local_trigger_position = 0;
    } else if (local_trigger_position > samples_number - 1) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): local_trigger_position is greater than samples_number!\n");
        }

        local_trigger_position = samples_number - 1;
    }

    int64_t local_end = samples_number - local_start;

    uint32_t baseline_end = config->baseline_samples;

    if (baseline_end < 1) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): baseline_end is less than one!\n");
        }

        baseline_end = 1;
    } else if (baseline_end > local_end) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): baseline_end is greater than local_end!\n");
        }

        baseline_end = local_end;
    }


    int64_t short_gate_start = local_trigger_position - config->short_pregate;
    int64_t long_gate_start = local_trigger_position - config->long_pregate;

    if (short_gate_start < 0) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): short_gate_start is less than zero!\n");
        }

        short_gate_start = 0;
    } else if (short_gate_start >= local_end) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): short_gate_start is greater than local_end!\n");
        }

        short_gate_start = local_end - 1;
    }

    if (long_gate_start < 0) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): long_gate_start is less than zero!\n");
        }

        long_gate_start = 0;
    } else if (long_gate_start >= local_end) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): long_gate_start is greater than local_end!\n");
        }

        long_gate_start = local_end - 1;
    }

    // N.B. That '-1' is to have the right integration window since,
    // having a discrete domain, the integrals should be considered
    // calculated always up to the right edge of the intervals.
    int64_t short_gate_end = short_gate_start + config->short_gate - 1;
    int64_t long_gate_end = long_gate_start + config->long_gate - 1;

    if (short_gate_end < 0) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): short_gate_end is less than zero!\n");
        }

        short_gate_end = 0;
    } else if (short_gate_end >= local_end) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): short_gate_end is greater than local_end!\n");
        }

        short_gate_end = local_end - 1;
    }

    if (long_gate_end < 0) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): long_gate_end is less than zero!\n");
        }

        long_gate_end = 0;
    } else if (long_gate_end >= local_end) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): long_gate_end is greater than local_end!\n");
        }

        long_gate_end = local_end - 1;
    }

    if (config->is_error || is_error) {
        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): Error status detected\n");
        }

        return;
    }

    cumulative_sum(samples + local_start, local_end, &config->samples_cumulative);

        if (config->enable_warnings) {
            printf("WARNING: libPSD energy_analysis(): long_gate_start is greater than local_end!\n");
        }

    const uint64_t raw_baseline = config->samples_cumulative[baseline_end - 1];
    const double baseline = (double)raw_baseline / baseline_end;

    integral_baseline_subtract(config->samples_cumulative, local_end, baseline, &config->curve_integral);

    const double qshort = config->curve_integral[short_gate_end]
                          - config->curve_integral[short_gate_start - 2];
    const double qlong  = config->curve_integral[long_gate_end]
                          - config->curve_integral[long_gate_start - 2];

    const double PSD = (qlong != 0) ? (qlong - qshort) / qlong : (config->PSD_min - 1);

    const double scaled_qshort = qshort * config->integrals_scaling
                                 * (config->pulse_polarity == POLARITY_POSITIVE ? 1.0 : -1.0);
    const double scaled_qlong  = qlong * config->integrals_scaling
                                 * (config->pulse_polarity == POLARITY_POSITIVE ? 1.0 : -1.0);

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

    if (scaled_qlong < config->energy_min || config->energy_max < scaled_qlong || PSD < config->PSD_min || PSD > config->PSD_max) {
        // Discard the event
        reallocate_buffers(trigger_positions, events_buffer, events_number, 0);
    } else {
    	config->counter_curves += 1;
    	uint16_t *waveform_samples = waveform_samples_get(waveform);
    	
    	// Calculating average waveform
    	for (uint32_t i = 0; i < samples_number; i++ ) {
        	config->curve_cumulative[i] += (samples[i] - baseline);

		waveform_samples[i] = (config->curve_cumulative[i] / config->counter_curves) + (UINT16_MAX / 2);
    	}
    	
        // Output
        // We have to assume that this was taken care earlier
        //(*events_buffer)[0].timestamp = waveform->timestamp;
        (*events_buffer)[0].qshort = int_qshort;
        (*events_buffer)[0].qlong = int_qlong;
        (*events_buffer)[0].baseline = int_baseline;
        (*events_buffer)[0].channel = waveform->channel;
        (*events_buffer)[0].group_counter = group_counter;

        const uint8_t initial_additional_number = waveform_additional_get_number(waveform);
        const uint8_t new_additional_number = initial_additional_number + 4;

        waveform_additional_set_number(waveform, new_additional_number);

        uint8_t *additional_gate_short = waveform_additional_get(waveform, initial_additional_number + 0);
        uint8_t *additional_gate_long = waveform_additional_get(waveform, initial_additional_number + 1);
        uint8_t *additional_gate_baseline = waveform_additional_get(waveform, initial_additional_number + 2);
        uint8_t *additional_integral = waveform_additional_get(waveform, initial_additional_number + 3);

        double integral_min = 0;
        double integral_max = 0;
        size_t integral_index_min = 0;
        size_t integral_index_max = 0;

        find_extrema(config->curve_integral, 0, local_end,
                     &integral_index_min, &integral_index_max,
                     &integral_min, &integral_max);

        const double integral_abs_max = (fabs(integral_max) > fabs(integral_min)) ? fabs(integral_max) : fabs(integral_min);

        const uint8_t ZERO = UINT8_MAX / 2;
        const uint8_t MAX = UINT8_MAX / 2;

        for (uint32_t global_i = 0; global_i < local_start; global_i++) {
            additional_integral[global_i] = ZERO;
            additional_gate_baseline[global_i] = ZERO;
            additional_gate_short[global_i] = ZERO;
            additional_gate_long[global_i] = ZERO;
        }

        for (uint32_t global_i = local_start; global_i < samples_number; global_i++) {
            const uint32_t local_i = global_i - local_start;

            additional_integral[global_i] = (config->curve_integral[local_i] / integral_abs_max) * MAX + ZERO;

            if (local_i < baseline_end)
            {
                additional_gate_baseline[global_i] = ZERO + MAX / 2;
            } else {
                additional_gate_baseline[global_i] = ZERO;
            }

            if (short_gate_start <= local_i && local_i < short_gate_end)
            {
                additional_gate_short[global_i] = ZERO + MAX;
            } else {
                additional_gate_short[global_i] = ZERO;
            }

            if (long_gate_start <= local_i && local_i < long_gate_end)
            {
                additional_gate_long[global_i] = ZERO + MAX;
            } else {
                additional_gate_long[global_i] = ZERO;
            }
        }
    }
}

void reallocate_curves(uint32_t samples_number, struct PSD_config **user_config)
{
    struct PSD_config *config = (*user_config);

    if (samples_number != config->previous_samples_number) {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        uint64_t *new_samples_cumulative = realloc(config->samples_cumulative,
                                            samples_number * sizeof(uint64_t));
        double *new_curve_integral = realloc(config->curve_integral,
                                                samples_number * sizeof(double));
        double *new_curve_cumulative = realloc(config->curve_cumulative,
                                                samples_number * sizeof(double));

        if (!new_samples_cumulative) {
            printf("ERROR: libPSD reallocate_curves(): Unable to allocate samples_cumulative memory\n");

            config->is_error = true;
        } else {
            config->samples_cumulative = new_samples_cumulative;
        }
        if (!new_curve_integral) {
            printf("ERROR: libPSD reallocate_curves(): Unable to allocate curve_integral memory\n");

            config->is_error = true;
        } else {
            config->curve_integral = new_curve_integral;
        }
        if (!new_curve_cumulative) {
            printf("ERROR: libPSD reallocate_curves(): Unable to allocate curve_cumulative memory\n");

            config->is_error = true;
        } else {
            config->curve_cumulative = new_curve_cumulative;
        }
    }

    if (config->counter_curves == 0) {
        for (uint32_t i = 0; i < samples_number; i++ ) {
            config->curve_cumulative[i] = 0;
	}
    }
}
