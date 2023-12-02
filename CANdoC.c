//------------------------------------------------------------------------------
//  TITLE :- CANdoC main routines - CANdoC.c
//  AUTHOR :- Martyn Brown
//  DATE :- 15/12/14
//
//  DESCRIPTION :- 'C' example program to demonstrate using libCANdo.so to
//  interface to the CANdo device.
//
//  UPDATES :-
//  08/05/14 Created
//  28/08/14 CANdoC v4.1
//  02/09/14 1) CANdoC v4.2
//           2) GetKey(...) function added
//           3) CANdoPID(...) function added
//           4) Date of manuf. status added
//  15/12/14 Modified to load libCANdo.so dynamically
//
//  LICENSE :-
//  The SDK (Software Development Kit) provided for use with the CANdo device
//  is issued as FREE software, meaning that it is free for personal,
//  educational & commercial use, without restriction or time limit. The
//  software is supplied "as is", with no implied warranties or guarantees.
//
//  (c) 2014 Netronics Ltd. All rights reserved.
//------------------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <dlfcn.h>
#include "CANdoImport.h"
#include "CANdoC.h"
#include "milcan.h"
//------------------------------------------------------------------------------
// GLOBALS
//------------------------------------------------------------------------------
// Library handle
#ifdef _WIN32
  HINSTANCE DLLHandle = NULL;
#define GET_ADDRESS GetProcAddress
#elif __unix
  void * DLLHandle = NULL;
#define GET_ADDRESS dlsym
#endif
// Pointers to functions exported by library
PCANdoGetPID CANdoGetPID;
PCANdoGetDevices CANdoGetDevices;
PCANdoOpen CANdoOpen;
PCANdoOpenDevice CANdoOpenDevice;
PCANdoClose CANdoClose;
PCANdoFlushBuffers CANdoFlushBuffers;
PCANdoSetBaudRate CANdoSetBaudRate;
PCANdoSetMode CANdoSetMode;
PCANdoSetFilters CANdoSetFilters;
PCANdoSetState CANdoSetState;
PCANdoReceive CANdoReceive;
PCANdoTransmit CANdoTransmit;
PCANdoRequestStatus CANdoRequestStatus;
PCANdoRequestDateStatus CANdoRequestDateStatus;
PCANdoRequestBusLoadStatus CANdoRequestBusLoadStatus;
PCANdoRequestSetupStatus CANdoRequestSetupStatus;
PCANdoRequestAnalogInputStatus CANdoRequestAnalogInputStatus;
PCANdoClearStatus CANdoClearStatus;
PCANdoGetVersion CANdoGetVersion;
PCANdoAnalogStoreRead CANdoAnalogStoreRead;
PCANdoAnalogStoreWrite CANdoAnalogStoreWrite;
PCANdoAnalogStoreClear CANdoAnalogStoreClear;
PCANdoTransmitStoreRead CANdoTransmitStoreRead;
PCANdoTransmitStoreWrite CANdoTransmitStoreWrite;
PCANdoTransmitStoreClear CANdoTransmitStoreClear;

TCANdoUSB CANdoUSB;  // Store for parameters relating to connected CANdo
TCANdoCANBuffer CANdoCANBuffer;  // Cyclic store for CAN messages collected from CANdo
TCANdoStatus CANdoStatus;  // Store for status message collected from CANdo

unsigned char RunState;  // CANdo run state
unsigned int DeviceType;  // Type of H/W connected
unsigned char BusLoadEnableFlag;  // CAN bus load enable

TCANdoUSB* CANdoUSBStatus()
{
  return &CANdoUSB;
}

void CANdoCloseAndFinalise() {
  if(CANdoUSB.OpenFlag) {
    CANdoStop();
    CANdoClose(&CANdoUSB);
  }
  CANdoFinalise();
}

