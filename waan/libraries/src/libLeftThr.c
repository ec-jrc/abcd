/*! \brief Determination of the trigger position by employing a threshold
 *         relative to the pulse maximum.
 *
 * Calculation procedure:
 *  1. The signal is smoothed with a recursive running mean.
 *  2. The baseline is determined averaging the first N samples.
 *  3. The pulse is offset by the baseline to center it around zero.
 *  4. The trigger position is determined as the first sample passing the
 *     threshold starting from the maximum going towards left.
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
 * - `fraction`: the threshold value relative to the maximum of the smoothed
 *   signal.
 * - `absolute_threshold`: a threshold applied to the signal before the relative
 *   threshold is calculated, in order to discard signals that contain only
 *   noise. It is relative to the signal baseline.
 *   Optional, default value: 0
 * - `zero_crossing_samples`: the number of samples to be used in the linear
 *   interpolation of the zero-crossing region. Optional, default value: 2
 * - `smooth_samples`: the number of samples to be averaged in the running
 *   mean, rounded to the next odd number. Optional, default value: 1
 * - `fractional_bits`: the number of fractional bits of the timestamp.
 *   Optional, default value: 10
 * - `disable_shift`: disable the bit shift of the timestamp, in order to
 *   recalculate fine timestamps of previously analyzed data.
 *   Optional, default value: false
 * - `disable_LeftThr_gates`: disable the display of the additional waveforms of
 *   the LeftThr calculation.
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
struct LeftThr_config
{
    uint32_t baseline_samples;
    enum pulse_polarity_t pulse_polarity;
    uint32_t smooth_samples;
    double fraction;
    double absolute_threshold;
    uint32_t zero_crossing_samples;
    uint8_t fractional_bits;
    int64_t time_offset;
    bool disable_shift;
    bool disable_LeftThr_gates;

    bool is_error;

    uint32_t previous_samples_number;

    double *curve_samples;
    double *curve_smoothed;
    double *curve_offset;
    double *curve_LeftThr;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct LeftThr_config **user_config);

/*! \brief Function that reads the json_t configuration for the timestamp_analysis function.
 *
 * This function parses a JSON object determining the configuration for the
 * `timestamp_analysis()` function. The configuration is returned as an
 * allocated `struct LeftThr_config`.
 *
 */
