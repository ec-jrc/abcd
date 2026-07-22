/*! \brief Determination of the timing and energy information from a pulse, by
 *         fitting it with a logistic function
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the first N samples in the analysis
 *     range.
 *  2. The pulse is offset by the baseline to center it around zero.
 *  3. The decay of the pulse is compensated.
 *  4. The resulting waveform is fitted against a logistic function.
 *  5. The pulse energy is the height of the logistic function and the qshort
 *     is the slope of the function
 *
 * The algorithm may be run over a subset of the waveform, by setting the
 * parameters: `analysis_start` and `analysis_end`
 *
 * This function determines only one event_PSD and will discard the other
 * trigger positions determined in the timestamp analysis step.
 *
 * The configuration parameters that are searched in a `json_t` object are:
 *
 * - `analysis_start`: the first bin of the analysis relative to the trigger
 *   position. It can be negative. Optional, default value: 0
 * - `analysis_end`: the last bin of the analysis relative to the trigger
 *   position. Optional, default value: `samples_number`
 * - `baseline_samples`: the number of samples to average to determine the
 *   baseline. The average starts from the beginning of the analysis range.
 * - `pulse_polarity`: a string describing the expected pulse polarity, it
 *   can be `positive` or `negative`.
 * - `decay_time`: the pulse decay time in terms of clock samples, for the
 *   compensation.
 * - `guessed_tau`: the starting value for the fit for the tau of the logistic
 *   function.
 * - `fit_tolerance`: the value against wich the absolute value of the function
 *   gradient is tested against. If the gradient is less than the tolerance then
 *   the minimum is reached.
 * - `max_iterations`: maximum number of fit iterations.
 * - `fractional_bits`: the number of fractional bits of the timestamp.
 *   Optional, default value: 10
 * - `disable_shift`: disable the bit shift of the timestamp, in order to
 *   recalculate fine timestamps of previously analyzed data.
 *   Optional, default value: false
 * - `tau_scaling`: a scaling factor multiplied to the function tau.
 * - `height_scaling`: a scaling factor multiplied to the function height.
 *   Optional, default value: 1
 * - `energy_threshold`: pulses with an energy lower than the threshold are
 *   discared. Optional, default value: 0
 * - `time_offset`: value added to the timestamp after the determination, to
 *   center the signals on the ToF distribution.
 *   Optional, default value: 0
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

#include <gsl/gsl_vector.h>
// #include <gsl/gsl_blas.h>
#include <gsl/gsl_multimin.h>

#include "events.h"
#include "analysis_functions.h"
#include "DSP_functions.h"

#define PARAMETERS_NUMBER 4
#define STEP_TIMESHIFT 10
#define STEP_TAU 1
#define STEP_LOGISTICHEIGHT 10
#define STEP_BASELINE 10
#define GUESSED_TAU 14

/*! \brief Sctructure that holds the configuration for the `energy_analysis()` function.
 */
struct Logistic_config
{
    double guessed_tau;

    int64_t analysis_start;
    int64_t analysis_end;
    int64_t baseline_samples;
    double decay_time;
    enum pulse_polarity_t pulse_polarity;
    double tau_scaling;
    double height_scaling;
    double energy_threshold;

    uint8_t fractional_bits;
    int64_t time_offset;
    bool disable_shift;

    size_t max_iterations;
    double fit_tolerance;

    gsl_multimin_fminimizer *minim;
    gsl_vector *initial_step_sizes;
    gsl_vector *fit_variables;
    gsl_multimin_function minex_func;

    uint32_t previous_samples_number;

    uint32_t current_samples_number;
    int64_t current_analysis_start;
    int64_t current_analysis_end;
    double *curve_samples;
    double *curve_offset;
    double *curve_compensated;

    size_t function_calls;

    unsigned int verbosity;
    bool is_error;
};

inline double logistic(double t)
{
    return 1.0 / (1.0 + exp(-1. * t));
}

