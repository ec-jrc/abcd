#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>
#include <jansson.h>
#include <gsl/gsl_interp2d.h>
#include <gsl/gsl_vector.h>
//#include <gsl/gsl_blas.h>
#include <gsl/gsl_multimin.h>

#include "analysis_functions.h"
#include "events.h"

#define PARAMETERS_NUMBER 4
#define STEP_BASELINE  10
#define STEP_HEIGHT    10
#define STEP_DELAY     10
#define STEP_TIMESHIFT 10

/*! \brief Sctructure that holds the configuration for the `energy_analysis` function.
 */
struct GeP_config
{
    double *delays;
    double *clocks;
    double *samples;
    size_t delays_size;
    size_t clocks_size;
    size_t samples_size;
    double delay_starting;
    double timeshift_starting;
    double height_scaling;
    double clock_step;
    unsigned int fractional_bits;
    size_t max_iterations;
    double fit_tolerance;
    double ns_per_sample;

    gsl_interp2d *interp;
    gsl_interp_accel *xacc;
    gsl_interp_accel *yacc;
    gsl_multimin_fminimizer *minim;
    gsl_vector *initial_step_sizes;
    gsl_multimin_function minex_func;

    uint16_t *waveform_samples;
    uint32_t waveform_samples_number;
    size_t function_calls;

    unsigned int verbosity;
};

/*! \brief Function that computes the chi_squared against the reference waveform.
 *
 *  \param x The gsl_vector containing the fit variables
 *  \param params The buffer containing the waveforms and parameters
 */
double func_f (const gsl_vector * x, void *params)
{
    struct GeP_config *config = (struct GeP_config*)params;

    config->function_calls += 1;

    //printf("N: %u, min: %f, bin_width: %f\n", N, min, bin_width);

    const double baseline = gsl_vector_get(x, 0); /*!< The vertical offset of the waveform */
    const double height = gsl_vector_get(x, 1);   /*!< The amplitude of the pulse */
    const double delay = gsl_vector_get(x, 2);    /*!< The time delay of the reference pulse, i.e. the parameter describing the sape */
    const double timeshift = gsl_vector_get(x, 3);/*!< The time shift of the pulse, i.e. a temporal shift not affecting the pulse shape */

    if (config->verbosity > 2) {
        printf("GeP: func_f: baseline: %f; height: %f; delay: %f; timeshift: %f;\n",
               baseline, height, delay, timeshift);
    }

    double chi_squared = 0;

    for (uint32_t i = 0; i < config->waveform_samples_number; ++i)
    {
        const double clock = (i - timeshift) * config->clock_step;
        const double reference_value = gsl_interp2d_eval_extrap(config->interp,
                                                                config->delays,
                                                                config->clocks,
                                                                config->samples,
                                                                delay, clock,
                                                                config->xacc, config->yacc);
        const double v = (reference_value * height) + baseline;
        const double difference = ((double)config->waveform_samples[i]) - v;

        if (config->verbosity > 2) {
            printf("GeP: func_f: iter: i: %" PRIu32 "; clock: %f; reference_value: %f; v: %f; difference: %f;\n",
                   i, clock, reference_value, v, difference);
        }


        chi_squared += difference * difference;
    }

    return chi_squared;
}

/*! \brief Function that reads the json_t configuration for the `energy_analysis` function.
 *
 * This function parses a JSON object determining the configuration for the
 * `energy_analysis()` function. The configuration is returned as an
 * allocated `struct GeP_config`.
 *
 * The parameters that are searched in the json_t object are:
 *
 * - "delays": an array describing the time delays steps in ns.
 *   ```
 *   delays_i = delays[i]
 *   ```
 * - "clocks": an array describing the clock steps in ns.
 *   ```
 *   clocks_j = delays[j]
 *   ```
 * - "samples": an array with the samples values corresponding to the
 *   (delay, clock) tuples. The array shall be monodimensional with an ordering
 *   ```
 *   samples_ij = samples[i + j * N_delay]
 *   ```
 *   where `i` is the index on the delay values and `j` on the clock values.
 * - "height_scaling": a scaling factor multiplied to the pulse height.
 *   Optional, default value: 1
 */
