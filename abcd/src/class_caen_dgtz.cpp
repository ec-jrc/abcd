// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

// file includes
#include <algorithm>
#include <cstring>
#include <string>

#include <json/json.h>

#include "class_caen_dgtz.h"

// registers definition
#define ADD_1ST_RELE 0x1020
#define ADD_1ST_ZSTH 0x1024
#define ADD_1ST_ZSNS 0x1028
#define ADD_1ST_3VPP 0x1028
#define ADD_1ST_ANEV 0x1034
#define ADD_1ST_PTRG 0x1038
#define ADD_1ST_SGTE 0x1054
#define ADD_1ST_LGTE 0x1058
#define ADD_1ST_PGTE 0x105C
#define ADD_1ST_DTHR 0x1060
#define ADD_1ST_BLTH 0x1064
#define ADD_1ST_BLTO 0x1068
#define ADD_STD_CTRL 0x1050 // same register, different address
#define ADD_DPP_CTRL 0x1080 // in standard and DPP-PSD versions
// 0x1n6C latency for coincidence
#define ADD_1ST_CWIN 0x1070 // coincidence window
#define ADD_1ST_HOLD 0x1074 // trigger holdoff
#define ADD_1ST_PSDC 0x1078
#define ADD_1ST_PGAP 0x107C // pile-up rejection gap setting
#define ADD_1ST_TRNS 0x1084
#define ADD_DPP_CTR2 0x1084
#define ADD_1ST_CHST 0x1088
#define ADD_1ST_FWRV 0x108C
#define ADD_1ST_CHBO 0x1094
#define ADD_CHA_CNFG 0x8000
#define ADD_DPP_CNFG 0x8000
#define ADD_NUM_BUFF 0x800C
#define ADD_ADC_CONF 0x809C
#define ADD_ACQ_CTRL 0x8100
#define ADD_ACQ_STAT 0x8104 // acquisition status
#define ADD_TRG_MASK 0x810C
// 0x8110 trigger out mask
#define ADD_TRG_PTNS 0x8114
#define ADD_FIO_CTRL 0x811C
// 0x8120 channel enable mask
#define ADD_EVT_STRD 0x812C
// 0x814c event size
#define ADD_RUN_DLAY 0x8170
// 0x8180 mask base board coincidence
#define ADD_VME_CTRL 0xEF00
#define ADD_VME_STAT 0xEF04
#define ADD_INT_STID 0xEF14
#define ADD_INT_EVNO 0xEF18

namespace errors {
const int CONFIG_FILE_NOT_FOUND = -60;
}

// --------------------------------------------------------------------------------
// constructor & destructor
// --------------------------------------------------------------------------------

#ifdef LOGBOOK
CAENDgtz::CAENDgtz(LogBook* _logbook)
    : logbook(_logbook)
    , handle(-1)
    , isActive(0)
    , error(0)
{
#else
CAENDgtz::CAENDgtz()
    : handle(-1)
    , isActive(0)
    , error(0)
{
#endif
    boardName.assign("DGTZ");
    for (int ch = 0; ch < MAXC_DG; ch++) {
        specThr[ch] = 0;
        channelName[ch].assign("ch" + std::to_string(ch));
    }
    isMaster = 1;
    showGates = 0;
    writeBuffer = 0;
    verboseDebug = 1;
    ext_errcode = NULL;
    ext_errdesc = NULL;

    memset(reinterpret_cast<void*>(&boardInfo), 0, sizeof(boardInfo));
}

CAENDgtz::~CAENDgtz()
{
    if (IsActive(1) == 1) {
        Deactivate();
    }
}

// -----------------------------------------------------------------------------
// general methods
// -----------------------------------------------------------------------------
//       1         2         3         4         5         6         7         8
//345678901234567890123456789012345678901234567890123456789012345678901234567890

void CAENDgtz::Activate(int connection_type,
    int link_number,
    int connection_node,
    uint32_t address)
{
    error = 0;

    if (isActive == 0) {
        //std::cout << "ACT: ";
        board_base_address = address;
        CAEN_DGTZ_ConnectionType conntype;

        switch (connection_type) {
        case 0:
            conntype = CAEN_DGTZ_USB;
            break;

        // The followings are deprecated, they were all substituted by
        // CAEN_DGTZ_OpticalLink
        //                                              -- Cristiano Fontana

        //case 1:  {conntype = CAEN_DGTZ_PCI_OpticalLink;} break;
        //case 2:  {conntype = CAEN_DGTZ_PCIE_OpticalLink;} break;
        //case 3:  {conntype = CAEN_DGTZ_PCIE_EmbeddedDigitizer;} break;
        default:
            conntype = CAEN_DGTZ_OpticalLink;
        }

        //       1         2         3         4         5         6         7         8
        //345678901234567890123456789012345678901234567890123456789012345678901234567890
        error = (int)CAEN_DGTZ_OpenDigitizer(conntype,
            link_number,
            connection_node,
            board_base_address,
            &handle);
        if (error == 0) {
            if (boardName.compare("DGTZ") == 0) {
                boardName.append(" #" + std::to_string(handle));
            }

            // board info
            boardInfoFromLib = GetInfo();

            for (i = 0; i < 12; i++) {
                boardInfo.ModelName[i] = boardInfoFromLib.ModelName[i];
            }
            boardInfo.Model = boardInfoFromLib.Model;
            boardInfo.Channels = boardInfoFromLib.Channels;
            boardInfo.FormFactor = boardInfoFromLib.FormFactor;
            boardInfo.FamilyCode = boardInfoFromLib.FamilyCode;
            for (i = 0; i < 20; i++) {
                boardInfo.ROC_FirmwareRel[i] = boardInfoFromLib.ROC_FirmwareRel[i];
            }
            for (i = 0; i < 40; i++) {
                boardInfo.AMC_FirmwareRel[i] = boardInfoFromLib.AMC_FirmwareRel[i];
            }
            boardInfo.SerialNumber = boardInfoFromLib.SerialNumber;
            boardInfo.PCB_Revision = boardInfoFromLib.PCB_Revision;
            boardInfo.ADC_NBits = boardInfoFromLib.ADC_NBits;

            if (verboseDebug) {
                //       1         2         3         4         5         6         7         8
                //345678901234567890123456789012345678901234567890123456789012345678901234567890
                std::cout << "ModelName:\t" << boardInfo.ModelName << std::endl;
                std::cout << "Model:\t" << boardInfo.Model << std::endl;
                std::cout << "Channels:\t" << boardInfo.Channels << std::endl;
                std::cout << "FormFactor:\t" << boardInfo.FormFactor << std::endl;
                std::cout << "FamilyCode:\t" << boardInfo.FamilyCode << std::endl;
                std::cout << "ROC FW rel:\t" << boardInfo.ROC_FirmwareRel << std::endl;
                std::cout << "AMC FW rel:\t" << boardInfo.AMC_FirmwareRel << std::endl;
                std::cout << "Serial no.:\t" << boardInfo.SerialNumber << std::endl;
                std::cout << "ADC_NBits:\t" << boardInfo.ADC_NBits << std::endl;

                const float mins_left = ReadRegister(0x8158, "Init - Read 0x8158") * 84.0 / 60000;
                std::cout << "Mins left:\t" << mins_left << std::endl;
            }

            // model number
            uint32_t vers = ReadRegister(0xF030, "Init - Read 0xF030");
            std::cout << "DEBUG: CAENDgtz::Activate(): ";
            std::cout << "vers: " << vers;
            std::cout << std::endl;

            boardInfo.vers = 0;
            if (vers >= 0x30 && vers <= 0x3a) {
                boardInfo.vers = 720;
                boardInfo.nsPerSample = 4;
                boardInfo.nsPerTimetag = 8;
            }
	    else if (vers >= 0x60 && vers <= 0x62) {
                boardInfo.vers = 751;
                boardInfo.nsPerSample = 1;
                boardInfo.nsPerTimetag = 8;
            }
	    else if (vers >= 0x90 && vers <= 0x90) {
                boardInfo.vers = 724;
                boardInfo.nsPerSample = 10;
                boardInfo.nsPerTimetag = 8;
            }
	    else if (vers >= 0xc0 && vers <= 0xc1) {
                boardInfo.vers = 730;
                boardInfo.nsPerSample = 2;
                boardInfo.nsPerTimetag = 8;
                WriteRegister(0x809C, 1);
            }
            // DT5790
	    else if (vers >= 0xd0 && vers <= 0xd2) {
                boardInfo.vers = 720;
                boardInfo.nsPerSample = 4;
                boardInfo.nsPerTimetag = 8;
            }
            // DT5725
	    else if (vers >= 0xf0 && vers <= 0xf1) {
                boardInfo.vers = 725;
                boardInfo.nsPerSample = 4;
                boardInfo.nsPerTimetag = 8;
            }
            // DT5730S with new FPGA
	    else if (vers >= 0xc4 && vers <= 0xc5) {
                boardInfo.vers = 730;
                boardInfo.nsPerSample = 2;
                boardInfo.nsPerTimetag = 8;
                WriteRegister(0x809C, 1);
            }

            if (verboseDebug) {
                std::cout << "vers: 0x" << std::hex << vers << std::dec << ";";
                std::cout << " ";
                std::cout << "ns: " << 0 + boardInfo.nsPerSample << ";";
                std::cout << std::endl;
            }

            // firmware type
            uint32_t firmware;
            if (boardInfo.Channels > 2) {
                firmware = ReadRegister(ADD_1ST_FWRV + 0x200, "Init - Read FW");
            } else {
                firmware = ReadRegister(ADD_1ST_FWRV, "Init - Read FW");
            }

            if (verboseDebug) {
                std::cout << "FW: 0x" << std::hex << ((firmware & 0xFF00) >> 8) << std::dec << std::endl;
            }

            switch ((firmware & 0xFF00) >> 8) {
	      std::cout << "   setting FW " << ((firmware & 0xFF00) >> 8) << std::endl;
            case 136:
                // 0x88 DPP_PSD x730
                boardInfo.dppVersion = 3;
                ConfigSetPSDMandatoryBits();
                break;
            case 132:
                // 0x84 DPP_PSD x751
                boardInfo.dppVersion = 3;
                ConfigSetPSDMandatoryBits();
                break;
            case 131:
                // 0x83 DPP_PSD x720
                boardInfo.dppVersion = 3;
                ConfigSetPSDMandatoryBits();
                break;
            case 130:
                // 0x82 DPP_CI
                boardInfo.dppVersion = 2;
                break;
            case 128:
                // 0x80 DPP_PHA
                boardInfo.dppVersion = 1;
                break;
            default:
                // standard firmware revision
                boardInfo.dppVersion = 0;
            }

            if (boardInfo.dppVersion > 0) {
                boardInfo.nsPerTimetag = boardInfo.nsPerSample;
            }

            // reset = initialization
            Reset();

            // formal activation
            isActive = 1;
        }
    }

    EmitError("Activate"); // can also show "Operation successfully completed";
}

void CAENDgtz::Deactivate()
{
    isActive = 0;
    error = (int)CAEN_DGTZ_CloseDigitizer(handle);
    if (error != 0) {
        EmitError("Deactivate");
    }
    //std::cout << "DEAC " << isActive << std::endl;
}

void CAENDgtz::SetVerboseDebug(int mode)
{
    verboseDebug = (mode & 1);
}

int CAENDgtz::IsActive(int silent)
{
    // check if communication is possibile
    if (isActive == 1 && silent == 0) {
        ReadRegister(0xF000, "IsActive()");
    } // will EmitError in case of errors;
    return isActive; // EmitError will set isActive=0 in case of COM errors
}

int CAENDgtz::IsMaster()
{
    return isMaster;
}

int CAENDgtz::GetShowGates()
{
    return showGates;
}

int CAENDgtz::GetWriteBuffer()
{
    return writeBuffer;
}

uint8_t CAENDgtz::GetDPPVersion()
{
    return boardInfo.dppVersion;
}

unsigned int CAENDgtz::GetNumChannels()
{
    return boardInfo.Channels;
}

uint8_t CAENDgtz::GetNsPerSample()
{
    return boardInfo.nsPerSample;
}

uint8_t CAENDgtz::GetNsPerTimetag()
{
    return boardInfo.nsPerTimetag;
}

void CAENDgtz::Reset()
{
    error = (int)CAEN_DGTZ_Reset(handle);

    if (boardInfo.dppVersion == 3) {
        ConfigSetPSDMandatoryBits();
    }
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        WriteRegister(0x8004, (1 << 2)); // GPIO setting
    }
    // channel calibration
    if (boardInfo.vers == 730) {
        //        CAEN_DGTZ_Calibrate(handle);
        WriteRegister(0x809c, 1, "Initial channel calibration");
        uint32_t timeout_ms = 0;
        uint32_t flag_cal_ok = 0;
        while (timeout_ms < 1000 && flag_cal_ok == 0) {
            Sleep(100);
            timeout_ms += 100;
            flag_cal_ok = GetRegisterSpecificBits(0x1088, 3, 3, "Initial channel calibration");
        }
        if (verboseDebug) {
            if (flag_cal_ok == 1) {
                std::cout << "Channels calibrated." << std::endl;
            } else {
                std::cout << "Channels did not calibrate before 1 s." << std::endl;
            }
        }
    }

   // channel calibration
    if (boardInfo.vers == 725) {
      error = (int)CAEN_DGTZ_Calibrate(handle);
      if (verboseDebug) printf("---> Reset: calling CAEN_DGTZ_Calibrate(handle)   :  %d\n",error);
    }


    if (error != 0) {
        EmitError("Reset");
    }
}

void CAENDgtz::SetBoardName(const char* name)
{
    boardName.assign(name);
}

const char* CAENDgtz::GetBoardName()
{
    return boardName.data();
}

void CAENDgtz::SetChannelName(int channel, const char* name)
{
    channelName[channel].assign(name);
}

const char* CAENDgtz::GetChannelName(int channel)
{
    return channelName[channel].data();
}

int CAENDgtz::GetModel()
{
    return boardInfo.vers;
}

const char* CAENDgtz::GetModelName()
{
    return boardInfoFromLib.ModelName;
}

int CAENDgtz::GetError() { return error; }

const char* CAENDgtz::GetErrorDesc()
{
    switch (error) {
    case 0: {
        return "Operation completed";
    } break;
    case -1: {
        return "Communication error";
    } break;
    case -2: {
        return "Unspecified error";
    } break;
    case -3: {
        return "Invalid parameter";
    } break;
    case -4: {
        return "Invalid link type";
    } break;
    case -5: {
        return "Invalid device handle";
    } break;
    case -6: {
        return "Maximum number of devices exceeded";
    } break;
    case -7: {
        return "The operation is not allowed on this type of board";
    } break;
    case -8: {
        return "The interrupt level is not allowed";
    } break;
    case -9: {
        return "The event number is bad";
    } break;
    case -10: {
        return "Unable to read the registry";
    } break;
    case -11: {
        return "Unable to write into the registry";
    } break;
    case -12: {
        return "This function is incompatible with current settings";
    } break;
    case -13: {
        return "The channel number is invalid";
    } break;
    case -14: {
        return "The channel is busy";
    } break;
    case -15: {
        return "Invalid FPIO Mode";
    } break;
    case -16: {
        return "Wrong acquisition mode";
    } break;
    case -17: {
        return "This function is not allowed for this module";
    } break;
    case -18: {
        return "Communication timeout";
    } break;
    case -19: {
        return "The buffer is invalid";
    } break;
    case -20: {
        return "The event is not found";
    } break;
    case -21: {
        return "The event is invalid";
    } break;
    case -22: {
        return "Out of memory";
    } break;
    case -23: {
        return "Unable to calibrate the board";
    } break;
    case -24: {
        return "Unable to open the digitizer";
    } break;
    case -25: {
        return "The digitizer is already open";
    } break;
    case -26: {
        return "The digitizer is not ready to operate";
    } break;
    case -27: {
        return "The digitizer has not the IRQ configured";
    } break;
    case errors::CONFIG_FILE_NOT_FOUND: {
        return "Configuration file not found";
    } break;
    case -99: {
        return "The function is not yet implemented";
    } break;
    default: {
        return "Unclear error";
    }
    }
}

void CAENDgtz::EmitError(const char* reference)
{
#ifdef LOGBOOK
    logbook->AddEntry((error == 0 ? LogBook::LOG_INFO : LogBook::LOG_ERROR), boardName + ": " + GetErrorDesc() + " [" + reference + "()]", verboseDebug);
#else
    if (verboseDebug) {
        printf("%s: %s [%s]\n", boardName.data(), GetErrorDesc(), reference);
    }
#endif
    if (ext_errcode != NULL) {
        (*ext_errcode) = error;
    }
    if (ext_errdesc != NULL) {
        (*ext_errdesc) = GetErrorDesc();
        ext_errdesc->append(" [");
        ext_errdesc->append(reference);
        ext_errdesc->append("()]");
    }
    if (error == -1 || error == -5 || error == -18 || error == -26) {
        isActive = 0;
    }
    return;
}

void CAENDgtz::SetExternalErrorCode(int* ext_errc)
{
    ext_errcode = ext_errc;
    return;
}

void CAENDgtz::SetExternalErrorDesc(std::string* ext_errd)
{
    ext_errdesc = ext_errd;
    return;
}

int CAENDgtz::GetHandle()
{
    return handle;
}

CAEN_DGTZ_BoardInfo_t CAENDgtz::GetInfo()
{
    CAEN_DGTZ_BoardInfo_t this_boardInfo;
    memset(reinterpret_cast<void*>(&this_boardInfo), 0, sizeof(CAEN_DGTZ_BoardInfo_t));

    error = (int)CAEN_DGTZ_GetInfo(handle, &this_boardInfo);
    if (error != 0) {
        EmitError("GetInfo");
    }

    return this_boardInfo;
}

void CAENDgtz::WriteRegister(uint32_t address, uint32_t data, const char* reference)
{
    error = (int)CAEN_DGTZ_WriteRegister(handle, address, data);
    if (error != 0) {
        EmitError(reference);
    }
}

uint32_t CAENDgtz::ReadRegister(uint32_t address, const char* reference)
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);

    std::stringstream errorReference;
    errorReference << "ReadRegister(0x";
    errorReference << std::hex << std::setfill('0') << std::setw(4) << address;
    errorReference << ") called by " << reference << std::dec;

    if (error != 0) {
        EmitError(errorReference.str().c_str());
    }

    return data;
}

