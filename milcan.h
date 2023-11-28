// milcan.h
#ifndef __MILCAN_H__
#define __MILCAN_H__
#include "can.h"


#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define MILCAN_OK                   0
#define MILCAN_ERROR                -1
#define MILCAN_ERROR_CONN_CLOSED    -2

#define MILCAN_ID_MASK              0x1FFFFFFF
#define MILCAN_ID_PRIORITY_MASK     0x1C000000
#define MILCAN_ID_MILCAN_TYPE       0x02000000
#define MILCAN_ID_MILCAN_REQUEST    0x01000000
#define MILCAN_ID_PRIMARY_MASK      0x00FF0000
#define MILCAN_ID_SECONDARY_MASK    0x0000FF00
#define MILCAN_ID_SOURCE_MASK       0x000000FF

#define MILCAN_ID_PRIORITY_COUNT       8 // There are 8 levels of priority
#define MILCAN_ID_PRIORITY_0         0x00000000  // HRT0 - Highest Priority - Transmit immediately (although, lowest address still wins)
#define MILCAN_ID_PRIORITY_1         0x04000000  // HRT1 - Transmit within 1 PTU of brint triggered
#define MILCAN_ID_PRIORITY_2         0x08000000  // HRT2 - Transmit within 8 PTU of being triggered
#define MILCAN_ID_PRIORITY_3         0x0C000000  // HRT3 - Transmit within 64 PTU of being triggered
#define MILCAN_ID_PRIORITY_4         0x10000000  // SRT1 - Transmit within 8 PTU of being triggered
#define MILCAN_ID_PRIORITY_5         0x14000000  // SRT2 - Transmit within 64 PTU of being triggered
#define MILCAN_ID_PRIORITY_6         0x18000000  // SRT3 - Transmit within 1024 PTU of being triggered
#define MILCAN_ID_PRIORITY_7         0x1C000000  // NRT - Lowest Priority - Transmit whenever there is a space

#define MILCAN_ID_PRIORITY_MIN  0   // The minimum priority value is the highest priority message
#define MILCAN_ID_PRIORITY_MAX  7   // The maximum priority value is the lowest priority message

// The predefined Primary and secondary IDs
#define MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT                 0x00
#define MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_SYNC_FRAME    0x80
#define MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_ENTER_CONFIG  0x81
#define MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_EXIT_CONFIG   0x82
#define MILCAN_ID_PRIMARY_PHYSICALLY_ADDRESSED              0x31
#define MILCAN_ID_PRIMARY_SYSTEM_CONFIG                     0x32
#define MILCAN_ID_PRIMARY_SYSTEM_C_AND_C                    0x3C
#define MILCAN_ID_PRIMARY_MOTION_CONTROL                    0x3E
#define MILCAN_ID_PRIMARY_STA                               0x40
#define MILCAN_ID_PRIMARY_FIRE_CONTROL                      0x42
#define MILCAN_ID_PRIMARY_AUTOMOTIVE                        0x44
#define MILCAN_ID_PRIMARY_NAVIGATION                        0x46
#define MILCAN_ID_PRIMARY_POWER_MANAGEMENT                  0x50
#define MILCAN_ID_PRIMARY_DAS                               0x52
#define MILCAN_ID_PRIMARY_COMMUNICATIONS_BMS                0x54
#define MILCAN_ID_PRIMARY_HVAC_NBC                          0x56
#define MILCAN_ID_PRIMARY_VISION_SENSOR_CONTROL             0x58
#define MILCAN_ID_PRIMARY_GENERIC_MMI_DEVICES               0x5A
#define MILCAN_ID_PRIMARY_FDSS                              0x5C
#define MILCAN_ID_PRIMARY_LIGHTING                          0x5E
#define MILCAN_ID_PRIMARY_BODY_ELECTRONICS                  0x60
#define MILCAN_ID_PRIMARY_ALIVE_MESSAGE                     0x62
#define MILCAN_ID_PRIMARY_DIAGNOSTICS_0                     0x63
#define MILCAN_ID_PRIMARY_DIAGNOSTICS_1                     0x64
#define MILCAN_ID_PRIMARY_DIAGNOSTICS_2                     0x65
#define MILCAN_ID_PRIMARY_DIAGNOSTICS_3                     0x66

#define MILCAN_FRAME_TYPE_MESSAGE               0x00
#define MILCAN_FRAME_TYPE_CHANGE_MODE           0x01
#define MILCAN_FRAME_TYPE_NEW_FRAME             0x02
#define MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER    0x03

#define MILCAN_CONFIG_MODE_SEQ_NONE   0x00
#define MILCAN_CONFIG_MODE_SEQ_ENTER  0x01
#define MILCAN_CONFIG_MODE_SEQ_LEAVE  0x02

