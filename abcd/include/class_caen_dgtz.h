#ifndef CAEN_DGTZ_CLASS_H
#define CAEN_DGTZ_CLASS_H

#ifndef MAXC_DG
#define MAXC_DG 16
#endif

//using namespace std;

// linux specific includes
#define Sleep(x) usleep(x*1000)

#include <math.h>
#include <stdio.h>
#include <iostream>
#include <iomanip>    // setw
#include <cstdlib>    // atoi
#include <fstream>
#include <sstream>
#include <string>

#include <json/json.h>

#ifdef LOGBOOK
#include "class_logbook.h"
#endif

// linking C++ compatibility
extern "C" {
    #include "CAENDigitizer.h"
}

class CAENDgtz {
private:
#ifdef LOGBOOK
    LogBook    *logbook;
#endif
    int        i;                            // iterator
    //int        id;                        // identifier
    std::string    boardName;                // board name
    std::string    channelName[MAXC_DG];        // channel name
    int        handle;                    // device handler
    int        isActive;                // active or not
    int        isMaster;                // master or slave in syncronized chain
    int        showGates;                // show integration gates
    int        writeBuffer;            // writes raw CAEN buffer
    int        verboseDebug;            // outputs error messages
    uint16_t    specThr[MAXC_DG];        // spectrum lower threshold
    int        error;                    // return value from CAENDigitizer functions
    int        *ext_errcode;
    std::string    *ext_errdesc;