const char* CAENDgtz::DumpRegister(uint32_t address, int print)
{
    std::stringstream ss;

    uint32_t data = ReadRegister(address, "DumpRegister()");

    ss << "[0x";
    ss << std::hex << std::setw(4) << address << std::dec;
    ss << "] = 0x" << std::hex << std::setfill('0') << std::setw(8) << data << std::dec << " = ";
    for (int bit = 31; bit >= 0; bit--) {
        ss << ((data & (1 << bit)) >> bit);
        if (bit % 4 == 0) {
            ss << " ";
        }
        if (bit % 8 == 0) {
            ss << " ";
        }
    }
    ss << "= " << data;

    if (print == 1) {
        std::cout << ss.str().data() << std::endl;
    }

    return ss.str().data();
}

uint32_t CAENDgtz::GetRegisterSpecificBits(uint32_t reg_add,
    uint8_t bit_lower,
    uint8_t bit_upper,
    const char* reference)
{
    uint32_t reg_data = 0;
    uint32_t reg_mask = 0;
    if (bit_upper < 32 && bit_lower <= bit_upper) {
        for (int bit = 0; bit < 32; bit++) {
            if (bit >= bit_lower && bit <= bit_upper) {
                reg_mask += (1 << bit);
            }
        }
        error = (int)CAEN_DGTZ_ReadRegister(handle, reg_add, &reg_data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError(reference);
    }
    return (reg_data & reg_mask) >> bit_lower;
}

void CAENDgtz::SetRegisterSpecificBits(uint32_t reg_add, uint8_t bit_lower, uint8_t bit_upper, uint32_t value, const char* reference)
{
    uint8_t bit_number = bit_upper - bit_lower + 1;
    if ((value < ((uint32_t)1 << bit_number)) && bit_upper < 32 && bit_lower <= bit_upper) {
        uint32_t reg_data = 0;
        uint32_t reg_mask = 0;
        for (int bit = 0; bit < 32; bit++) {
            if (bit < bit_lower || bit > bit_upper) {
                reg_mask += (1 << bit);
            }
        }
        error = (int)CAEN_DGTZ_ReadRegister(handle, reg_add, &reg_data);
        if (error == 0) {
            reg_data = (reg_data & reg_mask) + (value << bit_lower);
            error = (int)CAEN_DGTZ_WriteRegister(handle, reg_add, reg_data);
        }
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError(reference);
    }
}

void CAENDgtz::SetFanSpeed(int value)
{
    if (boardInfo.vers == 730 || (boardInfo.vers == 724)) {
        SetRegisterSpecificBits(0x8168, 3, 3, value);
    } else {
        error = -17;
    }
    if (error != 0) {
        EmitError("SetFanSpeed");
    }
}

int CAENDgtz::GetFanSpeed()
{
    switch (boardInfo.vers) {
    case 725: {
        return GetRegisterSpecificBits(0x8168, 3, 3);
    } break;
    case 730: {
        return GetRegisterSpecificBits(0x8168, 3, 3);
    } break;
    case 724: {
        return GetRegisterSpecificBits(0x8168, 3, 3);
    } break;
    default: {
        error = -17;
    }
    }
    if (error != 0) {
        EmitError("GetFanSpeed");
    }
    return 1;
}

// --------------------------------------------------------------------------------
// operations
// --------------------------------------------------------------------------------

void CAENDgtz::SetChannelEnableMask(uint32_t mask)
{
    // disable even channels - AUTOMATICALLY DONE
    //if (boardInfo.vers==751 && boardInfo.dppVersion==0) {
    //    if (GetDualEdgeMode()==1) {
    //        error = (int)CAEN_DGTZ_SetChannelEnableMask(handle,(mask & 0x5555));
    //        if (error!=0) { EmitError("SetChannelEnableMask/DualEdgeMode"); }
    //        return;
    //    }
    //}
    error = (int)CAEN_DGTZ_SetChannelEnableMask(handle, mask);
    if (error != 0) {
        EmitError("SetChannelEnableMask");
    }
    return;
}

uint32_t CAENDgtz::GetChannelEnableMask()
{
    uint32_t mask;
    error = (int)CAEN_DGTZ_GetChannelEnableMask(handle, &mask);
    if (error != 0) {
        EmitError("GetChannelEnableMask");
    }
    return mask;
}

void CAENDgtz::SetCoincidenceMask(uint32_t mask)
{
    SetRegisterSpecificBits(ADD_TRG_MASK, 0, 7, mask, "SetCoincidenceMask");
}

uint32_t CAENDgtz::GetCoincidenceMask()
{
    return GetRegisterSpecificBits(ADD_TRG_MASK, 0, 7, "GetCoincidenceMask");
}

void CAENDgtz::SetCoincidenceLevel(uint8_t level)
{
    SetRegisterSpecificBits(ADD_TRG_MASK, 24, 26, level, "GetCoincidenceLevel");
}

uint32_t CAENDgtz::GetCoincidenceLevel()
{
    return GetRegisterSpecificBits(ADD_TRG_MASK, 24, 26, "GetCoincidenceLevel");
}

void CAENDgtz::SetSWTriggerMode(uint32_t mode)
{
    CAEN_DGTZ_TriggerMode_t caen_enum_mode;
    switch (mode) {
    case 1: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
    } break;
    case 2: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_EXTOUT_ONLY;
    } break;
    case 3: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
    } break;
    default: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_DISABLED;
    }
    }
    error = (int)CAEN_DGTZ_SetSWTriggerMode(handle, caen_enum_mode);
    if (error != 0) {
        EmitError("SetSWTriggerMode");
    }
}

uint32_t CAENDgtz::GetSWTriggerMode()
{
    CAEN_DGTZ_TriggerMode_t mode;
    error = (int)CAEN_DGTZ_GetSWTriggerMode(handle, &mode);
    if (error != 0) {
        EmitError("GetSWTriggerMode");
    }
    return (int)mode;
}

void CAENDgtz::SetAcquisitionMode(uint32_t mode)
{
    CAEN_DGTZ_AcqMode_t caen_enum_acqmode;
    switch (mode) {
    case 1: {
        caen_enum_acqmode = CAEN_DGTZ_S_IN_CONTROLLED;
    } break;
    default: {
        caen_enum_acqmode = CAEN_DGTZ_SW_CONTROLLED;
    }
    }
    error = (int)CAEN_DGTZ_SetAcquisitionMode(handle, caen_enum_acqmode);
    if (error != 0) {
        EmitError("SetAcquisitionMode");
    }
}

uint32_t CAENDgtz::GetAcquisitionMode()
{
    CAEN_DGTZ_AcqMode_t caen_enum_acqmode;
    error = (int)CAEN_DGTZ_GetAcquisitionMode(handle, &caen_enum_acqmode);
    if (error != 0) {
        EmitError("GetAcquisitionMode");
    }
    return (int)caen_enum_acqmode;
}

void CAENDgtz::SetExtTriggerInputMode(uint32_t mode)
{
    CAEN_DGTZ_TriggerMode_t caen_enum_mode;
    switch (mode) {
    case 1: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
    } break;
    case 2: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_EXTOUT_ONLY;
    } break;
    case 3: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
    } break;
    default: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_DISABLED;
    }
    }
    error = (int)CAEN_DGTZ_SetExtTriggerInputMode(handle, caen_enum_mode);
    if (error != 0) {
        EmitError("SetExtTriggerInputMode");
    }
}

uint32_t CAENDgtz::GetExtTriggerInputMode()
{
    CAEN_DGTZ_TriggerMode_t caen_enum_mode;
    error = (int)CAEN_DGTZ_GetExtTriggerInputMode(handle, &caen_enum_mode);
    if (error != 0) {
        EmitError("GetExtTriggerInputMode");
    }
    return (int)caen_enum_mode;
}

void CAENDgtz::SetEventPackaging(uint32_t value)
{
    if (value < 2) {
        WriteRegister(ADD_CHA_CNFG + 0x0004 * (2 - value), 1 << 11);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError("SetEventPackaging");
    }
}

uint32_t CAENDgtz::GetEventPackaging()
{
    uint32_t packaging = 0;
    if (boardInfo.vers == 720) {
        packaging = GetRegisterSpecificBits(ADD_CHA_CNFG, 11, 11, "GetEventPackaging");
    }
    return packaging;
}

void CAENDgtz::SetTriggerUnderThreshold(uint32_t value)
{
    if (value < 2) {
        WriteRegister(ADD_CHA_CNFG + 0x0004 * (2 - value), 1 << 6);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError("SetTriggerUnderThreshold");
    }
}

uint32_t CAENDgtz::GetTriggerUnderThreshold()
{
    return GetRegisterSpecificBits(ADD_CHA_CNFG, 6, 6, "GetTriggerUnderThreshold");
}

void CAENDgtz::SetMemoryAccess(uint32_t value)
{
    if (value < 2) {
        WriteRegister(ADD_CHA_CNFG + 0x0004 * (2 - value), 1 << 4);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError("SetMemoryAccess");
    }
}

uint32_t CAENDgtz::GetMemoryAccess()
{
    return (int)GetRegisterSpecificBits(ADD_CHA_CNFG, 4, 4, "GetMemoryAccess");
}

void CAENDgtz::SetTestPatternGeneration(uint32_t value)
{
    if (value < 2) {
        WriteRegister(ADD_CHA_CNFG + 0x0004 * (2 - value), 1 << 3);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetTestPatternGeneration()
{
    return (int)GetRegisterSpecificBits(ADD_CHA_CNFG, 3, 3, "GetTestPatternGeneration");
}

void CAENDgtz::SetTriggerOverlap(uint32_t value)
{
    if (value < 2) {
        WriteRegister(ADD_CHA_CNFG + 0x0004 * (2 - value), 1 << 1);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetTriggerOverlap()
{
    return (int)GetRegisterSpecificBits(ADD_CHA_CNFG, 1, 1, "GetTriggerOverlap");
}

void CAENDgtz::SetDPPEventAggregation(uint32_t value1, uint32_t value2)
{
    error = (int)CAEN_DGTZ_SetDPPEventAggregation(handle, value1, value2);
    if (error != 0) {
        EmitError("SetDPPEventAggregation");
    }
}

void CAENDgtz::SetBufferBlockNumber(uint32_t value)
{
    if (value < 11) {
        error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_NUM_BUFF, value);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError("SetBufferBlockNumber");
    }
}

uint32_t CAENDgtz::GetBufferBlockNumber()
{
    return ReadRegister(ADD_NUM_BUFF, "GetBufferBlockNumber()");
}

// --------------------------------------------------------------------------------
// front panel I/O control
// --------------------------------------------------------------------------------

void CAENDgtz::SetIOTrgOutProgram(uint32_t value)
{
    if (value < 4) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFF3FFFF) + (value << 18);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOTrgOutProgram()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x000C0000) >> 18;
}

void CAENDgtz::SetIOTrgOutDisplay(uint32_t value)
{
    if (value < 4) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFCFFFF) + (value << 16);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOTrgOutDisplay()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00030000) >> 16;
}

void CAENDgtz::SetIOTrgOutMode(uint32_t value)
{
    if (value < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFF7FFF) + (value << 15);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOTrgOutMode()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00008000) >> 15;
}

void CAENDgtz::SetIOTrgOutTestMode(uint32_t value)
{
    if (value < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFFBFFF) + (value << 14);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOTrgOutTestMode()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00004000) >> 14;
}

void CAENDgtz::SetIOExtTrgPropagation(uint32_t value)
{
    SetRegisterSpecificBits(ADD_FIO_CTRL, 11, 11, value);
}

int CAENDgtz::GetIOExtTrgPropagation()
{
    return GetRegisterSpecificBits(ADD_FIO_CTRL, 11, 11);
}

void CAENDgtz::SetIOPatternLatchMode(uint32_t value)
{
    if (value < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFFFEFF) + (value << 9);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOPatternLatchMode()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00000100) >> 9;
}

void CAENDgtz::SetIOMode(uint32_t value)
{
    if (value < 3) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFFFF3F) + (value << 6);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOMode()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x000000C0) >> 6;
}

void CAENDgtz::SetIOLVDSFourthBlkOutputs(uint32_t value)
{
    if (value < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFFFFDF) + (value << 5);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOLVDSFourthBlkOutputs()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00000020) >> 5;
}

void CAENDgtz::SetIOLVDSThirdBlkOutputs(uint32_t value)
{
    if (value < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFFFFEF) + (value << 4);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOLVDSThirdBlkOutputs()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00000010) >> 4;
}

void CAENDgtz::SetIOLVDSSecondBlkOutputs(uint32_t value)
{
    if (value < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFFFFF7) + (value << 3);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOLVDSSecondBlkOutputs()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00000008) >> 3;
}

void CAENDgtz::SetIOLVDSFirstBlkOutputs(uint32_t value)
{
    if (value < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFFFFFB) + (value << 2);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOLVDSFirstBlkOutputs()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00000004) >> 2;
}

void CAENDgtz::SetIOHighImpedence(uint32_t value)
{
    if (value < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFFFFFD) + (value << 1);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOHighImpedence()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00000002) >> 1;
}

void CAENDgtz::SetIOLevel(uint32_t value)
{
    if (value < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
        data = (data & 0xFFFFFFFE) + value;
        error = CAEN_DGTZ_WriteRegister(handle, ADD_FIO_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetIOLevel()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_FIO_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return data & 0x00000001;
}

// --------------------------------------------------------------------------------
// VME control
// --------------------------------------------------------------------------------

void CAENDgtz::SetInterruptMode(uint32_t mode)
{
    if (mode < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
        data = (data & 0xBF) + (mode << 7);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_VME_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetInterruptMode()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data >> 7) & 0x1;
}

void CAENDgtz::SetEnabledRELOC(uint32_t mode)
{
    if (mode < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
        data = (data & 0xCF) + (mode << 6);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_VME_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetEnabledRELOC()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data >> 6) & 0x1;
}

void CAENDgtz::SetEnabledALIGN64(uint32_t mode)
{
    if (mode < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
        data = (data & 0xDF) + (mode << 5);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_VME_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetEnabledALIGN64()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data >> 5) & 0x1;
}

void CAENDgtz::SetEnabledBERR(uint32_t mode)
{
    if (mode < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
        data = (data & 0xEF) + (mode << 4);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_VME_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetEnabledBERR()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data >> 4) & 0x1;
}

void CAENDgtz::SetEnabledOLIRQ(uint32_t mode)
{
    if (mode < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
        data = (data & 0xF7) + (mode << 3);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_VME_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetEnabledOLIRQ()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return (data >> 3) & 0x1;
}

void CAENDgtz::SetInterruptLevel(uint32_t level)
{
    if (level < 8) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
        data = (data & 0xF8) + level;
        error = CAEN_DGTZ_WriteRegister(handle, ADD_VME_CTRL, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetInterruptLevel()
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, ADD_VME_CTRL, &data);
    if (error != 0) {
        EmitError();
    }
    return data & 0x7;
}

void CAENDgtz::SetInterruptStatusID(uint32_t value)
{
    error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_INT_STID, value);
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetInterruptStatusID()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_INT_STID, &data);
    if (error != 0) {
        EmitError();
    }
    return data;
}

void CAENDgtz::SetInterruptEventNumber(uint32_t value)
{
    if (value < 1024) {
        error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_INT_EVNO, value);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetInterruptEventNumber()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_INT_EVNO, &data);
    if (error != 0) {
        EmitError();
    }
    return data;
}

void CAENDgtz::IRQWait(uint32_t timeout)
{
    error = (int)CAEN_DGTZ_IRQWait(handle, timeout);
    if (error != 0) {
        EmitError();
    }
}

// --------------------------------------------------------------------------------
// monitoring
// --------------------------------------------------------------------------------

uint32_t CAENDgtz::GetEventStoredNumber()
{
    return ReadRegister(ADD_EVT_STRD, "GetEventStoredNumber()");
}

CAENDgtz::acqStatus_t CAENDgtz::GetAcquisitionStatus()
{
    acqStatus_t acqStatus;
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_ACQ_STAT, &data);
    if (error == 0) {
        acqStatus.ready = (data >> 8) & 1;
        acqStatus.pllLock = (data >> 7) & 1;
        acqStatus.pllBypass = (data >> 6) & 1;
        acqStatus.clockSource = (data >> 5) & 1;
        acqStatus.eventFull = (data >> 4) & 1;
        acqStatus.eventReady = (data >> 3) & 1;
        acqStatus.run = (data >> 2) & 1;
    } else {
        EmitError("GetAcquisitionStatus");
    }
    return acqStatus;
}

CAENDgtz::vmeStatus_t CAENDgtz::GetVMEStatus()
{
    vmeStatus_t vmeStatus;
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_VME_STAT, &data);
    if (error == 0) {
        vmeStatus.busErr = (data >> 2) & 1;
        vmeStatus.busFull = (data >> 1) & 1;
        vmeStatus.eventReady = data & 1;
    } else {
        EmitError("GetVMEStatus");
    }
    return vmeStatus;
}

CAENDgtz::chnStatus_t CAENDgtz::GetChannelStatus(uint32_t channel)
{
    chnStatus_t chnStatus;
    uint32_t data = 0;
    uint32_t address = ADD_1ST_CHST + 0x0100 * channel;
    error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
    if (error == 0) {
        chnStatus.bufErr = (data >> 5) & 1;
        chnStatus.dacBusy = (data >> 2) & 1;
        chnStatus.memEmpty = (data >> 1) & 1;
        chnStatus.memFull = data & 1;
    } else {
        EmitError("GetChannelStatus");
    }
    return chnStatus;
}
uint32_t CAENDgtz::GetChannelBufferOccupancy(uint32_t channel)
{
    uint32_t data = 0;
    uint32_t address = ADD_1ST_CHBO + 0x0100 * channel;
    error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
    if (error != 0) {
        EmitError("GetChannelBufferOccupancy");
    }
    return (data & 0x400);
}

// --------------------------------------------------------------------------------
// acquisition
// --------------------------------------------------------------------------------

void CAENDgtz::ClearData()
{
    error = CAEN_DGTZ_ClearData(handle);
    if (error != 0) {
        EmitError("ClearData");
    }
}

void CAENDgtz::SWStartAcquisition()
{
    error = (int)CAEN_DGTZ_SWStartAcquisition(handle);
    if (error != 0) {
        EmitError("SWStartAcquisition");
    }
}

