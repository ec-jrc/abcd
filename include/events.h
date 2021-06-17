#ifndef __EVENTS_H__
#define __EVENTS_H__ 1

// For all the integers
#include <stdint.h>
// For malloc
#include <stdlib.h>
// For memcpy
#include <string.h>

struct event_PSD
{
    // It is necessary to have the size of this struct a multiple of 64 bits.
    // Thus we can be more confident that we have a binary structure
    // corresponding to the expectations.

    uint64_t timestamp;
    uint16_t qshort;
    uint16_t qlong;
    uint16_t baseline;
    uint8_t channel;
    uint8_t pur;
};

struct event_waveform
{
    uint64_t timestamp;
    uint8_t channel;
    uint32_t samples_number;
    uint8_t additional_waveforms;

    uint8_t *buffer;
};

inline extern size_t waveform_header_size()
{
    const struct event_waveform *empty = 0;

    return sizeof(empty->timestamp)
           + sizeof(empty->channel)
           + sizeof(empty->samples_number)
           + sizeof(empty->additional_waveforms);
}

inline extern size_t waveform_size(struct event_waveform *event)
{
    return waveform_header_size()
           + sizeof(uint16_t) * event->samples_number
           + sizeof(uint8_t) * event->samples_number * event->additional_waveforms;
}

inline extern void waveform_realloc(struct event_waveform *event)
{
    uint8_t *const new_buffer = (uint8_t*)realloc(event->buffer,
                                                  waveform_size(event) * sizeof(uint8_t));

    // In any case this should be fine: if the reallocation failed this is
    // going to store a NULL, in the other case this is going to store the new
    // pointer, while the old one is freed.
    event->buffer = new_buffer;
}

inline extern struct event_waveform waveform_create(uint64_t Timestamp,
                                                    uint8_t Channel,
                                                    uint32_t Samples_number,
                                                    uint8_t Additional_waveforms)
{
    struct event_waveform event;

    event.timestamp = Timestamp;
    event.channel = Channel;
    event.samples_number = Samples_number;
    event.additional_waveforms = Additional_waveforms;
    event.buffer = NULL;

    waveform_realloc(&event);

    return event;
}

inline extern uint32_t waveform_samples_get_number(struct event_waveform *event)
{
    return event->samples_number;
}

inline extern void waveform_samples_set_number(struct event_waveform *event,
                                               uint32_t Samples_number)
{
    event->samples_number = Samples_number;
    waveform_realloc(event);
}

inline extern uint8_t waveform_additional_get_number(struct event_waveform *event)
{
    return event->additional_waveforms;
}

inline extern void waveform_additional_set_number(struct event_waveform *event,
                                                  uint8_t Additional_waveforms)
{
    event->additional_waveforms = Additional_waveforms;
    waveform_realloc(event);
}

inline extern void waveform_samples_set(struct event_waveform *event,
                                        const uint16_t *Samples)
{
    if (event->buffer) {
        memcpy(event->buffer + waveform_header_size(),
               Samples,
               event->samples_number * sizeof(uint16_t));
    }
}

inline extern uint16_t* waveform_samples_get(struct event_waveform *event)
{
    if (event->buffer) {
        return (uint16_t*)(event->buffer + waveform_header_size());
    } else {
        return NULL;
    }
}

inline extern void waveform_additional_set(struct event_waveform *event,
                                           uint8_t Index,
                                           const uint8_t *Additional_samples)
{
    if (event->buffer) {
        memcpy(event->buffer + waveform_header_size() 
                             + sizeof(uint16_t) * event->samples_number
                             + sizeof(uint8_t) * event->samples_number * Index,
               Additional_samples,
               event->samples_number * sizeof(uint8_t));
    }
}

inline extern uint8_t* waveform_additional_get(struct event_waveform *event,
                                               uint8_t Index)
{
    if (event->buffer) {
        return (uint8_t*)(event->buffer + waveform_header_size() 
                + sizeof(uint16_t) * event->samples_number
                + sizeof(uint8_t) * event->samples_number * Index);
    } else {
        return NULL;
    }
}

inline extern void waveform_destroy_samples(struct event_waveform *event)
{
    if (event->buffer) {
        free(event->buffer);
    }
}

inline extern uint8_t* waveform_serialize(struct event_waveform *event)
{
    if (event->buffer) {
        memcpy(event->buffer,
               &event->timestamp,
               sizeof(event->timestamp));

        memcpy(event->buffer + sizeof(event->timestamp),
               &event->channel,
               sizeof(event->channel));

        memcpy(event->buffer + sizeof(event->timestamp)
                             + sizeof(event->channel),
               &event->samples_number,
               sizeof(event->samples_number));

        memcpy(event->buffer + sizeof(event->timestamp)
                             + sizeof(event->channel)
                             + sizeof(event->samples_number),
               &event->additional_waveforms,
               sizeof(event->additional_waveforms));
    }

    return event->buffer;
}

#endif
