#ifndef __DIGITIZER_hpp__
#define __DIGITIZER_hpp__

// For fabs()
#include <cmath>

extern "C" {
#include <jansson.h>

#include "events.h"
}

#include <string>
#include <vector>

#define DIGITIZER_SUCCESS 1
#define DIGITIZER_FAILURE 0

namespace ABCD {

class Digitizer {
    unsigned int channels_number;
    int verbosity;
    bool enabled;
    std::string model;
    std::string name;

    std::vector<bool> channels_enabled;
    std::vector<bool> channels_triggering_enabled;

public:

    Digitizer(int Verbosity) : verbosity(Verbosity),
                               enabled(false),
                               model("Digitizer"),
                               name("")
                               { }
    virtual ~Digitizer() { }

    //--------------------------------------------------------------------------

    virtual int Initialize() { return DIGITIZER_FAILURE; }
    virtual int ReadConfig(json_t* /*config*/) { return DIGITIZER_FAILURE; }
    virtual int Configure() { return DIGITIZER_FAILURE; }

    //--------------------------------------------------------------------------

    virtual void SetModel(std::string M) { model = M; }
    virtual const std::string GetModel() const { return model; }

    virtual void SetName(std::string N) { name = N; }
    virtual const std::string GetName() const { return name; }

    virtual void SetChannelsNumber(unsigned int n) {
        channels_enabled.resize(n, false);
        channels_triggering_enabled.resize(n, false);
        channels_number = n;
    }
    virtual unsigned int GetChannelsNumber() const { return channels_number; }

    virtual void SetVerbosity(int v) { verbosity = v; }
    virtual int GetVerbosity() const { return verbosity; }

    //--------------------------------------------------------------------------

    virtual void SetEnabled(bool e) { enabled = e; }
    virtual bool IsEnabled() const { return enabled; }

    virtual void SetChannelEnabled(unsigned int ch, bool enabled) {
        if (ch < GetChannelsNumber()) {
            channels_enabled[ch] = enabled;
        }
    }
    virtual bool IsChannelEnabled(unsigned int ch) const {
        if (ch < GetChannelsNumber()) {
            return channels_enabled[ch];
        } else {
            return false;
        }
    }

    virtual void SetChannelTriggering(unsigned int ch, bool enabled) {
        if (ch < GetChannelsNumber()) {
            channels_triggering_enabled[ch] = enabled;
        }
    }
    virtual bool IsChannelTriggering(unsigned int ch) const {
        if (ch < GetChannelsNumber()) {
            return channels_triggering_enabled[ch];
        } else {
            return false;
        }
    }

    //--------------------------------------------------------------------------

    virtual int StartAcquisition() { return DIGITIZER_FAILURE; }
    virtual int RearmTrigger() { return DIGITIZER_FAILURE; }
    virtual int StopAcquisition() { return DIGITIZER_FAILURE; }
    virtual int ForceSoftwareTrigger() { return DIGITIZER_FAILURE; }
    virtual int ResetOverflow() { return DIGITIZER_FAILURE; }

    virtual bool AcquisitionReady() { return false; }
    virtual bool DataOverflow() { return false; }

    virtual int GetWaveformsFromCard(std::vector<struct event_waveform>& /*waveforms*/) {
        return DIGITIZER_FAILURE;
    }
};
}

#endif
