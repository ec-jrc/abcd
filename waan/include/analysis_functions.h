/*! \file      analysis_functions.h
 *  \brief     Definition file for the user-functions used by waan for waveforms analysis.
 *  \author    Cristiano L. Fontana
 *  \version   0.1
 *  \date      April 2021
 *  \copyright MIT
 *
 * This header file contains the prototypes of the functions used by waan for
 * its waveforms analyses. It can be used to define both a library with
 * functions determining the timestamp information, as well as with functions
 * determining the energy information. The libraries should have a C interface.
 * Both libraries should have an init function, an analysis function and a
 * close function. Waan will look for the following function names:
 *
 * - `timestamp_init`: the init function for the timestamp analysis. It shall
 *   read a json_t object, as defined in the jansson library, and return an
 *   buffer that contains the custom configuration for the analysis function.
 *   The function prototype is the \ref `WA_init_fn`.
 * - `timestamp_close`: the closing function that frees the memory allocated by
 *   the `timestamp_init` function.
 *   The function prototype is the \ref `WA_close_fn`.
 * - `timestamp_analysis`: the function analysing the event waveform in order to
 *   determine the timestamp information.
 *   The function prototype is the \ref `WA_timestamp_fn`.
 * - `energy_init`: the init function for the energy analysis. It behaves as the
 *   `timestamp_init` function.
 *   The function prototype is the \ref `WA_init_fn`.
 * - `energy_close`: the closing function that frees the memory allocated by the
 *   `energy_init` function.
 *   The function prototype is the \ref `WA_close_fn`.
 * - `energy_analysis`: the function analysing the event waveform in order to
 *   determine the energy information.
 *   The function prototype is the \ref `WA_energy_fn`.
 *
 */

#ifndef __ANALYSIS_FUNCTIONS_H__
#define __ANALYSIS_FUNCTIONS_H__ 1

// For the fixed width integer numbers
#include <stdint.h>
// For the dynamic load of libraries
#include <dlfcn.h>
// For the JSON parsing and managing
#include <jansson.h>

#include "events.h"

/*! \brief `enum` used to define portable `true` and `false` between C and C++.
 */
enum selection_boolean_t {
    SELECT_TRUE = 1,
    SELECT_FALSE = 0
};
typedef enum selection_boolean_t selection_boolean_t;

/*! \brief Workaround to avoid compiler warnings
 *
 *  \param[in] X the variable that is to be declared as used.
 */
#define UNUSED(X) (void)(X)

/*! \brief Type of a function used to read a configuration and initialize the analyses.
 *
 * This function allows the user to insert some custom configuration in the main
 * JSON configuration file. The function receives the pointer to a `json_t`
 * object as defined in the jansson JSON library. The `json_t` object will be
 * destroyed after the call, so the user shall not rely on its persistence nor
 * shall destroy it. The `void` pointer may point to a buffer that can hold any
 * kind of data and may be used to store the configuration. The user has to
 * allocate the necessary memory in this function. The `user_config` is passed
 * to the analysis function, for the user convenience. This function is
 * optional, if not defined the `dummy_init()` function is used.
 *
 * \param[in]  json_config a pointer to the `json_t` object containing the
 *                         user-defined configuration in the main JSON
 *                         configuration file.
 * \param[out] user_config pointer to a generic buffer that the user has to
 *                         allocate in the init function.
 *
 * \return Nothing
 */
typedef void (*WA_init_fn)(json_t* json_config,
                           void **user_config);

inline void dummy_init(json_t* json_config,
                       void **user_config)
{
    UNUSED(json_config);

    (*user_config) = NULL;
}

/*! \brief Type of a function used to free the user-defined configuration.
 *
 * This function shall be used to free the user data allocated with the init
 * function. This function is optional, if not defined the `dummy_close()`
 * function is used.
 *
 * \param[in]  user_config pointer to a generic buffer that the user has to
 *                         allocate in the init function.
 *
 * \return Nothing
 */
typedef void (*WA_close_fn)(void *user_config);

