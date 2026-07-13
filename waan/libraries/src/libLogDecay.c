/*! \brief Determination of the energy information from a pulse, and the decay
 *         time by fitting the log of its tail with multiple linear functions
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the samples before the integration
 *     range.
 *  2. The pulse is offset by the baseline to center it around zero.
 *  3. The pulse is integrated to get its energy.
 *  4. For every linear function in the user-settings:
 *     a. The pulse is smoothed.
 *     b. A tail region is determined and the pulse is offset so this region
 *        is set to zero.
 *     c. In the fitting region the logarithm is applied with a cutoff value.
 *     d. The logarithm is fitted with a straight line.
 *     e. The slope of the line is saved in the qshort.
 *     f. A new event is created with a channel based on the index of the fit
 *        region.
 *
 * This function determines one event_PSD per fitting region.
 *
 * The configuration parameters that are searched in a `json_t` object are:
 *
 * - `baseline_samples`: the number of samples to average to determine the
 *   baseline. The average ends at the beginning of the integration range.
 * - `pulse_polarity`: a string describing the expected pulse polarity, it
 *   can be `positive` or `negative`.
 * - `pregate`: the number of samples before the `trigger_position` that define
 *   the beginning of the integration windows. It may be negative.
 * - `long_gate`: the number of samples of the width of the long integration
 *   window.
 * - `integrals_scaling`: a scaling factor multiplied to the integrals.
 *   Optional, default value: 1
 * - `energy_threshold`: pulses with an energy lower than the threshold are
 *   discared. Optional, default value: 0
 * - `prefits`: an array with the number of samples before the `trigger_position`
 *   that define the beginning of the fitting windows. They may be negative.
 * - `fit_gates`: an array with the number of samples of the width of the fit
 *   integration windows.
 * - `pretails`: an array with the number of samples of the gap between the fit
 *   windows and the tail windows.
 * - `tail_samples`: an array with the number of samples to average to determine
 *   the tail of the fit window.
 * - `smoothings`: an array with the number of samples of the smoothing windows.
 * - `cutoff_value`: the minimum value passed to the logarithm.
 * - `tau_scaling`: a scaling factor multiplied to the decay constant.
 *   Optional, default value: 1
 * - `energy_threshold`: pulses with an energy lower than the threshold are
 *   discared. Optional, default value: 0
 * - `verbosity`: define the verbosity level, output is sent to stdout
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <jansson.h>

#include "events.h"
#include "analysis_functions.h"
#include "DSP_functions.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/*! \brief Sctructure that holds the configuration for the `energy_analysis()` function.
 */
struct LogDecay_config
{

    int64_t baseline_samples;
    int64_t pregate;
    int64_t long_gate;

    size_t max_channels;
    size_t number_of_fits;
    int64_t *prefits;
    int64_t *fit_gates;
    int64_t *pretails;
    int64_t *tail_samples;
    unsigned int *smoothings;

    double *constants;
    double *slopes;

    double cutoff_value;

    enum pulse_polarity_t pulse_polarity;

    double integrals_scaling;
    double tau_scaling;
    double energy_threshold;

    uint32_t previous_samples_number;

    unsigned int verbosity;
    bool is_error;

    double *curve_samples;
    double *curve_offset;
    double *curve_smooth;
    double *curve_to_fit;
    double *curve_log;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct LogDecay_config **user_config);

/*! \brief Function that reads the json_t configuration for the `energy_analysis()` function.
 *
 * This function parses a JSON object determining the configuration for the
 * `energy_analysis()` function. The configuration is returned as an
 * allocated `struct LogDecay_config`.
 */
