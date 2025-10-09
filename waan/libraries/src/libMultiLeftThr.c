/*! \brief Determination of the trigger positions of mulitple pulses per
 *         per waveform by employing a Leading Edge (LE) discriminator to detect
 *         the pulses, with a threshold relative to the baseline. The precise
 *         timing information is determined by employing a threshold relative to
 *         the maximum.
 *
 * Calculation procedure:
 *  1. The signal is smoothed with a recursive running mean.
 *  2. The baseline is determined averaging the first N samples.
 *  3. The pulse is offset by the baseline to center it around zero.
 *  4. Pulses are detected with the LE.
 *  5. For each detected the precise trigger position is determined as the first
 *     sample passing the threshold starting from the local maximum going
 *     towards left.
 *
 * The trigger position is added to time timestamp provided by the digitizer
 * in order to determine the pulse absolute time. The trigger position is
 * determined with a resolution better than the clock step, the fractional part
 * is added to the timestamp by shifting it by a user-configurable number of
 * bits.
 *
 * The configuration parameters that are searched in a `json_t` object are:
 *
 * - `baseline_samples`: the number of samples to average to determine the
 *   baseline. The average starts from the beginning of the waveform.
 * - `pulse_polarity`: a string describing the expected pulse polarity, it
 *   can be `positive` or `negative`.
 * - `threshold`: the absolute threshold value relative to the baseline used by
 *   the LE to detect pulses.
 * - `fraction`: the threshold value relative to the local maximum of each pulse
 *   of the smoothed signal.
 * - `threshold_crossing_samples`: the number of samples to be used in the linear
 *   interpolation of the threshold-crossing region. Optional, default value: 2
 * - `smooth_samples`: the number of samples to be averaged in the running
 *   mean, rounded to the next odd number. Optional, default value: 1
 * - `fractional_bits`: the number of fractional bits of the timestamp.
 *   Optional, default value: 10
 * - `disable_shift`: disable the bit shift of the timestamp, in order to
 *   recalculate fine timestamps of previously analyzed data.
 *   Optional, default value: false
 * - `disable_MultiLeftThr_gates`: disable the display of the additional waveforms of
 *   the MultiLeftThr calculation.
 *   Optional, default value: false
 * - `time_offset`: value added to the timestamp after the determination, to
 *   center the signals on the ToF distribution.
 *   Optional, default value: 0
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

/*! \brief Sctructure that holds the configuration for the `timestamp_analysis` function.
 */
struct MultiLeftThr_config
{
    uint32_t baseline_samples;
    enum pulse_polarity_t pulse_polarity;
    uint32_t smooth_samples;
    double fraction;
    double threshold;
    uint32_t threshold_crossing_samples;
    uint8_t fractional_bits;
    int64_t time_offset;
    bool disable_shift;
    bool disable_MultiLeftThr_gates;

    bool is_error;

    uint32_t previous_samples_number;

    double *curve_samples;
    double *curve_smoothed;
    double *curve_offset;
    double *curve_LE;
    double *thresholds;
    uint32_t *triggers_rising;
    uint32_t *triggers_falling;
    uint32_t *thresholds_crossings;
    uint32_t *local_maxima;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct MultiLeftThr_config **user_config);

/*! \brief Function that reads the json_t configuration for the timestamp_analysis function.
 *
 * This function parses a JSON object determining the configuration for the
 * `timestamp_analysis()` function. The configuration is returned as an
 * allocated `struct MultiLeftThr_config`.
 *
 */
