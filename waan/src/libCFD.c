#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "events.h"
#include "analysis_functions.h"
#include "DSP_functions.h"

/*! \brief Sctructure that holds the configuration for the `timestamp_analysis` function.
 */
struct CFD_config
{
    uint32_t baseline_samples;
    uint32_t smooth_samples;
    double fraction;
    int32_t delay;
    uint32_t zero_crossing_samples;
    uint8_t fractional_bits;
    bool disable_shift;
    bool disable_CFD_gates;

    bool is_error;

    uint32_t previous_samples_number;

    double *curve_smoothed;
    double *curve_centered;
    double *curve_CFD;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct CFD_config **user_config);

/*! \brief Function that reads the json_t configuration for the timestamp_analysis function.
 *
 * This function parses a JSON object determining the configuration for the
 * `timestamp_analysis()` function. The configuration is returned as an
 * allocated `struct CFD_config`.
 *
 * The parameters that are searched in the json_t object are:
 *
 * - "baseline_samples": the number of samples to be used in the baseline
 *   evaluation.
 * - "fraction": the multiplication factor of the signal that is to be summed
 *   to the delayed version of the signal itself.
 * - "delay": the delay of the signal in terms of clock samples.
 * - "zero_crossing_samples": the number of samples to be used in the linear
 *   interpolation of the zero-crossing region. Optional, default value: 2
 * - "smooth_samples": the number of samples to be averaged in the running
 *   mean, rounded to the next odd number. Optional, default value: 1
 * - "fractional_bits": the number of fractional bits of the timestamp.
 *   Optional, default value: 10
 * - "disable_shift": disable the bit shift of the timestamp, in order to
 *   recalculate fine timestamps of previously analyzed data.
 *   Optional, default value: false
 * - "disable_CFD_gates": disable the display of the additional waveforms of
 *   the CFD calculation.
 *   Optional, default value: false
 */
