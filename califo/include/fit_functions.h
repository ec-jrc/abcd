#ifndef __FIT_FUNCTIONS_H__
#define __FIT_FUNCTIONS_H__

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multifit_nlinear.h>

#ifndef counter_type
#define counter_type double
#endif

#include "histogram.h"

struct peak_parameters
{
    double A;
    double mu;
    double sigma;
    double B;
    double alpha;
    int fit_result;
};

extern inline double gaussian(const double mu,
                              const double sigma,
                              const double t)
{
    const double z = (t - mu) / sigma;
    return exp(-0.5 * z * z);
}

extern inline double exponential(const double alpha,
                                 const double t)
{
    return exp(-alpha * t);
}

extern inline double linear(const double m,
                            const double q,
                            const double t)
{
    return m * t + q;
}

extern inline double peak( const double A,
                    const double mu,
                    const double sigma,
                    const double B,
                    const double alpha,
                    const double t)
{
    return A * gaussian(mu, sigma, t) + B * exponential(alpha, t);
}

extern inline
int func_f (const gsl_vector * x, void *params, gsl_vector * f)
{
    struct histogram_t *histo = (struct histogram_t*) params;

    const unsigned int N = histo->bins;
    const double min = histo->min;
    const double bin_width = histo->bin_width;

    //printf("N: %u, min: %f, bin_width: %f\n", N, min, bin_width);

    const double A = gsl_vector_get(x, 0);
    const double mu = gsl_vector_get(x, 1);
    const double sigma = gsl_vector_get(x, 2);
    const double B = gsl_vector_get(x, 3);
    const double alpha = gsl_vector_get(x, 4);

    for (unsigned int i = 0; i < N; ++i)
    {
        const double ti = i * bin_width + min;
        const counter_type yi = histo->histo[i];
        const double y = peak(A, mu, sigma, B, alpha, ti);
        const double difference = ((double)yi) - y;

        //printf("i: %u, ti: %f, yi - y: %f\n", i, ti, difference);
  
        gsl_vector_set(f, i, difference);
    }

    return GSL_SUCCESS;
}

extern inline
int func_J (const gsl_vector * x, void *params, gsl_matrix * J)
{
    struct histogram_t *histo = (struct histogram_t*) params;

    const unsigned int N = histo->bins;
    const double min = histo->min;
    const double bin_width = histo->bin_width;

    const double A = gsl_vector_get(x, 0);
    const double mu = gsl_vector_get(x, 1);
    const double sigma = gsl_vector_get(x, 2);
    const double B = gsl_vector_get(x, 3);
    const double alpha = gsl_vector_get(x, 4);

    for (unsigned int i = 0; i < N; ++i)
    {
        const double ti = i * bin_width + min;
        const double g = gaussian(mu, sigma, ti);
        const double e = exponential(alpha, ti);
        const double rt = ti - mu;

        gsl_matrix_set(J, i, 0, g);
        gsl_matrix_set(J, i, 1, A * rt * g / sigma / sigma);
        gsl_matrix_set(J, i, 2, A * rt * rt * g / sigma / sigma / sigma);
        gsl_matrix_set(J, i, 3, e);
        gsl_matrix_set(J, i, 4, -B * ti * e);

        //printf("i: %u, ti: %f, yi - y: %f\n", i, ti, difference);
    }

    return GSL_SUCCESS;
}

// This function is executed at each iteration step
extern inline
void callback(const size_t iter, void *params,
              const gsl_multifit_nlinear_workspace *work)
{
    // Params are not used as the data points are constant
    // values and they do not change the Jacobian
    (void) params;
  
    gsl_vector *f = gsl_multifit_nlinear_residual(work);
    gsl_vector *current_x = gsl_multifit_nlinear_position(work);
    double avratio = gsl_multifit_nlinear_avratio(work);
  
    double rcond;
    // compute reciprocal condition number of J(x)
    gsl_multifit_nlinear_rcond(&rcond, work);
  
    printf("# iter %2zu: A = %.4f, mu = %.4f, sigma = %.4f, B = %.4f, alpha = %.4f, |a|/|v| = %.4f cond(J) = %8.4f, |f(x)| = %.4f\n",
            iter,
            gsl_vector_get(current_x, 0),
            gsl_vector_get(current_x, 1),
            gsl_vector_get(current_x, 2),
            gsl_vector_get(current_x, 3),
            gsl_vector_get(current_x, 4),
            avratio,
            1.0 / rcond,
            gsl_blas_dnrm2(f));
}

extern inline
struct peak_parameters fit_peak(struct histogram_t *histo, struct peak_parameters initial)
{
    // Number of data points to fit
    const size_t n = histo->bins;
    // Number of model parameters
    const size_t p = 5;

    // Fit parameters
    const size_t max_iter = 200;
    // A general guideline for selecting the step tolerance
    // is to choose xtol = 10^−d where d is the number of
    // accurate decimal digits desired in the solution x.
    // See Dennis and Schnabel for more information.
    const double xtol = 1.0e-8;
    // A general guideline for choosing the gradient
    // tolerance is to set gtol = GSL_DBL_EPSILON^(1/3).
    // See Dennis and Schnabel for more information.
    const double gtol = 1.0e-8;
    const double ftol = 1.0e-8;
  