void energy_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config))
    {
        printf("ERROR: libLogDecay energy_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    }
    else
    {
        struct LogDecay_config *config = calloc(1, sizeof(struct LogDecay_config));

        if (!config)
        {
            printf("ERROR: libLogDecay energy_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }
        else
        {
            json_t *verbosity = json_object_get(json_config, "verbosity");
            config->verbosity = (json_is_number(verbosity)) ? json_number_value(verbosity) : 0;

            json_t *integrals_scaling = json_object_get(json_config, "integrals_scaling");
            config->integrals_scaling = (json_is_number(integrals_scaling)) ? json_number_value(integrals_scaling) : 1.0;

            json_t *tau_scaling = json_object_get(json_config, "tau_scaling");
            config->tau_scaling = (json_is_number(tau_scaling)) ? json_number_value(tau_scaling) : 1.0;

            json_t *max_channels = json_object_get(json_config, "max_channels");
            config->max_channels = (json_is_number(max_channels)) ? json_number_value(max_channels) : 8;

            json_t *cutoff_value = json_object_get(json_config, "cutoff_value");
            config->cutoff_value = (json_is_number(cutoff_value)) ? json_number_value(cutoff_value) : 1e-1;

            config->baseline_samples = json_number_value(json_object_get(json_config, "baseline_samples"));
            config->pregate = json_number_value(json_object_get(json_config, "pregate"));
            config->long_gate = json_number_value(json_object_get(json_config, "long_gate"));

            json_t *prefits = json_object_get(json_config, "prefits");
            json_t *fit_gates = json_object_get(json_config, "fit_gates");
            json_t *pretails = json_object_get(json_config, "pretails");
            json_t *tail_samples = json_object_get(json_config, "tail_samples");
            json_t *smoothings = json_object_get(json_config, "smoothings");

            config->number_of_fits = MIN(MIN(MIN(json_array_size(prefits), json_array_size(fit_gates)), MIN(json_array_size(tail_samples), json_array_size(smoothings))), json_array_size(pretails));

            config->prefits = (int64_t *)calloc(config->number_of_fits, sizeof(int64_t));
            config->fit_gates = (int64_t *)calloc(config->number_of_fits, sizeof(int64_t));
            config->pretails = (int64_t *)calloc(config->number_of_fits, sizeof(int64_t));
            config->tail_samples = (int64_t *)calloc(config->number_of_fits, sizeof(int64_t));
            config->smoothings = (unsigned int *)calloc(config->number_of_fits, sizeof(unsigned int));

            config->constants = (double *)calloc(config->number_of_fits, sizeof(double));
            config->slopes = (double *)calloc(config->number_of_fits, sizeof(double));

            if (!config->prefits)
            {
                printf("ERROR: libLogDecay energy_init(): Unable to allocate prefits memory\n");
            }
            if (!config->fit_gates)
            {
                printf("ERROR: libLogDecay energy_init(): Unable to allocate fit_gates memory\n");
            }
            if (!config->pretails)
            {
                printf("ERROR: libLogDecay energy_init(): Unable to allocate pretails memory\n");
            }
            if (!config->tail_samples)
            {
                printf("ERROR: libLogDecay energy_init(): Unable to allocate tail_samples memory\n");
            }
            if (!config->smoothings)
            {
                printf("ERROR: libLogDecay energy_init(): Unable to allocate smoothings memory\n");
            }
            if (!config->constants)
            {
                printf("ERROR: libLogDecay energy_init(): Unable to allocate constants memory\n");
            }
            if (!config->slopes)
            {
                printf("ERROR: libLogDecay energy_init(): Unable to allocate slopes memory\n");
            }

            for (size_t fit_index = 0; fit_index < config->number_of_fits; fit_index += 1)
            {
                config->prefits[fit_index] = json_number_value(json_array_get(prefits, fit_index));
                config->fit_gates[fit_index] = json_number_value(json_array_get(fit_gates, fit_index));
                config->pretails[fit_index] = json_number_value(json_array_get(pretails, fit_index));
                config->tail_samples[fit_index] = json_number_value(json_array_get(tail_samples, fit_index));
                config->smoothings[fit_index] = json_number_value(json_array_get(smoothings, fit_index));
            }

            config->pulse_polarity = POLARITY_NEGATIVE;

            if (json_is_string(json_object_get(json_config, "pulse_polarity")))
            {
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

            json_t *energy_threshold = json_object_get(json_config, "energy_threshold");
            config->energy_threshold = (json_is_number(energy_threshold)) ? json_number_value(energy_threshold) : 0;

            config->previous_samples_number = 0;

            config->curve_samples = NULL;
            config->curve_offset = NULL;
            config->curve_smooth = NULL;
            config->curve_to_fit = NULL;
            config->curve_log = NULL;

            (*user_config) = (void *)config;
        }
    }
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
    if (!user_config)
    {
        return;
    }

    struct LogDecay_config *config = (struct LogDecay_config *)user_config;

    if (config->prefits)
    {
        free(config->prefits);
    }
    if (config->fit_gates)
    {
        free(config->fit_gates);
    }
    if (config->pretails)
    {
        free(config->pretails);
    }
    if (config->tail_samples)
    {
        free(config->tail_samples);
    }
    if (config->smoothings)
    {
        free(config->smoothings);
    }
    if (config->constants)
    {
        free(config->constants);
    }
    if (config->slopes)
    {
        free(config->slopes);
    }
    if (config->curve_samples)
    {
        free(config->curve_samples);
    }
    if (config->curve_offset)
    {
        free(config->curve_offset);
    }
    if (config->curve_smooth)
    {
        free(config->curve_smooth);
    }
    if (config->curve_to_fit)
    {
        free(config->curve_to_fit);
    }
    if (config->curve_log)
    {
        free(config->curve_log);
    }
    if (user_config)
    {
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
    if (!user_config)
    {
        return;
    }

    struct LogDecay_config *config = (struct LogDecay_config *)user_config;

    reallocate_curves(samples_number, &config);

    if (config->is_error)
    {
        printf("ERROR: libLogDecay energy_analysis(): Error during reallocate_curves()\n;");

        return;
    }

    to_double(samples, samples_number, &config->curve_samples);

    // Preventing segfaults by checking the boundaries
    const int64_t baseline_start = clamp((*trigger_positions)[0] - config->pregate - config->baseline_samples, 0, samples_number - 1);
    const uint32_t baseline_end = clamp(baseline_start + config->baseline_samples, 1, samples_number);

    double baseline = 0;
    calculate_average(config->curve_samples, baseline_start, baseline_end, &baseline);

    if (config->pulse_polarity == POLARITY_POSITIVE)
    {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, 1.0, &config->curve_offset);
    }
    else
    {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, -1.0, &config->curve_offset);
    }

    const int64_t long_gate_start = clamp((*trigger_positions)[0] - config->pregate, 0, samples_number);
    const int64_t long_gate_end = clamp(long_gate_start + config->long_gate, 0, samples_number);

    double qlong = 0;

    calculate_sum(config->curve_offset, long_gate_start, long_gate_end, &qlong);

    const double scaled_qlong = qlong * config->integrals_scaling;

    if (scaled_qlong < config->energy_threshold)
    {
        if (config->verbosity > 1)
        {
            printf("libLogDecay energy_analysis(): Discarding event: qlong: %f, energy: %f;\n", qlong, scaled_qlong);
        }
        // Discard the event
        reallocate_buffers(trigger_positions, events_buffer, events_number, 0);
    }
    else
    {
        // Before resetting we store the timestamp of the event
        const uint64_t event_timestamp = (*events_buffer)[0].timestamp;

        reallocate_buffers(trigger_positions, events_buffer, events_number, config->number_of_fits);

        // Resetting the curve_log so it is ready to generate the additionals
        for (size_t index = 0; index < samples_number; index += 1)
        {
            config->curve_to_fit[index] = 0;
            config->curve_log[index] = 0;
        }

        for (size_t fit_index = 0; fit_index < config->number_of_fits; fit_index += 1)
        {
            const uint8_t new_channel = waveform->channel + (fit_index * config->max_channels);

            const int64_t fit_start = clamp((*trigger_positions)[0] - config->prefits[fit_index], 0, samples_number);
            const int64_t fit_end = clamp(fit_start + config->fit_gates[fit_index], 0, samples_number);
            const int64_t fit_length = fit_end - fit_start;
            const int64_t tail_start = clamp(fit_end + config->pretails[fit_index], 0, samples_number);
            const int64_t tail_end = clamp(tail_start + config->tail_samples[fit_index], 0, samples_number);

            if (config->verbosity > 1)
            {
                printf("libLogDecay energy_analysis(): Fitting region: index: %zu, new_channel: %" PRIu8 "; fit_start: %" PRId64 ";\n", fit_index, new_channel, fit_start);
            }

            running_mean(config->curve_offset, samples_number, config->smoothings[fit_index], &config->curve_smooth);

            double tailline = 0;
            calculate_average(config->curve_smooth, tail_start, tail_end, &tailline);

            for (uint32_t index = fit_start; index < tail_end; index += 1)
            {
                config->curve_to_fit[index] = config->curve_smooth[index];
            }

            for (uint32_t index = fit_start; index < fit_end; index += 1)
            {
                const double value = MAX(config->curve_smooth[index] - tailline, config->cutoff_value);
                config->curve_log[index] = log(value);
            }

            double constant = 0;
            double slope = 0;

            linear_interpolation(config->curve_log + fit_start, fit_length, &constant, &slope);

            config->constants[fit_index] = constant;
            config->slopes[fit_index] = slope;

            const double tau = -1.0 / slope;

            if (config->verbosity > 0)
            {
                printf("libLogDecay energy_analysis(): Linear interpolation results: constant: %f; slope: %f; tau: %f;\n", constant, slope, tau);
            }

            const double scaled_qshort = tau * config->tau_scaling;

            const uint8_t group_counter = 0;

            if (config->verbosity > 0)
            {
                printf("libLogDecay energy_analysis(): Creating output event: new_channel: %" PRIu8 "; energy: %f; tau: %f;\n", new_channel, scaled_qlong, scaled_qshort);
            }

            // This should have already been taken care but we just stored the value
            (*events_buffer)[fit_index].timestamp = event_timestamp;
            (*events_buffer)[fit_index].qshort = clamp_to_uint16(scaled_qshort);
            (*events_buffer)[fit_index].qlong = clamp_to_uint16(scaled_qlong);
            (*events_buffer)[fit_index].baseline = clamp_to_uint16(baseline);
            (*events_buffer)[fit_index].channel = new_channel;
            (*events_buffer)[fit_index].group_counter = group_counter;
        }

        const uint8_t initial_additional_number = waveform_additional_get_number(waveform);

        if (config->verbosity > 1)
        {
            printf("libLogDecay energy_analysis(): creating additional waveforms, initial number: %u\n", (unsigned int)initial_additional_number);
        }

        const uint8_t new_additional_number = initial_additional_number + 6;

        waveform_additional_set_number(waveform, new_additional_number);

        uint8_t *additional_gate_baseline = waveform_additional_get(waveform, initial_additional_number + 0);
        uint8_t *additional_gate_long = waveform_additional_get(waveform, initial_additional_number + 1);
        uint8_t *additional_gate_taillines = waveform_additional_get(waveform, initial_additional_number + 2);
        uint8_t *additional_to_fit = waveform_additional_get(waveform, initial_additional_number + 3);
        uint8_t *additional_log = waveform_additional_get(waveform, initial_additional_number + 4);
        uint8_t *additional_fitted = waveform_additional_get(waveform, initial_additional_number + 5);

        double fit_min = 0;
        double fit_max = 0;
        size_t fit_index_min = 0;
        size_t fit_index_max = 0;

        find_extrema(config->curve_to_fit, 0, samples_number,
                     &fit_index_min, &fit_index_max,
                     &fit_min, &fit_max);

        const double absolute_fit_max = MAX(fabs(fit_max), fabs(fit_min));

        double log_min = 0;
        double log_max = 0;
        size_t log_index_min = 0;
        size_t log_index_max = 0;

        find_extrema(config->curve_log, 0, samples_number,
                     &log_index_min, &log_index_max,
                     &log_min, &log_max);

        const double absolute_log_max = MAX(fabs(log_max), fabs(log_min));

        const uint8_t ZERO = UINT8_MAX / 2;
        const uint8_t MAX_VALUE = UINT8_MAX / 2;

        for (uint32_t i = 0; i < samples_number; i++)
        {
            if (baseline_start <= i && i < baseline_end)
            {
                additional_gate_baseline[i] = ZERO + MAX_VALUE / 4;
            }
            else
            {
                additional_gate_baseline[i] = ZERO;
            }

            if (long_gate_start <= i && i < long_gate_end)
            {
                additional_gate_long[i] = ZERO + MAX_VALUE * 3 / 4;
            }
            else
            {
                additional_gate_long[i] = ZERO;
            }

            additional_to_fit[i] = (config->curve_to_fit[i] / absolute_fit_max) * MAX_VALUE + ZERO;
            additional_log[i] = (config->curve_log[i] / absolute_log_max) * MAX_VALUE + ZERO;
            additional_gate_taillines[i] = ZERO;
            additional_fitted[i] = ZERO;
        }

        for (size_t fit_index = 0; fit_index < config->number_of_fits; fit_index += 1)
        {
            for (uint32_t i = 0; i < samples_number; i++)
            {
                const int64_t fit_start = clamp((*trigger_positions)[0] - config->prefits[fit_index], 0, samples_number);
                const int64_t fit_end = clamp(fit_start + config->fit_gates[fit_index], 0, samples_number);
                const int64_t tail_start = clamp(fit_end + config->pretails[fit_index], 0, samples_number);
                const int64_t tail_end = clamp(tail_start + config->tail_samples[fit_index], 0, samples_number);

                if (tail_start <= i && i < tail_end)
                {
                    additional_gate_taillines[i] += MAX_VALUE / config->number_of_fits;
                }

                if (fit_start <= i && i < fit_end)
                {
                    additional_fitted[i] = (config->constants[fit_index] + config->slopes[fit_index] * (i - fit_start)) / absolute_log_max * MAX_VALUE + ZERO;
                }
            }
        }
    }
}

void reallocate_curves(uint32_t samples_number, struct LogDecay_config **user_config)
{
    struct LogDecay_config *config = (*user_config);

    if (samples_number != config->previous_samples_number)
    {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_offset = realloc(config->curve_offset,
                                           samples_number * sizeof(double));
        double *new_curve_smooth = realloc(config->curve_smooth,
                                           samples_number * sizeof(double));
        double *new_curve_to_fit = realloc(config->curve_to_fit,
                                           samples_number * sizeof(double));
        double *new_curve_log = realloc(config->curve_log,
                                        samples_number * sizeof(double));

        if (!new_curve_samples)
        {
            printf("ERROR: libLogDecay reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_offset)
        {
            printf("ERROR: libLogDecay reallocate_curves(): Unable to allocate curve_offset memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_offset = new_curve_offset;
        }
        if (!new_curve_smooth)
        {
            printf("ERROR: libLogDecay reallocate_curves(): Unable to allocate curve_smooth memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_smooth = new_curve_smooth;
        }
        if (!new_curve_to_fit)
        {
            printf("ERROR: libLogDecay reallocate_curves(): Unable to allocate curve_to_fit memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_to_fit = new_curve_to_fit;
        }
        if (!new_curve_log)
        {
            printf("ERROR: libLogDecay reallocate_curves(): Unable to allocate curve_log memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_log = new_curve_log;
        }
    }
}
