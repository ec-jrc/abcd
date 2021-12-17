/*! \brief Determination of the trigger position by employing a Timing Filter
 *         Amplifier and then a Constant Fraction Discriminator algorithm.
 *
 * Calculation procedure:
 *  1. A recursive high-pass filter (CR filter) is applied.
 *  2. A recursive low-pass filter (RC filter) is applied.
 *  3. The CFD algorithm is applied to the resulting pulse.
 *
 * The trigger position is added to time timestamp provided by the digitizer
 * in order to determine the pulse absolute time. The trigger position is
 * determined with a resolution better than the clock step, the fractional part
 * is added to the timestamp by shifting it by a user-configurable number of
 * bits.
 *
 * The configuration parameters that are searched in a `json_t` object are:
 *
 * - `highpass_time`: the decay time of the high-pass filter, in terms of clock
 *   samples.
 * - `lowpass_time`: the decay time of the low-pass filter, in terms of clock
 *   samples.
 * - `fraction`: the multiplication factor of the signal that is to be summed
 *   to the delayed version of the signal itself.
 * - `delay`: the delay of the signal in terms of clock samples.
 * - `zero_crossing_samples`: the number of samples to be used in the linear
 *   interpolation of the zero-crossing region. Optional, default value: 2
 * - `smooth_samples`: the number of samples to be averaged in the running
 *   mean, rounded to the next odd number. Optional, default value: 1
 * - `fractional_bits`: the number of fractional bits of the timestamp.
 *   Optional, default value: 10
 * - `disable_shift`: disable the bit shift of the timestamp, in order to
 *   recalculate fine timestamps of previously analyzed data.
 *   Optional, default value: false
 * - `disable_CFD_gates`: disable the display of the additional waveforms of
 *   the CFD calculation.
 *   Optional, default value: false
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
struct TFACFD_config
{
    double highpass_time;
    double lowpass_time;
    double fraction;
    int32_t delay;
    uint32_t zero_crossing_samples;
    uint8_t fractional_bits;
    bool disable_shift;
    bool disable_CFD_gates;

    bool is_error;

    uint32_t previous_samples_number;

    double *curve_samples;
    double *curve_CR;
    double *curve_RC;
    double *curve_CFD;
};

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct TFACFD_config **user_config);

/*! \brief Function that reads the json_t configuration for the timestamp_analysis function.
 *
 * This function parses a JSON object determining the configuration for the
 * `timestamp_analysis()` function. The configuration is returned as an
 * allocated `struct TFACFD_config`.
 *
 */