void CAENDgtz::SWStopAcquisition()
{
    error = (int)CAEN_DGTZ_SWStopAcquisition(handle);
    if (error != 0) {
        EmitError("SWStopAcquisition");
    }
}

void CAENDgtz::SendSWTrigger()
{
    error = (int)CAEN_DGTZ_SendSWtrigger(handle);
    if (error != 0) {
        EmitError("SendSWTrigger");
    }
}

void CAENDgtz::ReadData(CAEN_DGTZ_ReadMode_t mode, char* buffer, uint32_t* bufferSize)
{
    error = (int)CAEN_DGTZ_ReadData(handle, mode, buffer, bufferSize);
    if (error != 0) {
        EmitError("ReadData");
    }
}

void CAENDgtz::MallocReadoutBuffer(char** buffer, uint32_t* size)
{
    error = (int)CAEN_DGTZ_MallocReadoutBuffer(handle, buffer, size);
    if (error != 0) {
        EmitError("MallocReadoutBuffer");
    }
}
void CAENDgtz::FreeReadoutBuffer(char** buffer)
{
    error = (int)CAEN_DGTZ_FreeReadoutBuffer(buffer);
    if (error != 0) {
        EmitError("FreeReadoutBuffer");
    }
}

// --------------------------------------------------------------------------------
// firmware specific methods
// --------------------------------------------------------------------------------

void CAENDgtz::SetChannelSelfTrigger(uint32_t channel, uint32_t mode)
{
    CAEN_DGTZ_TriggerMode_t caen_enum_mode;
    switch (mode) {
    case 1: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_ACQ_ONLY;
    } break;
    case 2: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_EXTOUT_ONLY;
    } break;
    case 3: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT;
    } break;
    default: {
        caen_enum_mode = CAEN_DGTZ_TRGMODE_DISABLED;
    }
    }
    if (channel == 255) {
        error = (int)CAEN_DGTZ_SetChannelSelfTrigger(handle, caen_enum_mode, 255);
    } else {
        error = (int)CAEN_DGTZ_SetChannelSelfTrigger(handle, caen_enum_mode, (1 << channel));
    }
    if (error != 0) {
        EmitError("SetChannelSelfTrigger");
    }
}

uint32_t CAENDgtz::GetChannelSelfTrigger(uint32_t channel)
{
    CAEN_DGTZ_TriggerMode_t caen_enum_mode;
    error = (int)CAEN_DGTZ_GetChannelSelfTrigger(handle, channel, &caen_enum_mode);
    if (error != 0) {
        EmitError("GetChannelSelfTrigger");
    }
    return (int)caen_enum_mode;
}

void CAENDgtz::SetChannelTriggerThreshold(uint32_t channel, uint32_t threshold)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x106C + 0x0100 * channel, 0, 14, threshold, "SetChannelTriggerThreshold");
    } else {
        error = (int)CAEN_DGTZ_SetChannelTriggerThreshold(handle, channel, threshold);
        if (error != 0) {
            EmitError("SetChannelTriggerThreshold");
        }
    }
}

uint32_t CAENDgtz::GetChannelTriggerThreshold(uint32_t channel)
{
    uint32_t Tvalue = 0;
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x106C + 0x0100 * channel, 0, 14, "GetChannelTriggerThreshold");
    } else {
        error = (int)CAEN_DGTZ_GetChannelTriggerThreshold(handle, channel, &Tvalue);
        if (error != 0) {
            EmitError("GetChannelTriggerThreshold");
        }
    }
    return Tvalue;
}

