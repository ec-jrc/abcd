#ifndef __EVENTS_HPP__
#define __EVENTS_HPP__ 1

#include <cstring>
#include <cstdint>
#include <vector>

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

    inline event_PSD(uint64_t T, uint16_t s, uint16_t l, uint16_t b, uint8_t c, uint8_t p = 0) : \
        timestamp(T), qshort(s), qlong(l), baseline(b), channel(c), pur(p) {};
    inline ~event_PSD() {};
};

struct event_waveform
{
    const uint64_t timestamp;
    const uint8_t channel;
    const uint32_t samples_number;
    const uint8_t additional_waveforms;

    std::vector<uint16_t> samples;

    std::vector<std::vector<uint8_t>> additional_samples;

    inline event_waveform(uint64_t Timetag,
                          uint8_t Channel,
                          uint32_t Samples_Number,
                          uint8_t Additional_Waveforms = 0)
                          :
                          timestamp(Timetag),
                          channel(Channel),
                          samples_number(Samples_Number),
                          additional_waveforms(Additional_Waveforms)
    {
        samples.resize(samples_number);

        additional_samples.resize(additional_waveforms);

        for (size_t i = 0; i < additional_waveforms; i++)
        {
            additional_samples[i].resize(samples_number);
        }
    }

    inline ~event_waveform() {};

    inline size_t size() const
    {
        const size_t total_size = sizeof(timestamp)
                                + sizeof(channel)
                                + sizeof(samples_number)
                                + sizeof(additional_waveforms)
                                + sizeof(uint16_t) * samples_number
                                + sizeof(uint8_t) * samples_number * additional_waveforms;

        return total_size;
    }

    inline std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> output_buffer(size());

        memcpy(output_buffer.data(),
               reinterpret_cast<const void*>(&timestamp),
               sizeof(timestamp));

        memcpy(output_buffer.data() + sizeof(timestamp),
               reinterpret_cast<const void*>(&channel),
               sizeof(channel));

        memcpy(output_buffer.data() + sizeof(timestamp)
                                    + sizeof(channel),
               reinterpret_cast<const void*>(&samples_number),
               sizeof(samples_number));

        memcpy(output_buffer.data() + sizeof(timestamp)
                                    + sizeof(channel)
                                    + sizeof(samples_number),
               reinterpret_cast<const void*>(&additional_waveforms),
               sizeof(additional_waveforms));

        memcpy(output_buffer.data() + sizeof(timestamp)
                                    + sizeof(channel)
                                    + sizeof(samples_number)
                                    + sizeof(additional_waveforms),
               reinterpret_cast<const void*>(samples.data()),
               sizeof(uint16_t) * samples_number);

        for (size_t i = 0; i < additional_waveforms; i++)
        {
            memcpy(output_buffer.data() + sizeof(timestamp)
                                        + sizeof(channel)
                                        + sizeof(samples_number)
                                        + sizeof(additional_waveforms)
                                        + sizeof(uint16_t) * samples_number
                                        + sizeof(uint8_t) * samples_number * i,
                   reinterpret_cast<const void*>(additional_samples[i].data()),
                   sizeof(uint8_t) * samples_number);
        }

        return output_buffer;
    }
};

// Compile time error to check the size of event_PSD
static_assert(sizeof(struct event_PSD) == 16, \
"On this architecture the size of event_PSD does not match the expected value");

#endif