    uint32_t    board_base_address;
    //CAEN_DGTZ_ConnectionType    conntype;
    CAEN_DGTZ_BoardInfo_t        boardInfoFromLib;
    // structs
    struct boardInfo_t {
        // CAEN_DGTZ native values
        char            ModelName[12];
        uint32_t        Model;
        uint32_t        Channels;
        uint32_t        FormFactor;
        uint32_t        FamilyCode;
        char            ROC_FirmwareRel[20];
        char            AMC_FirmwareRel[40];
        uint32_t        SerialNumber;
        uint32_t        PCB_Revision;
        uint32_t        ADC_NBits;
        // custom values
        uint16_t        vers;
        uint8_t        dppVersion;                    // standard or DPP
        uint8_t        nsPerSample;                // time resolution
        uint8_t        nsPerTimetag;                // time resolution
    } boardInfo;
    // error handling - signals & slots
    void        EmitError(const char *reference = "");

public:
// ------------------------
// structs
    struct acqStatus_t {
        uint32_t    ready;
        uint32_t    pllLock;
        uint32_t    pllBypass;
        uint32_t    clockSource;
        uint32_t    eventFull;
        uint32_t    eventReady;
        uint32_t    run;
    };
    struct vmeStatus_t {
        uint32_t    busErr;
        uint32_t    busFull;
        uint32_t    eventReady;
    };    struct chnStatus_t {
        uint32_t    bufErr;
        uint32_t    dacBusy;
        uint32_t    memEmpty;
        uint32_t    memFull;
    };
    // constructor & destructor
#ifdef LOGBOOK
    CAENDgtz(LogBook *_logbook);
#else
    CAENDgtz();
#endif
    virtual ~CAENDgtz();
    // general methods
    void        Activate(int interface = 0, int linknum = 0, int conetnode = 0, uint32_t address = 0x32100000);
    void        Deactivate();
    void        SetVerboseDebug(int mode);
    void        SetExternalErrorCode(int *ext_errc);
    void        SetExternalErrorDesc(std::string *ext_errd);
    int         IsActive(int silent = 0);
    int         IsMaster();
    void        Reset();
    void        SetBoardName(const char *name);
    const char* GetBoardName();
    void        SetChannelName(int channel, const char *name);
    const char* GetChannelName(int channel);
    int         GetError();
    const char* GetErrorDesc();
    int         GetHandle();
    CAEN_DGTZ_BoardInfo_t    GetInfo();
    int         GetModel();
    const char* GetModelName();
    unsigned int     GetNumChannels();
    uint8_t     GetNsPerSample();
    uint8_t     GetNsPerTimetag();
    uint8_t     GetDPPVersion();
    int         GetShowGates();
    int         GetWriteBuffer();
    // register operations
    uint32_t    GetRegisterSpecificBits(uint32_t reg_add, uint8_t bit_lower, uint8_t bit_upper, const char *reference = "GetRegisterSpecificBits");
    void        SetRegisterSpecificBits(uint32_t reg_add, uint8_t bit_lower, uint8_t bit_upper, uint32_t value, const char *reference = "SetRegisterSpecificBits");
    void        WriteRegister(uint32_t address, uint32_t data, const char *reference = "WriteRegister");
    uint32_t    ReadRegister(uint32_t address, const char *reference = "unknown function");
    const char* DumpRegister(uint32_t address, int print = 1);
    void        SetFanSpeed(int value);
    int         GetFanSpeed();
    // operations
    void        SetChannelEnableMask(uint32_t mask);
    uint32_t    GetChannelEnableMask();
    void        SetCoincidenceMask(uint32_t mask);
    uint32_t    GetCoincidenceMask();
    void        SetCoincidenceLevel(uint8_t level);
    uint32_t    GetCoincidenceLevel();
    void        SetSWTriggerMode(uint32_t mode);
    uint32_t    GetSWTriggerMode();
    void        SetAcquisitionMode(uint32_t mode);
    uint32_t    GetAcquisitionMode();
    void        SetExtTriggerInputMode(uint32_t mode);
    uint32_t    GetExtTriggerInputMode();
    void        SetEventPackaging(uint32_t value);
    uint32_t    GetEventPackaging();
    void        SetTriggerUnderThreshold(uint32_t value);
    uint32_t    GetTriggerUnderThreshold();
    void        SetMemoryAccess(uint32_t value);
    uint32_t    GetMemoryAccess();
    void        SetTestPatternGeneration(uint32_t value);
    uint32_t    GetTestPatternGeneration();
    void        SetTriggerOverlap(uint32_t value);
    uint32_t    GetTriggerOverlap();
    void        SetDPPEventAggregation(uint32_t value1, uint32_t value2);
    void        SetBufferBlockNumber(uint32_t value);
    uint32_t    GetBufferBlockNumber();
    void        ConfigureFromFile(const char *conf_file_name);
    void        ConfigureFromJSON(const Json::Value &config);
    // front panel I/O control
    void        SetIOTrgOutProgram(uint32_t value);
    uint32_t    GetIOTrgOutProgram();
    void        SetIOTrgOutDisplay(uint32_t value);
    uint32_t    GetIOTrgOutDisplay();
    void        SetIOTrgOutMode(uint32_t value);
    uint32_t    GetIOTrgOutMode();
    void        SetIOTrgOutTestMode(uint32_t value);
    uint32_t    GetIOTrgOutTestMode();
    void        SetIOExtTrgPropagation(uint32_t value);
    int         GetIOExtTrgPropagation();
    void        SetIOPatternLatchMode(uint32_t value);
    uint32_t    GetIOPatternLatchMode();
    void        SetIOMode(uint32_t value);
    uint32_t    GetIOMode();
    void        SetIOLVDSFourthBlkOutputs(uint32_t value);
    uint32_t    GetIOLVDSFourthBlkOutputs();
    void        SetIOLVDSThirdBlkOutputs(uint32_t value);
    uint32_t    GetIOLVDSThirdBlkOutputs();
    void        SetIOLVDSSecondBlkOutputs(uint32_t value);
    uint32_t    GetIOLVDSSecondBlkOutputs();
    void        SetIOLVDSFirstBlkOutputs(uint32_t value);
    uint32_t    GetIOLVDSFirstBlkOutputs();
    void        SetIOHighImpedence(uint32_t value);
    uint32_t    GetIOHighImpedence();
    void        SetIOLevel(uint32_t level);
    uint32_t    GetIOLevel();
    // VME control
    void        SetInterruptMode(uint32_t mode);
    uint32_t    GetInterruptMode();
    void        SetEnabledRELOC(uint32_t mode);
    uint32_t    GetEnabledRELOC();
    void        SetEnabledALIGN64(uint32_t mode);
    uint32_t    GetEnabledALIGN64();
    void        SetEnabledBERR(uint32_t mode);
    uint32_t    GetEnabledBERR();
    void        SetEnabledOLIRQ(uint32_t mode);
    uint32_t    GetEnabledOLIRQ();
    void        SetInterruptLevel(uint32_t value);
    uint32_t    GetInterruptLevel();
    void        SetInterruptStatusID(uint32_t value);
    uint32_t    GetInterruptStatusID();
    void        SetInterruptEventNumber(uint32_t value);
    uint32_t    GetInterruptEventNumber();
    void        IRQWait(uint32_t timeout);
    void        SetChannelTriggerHoldOff(int channel, int value);
    int         GetChannelTriggerHoldOff(int channel);
    void        SetChannelTriggerHoldOffNs(int channel, int value);
    int         GetChannelTriggerHoldOffNs(int channel);
    // coincidences
    void        SetEnabledCoincidencesOnBoard(int value);
    int         GetEnabledCoincidencesOnBoard();
    void        SetChannelCoincidenceMode(int channel, int mode);
    int         GetChannelCoincidenceMode(int channel);
    void        SetChannelCoincidenceWindowNs(int channel, int ns);
    int         GetChannelCoincidenceWindowNs(int channel);
    void        SetChannelCoincMask(int channel, int mask);
    int         GetChannelCoincMask(int channel);
    void        SetChannelCoincLogic(int channel, int logic);
    int         GetChannelCoincLogic(int channel);
    void        SetChannelCoincMajorityLevel(int channel, int level);
    int         GetChannelCoincMajorityLevel(int channel);
    void        SetChannelCoincExtTriggerMask(int channel, int mask);
    int         GetChannelCoincExtTriggerMask(int channel);
    // coincidences - COUPLES in x730
    void        SetCoupleTriggerOutMode(int couple, int mode);
    int         GetCoupleTriggerOutMode(int couple);
    void        SetCoupleTriggerOutEnabled(int couple, int enabled);
    int         GetCoupleTriggerOutEnabled(int couple);
    void        SetCoupleValidationSource(int couple, int source);
    int         GetCoupleValidationSource(int couple);
    void        SetCoupleValidationEnabled(int couple, int enabled);
    int         GetCoupleValidationEnabled(int couple);
    void        SetChannelValidationMode(int channel, int mode);
    int         GetChannelValidationMode(int channel);
    void        SetCoupleValidationMask(int couple, int mask);
    int         GetCoupleValidationMask(int couple);
    void        SetCoupleValidationLogic(int couple, int logic);
    int         GetCoupleValidationLogic(int couple);
    void        SetCoupleValidationMajorityLevel(int couple, int level);
    int         GetCoupleValidationMajorityLevel(int couple);
    void        SetCoupleValidationExtTriggerMask(int couple, int mask);
    int         GetCoupleValidationExtTriggerMask(int couple);
    void        SetCoupleVetoMode(uint32_t couple, uint32_t value);
    uint32_t    GetCoupleVetoMode(uint32_t couple);
    void        SetChannelVetoWindow(uint32_t channel, uint32_t value);
    uint32_t    GetChannelVetoWindow(uint32_t channel);
    // monitoring
    uint32_t    GetEventStoredNumber();
    acqStatus_t GetAcquisitionStatus();
    vmeStatus_t GetVMEStatus();
    chnStatus_t GetChannelStatus(uint32_t channel);
    uint32_t    GetChannelBufferOccupancy(uint32_t channel);
    // acquisition
    void        ClearData();
    void        SWStartAcquisition();
    void        SWStopAcquisition();
    void        SendSWTrigger();
    void        ReadData(CAEN_DGTZ_ReadMode_t mode, char *buffer, uint32_t *bufferSize);
    void        MallocReadoutBuffer(char **buffer, uint32_t *size);
    void        FreeReadoutBuffer(char **buffer);
    // *751 Dual Edge Mode
    void        SetDualEdgeMode(int value);
    int         GetDualEdgeMode();
// ------------------------
// firmware specific methods
    void        SetChannelScopeSamples(uint32_t channel, uint32_t samples);
    uint32_t    GetChannelScopeSamples(uint32_t channel = 255);
    void        SetChannelSelfTrigger(uint32_t channel, uint32_t mode);
    uint32_t    GetChannelSelfTrigger(uint32_t channel);
    void        SetChannelTriggerThreshold(uint32_t channel, uint32_t threshold);
    uint32_t    GetChannelTriggerThreshold(uint32_t channel);
    void        SetChannelTriggerPolarity(uint32_t channel, uint32_t polarity);
    uint32_t    GetChannelTriggerPolarity(uint32_t channel);
    void        SetChannelTriggerNSamples(uint32_t channel, uint32_t nsamples);
    uint32_t    GetChannelTriggerNSamples(uint32_t channel);
    void        SetChannelDCOffsetPercent(uint32_t channel, int value);
    void        SetChannelDCOffset10bit(uint32_t channel, uint32_t offset);
    void        SetChannelDCOffset12bit(uint32_t channel, uint32_t offset);
    void        SetChannelDCOffset14bit(uint32_t channel, uint32_t offset);
    void        SetChannelDCOffset16bit(uint32_t channel, uint32_t offset);
    uint32_t    GetChannelDCOffset(uint32_t channel);
    uint32_t    GetChannelDCOffset10bit(uint32_t channel);
    uint32_t    GetChannelDCOffset12bit(uint32_t channel);
    uint32_t    GetChannelDCOffset14bit(uint32_t channel);
    uint32_t    GetChannelDCOffset16bit(uint32_t channel);
    int         GetChannelDCOffsetPercent(uint32_t channel);
    void        SetMaxNumEventsBLT(uint32_t maxNumEvents);
    uint32_t    GetMaxNumEventsBLT();
    void        SetPostTriggerSize(uint32_t percent);
    uint32_t    GetPostTriggerSize();
    void        SetPostTriggerSamples(uint32_t samples);
    uint32_t    GetPostTriggerSamples();
    void        SetRunStartDelaySamples(uint32_t delay);
    uint32_t    GetRunStartDelaySamples();
    void        SetRunStartDelayNanoseconds(uint32_t delay);
    uint32_t    GetRunStartDelayNanoseconds();
    // zero suppression
    void        SetZeroSuppressionMode(uint32_t mode);
    uint32_t    GetZeroSuppressionMode();
    void        SetChannelZSThresholdWeight(uint32_t channel, int32_t weight);
    int32_t     GetChannelZSThresholdWeight(uint32_t channel);
    void        SetChannelZSLogic(uint32_t channel, uint32_t logic);
    uint32_t    GetChannelZSLogic(uint32_t channel);
    void        SetChannelZSThreshold(uint32_t channel, uint32_t threshold);
    uint32_t    GetChannelZSThreshold(uint32_t channel);
    void        SetChannelZSAMPNSamples(uint32_t channel, int32_t nsamples);
    int32_t     GetChannelZSAMPNSamples(uint32_t channel);
    void        SetChannelZSZLENSamplesBeforeThr(uint32_t channel, int32_t nsamples);
    int32_t     GetChannelZSZLENSamplesBeforeThr(uint32_t channel);
    void        SetChannelZSZLENSamplesAfterThr(uint32_t channel, int32_t nsamples);
    int32_t     GetChannelZSZLENSamplesAfterThr(uint32_t channel);
    // acquisition
    void        AllocateEvent(void **Evt);
    void        DecodeEvent(char *evtPtr, void **Evt);
    void        FreeEvent(void **Evt);
    CAEN_DGTZ_EventInfo_t    GetEventInfo(char *buffer, uint32_t buffsize, int32_t numEvent, char **EventPtr);
    uint32_t    GetNumEvents(char *buffer, uint32_t buffsize);
// ------------------------
// DPP-CI  specific methods
    int         isPowerOfTwo(unsigned int x);
    void        SetDPPAcquisitionMode(int mode);
    uint32_t    GetDPPAcquisitionMode();
    void        SetDPPSaveParameter(CAEN_DGTZ_DPP_SaveParam_t param);
    uint32_t    GetDPPSaveParameter();
// ------------------------
// DPP-PSD specific methods
    void        ConfigSetPSDMandatoryBits();
    void        SetChannelPSDRatioFiltering(uint32_t channel, uint32_t value);
    uint32_t    GetChannelPSDRatioFiltering(uint32_t channel);
    void        SetChannelPSDRatioThresholdPercent(uint32_t channel, uint32_t value);
    uint32_t    GetChannelPSDRatioThresholdPercent(uint32_t channel);
    void        SetMaxNumAggregatesBLT(uint32_t maxNumEvents);
    uint32_t    GetMaxNumAggregatesBLT();
    void        SetChannelMaxNumEventsPerAggregate(uint32_t channel, uint32_t maxNumEvents);
    uint32_t    GetChannelMaxNumEventsPerAggregate(uint32_t channel);
    void        SetDualTrace(uint32_t value);
    uint32_t    GetDualTrace();
    void        SetAnalogProbe(uint32_t value);
    uint32_t    GetAnalogProbe();
    void        SetDigitalProbe(uint32_t value);
    uint32_t    GetDigitalProbe();
    void        SetEnabledScope(uint32_t value);
    uint32_t    GetEnabledScope();
    void        SetEnabledBSL(uint32_t value);
    uint32_t    GetEnabledBSL();
    void        SetEnabledTime(uint32_t value);
    uint32_t    GetEnabledTime();
    void        SetEnabledCharge(uint32_t value);
    uint32_t    GetEnabledCharge();
    void        SetEnabledEventDataFormatWord(uint32_t value);
    uint32_t    GetEnabledEventDataFormatWord();
    void        SetChannelPreTrigger(uint32_t channel, uint32_t value);
    uint32_t    GetChannelPreTrigger(uint32_t channel);
    uint8_t     GetGateShortBitSize();
    uint8_t     GetGateLongBitSize();
    void        SetChannelThreshold(uint32_t channel, uint32_t value);
    uint32_t    GetChannelThreshold(uint32_t channel);
    void        SetChannelPreGate(uint32_t channel, uint32_t value);
    uint32_t    GetChannelPreGate(uint32_t channel);
    void        SetChannelLongGate(uint32_t channel, uint32_t value);
    uint32_t    GetChannelLongGate(uint32_t channel);
    void        SetChannelShortGate(uint32_t channel, uint32_t value);
    uint32_t    GetChannelShortGate(uint32_t channel);
    void        SetChannelPreGateNs(uint32_t channel, uint32_t value);
    uint32_t    GetChannelPreGateNs(uint32_t channel);
    void        SetChannelLongGateNs(uint32_t channel, uint32_t value);
    uint32_t    GetChannelLongGateNs(uint32_t channel);
    void        SetChannelShortGateNs(uint32_t channel, uint32_t value);
    uint32_t    GetChannelShortGateNs(uint32_t channel);
    void        SetChannelPulsePolarity(uint32_t channel, uint32_t value);
    uint32_t    GetChannelPulsePolarity(uint32_t channel);
    void        SetChannelTriggerMode(uint32_t channel, uint32_t value);
    uint32_t    GetChannelTriggerMode(uint32_t channel);
    void        SetChannelChargeSensitivity(uint32_t channel, uint32_t value);
    uint32_t    GetChannelChargeSensitivity(uint32_t channel);
    void        SetChannelBSLSamples(uint32_t channel, uint32_t value);
    uint32_t    GetChannelBSLSamples(uint32_t channel);
    void        SetChannelBSLThreshold(uint32_t channel, uint32_t value);
    uint32_t    GetChannelBSLThreshold(uint32_t channel);
    void        SetChannelBSLTimeout(uint32_t channel, uint32_t value);
    uint32_t    GetChannelBSLTimeout(uint32_t channel);
    void        SetDPPPow2NumBuffers(uint32_t value);
    uint32_t    GetDPPPow2NumBuffers();
    void        SetNumEventsPerAggregate(uint32_t channel, uint32_t nEvPerBuff);
    uint32_t    GetNumEventsPerAggregate(uint32_t channel);
    void        SetChannelEnabledPUR(int channel, int value);
    uint32_t    GetChannelEnabledPUR(int channel);
    void        SetChannelPURGap(int channel, int value);
    uint32_t    GetChannelPURGap(int channel);
    void        MallocDPPEvents(void **Events, uint32_t *AllocatedSize);
    void        MallocDPPWaveforms(void **Waveforms, uint32_t *AllocatedSize);
    void        DecodeDPPWaveforms(void *WaveIn, void *Waveforms);
    void        GetDPPEvents(char *buffer, uint32_t BufferSize, void **Events, uint32_t *NumEvents);
    void        FreeDPPWaveforms(void *Waveforms);
    void        FreeDPPEvents(void **Events);
    // DPP_PHA specific methods
    void        SetChannelTriggerFilterSmoothing(uint32_t channel, uint32_t value);
    uint32_t    GetChannelTriggerFilterSmoothing(uint32_t channel);
    void        SetChannelInputRiseTime(uint32_t channel, uint32_t value);
    uint32_t    GetChannelInputRiseTime(uint32_t channel);
    void        SetChannelTrapezoidRiseTime(uint32_t channel, uint32_t value);
    uint32_t    GetChannelTrapezoidRiseTime(uint32_t channel);
    void        SetChannelTrapezoidFlatTop(uint32_t channel, uint32_t value);
    uint32_t    GetChannelTrapezoidFlatTop(uint32_t channel);
    void        SetChannelPeakingTime(uint32_t channel, uint32_t value);
    uint32_t    GetChannelPeakingTime(uint32_t channel);
    void        SetChannelDecayTime(uint32_t channel, uint32_t value);
    uint32_t    GetChannelDecayTime(uint32_t channel);
    void        SetChannelRTDWindowWidth(uint32_t channel, uint32_t value);
    uint32_t    GetChannelRTDWindowWidth(uint32_t channel);
    void        SetChannelPeakHoldOffExtension(uint32_t channel, uint32_t value);
    uint32_t    GetChannelPeakHoldOffExtension(uint32_t channel);
    void        SetChannelBaselineHoldOffExtension(uint32_t channel, uint32_t value);
    uint32_t    GetChannelBaselineHoldOffExtension(uint32_t channel);
    void        SetChannelTrapezoidRescaling(uint32_t channel, uint32_t value);
    uint32_t    GetChannelTrapezoidRescaling(uint32_t channel);
    void        SetChannelDecimation(uint32_t channel, uint32_t value);
    uint32_t    GetChannelDecimation(uint32_t channel);
    void        SetChannelDigitalGain(uint32_t channel, uint32_t value);
    uint32_t    GetChannelDigitalGain(uint32_t channel);
    void        SetChannelPeakAveragingWindow(uint32_t channel, uint32_t value);
    uint32_t    GetChannelPeakAveragingWindow(uint32_t channel);
    void        SetChannelEnabledRollOverEvents(uint32_t channel, uint32_t value);
    uint32_t    GetChannelEnabledRollOverEvents(uint32_t channel);
    void        SetChannelPileUpFlagging(int channel, int value);
    uint32_t    GetChannelPileUpFlagging(int channel);
    // *730 specific methods
    void        SetChannelDynamicRange(int channel, int value);
    int         GetChannelDynamicRange(int channel);
    int         GetChannelTemperature(int channel);
    int         GetMaximumTemperature();

    void        SetChannelSpectrumThreshold(uint32_t channel, uint32_t value);
    uint16_t    GetChannelSpectrumThreshold(uint32_t channel);
};

#endif