inline double generalized_logistic(double t, double timeshift, double tau, double height, double baseline)
{
    return height * logistic((t - timeshift) / tau) + baseline;
}

/*! \brief Function that computes the chi_squared against the reference waveform.
 *
 *  \param x The gsl_vector containing the fit variables
 *  \param params The buffer containing the waveforms and parameters
 */
double func_f(const gsl_vector *x, void *params)
{
    struct Logistic_config *config = (struct Logistic_config *)params;

    config->function_calls += 1;

    const double timeshift = gsl_vector_get(x, 0);      /*!< The time shift of the waveform */
    const double tau = gsl_vector_get(x, 1);            /*!< The time factor in the exponential */
    const double logisticheight = gsl_vector_get(x, 2); /*!< The amplitude of the signal waveform */
    const double baseline = gsl_vector_get(x, 3);       /*!< The vertical offset of the waveform */

    if (config->verbosity > 2)
    {
        printf("libLogistic func_f(): timeshift: %f; tau: %f; logisticheight: %f; baseline: %f;\n",
               timeshift, tau, logisticheight, baseline);
    }

    double chi_squared = 0;

    for (uint32_t index_sample = config->current_analysis_start; index_sample < config->current_analysis_end; ++index_sample)
    {
        const double v = generalized_logistic(index_sample, timeshift, tau, logisticheight, baseline);
        const double difference = config->curve_compensated[index_sample] - v;

        if (config->verbosity > 2)
        {
            printf("libLogistic func_f(): iter: index_sample: %" PRIu32 "; v: %f; difference: %f;\n",
                   index_sample, v, difference);
        }

        chi_squared += difference * difference;
    }

    return chi_squared;
}

/*! \brief Function that allocates the necessary memory for the calculations.
 */
void reallocate_curves(uint32_t samples_number, struct Logistic_config **user_config);

/*! \brief Function that reads the json_t configuration for the `energy_analysis()` function.
 *
 * This function parses a JSON object determining the configuration for the
 * `energy_analysis()` function. The configuration is returned as an
 * allocated `struct Logistic_config`.
 */
