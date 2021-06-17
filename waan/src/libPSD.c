#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "events.h"
#include "analysis_functions.h"
#include "DSP_functions.h"

/*! \brief Sctructure that holds the configuration for the `energy_analysis` function.
 *
 * This function parses a JSON object determining the configuration for the
 * `energy_analysis` function.
 */
struct PSD_config
{
    uint32_t pregate;
    uint32_t short_gate;
    uint32_t long_gate;
    enum pulse_polarity_t pulse_polarity;
    double integrals_scaling;
};

/*! \brief Function that reads the json_t configuration for the `energy_analysis` function.
 *
 * This function parses a JSON object determining the configuration for the
 * `energy_analysis()` function. The configuration is returned as an
 * allocated `struct PSD_config`.
 *
 * The parameters that are searched in the json_t object are:
 *
 * - "pulse_polarity": a string describing the expected pulse polarity, it
 *   can be "positive" or "negative".
 * - "pregate": the number of samples before the trigger_position that define
 *   the beginning of the integration windows.
 * - "short_gate": the number of samples of the width of the short integration
 *   window.
 * - "long_gate": the number of samples of the width of the long integration
 *   window.
 * - "integrals_scaling": a scaling factor multiplied to both the integrals.
 *   Optional, default value: 1
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

        config->pregate = json_integer_value(json_object_get(json_config, "pregate"));
        config->short_gate = json_integer_value(json_object_get(json_config, "short_gate"));
        config->long_gate = json_integer_value(json_object_get(json_config, "long_gate"));

        if (json_is_number(json_object_get(json_config, "integrals_scaling"))) {
            config->integrals_scaling = json_number_value(json_object_get(json_config, "integrals_scaling"));
        } else {
            config->integrals_scaling = 1;
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

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
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

    bool is_error = false;

    uint64_t *integral_samples = malloc(samples_number * sizeof(uint64_t));
    double *integral_curve = malloc(samples_number * sizeof(double));

    if (!integral_samples) {
        printf("ERROR: libPSD energy_analysis(): Unable to allocate integral_samples memory\n");

        is_error = true;
    }
    if (!integral_curve) {
        printf("ERROR: libPSD energy_analysis(): Unable to allocate integral_curve memory\n");

        is_error = true;
    }

    // N.B. That '-1' is to have the right integration window since,
    // having a discrete domain, the integrals should be considered
    // calculated always up to the right edge of the intervals.
    const int64_t baseline_end = trigger_position - config->pregate;
    const int64_t short_gate_end = baseline_end + config->short_gate - 1;
    const int64_t long_gate_end = baseline_end + config->long_gate - 1;

    if (baseline_end < 1) {
        printf("ERROR: libPSD energy_analysis(): baseline_end is less than one!\n");

        is_error = true;
    } else if (baseline_end > samples_number) {
        printf("ERROR: libPSD energy_analysis(): baseline_end is greater than samples_number!\n");

        is_error = true;
    }

    if (short_gate_end < 0) {
        printf("ERROR: libPSD energy_analysis(): short_gate_end is less than zero!\n");

        is_error = true;
    } else if (short_gate_end >= samples_number) {
        printf("ERROR: libPSD energy_analysis(): short_gate_end is greater than samples_number!\n");

        is_error = true;
    }

    if (long_gate_end < 0) {
        printf("ERROR: libPSD energy_analysis(): long_gate_end is less than zero!\n");

        is_error = true;
    } else if (long_gate_end >= samples_number) {
        printf("ERROR: libPSD energy_analysis(): long_gate_end is greater than samples_number!\n");

        is_error = true;
    }

    if (is_error) {
        if (integral_samples) {
            free(integral_samples);
        }
        if (integral_curve) {
            free(integral_curve);
        }

        return;
    }

    cumulative_sum(samples, samples_number, &integral_samples);

    const uint64_t raw_baseline = integral_samples[baseline_end - 1];
    const double baseline = (double)raw_baseline / baseline_end;

    integral_baseline_subtract(integral_samples, samples_number, baseline, &integral_curve);

    const double qshort = integral_curve[short_gate_end]
                          - integral_curve[baseline_end - 2];
    const double qlong  = integral_curve[long_gate_end]
                          - integral_curve[baseline_end - 2];

    const double scaled_qshort = qshort * config->integrals_scaling;
    const double scaled_qlong = qlong * config->integrals_scaling;

    uint64_t long_qshort = 0;
    uint64_t long_qlong = 0;

    if (config->pulse_polarity == POLARITY_POSITIVE)
    {
        long_qshort = (uint64_t)round(scaled_qshort);
        long_qlong = (uint64_t)round(scaled_qlong);
    } else {
        long_qshort = (uint64_t)round(scaled_qshort * -1);
        long_qlong = (uint64_t)round(scaled_qlong * -1);
    }

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

    const uint8_t initial_additional_number = waveform_additional_get_number(waveform);
    const uint8_t new_additional_number = initial_additional_number + 2;

    waveform_additional_set_number(waveform, new_additional_number);

    uint8_t *additional_gate_short = waveform_additional_get(waveform, initial_additional_number + 0);
    uint8_t *additional_gate_long = waveform_additional_get(waveform, initial_additional_number + 1);

    const uint8_t ZERO = UINT8_MAX / 2;
    const uint8_t MAX = UINT8_MAX / 2;

    for (uint32_t i = 0; i < samples_number; i++) {
        if (baseline_end <= i && i < baseline_end + config->short_gate)
        {
            additional_gate_short[i] = ZERO + MAX;
        } else {
            additional_gate_short[i] = ZERO;
        }

        if (baseline_end <= i && i < baseline_end + config->long_gate)
        {
            additional_gate_long[i] = ZERO + MAX;
        } else {
            additional_gate_long[i] = ZERO;
        }
    }

    (*select_event) = SELECT_TRUE;

    // Cleanup
    if (integral_samples) {
        free(integral_samples);
    }
    if (integral_curve) {
        free(integral_curve);
    }
}
