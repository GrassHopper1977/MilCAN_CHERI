//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
// THIS HEADER FILE CONTAINS THE FUNCTION IMPORTS AND CONSTANTS FOR THE
// FUNCTIONS EXPORTED BY THE CANdo API LIBRARY -
// Windows CANdo.dll v4.1
// Linux libCANdo.so v2.0
//--------------------------------------------------------------------------
//--------------------------------------------------------------------------
//  TITLE :- CANdo.dll/libCANdo.so import unit - CANdoImport.h
//  AUTHOR :- Martyn Brown
//  DATE :- 15/12/14
//
//  DESCRIPTION :- 'C' header file for interfacing to CANdo API library.
//
//  UPDATES :-
//  30/01/06 Created
//  29/07/10 1) Function return values updated
//           2) CANdoGetVersion function added
//  10/01/11 1) CANdoClose prototype return type modified
//           2) CANDO_INVALID_HANDLE return value added
//  25/02/11 1) CANdo H/W type constants added
//           2) CANdoDevice typedef added
//           3) CANdoGetDevices function added
//           4) CANdoOpenDevice function added
//           5) CANdo constants added
//           6) CANDO_MAX_CAN_BUFFER_LENGTH renamed to CANDO_CAN_BUFFER_LENGTH
//           7) CANDO_CLOSED return value renamed CANDO_CONNECTION_CLOSED
//  03/11/11 1) Updated for CANdo API v3.0
//           2) CANdoRequestDateStatus function added
//           3) CANdoRequestBusLoadStatus function added
//           4) CANdoClearStatus function added
//           5) CANdoGetPID function added
//           6) CANdo status type constants added
//           7) CANdo USB PID constants added
//  29/08/13 1) Updated for CANdo API v4.0
//           2) CANdo AUTO support added
//           3) CANdoRequestSetupStatus function added
//           4) CANdoRequestAnalogInputStatus function added
//           5) CANdoAnalogStoreRead function added
//           6) CANdoAnalogStoreWrite function added
//           7) CANdoAnalogStoreClear function added
//           8) CANdoTransmitStoreRead function added
//           9) CANdoTransmitStoreWrite function added
//           10) CANdoTransmitStoreClear function added
//  15/12/14 Windows/Linux multi-platform support added
//
//  LICENSE :-
//  The SDK (Software Development Kit) provided for use with the CANdo device
//  is issued as FREE software, meaning that it is free for personal,
//  educational & commercial use, without restriction or time limit. The
//  software is supplied "as is", with no implied warranties or guarantees.
//
//  (c) 2006-14 Netronics Ltd. All rights reserved.
//------------------------------------------------------------------------------
#ifndef CANDO_IMPORT_H
#define CANDO_IMPORT_H