void CAENDgtz::SetChannelTriggerPolarity(uint32_t channel, uint32_t polarity)
{
    CAEN_DGTZ_TriggerPolarity_t caen_enum_polarity;
    switch (polarity) {
    case 1: {
        caen_enum_polarity = CAEN_DGTZ_TriggerOnFallingEdge;
    } break;
    default: {
        caen_enum_polarity = CAEN_DGTZ_TriggerOnRisingEdge;
    }
    }
    error = (int)CAEN_DGTZ_SetTriggerPolarity(handle, channel, caen_enum_polarity);
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelTriggerPolarity(uint32_t channel)
{
    CAEN_DGTZ_TriggerPolarity_t polarity;
    error = (int)CAEN_DGTZ_GetTriggerPolarity(handle, channel, &polarity);
    if (error != 0) {
        EmitError();
    }
    return (int)polarity;
}

void CAENDgtz::SetChannelTriggerNSamples(uint32_t channel, uint32_t nsamples)
{
    int packNsamples = 4 + GetEventPackaging();
    if (nsamples % packNsamples == 0) {
        uint32_t data = 0;
        uint32_t address = ADD_1ST_TRNS + (0x0100 * channel);
        error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
        data = (data & 0xF000) + ((nsamples / packNsamples) & 0x0FFF);
        error = (int)CAEN_DGTZ_WriteRegister(handle, address, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelTriggerNSamples(uint32_t channel)
{
    int packNsamples = 4 + GetEventPackaging();
    uint32_t data = 0;
    uint32_t address = ADD_1ST_TRNS + (0x0100 * channel);
    error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x0FFF) * packNsamples;
}

void CAENDgtz::SetChannelDCOffsetPercent(uint32_t channel, int value)
{
    if (value >= -50 && value <= 50) {
        const uint32_t offset = (int)((value + 50) * 65535 / 100);
        error = (int)CAEN_DGTZ_SetChannelDCOffset(handle, channel, offset);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetChannelDCOffsetPercent(uint32_t channel)
{
    uint32_t offset;
    error = (int)CAEN_DGTZ_GetChannelDCOffset(handle, channel, &offset);
    std::cout << "DCOffset: " << offset << std::endl;
    if (error != 0) {
        EmitError();
    }
    return round((offset / 655.35) - 50);
}

void CAENDgtz::SetChannelDCOffset10bit(uint32_t channel, uint32_t offset)
{
    if (offset < 1024) {
        error = (int)CAEN_DGTZ_SetChannelDCOffset(handle, channel, (offset << 6));
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelDCOffset10bit(uint32_t channel)
{
    uint32_t offset;
    error = (int)CAEN_DGTZ_GetChannelDCOffset(handle, channel, &offset);
    if (error != 0) {
        EmitError();
    }
    return (offset >> 6);
}

void CAENDgtz::SetChannelDCOffset12bit(uint32_t channel, uint32_t offset)
{
    if (offset < 4096) {
        error = (int)CAEN_DGTZ_SetChannelDCOffset(handle, channel, (offset << 4));
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelDCOffset12bit(uint32_t channel)
{
    uint32_t offset;
    error = (int)CAEN_DGTZ_GetChannelDCOffset(handle, channel, &offset);
    if (error != 0) {
        EmitError();
    }
    return (offset >> 4);
}

void CAENDgtz::SetChannelDCOffset14bit(uint32_t channel, uint32_t offset)
{
    if (offset < 16384) {
        error = (int)CAEN_DGTZ_SetChannelDCOffset(handle, channel, (offset << 2));
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelDCOffset14bit(uint32_t channel)
{
    uint32_t offset;
    error = (int)CAEN_DGTZ_GetChannelDCOffset(handle, channel, &offset);
    if (error != 0) {
        EmitError();
    }
    return (offset >> 2);
}

void CAENDgtz::SetChannelDCOffset16bit(uint32_t channel, uint32_t offset)
{
    if (offset < 65536) {
        error = (int)CAEN_DGTZ_SetChannelDCOffset(handle, channel, offset);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelDCOffset16bit(uint32_t channel)
{
    uint32_t offset;
    error = (int)CAEN_DGTZ_GetChannelDCOffset(handle, channel, &offset);
    std::cout << "DCOffset16bit: " << offset << std::endl;
    if (error != 0) {
        EmitError();
    }
    return offset;
}

uint32_t CAENDgtz::GetChannelDCOffset(uint32_t channel)
{
    return GetChannelDCOffset16bit(channel);
}

void CAENDgtz::SetMaxNumEventsBLT(uint32_t maxNumEvents)
{
    error = (int)CAEN_DGTZ_SetMaxNumEventsBLT(handle, maxNumEvents);
    if (error != 0) {
        EmitError("SetMaxNumEventsBLT");
    }
}

uint32_t CAENDgtz::GetMaxNumEventsBLT()
{
    uint32_t maxNumEvents;
    error = (int)CAEN_DGTZ_GetMaxNumEventsBLT(handle, &maxNumEvents);
    if (error != 0) {
        EmitError("GetMaxNumEventsBLT");
    }
    return maxNumEvents;
}

void CAENDgtz::SetPostTriggerSize(uint32_t percent)
{
    error = (int)CAEN_DGTZ_SetPostTriggerSize(handle, percent);
    if (error != 0) {
        EmitError("SetPostTriggerSize");
    }
}

uint32_t CAENDgtz::GetPostTriggerSize()
{
    uint32_t percent;
    error = (int)CAEN_DGTZ_GetPostTriggerSize(handle, &percent);
    if (error != 0) {
        EmitError("GetPostTriggerSize");
    }
    return percent;
}

void CAENDgtz::SetPostTriggerSamples(uint32_t samples)
{
    switch (boardInfo.vers) {
    case 720: {
        if (samples % 4 == 0) {
	  error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_TRG_PTNS, (samples / 4));
	  if (verboseDebug) printf("---> SetPostTriggerSamples: calling CAEN_DGTZ_WriteRegister(handle, 0x%x , %d )  :  %d\n",ADD_TRG_PTNS, (samples / 4), error);
       } else {
            error = -3;
        }
    } break;
    case 725: {
        if (samples % 4 == 0) {
	  error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_TRG_PTNS, (samples / 4));
	  if (verboseDebug) printf("---> SetPostTriggerSamples: calling CAEN_DGTZ_WriteRegister(handle, 0x%x , %d )  :  %d\n",ADD_TRG_PTNS, (samples / 4), error);
        } else {
            error = -3;
        }
    } break;
    case 730: {
        if (samples % 8 == 0) {
            error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_TRG_PTNS, (samples / 8));
        } else {
            error = -3;
        }
    } break;
    case 751: {
        if (samples % 16 == 0) {
            error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_TRG_PTNS, (samples / 16));
        } else {
            error = -3;
        }
    } break;
    default: {
    }
    } // end switch
    if (error != 0) {
        EmitError("SetPostTriggerSamples");
    }
}

uint32_t CAENDgtz::GetPostTriggerSamples()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_TRG_PTNS, &data);
    if (error != 0) {
        EmitError("GetPostTriggerSamples");
    }
    switch (boardInfo.vers) {
    case 720: {
        data *= 4;
    } break;
    case 725: {
        data *= 4;
    } break;
    case 730: {
        data *= 8;
    } break;
    case 751: {
        data *= 16;
    } break;
    default: {
    }
    } // end switch
    return data;
}

void CAENDgtz::SetChannelScopeSamples(uint32_t channel, uint32_t samples)
{
    error = (int)CAEN_DGTZ_SetRecordLength(handle, samples, channel);
    /*
    int samplesPerDivision = 0;
    uint32_t value = 0;
    switch (boardInfo.vers) {
        case 720:    {
            if (boardInfo.dppVersion==3) { samplesPerDivision = 8; } else {
                if (GetEventPackaging()==0) { samplesPerDivision = 2; } else { samplesPerDivision = 5; }
            }
        } break;
        case 730:    {
            if (boardInfo.dppVersion==3) { samplesPerDivision = 8; } else { samplesPerDivision = 10; }
        } break;
        case 751:    {
            if (boardInfo.dppVersion==3) { samplesPerDivision = 12; } else { samplesPerDivision = 7; } // (!) ignoring 2GS/s mode
        } break;
        default: {  }
    }    // end switch
    if (((int)samples)%samplesPerDivision==0) {
        // write value
        if (value<4096) {
            error = (int)CAEN_DGTZ_SetRecordLength(handle, samples, channel);
        } else { error = -3; }
    } else { error = -3; }
*/
    if (error != 0) {
        EmitError("SetChannelScopeSamples");
    }
}

uint32_t CAENDgtz::GetChannelScopeSamples(uint32_t channel)
{
    uint32_t ch = channel;
    if (ch == 255) {
        ch = 0;
    }
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_GetRecordLength(handle, &data, ch);
    if (error != 0) {
        EmitError("GetChannelScopeSamples");
    }
    return data;
}

void CAENDgtz::SetRunStartDelaySamples(uint32_t samples)
{
    if (samples % 2 == 0) {
        error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_RUN_DLAY, (samples / 2));
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError("SetRunStartDelaySamples");
    }
}

uint32_t CAENDgtz::GetRunStartDelaySamples()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_RUN_DLAY, &data);
    if (error != 0) {
        EmitError("GetRunStartDelaySamples");
    }
    return data * 2;
}

void CAENDgtz::SetRunStartDelayNanoseconds(uint32_t samples)
{
    if (samples % 8 == 0) {
        error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_RUN_DLAY, (samples / 8));
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError("SetRunStartDelayNanoseconds");
    }
}

uint32_t CAENDgtz::GetRunStartDelayNanoseconds()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_RUN_DLAY, &data);
    if (error != 0) {
        EmitError("GetRunStartDelayNanoseconds");
    }
    return data * 8;
}

// --------------------------------------------------------------------------------
// zero suppression
// --------------------------------------------------------------------------------

void CAENDgtz::SetZeroSuppressionMode(uint32_t mode)
{
    CAEN_DGTZ_ZS_Mode_t caen_enum_mode;
    switch (mode) {
    case 1: {
        caen_enum_mode = CAEN_DGTZ_ZS_INT;
    } break;
    case 2: {
        caen_enum_mode = CAEN_DGTZ_ZS_ZLE;
    } break;
    case 3: {
        caen_enum_mode = CAEN_DGTZ_ZS_AMP;
    } break;
    default: {
        caen_enum_mode = CAEN_DGTZ_ZS_NO;
    }
    }
    error = (int)CAEN_DGTZ_SetZeroSuppressionMode(handle, caen_enum_mode);
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetZeroSuppressionMode()
{
    CAEN_DGTZ_ZS_Mode_t mode;
    error = (int)CAEN_DGTZ_GetZeroSuppressionMode(handle, &mode);
    if (error != 0) {
        EmitError();
    }
    return (int)mode;
}

void CAENDgtz::SetChannelZSThresholdWeight(uint32_t channel, int weight)
{
    CAEN_DGTZ_ThresholdWeight_t caen_enum_weight;
    int32_t threshold, nsamp;
    error = (int)CAEN_DGTZ_GetChannelZSParams(handle, channel, &caen_enum_weight, &threshold, &nsamp);
    switch (weight) {
    case 1: {
        caen_enum_weight = CAEN_DGTZ_ZS_COARSE;
    } break;
    default: {
        caen_enum_weight = CAEN_DGTZ_ZS_FINE;
    }
    }
    error = (int)CAEN_DGTZ_SetChannelZSParams(handle, channel, caen_enum_weight, threshold, nsamp);
    if (error != 0) {
        EmitError();
    }
}

int32_t CAENDgtz::GetChannelZSThresholdWeight(uint32_t channel)
{
    CAEN_DGTZ_ThresholdWeight_t weight;
    int32_t threshold, nsamp;
    error = (int)CAEN_DGTZ_GetChannelZSParams(handle, channel, &weight, &threshold, &nsamp);
    if (error != 0) {
        EmitError();
    }
    return (int)weight;
}

void CAENDgtz::SetChannelZSLogic(uint32_t channel, uint32_t logic)
{
    if (logic < 2) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, (ADD_1ST_ZSTH + (0x0100 * channel)), &data);
        data = (data & 0x7FFFFFFF) + (logic << 31);
        error = CAEN_DGTZ_WriteRegister(handle, ADD_1ST_ZSTH, data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelZSLogic(uint32_t channel)
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, (ADD_1ST_ZSTH + (0x0100 * channel)), &data);
    if (error != 0) {
        EmitError();
    }
    return ((data & 0x80000000) >> 31);
}

void CAENDgtz::SetChannelZSThreshold(uint32_t channel, uint32_t threshold)
{
    if (threshold < 4096) {
        uint32_t data = 0;
        error = CAEN_DGTZ_ReadRegister(handle, (ADD_1ST_ZSTH + (0x0100 * channel)), &data);
        data = (data & 0xFFFFF000) + threshold;
        error = CAEN_DGTZ_WriteRegister(handle, (ADD_1ST_ZSTH + (0x0100 * channel)), data);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelZSThreshold(uint32_t channel)
{
    uint32_t data = 0;
    error = CAEN_DGTZ_ReadRegister(handle, (ADD_1ST_ZSTH + (0x0100 * channel)), &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00000FFF);
}

void CAENDgtz::SetChannelZSAMPNSamples(uint32_t channel, int32_t nsamples)
{
    if (GetZeroSuppressionMode() == 3) {
        if (nsamples < 0x20000) { // only first 21 bits are valid, see documentation
            error = CAEN_DGTZ_WriteRegister(handle, (ADD_1ST_ZSNS + (0x0100 * channel)), nsamples);
        } else {
            error = -3;
        }
    } else {
        error = -12;
    }
    if (error != 0) {
        EmitError();
    }
}

int32_t CAENDgtz::GetChannelZSAMPNSamples(uint32_t channel)
{
    uint32_t data = 0;
    if (GetZeroSuppressionMode() == 3) {
        error = CAEN_DGTZ_ReadRegister(handle, (ADD_1ST_ZSNS + (0x0100 * channel)), &data);
    } else {
        error = -12;
    }
    if (error != 0) {
        EmitError();
    }
    return (data & 0x0001FFFF);
}

void CAENDgtz::SetChannelZSZLENSamplesBeforeThr(uint32_t channel, int32_t nsamples)
{
    if (GetZeroSuppressionMode() == 2) {
        if (nsamples < 0x10000) { // only 16 bits are valid, see documentation
            uint32_t data = 0;
            error = CAEN_DGTZ_ReadRegister(handle, (ADD_1ST_ZSNS + (0x0100 * channel)), &data);
            data = (data & 0x0000FFFF) + (nsamples << 16);
            error = CAEN_DGTZ_WriteRegister(handle, (ADD_1ST_ZSNS + (0x0100 * channel)), data);
        } else {
            error = -3;
        }
    } else {
        error = -12;
    }
    if (error != 0) {
        EmitError();
    }
}

int32_t CAENDgtz::GetChannelZSZLENSamplesBeforeThr(uint32_t channel)
{
    uint32_t data = 0;
    if (GetZeroSuppressionMode() == 2) {
        error = CAEN_DGTZ_ReadRegister(handle, (ADD_1ST_ZSNS + (0x0100 * channel)), &data);
    } else {
        error = -12;
    }
    if (error != 0) {
        EmitError();
    }
    return (data >> 16);
}

void CAENDgtz::SetChannelZSZLENSamplesAfterThr(uint32_t channel, int32_t nsamples)
{
    if (GetZeroSuppressionMode() == 2) {
        if (nsamples < 0x10000) { // only 16 bits are valid, see documentation
            uint32_t data = 0;
            error = CAEN_DGTZ_ReadRegister(handle, (ADD_1ST_ZSNS + (0x0100 * channel)), &data);
            data = (data & 0xFFFF0000) + nsamples;
            error = CAEN_DGTZ_WriteRegister(handle, (ADD_1ST_ZSNS + (0x0100 * channel)), data);
        } else {
            error = -3;
        }
    } else {
        error = -12;
    }
    if (error != 0) {
        EmitError();
    }
}

int32_t CAENDgtz::GetChannelZSZLENSamplesAfterThr(uint32_t channel)
{
    uint32_t data = 0;
    if (GetZeroSuppressionMode() == 2) {
        error = CAEN_DGTZ_ReadRegister(handle, (ADD_1ST_ZSNS + (0x0100 * channel)), &data);
    } else {
        error = -12;
    }
    if (error != 0) {
        EmitError();
    }
    return (data & 0x0000FFFF);
}

// --------------------------------------------------------------------------------
// acquisition
// --------------------------------------------------------------------------------

void CAENDgtz::AllocateEvent(void** Evt)
{
    error = CAEN_DGTZ_AllocateEvent(handle, Evt);
    if (error != 0) {
        EmitError("AllocateEvent");
    }
}

void CAENDgtz::DecodeEvent(char* evtPtr, void** Evt)
{
    error = (int)CAEN_DGTZ_DecodeEvent(handle, evtPtr, Evt);
    if (error != 0) {
        EmitError("DecodeEvent");
    }
}

void CAENDgtz::FreeEvent(void** Evt)
{
    error = (int)CAEN_DGTZ_FreeEvent(handle, Evt);
    if (error != 0) {
        EmitError("FreeEvent");
    }
}
CAEN_DGTZ_EventInfo_t CAENDgtz::GetEventInfo(char* buffer, uint32_t buffsize, int32_t numEvent, char** EventPtr)
{
    CAEN_DGTZ_EventInfo_t eventInfo;
    error = (int)CAEN_DGTZ_GetEventInfo(handle, buffer, buffsize, numEvent, &eventInfo, (char**)EventPtr);
    if (error != 0) {
        EmitError("GetEventInfo");
    }
    return eventInfo;
}

uint32_t CAENDgtz::GetNumEvents(char* buffer, uint32_t buffsize)
{
    uint32_t numEvents;
    error = (int)CAEN_DGTZ_GetNumEvents(handle, buffer, buffsize, &numEvents);
    if (error != 0) {
        EmitError("GetNumEvents");
    }
    return numEvents;
}

// --------------------------------------------------------------------------------
// DPP-CI specific methods
// --------------------------------------------------------------------------------

int CAENDgtz::isPowerOfTwo(unsigned int x)
{
    return ((x != 0) && ((x & (~x + 1)) == x));
}

void CAENDgtz::SetDPPAcquisitionMode(int mode)
{
    error = (int)CAEN_DGTZ_SetDPPAcquisitionMode(handle, (CAEN_DGTZ_DPP_AcqMode_t)mode, (CAEN_DGTZ_DPP_SaveParam_t)2);
    if (error != 0) {
        EmitError("SetDPPAcquisitionMode");
    }
}

uint32_t CAENDgtz::GetDPPAcquisitionMode()
{
    CAEN_DGTZ_DPP_AcqMode_t mode;
    CAEN_DGTZ_DPP_SaveParam_t param;
    error = CAEN_DGTZ_GetDPPAcquisitionMode(handle, &mode, &param);
    if (error != 0) {
        EmitError("GetDPPAcquisitionMode");
    }
    return (int)mode;
}

void CAENDgtz::SetDPPSaveParameter(CAEN_DGTZ_DPP_SaveParam_t param)
{
    CAEN_DGTZ_DPP_AcqMode_t mode;
    CAEN_DGTZ_DPP_SaveParam_t futile;
    error = CAEN_DGTZ_GetDPPAcquisitionMode(handle, &mode, &futile);
    if (error != 0) {
        EmitError("SetDPPSaveParameter");
    }
    error = (int)CAEN_DGTZ_SetDPPAcquisitionMode(handle, mode, param);
    if (error != 0) {
        EmitError("SetDPPSaveParameter");
    }
}

uint32_t CAENDgtz::GetDPPSaveParameter()
{
    CAEN_DGTZ_DPP_AcqMode_t mode;
    CAEN_DGTZ_DPP_SaveParam_t param;
    error = CAEN_DGTZ_GetDPPAcquisitionMode(handle, &mode, &param);
    if (error != 0) {
        EmitError("GetDPPSaveParameter");
    }
    return (int)param;
}

/*
void CAENDgtz::SetDPPParameter(uint32_t channel, CAEN_DGTZ_DPP_PARAMETER_t param, uint32_t value) {
    error = (int)CAEN_DGTZ_SetDPPParameter(handle,channel,param,value);
    error = -99;
    if (error!=0) { EmitError(); }
}
*/

/*
uint32_t CAENDgtz::GetDPPParameter(uint32_t channel, CAEN_DGTZ_DPP_PARAMETER_t param) {
    uint32_t value = 0;
    error = (int)CAEN_DGTZ_GetDPPParameter(handle, channel, param, &value);
    error = -99;
    if (error!=0) { EmitError(); }
    return value;
}
*/

/*
void CAENDgtz::SetSingleDPPParameter(uint32_t channel, CAEN_DGTZ_DPP_PARAMETER_t Param, uint32_t value) {
    uint32_t Address;
    uint32_t Data;
    switch (Param) {
        case CAEN_DGTZ_DPP_Param_InvertInput: {
              // can't be set individually
            SetDPPParameter(255, CAEN_DGTZ_DPP_Param_InvertInput, value);
        } break;
        case CAEN_DGTZ_DPP_Param_NSBaseline: {
            if (value<4096 && isPowerOfTwo(value)) {
                Address = 0x102c + (0x0100*channel);
                error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
                if (error==0) {
                    Data = (Data & 0xf000ffff) + (0x00010000*value);
                    error = (int)CAEN_DGTZ_WriteRegister(handle, Address, Data);
                }
            } else { error = -3; }
        } break;
        case CAEN_DGTZ_DPP_Param_GateMode: {
            // can't be set individually
            SetDPPParameter(255, CAEN_DGTZ_DPP_Param_GateMode, value);
        } break;
        case CAEN_DGTZ_DPP_Param_GateWidth: {
            if (value<1024) {
                Address = 0x1028 + (0x0100*channel);
                error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
                if (error==0) {
                    Data = (Data & 0xfffffc00) + value;
                    error = (int)CAEN_DGTZ_WriteRegister(handle, Address, Data);
                }
            } else { error = -3; }
        } break;
        case CAEN_DGTZ_DPP_Param_PreGate: {
            if (value<256) {
                Address = 0x1028 + (0x0100*channel);
                error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
                if (error==0) {
                    Data = (Data & 0xfff00fff) + (0x0001000*value);
                    error = (int)CAEN_DGTZ_WriteRegister(handle, Address, Data);
                }
            } else { error = -3; }
        } break;
        case CAEN_DGTZ_DPP_Param_HoldOffTime: {
            if (value<2048) {
                Address = 0x1028 + (0x0100*channel);
                error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
                if (error==0) {
                    Data = (Data & 0x800fffff) + (0x00100000*value);
                    error = (int)CAEN_DGTZ_WriteRegister(handle, Address, Data);
                }
            } else { error = -3; }
        } break;
        case CAEN_DGTZ_DPP_Param_BslThreshold: {
            if (value<256) {
                Address = 0x1024 + (0x0100*channel);
                error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
                if (error==0) {
                    Data = (Data & 0xff00ffff) + (0x00010000*value);
                    error = (int)CAEN_DGTZ_WriteRegister(handle, Address, Data);
                }
            } else { error = -3; }            
        } break;
        case CAEN_DGTZ_DPP_Param_NoFlatTime: {
            if (value<2048) {
                Address = 0x102c + (0x0100*channel);
                error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
                if (error==0) {
                    Data = (Data & 0xfffff800) + value;
                    error = (int)CAEN_DGTZ_WriteRegister(handle, Address, Data);
                }
            } else { error = -3; }
        } break;
        case CAEN_DGTZ_DPP_Param_a: {
            if (value<64 && isPowerOfTwo(value)) {
                Address = 0x1024 + (0x0100*channel);
                error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
                if (error==0) {
                    Data = (Data & 0xffffc0ff) + (0x00000100*value);
                    error = (int)CAEN_DGTZ_WriteRegister(handle, Address, Data);
                }
            } else { error = -3; }
        } break;
        case CAEN_DGTZ_DPP_Param_b: {
            if (value<33) {
                Address = 0x1024 + (0x0100*channel);
                error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
                if (error==0) {
                    Data = (Data & 0xffffffc0) + value;
                    error = (int)CAEN_DGTZ_WriteRegister(handle, Address, Data);
                }
            } else { error = -3; }
        } break;
        default: {}
    }
    if (error!=0) { EmitError(); }
}
*/

/*
uint32_t CAENDgtz::GetSingleDPPParameter(uint32_t channel, CAEN_DGTZ_DPP_PARAMETER_t Param) {
    uint32_t Address, Data, value;
    switch (Param) {
        case CAEN_DGTZ_DPP_Param_InvertInput: {
            // can't be set individually
            value = GetDPPParameter(255, CAEN_DGTZ_DPP_Param_InvertInput);
        } break;
        case CAEN_DGTZ_DPP_Param_NSBaseline: {
            Address = 0x102c + (0x0100*channel);
            error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
            value = (Data & 0x0fff0000) / (0x00010000);
        } break;
        case CAEN_DGTZ_DPP_Param_GateMode: {
            // can't be set individually
            value = GetDPPParameter(255, CAEN_DGTZ_DPP_Param_GateMode);
        } break;
        case CAEN_DGTZ_DPP_Param_GateWidth: {
            Address = 0x1028 + (0x0100*channel);
            error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
            value = (Data & 0x000003ff);
        } break;
        case CAEN_DGTZ_DPP_Param_PreGate: {
            Address = 0x1028 + (0x0100*channel);
            error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
            value = (Data & 0x000ff000) / (0x00001000);
        } break;
        case CAEN_DGTZ_DPP_Param_HoldOffTime: {
            Address = 0x1028 + (0x0100*channel);
            error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
            value = (Data & 0x7ff00000) / (0x00100000);
        } break;
        case CAEN_DGTZ_DPP_Param_BslThreshold: {
            Address = 0x1024 + (0x0100*channel);
            error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
            value = (Data & 0x00ff0000) / (0x00010000);
        } break;
        case CAEN_DGTZ_DPP_Param_NoFlatTime: {
            Address = 0x102c + (0x0100*channel);
            error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
            value = (Data & 0x000007ff);
        } break;
        case CAEN_DGTZ_DPP_Param_a: {
            Address = 0x1024 + (0x0100*channel);
            error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
            value = (Data & 0x00003f00) / (0x00000100);
        } break;
        case CAEN_DGTZ_DPP_Param_b: {
            Address = 0x1024 + (0x0100*channel);
            error = (int)CAEN_DGTZ_ReadRegister(handle, Address, &Data);
            value = (Data & 0x0000003f);
        } break;
        default: { value = 0; }
    }
    if (error!=0) { EmitError(); }
    return value;
}
*/

// --------------------------------------------------------------------------------
// DPP-PSD specific methods
// --------------------------------------------------------------------------------

void CAENDgtz::ConfigSetPSDMandatoryBits()
{
    uint32_t value = 0;
    value = (1 << 4) | (1 << 8);
    WriteRegister(ADD_DPP_CNFG + 0x0004, value);
    value = 1 | (7 << 5) | (1 << 10);
    WriteRegister(ADD_DPP_CNFG + 0x0008, value);
}

void CAENDgtz::SetChannelPSDRatioFiltering(uint32_t channel, uint32_t value)
{
    uint32_t address = ADD_DPP_CTRL;
    if (channel < 255) {
        address += (channel << 8);
    } else {
        address += 0x7000;
    }
    if (value < 3) {
        SetRegisterSpecificBits(address, 27, 28, value);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError("SetChannelPSDRatioFiltering");
    }
}

uint32_t CAENDgtz::GetChannelPSDRatioFiltering(uint32_t channel)
{
    uint32_t address = ADD_DPP_CTRL;
    if (channel < 255) {
        address += (channel << 8);
    }
    return GetRegisterSpecificBits(address, 27, 28);
}

void CAENDgtz::SetChannelPSDRatioThresholdPercent(uint32_t channel, uint32_t value)
{
    uint32_t address = ADD_1ST_PSDC;
    if (channel < 255) {
        address += (channel << 8);
    } else {
        address += 0x7000;
    }
    if (value < 100) {
        value = (int)(value * 10.24 + 0.5);
        error = (int)CAEN_DGTZ_WriteRegister(handle, address, value);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError("SetChannelPSDRatioThresholdPercent");
    }
}

uint32_t CAENDgtz::GetChannelPSDRatioThresholdPercent(uint32_t channel)
{
    uint32_t data = 0;
    uint32_t address = ADD_1ST_PSDC;
    if (channel < 255) {
        address += (channel << 8);
    }
    error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
    if (error != 0) {
        EmitError();
    }
    data = (data / 10.24) + 0.5;
    return data;
}

void CAENDgtz::SetMaxNumAggregatesBLT(uint32_t maxNumEvents)
{
    SetMaxNumEventsBLT(maxNumEvents);
}

uint32_t CAENDgtz::GetMaxNumAggregatesBLT()
{
    return GetMaxNumEventsBLT();
}

void CAENDgtz::SetChannelMaxNumEventsPerAggregate(uint32_t channel, uint32_t maxNumEvents)
{
    SetRegisterSpecificBits(ADD_1ST_ANEV + 0x100 * channel, 0, 9, maxNumEvents, "SetMaxNumEventsPerAggregate");
}

uint32_t CAENDgtz::GetChannelMaxNumEventsPerAggregate(uint32_t channel)
{
    return GetRegisterSpecificBits(ADD_1ST_ANEV + 0x100 * channel, 0, 9, "SetMaxNumEventsPerAggregate");
    ;
}

void CAENDgtz::SetDualTrace(uint32_t value)
{
    if (value < 2) {
        uint32_t address = ADD_DPP_CNFG + 0x0004 * (2 - value);
        error = (int)CAEN_DGTZ_WriteRegister(handle, address, 1 << 11);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError("SetDualTrace");
    }
}

uint32_t CAENDgtz::GetDualTrace()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_DPP_CNFG, &data);
    if (error != 0) {
        EmitError("GetDualTrace");
    }
    return (data >> 11) & 0x1;
}

void CAENDgtz::SetAnalogProbe(uint32_t value)
{
    if (value < 2) {
        uint32_t address = ADD_DPP_CNFG + 0x0004 * (2 - value);
        error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_DPP_CNFG + 0x0008, 1 << 13);
        error = (int)CAEN_DGTZ_WriteRegister(handle, address, 1 << 12);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetAnalogProbe()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_DPP_CNFG, &data);
    if (error != 0) {
        EmitError();
    }
    return (data >> 12) & 0x1;
}

void CAENDgtz::SetDigitalProbe(uint32_t value)
{
    if (value < 3) {
        switch (value) {
        case 2: {
            error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_DPP_CNFG + 0x0008, 1 << 21);
            error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_DPP_CNFG + 0x0004, 1 << 22);
        } break;
        case 1: {
            error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_DPP_CNFG + 0x0004, 1 << 21);
            error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_DPP_CNFG + 0x0008, 1 << 22);
        } break;
        default: {
            error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_DPP_CNFG + 0x0008, 3 << 21);
        }
        }
        uint32_t address = ADD_DPP_CNFG + 0x0004 * (2 - value);
        error = (int)CAEN_DGTZ_WriteRegister(handle, address, 1 << 21);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetDigitalProbe()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_DPP_CNFG, &data);
    if (error != 0) {
        EmitError();
    }
    return (data >> 21) & 0x3;
}

void CAENDgtz::SetEnabledScope(uint32_t value)
{
    if (value < 2) {
        uint32_t address = ADD_DPP_CNFG + 0x0004 * (2 - value);
        error = (int)CAEN_DGTZ_WriteRegister(handle, address, 1 << 16);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetEnabledScope()
{
    if (boardInfo.dppVersion == 0) { // STD firmware
        return 1;
    } else { // DPP firmware
        uint32_t data = 0;
        error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_DPP_CNFG, &data);
        if (error != 0) {
            EmitError("GetEnabledScope");
        }
        return (data >> 16) & 0x1;
    }
}

void CAENDgtz::SetEnabledBSL(uint32_t value)
{
    if (value < 2) {
        uint32_t address = ADD_DPP_CNFG + 0x0004 * (2 - value);
        error = (int)CAEN_DGTZ_WriteRegister(handle, address, 1 << 17);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetEnabledBSL()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_DPP_CNFG, &data);
    if (error != 0) {
        EmitError();
    }
    return (data >> 17) & 0x1;
}

void CAENDgtz::SetEnabledTime(uint32_t value)
{
    if (value < 2) {
        uint32_t address = ADD_DPP_CNFG + 0x0004 * (2 - value);
        error = (int)CAEN_DGTZ_WriteRegister(handle, address, 1 << 18);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetEnabledTime()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_DPP_CNFG, &data);
    if (error != 0) {
        EmitError();
    }
    return (data >> 18) & 0x1;
}

void CAENDgtz::SetEnabledCharge(uint32_t value)
{
    if (value < 2) {
        uint32_t address = ADD_DPP_CNFG + 0x0004 * (2 - value);
        error = (int)CAEN_DGTZ_WriteRegister(handle, address, 1 << 19);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetEnabledCharge()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_DPP_CNFG, &data);
    if (error != 0) {
        EmitError();
    }
    return (data >> 19) & 0x1;
}

void CAENDgtz::SetChannelPreTrigger(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1038 + 0x0100 * channel, 0, 9, value / 2, "SetChannelPreTrigger");
    } else {
        error = (int)CAEN_DGTZ_SetDPPPreTriggerSize(handle, channel, value);
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelPreTrigger(uint32_t channel)
{
    uint32_t data = 0;
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        data = 2 * GetRegisterSpecificBits(0x1038 + 0x0100 * channel, 0, 9, "GetChannelPreTrigger");
    } else {
        error = (int)CAEN_DGTZ_GetDPPPreTriggerSize(handle, channel, &data);
    }
    if (error != 0) {
        EmitError();
    }
    return data;
}

