#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "selection.h"
#include "events.h"

#define UNUSED(x) (void)(x)

/*! An initialization function for auxiliary data to be given to the
 *  select_event() function. It shall return a pointer to a block of data that
 *  contains the auxiliary data. The users must take care of cleaning up the
 *  memory.
 *
 * \return A pointer to the allocated data.
 */
void *select_init()
{
    return NULL;
}

/*! The events selection function. It shall return a boolean value that
 *  indicated whether the event should be forwarded or not.
 *  The user shall make sure that user_data is a valid pointer.
 *  Warning: This function should be as fast as possible, as it is run
 *  on every event.
 *
 * \param samples_number The number of samples in the waveform.
 * \param samples A pointer to the samples array.
 * \param baseline_end The position from which the integrals are calculated, as provided by the user or calculared by the CFD.
 * \param timestamp The timestamp of the event.
 * \param qshort The calculated qshort value of the event, as a double.
 * \param qlong The calculated qlong value of the event, as a double.
 * \param baseline The calculatd baseline of the waveform, as a double.
 * \param channel The channel number.
 * \param PUR The Pile-Up Rejection flag (currently not calculated).
 * \param user_data Pointer to the current event, in order to allow edits by the user.
 * \param user_data The pointer to the auxiliary data.
 * \return A boolean value to select the event.
 */
bool select_event(uint32_t samples_number,
                  const uint16_t *samples,
                  size_t baseline_end,
                  uint64_t timestamp,
                  double qshort,
                  double qlong,
                  double baseline,
                  uint8_t channel,
                  uint8_t PUR,
                  struct event_PSD *event,
                  void *user_data)
{
    // These are not necessary, they are only used to avoid warnings.
    //UNUSED(samples_number);
    //UNUSED(samples);
    UNUSED(baseline_end);
    UNUSED(timestamp);
    UNUSED(qshort);
    UNUSED(qlong);
    //UNUSED(baseline);
    UNUSED(channel);
    UNUSED(PUR);
    UNUSED(event);
    UNUSED(user_data);

    const double THRESHOLD = 50;

    size_t index_min = 0;
    size_t index_max = 0;
    double minimum = samples[0];
    double maximum = samples[0];

    for (size_t n = 0; n < samples_number; ++n)
    {
        if (minimum > samples[n])
        {
            minimum = samples[n];
            index_min = n;
        }
        if (maximum < samples[n])
        {
            maximum = samples[n];
            index_max = n;
        }
    }

    return (maximum - baseline) < THRESHOLD;
}

/*! A clean-up function in which the user shall deallocate the memory
 *  allovated for the auxiliary data.
 *  The user shall make sure that user_data is a valid pointer.
 *
 * \param user_data The pointer to the auxiliary data.
 * \return An integer indetifying potential errors. A zero means no error.
 */
int select_close(void *user_data)
{
    return 0;
}

