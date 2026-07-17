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
// For round()
#include <math.h>
// For tolower()
#include <ctype.h>
// For strstr()
#include <string.h>

#include "events.h"

/*! \brief `enum` used to define portable `true` and `false` between C and C++.
 */
enum selection_boolean_t
{
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
typedef void (*WA_init_fn)(json_t *json_config,
                           void **user_config);

inline void dummy_init(json_t *json_config,
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
    if (user_config)
    {
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
    if ((*old_events_number) != new_events_number)
    {
        uint32_t *new_positions = (uint32_t *)realloc((*trigger_positions),
                                                      new_events_number * sizeof(uint32_t));

        // It is allowed to have a zero size, but the result is implementation specific.
        // It could be a NULL pointer or a non-NULL pointer.
        if (!new_positions && (new_events_number > 0))
        {
            printf("ERROR: reallocate_buffers(): Unable to allocate trigger_positions memory\n");

            (*old_events_number) = 0;

            return false;
        }
        else
        {
            (*trigger_positions) = new_positions;
        }

        struct event_PSD *new_buffer = (struct event_PSD *)realloc((*events_buffer),
                                                                   new_events_number * sizeof(struct event_PSD));

        if (!new_buffer && (new_events_number > 0))
        {
            printf("ERROR: reallocate_buffers(): Unable to allocate events_buffer memory\n");

            (*old_events_number) = 0;

            return false;
        }
        else
        {
            (*events_buffer) = new_buffer;
        }

        // Initialize the new trigger_positions if there were none before
        if ((*old_events_number) == 0 && new_events_number > 0)
        {
            memset((*trigger_positions), 0, new_events_number * sizeof(uint32_t));
        }

        (*old_events_number) = new_events_number;
    }

    return true;
}

/*! \brief Function that clamps an index value within the given boundaries (typically for an array)
 *
 * \param[in] index the value to be forced to be in the boundaries
 * \param[in] start_index the minimum value
 * \param[in] end_index the maximum value
 *
 * \return The value modified to be within the boundaries
 */
inline extern int64_t clamp(int64_t index, int64_t start_index, int64_t end_index)
{
    if (index < start_index)
    {
        return start_index;
    }
    else if (index > end_index)
    {
        return end_index;
    }
    else
    {
        return index;
    }
}

/*! \brief Function that clamps a value within the boundaries of an uint64_t
 *
 * \param[in] value the value to be clamped
 *
 * \return The value modified to be within the boundaries
 */
inline extern uint16_t clamp_to_uint16(double value)
{
    int64_t rounded = (int64_t)round(value);

    if (rounded < 0)
    {
        return 0;
    }
    else if (rounded > UINT16_MAX)
    {
        return UINT16_MAX;
    }
    else
    {
        return rounded;
    }
}

/*! \brief Macro that reads a read value from the json_t config objects and stores it in the user config.
 *  After reading the value it overwrites it so that it appears in the configuration for the user's convenience.
 *
 * \param[in] json_config the json_t* pointer to the configuration
 * \param[in] name the name of the parameter, should match the member in the user configuration
 * \param[in] default_value the default value to use if the parameter does not exist in the json_config
 * \param[in] config the pointer to the user configuration
 * \param[in] overwrite define whether the parameter should be overwritten in the json_config
 */
#define read_config_number_overwrite(json_config, name, default_value, config, overwrite)            \
    {                                                                                                \
        json_t *json_##name = json_object_get(json_config, #name);                                   \
        config->name = json_is_number(json_##name) ? json_number_value(json_##name) : default_value; \
        if (overwrite)                                                                               \
        {                                                                                            \
            json_object_set_nocheck(json_config, #name, json_real(config->name));                    \
        }                                                                                            \
    }

/*! \brief Macro that reads boolean value from the json_t config objects and stores it in the user config.
 *  After reading the value it overwrites it so that it appears in the configuration for the user's convenience.
 *
 * \param[in] json_config the json_t* pointer to the configuration
 * \param[in] name the name of the parameter, should match the member in the user configuration
 * \param[in] default_value the default value to use if the parameter does not exist in the json_config
 * \param[in] config the pointer to the user configuration
 * \param[in] overwrite define whether the parameter should be overwritten in the json_config
 */
#define read_config_boolean_overwrite(json_config, name, default_value, config, overwrite)       \
    {                                                                                            \
        json_t *json_##name = json_object_get(json_config, #name);                               \
        config->name = json_is_boolean(json_##name) ? json_is_true(json_##name) : default_value; \
        if (overwrite)                                                                           \
        {                                                                                        \
            json_object_set_nocheck(json_config, #name, json_boolean(config->name));             \
        }                                                                                        \
    }

/*! \brief Macro that reads a string value from the json_t config objects, compares it with a given set of options,
 *  and stores the value of the corresponding option in the user config.
 *  The default option is the first one of the array.
 *
 * \param[in] json_config the json_t* pointer to the configuration
 * \param[in] name the name of the parameter, should match the member in the user configuration
 * \param[in] string_array a statically defined array with the set of strings to compare the string to, it should have at least one element
 * \param[in] values_array a statically defined array with the set of values corresponding to the strings
 * \param[in] config the pointer to the user configuration
 * \param[in] overwrite define whether the parameter should be overwritten in the json_config
 */
#define read_config_options_overwrite(json_config, name, string_array, values_array, config, overwrite)     \
    {                                                                                                       \
        const size_t number_of_options = sizeof(string_array) / sizeof(string_array[0]);                    \
                                                                                                            \
        size_t selected_index_options = 0;                                                                  \
        config->name = values_array[0];                                                                     \
                                                                                                            \
        json_t *json_##name = json_object_get(json_config, #name);                                          \
                                                                                                            \
        if (json_is_string(json_##name))                                                                    \
        {                                                                                                   \
            const char *str_##name = json_string_value(json_##name);                                        \
            const size_t length = strlen(str_##name);                                                       \
                                                                                                            \
            char *str_lowercase_##name = calloc(length, sizeof(char));                                      \
                                                                                                            \
            if (str_lowercase_##name)                                                                       \
            {                                                                                               \
                for (size_t index_options = 0; index_options < number_of_options; index_options += 1)       \
                {                                                                                           \
                    if (strstr(str_##name, string_array[index_options]))                                    \
                    {                                                                                       \
                        selected_index_options = index_options;                                             \
                        config->name = values_array[index_options];                                         \
                    }                                                                                       \
                }                                                                                           \
                                                                                                            \
                free(str_lowercase_##name);                                                                 \
            }                                                                                               \
        }                                                                                                   \
        if (overwrite)                                                                                      \
        {                                                                                                   \
            json_object_set_nocheck(json_config, #name, json_string(string_array[selected_index_options])); \
        }                                                                                                   \
    }

/*! \brief Macro that reads a read value from the json_t config objects and stores it in the user config.
 *  After reading the value it overwrites it so that it appears in the configuration for the user's convenience.
 *
 * \param[in] json_config the json_t* pointer to the configuration
 * \param[in] name the name of the parameter, should match the member in the user configuration
 * \param[in] default_value the default value to use if the parameter does not exist in the json_config
 * \param[in] config the pointer to the user configuration
 */
#define read_config_number(json_config, name, default_value, config) \
    read_config_number_overwrite(json_config, name, default_value, config, true)

/*! \brief Macro that reads boolean value from the json_t config objects and stores it in the user config.
 *  After reading the value it overwrites it so that it appears in the configuration for the user's convenience.
 *
 * \param[in] json_config the json_t* pointer to the configuration
 * \param[in] name the name of the parameter, should match the member in the user configuration
 * \param[in] default_value the default value to use if the parameter does not exist in the json_config
 * \param[in] config the pointer to the user configuration
 */
#define read_config_boolean(json_config, name, default_value, config) \
    read_config_boolean_overwrite(json_config, name, default_value, config, true)

/*! \brief Macro that reads a string value from the json_t config objects, compares it with a given set of options,
 *  and stores the value of the corresponding option in the user config.
 *  The default option is the first one of the array.
 *  After reading the value it overwrites it so that it appears in the configuration for the user's convenience.
 *
 * \param[in] json_config the json_t* pointer to the configuration
 * \param[in] name the name of the parameter, should match the member in the user configuration
 * \param[in] string_array the set of strings to compare the string to, it should have at least one element
 * \param[in] values_array the set of values corresponding to the strings
 * \param[in] config the pointer to the user configuration
 */
#define read_config_options(json_config, name, string_array, values_array, config) \
    read_config_options_overwrite(json_config, name, string_array, values_array, config, false)

#endif