void CAENDgtz::SetEnabledEventDataFormatWord(uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x8000, 24, 24, value, "SetEnabledEventDataFormatWord");
    } else {
        error = -7;
        EmitError();
    }
}

uint32_t CAENDgtz::GetEnabledEventDataFormatWord()
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x8000, 24, 24, "GetEnabledEventDataFormatWord");
    } else {
        error = -7;
        EmitError();
        return 0;
    }
}

void CAENDgtz::SetChannelThreshold(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x106C + 0x0100 * channel, 0, 14, value, "SetChannelThreshold");
    } else {
        uint32_t data = 0;
        uint32_t address = ADD_1ST_DTHR + (channel << 8);
        error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
        if (error == 0) {
            data = (data & 0xfffff000) + (value & 0xfff);
            error = (int)CAEN_DGTZ_WriteRegister(handle, address, data);
        }
        if (error != 0) {
            EmitError();
        }
    }
}

uint32_t CAENDgtz::GetChannelThreshold(uint32_t channel)
{
    uint32_t data = 0;
    uint32_t address = ADD_1ST_DTHR + (channel << 8);
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x106C + 0x0100 * channel, 0, 14, "GetChannelThreshold");
    } else {
        error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
        if (error != 0) {
            EmitError();
        }
    }
    return (data & 0xfff);
}

// ----------------------------------------------------------------------
// PreGate, ShortGate, LongGate: settings in samples and in nanoseconds
// *720:
// PreGate 8 bit register PGTE
// ShortGate 10 bit register SGTE
// LongGate 14 bit register LGTE
// *730:
// PreGate 8 bit register PGTE
// ShortGate 12 bit register SGTE
// LongGate 16 bit register LGTE
// *751:
// PreGate 8 bit register PGTE
// ShortGate 10 bit register SGTE
// LongGate 14 bit register LGTE
// ----------------------------------------------------------------------

uint8_t CAENDgtz::GetGateShortBitSize()
{
    uint8_t size = 0;
    switch (boardInfo.vers) {
    case 725: {
        size = 12;
    } break;
    case 730: {
        size = 12;
    } break;
    default: {
        size = 10;
    }
    }
    return size;
}

uint8_t CAENDgtz::GetGateLongBitSize()
{
    uint8_t size = 0;
    switch (boardInfo.vers) {
    case 725: {
        size = 16;
    } break;
    case 730: {
        size = 16;
    } break;
    default: {
        size = 14;
    }
    }
    return size;
}

void CAENDgtz::SetChannelPreGate(uint32_t channel, uint32_t value)
{
    SetRegisterSpecificBits(ADD_1ST_PGTE + (channel << 8), 0, 8, value);
}

uint32_t CAENDgtz::GetChannelPreGate(uint32_t channel)
{
    return GetRegisterSpecificBits(ADD_1ST_PGTE + (channel << 8), 0, 8);
}

void CAENDgtz::SetChannelShortGate(uint32_t channel, uint32_t value)
{
    SetRegisterSpecificBits(ADD_1ST_SGTE + (channel << 8), 0, GetGateShortBitSize(), value, "SetChannelShortGate");
    /*
    uint32_t data = 0;
    uint32_t address = ADD_1ST_SGTE + (channel<<8);
        error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
        if (error==0) {
            data = (data & 0xfffffc00) + (value & 0x3ff);
            error = (int)CAEN_DGTZ_WriteRegister(handle, address, data);
        }
    if (error!=0) { EmitError(); }    
*/
}

uint32_t CAENDgtz::GetChannelShortGate(uint32_t channel)
{
    return GetRegisterSpecificBits(ADD_1ST_SGTE + (channel << 8), 0, GetGateShortBitSize());
}

void CAENDgtz::SetChannelLongGate(uint32_t channel, uint32_t value)
{
    SetRegisterSpecificBits(ADD_1ST_LGTE + (channel << 8), 0, GetGateLongBitSize(), value);
    /*
    uint32_t address = ADD_1ST_LGTE + (channel<<8);
    if (value <= 0x3fff) {
        error = (int)CAEN_DGTZ_WriteRegister(handle, address, value);
    } else { error = -3; }    
    if (error!=0) { EmitError(); }    
*/
}

uint32_t CAENDgtz::GetChannelLongGate(uint32_t channel)
{
    return GetRegisterSpecificBits(ADD_1ST_LGTE + (channel << 8), 0, GetGateLongBitSize());
}

// ----- settings in nanoseconds:

void CAENDgtz::SetChannelPreGateNs(uint32_t channel, uint32_t value)
{
    if (value % boardInfo.nsPerSample == 0) {
        SetRegisterSpecificBits(ADD_1ST_PGTE + (channel << 8), 0, 8, value / boardInfo.nsPerSample);
    } else {
        error = -3;
        EmitError("Must be a multiple of the sample width.");
    }
}

uint32_t CAENDgtz::GetChannelPreGateNs(uint32_t channel)
{
    return boardInfo.nsPerSample * GetRegisterSpecificBits(ADD_1ST_PGTE + (channel << 8), 0, 8);
}

void CAENDgtz::SetChannelShortGateNs(uint32_t channel, uint32_t value)
{
    std::cout << "DEBUG: CAENDgtz::SetChannelShortGateNs() ";
    std::cout << "channel: " << channel << "; ";
    std::cout << "value: " << value << "; ";
    std::cout << "boardInfo.nsPerSample: '" << boardInfo.nsPerSample << "'; ";
    std::cout << std::endl;

    if (value % boardInfo.nsPerSample == 0) {
        SetRegisterSpecificBits(ADD_1ST_SGTE + (channel << 8), 0, GetGateShortBitSize(), value / boardInfo.nsPerSample);
    } else {
        error = -3;
        EmitError("Must be a multiple of the sample width.");
    }
}

uint32_t CAENDgtz::GetChannelShortGateNs(uint32_t channel)
{
    return boardInfo.nsPerSample * GetRegisterSpecificBits(ADD_1ST_SGTE + (channel << 8), 0, GetGateShortBitSize());
}

void CAENDgtz::SetChannelLongGateNs(uint32_t channel, uint32_t value)
{
    if (value % boardInfo.nsPerSample == 0) {
        SetRegisterSpecificBits(ADD_1ST_LGTE + (channel << 8), 0, GetGateLongBitSize(), value / boardInfo.nsPerSample);
    } else {
        error = -3;
        EmitError("Must be a multiple of the sample width.");
    }
}

uint32_t CAENDgtz::GetChannelLongGateNs(uint32_t channel)
{
    return boardInfo.nsPerSample * GetRegisterSpecificBits(ADD_1ST_LGTE + (channel << 8), 0, GetGateLongBitSize());
}

// ---------- end pregate / shortgate / longgate

void CAENDgtz::SetChannelPulsePolarity(uint32_t channel, uint32_t value)
{
    uint32_t data = 0;
    uint32_t address = ADD_DPP_CTRL + (0x0100 * channel);
    if (value < 2) {
        error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
        if (error == 0) {
            data = (data & 0xfffeffff) + (value << 16);
            error = (int)CAEN_DGTZ_WriteRegister(handle, address, data);
        }
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelPulsePolarity(uint32_t channel)
{
    uint32_t data = 0;
    uint32_t address = ADD_DPP_CTRL + (0x0100 * channel);
    error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00010000) >> 16;
}

void CAENDgtz::SetChannelTriggerMode(uint32_t channel, uint32_t value)
{
    uint32_t data = 0;
    uint32_t address = ADD_DPP_CTRL + (0x0100 * channel);
    if (value < 2) {
        error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
        if (error == 0) {
            data = (data & 0xfffdffff) + (value << 17);
            error = (int)CAEN_DGTZ_WriteRegister(handle, address, data);
        }
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelTriggerMode(uint32_t channel)
{
    uint32_t data = 0;
    uint32_t address = ADD_DPP_CTRL + (0x0100 * channel);
    error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x00020000) >> 17;
}

void CAENDgtz::SetChannelChargeSensitivity(uint32_t channel, uint32_t value)
{
    SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 0, 2, value);
}

uint32_t CAENDgtz::GetChannelChargeSensitivity(uint32_t channel)
{
    return GetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 0, 2);
}

void CAENDgtz::SetChannelBSLSamples(uint32_t channel, uint32_t value)
{
    //std::cout << value << std::endl;
    switch (boardInfo.vers) {
    case 724: { // *724
        switch (value) {
        case 0: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 0);
        } break;
        case 16: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 1);
        } break;
        case 64: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 2);
        } break;
        case 256: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 3);
        } break;
        case 1024: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 4);
        } break;
        case 4096: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 5);
        } break;
        case 16384: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 6);
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } break;
    case 751: { // *751
        switch (value) {
        case 0: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 0);
        } break;
        case 8: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 1);
        } break;
        case 16: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 2);
        } break;
        case 32: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 3);
        } break;
        case 64: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 4);
        } break;
        case 128: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 5);
        } break;
        case 256: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 6);
        } break;
        case 512: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 7);
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } break;
    case 725: { // *725 TO BE VERIFIED
      printf("=====> sono qui a settare la BSLSamples\n");
        switch (value) {
        case 0: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 0);
        } break;
        case 16: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 1);
        } break;
        case 64: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 2);
        } break;
        case 256: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 3);
        } break;
        case 1024: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 4);
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } break;
    case 730: { // *730
        switch (value) {
        case 0: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 0);
        } break;
        case 16: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 1);
        } break;
        case 64: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 2);
        } break;
        case 256: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 3);
        } break;
        case 1024: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 4);
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } break;
    case 720: { // *720
        switch (value) {
        case 0: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 0);
        } break;
        case 8: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 1);
        } break;
        case 32: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 2);
        } break;
        case 128: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 3);
        } break;
        case 512: {
            SetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22, 4);
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } break;
    default: {
        error = -7;
    }
    } // end switch

    std::cout << "Channel Baseline Samples: " << GetChannelBSLSamples(channel) << std::endl;

    if (error != 0) {
        EmitError("SetChannelBSLSamples");
    }
}

