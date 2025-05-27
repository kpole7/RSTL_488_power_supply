// modbusTcpMaster.cpp
//
// Threads: peripheral thread (exception: DescriptionTextCopies)

#include "modbusTcpMaster.h"

#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <assert.h>
#include "dataSharingInterface.h"
#include "modbusTcpSlave.h"

//.................................................................................................
// Global variables
//.................................................................................................

ModbusTcpClientStateClass ModbusTcpCommunicationState;
const char* TcpServerErrorMessage;
bool NewStateOfModbusTcpInterface;
bool IsTcpServerIdentified;

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define READING_TCP_REGISTERS_NUMBER	22

#define MODBUS_TCP_HEADER_SIZE			9

//.................................................................................................
// Local variables
//.................................................................................................

static int TcpSocket;

static uint8_t ResponseBuffer[256];

static char* TextLoadedViaModbusTcp = (char*)&(ResponseBuffer[MODBUS_TCP_HEADER_SIZE]);

static uint8_t ChannelDescriptionTextRunUp;

static uint16_t ChannelDescriptionTextLengths[MAX_NUMBER_OF_SERIAL_PORTS];

// This array is used in the main FLTK thread
// It is set only once in the peripheral thread
static std::string DescriptionTextCopies[MAX_NUMBER_OF_SERIAL_PORTS];

//.................................................................................................
// Local function prototypes
//.................................................................................................

// This function implements timeout
static bool waitForResponse(int Socket, int TimeoutInSeconds);

static int createTcpSocket(void);

static int sendDataReadRequestFrame( uint16_t StartAddress, uint8_t NumberOfRegisters );

static int sendDataWriteRequestFrame( uint16_t RegisterAddress, uint16_t RegisterNewValue );

static int receiveResponseFrameForReadRequest( uint8_t ExpectedNumberOfRegisters );

static int receiveResponseFrameForWriteRequest( uint16_t RegisterAddress, uint16_t RegisterNewValue );

static int loadPrimitiveDataFromServer( uint16_t StartAddress, uint8_t NumberOfRegisters );

static int loadSectorFromServer( uint8_t Sector );

static int sendPrimitiveDataToServer( uint16_t RegisterAddress, uint16_t RegisterNewValue );

static inline uint8_t getLoadedDataUInt8( uint8_t Offset );

static inline uint8_t getLoadedDataUInt16( uint8_t Offset );

//.................................................................................................
// Function definitions
//.................................................................................................

// Variable initialization for Modbus TCP client
void initializeTcpClientVariables(void){
	ModbusTcpCommunicationState = ModbusTcpClientStateClass::NOT_CONNECTED_YET;
	NewStateOfModbusTcpInterface = false;
	IsTcpServerIdentified = false;
	TcpSocket = -1;
	ChannelDescriptionTextRunUp = 0;
}

// Exit procedure
void closeTcpClient(void){
	if (-1 != TcpSocket){
		close(TcpSocket);
		TcpSocket = -1;
	}
}

