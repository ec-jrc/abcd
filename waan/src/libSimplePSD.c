#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_functions.h"
#include "events.h"

/*! \brief The number of samples to be averaged at the pulse begin, in order to
 *         determine the baseline.
 */
#define BASELINE_SAMPLES 64

/*! \brief Defining if the pulse is positive.
 */
#define POSITIVE_PULSE false

/*! \brief The start position of the integration gates.
 */
#define INTEGRATION_START 110

/*! \brief The short integration gate.
 */
#define GATE_SHORT 30

/*! \brief The long integration gate.
 */
#define GATE_LONG 90

/*! \brief Optional init function. 
 */
void energy_init(json_t* json_config, void** user_config)
{
    UNUSED(json_config);
    UNUSED(user_config);
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
    UNUSED(user_config);

    // Assuring that there is one event_PSD and discarding others
    reallocate_buffers(trigger_positions, events_buffer, events_number, 1);

    // Calculating the baseline
    double baseline = 0;

    for (uint32_t i = 0; (i < BASELINE_SAMPLES) && (i < samples_number); i++) {

        baseline += samples[i];
    }

    baseline /= BASELINE_SAMPLES;

    double qshort = 0;

    for (uint32_t i = INTEGRATION_START; (i < (INTEGRATION_START + GATE_SHORT)) && (i < samples_number); i++) {

        if (POSITIVE_PULSE) {
            qshort += (samples[i] - baseline);
        } else {
            qshort += (baseline - samples[i]);
        }
    }

    double qlong = 0;

    for (uint32_t i = INTEGRATION_START; (i < (INTEGRATION_START + GATE_LONG)) && (i < samples_number); i++) {

        if (POSITIVE_PULSE) {
            qlong += (samples[i] - baseline);
        } else {
            qlong += (baseline - samples[i]);
        }
    }

    // Output of the function
    (*events_buffer)[0].timestamp = waveform->timestamp;
    (*events_buffer)[0].qshort = qshort;
    (*events_buffer)[0].qlong = qlong;
    (*events_buffer)[0].baseline = baseline;
    (*events_buffer)[0].channel = waveform->channel;
    (*events_buffer)[0].group_counter = 0;
}