uint32_t CAENDgtz::GetChannelBSLSamples(uint32_t channel)
{
    uint32_t samples = 0;
    uint32_t value = GetRegisterSpecificBits(ADD_DPP_CTRL + (0x0100 * channel), 20, 22);
    switch (boardInfo.vers) {
    case 724: { // *724
        switch (value) {
        case 0: {
            samples = 0;
        } break;
        case 1: {
            samples = 16;
        } break;
        case 2: {
            samples = 64;
        } break;
        case 3: {
            samples = 256;
        } break;
        case 4: {
            samples = 1024;
        } break;
        case 5: {
            samples = 4096;
        } break;
        case 6: {
            samples = 16384;
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } break;
    case 751: { // *751
        switch (value) {
        case 0: {
            samples = 0;
        } break;
        case 1: {
            samples = 8;
        } break;
        case 2: {
            samples = 16;
        } break;
        case 3: {
            samples = 32;
        } break;
        case 4: {
            samples = 64;
        } break;
        case 5: {
            samples = 128;
        } break;
        case 6: {
            samples = 256;
        } break;
        case 7: {
            samples = 512;
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } break;
    case 725: { // *725 TO BE VERIFIED
      printf("=====> sono qui a rileggere la BSLSamples\n");
        switch (value) {
        case 0: {
            samples = 0;
        } break;
        case 1: {
            samples = 16;
        } break;
        case 2: {
            samples = 64;
        } break;
        case 3: {
            samples = 256;
        } break;
        case 4: {
            samples = 1024;
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } break;
    case 730: { // *730
        switch (value) {
        case 0: {
            samples = 0;
        } break;
        case 1: {
            samples = 16;
        } break;
        case 2: {
            samples = 64;
        } break;
        case 3: {
            samples = 256;
        } break;
        case 4: {
            samples = 1024;
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } break;
    case 720: { // *720
        switch (value) {
        case 0: {
            samples = 0;
        } break;
        case 1: {
            samples = 8;
        } break;
        case 2: {
            samples = 32;
        } break;
        case 3: {
            samples = 128;
        } break;
        case 4: {
            samples = 512;
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } break;
    default: {
        samples = 0;
        error = -7;
        EmitError("GetChannelBSLSamples");
    }
    } // end switch
    return samples;
}

void CAENDgtz::SetChannelBSLThreshold(uint32_t channel, uint32_t value)
{
    uint32_t data = 0;
    uint32_t address = ADD_1ST_BLTH + (0x0100 * channel);
    if (value < 128) {
        error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
        if (error == 0) {
            data = (data & 0xffffff80) + value;
            error = (int)CAEN_DGTZ_WriteRegister(handle, address, data);
        }
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelBSLThreshold(uint32_t channel)
{
    uint32_t data = 0;
    uint32_t address = ADD_1ST_BLTH + (0x0100 * channel);
    error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x0000007f);
}

void CAENDgtz::SetChannelBSLTimeout(uint32_t channel, uint32_t value)
{
    uint32_t data = 0;
    uint32_t address = ADD_1ST_BLTO + (0x0100 * channel);
    if (value < 256) {
        error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
        if (error == 0) {
            data = (data & 0xffffff00) + value;
            error = (int)CAEN_DGTZ_WriteRegister(handle, address, data);
        }
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

uint32_t CAENDgtz::GetChannelBSLTimeout(uint32_t channel)
{
    uint32_t data = 0;
    uint32_t address = ADD_1ST_BLTO + (0x0100 * channel);
    error = (int)CAEN_DGTZ_ReadRegister(handle, address, &data);
    if (error != 0) {
        EmitError();
    }
    return (data & 0x000000ff);
}

void CAENDgtz::SetDPPPow2NumBuffers(uint32_t value)
{
    if (value > 1 && value < 11) {
        uint32_t data = 0;
        error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_NUM_BUFF, &data);
        if (error == 0) {
            data = (data & 0xfffffff8) + value;
            error = (int)CAEN_DGTZ_WriteRegister(handle, ADD_NUM_BUFF, data);
        }
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError("SetDPPPow2NumBuffers");
    }
}

uint32_t CAENDgtz::GetDPPPow2NumBuffers()
{
    uint32_t data = 0;
    error = (int)CAEN_DGTZ_ReadRegister(handle, ADD_NUM_BUFF, &data);
    if (error != 0) {
        EmitError("GetDPPPow2NumBuffers");
    }
    return data & 0x00000007;
}

void CAENDgtz::SetNumEventsPerAggregate(uint32_t channel, uint32_t nEvPerBuff)
{
    error = CAEN_DGTZ_SetNumEventsPerAggregate(handle, nEvPerBuff, channel);
    if (error != 0) {
        EmitError("SetNumEventsPerAggregate");
    }
}

uint32_t CAENDgtz::GetNumEventsPerAggregate(uint32_t channel)
{
    uint32_t data = 0;
    error = CAEN_DGTZ_GetNumEventsPerAggregate(handle, &data, channel);
    if (error != 0) {
        EmitError("GetNumEventsPerAggregate");
    }
    return data;
}

void CAENDgtz::MallocDPPEvents(void** Events, uint32_t* AllocatedSize)
{
    error = CAEN_DGTZ_MallocDPPEvents(handle, Events, AllocatedSize);
    if (error != 0) {
        EmitError("MallocDPPEvents");
    }
}

void CAENDgtz::MallocDPPWaveforms(void** Waveforms, uint32_t* AllocatedSize)
{
    error = CAEN_DGTZ_MallocDPPWaveforms(handle, Waveforms, AllocatedSize);
    if (error != 0) {
        EmitError("MallocDPPWaveforms");
    }
}

void CAENDgtz::DecodeDPPWaveforms(void* event, void* waveforms)
{
    error = CAEN_DGTZ_DecodeDPPWaveforms(handle, event, waveforms);
    if (error != 0) {
        EmitError("DecodeDPPWaveforms");
    }
}

void CAENDgtz::GetDPPEvents(char* buffer, uint32_t BufferSize, void** Events, uint32_t* NumEvents)
{
    error = CAEN_DGTZ_GetDPPEvents(handle, buffer, BufferSize, Events, NumEvents);
    if (error != 0) {
        EmitError("GetDPPEvents");
    }
}

void CAENDgtz::FreeDPPWaveforms(void* Waveforms)
{
    error = CAEN_DGTZ_FreeDPPWaveforms(handle, Waveforms);
    if (error != 0) {
        EmitError("FreeDPPWaveforms");
    }
}

void CAENDgtz::FreeDPPEvents(void** Events)
{
    error = CAEN_DGTZ_FreeDPPEvents(handle, Events);
    if (error != 0) {
        EmitError("FreeDPPEvents");
    }
}

void CAENDgtz::SetChannelEnabledPUR(int channel, int value)
{
    SetRegisterSpecificBits(ADD_DPP_CTRL + (0x100 * channel), 26, 26, value, "SetChannelEnabledPUR");
}

uint32_t CAENDgtz::GetChannelEnabledPUR(int channel)
{
    return GetRegisterSpecificBits(ADD_DPP_CTRL + (0x100 * channel), 26, 26, "GetChannelEnabledPUR");
}

void CAENDgtz::SetChannelPURGap(int channel, int value)
{
    if (boardInfo.vers == 720 && boardInfo.dppVersion == 3) {
        SetRegisterSpecificBits(ADD_1ST_PGAP + (0x100 * channel), 0, 11, value, "SetChannelPURGap");
    } else {
        error = -17;
        EmitError("SetPURGap");
    }
}

uint32_t CAENDgtz::GetChannelPURGap(int channel)
{
    if (boardInfo.vers == 720 && boardInfo.dppVersion == 3) {
        return GetRegisterSpecificBits(ADD_1ST_PGAP + (0x100 * channel), 0, 11, "GetChannelPURGap");
    } else {
        error = -17;
        EmitError("GetChannelPURGap");
        return 0;
    }
}

void CAENDgtz::SetChannelTriggerHoldOff(int channel, int value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1074 + 0x0100 * channel, 0, 5, value, "SetChannelTriggerHoldOff");
    } else {
        SetRegisterSpecificBits(0x1074 + 0x0100 * channel, 0, 15, value, "SetChannelTriggerHoldOff");
    }
}

int CAENDgtz::GetChannelTriggerHoldOff(int channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return (int)GetRegisterSpecificBits(0x1074 + 0x0100 * channel, 0, 5, "GetChannelTriggerHoldOff");
    } else {
        return (int)GetRegisterSpecificBits(0x1074 + 0x0100 * channel, 0, 15, "GetChannelTriggerHoldOff");
    }
}

void CAENDgtz::SetChannelTriggerHoldOffNs(int channel, int value)
{
    if (value % 8 == 0) { // unit = 8 ns, always (*751, *730, *720)
        SetChannelTriggerHoldOff(channel, value / 8);
    } else {
        error = -3;
        EmitError("Must be a multiple of 8 ns");
    }
}

int CAENDgtz::GetChannelTriggerHoldOffNs(int channel)
{
    return 8 * GetChannelTriggerHoldOff(channel);
}

// --------------------------------------------------------------------------------
// *751 Dual Edge Sampling
// --------------------------------------------------------------------------------

void CAENDgtz::SetDualEdgeMode(int value)
{
    if (boardInfo.vers == 751 && boardInfo.dppVersion == 0) {
        if ((value & 1) == value) {
            // disable even channels - AUTOMATICALLY DONE
            //SetChannelEnableMask( (GetChannelEnableMask() & 0x5555) );
            // select dual edge sampling mode
            WriteRegister(ADD_CHA_CNFG + 0x0004 * (2 - value), 1 << 12, "SetDualEdgeMode()");
            // channel calibration
            WriteRegister(ADD_ADC_CONF, 0);
            WriteRegister(ADD_ADC_CONF, 2);
            // pause while calibrating
            while (GetRegisterSpecificBits(0x1088, 6, 6) == 0) {
            }
            WriteRegister(ADD_ADC_CONF, 0);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError("SetDualEdgeMode");
    }
    return;
}

int CAENDgtz::GetDualEdgeMode()
{
    if (boardInfo.vers == 751 && boardInfo.dppVersion == 0) {
        return GetRegisterSpecificBits(ADD_CHA_CNFG, 12, 12);
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError("GetDualEdgeMode");
    }
    return error;
}

// --------------------------------------------------------------------------------
// coincidences
// --------------------------------------------------------------------------------

void CAENDgtz::SetEnabledCoincidencesOnBoard(int value)
{
    SetRegisterSpecificBits(ADD_CHA_CNFG, 2, 2, value);
}

int CAENDgtz::GetEnabledCoincidencesOnBoard()
{
    return GetRegisterSpecificBits(ADD_CHA_CNFG, 2, 2);
}

void CAENDgtz::SetChannelCoincidenceMode(int channel, int mode)
{
    if ((channel >= 0 && channel < (int)boardInfo.Channels) || (channel == 255)) {
        uint32_t address = 0x1080;
        if (channel < 255) {
            address += (channel << 8);
        } else {
            address += 0x7000;
        }
        SetRegisterSpecificBits(address, 18, 19, mode);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetChannelCoincidenceMode(int channel)
{
    int mode = 0;
    if ((channel >= 0 && channel < (int)boardInfo.Channels) || (channel == 255)) {
        uint32_t address = 0x1080;
        if (channel < 255) {
            address += (channel << 8);
        } else {
            address += 0x7000;
        }
        mode = GetRegisterSpecificBits(address, 18, 19);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
    return mode;
}

void CAENDgtz::SetChannelCoincidenceWindowNs(int channel, int ns)
{
    if (channel >= 0 && channel < (int)boardInfo.Channels && ns % 8 == 0) {
        // durata impulso trigger - trigger out width
        SetRegisterSpecificBits(ADD_1ST_CWIN + (channel << 8), 0, 7, ns / 8);
        // trigger validation acceptance window (default = 4)
        // attesa AGGIUNTIVA per validare il trigger originale
        SetRegisterSpecificBits(0x106C + (channel << 8), 0, 7, 8);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetChannelCoincidenceWindowNs(int channel)
{
    uint32_t ns = 0;
    if (channel >= 0 && channel < (int)boardInfo.Channels) {
        ns = 8 * GetRegisterSpecificBits(0x1070 + (channel << 8), 0, 7);
    } else {
        error = -3;
    }
    if (error != 0) {
        EmitError();
    }
    return (int)ns;
}

void CAENDgtz::SetChannelCoincMask(int channel, int mask)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        error = -7;
        EmitError();
    } else {
        SetRegisterSpecificBits(0x8180 + channel * 4, 0, 7, mask);
    }
    return;
}

int CAENDgtz::GetChannelCoincMask(int channel)
{
    uint32_t mask = 0;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        error = -7;
        EmitError();
    } else {
        mask = GetRegisterSpecificBits(0x8180 + channel * 4, 0, 7);
    }
    return (int)mask;
}

void CAENDgtz::SetChannelCoincLogic(int channel, int logic)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        error = -7;
        EmitError();
    } else {
        SetRegisterSpecificBits(0x8180 + channel * 4, 8, 9, logic);
    }
}

int CAENDgtz::GetChannelCoincLogic(int channel)
{
    uint32_t logic = 0;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        error = -7;
    } else {
        if (channel >= 0 && channel < (int)boardInfo.Channels) {
            logic = GetRegisterSpecificBits(0x8180 + channel * 4, 8, 9);
        } else {
            error = -3;
        }
    }
    if (error != 0) {
        EmitError();
    }
    return (int)logic;
}

void CAENDgtz::SetChannelCoincMajorityLevel(int channel, int level)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        error = -7;
        EmitError();
    } else {
        SetRegisterSpecificBits(0x8180 + channel * 4, 10, 12, level);
    }
}

int CAENDgtz::GetChannelCoincMajorityLevel(int channel)
{
    uint32_t level = 0;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        error = -7;
    } else {
        if (channel >= 0 && channel < (int)boardInfo.Channels) {
            level = GetRegisterSpecificBits(0x8180 + channel * 4, 10, 12);
        } else {
            error = -3;
        }
    }
    if (error != 0) {
        EmitError();
    }
    return (int)level;
}

void CAENDgtz::SetChannelCoincExtTriggerMask(int channel, int mask)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        error = -7;
        EmitError();
    } else {
        SetRegisterSpecificBits(0x8180 + channel * 4, 29, 31, mask);
    }
}

int CAENDgtz::GetChannelCoincExtTriggerMask(int channel)
{
    uint32_t mask = 0;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        error = -7;
    } else {
        if (channel >= 0 && channel < (int)boardInfo.Channels) {
            mask = GetRegisterSpecificBits(0x8180 + channel * 4, 29, 31);
        } else {
            error = -3;
        }
    }
    if (error != 0) {
        EmitError();
    }
    return (int)mask;
}

// -----------------------------------
// coincidences between couples (*730)
// ----------
// register 0x1n84 ADD_DPP_CTR2 (n = channel)
// even channels R/W, odd channels R
// ----------
// Bit mapping:
// [1:0]    CoupleTriggerOutMode
//                00 chA AND chB
//                01 chA only
//                10 chB only
//                11 chA OR chB
// [2]        CoupleTriggerOutEnabled
//                0 off, 1 on
// [5:4]    CoupleValidationSource
//                00 from motherboard AND from channel
//                01 from motherboard only
//                10 from channel only
//                11 from motherboard OR from channel
// [6]        CoupleTriggerValidationEnabled
//                0 off, 1 on
// ----------
// register 0x1n80 ADD_DPP_CTRL (n = channel)
// even channels R/W, odd channels R/W
// ----------
// Bit mapping:
//    [19:18]    CoupleValidationMode
//                00 normal mode
//                01 coincidence
//                10 - not used -
//                11 anticoincidence
// ----------
// register 0x818n lower validation mask (n = 4*couple)
// ----------
// Bit mapping:
//    [0:7]    CoupleValidationMask
//    [8:9]    CoupleValidationLogic
//                00 OR
//                01 AND
//                10 majority
//                11 - not used -
//    [10:12]    CoupleValidationMajorityLevel
//    [29:31]    CoupleValidationExtTriggerMask
//                [29] I/O LVDS
//                [30] EXT TRG
//                [31] SW TRG

void CAENDgtz::SetCoupleTriggerOutEnabled(int couple, int enabled)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            SetRegisterSpecificBits(ADD_DPP_CTR2 + 0x0200 * couple, 2, 2, enabled);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetCoupleTriggerOutEnabled(int couple)
{
    int enabled = -1;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            enabled = (int)GetRegisterSpecificBits(ADD_DPP_CTR2 + 0x0200 * couple, 2, 2);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
    return enabled;
}

void CAENDgtz::SetCoupleTriggerOutMode(int couple, int mode)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            SetRegisterSpecificBits(ADD_DPP_CTR2 + 0x0200 * couple, 0, 1, mode);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetCoupleTriggerOutMode(int couple)
{
    int mode = -1;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            mode = (int)GetRegisterSpecificBits(ADD_DPP_CTR2 + 0x0200 * couple, 0, 1);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
    return mode;
}

void CAENDgtz::SetCoupleValidationEnabled(int couple, int enabled)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            SetRegisterSpecificBits(ADD_DPP_CTR2 + 0x0200 * couple, 6, 6, enabled);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetCoupleValidationEnabled(int couple)
{
    int enabled = -1;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            enabled = (int)GetRegisterSpecificBits(ADD_DPP_CTR2 + 0x0200 * couple, 6, 6);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
    return enabled;
}

void CAENDgtz::SetChannelValidationMode(int channel, int mode)
{
    if (boardInfo.dppVersion == 1 || boardInfo.dppVersion == 3) {
        if (channel >= 0 && channel < ((int)boardInfo.Channels)) {
            SetRegisterSpecificBits(ADD_DPP_CTRL + 0x0100 * channel, 18, 19, mode);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetChannelValidationMode(int channel)
{
    int mode = -1;
    if (boardInfo.dppVersion == 1 || boardInfo.dppVersion == 3) {
        if (channel >= 0 && channel < ((int)boardInfo.Channels)) {
            mode = (int)GetRegisterSpecificBits(ADD_DPP_CTRL + 0x0100 * channel, 18, 19);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
    return mode;
}

void CAENDgtz::SetCoupleValidationSource(int couple, int source)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            SetRegisterSpecificBits(ADD_DPP_CTR2 + 0x0200 * couple, 4, 5, source);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetCoupleValidationSource(int couple)
{
    int source = -1;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            source = (int)GetRegisterSpecificBits(ADD_DPP_CTR2 + 0x0200 * couple, 4, 5);
        } else {
            error = -13;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
    return source;
}

void CAENDgtz::SetCoupleValidationMask(int couple, int mask)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            SetRegisterSpecificBits(0x8180 + couple * 4, 0, 7, mask);
        } else {
            error = -3;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetCoupleValidationMask(int couple)
{
    uint32_t mask = 0;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            mask = GetRegisterSpecificBits(0x8180 + couple * 4, 0, 7);
        } else {
            error = -3;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
    return (int)mask;
}

void CAENDgtz::SetCoupleValidationLogic(int couple, int logic)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            SetRegisterSpecificBits(0x8180 + couple * 4, 8, 9, logic);
        } else {
            error = -3;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetCoupleValidationLogic(int couple)
{
    uint32_t logic = 0;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            logic = GetRegisterSpecificBits(0x8180 + couple * 4, 8, 9);
        } else {
            error = -3;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
    return (int)logic;
}

void CAENDgtz::SetCoupleValidationMajorityLevel(int couple, int level)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            SetRegisterSpecificBits(0x8180 + couple * 4, 10, 12, level);
        } else {
            error = -3;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetCoupleValidationMajorityLevel(int couple)
{
    uint32_t level = 0;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            level = GetRegisterSpecificBits(0x8180 + couple * 4, 10, 12);
        } else {
            error = -3;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
    return (int)level;
}

void CAENDgtz::SetCoupleValidationExtTriggerMask(int couple, int mask)
{
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            SetRegisterSpecificBits(0x8180 + couple * 4, 29, 31, mask);
        } else {
            error = -3;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
}

int CAENDgtz::GetCoupleValidationExtTriggerMask(int couple)
{
    uint32_t mask = 0;
    if (boardInfo.vers == 730 && boardInfo.dppVersion == 3) {
        if (couple >= 0 && couple < ((int)boardInfo.Channels / 2)) {
            mask = GetRegisterSpecificBits(0x8180 + couple * 4, 29, 31);
        } else {
            error = -3;
        }
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError();
    }
    return (int)mask;
}

// --------------------------------------------------------------------------------
// *730 only
// --------------------------------------------------------------------------------

void CAENDgtz::SetChannelDynamicRange(int channel, int value)
{
    if (boardInfo.vers == 730 || boardInfo.vers == 725) {
        if (channel == 255 && value > -1 && value < 2) {
            WriteRegister(ADD_1ST_3VPP + 0x7000, value, "SetChannelDynamicRange");
        } else {
            SetRegisterSpecificBits(ADD_1ST_3VPP + 0x0100 * channel, 0, 0, value);
        }
    } else if (boardInfo.vers == 724) {
        switch (value) {
        case 0: {
            SetRegisterSpecificBits(0x10B4 + 0x0100 * channel, 0, 3, 5, "SetChannelDynamicRange");
        } break;
        case 1: {
            SetRegisterSpecificBits(0x10B4 + 0x0100 * channel, 0, 3, 6, "SetChannelDynamicRange");
        } break;
        case 2: {
            SetRegisterSpecificBits(0x10B4 + 0x0100 * channel, 0, 3, 9, "SetChannelDynamicRange");
        } break;
        case 3: {
            SetRegisterSpecificBits(0x10B4 + 0x0100 * channel, 0, 3, 10, "SetChannelDynamicRange");
        } break;
        default: {
            error = -3;
            EmitError("SetChannelDynamicRange");
        }
        } // end switch
    } else {
        error = -7;
        EmitError("SetChannelDynamicRange");
    }
}

int CAENDgtz::GetChannelDynamicRange(int channel)
{
    if (boardInfo.vers == 730 || boardInfo.vers == 725) {
        return GetRegisterSpecificBits((channel == 255) ? (ADD_1ST_3VPP) : (ADD_1ST_3VPP + 0x0100 * channel), 0, 0, "GetChannelDynamicRange");
    } else if (boardInfo.vers == 724) {
        switch (GetRegisterSpecificBits(0x10B4 + 0x0100 * channel, 0, 3, "GetChannelDynamicRange")) {
        case 0x5: {
            return 0;
        } break;
        case 0x6: {
            return 1;
        } break;
        case 0x9: {
            return 2;
        } break;
        case 0xA: {
            return 3;
        } break;
        } // end switch
    } else {
        error = -7;
        EmitError("GetChannelDynamicRange");
    }
    return 0;
}

int CAENDgtz::GetChannelTemperature(int channel)
{
    if (boardInfo.vers == 730) {
        return ReadRegister(0x10A8 + 0x0100 * channel, "GetChannelTemperature");
    } else {
        error = -7;
        EmitError("GetChannelTemperature");
    }
    return 0;
}

int CAENDgtz::GetMaximumTemperature()
{
    int maximum = 0;
    if (boardInfo.vers == 730 || boardInfo.vers == 725) {
        for (unsigned int channel = 0; channel < boardInfo.Channels; ++channel) {
            const int current = ReadRegister(0x10A8 + 0x0100 * channel, "GetChannelTemperature");
            if (current > maximum) {
                maximum = current;
            }
        }
    } else {
        error = -7;
        EmitError("GetMaximumTemperature");
    }
    return maximum;
}

void CAENDgtz::SetCoupleVetoMode(uint32_t couple, uint32_t value)
{
    SetRegisterSpecificBits(0x1084 + 0x0200 * couple, 7, 7, value, "SetCoupleVetoMode");
}

uint32_t CAENDgtz::GetCoupleVetoMode(uint32_t couple)
{
    return GetRegisterSpecificBits(0x1084 + 0x0200 * couple, 7, 7, "GetCoupleVetoMode");
}

// 0x1nD4: veto duration (1 LSB = 8 ns)
void CAENDgtz::SetChannelVetoWindow(uint32_t channel, uint32_t value)
{
    SetRegisterSpecificBits(0x10D4 + 0x0100 * channel, 0, 16, value, "SetChannelVetoWindow");
}

uint32_t CAENDgtz::GetChannelVetoWindow(uint32_t channel)
{
    return GetRegisterSpecificBits(0x10D4 + 0x0100 * channel, 0, 16, "GetChannelVetoWindow");
}

void CAENDgtz::SetChannelSpectrumThreshold(uint32_t channel, uint32_t value)
{
    specThr[channel] = value;
}

uint16_t CAENDgtz::GetChannelSpectrumThreshold(uint32_t channel)
{
    return specThr[channel];
}

// --------------------------------------------------------------------------------
// DPP-PHA specific parameters
// --------------------------------------------------------------------------------

void CAENDgtz::SetChannelTriggerFilterSmoothing(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1054 + 0x0100 * channel, 0, 5, value);
    } else {
        error = -7;
        EmitError("SetChannelTriggerFilterSmoothing");
    }
    return;
}

uint32_t CAENDgtz::GetChannelTriggerFilterSmoothing(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x1054 + 0x0100 * channel, 0, 5);
    } else {
        error = -7;
        EmitError("GetChannelTriggerFilterSmoothing");
        return 0;
    }
}

void CAENDgtz::SetChannelInputRiseTime(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1058 + 0x0100 * channel, 0, 7, value);
    } else {
        error = -7;
        EmitError("SetChannelInputRiseTime");
    }
    return;
}

uint32_t CAENDgtz::GetChannelInputRiseTime(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x1058 + 0x0100 * channel, 0, 7);
    } else {
        error = -7;
        EmitError("GetChannelInputRiseTime");
        return 0;
    }
}