//------------------------------------------------------------------------------
// CANdoInitialise2
//
// Load the CANdo.dll & map the functions.
//
// Returns -
//    FALSE = Error loading DLL or mapping functions
//    TRUE = DLL loaded & functions all mapped
//------------------------------------------------------------------------------
unsigned char CANdoInitialise2(void)
{
  unsigned char Status;

  if (DLLHandle == NULL)
#ifdef _WIN32
    DLLHandle = LoadLibrary("CANdo.dll");
#elif __unix
    DLLHandle = dlopen("libCANdo.so", RTLD_LAZY);
#endif

  if (DLLHandle != NULL)
  {
    // DLL loaded, so map functions
    if (CANdoMapFunctionPointers())
    {
      // One or more functions not mapped correctly, so deallocate all resources
      CANdoFinalise();
      Status = FALSE;  // Error
    }
    else
      Status = TRUE;  // OK
  }
  else
    Status = FALSE;  // Error

  return Status;
}
//--------------------------------------------------------------------------
// CANdoFinalise
//
// Unmap the functions & unload the CANdo.dll.
//
// Returns -
//    Nothing
//--------------------------------------------------------------------------
void CANdoFinalise(void)
{
  // Unmap the function pointers to the DLL
  CANdoUnmapFunctionPointers();
  // Unload the library
#ifdef _WIN32
  FreeLibrary((HMODULE)DLLHandle);
#elif __unix
  if (DLLHandle != NULL)
    dlclose((void *)DLLHandle);
#endif
}
//--------------------------------------------------------------------------
// CANdoMapFunctionPointers
//
// Map function pointers to functions within CANdo.dll.
//
// Returns -
//    0 = OK
//    >0 = At least one function not mapped to DLL
//--------------------------------------------------------------------------
int CANdoMapFunctionPointers(void)
{
  int MapState;
  printf("CANdoMapFunctionPointers()\n");

  if (DLLHandle != NULL)
  {
    MapState = 0x00000000;

    CANdoGetPID = (PCANdoGetPID)GET_ADDRESS(DLLHandle, "CANdoGetPID");
    if (CANdoGetPID == NULL)
      MapState = 0x00000001;  // Function not mapped

    CANdoGetDevices = (PCANdoGetDevices)GET_ADDRESS(DLLHandle, "CANdoGetDevices");
    if (CANdoGetDevices == NULL)
      MapState |= 0x00000002;  // Function not mapped

    CANdoOpen = (PCANdoOpen)GET_ADDRESS(DLLHandle, "CANdoOpen");
    if (CANdoOpen == NULL)
      MapState |= 0x00000004;  // Function not mapped

    CANdoOpenDevice = (PCANdoOpenDevice)GET_ADDRESS(DLLHandle, "CANdoOpenDevice");
    if (CANdoOpenDevice == NULL)
      MapState |= 0x00000008;  // Function not mapped

    CANdoClose = (PCANdoClose)GET_ADDRESS(DLLHandle, "CANdoClose");
    if (CANdoClose == NULL)
      MapState |= 0x00000010;  // Function not mapped

    CANdoFlushBuffers = (PCANdoFlushBuffers)GET_ADDRESS(DLLHandle, "CANdoFlushBuffers");
    if (CANdoFlushBuffers == NULL)
      MapState |= 0x00000020;  // Function not mapped

    CANdoSetBaudRate = (PCANdoSetBaudRate)GET_ADDRESS(DLLHandle, "CANdoSetBaudRate");
    if (CANdoSetBaudRate == NULL)
      MapState |= 0x00000040;  // Function not mapped

    CANdoSetMode = (PCANdoSetMode)GET_ADDRESS(DLLHandle, "CANdoSetMode");
    if (CANdoSetMode == NULL)
      MapState |= 0x00000080;  // Function not mapped

    CANdoSetFilters = (PCANdoSetFilters)GET_ADDRESS(DLLHandle, "CANdoSetFilters");
    if (CANdoSetFilters == NULL)
      MapState |= 0x00000100;  // Function not mapped

    CANdoSetState = (PCANdoSetState)GET_ADDRESS(DLLHandle, "CANdoSetState");
    if (CANdoSetState == NULL)
      MapState |= 0x00000200;  // Function not mapped

    CANdoReceive = (PCANdoReceive)GET_ADDRESS(DLLHandle, "CANdoReceive");
    if (CANdoReceive == NULL)
      MapState |= 0x00000400;  // Function not mapped

    CANdoTransmit = (PCANdoTransmit)GET_ADDRESS(DLLHandle, "CANdoTransmit");
    if (CANdoTransmit == NULL)
      MapState |= 0x00000800;  // Function not mapped

    CANdoRequestStatus = (PCANdoRequestStatus)GET_ADDRESS(DLLHandle, "CANdoRequestStatus");
    if (CANdoRequestStatus == NULL)
      MapState |= 0x00001000;  // Function not mapped

    CANdoRequestDateStatus = (PCANdoRequestDateStatus)GET_ADDRESS(DLLHandle, "CANdoRequestDateStatus");
    if (CANdoRequestDateStatus == NULL)
      MapState |= 0x00002000;  // Function not mapped

    CANdoRequestBusLoadStatus = (PCANdoRequestBusLoadStatus)GET_ADDRESS(DLLHandle, "CANdoRequestBusLoadStatus");
    if (CANdoRequestBusLoadStatus == NULL)
      MapState |= 0x00004000;  // Function not mapped

    CANdoRequestSetupStatus = (PCANdoRequestSetupStatus)GET_ADDRESS(DLLHandle, "CANdoRequestSetupStatus");
    if (CANdoRequestSetupStatus == NULL)
      MapState |= 0x00008000;  // Function not mapped

    CANdoRequestAnalogInputStatus = (PCANdoRequestAnalogInputStatus)GET_ADDRESS(DLLHandle, "CANdoRequestAnalogInputStatus");
    if (CANdoRequestAnalogInputStatus == NULL)
      MapState |= 0x00010000;  // Function not mapped

    CANdoClearStatus = (PCANdoClearStatus)GET_ADDRESS(DLLHandle, "CANdoClearStatus");
    if (CANdoClearStatus == NULL)
      MapState |= 0x00020000;  // Function not mapped

    CANdoGetVersion = (PCANdoGetVersion)GET_ADDRESS(DLLHandle, "CANdoGetVersion");
    if (CANdoGetVersion == NULL)
      MapState |= 0x00040000;  // Function not mapped

    CANdoAnalogStoreRead = (PCANdoAnalogStoreRead)GET_ADDRESS(DLLHandle, "CANdoAnalogStoreRead");
    if (CANdoAnalogStoreRead == NULL)
      MapState |= 0x00080000;  // Function not mapped

    CANdoAnalogStoreWrite = (PCANdoAnalogStoreWrite)GET_ADDRESS(DLLHandle, "CANdoAnalogStoreWrite");
    if (CANdoAnalogStoreWrite == NULL)
      MapState |= 0x00100000;  // Function not mapped

    CANdoAnalogStoreClear = (PCANdoAnalogStoreClear)GET_ADDRESS(DLLHandle, "CANdoAnalogStoreClear");
    if (CANdoAnalogStoreClear == NULL)
      MapState |= 0x00200000;  // Function not mapped

    CANdoTransmitStoreRead = (PCANdoTransmitStoreRead)GET_ADDRESS(DLLHandle, "CANdoTransmitStoreRead");
    if (CANdoTransmitStoreRead == NULL)
      MapState |= 0x00400000;  // Function not mapped

    CANdoTransmitStoreWrite = (PCANdoTransmitStoreWrite)GET_ADDRESS(DLLHandle, "CANdoTransmitStoreWrite");
    if (CANdoTransmitStoreWrite == NULL)
      MapState |= 0x00800000;  // Function not mapped

    CANdoTransmitStoreClear = (PCANdoTransmitStoreClear)GET_ADDRESS(DLLHandle, "CANdoTransmitStoreClear");
    if (CANdoTransmitStoreClear == NULL)
      MapState |= 0x01000000;  // Function not mapped
  }
  else
    MapState = 0x7FFFFFFF;

  return MapState;
}
//--------------------------------------------------------------------------
// CANdoUnmapFunctionPointers
//
// Unmap function pointers to functions within CANdo.dll.
//
// Returns -
//    Nothing
//--------------------------------------------------------------------------
void CANdoUnmapFunctionPointers(void)
{
  CANdoGetPID = NULL;
  CANdoGetDevices = NULL;
  CANdoOpen = NULL;
  CANdoOpenDevice = NULL;
  CANdoClose = NULL;
  CANdoFlushBuffers = NULL;
  CANdoSetBaudRate = NULL;
  CANdoSetMode = NULL;
  CANdoSetFilters = NULL;
  CANdoSetState = NULL;
  CANdoReceive = NULL;
  CANdoTransmit = NULL;
  CANdoRequestStatus = NULL;
  CANdoRequestDateStatus = NULL;
  CANdoRequestBusLoadStatus = NULL;
  CANdoRequestSetupStatus = NULL;
  CANdoRequestAnalogInputStatus = NULL;
  CANdoClearStatus = NULL;
  CANdoGetVersion = NULL;
  CANdoAnalogStoreRead = NULL;
  CANdoAnalogStoreWrite = NULL;
  CANdoAnalogStoreClear = NULL;
  CANdoTransmitStoreRead = NULL;
  CANdoTransmitStoreWrite = NULL;
  CANdoTransmitStoreClear = NULL;
}
//--------------------------------------------------------------------------
// CANdoConnect
//
// This function scans for CANdo devices & connects to the 1st device found.
// If more than one device is found, then a list of all the devices is also
// displayed.
//
// Returns -
//    Nothing
//--------------------------------------------------------------------------
int CANdoConnect(u_int16_t deviceNum)
{
  unsigned int NoOfDevices, Status, DeviceNo;
  TCANdoDevice CANdoDevices[MAX_NO_OF_DEVICES];
  TCANdoDeviceString Description;

  DeviceType = CANDO_TYPE_UNKNOWN;  // Device type unknown
  NoOfDevices = MAX_NO_OF_DEVICES;  // Max. no. of devices to enumerate
  CANdoVersion();
  Status = CANdoGetDevices(CANdoDevices, &NoOfDevices);  // Get a list of the CANdo devices connected
  printf("Number of CANdo devices available: %u\n", NoOfDevices);
  
  if (Status == CANDO_SUCCESS)
  {
    if(deviceNum >= NoOfDevices) {
      return CANDO_CONNECT_OUT_OF_RANGE;
    }
    printf("%d CANdo Found:\n", NoOfDevices);
    for (DeviceNo = 0; DeviceNo < NoOfDevices; DeviceNo++)
    {
      if (CANdoDevices[DeviceNo].HardwareType == CANDO_TYPE_CANDO)
        printf("%d = CANdo S/N %s\n", DeviceNo + 1, CANdoDevices[DeviceNo].SerialNo);
      else
      if (CANdoDevices[DeviceNo].HardwareType == CANDO_TYPE_CANDOISO)
        printf("%d = CANdoISO S/N %s\n", DeviceNo + 1, CANdoDevices[DeviceNo].SerialNo);
      else
      if (CANdoDevices[DeviceNo].HardwareType == CANDO_TYPE_CANDO_AUTO)
        printf("%d = CANdo AUTO S/N %s\n", DeviceNo + 1, CANdoDevices[DeviceNo].SerialNo);
      else
        printf("%d = Type Unknown?\n", DeviceNo + 1);
    }

		if (NoOfDevices > 0)
		{
			// At least 1 device found, so connect to 1st device
      // if (CANdoOpen(&CANdoUSB) == CANDO_SUCCESS)
      if (CANdoOpenDevice(&CANdoUSB, &CANdoDevices[deviceNum]) == CANDO_SUCCESS)
      {
			  // Connection open
        DeviceType = CANdoDevices[deviceNum].HardwareType;
			  strcpy((char *)Description, (char *)CANdoUSB.Description);
			  strcat((char *)Description, " S/N ");
			  strcat((char *)Description, (char *)CANdoUSB.SerialNo);
			  printf("%s connected\n", (char *)Description);
			  return CANDO_CONNECT_OK;
      }
		}
  }
  else if (Status == CANDO_USB_DLL_ERROR)
  {
    printf("CANdo USB DLL not found\n");
    return CANDO_CONNECT_DLL_ERROR;
  }
  else if (Status == CANDO_USB_DRIVER_ERROR)
  {
    printf("CANdo driver not found\n");
    return CANDO_CONNECT_USB_DRIVER_ERROR;
  }
  else
  {
    printf("CANdo not found\n");
    return CANDO_CONNECT_NOT_FOUND;
  }

  return CANDO_CONNECT_FAIL;
}
//--------------------------------------------------------------------------
// CANdoStart
//
// Configure CANdo & set to run state.
//
// Returns -
//    TRUE is running, else FALSE
//--------------------------------------------------------------------------
int CANdoStart(unsigned char baudrate)
{
  unsigned char SJW, BRP, PHSEG1, PHSEG2, PROPSEG, SAM;
  // Note:
  // Bit Rate = 20000000 / 2 * (BRP + 1) * (4 + PROPSEG _ PHSEG1 + PHSEG2)
  // Sample Point = (3 + PROPSEG + PHSEG1) * 100 / (4 + PROPSEG + PHSEG1 + PHSEG2)
  // Rules:
  // PROPSEG + PHSEG1 + 1 >= PHSEG2
  // PROPSEG + PHSEG1 + PHSEG2 >= 4
  // PHSEG2 >= SJW
  switch(baudrate) {
    case 0: // 250k - Sample point must be 87.5% or later
      BRP = 3;
      PROPSEG = 2;
      PHSEG1 = 4;
      PHSEG2 = 0;
      // Sample point = 90%
      break;
    case 1: // 500k - Sample point must be 87.5% or later
      BRP = 1;
      PROPSEG = 2;
      PHSEG1 = 4;
      PHSEG2 = 0;
      // Sample point = 90%
      break;
    case 2: // 1M - Sample point must be 75% or higher, preferably 80%
      BRP = 0;
      PROPSEG = 1;
      PHSEG1 = 4;
      PHSEG2 = 1;
      // Sample point = 80%
      break;
  }
  // These are defined in MilCAN A Spec Section 2.4.2
  SJW = 0;  // Sync Jump Width (0-3). 0 = 1 jump bit ... 3 = 4 jump bits.
  SAM = 1;  // Samples per bit (0-1). 0 = 1 sample per bit, 1 = three samples per bit. 

  RunState = FALSE;  // CANdo stopped
  if (CANdoUSB.OpenFlag)
  {
    // if (CANdoSetBaudRate(&CANdoUSB, 0, 1, 7, 7, 2, 0) == CANDO_SUCCESS)  // Set baud rate to 250k
    if (CANdoSetBaudRate(&CANdoUSB, SJW, BRP, PHSEG1, PHSEG2, PROPSEG, SAM) == CANDO_SUCCESS)
    {
      usleep(100000);  // Wait 100ms to allow CANdo to store baud rate in EEPROM, in case modified
      // Set mode to 'Normal'
      if (CANdoSetMode(&CANdoUSB, CANDO_NORMAL_MODE) == CANDO_SUCCESS)
      {
        usleep(10000);  // Wait 10ms to allow CANdo to store mode in EEPROM, in case modified
        // Set filters to accept all messages
        if (CANdoSetFilters(&CANdoUSB,
          0,
          CANDO_ID_29_BIT, 0,
          CANDO_ID_11_BIT, 0,
          0,
          CANDO_ID_29_BIT, 0,
          CANDO_ID_11_BIT, 0,
          CANDO_ID_29_BIT, 0,
          CANDO_ID_11_BIT, 0) == CANDO_SUCCESS)
          {
            usleep(10000);  // Wait 10ms to allow filters to be configured in CAN module
            // Flush USB buffers
            if (CANdoFlushBuffers(&CANdoUSB) == CANDO_SUCCESS)
              // Set CANdo state to run
              if (CANdoSetState(&CANdoUSB, CANDO_RUN) == CANDO_SUCCESS)
                RunState = TRUE;  // Running
          }
      }
    }
  }

  long br = 20000000 / (2*(BRP + 1)*(4 + PROPSEG + PHSEG1 + PHSEG2));
  int sp = (3 + PROPSEG + PHSEG1) * 100 / (4 + PROPSEG + PHSEG1 + PHSEG2);
  if (RunState)
    printf("CANdo started at %li (sample point %i).\n", br, sp);
  return RunState;
}
//--------------------------------------------------------------------------
// CANdoStop
//
// Stop CANdo.
//
// Returns -
//    Nothing
//--------------------------------------------------------------------------
void CANdoStop(void)
{
  if (RunState)
  {
    if (CANdoUSB.OpenFlag)
      if (CANdoSetState(&CANdoUSB, CANDO_STOP) == CANDO_SUCCESS)
        RunState = FALSE;  // Stopped

    if (!RunState)
      printf("CANdo stopped\n");
  }
}
//--------------------------------------------------------------------------
// CANdoGetStatus
//
// Request the CANdo status.
//
// Returns -
//    Nothing
//--------------------------------------------------------------------------
void CANdoGetStatus(unsigned char StatusType)
{
  // Send status request to CANdo
  switch (StatusType)
  {
    case CANDO_DEVICE_STATUS : CANdoRequestStatus(&CANdoUSB); break;
    case CANDO_DATE_STATUS : CANdoRequestDateStatus(&CANdoUSB); break;
    case CANDO_BUS_LOAD_STATUS : CANdoRequestBusLoadStatus(&CANdoUSB); break;
  }
}
//--------------------------------------------------------------------------
// CANdoPID
//
// Display the USB PID for the connected device.
//
// Returns -
//    Nothing
//--------------------------------------------------------------------------
void CANdoPID(void)
{
  TCANdoDeviceString PID;

  if (CANdoGetPID(CANdoUSB.No, PID) == CANDO_SUCCESS)
    printf("\n CANdo USB PID 0x%s\n >", (char *)PID);
  else
    printf("\n Error reading USB PID\n >");
}
//--------------------------------------------------------------------------
// CANdoVersion
//
// Display the versions of the CANdo API, USB DLL & driver.
//
// Returns -
//    Nothing
//--------------------------------------------------------------------------
void CANdoVersion(void)
{
  unsigned int APIVersion, DLLVersion, DriverVersion;

  CANdoGetVersion(&APIVersion, &DLLVersion, &DriverVersion);

  printf(" CANdo API DLL v%.1f\n CANdo USB DLL v%.1f\n CANdo driver v%.1f\n",
    (float)APIVersion / 10, (float)DLLVersion / 10, (float)DriverVersion / 10);
}