    // This data type defines a general system of functions
    // with arbitrary parameters, the corresponding Jacobian
    // matrix of derivatives, and optionally the second
    // directional derivative of the functions for geodesic
    // acceleration.
    gsl_multifit_nlinear_fdf fdf;
  
    // Define function to be minimized
    fdf.f = func_f;
    fdf.df = NULL;
    fdf.fvv = NULL;
    fdf.n = n;
    fdf.p = p;
    fdf.params = histo;
  
    // These functions return a set of recommended default
    // parameters for use in solving nonlinear least squares
    // problems.
    gsl_multifit_nlinear_parameters fdf_params =
      gsl_multifit_nlinear_default_parameters();

    // This selects the Levenberg-Marquardt algorithm.
    //fdf_params.trs = gsl_multifit_nlinear_trs_lm;
    // This selects the Levenberg-Marquardt algorithm with
    // geodesic acceleration.
    // It is faster, but it needs the fvv, that can be
    // numerically estimated anyways.
    fdf_params.trs = gsl_multifit_nlinear_trs_lmaccel;

    // This specifies a trust region method.
    // It is currently the only implemented nonlinear least squares method.
    const gsl_multifit_nlinear_type *T = gsl_multifit_nlinear_trust;
  
    gsl_multifit_nlinear_workspace *work =
      gsl_multifit_nlinear_alloc(T, &fdf_params, n, p);
  
    // Initial fit parameters
    gsl_vector *initial_x = gsl_vector_alloc(p);
  
    // Estimation of the starting point in the parameters space
    gsl_vector_set(initial_x, 0, initial.A);
    gsl_vector_set(initial_x, 1, initial.mu);
    gsl_vector_set(initial_x, 2, initial.sigma);
    gsl_vector_set(initial_x, 3, initial.B);
    gsl_vector_set(initial_x, 4, initial.alpha);

    printf("# Starting point: A = %.4f, mu = %.4f, sigma = %.4f, B = %.4f, alpha = %.4f\n",
        gsl_vector_get(initial_x, 0),
        gsl_vector_get(initial_x, 1),
        gsl_vector_get(initial_x, 2),
        gsl_vector_get(initial_x, 3),
        gsl_vector_get(initial_x, 4));

    int info;
    double chisq0, chisq, rcond;
  
    // Initialize solver
    gsl_multifit_nlinear_init(initial_x, &fdf, work);
  
    gsl_vector *f_values = gsl_multifit_nlinear_residual(work);
    gsl_vector *current_x = gsl_multifit_nlinear_position(work);
  
    // Store initial cost
    gsl_blas_ddot(f_values, f_values, &chisq0);
  
    // Iterate until convergence
    const int fit_result = gsl_multifit_nlinear_driver(max_iter, xtol, gtol, ftol,
                                                       callback, NULL, &info, work);

    if (fit_result == GSL_SUCCESS)
    {
        printf("# Successfull fit!!!\n");
    }
    else if (fit_result == GSL_EMAXITER)
    {
        printf("ERROR: Maximum number of iterations reached.\n");
    }
    else if (fit_result == GSL_ENOPROG)
    {
        printf("ERROR: No further progress can be made.\n");
    }
    else
    {
        printf("ERROR: Error number: %d.\n", fit_result);
    }
  
    // Store final cost
    gsl_blas_ddot(f_values, f_values, &chisq);
  
    // Store cond(J(x))
    gsl_multifit_nlinear_rcond(&rcond, work);
  
    // Store the final set of fit parameters
    gsl_vector *final_x = gsl_vector_alloc(p);
    gsl_vector_memcpy(final_x, current_x);
  
    // Store the number of iterations
    const size_t niter = gsl_multifit_nlinear_niter(work);

    // Clean up the workspace memory
    gsl_multifit_nlinear_free(work);
  
    // Print summary
    printf("# MAX ITER      = %zu\n", max_iter);
    printf("# NITER         = %zu\n", niter);
    printf("# NFEV          = %zu\n", fdf.nevalf);
    printf("# NJEV          = %zu\n", fdf.nevaldf);
    printf("# NAEV          = %zu\n", fdf.nevalfvv);
    printf("# initial cost  = %.12e\n", chisq0);
    printf("# final cost    = %.12e\n", chisq);
    printf("# final x       = (%.12e, %.12e, %12e, %12e, %12e)\n",
        gsl_vector_get(final_x, 0),
        gsl_vector_get(final_x, 1),
        gsl_vector_get(final_x, 2),
        gsl_vector_get(final_x, 3),
        gsl_vector_get(final_x, 4));
    printf("# final cond(J) = %.12e\n", 1.0 / rcond);

    printf("# Final point: A = %.4f, mu = %.4f, sigma = %.4f, B = %.4f, alpha = %.4f\n",
        gsl_vector_get(final_x, 0),
        gsl_vector_get(final_x, 1),
        gsl_vector_get(final_x, 2),
        gsl_vector_get(final_x, 3),
        gsl_vector_get(final_x, 4));
  
    const struct peak_parameters p_p = {gsl_vector_get(final_x, 0),
                                        gsl_vector_get(final_x, 1),
                                        gsl_vector_get(final_x, 2),
                                        gsl_vector_get(final_x, 3),
                                        gsl_vector_get(final_x, 4),
                                        fit_result};

    gsl_vector_free(initial_x);
    gsl_vector_free(final_x);
  
    return p_p;
}

#endif