void CAENDgtz::SetChannelTrapezoidRiseTime(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x105C + 0x0100 * channel, 0, 9, value);
    } else {
        error = -7;
        EmitError("SetChannelTrapezoidRiseTime");
    }
    return;
}

uint32_t CAENDgtz::GetChannelTrapezoidRiseTime(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x105C + 0x0100 * channel, 0, 9);
    } else {
        error = -7;
        EmitError("GetChannelTrapezoidRiseTime");
        return 0;
    }
}

void CAENDgtz::SetChannelTrapezoidFlatTop(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1060 + 0x0100 * channel, 0, 9, value);
    } else {
        error = -7;
        EmitError("SetChannelTrapezoidFlatTop");
    }
    return;
}

uint32_t CAENDgtz::GetChannelTrapezoidFlatTop(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x1060 + 0x0100 * channel, 0, 9);
    } else {
        error = -7;
        EmitError("GetChannelTrapezoidFlatTop");
        return 0;
    }
}

void CAENDgtz::SetChannelPeakingTime(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1064 + 0x0100 * channel, 0, 10, value);
    } else {
        error = -7;
        EmitError("SetChannelPeakingTime");
    }
    return;
}

uint32_t CAENDgtz::GetChannelPeakingTime(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x1064 + 0x0100 * channel, 0, 10);
    } else {
        error = -7;
        EmitError("GetChannelPeakingTime");
        return 0;
    }
}

void CAENDgtz::SetChannelDecayTime(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1068 + 0x0100 * channel, 0, 15, value);
    } else {
        error = -7;
        EmitError("SetChannelDecayTime");
    }
    return;
}

uint32_t CAENDgtz::GetChannelDecayTime(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x1068 + 0x0100 * channel, 0, 15);
    } else {
        error = -7;
        EmitError("GetChannelDecayTime");
        return 0;
    }
}

void CAENDgtz::SetChannelRTDWindowWidth(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1070 + 0x0100 * channel, 0, 9, value);
    } else {
        error = -7;
        EmitError("SetChannelRTDWindowWidth");
    }
    return;
}

uint32_t CAENDgtz::GetChannelRTDWindowWidth(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x1070 + 0x0100 * channel, 0, 9);
    } else {
        error = -7;
        EmitError("GetChannelRTDWindowWidth");
        return 0;
    }
}

void CAENDgtz::SetChannelPeakHoldOffExtension(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1078 + 0x0100 * channel, 0, 7, value);
    } else {
        error = -7;
        EmitError("SetChannelPeakHoldOffExtension");
    }
    return;
}

uint32_t CAENDgtz::GetChannelPeakHoldOffExtension(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x1078 + 0x0100 * channel, 0, 7);
    } else {
        error = -7;
        EmitError("GetChannelPeakHoldOffExtension");
        return 0;
    }
}

void CAENDgtz::SetChannelBaselineHoldOffExtension(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x107C + 0x0100 * channel, 0, 7, value);
    } else {
        error = -7;
        EmitError("SetChannelBaselineHoldOffExtension");
    }
    return;
}

uint32_t CAENDgtz::GetChannelBaselineHoldOffExtension(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x107C + 0x0100 * channel, 0, 7);
    } else {
        error = -7;
        EmitError("GetChannelBaselineHoldOffExtension");
        return 0;
    }
}

void CAENDgtz::SetChannelTrapezoidRescaling(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 0, 5, value, "SetChannelTrapezoidRescaling");
    } else {
        error = -7;
        EmitError("SetChannelTrapezoidRescaling");
    }
    return;
}

uint32_t CAENDgtz::GetChannelTrapezoidRescaling(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x1080 + (0x0100 * channel), 0, 5, "GtChannelTrapezoidRescaling");
    } else {
        error = -7;
        EmitError("SetChannelTrapezoidRescaling");
    }
    return 0;
}

void CAENDgtz::SetChannelDecimation(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        switch (value) {
        case 0: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 8, 9, 0, "SetChannelDigitalGain");
        } break;
        case 2: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 8, 9, 1, "SetChannelDigitalGain");
        } break;
        case 4: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 8, 9, 2, "SetChannelDigitalGain");
        } break;
        case 8: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 8, 9, 3, "SetChannelDigitalGain");
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } else {
        error = -7;
        EmitError("SetChannelDecimation");
    }
    return;
}

uint32_t CAENDgtz::GetChannelDecimation(uint32_t channel)
{
    uint32_t decimation = 0;
    const uint32_t value = GetRegisterSpecificBits(0x1080 + (0x0100 * channel), 8, 9);
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        switch (value) {
        case 0: {
            decimation = 0;
        } break;
        case 1: {
            decimation = 2;
        } break;
        case 2: {
            decimation = 4;
        } break;
        case 3: {
            decimation = 8;
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } else {
        error = -7;
    }
    if (error != 0) {
        EmitError("GetChannelDecimation");
    }
    return decimation;
}

void CAENDgtz::SetChannelDigitalGain(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        switch (value) {
        case 1: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 10, 11, 0, "SetChannelDigitalGain");
        } break;
        case 2: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 10, 11, 1, "SetChannelDigitalGain");
        } break;
        case 4: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 10, 11, 2, "SetChannelDigitalGain");
        } break;
        case 8: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 10, 11, 3, "SetChannelDigitalGain");
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } else {
        error = -7;
        EmitError("SetChannelDigitalGain");
    }
    return;
}

uint32_t CAENDgtz::GetChannelDigitalGain(uint32_t channel)
{
    uint32_t gain = 0;
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        const uint32_t value = GetRegisterSpecificBits(0x1080 + (0x0100 * channel), 10, 11);
        switch (value) {
        case 0: {
            gain = 1;
        } break;
        case 1: {
            gain = 2;
        } break;
        case 2: {
            gain = 4;
        } break;
        case 3: {
            gain = 8;
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } else {
        error = -7;
    } // end switch
    if (error != 0) {
        EmitError("GetChannelDigitalGain");
    }
    return gain;
}

void CAENDgtz::SetChannelPeakAveragingWindow(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        switch (value) {
        case 0: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 12, 13, 0, "SetChannelPeakAveragingWindow");
        } break;
        case 4: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 12, 13, 1, "SetChannelPeakAveragingWindow");
        } break;
        case 16: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 12, 13, 2, "SetChannelPeakAveragingWindow");
        } break;
        case 64: {
            SetRegisterSpecificBits(0x1080 + (0x0100 * channel), 12, 13, 3, "SetChannelPeakAveragingWindow");
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } else {
        error = -7;
        EmitError("SetChannelPeakAveragingWindow");
    }
    return;
}

uint32_t CAENDgtz::GetChannelPeakAveragingWindow(uint32_t channel)
{
    uint32_t samples = 0;
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        const uint32_t value = GetRegisterSpecificBits(0x1080 + (0x0100 * channel), 12, 13);
        switch (value) {
        case 0: {
            samples = 0;
        } break;
        case 1: {
            samples = 4;
        } break;
        case 2: {
            samples = 16;
        } break;
        case 3: {
            samples = 64;
        } break;
        default: {
            error = -3;
        }
        } // end switch value
    } else {
        error = -7;
    } // end switch
    if (error != 0) {
        EmitError("GetChannelPeakAveragingWindow");
    }
    return samples;
}

void CAENDgtz::SetChannelEnabledRollOverEvents(uint32_t channel, uint32_t value)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        SetRegisterSpecificBits(0x1080 + 0x0100 * channel, 26, 26, value);
    } else {
        error = -7;
        EmitError("SetChannelEnabledRollOverEvents");
    }
    return;
}

uint32_t CAENDgtz::GetChannelEnabledRollOverEvents(uint32_t channel)
{
    if (boardInfo.vers == 724 && boardInfo.dppVersion == 1) {
        return GetRegisterSpecificBits(0x1080 + 0x0100 * channel, 26, 26);
    } else {
        error = -7;
        EmitError("GetChannelEnabledRollOverEvents");
        return 0;
    }
}

void CAENDgtz::SetChannelPileUpFlagging(int channel, int value)
{
    if (boardInfo.vers == 724) {
        SetRegisterSpecificBits(0x1080 + (0x100 * channel), 27, 27, value, "SetChannelPileUpFlagging");
    } else {
        error = -7;
        EmitError("GetChannelPileUpFlagging");
    }
}

uint32_t CAENDgtz::GetChannelPileUpFlagging(int channel)
{
    if (boardInfo.vers == 724) {
        return GetRegisterSpecificBits(0x1080 + (0x100 * channel), 27, 27, "GetChannelPileUpFlagging");
    } else {
        error = -7;
        EmitError("GetChannelPileUpFlagging");
        return 0;
    }
}

// --------------------------------------------------------------------------------
// board configuration
// --------------------------------------------------------------------------------

void CAENDgtz::ConfigureFromFile(const char* config_file_name)
{

    // reading configuration values
    std::ifstream config_file(config_file_name, std::ifstream::in);

    if (!config_file.good()) {
        error = errors::CONFIG_FILE_NOT_FOUND;
        EmitError();
        return;
    }

    Json::Value config;
    //Json::Reader reader;
    //const bool parse_success = reader.parse(config_file, config, false);
    Json::CharReaderBuilder json_reader;
    std::string json_parsing_error;
    const bool parse_success = Json::parseFromStream(json_reader,
                                                     config_file,
                                                     &config,
                                                     &json_parsing_error);

    config_file.close();


    if (!parse_success) {
        error = errors::CONFIG_FILE_NOT_FOUND;
        EmitError();
        return;
    }

    ConfigureFromJSON(config);
}

