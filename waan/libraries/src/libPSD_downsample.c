/*! \brief Determination of the energy information from a short pulse by
 *         integrating the whole pulse. Pulse Shape Discrimination is obtained
 *         through the double integration method. The waveform is downsampled.
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the samples right before the
 *     integration gates.
 *  2. The pulse is offset by the baseline to center it around zero.
 *  3. The pulse is integrated over a short and a long integration domains.
 *  4. If the event is selected, the waveform is smoothed and downsampled
 *  5. The timestamp of the waveform is set to the timestamp of the event_PSD,
 *     assuming that the timestamp analysis function did its job before.
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
 * - `max_equals`: maximum number of samples with the exact same value, to
 *   discard saturated pulses. Default value: UINT32_MAX (i.e. it is disabled)
 * - `smooth_samples`: the number of samples to be averaged in the running
 *   mean, rounded to the next odd number. Optional, default value: 1
 * - `downsample`: the reduction factor of the number of samples.
 *   Optional, default value: 1
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
struct PSD_config
{
    uint32_t baseline_samples;
    uint32_t smooth_samples;
    uint32_t downsample;
    int64_t short_pregate;
    int64_t long_pregate;
    int64_t short_gate;
    int64_t long_gate;
    enum pulse_polarity_t pulse_polarity;
    double integrals_scaling;
    double energy_threshold;
    double PSD_min;
    double PSD_max;
    uint32_t max_equals;

    bool is_error;

    uint32_t previous_samples_number;

    uint64_t *samples_cumulative;
    double *curve_integral;
    double *curve_samples;
    double *curve_smoothed;
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

    struct PSD_config *config = calloc(1, sizeof(struct PSD_config));

    if (!config)
    {
        printf("ERROR: libPSD energy_init(): Unable to allocate config memory\n");

        return;
    }

    json_t *pregate = json_object_get(json_config, "pregate");

    if (json_is_number(pregate))
    {
        config->short_pregate = json_number_value(pregate);
        config->long_pregate = json_number_value(pregate);
    }

    json_t *short_pregate = json_object_get(json_config, "short_pregate");
    json_t *long_pregate = json_object_get(json_config, "long_pregate");

    if (json_is_number(short_pregate))
    {
        config->short_pregate = json_number_value(short_pregate);
    }
    if (json_is_number(long_pregate))
    {
        config->long_pregate = json_number_value(long_pregate);
    }

    if (json_is_number(short_pregate) || json_is_number(long_pregate))
    {
        json_object_set_nocheck(json_config, "short_pregate", json_integer(config->short_pregate));
        json_object_set_nocheck(json_config, "long_pregate", json_integer(config->long_pregate));
    }
    else
    {
        json_object_set_nocheck(json_config, "pregate", json_integer(config->short_pregate));
    }

    read_config_number(json_config, short_gate, 1, config);
    read_config_number(json_config, long_gate, 2, config);

    read_config_number(json_config, baseline_samples, 1, config);
    read_config_number(json_config, smooth_samples, 1, config);
    read_config_number(json_config, downsample, 1, config);

    read_config_number(json_config, integrals_scaling, 1.0, config);

    read_config_number(json_config, energy_threshold, 0.0, config);

    read_config_number(json_config, PSD_min, -0.1, config);
    read_config_number(json_config, PSD_max, 1.1, config);

    char *pulse_polarities_strs[] = {"negative", "positive"};
    enum pulse_polarity_t pulse_polarities_vals[] = {POLARITY_NEGATIVE, POLARITY_POSITIVE};
    read_config_options(json_config, pulse_polarity, pulse_polarities_strs, pulse_polarities_vals, config);

    read_config_number_overwrite(json_config, max_equals, UINT32_MAX, config, false);

    config->is_error = false;
    config->previous_samples_number = 0;

    config->samples_cumulative = NULL;
    config->curve_integral = NULL;
    config->curve_samples = NULL;
    config->curve_smoothed = NULL;

    (*user_config) = (void *)config;
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
    if (!user_config)
    {
        return;
    }

    struct PSD_config *config = (struct PSD_config *)user_config;

    if (config->samples_cumulative)
    {
        free(config->samples_cumulative);
    }
    if (config->curve_integral)
    {
        free(config->curve_integral);
    }
    if (config->curve_samples)
    {
        free(config->curve_samples);
    }
    if (config->curve_smoothed)
    {
        free(config->curve_smoothed);
    }

    if (user_config)
    {
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
    if (!user_config)
    {
        printf("ERROR: libPSD energy_analysis(): User config not defined, not performing analysis\n");

        return;
    }

    struct PSD_config *config = (struct PSD_config *)user_config;

    reallocate_curves(samples_number, &config);

    bool is_error = false;

    if ((*events_number) != 1)
    {
        // Assuring that there is one event_PSD and discarding others
        is_error = !reallocate_buffers(trigger_positions, events_buffer, events_number, 1);

        if (is_error)
        {
            printf("ERROR: libPSD energy_analysis(): Unable to reallocate buffers\n");
        }
    }

    if (config->is_error || is_error)
    {
        printf("ERROR: libPSD energy_analysis(): Error status detected, not performing analysis\n");

        return;
    }

    const int64_t global_pregate = (config->long_pregate > config->short_pregate) ? config->long_pregate : config->short_pregate;

    // We calculate everything relative this `local_start` to advance the pointer
    // to the samples until the beginning of the baseline.
    const int64_t local_start = clamp((*trigger_positions)[0] - global_pregate - config->baseline_samples, 0, samples_number - 1);

    const int64_t local_trigger_position = clamp((*trigger_positions)[0] - local_start, 0, samples_number);
    const int64_t local_samples_number = clamp(samples_number - local_start, 0, samples_number);

    // We do not use `baseline_samples` here because the end of the baseline
    // should be the beginning of the integration gates. If we'd be using
    // `baseline_samples` here and the beginning of the baseline is before the
    // beginning of the waveform, we would have a baseline that overlaps with
    // the integration gates.
    const int64_t baseline_end = clamp(local_trigger_position - global_pregate, 1, local_samples_number);

    const int64_t short_gate_start = clamp(local_trigger_position - config->short_pregate, 1, local_samples_number - 1);
    const int64_t long_gate_start = clamp(local_trigger_position - config->long_pregate, 1, local_samples_number - 1);
    // N.B. That '-1' is to have the right integration window since,
    // having a discrete domain, the integrals should be considered
    // calculated always up to the right edge of the intervals.
    const int64_t short_gate_end = clamp(short_gate_start + config->short_gate - 1, 0, local_samples_number - 1);
    const int64_t long_gate_end = clamp(long_gate_start + config->long_gate - 1, 0, local_samples_number - 1);

    // Checking if there are multiple subsequent samples with the exact same
    // value. If it is the case, then probably the waveform was cut by
    // saturation.
    uint16_t previous_sample = samples[0];
    uint32_t counter_equals = 1;
    bool saturation_detected = false;

    for (uint32_t index = 1; index < samples_number; index += 1)
    {
        if (samples[index] == previous_sample)
        {
            counter_equals += 1;

            if (counter_equals > config->max_equals)
            {
                saturation_detected = true;

                break;
            }
        }
        else
        {
            previous_sample = samples[index];
            counter_equals = 1;
        }
    }

    cumulative_sum(samples + local_start, local_samples_number, &config->samples_cumulative);

    const double baseline = (double)(config->samples_cumulative[baseline_end - 1]) / baseline_end;

    integral_baseline_subtract(config->samples_cumulative, local_samples_number, baseline, &config->curve_integral);

    const double qshort = config->curve_integral[short_gate_end] - config->curve_integral[short_gate_start - 2];
    const double qlong = config->curve_integral[long_gate_end] - config->curve_integral[long_gate_start - 2];

    const double PSD = (qlong != 0) ? (qlong - qshort) / qlong : (config->PSD_min - 1);

    const double scaled_qshort = qshort * config->integrals_scaling * (config->pulse_polarity == POLARITY_POSITIVE ? 1.0 : -1.0);
    const double scaled_qlong = qlong * config->integrals_scaling * (config->pulse_polarity == POLARITY_POSITIVE ? 1.0 : -1.0);

    const uint8_t group_counter = 0;

    if (scaled_qlong < config->energy_threshold || PSD < config->PSD_min || PSD > config->PSD_max || saturation_detected)
    {
        // Discard the event
        reallocate_buffers(trigger_positions, events_buffer, events_number, 0);
    }
    else
    {
        // Downsampling is performed only if the waveform is selected, because
        // it is of interest only when we are actually fowarding it
        to_double(samples, samples_number, &config->curve_samples);
        running_mean(config->curve_samples, samples_number, config->smooth_samples, &config->curve_smoothed);

        // Output
        // We have to assume that this was taken care earlier
        //(*events_buffer)[0].timestamp = waveform->timestamp;
        (*events_buffer)[0].qshort = clamp_to_uint16(scaled_qshort);
        (*events_buffer)[0].qlong = clamp_to_uint16(scaled_qlong);
        (*events_buffer)[0].baseline = clamp_to_uint16(baseline);
        (*events_buffer)[0].channel = waveform->channel;
        (*events_buffer)[0].group_counter = group_counter;

        waveform_samples_set_number(waveform, samples_number / config->downsample);

        // Since we are changing the samples of the waveform the previous
        // additionals will become useless. We should somehow store them and
        // then recover them, but this is a lot of work to do for just a
        // debugging information. We will just overwrite them.
        waveform_additional_set_number(waveform, 5);

        // We set the timestamp of the waveform from the event_PSD as the timing
        // information will be partially lost, due to the downsampling.
        // ...hoping that the previous timestamp analysis function did its job
        waveform->timestamp = (*events_buffer)[0].timestamp;

        uint16_t *samples_new = waveform_samples_get(waveform);

        uint8_t *additional_smoothed = waveform_additional_get(waveform, 0);
        uint8_t *additional_gate_short = waveform_additional_get(waveform, 1);
        uint8_t *additional_gate_long = waveform_additional_get(waveform, 2);
        uint8_t *additional_gate_baseline = waveform_additional_get(waveform, 3);
        uint8_t *additional_integral = waveform_additional_get(waveform, 4);

        double integral_min = 0;
        double integral_max = 0;
        size_t integral_index_min = 0;
        size_t integral_index_max = 0;

        find_extrema(config->curve_integral, 0, local_samples_number,
                     &integral_index_min, &integral_index_max,
                     &integral_min, &integral_max);

        const double integral_abs_max = (fabs(integral_max) > fabs(integral_min)) ? fabs(integral_max) : fabs(integral_min);

        double smoothed_min = 0;
        double smoothed_max = 0;
        size_t smoothed_index_min = 0;
        size_t smoothed_index_max = 0;

        find_extrema(config->curve_smoothed, 0, samples_number,
                     &smoothed_index_min, &smoothed_index_max,
                     &smoothed_min, &smoothed_max);

        // The curve has an offset (baseline) so we move it to zero, because it
        // is only for display purposes
        const double smoothed_abs_max = (fabs(smoothed_max - baseline) > fabs(smoothed_min - baseline)) ? fabs(smoothed_max - baseline) : fabs(smoothed_min - baseline);

        const uint8_t ZERO = UINT8_MAX / 2;
        const uint8_t MAX = UINT8_MAX / 2;

        for (uint32_t global_i = 0; global_i < samples_number / config->downsample; global_i++)
        {
            const uint32_t local_i = global_i * config->downsample;

            additional_smoothed[global_i] = ((config->curve_smoothed[local_i] - baseline) / smoothed_abs_max) * MAX + ZERO;
            samples_new[global_i] = config->curve_smoothed[local_i];
        }

        for (uint32_t global_i = 0; global_i < local_start / config->downsample; global_i++)
        {
            additional_integral[global_i] = ZERO;
            additional_gate_baseline[global_i] = ZERO;
            additional_gate_short[global_i] = ZERO;
            additional_gate_long[global_i] = ZERO;
        }

        for (uint32_t global_i = local_start / config->downsample; global_i < samples_number / config->downsample; global_i++)
        {
            const uint32_t local_i = (global_i - (local_start / config->downsample)) * config->downsample;

            additional_integral[global_i] = (config->curve_integral[local_i] / integral_abs_max) * MAX + ZERO;

            if (local_i < baseline_end)
            {
                additional_gate_baseline[global_i] = ZERO + MAX / 2;
            }
            else
            {
                additional_gate_baseline[global_i] = ZERO;
            }

            if (short_gate_start <= local_i && local_i < short_gate_end)
            {
                additional_gate_short[global_i] = ZERO + MAX;
            }
            else
            {
                additional_gate_short[global_i] = ZERO;
            }

            if (long_gate_start <= local_i && local_i < long_gate_end)
            {
                additional_gate_long[global_i] = ZERO + MAX;
            }
            else
            {
                additional_gate_long[global_i] = ZERO;
            }
        }
    }
}

void reallocate_curves(uint32_t samples_number, struct PSD_config **user_config)
{
    struct PSD_config *config = (*user_config);

    if (samples_number != config->previous_samples_number)
    {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        uint64_t *new_samples_cumulative = realloc(config->samples_cumulative,
                                                   samples_number * sizeof(uint64_t));
        double *new_curve_integral = realloc(config->curve_integral,
                                             samples_number * sizeof(double));
        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_smoothed = realloc(config->curve_smoothed,
                                             samples_number * sizeof(double));

        if (!new_samples_cumulative)
        {
            printf("ERROR: libPSD reallocate_curves(): Unable to allocate samples_cumulative memory\n");

            config->is_error = true;
        }
        else
        {
            config->samples_cumulative = new_samples_cumulative;
        }
        if (!new_curve_integral)
        {
            printf("ERROR: libPSD reallocate_curves(): Unable to allocate curve_integral memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_integral = new_curve_integral;
        }
        if (!new_curve_samples)
        {
            printf("ERROR: libPSD reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_smoothed)
        {
            printf("ERROR: libPSD reallocate_curves(): Unable to allocate curve_smoothed memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_smoothed = new_curve_smoothed;
        }
    }
}
