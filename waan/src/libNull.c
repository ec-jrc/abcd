/*! \brief Library that simply forwards the waveform to the energy analysis
 *         without doing anything.
 *  
 * This library does not perform any analysis on the waveform and it simply
 * forwards it to the energy_analysis(). It is useful if the user is not
 * interested in determining timing information from the pulse or if the user
 * wants to perform all the analysis in the energy analysis step.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "events.h"
#include "analysis_functions.h"
#include "DSP_functions.h"

void timestamp_analysis(const uint16_t *samples,
                        uint32_t samples_number,
                        uint32_t *trigger_position,
                        struct event_waveform *waveform,
                        struct event_PSD *event,
                        int8_t *select_event,
                        void *user_config)
{
    UNUSED(samples);
    UNUSED(samples_number);
    UNUSED(user_config);

    // Output
    (*trigger_position) = 0;
    event->timestamp = waveform->timestamp;
    (*select_event) = SELECT_TRUE;
}