// This function is used in 'remote computer' mode; it drives TCP client;
// it establishes connection with the Modbus TCP server, sends requests and receives responses.
// It is to be called periodically in the peripheral thread.
bool communicateTcpServer(void){
	int Result;
	bool ReturnValue = false;

	assert( 0 == IsModbusTcpSlave );

	// Checking The TCP connection
	if (-1 == TcpSocket){
		Result = createTcpSocket();
		if (Result < 0){
			NewStateOfModbusTcpInterface = true;
			if (TcpSocket != -1){
		        close(TcpSocket);
		    	TcpSocket = -1;
			}
			usleep( 1000 );
			return ReturnValue;
		}
	}
	assert( 0 < TcpSocket );
	//--- Activities related to the zero sector ---
	Result = loadSectorFromServer( 0 );
	if (Result != 0){
		return ReturnValue;
	}
	// Checking the identification label (including the date and time of compilation) in the Modbus TCP register area
	for (uint8_t J = 0; J < sizeof(TcpSlaveIdentifier); J++){
		if (getLoadedDataUInt8(J+BYTE_OFFSET_IDENTIFICATION_LABEL) != TcpSlaveIdentifier[J]){
	        close(TcpSocket);
	    	TcpSocket = -1;

			ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_IDENTICATION_LABEL_MISMATCH;
			NewStateOfModbusTcpInterface = true;
	    	return ReturnValue;
		}
	}
	if (!IsTcpServerIdentified){
		// setting up NumberOfChannels

#if 0 // debugging
		printf("Pierwsza identyfikacja serwera TCP [%s]\n", TcpSlaveIdentifier );
#endif
		IsTcpServerIdentified = true;
		assert( 0 == NumberOfChannels );
		NumberOfChannels = getLoadedDataUInt8( BYTE_OFFSET_LSB_NUMBER_OF_CHANNELS );
		if ((0 == NumberOfChannels) || (NumberOfChannels > MAX_NUMBER_OF_SERIAL_PORTS) || (0 != getLoadedDataUInt8( BYTE_OFFSET_MSB_NUMBER_OF_CHANNELS ))){
	        close(TcpSocket);
	    	TcpSocket = -1;

			ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_INCORRECT_NUMBER_OF_CHANNELS;
			NewStateOfModbusTcpInterface = true;
	    	return ReturnValue;
		}
	}
	else{
		// Checking NumberOfChannels
		if ((NumberOfChannels != getLoadedDataUInt8( BYTE_OFFSET_LSB_NUMBER_OF_CHANNELS )) || (0 != getLoadedDataUInt8( BYTE_OFFSET_MSB_NUMBER_OF_CHANNELS ))){
	        close(TcpSocket);
	    	TcpSocket = -1;

			ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_NUMBER_OF_CHANNELS_CHANGE;
			NewStateOfModbusTcpInterface = true;
	    	return ReturnValue;
		}

		// Checking whether the control is remote or local
		pthread_mutex_t xLock = PTHREAD_MUTEX_INITIALIZER;
		pthread_mutex_lock( &xLock );
		if ((uint8_t)getLoadedDataUInt16( TCP_SERVER_ADDRESS_IS_REMOTE_CONTROL ) != ControlFromGuiHere){
			if (0 != getLoadedDataUInt16( TCP_SERVER_ADDRESS_IS_REMOTE_CONTROL )){
				assert( 1 == getLoadedDataUInt16( TCP_SERVER_ADDRESS_IS_REMOTE_CONTROL ));

				ControlFromGuiHere = 1;
			}
			else{
				ControlFromGuiHere = 0;
			}
			ReturnValue = true;	// to refresh application window label
		}
		pthread_mutex_unlock( &xLock );
	}

	//--- Activities related to the following sectors ---
	if (0 == ChannelDescriptionTextRunUp){
		// run-up actions
		Result = loadPrimitiveDataFromServer( TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS, MAX_NUMBER_OF_SERIAL_PORTS );
		if (Result != 0){
			return ReturnValue;
		}
		for(int M = 0; M < MAX_NUMBER_OF_SERIAL_PORTS; M++ ){
			ChannelDescriptionTextLengths[M] = getLoadedDataUInt16( 2*M );
		}
#if 0 // debugging
		printf("\nText lengths of descriptions ");
		for(int M = 0; M < MAX_NUMBER_OF_SERIAL_PORTS; M++ ){
			printf( "%2d ", ChannelDescriptionTextLengths[M] );
		}
		printf("\n");
#endif
		for(int M = 0; M < MAX_NUMBER_OF_SERIAL_PORTS; M++ ){
			if (0 != ChannelDescriptionTextLengths[M]){
				if (CHANNEL_DESCRIPTION_MAX_LENGTH > ChannelDescriptionTextLengths[M]){		// checking that the length of the text is correct
					Result = loadPrimitiveDataFromServer(
							TCP_SERVER_DESCRIPTION_LENGTHS_ADDRESS+(M+1)*TCP_SERVER_DESCRIPTION_ADDRESS_STEP,
							ChannelDescriptionTextLengths[M]/2 );
					if (Result != 0){
						return ReturnValue;
					}
					if (0 == TextLoadedViaModbusTcp[ ChannelDescriptionTextLengths[M]-1 ]){ // checking that the text is properly terminated

						pthread_mutex_t xLock = PTHREAD_MUTEX_INITIALIZER;
						pthread_mutex_lock( &xLock );
						DescriptionTextCopies[M] = TextLoadedViaModbusTcp;
						TableOfSharedDataForLowLevel[M].setDescription( &DescriptionTextCopies[M] );
						TableOfSharedDataForGui[M].setDescription( &DescriptionTextCopies[M] );
						pthread_mutex_unlock( &xLock );

						ReturnValue = true;
#if 0 // debugging
						std::cout << DescriptionTextCopies[M] << std::endl;
#endif
					}
				}
			}
		}
		ChannelDescriptionTextRunUp = 1;
	}
	else{
		// run-time actions

		for (uint8_t J = 0; J < NumberOfChannels; J++ ){
			Result = loadSectorFromServer( J+1 );
			if (Result != 0){
				return ReturnValue;
			}
			TableOfSharedDataForLowLevel[J].loadModbusTcpData( &ResponseBuffer[MODBUS_TCP_HEADER_SIZE] );

			if (TableOfSharedDataForLowLevel[J].isNewOrder()){
				uint16_t TemporaryValue;
				uint8_t TemporaryOrder = TableOfSharedDataForLowLevel[J].takeOrder( &TemporaryValue );
#if 0
				std::cout << " communicateTcpServer; new order= " << (int)TemporaryOrder << std::endl;
#endif
				if ((RTU_ORDER_POWER_ON == TemporaryOrder) ||
						(RTU_ORDER_POWER_OFF == TemporaryOrder) ||
						(RTU_ORDER_DELAYED_POWER_OFF == TemporaryOrder) ||
						(RTU_ORDER_CANCEL_DELAYED_POWER_OFF == TemporaryOrder))
				{
					Result = sendPrimitiveDataToServer(
							TCP_SERVER_START_ADDRESS+(J+1)*TCP_SERVER_SECTOR_ADDRESS_STEP+MODBUS_TCP_ADDRESS_ORDER_CODE,
							TemporaryOrder );
					if (Result != 0){
						return ReturnValue;
					}
				}
				if (RTU_ORDER_SET_VALUE == TemporaryOrder){
	   				// Modbus TCP command to write the set-point value is treated as an order for the lower layer
	   				// to set the set-point
					Result = sendPrimitiveDataToServer(
							TCP_SERVER_START_ADDRESS+(J+1)*TCP_SERVER_SECTOR_ADDRESS_STEP+MODBUS_TCP_ADDRESS_ORDER_VALUE,
							TemporaryValue );
					if (Result != 0){
						return ReturnValue;
					}
				}
			}
		}
	}

	assert( 0 < TcpSocket );

	if (ModbusTcpCommunicationState != ModbusTcpClientStateClass::NO_ERROR){
		NewStateOfModbusTcpInterface = true;
	}
	ModbusTcpCommunicationState = ModbusTcpClientStateClass::NO_ERROR;
	return ReturnValue;
}

