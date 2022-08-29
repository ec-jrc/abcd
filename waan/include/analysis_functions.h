/*! \file      analysis_functions.h
 *  \brief     Definition file for the user-functions used by waan for waveforms analysis.
 *  \author    Cristiano L. Fontana
 *  \version   0.2
 *  \date      December 2021
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

// For size_t
#include <stddef.h>
// For the fixed width integer numbers
#include <stdint.h>
// For the dynamic load of libraries
#include <dlfcn.h>
#include <stdbool.h>
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
 * \param[in,out] waveform          pointer to an `event_waveform` in which the
 *                                  user can store additional waveforms and the
 *                                  timestamp information. This object is going
 *                                  to be forwarded to the next ABCD modules.
 *                                  This pointer holds also the information
 *                                  gathered from the digitizer, such as the
 *                                  event timestamp.
 * \param[out]    trigger_positions pointer to an array of `uint32_t` in which
 *                                  the user can store the trigger posistions as
 *                                  the index of the waveform samples. This
 *                                  array shall be of the same size of the
 *                                  `events_buffer`, thus `events_number`. This
 *                                  array is then passed to the subsequent
 *                                  `energy_analysis()` function.
 * \param[in,out] events_buffer     pointer to an array of `struct event_PSD` in
 *                                  which the user can store the processed
 *                                  events. These selected events are going to
 *                                  be forwarded to the `energy_analysis()`.
 *                                  For convenience the input buffer already
 *                                  has one allocated `struct event_PSD`.
 * \param[in,out] events_number     pointer to a `size_t` variable with which
 *                                  the user signals how many events are in the
 *                                  `events_buffer`. A zero means that the user
 *                                  is discarding this waveform from the
 *                                  processing.
 * \param[in]     user_config       pointer to a user-defined buffer in which
 *                                  the user might have stored some
 *                                  configuration in the init function.
 *                                  This buffer is different from the one
 *                                  passed to the timestamp function.
 * \return Nothing
 */
typedef void (*WA_timestamp_fn)(const uint16_t *samples,
                                uint32_t samples_number,
                                struct event_waveform *waveform,
                                uint32_t **trigger_positions,
                                struct event_PSD **events_buffer,
                                size_t *events_number,
                                void *user_config);

/*! \brief Function that is used in place of the \ref `timestamp_analysis`
 *         user-supplied function. It simply forwards the waveforms to the
 *         \ref `energy_analysis` function without doing anything. It is used
 *         in case the user is not interested in determining timing information
 *         or if the user wants to perform all the analysis in the
 *         \ref `energy_analysis` function.
 *
 * The function parameters are the same of \ref `WA_timestamp_fn`.
 */
inline void dummy_timestamp_analysis(const uint16_t *samples,
                                     uint32_t samples_number,
                                     struct event_waveform *waveform,
                                     uint32_t **trigger_positions,
                                     struct event_PSD **events_buffer,
                                     size_t *events_number,
                                     void *user_config)
{
    UNUSED(samples);
    UNUSED(samples_number);
    UNUSED(waveform);
    UNUSED(trigger_positions);
    UNUSED(events_buffer);
    UNUSED(events_number);
    UNUSED(user_config);
}

/*! \brief Type of a function used to determine energy information
 *
 * The function described by this type is used to analyze waveform samples, in
 * order to determine the energy information.
 *
 * \param[in]     samples           pointer to the raw samples.
 * \param[in]     samples_number    the number of samples in the waveform.
 * \param[in,out] waveform          pointer to an `event_waveform` in which the
 *                                  user can store additional waveforms and the
 *                                  energy information. This object is going to
 *                                  be forwarded to the next modules.
 * \param[in]     trigger_positions pointer to an array of `uint32_t` in which
 *                                  the user stored the trigger posistions as
 *                                  the index of the waveform samples. This
 *                                  array shall be of the same size of the
 *                                  `events_buffer`, thus `events_number`.
 * \param[in,out] events_buffer     pointer to an array of `struct event_PSD` in
 *                                  which the user can store the processed
 *                                  events. These selected events are going to
 *                                  be forwarded to the next ABCD modules.
 * \param[in,out] events_number     pointer to a `size_t` variable with which
 *                                  the user signals how many events are in the
 *                                  `events_buffer`. A zero means that the user
 *                                  is discarding this waveform from the
 *                                  processing.
 * \param[in]     user_config       pointer to a user-defined buffer in which
 *                                  the user might have stored some
 *                                  configuration in the init function.
 *                                  This buffer is different from the one
 *                                  passed to the timestamp function.
 * \return Nothing
 */
typedef void (*WA_energy_fn)(const uint16_t *samples,
                             uint32_t samples_number,
                             struct event_waveform *waveform,
                             uint32_t **trigger_positions,
                             struct event_PSD **events_buffer,
                             size_t *events_number,
                             void *user_config);

// We define these unions in order to convert the data pointer from `dlsym()`
// to a function pointer, that might not be compatible, see:
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

/*! \brief Function to reallocate the memory required for the events buffer.
 *
 * The `new_events_number` may also be zero, signaling that no `event_PSD` shall
 * be selected.
 * In case of success, the function will store in `old_events_number` the value
 * of `new_events_number`; in case of failure, the function will store zero in
 * `old_events_number`. If the value of `old_events_number` is zero and the
 * value of `new_events_number` is non-zero, the function will initialize to
 * zero all the values of `trigger_positions`.
 *
 * \param[in,out] trigger_positions  pointer to the array that is to be reallocated
 * \param[in,out] events_buffer      pointer to the array that is to be reallocated
 * \param[in,out] old_events_number  the old size of the buffer in terms of events.
 * \param[in]     new_events_number  the new size of the buffer in terms of events.
 *
 * \return `true` if success, `false` otherwise
 */
inline bool reallocate_buffers(uint32_t **trigger_positions,
                               struct event_PSD **events_buffer,
                               size_t *old_events_number,
                               size_t new_events_number)
{
    if ((*old_events_number) != new_events_number) {
        uint32_t *new_positions = (uint32_t*)realloc((*trigger_positions),
                                  new_events_number * sizeof(uint32_t));

        // It is allowed to have a zero size, but the result is implementation specific.
        // It could be a NULL pointer or a non-NULL pointer.
        if (!new_positions && (new_events_number > 0)) {
            printf("ERROR: reallocate_buffers(): Unable to allocate trigger_positions memory\n");

            (*old_events_number) = 0;

            return false;
        } else {
            (*trigger_positions) = new_positions;
        }

        struct event_PSD *new_buffer = (struct event_PSD*)realloc((*events_buffer),
                                       new_events_number * sizeof(struct event_PSD));

        if (!new_buffer && (new_events_number > 0)) {
            printf("ERROR: reallocate_buffers(): Unable to allocate events_buffer memory\n");

            (*old_events_number) = 0;

            return false;
        } else {
            (*events_buffer) = new_buffer;
        }

        // Initialize the new trigger_positions if there were none before
        if ((*old_events_number) == 0 && new_events_number > 0) {
            memset((*trigger_positions), 0, new_events_number * sizeof(uint32_t));
        }

        (*old_events_number) = new_events_number;
    }

    return true;
}

#endif