/// @brief The MILCAN A frame is standard CAN but with the mortal field (0 means it never expires - anything else is the time in nanoseconds at which it will expire).
struct milcan_frame {
  uint8_t frame_type;
  struct can_frame frame;
  uint64_t mortal;
};

/// @brief Creates a valid MilCAN ID
/// @param priority - The mesage priorty. Range 0 to 7.
/// @param request - 1 if a request message, else 0.
/// @param primary - Message Primary Type. Range 0 to 255.
/// @param secondary - Message Secondary Type. Range 0 to 255.
/// @param source - Source Address. Range to 0 to 255.
#define MILCAN_MAKE_ID(priority, request, primary, secondary, source)\
  (\
    MILCAN_ID_MILCAN_TYPE | CAN_EFF_FLAG\
    | (((priority) << 26) & MILCAN_ID_PRIORITY_MASK)\
    | ((request) ? MILCAN_ID_MILCAN_REQUEST : 0)\
    | (((primary) << 16) & MILCAN_ID_PRIMARY_MASK)\
    | (((secondary) << 8) & MILCAN_ID_SECONDARY_MASK)\
    | ((source) & MILCAN_ID_SOURCE_MASK)\
  )

/// @brief Creates a MilCAn message. NOTE You MUST set all 8 bytes of data, even if you're not using all of them.
/// @param id - The CAN ID.
/// @param mort - 0 = Message never grows old if non 0 then is the time to live in nanoseconds.
/// @param length - The length of the message to send
/// @param data0 - The data array byte 0.
/// @param data1 - The data array byte 1.
/// @param data2 - The data array byte 2.
/// @param data3 - The data array byte 3.
/// @param data4 - The data array byte 4.
/// @param data5 - The data array byte 5.
/// @param data6 - The data array byte 6.
/// @param data7 - The data array byte 7.
#define MILCAN_MAKE_FRAME(id, mort, length, data0, data1, data2, data3, data4, data5, data6, data7)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_MESSAGE,\
    .mortal = (mort),\
    .frame.can_id = (id),\
    .frame.data = {(data0), (data1), (data2), (data3), (data4), (data5), (data6), (data7)},\
    .frame.len = (length),\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0\
  }

/// @brief Creates the Sync Frame
/// @param source - Source Address. Range to 0 to 255.
/// @param counter - The counter (range 0 to 1023).
#define MILCAN_MAKE_SYNC(source, counter)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_MESSAGE,\
    .frame.can_id = MILCAN_MAKE_ID(0, 0, MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT, MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_SYNC_FRAME, (source)),\
    .frame.data = {((counter) & 0x0FF), (((counter) >> 8) & 0x03), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},\
    .frame.len = 2,\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0,\
    .mortal = 0\
  }

// Enter Config Mode
/// @brief Creates the first Enter Config Mode mesage
/// @param source - Source Address. Range to 0 to 255.
#define MILCAN_MAKE_ENTER_CONFIG_0(source)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_MESSAGE,\
    .frame.can_id = MILCAN_MAKE_ID(0, 0, MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT, MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_ENTER_CONFIG, (source)),\
    .frame.data = {(uint8_t)'C', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},\
    .frame.len = 1,\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0,\
    .mortal = 0\
  }

/// @brief Creates the second Enter Config Mode mesage
/// @param source - Source Address. Range to 0 to 255.
#define MILCAN_MAKE_ENTER_CONFIG_1(source)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_MESSAGE,\
    .frame.can_id = MILCAN_MAKE_ID(0, 0, MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT, MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_ENTER_CONFIG, (source)),\
    .frame.data = {(uint8_t)'F', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},\
    .frame.len = 1,\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0,\
    .mortal = 0\
  }

/// @brief Creates the third Enter Config Mode mesage
/// @param source - Source Address. Range to 0 to 255.
#define MILCAN_MAKE_ENTER_CONFIG_2(source)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_MESSAGE,\
    .frame.can_id = MILCAN_MAKE_ID(0, 0, MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT, MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_ENTER_CONFIG, (source)),\
    .frame.data = {(uint8_t)'G', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},\
    .frame.len = 1,\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0,\
    .mortal = 0\
  }

// Exit Config Mode
/// @brief Creates the first Exit Config Mode mesage
/// @param source - Source Address. Range to 0 to 255.
#define MILCAN_MAKE_EXIT_CONFIG_0(source)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_MESSAGE,\
    .frame.can_id = MILCAN_MAKE_ID(0, 0, MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT, MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_EXIT_CONFIG, (source)),\
    .frame.data = {(uint8_t)'O', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},\
    .frame.len = 1,\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0,\
    .mortal = 0\
  }