void energy_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    struct Logistic_config *config = calloc(1, sizeof(struct Logistic_config));

    if (!config)
    {
        printf("ERROR: libLogistic energy_init(): Unable to allocate config memory\n");

        return;
    }

    read_config_number(json_config, analysis_start, 0, config);
    read_config_number(json_config, analysis_end, UINT32_MAX, config);
    read_config_number(json_config, decay_time, UINT32_MAX, config);
    read_config_number(json_config, baseline_samples, 1, config);
    read_config_number(json_config, guessed_tau, GUESSED_TAU, config);
    read_config_number(json_config, max_iterations, 200, config);
    read_config_number(json_config, fit_tolerance, 1, config);
    read_config_number(json_config, fractional_bits, 10, config);
    read_config_number(json_config, time_offset, 0, config);
    read_config_boolean(json_config, disable_shift, false, config);
    read_config_number(json_config, tau_scaling, 1.0, config);
    read_config_number(json_config, height_scaling, 1.0, config);
    read_config_number(json_config, energy_threshold, 0, config);
    read_config_number(json_config, verbosity, 0, config);

    char *pulse_polarities_strs[] = {"negative", "positive"};
    enum pulse_polarity_t pulse_polarities_vals[] = {POLARITY_NEGATIVE, POLARITY_POSITIVE};
    read_config_options(json_config, pulse_polarity, pulse_polarities_strs, pulse_polarities_vals, config);

    config->initial_step_sizes = gsl_vector_alloc(PARAMETERS_NUMBER);
    gsl_vector_set(config->initial_step_sizes, 0, STEP_TIMESHIFT);
    gsl_vector_set(config->initial_step_sizes, 1, STEP_TAU);
    gsl_vector_set(config->initial_step_sizes, 2, STEP_LOGISTICHEIGHT);
    gsl_vector_set(config->initial_step_sizes, 3, STEP_BASELINE);

    config->fit_variables = gsl_vector_alloc(PARAMETERS_NUMBER);
    gsl_vector_set(config->fit_variables, 0, STEP_TIMESHIFT);
    gsl_vector_set(config->fit_variables, 1, STEP_TAU);
    gsl_vector_set(config->fit_variables, 2, STEP_LOGISTICHEIGHT);
    gsl_vector_set(config->fit_variables, 3, STEP_BASELINE);

    /* Initialize method and iterate */
    config->minex_func.n = PARAMETERS_NUMBER;
    config->minex_func.f = func_f;
    config->minex_func.params = config;

    config->curve_samples = NULL;
    config->curve_offset = NULL;
    config->curve_compensated = NULL;

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

    struct Logistic_config *config = (struct Logistic_config *)user_config;

    if (config->curve_samples)
    {
        free(config->curve_samples);
    }
    if (config->curve_compensated)
    {
        free(config->curve_compensated);
    }
    if (config->curve_offset)
    {
        free(config->curve_offset);
    }
    if (config->initial_step_sizes)
    {
        gsl_vector_free(config->initial_step_sizes);
    }
    if (config->fit_variables)
    {
        gsl_vector_free(config->fit_variables);
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
        printf("ERROR: libLogistic energy_analysis(): User config not defined, not performing analysis\n");

        return;
    }

    struct Logistic_config *config = (struct Logistic_config *)user_config;

    reallocate_curves(samples_number, &config);

    bool is_error = false;

    if ((*events_number) != 1)
    {
        // Assuring that there is one event_PSD and discarding others
        is_error = !reallocate_buffers(trigger_positions, events_buffer, events_number, 1);

        if (is_error)
        {
            printf("ERROR: libLogistic energy_analysis(): Unable to reallocate buffers\n");
        }
    }

    if (config->verbosity > 1)
    {
        printf("libLogistic energy_analysis(): samples_number: %" PRIu32 "; trigger_position: %" PRIu32 ";\n", samples_number, (*trigger_positions)[0]);
    }

    to_double(samples, samples_number, &config->curve_samples);

    // Preventing segfaults by checking the boundaries
    const int64_t analysis_start = clamp(config->analysis_start + (*trigger_positions)[0], 0, samples_number);
    const int64_t analysis_end = clamp(config->analysis_end + (*trigger_positions)[0], config->analysis_start, samples_number);

    config->current_analysis_start = analysis_start;
    config->current_analysis_end = analysis_end;

    const uint32_t baseline_start = analysis_start;
    const uint32_t baseline_end = clamp(baseline_start + config->baseline_samples, 0, samples_number);

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

    decay_compensation(config->curve_offset, samples_number,
                       config->decay_time,
                       &config->curve_compensated);

    double compensated_min = 0;
    double compensated_max = 0;
    size_t compensated_index_min = 0;
    size_t compensated_index_max = 0;

    find_extrema(config->curve_compensated, 0, samples_number,
                 &compensated_index_min, &compensated_index_max,
                 &compensated_min, &compensated_max);

    const int64_t topline_end = analysis_end;
    const int64_t topline_start = clamp(topline_end - config->baseline_samples, 0, topline_end);

    double topline = 0;
    calculate_average(config->curve_compensated, topline_start, topline_end, &topline);

    // Storing the waveform pointer in the config to the func_f may access it
    config->current_samples_number = samples_number;
    config->function_calls = 0;

    const double guessed_timeshift = (*trigger_positions)[0];
    // The baseline should have already been corrected
    const double guessed_logisticheight = topline;
    const double guessed_baseline = 0;

    gsl_vector_set(config->fit_variables, 0, guessed_timeshift);
    gsl_vector_set(config->fit_variables, 1, config->guessed_tau);
    gsl_vector_set(config->fit_variables, 2, guessed_logisticheight);
    gsl_vector_set(config->fit_variables, 3, guessed_baseline);

    if (config->verbosity > 0)
    {
        printf("libLogistic energy_analysis(): guesses: timeshift: %f; tau: %f; logisticheight: %f; baseline: %f;\n",
               guessed_timeshift, config->guessed_tau, guessed_logisticheight, guessed_baseline);
    }

    if (config->verbosity > 1)
    {
        printf("libLogistic energy_analysis(): Creating minimizer\n");
    }

    // Initialization of the GSL minimization facility
    // const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex2;
    config->minim = gsl_multimin_fminimizer_alloc(gsl_multimin_fminimizer_nmsimplex2, PARAMETERS_NUMBER);

    if (!config->minim)
    {
        printf("ERROR: libLogistic energy_analysis(): Unable to allocate minimizer memory\n");

        reallocate_buffers(trigger_positions, events_buffer, events_number, 0);

        config->is_error = true;
    }
    else
    {
        gsl_multimin_fminimizer_set(config->minim,
                                    &config->minex_func,
                                    config->fit_variables,
                                    config->initial_step_sizes);

        if (config->verbosity > 1)
        {
            printf("libLogistic energy_analysis(): Created minimizer and launching minimization\n");
        }

        int status = GSL_CONTINUE;

        size_t iter;

        for (iter = 0; iter < config->max_iterations && status == GSL_CONTINUE; iter++)
        {
            status = gsl_multimin_fminimizer_iterate(config->minim);

            if (config->verbosity > 1)
            {
                gsl_vector *fitted = gsl_multimin_fminimizer_x(config->minim);

                printf("libLogistic energy_analysis(): iteration: iter: %zu, status: %d, timeshift: %f; tau: %f; logisticheight: %f; baseline: %f;\n",
                       iter, status,
                       gsl_vector_get(fitted, 0),
                       gsl_vector_get(fitted, 1),
                       gsl_vector_get(fitted, 2),
                       gsl_vector_get(fitted, 3));
            }

            if (status)
            {
                break;
            }

            const double value = gsl_multimin_fminimizer_minimum(config->minim);
            const double size = gsl_multimin_fminimizer_size(config->minim);
            status = gsl_multimin_test_size(size, config->fit_tolerance);

            if (config->verbosity > 1)
            {
                printf("libLogistic energy_analysis(): iteration: iter: %zu; status: %d; value: %f; size: %f;\n", iter, status, value, size);
            }
        }

        if (config->verbosity > 0)
        {
            printf("libLogistic energy_analysis(): Finished minimization; iterations: %zu; function_calls: %zu;\n", iter, config->function_calls);
        }

        double fitted_timeshift = guessed_timeshift;
        double fitted_tau = config->guessed_tau;
        double fitted_logisticheight = guessed_logisticheight;
        double fitted_baseline = guessed_baseline;

        if (status == GSL_SUCCESS)
        {
            // This seems to be managed in the minimizer and should not be freed
            gsl_vector *fitted = gsl_multimin_fminimizer_x(config->minim);

            fitted_timeshift = gsl_vector_get(fitted, 0);
            fitted_tau = gsl_vector_get(fitted, 1);
            fitted_logisticheight = gsl_vector_get(fitted, 2);
            fitted_baseline = gsl_vector_get(fitted, 3);

            if (config->verbosity > 0)
            {
                printf("libLogistic energy_analysis(): converged to minimum: timeshift: %f; tau: %f; logisticheight: %f; baseline: %f;\n",
                       fitted_timeshift, fitted_tau, fitted_logisticheight, fitted_baseline);
            }
        }
        else
        {
            printf("ERROR: libLogistic energy_analysis(): Unable to converge\n");
        }

        if (config->minim)
        {
            if (config->verbosity > 1)
            {
                printf("libLogistic energy_analysis(): Clearing minimizer\n");
            }

            gsl_multimin_fminimizer_free(config->minim);

            if (config->verbosity > 1)
            {
                printf("libLogistic energy_analysis(): Cleared minimizer\n");
            }
        }

        // Converting to fixed-point number
        const uint64_t fine_timestamp = round(fitted_timeshift * (1 << config->fractional_bits));

        uint64_t new_timestamp = fine_timestamp + config->time_offset;

        // Bitmask to delete the last fractional_bits in the uint64_t numbers
        const uint64_t bitmask = UINT64_MAX - ((1 << config->fractional_bits) - 1);

        if (config->disable_shift)
        {
            new_timestamp += (waveform->timestamp & bitmask);
        }
        else
        {
            new_timestamp += ((waveform->timestamp << config->fractional_bits) & bitmask);
        }

        const double scaled_qshort = fitted_tau * config->tau_scaling;
        const double scaled_qlong = fabs(fitted_logisticheight) * config->height_scaling;

        const uint8_t group_counter = 0;

        if (scaled_qlong < config->energy_threshold)
        {
            // Discard the event
            reallocate_buffers(trigger_positions, events_buffer, events_number, 0);
        }
        else
        {
            // Output
            (*events_buffer)[0].timestamp = new_timestamp;
            (*events_buffer)[0].qshort = clamp_to_uint16(scaled_qshort);
            (*events_buffer)[0].qlong = clamp_to_uint16(scaled_qlong);
            (*events_buffer)[0].baseline = clamp_to_uint16(baseline);
            (*events_buffer)[0].channel = waveform->channel;
            (*events_buffer)[0].group_counter = group_counter;

            if (config->verbosity > 0)
            {
                printf("libLogistic energy_analysis(): creating additional waveforms\n");
            }

            const uint8_t initial_additional_number = waveform_additional_get_number(waveform);
            const uint8_t new_additional_number = initial_additional_number + 4;

            waveform_additional_set_number(waveform, new_additional_number);

            uint8_t *additional_gate_analysis = waveform_additional_get(waveform, initial_additional_number + 0);
            uint8_t *additional_gate_baseline = waveform_additional_get(waveform, initial_additional_number + 1);
            uint8_t *additional_original = waveform_additional_get(waveform, initial_additional_number + 2);
            uint8_t *additional_fitted = waveform_additional_get(waveform, initial_additional_number + 3);

            const double absolute_max = compensated_max > fitted_logisticheight ? compensated_max : fitted_logisticheight;

            const uint8_t ZERO = UINT8_MAX / 2;
            const uint8_t MAX = UINT8_MAX / 2;

            for (uint32_t i = 0; i < samples_number; i++)
            {
                if ((baseline_start <= i && i < baseline_end) || (topline_start <= i && i < topline_end))
                {
                    additional_gate_baseline[i] = ZERO + MAX / 2;
                }
                else
                {
                    additional_gate_baseline[i] = ZERO;
                }

                if (analysis_start <= i && i < analysis_end)
                {
                    additional_gate_analysis[i] = ZERO + MAX;
                }
                else
                {
                    additional_gate_analysis[i] = ZERO;
                }

                additional_original[i] = (config->curve_compensated[i] / absolute_max) * MAX + ZERO;

                const double v = generalized_logistic(i,
                                                      fitted_timeshift,
                                                      fitted_tau,
                                                      fitted_logisticheight,
                                                      fitted_baseline);
                additional_fitted[i] = (v / absolute_max) * MAX + ZERO;
            }
        }
    }
}

void reallocate_curves(uint32_t samples_number, struct Logistic_config **user_config)
{
    struct Logistic_config *config = (*user_config);

    if (samples_number != config->previous_samples_number)
    {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_offset = realloc(config->curve_offset,
                                           samples_number * sizeof(double));
        double *new_curve_compensated = realloc(config->curve_compensated,
                                                samples_number * sizeof(double));

        if (!new_curve_samples)
        {
            printf("ERROR: libLogistic reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_offset)
        {
            printf("ERROR: libLogistic reallocate_curves(): Unable to allocate curve_offset memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_offset = new_curve_offset;
        }
        if (!new_curve_compensated)
        {
            printf("ERROR: libLogistic reallocate_curves(): Unable to allocate curve_compensated memory\n");

            config->is_error = true;
        }
        else
        {
            config->curve_compensated = new_curve_compensated;
        }
    }
}