#ifdef _WIN32
#include <windows.h>
#define CALL_TYPE __stdcall
#elif __unix
#define CALL_TYPE
#endif
//------------------------------------------------------------------------------
// DEFINES
//------------------------------------------------------------------------------
// CANdo constants
#define CANDO_CLOSED 0  // USB channel closed
#define CANDO_OPEN 1  // USB channel open
#define CANDO_STOP 0  // Stop Rx/Tx of CAN messages
#define CANDO_RUN 1  // Start Rx/Tx of CAN messages
#define CANDO_NORMAL_MODE 0  // Rx/Tx CAN mode
#define CANDO_LISTEN_ONLY_MODE 1  // Rx only mode, no ACKs
#define CANDO_LOOPBACK_MODE 2  // Tx internally looped back to Rx
#define CANDO_CLK_FREQ 20000  // CANdo clk. freq. in kHz for baud rate calc.
#define CANDO_CLK_FREQ_HIGH 40000  // CANdoISO & CANdo AUTO clk. freq. in kHz for baud rate calc.
#define CANDO_BRP_ENHANCED_OFFSET 63  // BRP enhanced baud rate setting offset (CANdoISO & CANdo AUTO only)
// CANdo AUTO constants
#define CANDO_AUTO_V1_INPUT 1  // V1 analogue I/P
#define CANDO_AUTO_V2_INPUT 2  // V2 analogue I/P
#define CANDO_AUTO_MAX_NO_OF_TX_ITEMS 10  // Max. no. of items in CAN Transmit store
// CAN message constants
#define CANDO_ID_11_BIT 0  // Standard 11 bit ID
#define CANDO_ID_29_BIT 1  // Extended 29 bit ID
#define CANDO_DATA_FRAME 0  // CAN data frame
#define CANDO_REMOTE_FRAME 1  // CAN remote frame
// CAN receive cyclic buffer size
#define CANDO_CAN_BUFFER_LENGTH 2048
// CANdo string type length
#define CANDO_STRING_LENGTH 256
// CANdo H/W types
#define CANDO_TYPE_ANY 0x0000  // Any H/W type
#define CANDO_TYPE_CANDO 0x0001  // CANdo H/W type
#define CANDO_TYPE_CANDOISO 0x0002  // CANdoISO H/W type
#define CANDO_TYPE_CANDO_AUTO 0x0003  // CANdo AUTO H/W type
#define CANDO_TYPE_UNKNOWN 0x8000  // Unknown H/W type
// CANdo status types
#define CANDO_NO_STATUS 0  // No new status received
#define CANDO_DEVICE_STATUS 1  // Device status received
#define CANDO_DATE_STATUS 2  // Date status received
#define CANDO_BUS_LOAD_STATUS 3  // Bus load status received
#define CANDO_SETUP_STATUS 4  // CAN setup status received
#define CANDO_ANALOG_INPUT_STATUS 5  // Analogue I/P status received
// CANdo USB PIDs
#define CANDO_PID "8095"  // CANdo PID
#define CANDOISO_PID "8660"  // CANdoISO PID
#define CANDO_AUTO_PID "889B"  // CANdo AUTO PID
// Function return values
#define CANDO_SUCCESS 0x0000  // All OK
#define CANDO_USB_DLL_ERROR 0x0001  // CANdo USB DLL error
#define CANDO_USB_DRIVER_ERROR 0x0002  // CANdo USB driver error
#define CANDO_NOT_FOUND 0x0004  // CANdo not found
#define CANDO_IO_FAILED 0x0008  // Failed to initialise USB UART parameters
#define CANDO_CONNECTION_CLOSED 0x0010  // No CANdo channel open
#define CANDO_READ_ERROR 0x0020  // USB UART read error
#define CANDO_WRITE_ERROR 0x0040  // USB UART write error
#define CANDO_WRITE_INCOMPLETE 0x0080  // Not all requested bytes written to CANdo
#define CANDO_BUFFER_OVERFLOW 0x0100  // Overflow in cyclic buffer
#define CANDO_RX_OVERRUN 0x0200  // Message received greater than max. message size
#define CANDO_RX_TYPE_UNKNOWN 0x0400  // Unknown message type received
#define CANDO_RX_CRC_ERROR 0x0800  // CRC mismatch
#define CANDO_RX_DECODE_ERROR 0x1000  // Error decoding message
#define CANDO_INVALID_HANDLE 0x2000  // Invalid device handle
#define CANDO_ERROR 0x8000  // Non specific error
//------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C"
{
#endif
//------------------------------------------------------------------------------
// TYPEDEFS
//------------------------------------------------------------------------------
typedef unsigned char TCANdoDeviceString[CANDO_STRING_LENGTH];  // CANdo string type

// Structure type used to store device identification for CANdo
typedef struct TCANdoDevice
{
  int HardwareType;  // H/W type of this CANdo
  TCANdoDeviceString SerialNo;  // USB S/N for this CANdo
} TCANdoDevice;

typedef TCANdoDevice * PCANdoDevice;  // Pointer type to TCANdoDevice

// Structure type used to store info. relating to connected CANdo
typedef struct TCANdoUSB
{
  int TotalNo;  // Total no. of CANdo on USB bus
  int No;  // No. of this CANdo
  unsigned char OpenFlag;  // USB communications channel state
  TCANdoDeviceString Description;  // USB descriptor string for CANdo
  TCANdoDeviceString SerialNo;  // USB S/N for this CANdo
#ifdef _WIN32
  HANDLE Handle;  // Handle to connected CANdo
#elif __unix
  void * Handle;  // Handle to connected CANdo
#endif
} TCANdoUSB;

typedef TCANdoUSB * PCANdoUSB;  // Pointer type to TCANdoUSB

// Structure type used to store a CAN message
typedef struct TCANdoCAN
{
  unsigned char IDE;
  unsigned char RTR;
  unsigned int ID;
  unsigned char DLC;
  unsigned char Data[8];
  unsigned char BusState;
  unsigned int TimeStamp;
} TCANdoCAN;

// Structure type used as a cyclic buffer to store decoded CAN messages received from CANdo
typedef struct TCANdoCANBuffer
{
  TCANdoCAN CANMessage[CANDO_CAN_BUFFER_LENGTH];
  int WriteIndex;
  int ReadIndex;
  unsigned char FullFlag;
} TCANdoCANBuffer;

typedef TCANdoCANBuffer * PCANdoCANBuffer;  // Pointer type to TCANdoCANBuffer

// Structure type used to store status information received from CANdo
typedef struct TCANdoStatus
{
  unsigned char HardwareVersion;
  unsigned char SoftwareVersion;
  unsigned char Status;
  unsigned char BusState;
  unsigned int TimeStamp;
  unsigned char NewFlag;
} TCANdoStatus;

typedef TCANdoStatus * PCANdoStatus;  // Pointer type to TCANdoStatus

// Function pointers
typedef int CALL_TYPE (* PCANdoGetPID)(unsigned int CANdoNo, const TCANdoDeviceString PID);
typedef int CALL_TYPE (* PCANdoGetDevices)(const TCANdoDevice CANdoDevices[], unsigned int * NoOfDevices);
typedef int CALL_TYPE (* PCANdoOpen)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoOpenDevice)(const PCANdoUSB CANdoUSBPointer, const PCANdoDevice CANdoDevicePointer);
typedef int CALL_TYPE (* PCANdoClose)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoFlushBuffers)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoSetBaudRate)(const PCANdoUSB CANdoUSBPointer,
  unsigned char SJW, unsigned char BRP, unsigned char PHSEG1,
  unsigned char PHSEG2, unsigned char PROPSEG, unsigned char SAM);