#define CAN_ERR_TX_TIMEOUT 0x00000001U   // TX timeout (by netdevice driver)
#define CAN_ERR_LOSTARB	   0x00000002U   // lost arbitration    / data[0]   
#define CAN_ERR_CRTL	   0x00000004U   // controller problems / data[1]   
#define CAN_ERR_PROT	   0x00000008U   // protocol violations / data[2..3]
#define CAN_ERR_TRX		   0x00000010U   // transceiver status  / data[4]
#define CAN_ERR_ACK		   0x00000020U   // received no ACK on transmission
#define CAN_ERR_BUSOFF	   0x00000040U   // bus off
#define CAN_ERR_BUSERROR   0x00000080U   // bus error (may flood!)
#define CAN_ERR_RESTARTED  0x00000100U   // controller restarted
#define CAN_ERR_CNT  	   0x00000200U   // error counts (data[6] = Tx error counter, data[7] = Rx error counter)


// Fills the Rx buffer
int CANdoRx() {
  if (CANdoReceive(&CANdoUSB, &CANdoCANBuffer, &CANdoStatus) != CANDO_SUCCESS)
    return FALSE;
  return TRUE;
  switch(CANdoReceive(&CANdoUSB, &CANdoCANBuffer, &CANdoStatus)) {
    case CANDO_SUCCESS:
      return MILCAN_OK;
    case CANDO_CONNECTION_CLOSED:
      return MILCAN_ERROR_CONN_CLOSED;
    default:
    case CANDO_READ_ERROR:
    case CANDO_BUFFER_OVERFLOW:
    case CANDO_RX_OVERRUN:
    case CANDO_RX_TYPE_UNKNOWN:
    case CANDO_RX_CRC_ERROR:
    case CANDO_RX_DECODE_ERROR:
      return MILCAN_ERROR;
  }
}