inline void dummy_close(void *user_config)
{
    if (user_config) {
        free(user_config);
    }
}

/*! \brief Type of a function used to determine timestamp information
 *
 * The function described by this type is used to analyze waveform samples, in
 * order to determine the timestamp information.
 *
 * \param[in]     samples           pointer to the raw samples.
 * \param[in]     samples_number    the number of samples in the waveform.
 * \param[out]    trigger_position  pointer to an `uint32_t` variable in which
 *                                  the user can store the trigger posistion as
 *                                  the index of the waveform samples. This
 *                                  value is then passed to the subsequent
 *                                  `energy_analysis()` function.
 * \param[in,out] waveform          pointer to an `event_waveform` in which the
 *                                  user can store additional waveforms and the
 *                                  timestamp information. This object is going
 *                                  to be forwarded to the next modules.
 *                                  This pointer holds also the information
 *                                  gathered from the digitizer, such as the
 *                                  event timestamp.
 * \param[in,out] event             pointer to an `event_PSD` in which the user
 *                                  can store the timestamp information. This
 *                                  object is going to be forwarded to the next
 *                                  modules.
 * \param[out]    select_event      pointer to an `int8_t` variable with which
 *                                  the user signals whether this event should
 *                                  be discarded. For portability reasons
 *                                  between C and C++ an `int8_t` variable was
 *                                  chosen. 0 corresponds to false and 1 to true
 * \param[in]     user_config       pointer to a user-defined buffer in which
 *                                  the user might have stored some
 *                                  configuration in the init function.
 *                                  This buffer is different from the one
 *                                  passed to the timestamp function.
 * \return Nothing
 */
typedef void (*WA_timestamp_fn)(const uint16_t *samples,
                                uint32_t samples_number,
                                uint32_t *trigger_position,
                                struct event_waveform *waveform,
                                struct event_PSD *event,
                                int8_t *select_event,
                                void *user_config);
/*! \brief Type of a function used to determine energy information
 *
 * The function described by this type is used to analyze waveform samples, in
 * order to determine the energy information.
 *
 * \param[in]     samples           pointer to the raw samples.
 * \param[in]     samples_number    the number of samples in the waveform.
 * \param[in]     trigger_position  index of the waveform samples where the
 *                                  trigger was determined.
 * \param[in,out] waveform          pointer to an `event_waveform` in which the
 *                                  user can store additional waveforms and the
 *                                  energy information. This object is going to
 *                                  be forwarded to the next modules.
 * \param[in,out] event             pointer to an `event_PSD` in which the user
 *                                  can store the energy information. This
 *                                  object is going to be forwarded to the next
 *                                  modules.
 * \param[in,out] select_event      pointer to an `int8_t` variable with which
 *                                  the user signals whether this event should
 *                                  be discarded. For portability reasons
 *                                  between C and C++ an `int8_t` variable was
 *                                  chosen. 0 corresponds to false and 1 to true
 * \param[in]     user_config       pointer to a user-defined buffer in which
 *                                  the user might have stored some
 *                                  configuration in the init function.
 *                                  This buffer is different from the one
 *                                  passed to the timestamp function.
 * \return Nothing
 */
typedef void (*WA_energy_fn)(const uint16_t *samples,
                             uint32_t samples_number,
                             uint32_t trigger_position,
                             struct event_waveform *waveform,
                             struct event_PSD *event,
                             int8_t *select_event,
                             void *user_config);

// We define these unions in order to convert the data pointer from `dlsym()`
// to a function pointer, that might not be compatible see:
// https://en.wikipedia.org/wiki/Dynamic_loading#UNIX_(POSIX)
union WA_init_union
{
    WA_init_fn fn;
    void *obj;
};
union WA_close_union
{
    WA_close_fn fn;
    void *obj;
};
union WA_timestamp_union
{
    WA_timestamp_fn fn;
    void *obj;
};
union WA_energy_union
{
    WA_energy_fn fn;
    void *obj;
};

#endif