void energy_init(json_t *json_config, void **user_config)
{
    (*user_config) = NULL;

    if (!json_is_object(json_config)) {
        printf("ERROR: libGeP energy_init(): json_config is not a json_t object\n");

        (*user_config) = NULL;
    } else {
	struct GeP_config *config = malloc(1 * sizeof(struct GeP_config));

        if (!config) {
            printf("ERROR: libGeP energy_init(): Unable to allocate config memory\n");

            (*user_config) = NULL;
        }

        json_t *delays  = json_object_get(json_config, "delays");
        json_t *clocks  = json_object_get(json_config, "clocks");
        json_t *samples = json_object_get(json_config, "samples");

        if (!json_is_array(delays) || !json_is_array(clocks) || !json_is_array(samples)) {
            printf("ERROR: libGeP energy_init(): delays, clocks or samples are not arrays\n");

            free(config);

            (*user_config) = NULL;
        } else {
            config->verbosity = 2;

            const size_t delays_size  = json_array_size(delays);
            const size_t clocks_size  = json_array_size(clocks);
            const size_t samples_size = json_array_size(samples);

            config->delays_size  = delays_size;
            config->clocks_size  = clocks_size;
            config->samples_size = samples_size;

	    config->delays  = malloc(delays_size  * sizeof(double));
	    config->clocks  = malloc(clocks_size  * sizeof(double));
	    config->samples = malloc(samples_size * sizeof(double));

            if (!config->delays || 
                !config->clocks || 
                !config->samples) {
                printf("ERROR: libGeP energy_init(): Could not allocate delays, clocks or samples\n");

                if (config->delays) {
                    free(config->delays);
                }
                if (config->clocks) {
                    free(config->clocks);
                }
                if (config->samples) {
                    free(config->samples);
                }

                free(config);

                (*user_config) = NULL;
            } else {
                size_t index;
                json_t *value;

                json_array_foreach(delays, index, value) {
                    config->delays[index] = json_number_value(value);
                }
                json_array_foreach(clocks, index, value) {
                    config->clocks[index] = json_number_value(value);
                }
                json_array_foreach(samples, index, value) {
                    config->samples[index] = json_number_value(value);
                }
            }

            json_t *height_scaling = json_object_get(json_config, "height_scaling");
            if (json_is_number(height_scaling)) {
                config->height_scaling = json_number_value(height_scaling);
            } else {
                config->height_scaling = 50;
            }

            json_t *starting_delay = json_object_get(json_config, "starting_delay");
            if (json_is_number(starting_delay)) {
                config->delay_starting = json_number_value(starting_delay);
            } else {
                config->delay_starting = 100; // ns
            }

            json_t *starting_timeshift = json_object_get(json_config, "starting_timeshift");
            if (json_is_number(starting_timeshift)) {
                config->timeshift_starting = json_number_value(starting_timeshift);
            } else {
                config->timeshift_starting = 0; // clock samples
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

            json_t *clock_step = json_object_get(json_config, "clock_step");
            if (json_is_number(clock_step)) {
                config->clock_step = json_number_value(clock_step);
            } else {
                config->clock_step = 2; // ns
            }

            json_t *fractional_bits = json_object_get(json_config, "fractional_bits");
            if (json_is_integer(fractional_bits)) {
                config->fractional_bits = json_integer_value(fractional_bits);
            } else {
                config->fractional_bits = 10;
            }

            // Initialization of the GSL interpolation facility
            const gsl_interp2d_type *type = gsl_interp2d_bilinear;

            config->xacc = gsl_interp_accel_alloc();
            config->yacc = gsl_interp_accel_alloc();

            config->interp = gsl_interp2d_alloc(type, config->delays_size, config->clocks_size);

            gsl_interp2d_init(config->interp,
                              config->delays, config->clocks, config->samples,
                              config->delays_size, config->clocks_size);

            // Initialization of the GSL minimization facility
            const gsl_multimin_fminimizer_type *T = gsl_multimin_fminimizer_nmsimplex2;
            config->minim = gsl_multimin_fminimizer_alloc(T, PARAMETERS_NUMBER);

            config->initial_step_sizes = gsl_vector_alloc(PARAMETERS_NUMBER);
            gsl_vector_set(config->initial_step_sizes, 0, STEP_BASELINE);
            gsl_vector_set(config->initial_step_sizes, 1, STEP_HEIGHT);
            gsl_vector_set(config->initial_step_sizes, 2, STEP_DELAY);
            gsl_vector_set(config->initial_step_sizes, 3, STEP_TIMESHIFT);

            /* Initialize method and iterate */
            config->minex_func.n = PARAMETERS_NUMBER;
            config->minex_func.f = func_f;
            config->minex_func.params = config;

            (*user_config) = (void*)config;
        }
    }
}

/*! \brief Function that cleans the memory allocated by `energy_init()`
 */
void energy_close(void *user_config)
{
    struct GeP_config *config = (struct GeP_config*)user_config;

    if (config->delays) {
        free(config->delays);
    }
    if (config->clocks) {
        free(config->clocks);
    }
    if (config->samples) {
        free(config->samples);
    }
    if (config->xacc) {
        gsl_interp_accel_free(config->xacc);
    }
    if (config->yacc) {
        gsl_interp_accel_free(config->yacc);
    }
    if (config->interp) {
        gsl_interp2d_free(config->interp);
    }
    if (config->minim) {
        gsl_multimin_fminimizer_free(config->minim);
    }
    if (config->initial_step_sizes) {
        gsl_vector_free(config->initial_step_sizes);
    }
    if (user_config) {
        free(user_config);
    }
}

/*! \brief Function that determines the energy and GeP information with the double integration method.
 */
void energy_analysis(const uint16_t *samples,
                     uint32_t samples_number,
                     uint32_t trigger_position,
                     struct event_waveform *waveform,
                     struct event_PSD *event,
                     int8_t *select_event,
                     void *user_config)
{
    (*select_event) = SELECT_TRUE;

    struct GeP_config *config = (struct GeP_config*)user_config;

    bool is_error = false;

    gsl_vector *fit_variables = gsl_vector_alloc(PARAMETERS_NUMBER);

    if (!fit_variables) {
        printf("ERROR: libGeP energy_analysis(): Unable to allocate fit_variables memory\n");

        is_error = true;
    }

    if (is_error) {
        if (fit_variables) {
            gsl_vector_free(fit_variables);
        }

        return;
    }

    if (config->verbosity > 0) {
        printf("GeP: samples_number: %" PRIu32 "; trigger_position: %" PRIu32 ";\n", samples_number, trigger_position);
    }

    // Storing the waveform pointer in the config to the func_f may access it
    config->waveform_samples = samples;
    config->waveform_samples_number = samples_number;
    config->function_calls = 0;

    const double guess_baseline = samples[0];
    const double guess_height = samples[samples_number - 1] - guess_baseline;
    const double guess_delay = config->delay_starting;
    const double guess_timeshift = config->timeshift_starting;

    gsl_vector_set(fit_variables, 0, guess_baseline);
    gsl_vector_set(fit_variables, 1, guess_height);
    gsl_vector_set(fit_variables, 2, guess_delay);
    gsl_vector_set(fit_variables, 3, guess_timeshift);

    if (config->verbosity > 0) {
        printf("GeP: guesses: baseline: %f; height: %f; delay: %f; timeshift: %f;\n", guess_baseline, guess_height, guess_delay, guess_timeshift);
    }

    gsl_multimin_fminimizer_set(config->minim,
                                &config->minex_func,
                                fit_variables,
                                config->initial_step_sizes);

    int status = GSL_CONTINUE;

    for (size_t iter = 0; iter < config->max_iterations && status == GSL_CONTINUE; iter++) {
        status = gsl_multimin_fminimizer_iterate(config->minim);

    	if (config->verbosity > 1) {
            gsl_vector *fitted = gsl_multimin_fminimizer_x(config->minim);

            printf("GeP: iter: iter: %zu; status: %d; baseline: %f; height: %f; delay: %f; timeshift: %f;\n",
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
    	    printf("GeP: inter: iter: %zu; status: %d; value: %f; size: %f;\n", iter, status, value, size);
    	}

    }

    if (config->verbosity > 0) {
        printf("GeP: function_calls: %zu;\n", config->function_calls);
    }

    if (status == GSL_SUCCESS) {
        printf ("converged to minimum at\n");
    } else {
        //(*select_event) = SELECT_FALSE;
        printf ("ERROR: unable to converge\n");
    }

    gsl_vector *fitted = gsl_multimin_fminimizer_x(config->minim);
    
    const double baseline = gsl_vector_get(fitted, 0);
    const double height = gsl_vector_get(fitted, 1);
    const double delay = gsl_vector_get(fitted, 2);
    const double timeshift = gsl_vector_get(fitted, 3);

    gsl_vector_free(fit_variables);

    const uint64_t timestamp = waveform->timestamp + round((timeshift - (delay / config->clock_step)) * (1 << config->fractional_bits));

    uint64_t long_qshort = 0;
    uint64_t long_qlong = (uint64_t)round(fabs(height) * config->height_scaling);

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
    event->timestamp = timestamp;
    event->qshort = int_qshort;
    event->qlong = int_qlong;
    event->baseline = int_baseline;
    event->channel = waveform->channel;
    event->pur = PUR;

    const uint8_t initial_additional_number = waveform_additional_get_number(waveform);
    const uint8_t new_additional_number = initial_additional_number + 1;

    waveform_additional_set_number(waveform, new_additional_number);

    uint8_t *additional_fitted = waveform_additional_get(waveform, initial_additional_number + 0);

    const uint8_t ZERO = UINT8_MAX / 2;
    const uint8_t MAX = UINT8_MAX / 2;

    for (uint32_t i = 0; i < samples_number; i++) {
        const double clock = (i - timeshift) * config->clock_step;
        const double reference_value = gsl_interp2d_eval_extrap(config->interp,
                                                                config->delays,
                                                                config->clocks,
                                                                config->samples,
                                                                delay, clock,
                                                                config->xacc, config->yacc);
        additional_fitted[i] = (reference_value * MAX) + ZERO;
    }
}
