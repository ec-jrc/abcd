// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <cstring>

// file includes
#include "class_caen_hv.h"
#include "CAENComm.h"

// VME HV adresses
#define ADD_VME_VMAX          0x0050
#define ADD_VME_IMAX          0x0054
#define ADD_VME_STATUS        0x0058
#define ADD_VME_FWREL         0x005C
#define ADD_VME_CHNUM         0x8100
#define ADD_VME_DESCR_FIRST   0x8102
#define ADD_VME_MODEL_FIRST   0x8116
#define ADD_VME_SERNUM        0x811E
#define ADD_VME_VME_FWREL     0x8120
#define ADD_VME_MCA_BINFO     0x8140
#define CH0_BASE_ADDR         0x0080
#define ADD_VME_CH0_TRIP_TIME 0x0098
#define ADD_VME_CH0_SVMAX     0x009C
#define ADD_VME_CH0_PWDOWN    0x00A8
#define ADD_VME_CH0_POLARITY  0x00AC
#define ADD_VME_CH0_TEMP      0x00B0


// constructor
#ifdef LOGBOOK
CAENHV::CAENHV(LogBook *_logbook, int _modelNo) : logbook(_logbook), modelNo(_modelNo), formFactor(0), isActive(0), error(0) {
#else
CAENHV::CAENHV(int _modelNo) : modelNo(_modelNo), formFactor(0), isActive(0), error(0) {
#endif

    // Resetting description
    memset(descr, 0, sizeof(descr) / sizeof(char));
}

// destructor
CAENHV::~CAENHV() {  }

void CAENHV::Deactivate() {
    isActive=0;
    error = (int)CAENComm_CloseDevice(handle);
    if (error!=0) { EmitError("Deactivate"); }
}

void CAENHV::InitializeModel(int model_index) {
    switch(model_index) {
        case    0: { modelNo = 6533; } break;
        case    1: { modelNo = 5780; } break;
        case    2: { modelNo = 5790; } break;
        case    3: { modelNo = 5533; } break;
        case 6533: { modelNo = 6533; } break;
        case 5780: { modelNo = 5780; } break;
        case 5790: { modelNo = 5790; } break;        
        case 5533: { modelNo = 5533; } break;
        default: { modelNo = -1; error = -12; EmitError("InitializeModel"); }
    }
}

// activation
void CAENHV::Activate(int interface, int linknum, int conetnode, uint32_t address, int forced_handle) {
    error = 0;
    if (isActive==0) {
        board_base_address = address;
        prev_error = -3;
        CAEN_Comm_ConnectionType conntype;
        switch(interface) {
            case 0:     { conntype = CAENComm_USB; } break;
            default: { conntype = CAENComm_OpticalLink; }
        }
        if (forced_handle == -1) {
            // normal connection
            error = (int)CAENComm_OpenDevice(conntype, linknum, conetnode, board_base_address, &handle);
            //std::cout << "Activating type " << conntype << ", link " << linknum << ", node " << conetnode;
            //std::cout << ", address = " << board_base_address << ", handle " << handle << " ERROR " << error << std::endl;
            //EmitError("Activate");
        } else {
            // user existing connection
            handle = forced_handle;
        }
        if (error==0) {
            isActive = 1;

            //cout << "[MODEL HV] " << modelNo << endl;

            // which kind of board are we connected to?
            // to implement automatic recognition
            // example: try to read register 0x8140: if !error -> MCA, else -> VME
            // error = (int)CAENComm_Read16(handle, ADD_VME_MCA_BINFO, &data);

      //error = (int)CAENComm_Read16(handle, 0x00B0, &data);

            switch (modelNo) {
                case 6533: {
                    // VME high voltage
                    formFactor = 0;
                    chnum = 0;
                    // reading and storing board parameters
                    // Microcontroller version
                    maj_rel_num = GetRegisterSpecificBits(0x005C,8,15,"FW Major release in Activate");
                    min_rel_num = GetRegisterSpecificBits(0x005C,0, 7,"FW Minor release in Activate");
                    //error = (int)CAENComm_Read16(handle, ADD_VME_FWREL, &data);
                    //if (error==0) { maj_rel_num = (0xff00 & data)/(0x0100); min_rel_num = (0x00ff & data); } else { EmitError("Read FWRel in Activate"); }
                    // VME FPGA version
                    error = (int)CAENComm_Read16(handle, ADD_VME_VME_FWREL, &data);
                    if (error==0) { vme_maj_rel_num = (0xff00 & data)/(0x0100); vme_min_rel_num = (0x00ff & data); } else { EmitError("Read VME FWRel in Activate()"); }
                    // channel number
                    error =(int)CAENComm_Read16(handle, ADD_VME_CHNUM, &data);
                    if (error==0) { chnum = data; } else { EmitError("Read ChNum in Activate"); }
                    // serial number
                    error =(int)CAENComm_Read16(handle, ADD_VME_SERNUM, &data);
                    if (error==0) { sernum = data; } else { EmitError("Read SerNum in Activate"); }
                    // model
                    for (i = 0; i<8; i+=2) {
                        error =(int)CAENComm_Read16(handle, ADD_VME_MODEL_FIRST+(0x0001*i), &data);
                        if (error==0) { model[i] = (0x00ff & data); model[i+1] = (0xff00 & data)/(0x0100); } else { EmitError("Read Model in Activate"); }
                    }
                    // description
                    for (i = 0; i<20; i+=2)
                    {
                        descr[i] = 0;
                        descr[i+1] = 0;

                        error = (int)CAENComm_Read16(handle, ADD_VME_DESCR_FIRST+(0x0001*i), &data);
                        if (error==0)
                        {
                            descr[i] = (0x00ff & data);
                            descr[i+1] = (0xff00 & data)/(0x0100);
                        }
                        else
                        {
                            EmitError("Read Descr in Activate");
                        }

                        std::cerr << "CAENHV: descr: " << descr << std::endl;
                    }
                    // polarity (factory setting)
                    for (i = 0; i<6; i++) {
                        error =(int)CAENComm_Read16(handle, (address_chspacing*i)+ADD_VME_CH0_POLARITY, &data);
                        if (error==0) { polarity[i] = (data*2)-1; } else { EmitError("Read Ch. polarity in Activate"); }
                    }
                    address_chspacing = 0x0080;
                    for (i=0; i<(int)chnum; i++) {
                        address_vset_ch[i] = 0x0080 + i*address_chspacing;
                        address_iset_ch[i] = 0x0084 + i*address_chspacing;
                        address_rmup_ch[i] = 0x00A4 + i*address_chspacing;
                        address_rmdn_ch[i] = 0x00A0 + i*address_chspacing;
                        address_vmax_ch[i] = 0x009C + i*address_chspacing;
                        address_ctrl_ch[i] = 0x0090 + i*address_chspacing;
                        address_stat_ch[i] = 0x0094 + i*address_chspacing;
                        address_vmon_ch[i] = 0x0088 + i*address_chspacing;
                        address_imon_ch[i] = 0x008C + i*address_chspacing;
                    }
                    units_vset = 10;
                    units_iset = 20;
                    units_rmup =  1;
                    units_rmdn =  1;
                    units_vmax = 10;
                    units_vmon = 10;
                    units_imon = 20; // 20 units = 1 V
                } break;
                case 5780: {
                    // MCA high voltage with *724 digitizer
                    formFactor = 2;
                    chnum = 2;
                    address_chspacing = 0x0100;
                    uint32_t address_offset = 0x0200; // HV channels are #2 and #3
                    for (i=0; i<(int)chnum; i++) {
                        address_vset_ch[i] = 0x1020 + i*address_chspacing + address_offset;
                        address_iset_ch[i] = 0x1024 + i*address_chspacing + address_offset;
                        address_rmup_ch[i] = 0x1028 + i*address_chspacing + address_offset;
                        address_rmdn_ch[i] = 0x102C + i*address_chspacing + address_offset;
                        address_vmax_ch[i] = 0x1030 + i*address_chspacing + address_offset;
                        address_ctrl_ch[i] = 0x1034 + i*address_chspacing + address_offset;
                        address_stat_ch[i] = 0x1038 + i*address_chspacing + address_offset;
                        address_vmon_ch[i] = 0x1040 + i*address_chspacing + address_offset;
                        address_imon_ch[i] = 0x1044 + i*address_chspacing + address_offset;
                    }
                    units_vset =  10;
                    units_iset = 100;
                    units_rmup =   1;
                    units_rmdn =   1;
                    units_vmax =   0.05;
                    units_vmon =  10;
                    units_imon = 100;
                } break;
                case 5790: {
                    // MCA high voltage with *720 digitizer
                    formFactor = 2;
                    chnum = 2;
                    address_chspacing = 0x0100;
                    uint32_t address_offset = 0x0000;
                    uint32_t serialno = ( GetRegisterSpecificBits(0xF084, 0, 7, "get serial no") | (GetRegisterSpecificBits(0xF080, 0, 7, "get serial no") << 8) );
                    if (serialno!=465) { address_offset = 0x0200; } // HV channels are #2 and #3
                    for (i=0; i<(int)chnum; i++) {
                        address_vset_ch[i] = 0x1020 + i*address_chspacing + address_offset;
                        address_iset_ch[i] = 0x1024 + i*address_chspacing + address_offset;
                        address_rmup_ch[i] = 0x1028 + i*address_chspacing + address_offset;
                        address_rmdn_ch[i] = 0x102C + i*address_chspacing + address_offset;
                        address_vmax_ch[i] = 0x1030 + i*address_chspacing + address_offset;
                        address_ctrl_ch[i] = 0x1034 + i*address_chspacing + address_offset;
                        address_stat_ch[i] = 0x1038 + i*address_chspacing + address_offset;
                        address_vmon_ch[i] = 0x1040 + i*address_chspacing + address_offset;
                        address_imon_ch[i] = 0x1044 + i*address_chspacing + address_offset;
                        SetRegisterSpecificBits(address_ctrl_ch[i], 7, 7, 0);
                    }
                    units_vset =  10;
                    units_iset =  20;
                    units_rmup =   1;
                    units_rmdn =   1;
                    units_vmax =   0.05;
                    units_vmon =  10;
                    units_imon =  20;
                } break;                
                case 5533: {
                    // Desktop Power Supply
                    formFactor = 2;
                    chnum = 4;
                    address_chspacing = 0x0100;
                    for (i=0; i<(int)chnum; i++) {
                        address_vset_ch[i] = 0x1020 + i*address_chspacing;
                        address_iset_ch[i] = 0x1024 + i*address_chspacing;
                        address_rmup_ch[i] = 0x1028 + i*address_chspacing;
                        address_rmdn_ch[i] = 0x102C + i*address_chspacing;
                        address_vmax_ch[i] = 0x1030 + i*address_chspacing;
                        address_ctrl_ch[i] = 0x1034 + i*address_chspacing;
                        address_stat_ch[i] = 0x1038 + i*address_chspacing;
                        address_vmon_ch[i] = 0x1040 + i*address_chspacing;
                        address_imon_ch[i] = 0x1044 + i*address_chspacing;
                        SetRegisterSpecificBits(address_ctrl_ch[i], 7, 7, 0);
                    }
                    units_vset =  10;
                    units_iset =  20;
                    units_rmup =   1;
                    units_rmdn =   1;
                    units_vmax =   0.05;
                    units_vmon =  10;
                    units_imon =  20;
                } break;                
            } // end switch (modelNo)
            // init status array
            for (i = 0; i<13; i++) { status_array[i] = 0; }
            //std::cout << "CARD: " << descr << " " << model << "(" << chnum << ")" << std::endl;
        } // end 'if active == 1'
    }// else { EmitError("Activate"); } // end 'if active == 0'
    EmitError("Activate");
}

// error handling
int CAENHV::GetError() { return (int)error; }

const char* CAENHV::GetErrorDesc() {
    switch (GetError()) {
        case   0: {return "Operation completed";} break;
        case  -1: {return "VME bus error during the cycle";} break;
        case  -2: {return "Communication error";} break;
        case  -3: {return "Unspecified error";} break;
        case  -4: {return "Invalid parameter";} break;
        case  -5: {return "Invalid link type";} break;
        case  -6: {return "Invalid device handle";} break;
        case  -7: {return "Communication Timeout";} break;
        case  -8: {return "Unable to open the requested Device";} break;
        case  -9: {return "Maximum number of devices exceeded";} break;
        case -10: {return "The device is already opened";} break;
        case -11: {return "Not supported function";} break;
        case -12: {return "There aren't board controlled by that CAEN Bridge";} break;
        case -13: {return "Communication terminated by the Device";} break;
        default:  {return "Unclear error";}
    }
}

void CAENHV::EmitError(const char *reference) {
    if (error!=prev_error) {
        #ifdef LOGBOOK
        logbook->AddEntry((error==0?LogBook::LOG_DEBUG:LogBook::LOG_ERROR),"HV" + std::to_string(id) + "(" + std::string(model) + "): " + std::string(GetErrorDesc()) + " [" + reference + "()]",1);
        #else
        printf("CAENHV::%s: %s\n", reference, GetErrorDesc());
        #endif
        //prev_error=error;
    }
    if (error==-2 || error==-5 || error==-6 || error==-7 || error==-8 || error==-9 || error==-13) { isActive = 0; }
    return;
}

// board information
unsigned int CAENHV::GetMajRelNum()        { return maj_rel_num; }
unsigned int CAENHV::GetMinRelNum()        { return min_rel_num; }
unsigned int CAENHV::GetVmeMajRelNum()    { return vme_maj_rel_num; }
unsigned int CAENHV::GetVmeMinRelNum()    { return vme_min_rel_num; }
unsigned int CAENHV::GetSerNum()            { return sernum; }
unsigned int CAENHV::GetChNum()            { return chnum; }
char* CAENHV::GetDescr()                    { return descr; }
char* CAENHV::GetModel()                    { return model; }
int CAENHV::GetModelNo()                    { return modelNo; }
int CAENHV::IsActive()                        { return isActive; }

// monitoring methods

int CAENHV::IsChannelOn(int channel) {
    uint16_t readout;
    error =(int)CAENComm_Read16(handle, address_stat_ch[channel], &readout);
    if (error==0) { return (readout & 1); } else { EmitError("IsChannelOn"); return -1; }
}

int* CAENHV::GetStatusArray(void) {
    uint16_t readout;
    error =(int)CAENComm_Read16(handle, ADD_VME_STATUS, &readout);
    if (error==0) {
        for (unsigned int k=0; k<12; k++)
        {
            status_array[k] = (readout & 1);
            readout >>= 1;
        }
    } else { EmitError("GetStatusArray"); }
    return status_array;
}

int* CAENHV::GetChStatusArray(int channel) {
    uint16_t readout;
    error =(int)CAENComm_Read16(handle, address_stat_ch[channel], &readout);
    if (error==0) {
        for (unsigned int k=0; k<13; k++) { status_array[k] = (readout & 1); readout >>= 1; }
    } else { EmitError("GetChStatusArray"); }
    return status_array;
}

const char* CAENHV::GetStatusDesc(int bit) {
    switch(bit) {
        case  0: { return "Channel 0 ALARM"; } break;
        case  1: { return "Channel 1 ALARM"; } break;
        case  2: { return "Channel 2 ALARM"; } break;
        case  3: { return "Channel 3 ALARM"; } break;
        case  4: { return "Channel 4 ALARM"; } break;
        case  5: { return "Channel 5 ALARM"; } break;
        case  8: { return "Board POWER FAIL"; } break;
        case  9: { return "Board OVER POWER"; } break;
        case 10: { return "Board MAXV UNCALIBRATED"; } break;
        case 11: { return "Board MAXI UNCALIBRATED"; } break;
        default: { return ""; }
    }
}

const char* CAENHV::GetChStatusDesc(int bit) {
    switch (modelNo) {
        case 6533: {
            switch(bit) {
                case  0: { return "CHANNEL ON"; } break;
                case  1: { return "RAMP UP"; } break;
                case  2: { return "RAMP DOWN"; } break;
                case  3: { return "OVER CURRENT"; } break;
                case  4: { return "OVER VOLTAGE"; } break;
                case  5: { return "UNDER VOLTAGE"; } break;
                case  6: { return "MAXV"; } break;
                case  7: { return "MAXI"; } break;
                case  8: { return "TRIP"; } break;
                case  9: { return "OVER POWER"; } break;
                case 10: { return "DISABLED"; } break;
                case 11: { return "INTERLOCK"; } break;
                case 12: { return "UNCALIBRATED"; } break;
                default: { return ""; }
            }
        } break; // end 6533
        case 5780: {
            switch(bit) {
                case  0: { return "CHANNEL ON"; } break;
                case  1: { return "RAMP UP"; } break;
                case  2: { return "RAMP DOWN"; } break;
                case  3: { return "OVER CURRENT"; } break;
                case  4: { return "OVER VOLTAGE"; } break;
                case  5: { return "UNDER VOLTAGE"; } break;
                case  6: { return "MAXV"; } break;
                case  7: { return "MAXI"; } break;
                case  8: { return "TEMP WARNING"; } break;
                case  9: { return "OVER TEMP"; } break;
                case 10: { return "DISABLED"; } break;
                case 11: { return "CALIBRATION ERROR"; } break;
                default: { return ""; }
            }
        } break; // end 5780
        case 5790: {
            switch(bit) {
                case  0: { return "CHANNEL ON"; } break;
                case  1: { return "RAMP UP"; } break;
                case  2: { return "RAMP DOWN"; } break;
                case  3: { return "OVER CURRENT"; } break;
                case  4: { return "OVER VOLTAGE"; } break;
                case  5: { return "UNDER VOLTAGE"; } break;
                case  6: { return "MAXV"; } break;
                case  7: { return "MAXI"; } break;
                case  8: { return "TEMP WARNING"; } break;
                case  9: { return "OVER TEMP"; } break;
                case 10: { return "DISABLED"; } break;
                case 11: { return "CALIBRATION ERROR"; } break;
                case 12: { return "RESETTING"; } break;
                case 13: { return "GOING OFF"; } break;
                case 14: { return "MAX POWER"; } break;
                default: { return ""; }
            }
        } break; // end 5790
        case 5533: {
            switch(bit) {
                // TO BE CONFIRMED!
                case  0: { return "CHANNEL ON"; } break;
                case  1: { return "RAMP UP"; } break;
                case  2: { return "RAMP DOWN"; } break;
                case  3: { return "OVER CURRENT"; } break;
                case  4: { return "OVER VOLTAGE"; } break;
                case  5: { return "UNDER VOLTAGE"; } break;
                case  6: { return "MAXV"; } break;
                case  7: { return "MAXI"; } break;
                case  8: { return "TEMP WARNING"; } break;
                case  9: { return "OVER TEMP"; } break;
                case 10: { return "DISABLED"; } break;
                case 11: { return "CALIBRATION ERROR"; } break;
                case 12: { return "RESETTING"; } break;
                case 13: { return "GOING OFF"; } break;
                case 14: { return "MAX POWER"; } break;
                case 15: { return "FAN SPEED HIGH"; } break;
                default: { return ""; }
            }
        } break; // end 5533
        default: { return ""; }
    } // end switch modelNo
}

float CAENHV::GetVMax(void) {
    error =(int)CAENComm_Read16(handle, ADD_VME_VMAX, &data);
    if (error!=0) { EmitError("GetVMax"); }
    return data;
}

float CAENHV::GetIMax(void) {
    error =(int)CAENComm_Read16(handle, ADD_VME_IMAX, &data);
    if (error!=0) { EmitError("GetIMax"); }
    return data;
}

float CAENHV::GetChannelSVMax(int channel) {
    error =(int)CAENComm_Read16(handle, address_vmax_ch[channel], &data);
    if (error!=0) { EmitError("GetChannelSVMax"); }
    return data/(float)units_vmax;
}

float CAENHV::GetChannelVoltage(int channel) {
    error =(int)CAENComm_Read16(handle, address_vmon_ch[channel], &data);
    if (error!=0) { EmitError("GetChannelVoltage"); }
    return data/(float)units_vmon;
}

float CAENHV::GetChannelCurrent(int channel) {
    error =(int)CAENComm_Read16(handle, address_imon_ch[channel], &data);
    if (error!=0) { EmitError("GetChannelCurrent"); }
    return data/(float)units_imon;
}

float CAENHV::GetChannelVoltageSetting(int channel) {
    error =(int)CAENComm_Read16(handle, address_vset_ch[channel], &data);
    if (error!=0) { EmitError("GetChannelVoltageSetting"); }
    return data/(float)units_vset;
}

float CAENHV::GetChannelCurrentSetting(int channel) {
    error =(int)CAENComm_Read16(handle, address_iset_ch[channel], &data);
    if (error!=0) { EmitError("GetChannelCurrentSetting"); }
    return data/(float)units_iset;
}

int CAENHV::GetChannelTemperature(int channel) {
    error =(int)CAENComm_Read16(handle, (address_chspacing*channel)+ADD_VME_CH0_TEMP, &data);
    if (error!=0) { EmitError("GetChannelTemperature"); }
    return data;
}

int CAENHV::GetChannelPolarity(int channel) {
    return polarity[channel];
}

int CAENHV::GetChannelPower(int channel) {
    error =(int)CAENComm_Read16(handle, address_ctrl_ch[channel], &data);
    if (error!=0) { EmitError("GetChannelPower"); }
    return data;
}

// operations

void CAENHV::SetChannelVoltage(int channel, int voltage) {
    error =(int)CAENComm_Write16(handle, address_vset_ch[channel], (int)(voltage*units_vset));
    if (error!=0) { EmitError("SetChannelVoltage"); }
}

void CAENHV::SetChannelCurrent(int channel, int current) {
    error =(int)CAENComm_Write16(handle, address_iset_ch[channel], (int)(current*units_iset));
    if (error!=0) { EmitError("SetChannelCurrent"); }
}

void CAENHV::SetChannelSVMax(int channel, int voltage) {
    error =(int)CAENComm_Write16(handle, address_vmax_ch[channel], (int)(voltage*units_vmax));
    if (error!=0) { EmitError("SetChannelSVMax"); }
}

void CAENHV::SetChannelOn(int channel) {
    SetRegisterSpecificBits(address_ctrl_ch[channel],0,0,1);
}

void CAENHV::SetChannelOff(int channel) {
    SetRegisterSpecificBits(address_ctrl_ch[channel],0,0,0);
}

float CAENHV::GetChannelTripTime(int channel) {
    error =(int)CAENComm_Read16(handle, (address_chspacing*channel)+ADD_VME_CH0_TRIP_TIME, &data);
    if (error!=0) { EmitError(); }
    return data*0.1f;
}

void CAENHV::SetChannelTripTime(int channel, int time) {
    error =(int)CAENComm_Write16(handle, (address_chspacing*channel)+ADD_VME_CH0_TRIP_TIME, time);
    if (error!=0) { EmitError(); }
}

float CAENHV::GetChannelRampUp(int channel) {
    error =(int)CAENComm_Read16(handle, address_rmup_ch[channel], &data);
    if (error!=0) { EmitError(); }
    return data*units_rmup;
}

void CAENHV::SetChannelRampUp(int channel, int v_over_s) {
    if (v_over_s < 0) { v_over_s = 0; }    if (v_over_s > 500) { v_over_s = 500; }
    error =(int)CAENComm_Write16(handle, address_rmup_ch[channel], v_over_s);
    if (error!=0) { EmitError(); }
}

float CAENHV::GetChannelRampDown(int channel) {
    error =(int)CAENComm_Read16(handle, address_rmdn_ch[channel], &data);
    if (error!=0) { EmitError(); }
    return data*units_rmdn;
}

void CAENHV::SetChannelRampDown(int channel, int v_over_s) {
    if (v_over_s < 0) { v_over_s = 0; }    if (v_over_s > 500) { v_over_s = 500; }
    error =(int)CAENComm_Write16(handle, address_rmdn_ch[channel], v_over_s);
    if (error!=0) { EmitError(); }
}

int CAENHV::GetChannelPowerDownMode(int channel) {
    error =(int)CAENComm_Read16(handle, (address_chspacing*channel)+ADD_VME_CH0_PWDOWN, &data);
    if (error!=0) { EmitError(); }
    return data;
}

void CAENHV::SetChannelPowerDownMode(int channel, int mode) {
    error =(int)CAENComm_Write16(handle, (address_chspacing*channel)+ADD_VME_CH0_PWDOWN, mode);
    if (error!=0) { EmitError(); }
}

// conversion units
uint32_t CAENHV::GetUnitsVoltage()            { return units_vmon; }
uint32_t CAENHV::GetUnitsCurrent()            { return units_imon; }
float        CAENHV::GetUnitsSVMax()                { return units_vmax; }
uint32_t CAENHV::GetUnitsVoltageSetting()    { return units_vset; }
uint32_t CAENHV::GetUnitsCurrentSetting()    { return units_iset; }
uint32_t CAENHV::GetUnitsRampUp()            { return units_rmup; }
uint32_t CAENHV::GetUnitsRampDown()            { return units_rmdn; }


// ----------------------
// general register tools

uint16_t CAENHV::GetRegisterSpecificBits(uint16_t reg_add, uint8_t bit_lower, uint8_t bit_upper, const char *reference) {
    uint16_t reg_data = 0;
    uint16_t reg_mask = 0;
    if (bit_upper<16 && bit_lower<=bit_upper) {
        for (int bit = 0; bit < 16; bit++) { if (bit>=bit_lower && bit<=bit_upper) { reg_mask+=(1<<bit); } }
        error = (int)CAENComm_Read16(handle, reg_add, &reg_data);
    } else { error = -3; }
    if (error!=0) { EmitError(reference); }
    return (reg_data & reg_mask) >> bit_lower;
}

void CAENHV::SetRegisterSpecificBits(uint16_t reg_add, uint8_t bit_lower, uint8_t bit_upper, uint16_t value, const char *reference) {
    int bit_number =  bit_upper - bit_lower + 1;
    if (value < (1<<bit_number) && bit_upper<16 && bit_lower<=bit_upper) {
        uint16_t reg_data = 0;
        uint16_t reg_mask = 0;
        for (int bit = 0; bit < 16; bit++) { if (bit<bit_lower || bit>bit_upper) { reg_mask+=(1<<bit); } }
        error = (int)CAENComm_Read16(handle, reg_add, &reg_data);
        if (error==0) {
            reg_data = (reg_data & reg_mask) + (value << bit_lower);
            error = (int)CAENComm_Write16(handle, reg_add, reg_data);
        }
    } else { error = -3; }
    if (error!=0) { EmitError(reference); }
}
