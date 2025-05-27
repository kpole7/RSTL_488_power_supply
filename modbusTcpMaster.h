// modbusTcpMaster.h

#ifndef MODBUSTCPMASTER_H_
#define MODBUSTCPMASTER_H_

//.................................................................................................
// Definitions of types
//.................................................................................................

enum class ModbusTcpClientStateClass{
	NOT_CONNECTED_YET					= 0,
	NO_ERROR							= 1,
	ERROR_SELECT						= 2,
	ERROR_TIMEOUT						= 3,
	ERROR_OPENING_SOCKET				= 4,
	ERROR_IP_ADDRESS_INVALID			= 5,
	ERROR_NO_CONNECTION_TO_SERVER		= 6,
	ERROR_SENDING_FRAME					= 7,
	ERROR_RECEIVING_FRAME				= 8,
	ERROR_RECEIVED_NOT_COMPLETE_FRAME	= 9,
	ERROR_RECEIVED_FROM_MODBUS_TCP_SLAVE= 10,
	ERROR_NUMBER_OF_RECEIVED_DATA		= 11,
	ERROR_UNEXPECTED_FUNCTION_CODE		= 12,
	ERROR_IDENTICATION_LABEL_MISMATCH	= 13,
	ERROR_NUMBER_OF_CHANNELS_CHANGE		= 14,
	ERROR_INCORRECT_NUMBER_OF_CHANNELS	= 15,
	ERROR_INCORRECT_RESPONSE_FOR_WRITING= 16,

	MODBUS_TCP_CLIENT_STATES_NUMER		= 17
};

//.................................................................................................
// Global variables
//.................................................................................................

extern ModbusTcpClientStateClass ModbusTcpCommunicationState;
extern const char* TcpServerErrorMessage;
extern bool NewStateOfModbusTcpInterface;
extern bool IsTcpServerIdentified;

//.................................................................................................
// Global function prototypes
//.................................................................................................

// Variable initialization for Modbus TCP client
void initializeTcpClientVariables(void);

// Exit procedure
void closeTcpClient(void);

// This function establishes connection with the Modbus TCP server, sends requests and receives responses.
// It is to be called periodically in the peripheral thread.
bool communicateTcpServer(void);

#endif /* MODBUSTCPMASTER_H_ */
