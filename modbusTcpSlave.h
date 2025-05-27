// modbusTcpSlave.h

#ifndef MODBUSTCPSLAVE_H_
#define MODBUSTCPSLAVE_H_

#include <inttypes.h>

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define MAX_NUMBER_OF_SERIAL_PORTS				16

#define MODBUS_RTU_REGISTERS_AREA				14

#define MODBUS_TCP_ADDRESS_PERMILLE_ERROR		(MODBUS_RTU_REGISTERS_AREA)
#define MODBUS_TCP_ADDRESS_MAX_SEQUENCE			(MODBUS_RTU_REGISTERS_AREA+1)
#define MODBUS_TCP_ADDRESS_COMMUNICATION_STATE	(MODBUS_RTU_REGISTERS_AREA+2)	// MSB = CommunicationState; LSB = TransmissionAcknowledged
#define MODBUS_TCP_ADDRESS_IS_POWER_ON			(MODBUS_RTU_REGISTERS_AREA+3)	// MSB = IsPowerSwitchOn; LSB = IsPsuPhysicalIdCompatibile
#define MODBUS_TCP_ADDRESS_LAST_FRAME_ERROR		(MODBUS_RTU_REGISTERS_AREA+4)
#define MODBUS_TCP_ADDRESS_EXPECTED_ID			(MODBUS_RTU_REGISTERS_AREA+5)
#define MODBUS_TCP_ADDRESS_ORDER_CODE			(MODBUS_RTU_REGISTERS_AREA+6)	// Order code and Order value must be at the end
#define MODBUS_TCP_ADDRESS_ORDER_VALUE			(MODBUS_RTU_REGISTERS_AREA+7)	// of Modbus TCP area for a given channel

#define MODBUS_TCP_SECTOR_SIZE					(MODBUS_RTU_REGISTERS_AREA+8)
#define MODBUS_TCP_SECTOR_READING_SIZE			MODBUS_TCP_ADDRESS_ORDER_CODE

#define RTU_ORDER_READING_ALL					0		// primitive order
#define RTU_ORDER_READING_FIRST					1		// primitive order
#define RTU_ORDER_READING_LAST					2		// primitive order
#define RTU_ORDER_POWER_ON						3		// primitive order
#define RTU_ORDER_POWER_OFF						4		// primitive order
#define RTU_ORDER_SET_VALUE						5		// primitive order
#define RTU_ORDER_DELAYED_POWER_OFF				6		// virtual order (for low layer state machine)
#define RTU_ORDER_CANCEL_DELAYED_POWER_OFF		7		// virtual order (for low layer state machine)
#define RTU_ORDER_NONE							255

#define RTU_PRIMITIVE_ORDER_TOTAL_NUMBER		RTU_ORDER_DELAYED_POWER_OFF

#define TCP_SERVER_START_ADDRESS				1000
#define TCP_SERVER_SECTOR_ADDRESS_STEP			100

#define TCP_SERVER_ADDRESS_IS_REMOTE_CONTROL	0	// Register 0 tells the remote computer:
													// for value of 0 'you have no control, only monitoring',
													// for value of 1 'you have full control'
#define TCP_SERVER_ADDRESS_NUMBER_OF_CHANNELS	1	// The number of channels is set in the configuration file, which is located in the 'local computer'
#define TCP_SERVER_ADDRESS_IDENTIFICATION_LABEL	2	// the label is constant (and includes date and time of compilation)

#define BYTE_OFFSET_MSB_IS_REMOTE_CONTROL		0
#define BYTE_OFFSET_LSB_IS_REMOTE_CONTROL		1
#define BYTE_OFFSET_MSB_NUMBER_OF_CHANNELS		2
#define BYTE_OFFSET_LSB_NUMBER_OF_CHANNELS		3
#define BYTE_OFFSET_IDENTIFICATION_LABEL		4

#define TCP_SERVER_LOCAL_CONTROL				0
#define TCP_SERVER_REMOTE_CONTROL				1

#define TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS	4000
#define TCP_SERVER_DESCRIPTION_ADDRESS_STEP		100

//...............................................................................................
// Global constants
//...............................................................................................

// The TCP server identification label includes the date and time of compilation
// to ensure that both the TCP server and client are on the same version
extern const char TcpSlaveIdentifier[40];

//...............................................................................................
// Global variables
//...............................................................................................