void timestamp_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config)) {
        printf("ERROR: libLeftThr timestamp_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
        struct LeftThr_config *config = malloc(1 * sizeof(struct LeftThr_config));

        if (!config) {
            printf("ERROR: libLeftThr timestamp_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }

        config->baseline_samples = json_integer_value(json_object_get(json_config, "baseline_samples"));
        config->fraction = json_number_value(json_object_get(json_config, "fraction"));
        
        if (json_is_number(json_object_get(json_config, "absolute_threshold"))) {
            config->absolute_threshold = json_number_value(json_object_get(json_config, "absolute_threshold"));
        } else {
            config->absolute_threshold = 0;
        }

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

        if (json_is_number(json_object_get(json_config, "zero_crossing_samples"))) {
            config->zero_crossing_samples = json_number_value(json_object_get(json_config, "zero_crossing_samples"));
        } else {
            config->zero_crossing_samples = 2;
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

        if (json_is_boolean(json_object_get(json_config, "disable_LeftThr_gates"))) {
            config->disable_LeftThr_gates = json_is_true(json_object_get(json_config, "disable_LeftThr_gates"));
        } else {
            config->disable_LeftThr_gates = false;
        }

        config->is_error = false;
        config->previous_samples_number = 0;

        config->curve_samples = NULL;
        config->curve_smoothed = NULL;
        config->curve_offset = NULL;
        config->curve_LeftThr = NULL;

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by timestamp_init()
 */
void timestamp_close(void *user_config)
{
    struct LeftThr_config *config = (struct LeftThr_config*)user_config;

    if (config->curve_samples) {
        free(config->curve_samples);
    }
    if (config->curve_smoothed) {
        free(config->curve_smoothed);
    }
    if (config->curve_offset) {
        free(config->curve_offset);
    }
    if (config->curve_LeftThr) {
        free(config->curve_LeftThr);
    }

    if (user_config) {
        free(user_config);
    }
}

/*! \brief Function that determines the trigger position using a Constant Fraction Discriminator algorithm.
 */
void timestamp_analysis(const uint16_t *samples,
                        uint32_t samples_number,
                        struct event_waveform *waveform,
                        uint32_t **trigger_positions,
                        struct event_PSD **events_buffer,
                        size_t *events_number,
                        void *user_config)
{
    struct LeftThr_config *config = (struct LeftThr_config*)user_config;

    //printf("libLeftThr timestamp_analysis(): Reallocating curves with samples number: %" PRIu32 "\n", samples_number);

    reallocate_curves(samples_number, &config);

    bool is_error = false;

    if ((*events_number) != 1) {
        printf("WARNING: libLeftThr timestamp_analysis(): Reallocating buffers\n");

        // Assuring that there is one event_PSD and discarding others
        is_error = !reallocate_buffers(trigger_positions, events_buffer, events_number, 1);
    }

    if (is_error || config->is_error) {
        printf("ERROR: libLeftThr timestamp_analysis(): Error status detected\n");

        return;
    }

    // Get pointers to the first elements in the buffers
    struct event_PSD *this_event = (*events_buffer);
    uint32_t *this_position = (*trigger_positions);

    //printf("libLeftThr timestamp_analysis(): Converting waveform to doubles with samples number: %" PRIu32 "\n", samples_number);

    to_double(samples, samples_number, &config->curve_samples);

    //printf("libLeftThr timestamp_analysis(): Applying a running mean to the double curve with smooth_samples: %" PRIu32 "\n", config->smooth_samples);

    running_mean(config->curve_samples, samples_number, config->smooth_samples, &config->curve_smoothed);

    //printf("libLeftThr timestamp_analysis(): Calculating the baseline\n");

    double baseline = 0;

    calculate_average(config->curve_smoothed, 0, config->baseline_samples, &baseline);

    //printf("libLeftThr timestamp_analysis(): Baseline: %f\n", baseline);

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

    // Checking if there is a signal in the waveform
    if (offset_max < config->absolute_threshold) {
        // There is no signal in the waveform so we clean up the trigger positions
        reallocate_buffers(trigger_positions, events_buffer, events_number, 0);
    } else {
        // There is probably a signal and thus we can apply the relative threshold
        const double threshold_value = config->fraction * offset_max;

        // Offsetting the curve so we can transform the problem to a zero crossing determination
        add_and_multiply_constant(config->curve_offset, samples_number,
                                  -1 * threshold_value, 1.0,
                                  &config->curve_LeftThr);

        uint32_t zero_crossing_index = 0;

        //printf("Starting search\n");
        
        // Look for the threshold crossing starting from the maximum and going
        // toward left.
        // We use an in64_t for the index because we are going toward negative
        // values and if we reach zero then an uint32_t would fold over and it
        // would always be positive.
        for (int64_t index = offset_index_max; index >= 0; index--) {
            //printf("index: %" PRId64 "\n", index);

            if (config->curve_LeftThr[index] < 0) {
                zero_crossing_index = index;
                //printf("Zero crossing index: %" PRId64 "\n", index);
                break;
            }
        }
        //printf("Zero crossing index at the end: %" PRIu32 "\n", zero_crossing_index);

        double fine_zero_crossing = 0;

        // The fine_zero_crossing contains also the zero_crossing_index information
        find_fine_zero_crossing(config->curve_LeftThr, samples_number,
                                zero_crossing_index,
                                config->zero_crossing_samples,
                                &fine_zero_crossing);

        //printf("Fine zero crossing: %f\n", fine_zero_crossing);

        // Converting to fixed-point number
        const uint64_t fine_timestamp = floor(fine_zero_crossing * (1 << config->fractional_bits));

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

        this_event->timestamp = new_timestamp;
        this_event->qshort = 0;
        this_event->qlong = 0;
        this_event->baseline = baseline;
        this_event->channel = waveform->channel;
        this_event->group_counter = 0;

        (*this_position) = zero_crossing_index;

        if (!config->disable_LeftThr_gates) {
            //printf("Generating additionals\n");

            waveform_additional_set_number(waveform, 2);

            unsigned int index = 0;
            uint8_t *additional_LeftThr_signal = waveform_additional_get(waveform, index);
            index++;
            uint8_t *additional_trigger = waveform_additional_get(waveform, index);

            const double LeftThr_max = offset_max - threshold_value;
            const double LeftThr_min = offset_min - threshold_value;
            const double LeftThr_abs_max = (fabs(LeftThr_max) > fabs(LeftThr_min)) ? fabs(LeftThr_max) : fabs(LeftThr_min);

            const uint8_t ZERO = UINT8_MAX / 2;
            const uint8_t MAX = UINT8_MAX / 2;

            for (uint32_t i = 0; i < samples_number; i++) {
                additional_LeftThr_signal[i] = (config->curve_LeftThr[i] / LeftThr_abs_max) * MAX + ZERO;

                if (i == zero_crossing_index) {
                    additional_trigger[i] = ZERO + MAX;
                } else {
                    additional_trigger[i] = ZERO;
                }
            }

            //printf("Done generating additionals\n");
        }
    }
}

void reallocate_curves(uint32_t samples_number, struct LeftThr_config **user_config)
{
    struct LeftThr_config *config = (*user_config);

    //printf("libLeftThr reallocate_curves(): Samples number: %" PRIu32 "; previous: %" PRIu32 "\n", samples_number, config->previous_samples_number);

    if (samples_number != config->previous_samples_number) {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_smoothed = realloc(config->curve_smoothed,
                                             samples_number * sizeof(double));
        double *new_curve_offset = realloc(config->curve_offset,
                                           samples_number * sizeof(double));
        double *new_curve_LeftThr = realloc(config->curve_LeftThr,
                                       samples_number * sizeof(double));

        if (!new_curve_samples) {
            printf("ERROR: libLeftThr reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        } else {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_smoothed) {
            printf("ERROR: libLeftThr reallocate_curves(): Unable to allocate curve_smoothed memory\n");

            config->is_error = true;
        } else {
            config->curve_smoothed = new_curve_smoothed;
        }
        if (!new_curve_offset) {
            printf("ERROR: libLeftThr reallocate_curves(): Unable to allocate curve_offset memory\n");

            config->is_error = true;
        } else {
            config->curve_offset = new_curve_offset;
        }
        if (!new_curve_LeftThr) {
            printf("ERROR: libLeftThr reallocate_curves(): Unable to allocate curve_LeftThr memory\n");

            config->is_error = true;
        } else {
            config->curve_LeftThr = new_curve_LeftThr;
        }
    }

    //printf("libLeftThr reallocate_curves(): Reallocation error: %s\n", (config->is_error ? "true" : "false"));
}
