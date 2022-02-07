/*!
 * \brief Definition of ADQ14 streaming header struct, from the examples of ADQAPI r40946
 */

#ifndef STREAMING_HEADER_H
#define STREAMING_HEADER_H

#include <stdint.h>

typedef struct
{
    uint8_t  RecordStatus;  // Record status byte
    uint8_t  UserID;        // UserID byte
    uint8_t  Channel;       // Channel byte
    uint8_t  DataFormat;    // Data format byte
    uint32_t SerialNumber;  // Serial number (32 bits)
    uint32_t RecordNumber;  // Record number (32 bits)
    int32_t  SamplePeriod;  // Sample period (32 bits)
    uint64_t Timestamp;     // Timestamp (64 bits)
    int64_t  RecordStart;   // Record start timestamp (64 bits)
    uint32_t RecordLength;  // Record length (32 bits)
    uint16_t MovingAverage; // Moving average (16 bits)
    uint16_t GateCounter;   // Gate counter (16 bits)
} StreamingHeader_t;

#endif