// This function implements timeout
static bool waitForResponse(int Socket, int TimeoutInSeconds) {
    fd_set ReadFileDescriptors;
    FD_ZERO(&ReadFileDescriptors);
    FD_SET(Socket, &ReadFileDescriptors);

    struct timeval Timeout;
    Timeout.tv_sec = TimeoutInSeconds;	// seconds
    Timeout.tv_usec = 0;				// microseconds

    // Wait until the socket is ready to be read (or timeout)
    int Ready = select(Socket + 1, &ReadFileDescriptors, nullptr, nullptr, &Timeout);

    if (Ready == -1) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_SELECT;
//    	AuxiliaryTcpClientError = errno;
        return false;
    }
    if (Ready == 0) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_TIMEOUT;
//    	AuxiliaryTcpClientError = TimeoutInSeconds;
        return false;
    }
    return true;  // socket ready to be read
}

static int createTcpSocket(void){
    TcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (TcpSocket < 0) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_OPENING_SOCKET;
    	TcpSocket = -1;
        return -3;
    }

    // prepare the server address
    sockaddr_in ServerAddress;
    memset(&ServerAddress, 0, sizeof(ServerAddress));
    ServerAddress.sin_family = AF_INET;
    ServerAddress.sin_port = htons(TcpPortNumber);
    if (inet_pton(AF_INET, TcpSlaveAddress, &ServerAddress.sin_addr) <= 0) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_IP_ADDRESS_INVALID;
        close(TcpSocket);
    	TcpSocket = -1;
        return -2;
    }

    // Connect to a server
    if (connect(TcpSocket, (sockaddr*)&ServerAddress, sizeof(ServerAddress)) < 0) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_NO_CONNECTION_TO_SERVER;
        close(TcpSocket);
    	TcpSocket = -1;
        return -1;
    }
    return 0;
}