typedef int CALL_TYPE (* PCANdoSetMode)(const PCANdoUSB CANdoUSBPointer, unsigned char Mode);
typedef int CALL_TYPE (* PCANdoSetFilters)(const PCANdoUSB CANdoUSBPointer,
  unsigned int Rx1Mask,
  unsigned char Rx1IDE1, unsigned int Rx1Filter1,
  unsigned char Rx1IDE2, unsigned int Rx1Filter2,
  unsigned int Rx2Mask,
  unsigned char Rx2IDE1, unsigned int Rx2Filter1,
  unsigned char Rx2IDE2, unsigned int Rx2Filter2,
  unsigned char Rx2IDE3, unsigned int Rx2Filter3,
  unsigned char Rx2IDE4, unsigned int Rx2Filter4);
typedef int CALL_TYPE (* PCANdoSetState)(const PCANdoUSB CANdoUSBPointer, unsigned char State);
typedef int CALL_TYPE (* PCANdoReceive)(const PCANdoUSB CANdoUSBPointer,
  const PCANdoCANBuffer CANdoCANBufferPointer, const PCANdoStatus CANdoStatusPointer);
typedef int CALL_TYPE (* PCANdoTransmit)(const PCANdoUSB CANdoUSBPointer, unsigned char IDExtended,
  unsigned int ID, unsigned char RTR, unsigned char DLC, const unsigned char * Data,
  unsigned char BufferNo, unsigned char RepeatTime);
typedef int CALL_TYPE (* PCANdoRequestStatus)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoRequestDateStatus)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoRequestBusLoadStatus)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoRequestSetupStatus)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoRequestAnalogInputStatus)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoClearStatus)(const PCANdoUSB CANdoUSBPointer);
typedef void CALL_TYPE (* PCANdoGetVersion)(unsigned int * APIVersionPointer, unsigned int * DLLVersionPointer, unsigned int * DriverVersionPointer);
typedef int CALL_TYPE (* PCANdoAnalogStoreRead)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoAnalogStoreWrite)(const PCANdoUSB CANdoUSBPointer, unsigned char InputNo,
  unsigned char IDExtended, unsigned int ID, unsigned char Start, unsigned char Length,
  double ScalingFactor, double Offset, unsigned char Padding, unsigned char RepeatTime);
typedef int CALL_TYPE (* PCANdoAnalogStoreClear)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoTransmitStoreRead)(const PCANdoUSB CANdoUSBPointer);
typedef int CALL_TYPE (* PCANdoTransmitStoreWrite)(const PCANdoUSB CANdoUSBPointer, unsigned char IDExtended,
  unsigned int ID, unsigned char RTR, unsigned char DLC, const unsigned char * Data, unsigned char RepeatTime);
typedef int CALL_TYPE (* PCANdoTransmitStoreClear)(const PCANdoUSB CANdoUSBPointer);
//------------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#endif