// Empties the Rx buffer.
int CANdoReadRxQueue(struct can_frame *frame) {
  if((CANdoCANBuffer.ReadIndex != CANdoCANBuffer.WriteIndex) || CANdoCANBuffer.FullFlag) {
    frame->len = CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].DLC;
    frame->data[0] = CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].Data[0];
    frame->data[1] = CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].Data[1];
    frame->data[2] = CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].Data[2];
    frame->data[3] = CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].Data[3];
    frame->data[4] = CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].Data[4];
    frame->data[5] = CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].Data[5];
    frame->data[6] = CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].Data[6];
    frame->data[7] = CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].Data[7];
    frame->can_id = CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].ID;
    if(CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].IDE) {
      frame->can_id |= CAN_EFF_FLAG;
    }
    if(CANdoCANBuffer.CANMessage[CANdoCANBuffer.ReadIndex].RTR) {
      frame->can_id |= CAN_RTR_FLAG;
    }

    // Move read pointer onto next slot in cyclic buffer
    if ((CANdoCANBuffer.ReadIndex + 1) < CANDO_CAN_BUFFER_LENGTH)
      CANdoCANBuffer.ReadIndex++;  // Increment index onto next free slot
    else
      CANdoCANBuffer.ReadIndex = 0;  // Wrap back to start

    CANdoCANBuffer.FullFlag = FALSE;  // Clear flag as buffer is not full
    return TRUE;
  }
  return FALSE;
}

int CANdoTx(unsigned char idExtended, unsigned int id, unsigned char dlc, unsigned char * data)
{
  // Transmit frame of data
  if (CANdoTransmit(&CANdoUSB, idExtended, id, CANDO_DATA_FRAME, dlc, data, 0, 0) == CANDO_SUCCESS)
    return TRUE;
  return FALSE;
}
