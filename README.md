# MilCAN_CHERI
An implementation of the MilCAN A stack for CheriBSD.

## Notes:
1. The Makefile should install both the Pure Caps and Hybrid versions of the SO into the correct directories.
2. This SO can work with both the CANdo (via our conversion of the driver found [here](https://github.com/GrassHopper1977/CANdoCheriBSD)) and Geschwister Schneider/candleLight style CAN to USB devices (via our driver available [here](https://github.com/GrassHopper1977/BSD_GSUSB)).
3. The code will dynamically load the the CANdo's SO if it's needed.
4. The [Geschwister Schneider/candleLight SO (libGSUSB.so)](https://github.com/GrassHopper1977/BSD_GSUSB) **MUST** be present for this code to work at all.
5. The MilCAN A Specification MWG-MILA-001 Revision 3 can be found [here](http://www.milcan.org).


## Using This Library
1. Run the Makefile, which will also place the SO files into the correct folders.
2. test.c shows a basic example of using the library.
3. test2.c demonstrates entering System Configuration Mode.
4. tests.c is a helper for the test script (runtests.sh) and contains code for all functions of this library.

## Functions

### void * milcan_open(uint8_t speed, uint16_t sync_freq_hz, uint8_t sourceAddress, uint8_t can_interface_type, uint16_t moduleNumber, uint16_t options);
Where:
* speed: One of MILCAN_A_250K, MILCAN_A_500K or MILCAN_A_1M.
* sync_freq_hz: The frequency to send Sync Frame at. You can also use the defaults, like MILCAN_A_250K_DEFAULT_SYNC_HZ, MILCAN_A_500K_DEFAULT_SYNC_HZ, MILCAN_A_1M_DEFAULT_SYNC_HZ.
* sourceAddress: The MilCAN device address. 0 is invalid. The lower the address the higher the priority.
* can_interface_type: One of CAN_INTERFACE_CANDO or CAN_INTERFACE_GSUSB_SO
* moduleNumber: 0 is the first USB to CAN device plugged in, 1 is the second, etc. The GSUSB and CANdo devices have separate counts. If we had one of each type, they would both be moduleNumber 0.
* options: 0 or value consisting of any of these OR'd together: MILCAN_A_OPTION_SYNC_MASTER, MILCAN_A_OPTION_ECHO or, MILCAN_A_OPTION_LISTEN_CONTROL.

Returns a void pointer that is passed to every other function to identify which device we are communicating with. In teh event of an error, returns NULL.

This function attempts top connect to teh chosen USB device and open it. It will immediately take part in any MilCAN A communications. If it is Sync Capable it will take part in the Sync Master selection process.

### void milcan_close(void * interface);
Where:
* interface: The void pointer returned by milcan_open();

Closes the connection.

### int milcan_send(void* interface, struct milcan_frame * frame);
Where:
* interface: The void pointer returned by milcan_open();
* frame: The MilCAN A frame to send.

Sends a MilCAN frame.

### int milcan_recv(void* interface, struct milcan_frame * frame);
Where:
* interface: The void pointer returned by milcan_open();
* frame: A pointer to a MilCAN A frame that can be filled in.

Used to read from the receive queue. If a message has been read we return 1, else 0.

// Start the process of changing to the Configuration Mode.
### void milcan_change_to_config_mode(void* interface);
Where:
* interface: The void pointer returned by milcan_open();

Begins the process of requesting to change to System Configuration Mode. The device will stay in System Configuration mode until it is:
a. Closed or;
b. Asked to send an Exit System Configuration Mode message or;
c. Receives an Exit System Configuration Mode message from another MilCAN A node.

### void milcan_exit_configuration_mode(void* interface);
Where:
* interface: The void pointer returned by milcan_open();

Sends the Exit System Configuration Mode message.

### void milcan_display_mode(void* interface)
Where:
* interface: The void pointer returned by milcan_open();

Just prints the current Milcan A mode or the device node to stdout. It can be useful for debugging.