// FIXME
void CAENDgtz::ConfigureFromJSON(const Json::Value &config)
{
    // TODO: separate different firmware's configuration parameters
    std::cout << "Configuring from JSON..." << std::endl;

    if (boardInfo.dppVersion == 3) {
        std::cout << "Configuring mandatory bits for DPP..." << std::endl;
        ConfigSetPSDMandatoryBits();
    }


    if (config.isMember("dgtzs")) {
        const Json::Value digitizer_config = config["dgtzs"];

        const Json::Value::Members members = digitizer_config.getMemberNames();

        for (const std::string &member : members) {
            std::cout << "config[\"dgtzs\"] member: " << member << std::endl;

            const std::string::size_type n = member.find("ch");

            if (n == std::string::npos) {
                // We have a keyword with no channel associated,
                // thus this is a global keyword.

                const std::string keyword = member;
                std::string s_value;
                int value = 0;
                
                try
                {
                    s_value = digitizer_config[keyword].asString();
                }
                catch (...) {}
                // We cannot use Json::LogicError to keep retrocompatibility with Ubuntu 14
                //catch (Json::LogicError &je) { std::cout << "JSON Logic error: " << je.what() << std::endl; }

                try
                {
                    value = digitizer_config[keyword].asInt();
                }
                catch (...) {}
                //catch (Json::LogicError &je) { std::cout << "JSON Logic error: " << je.what() << std::endl; }

                std::cout << "config[\"dgtzs\"][\"" << keyword << "\"]: '" << s_value << "', " << value << std::endl;

                if (keyword.compare("BoardName") == 0) {
                    SetBoardName(s_value.data());
                    continue;
                }

                // general keywords
                if (keyword.compare("FanSpeed") == 0) {
                    SetFanSpeed(value);
                    if (value - GetFanSpeed() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("VerboseDebug") == 0) {
                    SetVerboseDebug(value);
                    continue;
                }
                if (keyword.compare("ExtTriggerInputMode") == 0) {
                    SetExtTriggerInputMode(value);
                    if (value - GetExtTriggerInputMode() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("EventPackaging") == 0) {
                    SetEventPackaging(value);
                    if (value - GetEventPackaging() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("TriggerUnderThreshold") == 0) {
                    SetTriggerUnderThreshold(value);
                    if (value - GetTriggerUnderThreshold() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("MemoryAccess") == 0) {
                    SetMemoryAccess(value);
                    if (value - GetMemoryAccess() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("TestPatternGeneration") == 0) {
                    SetTestPatternGeneration(value);
                    if (value - GetTestPatternGeneration() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("TriggerOverlap") == 0) {
                    SetTriggerOverlap(value);
                    if (value - GetTriggerOverlap() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("RunStartDelaySamples") == 0) {
                    SetRunStartDelaySamples(value);
                    if (value - GetRunStartDelaySamples() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("RunStartDelayNanoseconds") == 0) {
                    SetRunStartDelayNanoseconds(value);
                    if (value - GetRunStartDelayNanoseconds() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("InterruptMode") == 0) {
                    SetInterruptMode(value);
                    if (value - GetInterruptMode() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("EnabledRELOC") == 0) {
                    SetEnabledRELOC(value);
                    if (value - GetEnabledRELOC() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("EnabledALIGN64") == 0) {
                    SetEnabledALIGN64(value);
                    if (value - GetEnabledALIGN64() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("EnabledBERR") == 0) {
                    SetEnabledBERR(value);
                    if (value - GetEnabledBERR() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("EnabledOLIRQ") == 0) {
                    SetEnabledOLIRQ(value);
                    if (value - GetEnabledOLIRQ() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("InterruptLevel") == 0) {
                    SetInterruptLevel(value);
                    if (value - GetInterruptLevel() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("InterruptStatusID") == 0) {
                    SetInterruptStatusID(value);
                    if (value - GetInterruptStatusID() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("InterruptEventNumber") == 0) {
                    SetInterruptEventNumber(value);
                    if (value - GetInterruptEventNumber() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("BufferBlockNumber") == 0) {
                    SetBufferBlockNumber(value);
                    if (value - GetBufferBlockNumber() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }

                if (keyword.compare("MaxNumEventsBLT") == 0) {
                    SetMaxNumEventsBLT(value);
                    if (value - GetMaxNumEventsBLT() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("ChannelEnableMask") == 0) {
                    SetChannelEnableMask(value);
                    if (value - GetChannelEnableMask() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("CoincidenceLevel") == 0) {
                    SetCoincidenceLevel(value);
                    if (value - GetCoincidenceLevel() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("CoincidenceMask") == 0) {
                    SetCoincidenceMask(value);
                    if (value - GetCoincidenceMask() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("TriggerEnableMask") == 0) {
                    SetCoincidenceMask(value);
                    if (value - GetCoincidenceMask() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("AcquisitionMode") == 0) {
                    SetAcquisitionMode(value);
                    if (value - GetAcquisitionMode() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("SWTriggerMode") == 0) {
                    SetSWTriggerMode(value);
                    if (value - GetSWTriggerMode() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }

                if (keyword.compare("PostTriggerSize") == 0) {
                    SetPostTriggerSize(value);
                    if (value - (int)GetPostTriggerSize() < 0) {
                        std::cout << "(!)  Got: " << (int)GetPostTriggerSize() << std::endl;
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("PostTriggerSamples") == 0) {
                    SetPostTriggerSamples(value);
                    if (value - GetPostTriggerSamples() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }

                // *751 Dual Edge Mode
                if (keyword.compare("DualEdgeMode") == 0) {
                    SetDualEdgeMode(value);
                    if (value - GetDualEdgeMode()) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                // zero suppression
                if (keyword.compare("ZeroSuppressionMode") == 0) {
                    SetZeroSuppressionMode(value);
                    if (value - GetZeroSuppressionMode() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }

                // front panel I/O control
                if (keyword.compare("IOTrgOutProgram") == 0) {
                    SetIOTrgOutProgram(value);
                    if (value - GetIOTrgOutProgram() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOTrgOutDisplay") == 0) {
                    SetIOTrgOutDisplay(value);
                    if (value - GetIOTrgOutDisplay() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOTrgOutMode") == 0) {
                    SetIOTrgOutMode(value);
                    if (value - GetIOTrgOutMode() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOTrgOutTestMode") == 0) {
                    SetIOTrgOutTestMode(value);
                    if (value - GetIOTrgOutTestMode() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOExtTrgPropagation") == 0) {
                    SetIOExtTrgPropagation(value);
                    if (value - GetIOExtTrgPropagation() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOPatternLatchMode") == 0) {
                    SetIOPatternLatchMode(value);
                    if (value - GetIOPatternLatchMode() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOLVDSFourthBlkOutputs") == 0) {
                    SetIOLVDSFourthBlkOutputs(value);
                    if (value - GetIOLVDSFourthBlkOutputs() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOLVDSThirdBlkOutputs") == 0) {
                    SetIOLVDSThirdBlkOutputs(value);
                    if (value - GetIOLVDSThirdBlkOutputs() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOLVDSSecondBlkOutputs") == 0) {
                    SetIOLVDSSecondBlkOutputs(value);
                    if (value - GetIOLVDSSecondBlkOutputs() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOLVDSFirstBlkOutputs") == 0) {
                    SetIOLVDSFirstBlkOutputs(value);
                    if (value - GetIOLVDSFirstBlkOutputs() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOMode") == 0) {
                    SetIOMode(value);
                    if (value - GetIOMode() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOHighImpedence") == 0) {
                    SetIOHighImpedence(value);
                    if (value - GetIOHighImpedence() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("IOLevel") == 0) {
                    SetIOLevel(value);
                    if (value - GetIOLevel() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }

                // DPP-PSD specific keywords
                if (keyword.compare("DPPAcquisitionMode") == 0) {
                    SetDPPAcquisitionMode(value);
                    if (value - GetDPPAcquisitionMode() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("MaxNumAggregatesBLT") == 0) {
                    SetMaxNumEventsBLT(value);
                    if (value - GetMaxNumEventsBLT() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }

                if (keyword.compare("EnabledScope") == 0) {
                    SetEnabledScope(value);
                    if (value - GetEnabledScope() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("EnabledBSL") == 0) {
                    SetEnabledBSL(value);
                    if (value - GetEnabledBSL() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("EnabledTime") == 0) {
                    SetEnabledTime(value);
                    if (value - GetEnabledTime() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("EnabledCharge") == 0) {
                    SetEnabledCharge(value);
                    if (value - GetEnabledCharge() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("EnabledEventDataFormatWord") == 0) {
                    SetEnabledEventDataFormatWord(value);
                    if (value - GetEnabledEventDataFormatWord() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }

                if (keyword.compare("DualTrace") == 0) {
                    SetDualTrace(value);
                    if (value - GetDualTrace() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("AnalogProbe") == 0) {
                    SetAnalogProbe(value);
                    if (value - GetAnalogProbe() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }
                if (keyword.compare("DigitalProbe") == 0) {
                    SetDigitalProbe(value);
                    if (value - GetDigitalProbe() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }

                if (keyword.compare("IsMaster") == 0) {
                    isMaster = value;
                    continue;
                }
                if (keyword.compare("ShowGates") == 0) {
                    showGates = value;
                    continue;
                }
                if (keyword.compare("WriteBuffer") == 0) {
                    writeBuffer = value;
                    continue;
                }

                // coincidences
                if (keyword.compare("EnabledCoincidencesOnBoard") == 0) {
                    SetEnabledCoincidencesOnBoard(value);
                    if (value - GetEnabledCoincidencesOnBoard() != 0) {
                        std::cout << "(!)  ^--- problem here" << std::endl;
                        error = -3;
                        EmitError();
                    }
                    continue;
                }

                // End of global keywords
            } else {
                // This is a keyword describing a channel,
                // thus we need to go down in the config tree.

                const int channel = std::stoi(member.substr(2));
                const Json::Value channel_config = digitizer_config[member];

                const Json::Value::Members channel_members = channel_config.getMemberNames();

                for (const std::string &keyword : channel_members) {
                    std::cout << "config[\"dgtzs\"][\"" << member << "\"]: " << keyword << std::endl;

                    std::string s_value;
                    int value = 0;
                    
                    try
                    {
                        s_value = channel_config[keyword].asString();
                    }
                    catch (...) {}
                    // We cannot use Json::LogicError to keep retrocompatibility with Ubuntu 14
                    //catch (Json::LogicError &je) { std::cout << "JSON Logic error: " << je.what() << std::endl; }
                    try
                    {
                        value = channel_config[keyword].asInt();
                    }
                    catch (...) {}
                    //catch (Json::LogicError &je) { std::cout << "JSON Logic error: " << je.what() << std::endl; }

                    std::cout << "config[\"dgtzs\"][\"" << member << "\"][\"" << keyword << "\"]: '" << s_value << "', " << value << std::endl;

                    if ((channel < 0 || channel > (MAXC_DG - 1)) && channel != 255) {
                        if (verboseDebug) {
                            printf("%s: Invalid channel in config JSON keyword: %s\n", boardName.data(), keyword.data());
                        }
                        continue;
                    }

                    // channel 255 is valid (255 = all channels)
                    if (keyword.compare("ScopeSamples") == 0) {
                        SetChannelScopeSamples(channel, value);
                        if (value - GetChannelScopeSamples(channel) != 0) {
                            std::cout << "(!)  ^--- problem here [" << GetChannelScopeSamples(channel) << "]" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("RecordLength") == 0) {
                        SetChannelScopeSamples(channel, value);
                        if (value - GetChannelScopeSamples(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PSDRatioFiltering") == 0) {
                        SetChannelPSDRatioFiltering(channel, value);
                        if (value - GetChannelPSDRatioFiltering(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PSDRatioThresholdPercent") == 0) {
                        SetChannelPSDRatioThresholdPercent(channel, value);
                        if (value - GetChannelPSDRatioThresholdPercent(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("SelfTrigger") == 0) {
                        SetChannelSelfTrigger(channel, value);
                        if (value - GetChannelSelfTrigger(0) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("DynamicRange") == 0) {
                        SetChannelDynamicRange(channel, value);
                        if (boardInfo.vers != 724 && value - GetChannelDynamicRange(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (channel == 255) {
                        continue;
                    } // channel 255 is not valid anymore
                    // names

                    if (keyword.compare("ChannelName") == 0) {
                        SetChannelName(channel, s_value.data());
                        continue;
                    }
                    if (keyword.compare("SpectrumThreshold") == 0) {
                        SetChannelSpectrumThreshold(channel, value);
                        continue;
                    }

                    if (keyword.compare("TriggerThreshold") == 0) {
                        SetChannelTriggerThreshold(channel, value);
                        if (value - GetChannelTriggerThreshold(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("DCOffsetPercent") == 0) {
                        SetChannelDCOffsetPercent(channel, value);
                        if (value - GetChannelDCOffsetPercent(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("DCOffset10bit") == 0) {
                        SetChannelDCOffset10bit(channel, value);
                        if (value - GetChannelDCOffset10bit(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("DCOffset12bit") == 0) {
                        SetChannelDCOffset12bit(channel, value);
                        if (value - GetChannelDCOffset12bit(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("DCOffset14bit") == 0) {
                        SetChannelDCOffset14bit(channel, value);
                        if (value - GetChannelDCOffset14bit(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("DCOffset16bit") == 0) {
                        SetChannelDCOffset16bit(channel, value);
                        if (value - GetChannelDCOffset16bit(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }

                    if (keyword.compare("TriggerPolarity") == 0) {
                        SetChannelTriggerPolarity(channel, value);
                        if (value - GetChannelTriggerPolarity(channel)) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }

                    if (keyword.compare("TriggerNSamples") == 0) {
                        SetChannelTriggerNSamples(channel, value);
                        if (value - GetChannelTriggerNSamples(channel)) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }

                    if (keyword.compare("ZSThresholdWeight") == 0) {
                        SetChannelZSThresholdWeight(channel, value);
                        if (value - GetChannelZSThresholdWeight(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("ZSLogic") == 0) {
                        SetChannelZSLogic(channel, value);
                        if (value - GetChannelZSLogic(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("ZSThreshold") == 0) {
                        SetChannelZSThreshold(channel, value);
                        if (value - GetChannelZSThreshold(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("ZSAMPNSamples") == 0) {
                        SetChannelZSAMPNSamples(channel, value);
                        if (value - GetChannelZSAMPNSamples(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("ZSZLENSamplesBeforeThr") == 0) {
                        SetChannelZSZLENSamplesBeforeThr(channel, value);
                        if (value - GetChannelZSZLENSamplesBeforeThr(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }

                    if (keyword.compare("ZSZLENSamplesAfterThr") == 0) {
                        SetChannelZSZLENSamplesAfterThr(channel, value);
                        if (value - GetChannelZSZLENSamplesAfterThr(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }

                    if (keyword.compare("MaxNumEvPerBuff") == 0) {
                        SetChannelMaxNumEventsPerAggregate(channel, value);
                        if (value - GetChannelMaxNumEventsPerAggregate(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("MaxNumEventsPerAggregate") == 0) {
                        SetChannelMaxNumEventsPerAggregate(channel, value);
                        if (value - GetChannelMaxNumEventsPerAggregate(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PulsePolarity") == 0) {
                        SetChannelPulsePolarity(channel, value);
                        if (value - GetChannelPulsePolarity(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("TriggerMode") == 0) {
                        SetChannelTriggerMode(channel, value);
                        if (value - GetChannelTriggerMode(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }

                    if (keyword.compare("ChargeSensitivity") == 0) {
                        SetChannelChargeSensitivity(channel, value);
                        if (value - GetChannelChargeSensitivity(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PreTrigger") == 0) {
                        SetChannelPreTrigger(channel, value);
                        if (value - GetChannelPreTrigger(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("Threshold") == 0) {
                        SetChannelThreshold(channel, value);
                        if (value - GetChannelThreshold(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("ShortGate") == 0) {
                        SetChannelShortGate(channel, value);
                        if (value - GetChannelShortGate(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("LongGate") == 0) {
                        SetChannelLongGate(channel, value);
                        if (value - GetChannelLongGate(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PreGate") == 0) {
                        SetChannelPreGate(channel, value);
                        if (value - GetChannelPreGate(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("ShortGateNs") == 0) {
                        SetChannelShortGateNs(channel, value);
                        if (value - GetChannelShortGateNs(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("LongGateNs") == 0) {
                        SetChannelLongGateNs(channel, value);
                        if (value - GetChannelLongGateNs(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PreGateNs") == 0) {
                        SetChannelPreGateNs(channel, value);
                        if (value - GetChannelPreGateNs(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }

                    if (keyword.compare("BSLSamples") == 0) {
                        SetChannelBSLSamples(channel, value);
                        if (value - GetChannelBSLSamples(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("BSLThreshold") == 0) {
                        SetChannelBSLThreshold(channel, value);
                        if (value - GetChannelBSLThreshold(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("BSLTimeout") == 0) {
                        SetChannelBSLTimeout(channel, value);
                        if (value - GetChannelBSLTimeout(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("EnabledPUR") == 0) {
                        SetChannelEnabledPUR(channel, value);
                        if (value - GetChannelEnabledPUR(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PURGap") == 0) {
                        SetChannelPURGap(channel, value);
                        if (value - GetChannelPURGap(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("TriggerHoldOff") == 0) {
                        SetChannelTriggerHoldOff(channel, value);
                        if (value - GetChannelTriggerHoldOff(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("TriggerHoldOffNs") == 0) {
                        SetChannelTriggerHoldOffNs(channel, value);
                        if (value - GetChannelTriggerHoldOffNs(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }

                    if (keyword.compare("ChannelCoincidenceMode") == 0) {
                        SetChannelCoincidenceMode(channel, value);
                        if (value - GetChannelCoincidenceMode(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoincidenceWindowNs") == 0) {
                        SetChannelCoincidenceWindowNs(channel, value);
                        if (value - GetChannelCoincidenceWindowNs(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoincMask") == 0) {
                        SetChannelCoincMask(channel, value);
                        if (value - GetChannelCoincMask(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoincLogic") == 0) {
                        SetChannelCoincLogic(channel, value);
                        if (value - GetChannelCoincLogic(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoincMajorityLevel") == 0) {
                        SetChannelCoincMajorityLevel(channel, value);
                        if (value - GetChannelCoincMajorityLevel(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoincExtTriggerMask") == 0) {
                        SetChannelCoincExtTriggerMask(channel, value);
                        if (value - GetChannelCoincExtTriggerMask(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    // coincidences - *730 only
                    if (keyword.compare("ChannelValidationMode") == 0) {
                        SetChannelValidationMode(channel, value);
                        if (value - GetChannelValidationMode(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoupleTriggerOutEnabled") == 0) {
                        SetCoupleTriggerOutEnabled(channel, value);
                        if (value - GetCoupleTriggerOutEnabled(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoupleTriggerOutMode") == 0) {
                        SetCoupleTriggerOutMode(channel, value);
                        if (value - GetCoupleTriggerOutMode(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoupleValidationEnabled") == 0) {
                        SetCoupleValidationEnabled(channel, value);
                        if (value - GetCoupleValidationEnabled(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoupleValidationSource") == 0) {
                        SetCoupleValidationSource(channel, value);
                        if (value - GetCoupleValidationSource(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoupleValidationMask") == 0) {
                        SetCoupleValidationMask(channel, value);
                        if (value - GetCoupleValidationMask(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoupleValidationLogic") == 0) {
                        SetCoupleValidationLogic(channel, value);
                        if (value - GetCoupleValidationLogic(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoupleValidationMajorityLevel") == 0) {
                        SetCoupleValidationMajorityLevel(channel, value);
                        if (value - GetCoupleValidationMajorityLevel(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoupleValidationExtTriggerMask") == 0) {
                        SetCoupleValidationExtTriggerMask(channel, value);
                        if (value - GetCoupleValidationExtTriggerMask(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("CoupleVetoMode") == 0) {
                        SetCoupleVetoMode(channel, value);
                        if (value - GetCoupleVetoMode(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("ChannelVetoWindow") == 0) {
                        SetChannelVetoWindow(channel, value);
                        if (value - GetChannelVetoWindow(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    // DPP-PHA specific keywords
                    if (keyword.compare("TriggerFilterSmoothing") == 0) {
                        SetChannelTriggerFilterSmoothing(channel, value);
                        if (value - GetChannelTriggerFilterSmoothing(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("InputRiseTime") == 0) {
                        SetChannelInputRiseTime(channel, value);
                        if (value - GetChannelInputRiseTime(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("TrapezoidRiseTime") == 0) {
                        SetChannelTrapezoidRiseTime(channel, value);
                        if (value - GetChannelTrapezoidRiseTime(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("TrapezoidFlatTop") == 0) {
                        SetChannelTrapezoidFlatTop(channel, value);
                        if (value - GetChannelTrapezoidFlatTop(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("TrapezoidFlatTopDelay") == 0) {
                        SetChannelPeakingTime(channel, value);
                        if (value - GetChannelPeakingTime(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PeakingTime") == 0) {
                        SetChannelPeakingTime(channel, value);
                        if (value - GetChannelPeakingTime(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("DecayTime") == 0) {
                        SetChannelDecayTime(channel, value);
                        if (value - GetChannelDecayTime(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("RTDWindowWidth") == 0) {
                        SetChannelRTDWindowWidth(channel, value);
                        if (value - GetChannelRTDWindowWidth(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PeakHoldOffExtension") == 0) {
                        SetChannelPeakHoldOffExtension(channel, value);
                        if (value - GetChannelPeakHoldOffExtension(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("BaselineHoldOffExtension") == 0) {
                        SetChannelBaselineHoldOffExtension(channel, value);
                        if (value - GetChannelBaselineHoldOffExtension(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("TrapezoidRescaling") == 0) {
                        SetChannelTrapezoidRescaling(channel, value);
                        if (value - GetChannelTrapezoidRescaling(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("Decimation") == 0) {
                        SetChannelDecimation(channel, value);
                        if (value - GetChannelDecimation(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("DigitalGain") == 0) {
                        SetChannelDigitalGain(channel, value);
                        if (value - GetChannelDigitalGain(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PeakAveragingWindow") == 0) {
                        SetChannelPeakAveragingWindow(channel, value);
                        if (value - GetChannelPeakAveragingWindow(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("EnabledRollOverEvents") == 0) {
                        SetChannelEnabledRollOverEvents(channel, value);
                        if (value - GetChannelEnabledRollOverEvents(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                    if (keyword.compare("PileUpFlagging") == 0) {
                        SetChannelPileUpFlagging(channel, value);
                        if (value - GetChannelPileUpFlagging(channel) != 0) {
                            std::cout << "(!)  ^--- problem here" << std::endl;
                            error = -3;
                            EmitError();
                        }
                        continue;
                    }
                }
            }
        } // end parsing of a single line
    } // end reading file

    if (config.isMember("abcd"))
    {
        // Let us put here the new configuration format
        const Json::Value abcd_config = config["abcd"];

        const Json::Value::Members members = abcd_config.getMemberNames();

        for (const std::string &member : members)
        {
            std::cout << "config[\"abcd\"] member: " << member << std::endl;

            const std::string keyword = member;
            std::string s_value;
            int value = 0;
                
            try
            {
                s_value = abcd_config[keyword].asString();
            }
            catch (...) {}
            // We cannot use Json::LogicError to keep retrocompatibility with Ubuntu 14
            //catch (Json::LogicError &je) { std::cout << "JSON Logic error: " << je.what() << std::endl; }

            try
            {
                value = abcd_config[keyword].asInt();
            }
            catch (...) {}
            //catch (Json::LogicError &je) { std::cout << "JSON Logic error: " << je.what() << std::endl; }

            std::cout << "config[\"abcd\"][\"" << keyword << "\"]: '" << s_value << "', " << value << std::endl;

            if (keyword.compare("registers") == 0)
            {
                // This is an array with some specific registers to set
                // A register should have a structure such as:
                // { "address": "0x103C",
                //   "value": 100,
                //   "start": 0,
                //   "n": 8 }
                for (const Json::Value &register_obj: abcd_config[keyword])
                {
                    if (register_obj.isMember("address")
                        && register_obj.isMember("value")
                        && register_obj.isMember("start")
                        && register_obj.isMember("n"))
                    {
                        try
                        {
                            const std::string s_address = register_obj["address"].asString();
                            const uint32_t address = std::stoul(s_address, nullptr, 0);
                            const uint32_t value = register_obj["value"].asUInt();
                            const uint8_t start = register_obj["start"].asUInt();
                            const uint8_t n = register_obj["n"].asUInt();
                            const uint32_t register_before = ReadRegister(address, "Before");

                            SetRegisterSpecificBits(address, start, start + n - 1, value, s_address.c_str());

                            const uint32_t register_after = ReadRegister(address, "After");

                            std::cout << "Setting register: '" << s_address << "' 0x" << std::hex << address << std::dec << std::endl;
                            std::cout << "Bits: [" << (int)start << ":" << (int)start + n - 1 << "]" << std::endl;
                            std::cout << "Value: " << value << std::endl;
                            try
                            {
                                std::cout << "Note: " << register_obj["note"].asString() << std::endl;
                            } catch (...) {}
                            std::cout << "Before: 0x" << std::hex << (int)register_before << std::dec << std::endl;
                            std::cout << " After: 0x" << std::hex << (int)register_after << std::dec << std::endl;
                        }
                        catch (...) { }
                    }
                    else if (register_obj.isMember("address")
                             && register_obj.isMember("value")
                             && !register_obj.isMember("start")
                             && !register_obj.isMember("n"))
                    {
                        try
                        {
                            const std::string s_address = register_obj["address"].asString();
                            const uint32_t address = std::stoul(s_address, nullptr, 0);
                            const uint32_t value = register_obj["value"].asUInt();

                            std::cout << "Setting register: '" << s_address << "' 0x" << std::hex << address << std::dec << std::endl;
                            std::cout << "Value: " << value << std::endl;
                            try
                            {
                                std::cout << "Note: " << register_obj["note"].asString() << std::endl;
                            } catch (...) {}

                            CAEN_DGTZ_WriteRegister(handle, address, value);
                        }
                        catch (...) { }
                    }
                }
            }
        }
    }

    // digital traces settings
    if (boardInfo.dppVersion == 3) {
        switch (boardInfo.vers) {
        case 751: {
            if (showGates == 1) {
                SetRegisterSpecificBits(0x8000, 23, 24, 3);
            } else {
                SetRegisterSpecificBits(0x8000, 23, 24, 0);
            }
        } break;
        case 725: {
            if (showGates == 1) {
                SetRegisterSpecificBits(0x8000, 23, 28, 0);
            }
        } break;
        case 730: {
            if (showGates == 1) {
                SetRegisterSpecificBits(0x8000, 23, 28, 0);
            }
        } break;
        case 720: {
            if (showGates == 1) {
                SetRegisterSpecificBits(0x8000, 23, 25, 0);
            }
        } break;
        default: {
        }
        } // end switch
        // enables/disable gates
    }

    //DumpRegister(0xEF1C);
}