/// @brief Creates the second Exit Config Mode mesage
/// @param source - Source Address. Range to 0 to 255.
#define MILCAN_MAKE_EXIT_CONFIG_1(source)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_MESSAGE,\
    .frame.can_id = MILCAN_MAKE_ID(0, 0, MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT, MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_EXIT_CONFIG, (source)),\
    .frame.data = {(uint8_t)'P', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},\
    .frame.len = 1,\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0,\
    .mortal = 0\
  }

/// @brief Creates the third Exit Config Mode mesage
/// @param source - Source Address. Range to 0 to 255.
#define MILCAN_MAKE_EXIT_CONFIG_2(source)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_MESSAGE,\
    .frame.can_id = MILCAN_MAKE_ID(0, 0, MILCAN_ID_PRIMARY_SYSTEM_MANAGEMENT, MILCAN_ID_SECONDARY_SYSTEM_MANAGEMENT_EXIT_CONFIG, (source)),\
    .frame.data = {(uint8_t)'R', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},\
    .frame.len = 1,\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0,\
    .mortal = 0\
  }

/// @brief Creates the change of mode message
/// @param mode - The new mode 
#define MILCAN_MAKE_CHANGE_MODE(mode)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_CHANGE_MODE,\
    .frame.can_id = (mode),\
    .frame.data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},\
    .frame.len = 0,\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0,\
    .mortal = 0\
  }

/// @brief Creates the new sync value message
/// @param sync - The new value of the sync frame
#define MILCAN_MAKE_NEW_FRAME(sync)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_NEW_FRAME,\
    .frame.can_id = (sync),\
    .frame.data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},\
    .frame.len = 0,\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0,\
    .mortal = 0\
  }

/// @brief Creates the change of sync master message
/// @param id - The ID os the new sync master
#define MILCAN_MAKE_NEW_SYNC_MASTER(id)\
  {\
    .frame_type = MILCAN_FRAME_TYPE_CHANGE_SYNC_MASTER,\
    .frame.can_id = (id),\
    .frame.data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},\
    .frame.len = 0,\
    .frame.__pad = 0,\
    .frame.__res0 = 0,\
    .frame.__res1 = 0,\
    .mortal = 0\
  }

#define MILCAN_OK                   0   // Generic pass
#define MILCAN_ERROR                -1  // Generic error
#define MILCAN_ERROR_FATAL          -2  // Generic fatal error
#define MILCAN_ERROR_EOF            -3  // End Of File i.e. no more messages to read.
#define MILCAN_ERROR_MEM            -4  // Unable to allocate enough memory.

#define CAN_INTERFACE_NONE          0   // Basically, NULL
#define CAN_INTERFACE_SOCKET_CAN    1   // Not supported yet
#define CAN_INTERFACE_CANDO         2   // The CANdo module from netronics
#define CAN_INTERFACE_GSUSB_SO      3   // Our GSUSB (including candleLight) Shared Object implementation.


// Sync Frame Frequencies as defined in MWG-MILA-001 Rev 3 Section 3.2.5.3 (Page 19 of 79)
// These are recommended frequnecies so we should allow for these to be changed.
#define MILCAN_A_250K_DEFAULT_SYNC_HZ   (512)
#define MILCAN_A_500K_DEFAULT_SYNC_HZ   (128)
#define MILCAN_A_1M_DEFAULT_SYNC_HZ     (64)

// Bit Rates as defined in MWG-MILA-001 Rev 3 Section 2.4.1 (Page 13 of 79)
#define MILCAN_A_250K   0
#define MILCAN_A_500K   1
#define MILCAN_A_1M     2

// MILCAN options
#define MILCAN_A_OPTION_SYNC_MASTER     (0x0001)  // This device can be a Sync Master
#define MILCAN_A_OPTION_ECHO            (0x0002)  // Messages from ourselevs will also be added to RX Q
#define MILCAN_A_OPTION_LISTEN_CONTROL  (0x0004)  // Control messages (Sync, Enter Config and Exit Config) will be added to Rx Q.

void milcan_display_mode(void* interface);
void * milcan_open(uint8_t speed, uint16_t sync_freq_hz, uint8_t sourceAddress, uint8_t can_interface_type, uint16_t moduleNumber, uint16_t options);
void milcan_close(void * interface);
int milcan_send(void* interface, struct milcan_frame * frame);
int milcan_recv(void* interface, struct milcan_frame * frame);
// Start the process of changing to the Configuration Mode.
void milcan_change_to_config_mode(void* interface);
// Start the process of leaving the Configuration Mode.
void milcan_exit_configuration_mode(void* interface);

#endif // __MILCAN_H__