// Number of serial ports declared in the configuration file
extern uint8_t NumberOfChannels;

// This variable is initialized while the configuration file is read
// IsModbusTcpSlave equals 1: Modbus TCP slave and Modbus RTU master are active
// IsModbusTcpSlave equals 0: Modbus TCP master is active and Modbus RTU is inactive
// Threads: the variable is modified only once in the peripheral thread
extern uint8_t IsModbusTcpSlave;

// This is Modbus TCP port number.
// This variable is set when reading the configuration file
// Threads: the variable is modified only once in the peripheral thread
extern uint16_t TcpPortNumber;

// The slave TCP address in text form, for instance "192.168.1.100" (used when Modbus TCP master is active)
// Threads: the variable is modified only once in the peripheral thread
extern char TcpSlaveAddress[20];

// If IsModbusTcpSlave == 1, then
//     if the user selects 'Local control' (using the GUI button), ControlFromGuiHere is set to '1' (the default value)
//     in the other case ControlFromGuiHere is cleared
// If IsModbusTcpSlave == 0, then
//     if the Modbus TCP master receives information that the TCP server (slave) has remote control set, ControlFromGuiHere is set to '1'
//     in the other case ControlFromGuiHere is cleared (the default value)
// Threads: main thread
extern uint8_t ControlFromGuiHere;

// This array is equivalent to TableOfSharedDataForLowLevel; this array is mainly used in the Modbus TCP server
// The array consists of sectors; the first sector contains information:
//   - whether the local computer has taken control, or has passed control to a remote PC;
//   - how many Modbus RTU channels there are;
//   - Modbus TCP server identification label
// the remaining sectors contain information about individual power supplies
extern uint16_t TableOfSharedDataForTcpServer[MAX_NUMBER_OF_SERIAL_PORTS+1][MODBUS_TCP_SECTOR_SIZE];

// This is a table of Modbus registers containing the description lengths of each power supply unit.
// These registers occupy addresses from TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS
// These registers are initialized once, at program startup, and remain constant thereafter
// View comments on ChannelDescriptionTextsPtr
extern uint16_t ChannelDescriptionLength[MAX_NUMBER_OF_SERIAL_PORTS];

// This is a table of pointers to texts that are the description of channels
// View comments on ChannelDescriptionTextsPtr
extern char* ChannelDescriptionTextsPtr[MAX_NUMBER_OF_SERIAL_PORTS];

// ChannelDescriptionPlainTextsPtr is a pointer to the allocated memory area used for the description of channels.
// Example
// An excerpt from the configuration file:
//
//id=13		port='/dev/ttyS0' 	opis='Magnes 1'
//id=0x66	port='/dev/ttyS1' 	opis='Magnes 2'
//id=51		port='/dev/ttyS2'	opis='Magnes 3'
//id=52		port='/dev/ttyS3'	opis='Magnes  4'
//id=102	port='/dev/ttyUSB0' opis='Magnes   5'
//id=53		port='/dev/ttyS4'	opis='Magnes 6'
//
// ChannelDescriptionLength[] = 10, 10, 10, 10, 12, 10, 0, 0, ...
// ChannelDescriptionPlainTextsPtr[] =
//	4D 61 67 6E 65 73 20 31 00 00
//	4D 61 67 6E 65 73 20 32 00 00
//	4D 61 67 6E 65 73 20 33 00 00
//	4D 61 67 6E 65 73 20 20 34 00
//	4D 61 67 6E 65 73 20 20 20 35 00 00
//	4D 61 67 6E 65 73 20 36 00 00
// For instance, in order to get description of the 3rd PSU (that is 'Magnes 3'),
// TCP client should send a request for 5 registers (that is ChannelDescriptionLength[2]/2)
// from address TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS + 3 * TCP_SERVER_DESCRIPTION_ADDRESS_STEP
extern char* ChannelDescriptionPlainTextsPtr;

//.................................................................................................
// Global function prototypes
//.................................................................................................

#ifdef __cplusplus
extern "C" {
#endif

// This function initializes TCP socket and starts additional thread for the Modbus TCP slave
char initializeModbusTcpSlave( void );

// This function closes the open socket
void closeModbusTcpSlave( void );

#ifdef __cplusplus
}
#endif

#endif /* MODBUSTCPSLAVE_H_ */