void timestamp_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config)) {
        printf("ERROR: libMultiLeftThr timestamp_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
        struct MultiLeftThr_config *config = malloc(1 * sizeof(struct MultiLeftThr_config));

        if (!config) {
            printf("ERROR: libMultiLeftThr timestamp_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }

        config->baseline_samples = json_integer_value(json_object_get(json_config, "baseline_samples"));
        config->fraction = json_number_value(json_object_get(json_config, "fraction"));
        config->threshold = json_number_value(json_object_get(json_config, "threshold"));


        if (json_is_number(json_object_get(json_config, "smooth_samples"))) {
            const unsigned int W = json_number_value(json_object_get(json_config, "smooth_samples"));
            // Rounding it to the next greater odd number
            config->smooth_samples = floor(W / 2) * 2 + 1;
        } else {
            config->smooth_samples = 1;
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

        if (json_is_number(json_object_get(json_config, "threshold_crossing_samples"))) {
            config->threshold_crossing_samples = json_number_value(json_object_get(json_config, "threshold_crossing_samples"));
        } else {
            config->threshold_crossing_samples = 2;
        }

        if (json_is_number(json_object_get(json_config, "fractional_bits"))) {
            config->fractional_bits = json_number_value(json_object_get(json_config, "fractional_bits"));
        } else {
            config->fractional_bits = 10;
        }

        if (json_is_number(json_object_get(json_config, "time_offset"))) {
            config->time_offset = json_number_value(json_object_get(json_config, "time_offset"));
        } else {
            config->time_offset = 0;
        }

        if (json_is_boolean(json_object_get(json_config, "disable_shift"))) {
            config->disable_shift = json_is_true(json_object_get(json_config, "disable_shift"));
        } else {
            config->disable_shift = false;
        }

        if (json_is_boolean(json_object_get(json_config, "disable_MultiLeftThr_gates"))) {
            config->disable_MultiLeftThr_gates = json_is_true(json_object_get(json_config, "disable_MultiLeftThr_gates"));
        } else {
            config->disable_MultiLeftThr_gates = false;
        }

        config->is_error = false;
        config->previous_samples_number = 0;

        config->curve_samples = NULL;
        config->curve_smoothed = NULL;
        config->curve_offset = NULL;
        config->curve_LE = NULL;
        config->thresholds = NULL;
        config->triggers_rising = NULL;
        config->triggers_falling = NULL;
        config->thresholds_crossings = NULL;
        config->local_maxima = NULL;

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by timestamp_init()
 */
void timestamp_close(void *user_config)
{
    struct MultiLeftThr_config *config = (struct MultiLeftThr_config*)user_config;

    if (config->curve_samples) {
        free(config->curve_samples);
    }
    if (config->curve_smoothed) {
        free(config->curve_smoothed);
    }
    if (config->curve_offset) {
        free(config->curve_offset);
    }
    if (config->curve_LE) {
        free(config->curve_LE);
    }
    if (config->thresholds) {
        free(config->thresholds);
    }
    if (config->triggers_rising) {
        free(config->triggers_rising);
    }
    if (config->triggers_falling) {
        free(config->triggers_falling);
    }
    if (config->thresholds_crossings) {
        free(config->thresholds_crossings);
    }
    if (config->local_maxima) {
        free(config->local_maxima);
    }

    if (user_config) {
        free(user_config);
    }
}

void timestamp_analysis(const uint16_t *samples,
                        uint32_t samples_number,
                        struct event_waveform *waveform,
                        uint32_t **trigger_positions,
                        struct event_PSD **events_buffer,
                        size_t *events_number,
                        void *user_config)
{
    struct MultiLeftThr_config *config = (struct MultiLeftThr_config*)user_config;

    reallocate_curves(samples_number, &config);

    if (config->is_error) {
        printf("ERROR: libMultiLeftThr timestamp_analysis(): Error status detected\n");

        return;
    }

    to_double(samples, samples_number, &config->curve_samples);

    running_mean(config->curve_samples, samples_number, config->smooth_samples, &config->curve_smoothed);

    double baseline = 0;

    calculate_average(config->curve_smoothed, 0, config->baseline_samples, &baseline);

    if (config->pulse_polarity == POLARITY_POSITIVE) {
        add_and_multiply_constant(config->curve_smoothed, samples_number, -1 * baseline, 1.0, &config->curve_offset);
    } else {
        add_and_multiply_constant(config->curve_smoothed, samples_number, -1 * baseline, -1.0, &config->curve_offset);
    }

    double offset_min = 0;
    double offset_max = 0;
    size_t offset_index_min = 0;
    size_t offset_index_max = 0;

    find_extrema(config->curve_offset, 0, samples_number,
                 &offset_index_min, &offset_index_max,
                 &offset_min, &offset_max);

    // Offsetting the curve so we can transform the problem to a zero crossing determination
    add_and_multiply_constant(config->curve_offset, samples_number,
                              -1 * config->threshold, 1.0,
                              &config->curve_LE);

    // Using a simple Finite State Machine to look for the zero crossings
    const int STATE_BELOW_THRESHOLD = 0;
    const int STATE_ABOVE_THRESHOLD = 1;

    // WARNING: Here we are assuming that the samples_number >= 1
    unsigned int state = (config->curve_LE[0] < 0) ? STATE_BELOW_THRESHOLD : STATE_ABOVE_THRESHOLD;

    double local_maximum = 0;
    uint32_t local_maximum_index = 0;
    double local_threshold = 0;

    uint32_t triggers_rising_counter = 0;
    uint32_t triggers_falling_counter = 0;
    uint32_t thresholds_crossings_counter = 0;

    for (uint32_t index = 1; index < samples_number; index++) {
        if (state == STATE_BELOW_THRESHOLD) {
            if (config->curve_LE[index] >= 0) {
                state = STATE_ABOVE_THRESHOLD;
                config->triggers_rising[triggers_rising_counter] = index - 1;
                triggers_rising_counter += 1;

                local_maximum = config->curve_LE[index];
                local_maximum_index = index;
                local_threshold = local_maximum * config->fraction;
            }
        } else if (state == STATE_ABOVE_THRESHOLD) {
            if (config->curve_LE[index] > local_maximum) {
                local_maximum = config->curve_LE[index];
                local_maximum_index = index;
                local_threshold = local_maximum * config->fraction;
            }

            if (config->curve_LE[index] < 0) {
                state = STATE_BELOW_THRESHOLD;
                config->triggers_falling[triggers_falling_counter] = index - 1;
                triggers_falling_counter += 1;

                config->local_maxima[thresholds_crossings_counter] = local_maximum_index;
                config->thresholds[thresholds_crossings_counter] = local_threshold;

                // Look for the threshold crossing starting from the maximum and going
                // towards left.
                // We use an in64_t for the index because we are going toward negative
                // values and if we reach zero then an uint32_t would fold over and it
                // would always be positive.
                for (int64_t local_index = local_maximum_index; local_index >= 0; local_index--) {
                    if ((config->curve_LE[local_index] - local_threshold) < 0) {
                        config->thresholds_crossings[thresholds_crossings_counter] = local_index;
                        thresholds_crossings_counter += 1;
                        break;
                    }
                }
            }
        }
    }

    if ((*events_number) != thresholds_crossings_counter) {
        reallocate_buffers(trigger_positions, events_buffer, events_number, thresholds_crossings_counter);
    }

    memcpy((*trigger_positions), config->thresholds_crossings, thresholds_crossings_counter * sizeof(uint32_t));

    for (uint32_t index = 0; index < thresholds_crossings_counter; index++) {
        double fine_threshold_crossing = 0;

        // The fine_zero_crossing contains also the zero_crossing_index information
        find_fine_threshold_crossing(config->curve_LE, samples_number,
                                     config->thresholds[index],
                                     config->thresholds_crossings[index],
                                     config->threshold_crossing_samples,
                                     &fine_threshold_crossing);

        // Converting to fixed-point number
        const uint64_t fine_timestamp = floor(fine_threshold_crossing * (1 << config->fractional_bits));

        uint64_t new_timestamp = fine_timestamp;

        // Bitmask to delete the last fractional_bits in the uint64_t numbers
        const uint64_t bitmask = UINT64_MAX - ((1 << config->fractional_bits) - 1);

        if (config->disable_shift) {
            new_timestamp += (waveform->timestamp & bitmask);
        } else {
            new_timestamp += ((waveform->timestamp << config->fractional_bits) & bitmask);
        }

        new_timestamp += config->time_offset;

        // Output

        (*events_buffer)[index].timestamp = new_timestamp;
        (*events_buffer)[index].qshort = 0;
        (*events_buffer)[index].qlong = 0;
        (*events_buffer)[index].baseline = baseline;
        (*events_buffer)[index].channel = waveform->channel;
        (*events_buffer)[index].group_counter = 0;
    }

    if (!config->disable_MultiLeftThr_gates) {
        waveform_additional_set_number(waveform, 4);

        unsigned int index = 0;
        uint8_t *additional_baseline_gate = waveform_additional_get(waveform, index);
        index++;
        uint8_t *additional_LE_signal = waveform_additional_get(waveform, index);
        index++;
        uint8_t *additional_triggers = waveform_additional_get(waveform, index);
        index++;
        uint8_t *additional_relative_thresholds_crossings = waveform_additional_get(waveform, index);

        const double LE_max = offset_max - config->threshold;
        const double LE_min = offset_min - config->threshold;
        const double LE_abs_max = (fabs(LE_max) > fabs(LE_min)) ? fabs(LE_max) : fabs(LE_min);

        const uint8_t ZERO = UINT8_MAX / 2;
        const uint8_t MAX = UINT8_MAX / 2;

        for (uint32_t i = 0; i < samples_number; i++) {
            additional_baseline_gate[i] = ZERO;
            additional_LE_signal[i] = (config->curve_LE[i] / LE_abs_max) * MAX + ZERO;
            additional_triggers[i] = ZERO;
            additional_relative_thresholds_crossings[i] = ZERO;
        }

        for (uint32_t i = 0; i < config->baseline_samples; i++) {
            additional_baseline_gate[i] = ZERO + MAX / 2;
        }

        for (uint32_t i = 0; i < triggers_rising_counter; i++) {
            additional_triggers[config->triggers_rising[i]] = ZERO + MAX;
        }
        for (uint32_t i = 0; i < triggers_falling_counter; i++) {
            additional_triggers[config->triggers_falling[i]] = ZERO - MAX;
        }
        for (uint32_t i = 0; i < thresholds_crossings_counter; i++) {
            additional_relative_thresholds_crossings[config->thresholds_crossings[i]] = ZERO + MAX / 2;
            additional_relative_thresholds_crossings[config->local_maxima[i]] = ZERO + MAX;
        }
    }
}

void reallocate_curves(uint32_t samples_number, struct MultiLeftThr_config **user_config)
{
    struct MultiLeftThr_config *config = (*user_config);

    if (samples_number != config->previous_samples_number) {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_smoothed = realloc(config->curve_smoothed,
                                             samples_number * sizeof(double));
        double *new_curve_offset = realloc(config->curve_offset,
                                           samples_number * sizeof(double));
        double *new_curve_LE = realloc(config->curve_LE,
                                       samples_number * sizeof(double));
        double *new_thresholds = realloc(config->thresholds,
                                       samples_number * sizeof(double));
        uint32_t *new_triggers_rising = realloc(config->triggers_rising,
                                                samples_number * sizeof(uint32_t));
        uint32_t *new_triggers_falling = realloc(config->triggers_falling,
                                                 samples_number * sizeof(uint32_t));
        uint32_t *new_thresholds_crossings = realloc(config->thresholds_crossings,
                                                     samples_number * sizeof(uint32_t));
        uint32_t *new_local_maxima = realloc(config->local_maxima,
                                             samples_number * sizeof(uint32_t));

        if (!new_curve_samples) {
            printf("ERROR: libMultiLeftThr reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        } else {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_smoothed) {
            printf("ERROR: libMultiLeftThr reallocate_curves(): Unable to allocate curve_smoothed memory\n");

            config->is_error = true;
        } else {
            config->curve_smoothed = new_curve_smoothed;
        }
        if (!new_curve_offset) {
            printf("ERROR: libMultiLeftThr reallocate_curves(): Unable to allocate curve_offset memory\n");

            config->is_error = true;
        } else {
            config->curve_offset = new_curve_offset;
        }
        if (!new_curve_LE) {
            printf("ERROR: libMultiLeftThr reallocate_curves(): Unable to allocate curve_LE memory\n");

            config->is_error = true;
        } else {
            config->curve_LE = new_curve_LE;
        }
        if (!new_thresholds) {
            printf("ERROR: libMultiLeftThr reallocate_curves(): Unable to allocate thresholds memory\n");

            config->is_error = true;
        } else {
            config->thresholds = new_thresholds;
        }
        if (!new_triggers_rising) {
            printf("ERROR: libMultiLeftThr reallocate_curves(): Unable to allocate triggers_rising memory\n");

            config->is_error = true;
        } else {
            config->triggers_rising = new_triggers_rising;
        }
        if (!new_triggers_falling) {
            printf("ERROR: libMultiLeftThr reallocate_curves(): Unable to allocate triggers_falling memory\n");

            config->is_error = true;
        } else {
            config->triggers_falling = new_triggers_falling;
        }
        if (!new_thresholds_crossings) {
            printf("ERROR: libMultiLeftThr reallocate_curves(): Unable to allocate thresholds_crossings memory\n");

            config->is_error = true;
        } else {
            config->thresholds_crossings = new_thresholds_crossings;
        }
        if (!new_local_maxima) {
            printf("ERROR: libMultiLeftThr reallocate_curves(): Unable to allocate local_maxima memory\n");

            config->is_error = true;
        } else {
            config->local_maxima = new_local_maxima;
        }
    }
}
