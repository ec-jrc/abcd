/*! \brief Very simple example of determination of the energy information of a
 *         pulse by integration.
 *
 * Calculation procedure:
 *  1. The baseline is determined averaging the first N samples.
 *  2. The pulse is integrated over two intervals from the integration start
 *     position.
 *
 * In the event_PSD structure the energy information is stored in the qlong
 * entry. The qshort stored the value of the shorted integral. The configuration
 * is hard-coded in the source in order to make the example very easy.
 */

#include <stdbool.h>
#include <stdint.h>

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

    // Calculating the integrals
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
