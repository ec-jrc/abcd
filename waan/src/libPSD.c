/*! \brief Determination of the energy information from an exponentially
 *         decaying pulse, by compensating its decay and then applying a CR-RC4
 *         filter to the waveforms.
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the first N samples.
 *  2. The pulse is offset by the baseline to center it around zero.
 *  3. The pulse is integrated over a short and a long integration domains.
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

/*! \brief Sctructure that holds the configuration for the `energy_analysis` function.
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

    bool is_error;

    uint32_t previous_samples_number;

    uint64_t *samples_cumulative;
    double *curve_integral;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct PSD_config **user_config);

/*! \brief Function that reads the json_t configuration for the `energy_analysis` function.
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

        config->samples_cumulative = NULL;
        config->curve_integral = NULL;

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

    if (user_config) {
        free(user_config);
    }
}

/*! \brief Function that determines the energy and PSD information with the double integration method.
 */
void energy_analysis(const uint16_t *samples,
                     uint32_t samples_number,
                     uint32_t trigger_position,
                     struct event_waveform *waveform,
                     struct event_PSD *event,
                     int8_t *select_event,
                     void *user_config)
{
    struct PSD_config *config = (struct PSD_config*)user_config;

    reallocate_curves(samples_number, &config);

    //const int64_t baseline_end = trigger_position - config->pregate;
    const int64_t baseline_end = config->baseline_samples;

    bool is_error = false;

    if (baseline_end < 1) {
        printf("ERROR: libPSD energy_analysis(): baseline_end is less than one!\n");

        is_error = true;
    } else if (baseline_end > samples_number) {
        printf("ERROR: libPSD energy_analysis(): baseline_end is greater than samples_number!\n");

        is_error = true;
    }

    int64_t short_gate_start = trigger_position - config->short_pregate;
    int64_t long_gate_start = trigger_position - config->long_pregate;

    if (short_gate_start < 0) {
        printf("ERROR: libPSD energy_analysis(): short_gate_start is less than zero!\n");

        short_gate_start = 0;
    } else if (short_gate_start >= samples_number) {
        printf("ERROR: libPSD energy_analysis(): short_gate_start is greater than samples_number!\n");

        short_gate_start = samples_number - 1;
    }

    if (long_gate_start < 0) {
        printf("ERROR: libPSD energy_analysis(): long_gate_start is less than zero!\n");

        long_gate_start = 0;
    } else if (long_gate_start >= samples_number) {
        printf("ERROR: libPSD energy_analysis(): long_gate_start is greater than samples_number!\n");

        long_gate_start = samples_number - 1;
    }

    // N.B. That '-1' is to have the right integration window since,
    // having a discrete domain, the integrals should be considered
    // calculated always up to the right edge of the intervals.
    int64_t short_gate_end = short_gate_start + config->short_gate - 1;
    int64_t long_gate_end = long_gate_start + config->long_gate - 1;

    if (short_gate_end < 0) {
        printf("ERROR: libPSD energy_analysis(): short_gate_end is less than zero!\n");

        short_gate_end = 0;
    } else if (short_gate_end >= samples_number) {
        printf("ERROR: libPSD energy_analysis(): short_gate_end is greater than samples_number!\n");

        short_gate_end = samples_number - 1;
    }

    if (long_gate_end < 0) {
        printf("ERROR: libPSD energy_analysis(): long_gate_end is less than zero!\n");

        long_gate_end = 0;
    } else if (long_gate_end >= samples_number) {
        printf("ERROR: libPSD energy_analysis(): long_gate_end is greater than samples_number!\n");

        long_gate_end = samples_number - 1;
    }

    if (config->is_error || is_error) {
        printf("ERROR: libPSD energy_analysis(): Error status detected\n");

        return;
    }

    cumulative_sum(samples, samples_number, &config->samples_cumulative);

    const uint64_t raw_baseline = config->samples_cumulative[baseline_end - 1];
    const double baseline = (double)raw_baseline / baseline_end;

    integral_baseline_subtract(config->samples_cumulative, samples_number, baseline, &config->curve_integral);

    const double qshort = config->curve_integral[short_gate_end]
                          - config->curve_integral[short_gate_start - 2];
    const double qlong  = config->curve_integral[long_gate_end]
                          - config->curve_integral[long_gate_start - 2];

    const double scaled_qshort = qshort * config->integrals_scaling * (config->pulse_polarity == POLARITY_POSITIVE ? 1.0 : -1.0);
    const double scaled_qlong = qlong * config->integrals_scaling * (config->pulse_polarity == POLARITY_POSITIVE ? 1.0 : -1.0);

    uint64_t long_qshort = (uint64_t)round(scaled_qshort);
    uint64_t long_qlong = (uint64_t)round(scaled_qlong);

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

    const bool PUR = false;

    // Output
    event->timestamp = waveform->timestamp;
    event->qshort = int_qshort;
    event->qlong = int_qlong;
    event->baseline = int_baseline;
    event->channel = waveform->channel;
    event->pur = PUR;

    if (scaled_qlong < config->energy_threshold) {
        (*select_event) = SELECT_FALSE;
    } else {
        const uint8_t initial_additional_number = waveform_additional_get_number(waveform);
        const uint8_t new_additional_number = initial_additional_number + 3;

        waveform_additional_set_number(waveform, new_additional_number);

        uint8_t *additional_gate_short = waveform_additional_get(waveform, initial_additional_number + 0);
        uint8_t *additional_gate_long = waveform_additional_get(waveform, initial_additional_number + 1);
        uint8_t *additional_integral = waveform_additional_get(waveform, initial_additional_number + 2);

        double integral_min = 0;
        double integral_max = 0;
        size_t integral_index_min = 0;
        size_t integral_index_max = 0;

        find_extrema(config->curve_integral, 0, samples_number,
                     &integral_index_min, &integral_index_max,
                     &integral_min, &integral_max);

        const double integral_abs_max = (fabs(integral_max) > fabs(integral_min)) ? fabs(integral_max) : fabs(integral_min);

        const uint8_t ZERO = UINT8_MAX / 2;
        const uint8_t MAX = UINT8_MAX / 2;

        for (uint32_t i = 0; i < samples_number; i++) {
            additional_integral[i] = (config->curve_integral[i] / integral_abs_max) * MAX + ZERO;

            if (short_gate_start <= i && i < short_gate_end)
            {
                additional_gate_short[i] = ZERO + MAX;
            } else {
                additional_gate_short[i] = ZERO;
            }

            if (long_gate_start <= i && i < long_gate_end)
            {
                additional_gate_long[i] = ZERO + MAX;
            } else {
                additional_gate_long[i] = ZERO;
            }
        }

        (*select_event) = SELECT_TRUE;
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
    }
}
