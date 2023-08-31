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
 * - `height_scaling`: a scaling factor multiplied to the function height.
 *   Optional, default value: 1
 * - `energy_threshold`: pulses with an energy lower than the threshold are
 *   discared. Optional, default value: 0
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
//#include <gsl/gsl_blas.h>
#include <gsl/gsl_multimin.h>

#include "events.h"
#include "analysis_functions.h"
#include "DSP_functions.h"

#define PARAMETERS_NUMBER 4
#define STEP_TIMESHIFT   10
#define STEP_TAU         1
#define STEP_LOGISTICHEIGHT 10
#define STEP_BASELINE    10
#define GUESSED_TAU      14

/*! \brief Sctructure that holds the configuration for the `energy_analysis()` function.
 */
struct Logistic_config
{
    double guessed_tau;

    int64_t analysis_start;
    int64_t analysis_end;
    int64_t baseline_samples;
    enum pulse_polarity_t pulse_polarity;
    double decay_time;
    double height_scaling;
    double energy_threshold;

    unsigned int fractional_bits;

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

inline double logistic(double t) {
    return 1.0 / (1.0 + exp(-1. * t));
}

inline double generalized_logistic(double t, double timeshift, double tau, double height, double baseline) {
    return height * logistic((t - timeshift) / tau) + baseline;
}


/*! \brief Function that computes the chi_squared against the reference waveform.
 *
 *  \param x The gsl_vector containing the fit variables
 *  \param params The buffer containing the waveforms and parameters
 */
double func_f (const gsl_vector * x, void *params)
{
    struct Logistic_config *config = (struct Logistic_config*)params;

    config->function_calls += 1;

    const double timeshift = gsl_vector_get(x, 0);/*!< The time shift of the waveform */
    const double tau = gsl_vector_get(x, 1);      /*!< The time factor in the exponential */
    const double logisticheight = gsl_vector_get(x, 2); /*!< The amplitude of the signal waveform */
    const double baseline = gsl_vector_get(x, 3); /*!< The vertical offset of the waveform */

    if (config->verbosity > 2) {
        printf("libLogistic func_f(): timeshift: %f; tau: %f; logisticheight: %f; baseline: %f;\n",
               timeshift, tau, logisticheight, baseline);
    }

    double chi_squared = 0;

    for (uint32_t index_sample = config->current_analysis_start; index_sample < config->current_analysis_end; ++index_sample)
    {
        const double v = generalized_logistic(index_sample, timeshift, tau, logisticheight, baseline);
        const double difference = config->curve_compensated[index_sample] - v;

        if (config->verbosity > 2) {
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

    if (!json_is_object(json_config)) {
        printf("ERROR: libLogistic energy_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
	struct Logistic_config *config = malloc(1 * sizeof(struct Logistic_config));

        if (!config) {
            printf("ERROR: libLogistic energy_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        } else {
            json_t *verbosity = json_object_get(json_config, "verbosity");
            if (json_is_number(verbosity)) {
                config->verbosity = json_number_value(verbosity);
            } else {
                config->verbosity = 0;
            }

            json_t *height_scaling = json_object_get(json_config, "height_scaling");
            if (json_is_number(height_scaling)) {
                config->height_scaling = json_number_value(height_scaling);
            } else {
                config->height_scaling = 1;
            }

            if (json_is_number(json_object_get(json_config, "analysis_start"))) {
                config->analysis_start = json_number_value(json_object_get(json_config, "analysis_start"));
            } else {
                config->analysis_start = 0;
            }
            if (json_is_number(json_object_get(json_config, "analysis_end"))) {
                config->analysis_end = json_number_value(json_object_get(json_config, "analysis_end"));
            } else {
                config->analysis_end = UINT32_MAX;
            }

            config->baseline_samples = json_number_value(json_object_get(json_config, "baseline_samples"));
            config->decay_time = json_number_value(json_object_get(json_config, "decay_time"));

            json_t *guessed_tau = json_object_get(json_config, "guessed_tau");
            if (json_is_number(guessed_tau)) {
                config->guessed_tau = json_number_value(guessed_tau);
            } else {
                config->guessed_tau = GUESSED_TAU;
            }

            json_t *max_iterations = json_object_get(json_config, "max_iterations");
            if (json_is_number(max_iterations)) {
                config->max_iterations = json_number_value(max_iterations);
            } else {
                config->max_iterations = 200;
            }

            json_t *fit_tolerance = json_object_get(json_config, "fit_tolerance");
            if (json_is_number(fit_tolerance)) {
                config->fit_tolerance = json_number_value(fit_tolerance);
            } else {
                config->fit_tolerance = 1;
            }

            json_t *fractional_bits = json_object_get(json_config, "fractional_bits");
            if (json_is_integer(fractional_bits)) {
                config->fractional_bits = json_number_value(fractional_bits);
            } else {
                config->fractional_bits = 10;
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

            json_t *energy_threshold = json_object_get(json_config, "energy_threshold");
            if (json_is_integer(energy_threshold)) {
                config->energy_threshold = json_number_value(energy_threshold);
            } else {
                config->energy_threshold = 0;
            }

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

            (*user_config) = (void*)config;
        }
    }
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
    struct Logistic_config *config = (struct Logistic_config*)user_config;

    if (config->curve_samples) {
        free(config->curve_samples);
    }
    if (config->curve_compensated) {
        free(config->curve_compensated);
    }
    if (config->curve_offset) {
        free(config->curve_offset);
    }
    if (config->initial_step_sizes) {
        gsl_vector_free(config->initial_step_sizes);
    }
    if (config->fit_variables) {
        gsl_vector_free(config->fit_variables);
    }
    if (user_config) {
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
    struct Logistic_config *config = (struct Logistic_config*)user_config;

    reallocate_curves(samples_number, &config);

    bool is_error = false;

    if ((*events_number) != 1) {
        if (config->verbosity > 0) {
            printf("WARNING: libLogistic energy_analysis(): Reallocating buffers, from events number: %zu\n", (*events_number));
        }

        // Assuring that there is one event_PSD and discarding others
        is_error = !reallocate_buffers(trigger_positions, events_buffer, events_number, 1);

        if (is_error) {
            printf("ERROR: libLogistic energy_analysis(): Unable to reallocate buffers\n");
        }
    }

    if (config->verbosity > 1) {
        printf("libLogistic energy_analysis(): samples_number: %" PRIu32 "; trigger_position: %" PRIu32 ";\n", samples_number, (*trigger_positions)[0]);
    }

    to_double(samples, samples_number, &config->curve_samples);

    // Preventing segfaults by checking the boundaries
    const int64_t raw_analysis_start = config->analysis_start + (*trigger_positions)[0];
    const int64_t raw_analysis_end = config->analysis_end + (*trigger_positions)[0];

    uint32_t analysis_start = raw_analysis_start;
    if (raw_analysis_start < 0) {
        analysis_start = 0;
    } else if (raw_analysis_start > samples_number) {
        analysis_start = samples_number;
    }
    const uint32_t analysis_end = (raw_analysis_end < samples_number) ? raw_analysis_end : samples_number;
    //const uint32_t analysis_width = analysis_end - analysis_start;

    config->current_analysis_start = analysis_start;
    config->current_analysis_end = analysis_end;

    const uint32_t baseline_start = analysis_start;
    const uint32_t baseline_end = ((baseline_start + config->baseline_samples) < samples_number) ? (baseline_start + config->baseline_samples) : samples_number;

    double baseline = 0;
    calculate_average(config->curve_samples, baseline_start, baseline_end, &baseline);

    if (config->pulse_polarity == POLARITY_POSITIVE) {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, 1.0, &config->curve_offset);
    } else {
        add_and_multiply_constant(config->curve_samples, samples_number, -1 * baseline, -1.0, &config->curve_offset);
    }

    decay_compensation(config->curve_offset, samples_number, \
                       config->decay_time, \
                       &config->curve_compensated);

    double compensated_min = 0;
    double compensated_max = 0;
    size_t compensated_index_min = 0;
    size_t compensated_index_max = 0;

    find_extrema(config->curve_compensated, 0, samples_number,
                 &compensated_index_min, &compensated_index_max,
                 &compensated_min, &compensated_max);

    const int64_t topline_end = analysis_end;
    const int64_t topline_start = ((topline_end - config->baseline_samples) > 0) ? (topline_end - config->baseline_samples) : 0;

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

    if (config->verbosity > 0) {
        printf("libLogistic energy_analysis(): guesses: timeshift: %f; tau: %f; logisticheight: %f; baseline: %f;\n",
               guessed_timeshift, config->guessed_tau, guessed_logisticheight, guessed_baseline);
    }

    if (config->verbosity > 1) {
        printf ("libLogistic energy_analysis(): Creating minimizer\n");
    }

    // Initialization of the GSL minimization facility
    //const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex2;
    config->minim = gsl_multimin_fminimizer_alloc(gsl_multimin_fminimizer_nmsimplex2, PARAMETERS_NUMBER);

    if (!config->minim) {
        printf("ERROR: libLogistic energy_analysis(): Unable to allocate minimizer memory\n");

	reallocate_buffers(trigger_positions, events_buffer, events_number, 0);

        config->is_error = true;
    } else {
    	gsl_multimin_fminimizer_set(config->minim,
    	                            &config->minex_func,
    	                            config->fit_variables,
    	                            config->initial_step_sizes);

    	if (config->verbosity > 1) {
    	    printf ("libLogistic energy_analysis(): Created minimizer and launching minimization\n");
    	}

    	int status = GSL_CONTINUE;

    	size_t iter;

    	for (iter = 0; iter < config->max_iterations && status == GSL_CONTINUE; iter++) {
    	    status = gsl_multimin_fminimizer_iterate(config->minim);

    		if (config->verbosity > 1) {
    	        gsl_vector *fitted = gsl_multimin_fminimizer_x(config->minim);

    	        printf("libLogistic energy_analysis(): iteration: iter: %zu, status: %d, timeshift: %f; tau: %f; logisticheight: %f; baseline: %f;\n",
    	               iter, status,
    	               gsl_vector_get(fitted, 0),
    	               gsl_vector_get(fitted, 1),
    	               gsl_vector_get(fitted, 2),
    	               gsl_vector_get(fitted, 3));
    	    }


    	    if (status) {
    	        break;
    	    }

    	    const double value = gsl_multimin_fminimizer_minimum(config->minim);
    	    const double size = gsl_multimin_fminimizer_size(config->minim);
    	    status = gsl_multimin_test_size (size, config->fit_tolerance);

    		if (config->verbosity > 1) {
    		    printf("libLogistic energy_analysis(): iteration: iter: %zu; status: %d; value: %f; size: %f;\n", iter, status, value, size);
    		}

    	}

    	if (config->verbosity > 0) {
    	    printf("libLogistic energy_analysis(): Finished minimization; iterations: %zu; function_calls: %zu;\n", iter, config->function_calls);
    	}

    	double fitted_timeshift = guessed_timeshift;
    	double fitted_tau = config->guessed_tau;
    	double fitted_logisticheight = guessed_logisticheight;
    	double fitted_baseline = guessed_baseline;

    	if (status == GSL_SUCCESS) {
    	    // This seems to be managed in the minimizer and should not be freed
    	    gsl_vector *fitted = gsl_multimin_fminimizer_x(config->minim);
    	    
    	    fitted_timeshift = gsl_vector_get(fitted, 0);
    	    fitted_tau = gsl_vector_get(fitted, 1);
    	    fitted_logisticheight = gsl_vector_get(fitted, 2);
    	    fitted_baseline = gsl_vector_get(fitted, 3);

    	    if (config->verbosity > 0) {
    	        printf("libLogistic energy_analysis(): converged to minimum: timeshift: %f; tau: %f; logisticheight: %f; baseline: %f;\n",
    	           fitted_timeshift, fitted_tau, fitted_logisticheight, fitted_baseline);
    	    }

    	} else {
    	    printf ("ERROR: libLogistic energy_analysis(): Unable to converge\n");
    	}

    	if (config->minim) {
    	    if (config->verbosity > 1) {
    	        printf ("libLogistic energy_analysis(): Clearing minimizer\n");
    	    }

    	    gsl_multimin_fminimizer_free(config->minim);

    	    if (config->verbosity > 1) {
    	        printf ("libLogistic energy_analysis(): Cleared minimizer\n");
    	    }

    	}

    	const uint64_t timestamp = waveform->timestamp + round(fitted_timeshift * (1 << config->fractional_bits));

	const double scaled_qshort = fitted_tau;
	const double scaled_qlong = fabs(fitted_logisticheight) * config->height_scaling;

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

    	const uint8_t group_counter = 0;

	if (scaled_qlong < config->energy_threshold) {
            // Discard the event
	    reallocate_buffers(trigger_positions, events_buffer, events_number, 0);
	} else {
    	    // Output
    	    (*events_buffer)[0].timestamp = timestamp;
    	    (*events_buffer)[0].qshort = int_qshort;
    	    (*events_buffer)[0].qlong = int_qlong;
    	    (*events_buffer)[0].baseline = int_baseline;
    	    (*events_buffer)[0].channel = waveform->channel;
    	    (*events_buffer)[0].group_counter = group_counter;

    	    if (config->verbosity > 0) {
    	        printf ("libLogistic energy_analysis(): creating additional waveforms\n");
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

    	    for (uint32_t i = 0; i < samples_number; i++) {
    	        if ((baseline_start <= i && i < baseline_end) || (topline_start <= i && i < topline_end)) {
    	            additional_gate_baseline[i] = ZERO + MAX / 2;
    	        } else {
    	            additional_gate_baseline[i] = ZERO;
    	        }

    	        if (analysis_start <= i && i < analysis_end) {
    	            additional_gate_analysis[i] = ZERO + MAX;
    	        } else {
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

    if (samples_number != config->previous_samples_number) {
        config->previous_samples_number = samples_number;

        config->is_error = false;

        double *new_curve_samples = realloc(config->curve_samples,
                                            samples_number * sizeof(double));
        double *new_curve_offset = realloc(config->curve_offset,
                                           samples_number * sizeof(double));
        double *new_curve_compensated = realloc(config->curve_compensated,
                                                samples_number * sizeof(double));

        if (!new_curve_samples) {
            printf("ERROR: libLogistic reallocate_curves(): Unable to allocate curve_samples memory\n");

            config->is_error = true;
        } else {
            config->curve_samples = new_curve_samples;
        }
        if (!new_curve_offset) {
            printf("ERROR: libLogistic reallocate_curves(): Unable to allocate curve_offset memory\n");

            config->is_error = true;
        } else {
            config->curve_offset = new_curve_offset;
        }
        if (!new_curve_compensated) {
            printf("ERROR: libLogistic reallocate_curves(): Unable to allocate curve_compensated memory\n");

            config->is_error = true;
        } else {
            config->curve_compensated = new_curve_compensated;
        }
    }
}