static int sendDataReadRequestFrame( uint16_t StartAddress, uint8_t NumberOfRegisters ){
    // Modbus TCP frame (Read Holding Registers)
    uint8_t RequestFrame[12];  		// frame size for "Read Holding Registers"
    uint16_t TransactionId = 1;	// Any transaction ID

    // header MBAP (Modbus Application Protocol)
    RequestFrame[0] = (TransactionId >> 8) & 0xFF;  // transaction ID (MSB)
    RequestFrame[1] = TransactionId & 0xFF;         // transaction ID (LSB)
    RequestFrame[2] = 0x00;  // Protocol (0x0000 for Modbus)
    RequestFrame[3] = 0x00;
    RequestFrame[4] = 0x00;  // Length (MSB)
    RequestFrame[5] = 0x06;  // Length (LSB)

    // PDU (Protocol Data Unit)
    RequestFrame[6] = 0x01;       // Unit ID (slave address)
    RequestFrame[7] = 0x03;       // function code (0x03 = Read Holding Registers)
    RequestFrame[8] = (uint8_t)(StartAddress >> 8);
    RequestFrame[9] = (uint8_t)StartAddress;
    RequestFrame[10] = 0;
    RequestFrame[11] = NumberOfRegisters;

    if (send(TcpSocket, RequestFrame, sizeof(RequestFrame), 0) != sizeof(RequestFrame)) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_SENDING_FRAME;
        close(TcpSocket);
    	TcpSocket = -1;
        return -1;
    }
    return 0;
}

static int sendDataWriteRequestFrame( uint16_t RegisterAddress, uint16_t RegisterNewValue ){
    // Modbus TCP frame (Write Single Holding Register)
    uint8_t RequestFrame[12];  		// frame size for "Read Holding Registers"
    uint16_t TransactionId = 1;	// Any transaction ID

    // header MBAP (Modbus Application Protocol)
    RequestFrame[0] = (TransactionId >> 8) & 0xFF;  // transaction ID (MSB)
    RequestFrame[1] = TransactionId & 0xFF;         // transaction ID (LSB)
    RequestFrame[2] = 0x00;  // Protocol (0x0000 for Modbus)
    RequestFrame[3] = 0x00;
    RequestFrame[4] = 0x00;  // Length (MSB)
    RequestFrame[5] = 0x06;  // Length (LSB)

    // PDU (Protocol Data Unit)
    RequestFrame[6] = 0x01;       // Unit ID (slave address)
    RequestFrame[7] = 0x06;       // function code (0x06 = Write Single Holding Register)
    RequestFrame[8] = (uint8_t)(RegisterAddress >> 8);
    RequestFrame[9] = (uint8_t)RegisterAddress;
    RequestFrame[10] = (uint8_t)(RegisterNewValue >> 8);
    RequestFrame[11] = (uint8_t)RegisterNewValue;

    if (send(TcpSocket, RequestFrame, sizeof(RequestFrame), 0) != sizeof(RequestFrame)) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_SENDING_FRAME;
        close(TcpSocket);
    	TcpSocket = -1;
        return -1;
    }
    return 0;
}

static int receiveResponseFrameForReadRequest( uint8_t ExpectedNumberOfRegisters ){
    // Waiting for a response with a timeout
    if (!waitForResponse(TcpSocket, 1)) {
        close(TcpSocket);
    	TcpSocket = -1;
        return -1;  // Timeout or failure; ServerTcpError is already set
    }

    int BytesReceived = recv(TcpSocket, ResponseBuffer, sizeof(ResponseBuffer), 0);
    if (BytesReceived <= 0) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_RECEIVING_FRAME;
        close(TcpSocket);
    	TcpSocket = -1;
        return -2;
    }

    if (BytesReceived < MODBUS_TCP_HEADER_SIZE) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_RECEIVED_NOT_COMPLETE_FRAME;
        return -3;
    }

    // check for an error message
    if (ResponseBuffer[7] == 0x83) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_RECEIVED_FROM_MODBUS_TCP_SLAVE;
//    	AuxiliaryTcpClientError = (int)ResponseBuffer[8];
        return -4;
    }

    // Checking the number of data bytes (for 10 registers: 20 bytes + 9 header bytes = 29)
    if ((2*ExpectedNumberOfRegisters+MODBUS_TCP_HEADER_SIZE != BytesReceived) || (2*ExpectedNumberOfRegisters != ResponseBuffer[8])) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_NUMBER_OF_RECEIVED_DATA;
        return -5;
    }

    if (ResponseBuffer[7] != 0x03) {  // Check function code (0x03 = Read Holding Registers)
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_UNEXPECTED_FUNCTION_CODE;
        return -6;
    }
    return 0;
}

