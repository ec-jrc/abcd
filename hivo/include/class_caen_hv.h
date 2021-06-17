#ifndef CAEN_CLASS_HV_H
#define CAEN_CLASS_HV_H

#ifndef MAXC_HV
#define MAXC_HV 6
#endif

#include <math.h>
#include <stdio.h>
#include <iostream>
// linking C++ compatibility
extern "C" {
    #include "CAENComm.h"
}

#ifdef LOGBOOK
#include "class_logbook.h"
#endif

class CAENHV {

private:
#ifdef LOGBOOK
    LogBook  *logbook;
#endif
    int      modelNo;
    int      formFactor;
    int      i;            // iterator
    int      handle;        // device handler
    int      isActive;    // active or not
    uint16_t data;        // data from VME
    int      error, prev_error;
    uint32_t board_base_address;
    int      status_array[13];
    // registers
    uint32_t address_chspacing;
    uint32_t address_vset_ch[MAXC_HV];
    uint32_t address_iset_ch[MAXC_HV];
    uint32_t address_rmup_ch[MAXC_HV];
    uint32_t address_rmdn_ch[MAXC_HV];
    uint32_t address_vmax_ch[MAXC_HV];
    uint32_t address_vmon_ch[MAXC_HV];
    uint32_t address_imon_ch[MAXC_HV];
    uint32_t address_ctrl_ch[MAXC_HV];
    uint32_t address_stat_ch[MAXC_HV];
    // value multipliers
    uint32_t units_vset;
    uint32_t units_iset;
    uint32_t units_rmup;
    uint32_t units_rmdn;
    float    units_vmax;
    uint32_t units_vmon;
    uint32_t units_imon;
    // firmware data & factory settings
    unsigned int maj_rel_num, min_rel_num, vme_maj_rel_num, vme_min_rel_num;
    char     descr[20];
    char     model[8];
    unsigned int        chnum, sernum;
    int      polarity[MAXC_HV];
    // error handling - signals & slots
    void     EmitError(const char *reference = "");

public:
#ifdef LOGBOOK
    CAENHV(LogBook *_logbook, int _modelNo = 6533);
#else
    CAENHV(int _modelNo = 6533);
#endif
    virtual ~CAENHV();
    // activation
    void                Activate(int interface, int linknum, int conetnode, uint32_t address, int forced_handle = -1);
    void                Deactivate();
    // error handling
    int                GetError();
    const char*        GetErrorDesc();
    // board information
    unsigned int    GetMajRelNum();
    unsigned int    GetMinRelNum();
    unsigned int    GetVmeMajRelNum();
    unsigned int    GetVmeMinRelNum();
    unsigned int    GetSerNum();
    unsigned int    GetChNum();
    char*                GetDescr();
    char*                GetModel();
    int                GetModelNo();
    void                InitializeModel(int model_index);
    // monitoring methods
    int                IsActive();
    int                IsChannelOn(int channel);
    int*                GetStatusArray(void);
    int*                GetChStatusArray(int channel);
    const char*        GetStatusDesc(int bit);
    const char*        GetChStatusDesc(int bit);
    int                GetChannelTemperature(int channel);
    int                GetChannelPower(int channel);
    int                GetChannelPolarity(int channel);
    float                GetVMax(void);
    float                GetIMax(void);
    float                GetChannelVoltage(int channel);
    float                GetChannelCurrent(int channel);
    float                GetChannelSVMax(int channel);
    float                GetChannelVoltageSetting(int channel);
    float                GetChannelCurrentSetting(int channel);
    float                GetChannelTripTime(int channel);
    float                GetChannelRampUp(int channel);
    float                GetChannelRampDown(int channel);
    int                GetChannelPowerDownMode(int channel);
    // operations
    void                SetChannelVoltage(int channel, int voltage);
    void                SetChannelCurrent(int channel, int current);
    void                SetChannelSVMax(int channel, int voltage);
    void                SetChannelOff(int channel);
    void                SetChannelOn(int channel);
    void                SetChannelTripTime(int channel, int time);
    void                SetChannelRampUp(int channel, int v_over_s);
    void                SetChannelRampDown(int channel, int v_over_s);
    void                SetChannelPowerDownMode(int channel, int mode);
    // conversion units
    uint32_t            GetUnitsVoltage();
    uint32_t            GetUnitsCurrent();
    float                GetUnitsSVMax();
    uint32_t            GetUnitsVoltageSetting();
    uint32_t            GetUnitsCurrentSetting();
    uint32_t            GetUnitsRampUp();
    uint32_t            GetUnitsRampDown();
    // general register tools
    uint16_t            GetRegisterSpecificBits(uint16_t reg_add, uint8_t bit_lower, uint8_t bit_upper, const char *reference = "");
    void                SetRegisterSpecificBits(uint16_t reg_add, uint8_t bit_lower, uint8_t bit_upper, uint16_t value, const char *reference = "");
};

#endif