void timestamp_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config)) {
        printf("ERROR: libTFACFD timestamp_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
	struct TFACFD_config *config = malloc(1 * sizeof(struct TFACFD_config));

        if (!config) {
            printf("ERROR: libTFACFD timestamp_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }

        config->highpass_time = json_number_value(json_object_get(json_config, "highpass_time"));
        config->lowpass_time = json_number_value(json_object_get(json_config, "lowpass_time"));
        config->fraction = json_number_value(json_object_get(json_config, "fraction"));
        config->delay = json_integer_value(json_object_get(json_config, "delay"));

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

        config->curve_samples = NULL;
        config->curve_CR = NULL;
        config->curve_RC = NULL;
        config->curve_CFD = NULL;

        (*user_config) = (void*)config;
    }
}

/*! \brief Function that cleans the memory allocated by timestamp_init()
 */
void timestamp_close(void *user_config)
{
    struct TFACFD_config *config = (struct TFACFD_config*)user_config;

    if (config->curve_samples) {
        free(config->curve_samples);
    }
    if (config->curve_CR) {
        free(config->curve_CR);
    }
    if (config->curve_RC) {
        free(config->curve_RC);
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
                        struct event_waveform *waveform,
                        uint32_t **trigger_positions,
                        struct event_PSD **events_buffer,
                        size_t *events_number,
                        void *user_config)
{
    struct TFACFD_config *config = (struct TFACFD_config*)user_config;

    reallocate_curves(samples_number, &config);

    bool is_error = false;

    if ((*events_number) != 1) {
        printf("WARNING: libTFACFD timestamp_analysis(): Reallocating buffers\n");

        // Assuring that there is one event_PSD and discarding others
        is_error = reallocate_buffers(trigger_positions, events_buffer, 1);
        (*events_number) = is_error ? 0 : 1;
    }

    if (is_error || config->is_error) {
        printf("ERROR: libTFACFD timestamp_analysis(): Error status detected\n");

        return;
    }

    to_double(samples, samples_number, &config->curve_samples);

    CR_filter(config->curve_samples, samples_number, \
              config->highpass_time, \
              &config->curve_CR);

    RC_filter(config->curve_CR, samples_number, \
              config->lowpass_time, \
              &config->curve_RC);

    CFD_signal(config->curve_RC, samples_number, \
               config->delay, config->fraction, \
               &config->curve_CFD);

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
    (*events_buffer)[0].timestamp = new_timestamp;
    (*events_buffer)[0].qshort = 0;
    (*events_buffer)[0].qlong = 0;
    (*events_buffer)[0].baseline = 0;
    (*events_buffer)[0].channel = waveform->channel;
    (*events_buffer)[0].pur = 0;

    (*trigger_positions)[0] = zero_crossing_index;

    if (!config->disable_CFD_gates) {
        double CR_min = 0;
        double CR_max = 0;
        size_t CR_index_min = 0;
        size_t CR_index_max = 0;

        find_extrema(config->curve_CR, 0, samples_number, &CR_index_min, &CR_index_max, &CR_min, &CR_max);

        double RC_min = 0;
        double RC_max = 0;
        size_t RC_index_min = 0;
        size_t RC_index_max = 0;

        find_extrema(config->curve_RC, 0, samples_number, &RC_index_min, &RC_index_max, &RC_min, &RC_max);

        waveform_additional_set_number(waveform, 4);

        unsigned int index = 0;
        uint8_t *additional_CR_signal = waveform_additional_get(waveform, index);
        index++;
        uint8_t *additional_RC_signal = waveform_additional_get(waveform, index);
        index++;
        uint8_t *additional_CFD_signal = waveform_additional_get(waveform, index);
        index++;
        uint8_t *additional_trigger = waveform_additional_get(waveform, index);

        const double CR_abs_max = (fabs(CR_max) > fabs(CR_min)) ? fabs(CR_max) : fabs(CR_min);
        const double RC_abs_max = (fabs(RC_max) > fabs(RC_min)) ? fabs(RC_max) : fabs(RC_min);
        const double CFD_abs_max = (fabs(CFD_max) > fabs(CFD_min)) ? fabs(CFD_max) : fabs(CFD_min);

        const uint8_t ZERO = UINT8_MAX / 2;
        const uint8_t MAX = UINT8_MAX / 2;

        for (uint32_t i = 0; i < samples_number; i++) {
            additional_CR_signal[i] = (config->curve_CR[i] / CR_abs_max) * MAX + ZERO;
            additional_RC_signal[i] = (config->curve_RC[i] / RC_abs_max) * MAX + ZERO;
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
}

void reallocate_curves(uint32_t samples_number, struct TFACFD_config **user_config)
{
    struct TFACFD_config *config = (*user_config);

    if (samples_number != config->previous_samples_number) {
	config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_CR = realloc(config->curve_CR,
                                       samples_number * sizeof(double));
        double *new_curve_RC = realloc(config->curve_RC,
                                       samples_number * sizeof(double));
        double *new_curve_CFD = realloc(config->curve_CFD,
                                        samples_number * sizeof(double));

        if (!new_curve_samples) {
            printf("ERROR: libTFACFD reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        } else {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_CR) {
            printf("ERROR: libTFACFD reallocate_curves(): Unable to allocate curve_CR memory\n");

            config->is_error = true;
        } else {
            config->curve_CR = new_curve_CR;
        }
        if (!new_curve_RC) {
            printf("ERROR: libTFACFD reallocate_curves(): Unable to allocate curve_RC memory\n");

            config->is_error = true;
        } else {
            config->curve_RC = new_curve_RC;
        }
        if (!new_curve_CFD) {
            printf("ERROR: libTFACFD reallocate_curves(): Unable to allocate curve_CFD memory\n");

            config->is_error = true;
        } else {
            config->curve_CFD = new_curve_CFD;
        }
    }
}