static int receiveResponseFrameForWriteRequest( uint16_t RegisterAddress, uint16_t RegisterNewValue ){
    // Waiting for a response with a timeout
    if (!waitForResponse(TcpSocket, 1)) {
        close(TcpSocket);
    	TcpSocket = -1;
        return -1;  // Timeout or failure; ServerTcpError is already set
    }

    int BytesReceived = recv(TcpSocket, ResponseBuffer, sizeof(ResponseBuffer), 0);
    if (BytesReceived <= 0) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_RECEIVING_FRAME;
        close(TcpSocket);
    	TcpSocket = -1;
        return -2;
    }

    if (BytesReceived < MODBUS_TCP_HEADER_SIZE) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_RECEIVED_NOT_COMPLETE_FRAME;
        return -3;
    }

    // check for an error message
    if (ResponseBuffer[7] == 0x83) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_RECEIVED_FROM_MODBUS_TCP_SLAVE;
//    	AuxiliaryTcpClientError = (int)ResponseBuffer[8];
        return -4;
    }

    // Checking the number of data bytes (for 10 registers: 20 bytes + 9 header bytes = 29)
    if (12 != BytesReceived){
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_NUMBER_OF_RECEIVED_DATA;
        return -5;
    }

    if (ResponseBuffer[7] != 0x06) {  // Check function code (0x06 = Write Single Holding Register)
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_UNEXPECTED_FUNCTION_CODE;
        return -6;
    }

    if ( 256u*(uint16_t)ResponseBuffer[8]+(uint16_t)ResponseBuffer[9] != RegisterAddress ) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_INCORRECT_RESPONSE_FOR_WRITING;
        return -7;
    }
    if ( 256u*(uint16_t)ResponseBuffer[10]+(uint16_t)ResponseBuffer[11] != RegisterNewValue ) {
    	ModbusTcpCommunicationState = ModbusTcpClientStateClass::ERROR_INCORRECT_RESPONSE_FOR_WRITING;
        return -8;
    }

    return 0;
}

static int loadPrimitiveDataFromServer( uint16_t StartAddress, uint8_t NumberOfRegisters ){
	int Result;

	Result = sendDataReadRequestFrame( StartAddress, NumberOfRegisters );
	if (Result < 0){
		NewStateOfModbusTcpInterface = true;
		if (TcpSocket != -1){
	        close(TcpSocket);
	    	TcpSocket = -1;
		}
		return -1;
	}

	assert( 0 < TcpSocket );

	Result = receiveResponseFrameForReadRequest( NumberOfRegisters );
	if (Result < 0){
		NewStateOfModbusTcpInterface = true;
		if (TcpSocket != -1){
	        close(TcpSocket);
	    	TcpSocket = -1;
		}
		return -1;
	}
	return 0;
}

static int loadSectorFromServer( uint8_t Sector ){
	int Result;

	Result = sendDataReadRequestFrame(
			(uint16_t)Sector * TCP_SERVER_SECTOR_ADDRESS_STEP + TCP_SERVER_START_ADDRESS,
			READING_TCP_REGISTERS_NUMBER );
	if (Result < 0){
		NewStateOfModbusTcpInterface = true;
		if (TcpSocket != -1){
	        close(TcpSocket);
	    	TcpSocket = -1;
		}
		return -1;
	}

	assert( 0 < TcpSocket );

	Result = receiveResponseFrameForReadRequest( READING_TCP_REGISTERS_NUMBER );
	if (Result < 0){
		NewStateOfModbusTcpInterface = true;
		if (TcpSocket != -1){
	        close(TcpSocket);
	    	TcpSocket = -1;
		}
		return -1;
	}
	return 0;
}

static int sendPrimitiveDataToServer( uint16_t RegisterAddress, uint16_t RegisterNewValue ){
	int Result;

	Result = sendDataWriteRequestFrame( RegisterAddress, RegisterNewValue );
	if (Result < 0){
		NewStateOfModbusTcpInterface = true;
		if (TcpSocket != -1){
	        close(TcpSocket);
	    	TcpSocket = -1;
		}
		return -1;
	}

	assert( 0 < TcpSocket );

	Result = receiveResponseFrameForWriteRequest( RegisterAddress, RegisterNewValue );
	if (Result < 0){
		NewStateOfModbusTcpInterface = true;
		if (TcpSocket != -1){
	        close(TcpSocket);
	    	TcpSocket = -1;
		}
		return -1;
	}
	return 0;
}

static inline uint8_t getLoadedDataUInt8( uint8_t Offset ){
	return ResponseBuffer[MODBUS_TCP_HEADER_SIZE + Offset];
}

static inline uint8_t getLoadedDataUInt16( uint8_t Offset ){
	return (((uint16_t)ResponseBuffer[MODBUS_TCP_HEADER_SIZE + Offset]) << 8) + (uint16_t)ResponseBuffer[MODBUS_TCP_HEADER_SIZE + Offset+1];
}