void timestamp_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config)) {
        printf("ERROR: libCFD timestamp_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
	struct CFD_config *config = malloc(1 * sizeof(struct CFD_config));

        if (!config) {
            printf("ERROR: libCFD timestamp_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }

        config->baseline_samples = json_integer_value(json_object_get(json_config, "baseline_samples"));
        config->fraction = json_number_value(json_object_get(json_config, "fraction"));
        config->delay = json_integer_value(json_object_get(json_config, "delay"));

        if (json_is_number(json_object_get(json_config, "smooth_samples"))) {
            const unsigned int W = json_number_value(json_object_get(json_config, "smooth_samples"));
            // Rounding it to the next greater odd number
            config->smooth_samples = floor(W / 2) * 2 + 1;
        } else {
            config->smooth_samples = 1;
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

        if (json_is_boolean(json_object_get(json_config, "disable_shift"))) {
            config->disable_shift = json_is_true(json_object_get(json_config, "disable_shift"));
        } else {
            config->disable_shift = false;
        }

        if (json_is_boolean(json_object_get(json_config, "disable_CFD_gates"))) {
            config->disable_CFD_gates = json_is_true(json_object_get(json_config, "disable_CFD_gates"));
        } else {
            config->disable_CFD_gates = false;
        }

        config->is_error = false;
        config->previous_samples_number = 0;

        config->curve_smoothed = NULL;
        config->curve_centered = NULL;
        config->curve_CFD = NULL;

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by timestamp_init()
 */
void timestamp_close(void *user_config)
{
    struct CFD_config *config = (struct CFD_config*)user_config;

    if (config->curve_smoothed) {
        free(config->curve_smoothed);
    }
    if (config->curve_centered) {
        free(config->curve_centered);
    }
    if (config->curve_CFD) {
        free(config->curve_CFD);
    }

    if (user_config) {
        free(user_config);
    }
}

/*! \brief Function that determines the trigger position using a Constant Fraction Discriminator algorithm.
 */
void timestamp_analysis(const uint16_t *samples,
                        uint32_t samples_number,
                        uint32_t *trigger_position,
                        struct event_waveform *waveform,
                        struct event_PSD *event,
                        int8_t *select_event,
                        void *user_config)
{
    struct CFD_config *config = (struct CFD_config*)user_config;

    reallocate_curves(samples_number, &config);

    if (config->is_error) {
        printf("ERROR: libCFD timestamp_analysis(): Error status detected\n");

        return;
    }

    running_mean(samples, samples_number, config->smooth_samples, &config->curve_smoothed);

    double baseline = 0;

    calculate_average(config->curve_smoothed, 0, config->baseline_samples, &baseline);

    add_and_multiply_constant(config->curve_smoothed, samples_number, \
                              -1 * baseline, 1.0, \
                              &config->curve_centered);

    CFD_signal(config->curve_centered, samples_number, config->delay, config->fraction, &config->curve_CFD);

    double CFD_min = 0;
    double CFD_max = 0;
    size_t CFD_index_min = 0;
    size_t CFD_index_max = 0;

    find_extrema(config->curve_CFD, 0, samples_number, &CFD_index_min, &CFD_index_max, &CFD_min, &CFD_max);

    size_t zero_crossing_index = 0;

    size_t CFD_index_left = CFD_index_min;
    size_t CFD_index_right = CFD_index_max;

    if (CFD_index_min > CFD_index_max)
    {
        CFD_index_left = CFD_index_max;
        CFD_index_right = CFD_index_min;
    }

    find_zero_crossing(config->curve_CFD, CFD_index_left, CFD_index_right, &zero_crossing_index);

    double fine_zero_crossing = 0;

    // The fine_zero_crossing contains also the zero_crossing_index information
    find_fine_zero_crossing(config->curve_CFD, samples_number,
                            zero_crossing_index, config->zero_crossing_samples,
                            &fine_zero_crossing);

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

    // Output
    waveform->timestamp = new_timestamp;
    event->timestamp = new_timestamp;
    event->baseline = baseline;

    (*trigger_position) = zero_crossing_index;

    if (!config->disable_CFD_gates) {
        waveform_additional_set_number(waveform, 2);

        unsigned int index = 0;
        uint8_t *additional_CFD_signal = waveform_additional_get(waveform, index);
        index++;
        uint8_t *additional_trigger = waveform_additional_get(waveform, index);

        const double CFD_abs_max = (fabs(CFD_max) > fabs(CFD_min)) ? fabs(CFD_max) : fabs(CFD_min);

        const uint8_t ZERO = UINT8_MAX / 2;
        const uint8_t MAX = UINT8_MAX / 2;

        for (uint32_t i = 0; i < samples_number; i++) {
            additional_CFD_signal[i] = (config->curve_CFD[i] / CFD_abs_max) * MAX + ZERO;

            if (i == zero_crossing_index) {
                additional_trigger[i] = ZERO + (MAX / 2);
            } else if (i == zero_crossing_index + 1) {
                additional_trigger[i] = ZERO - (MAX / 2);
            } else if (i == CFD_index_max) {
                additional_trigger[i] = ZERO + MAX;
            } else if (i == CFD_index_min) {
                additional_trigger[i] = ZERO - MAX;
            } else {
                additional_trigger[i] = ZERO;
            }
        }
    }

    (*select_event) = SELECT_TRUE;
}

void reallocate_curves(uint32_t samples_number, struct CFD_config **user_config)
{
    struct CFD_config *config = (*user_config);

    if (samples_number != config->previous_samples_number) {
	config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_smoothed = realloc(config->curve_smoothed,
                                             samples_number * sizeof(double));
        double *new_curve_centered = realloc(config->curve_centered,
                                             samples_number * sizeof(double));
        double *new_curve_CFD = realloc(config->curve_CFD,
                                        samples_number * sizeof(double));

        if (!new_curve_smoothed) {
            printf("ERROR: libCFD reallocate_curves(): Unable to allocate curve_smoothed memory\n");

            config->is_error = true;
        } else {
            config->curve_smoothed = new_curve_smoothed;
        }
        if (!new_curve_centered) {
            printf("ERROR: libCFD reallocate_curves(): Unable to allocate curve_centered memory\n");

            config->is_error = true;
        } else {
            config->curve_centered = new_curve_centered;
        }
        if (!new_curve_CFD) {
            printf("ERROR: libCFD reallocate_curves(): Unable to allocate curve_CFD memory\n");

            config->is_error = true;
        } else {
            config->curve_CFD = new_curve_CFD;
        }
    }
}
