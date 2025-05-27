// modbusRtuMaster.cpp
//
// Threads: peripheral thread
//
// This module is designed to handle a single communication channel with the power supply unit.
// The module is designed to operate in a peripheral thread.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <inttypes.h>
#include <assert.h>
#include <time.h>
#include <ctype.h>
#include <pthread.h>
#include "modbusRtuMaster.h"

#include "dataSharingInterface.h"

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define HISTORY_ELEMENT_BITS				32

// This constant determines Modbus speed; it is defined in termios.h
#define MODBUS_RTU_HARDWARE_SPEED			B19200

#define MODBUS_FRAME_SIZE_READING_ALL		33
#define MODBUS_FRAME_SIZE_READING_FIRST		21
#define MODBUS_FRAME_SIZE_READING_LAST		19
#define MODBUS_FRAME_SIZE_MAX				40

#define WRITE_NEW_VALUE_FRAME_SIZE			8

#define POSITION_OF_VALUE_IN_FRAME			4	// this refers to the command to write a single register
#define POSITION_OF_CRC_IN_FRAME			6

#define POSITION_OF_DATA_IN_FRAME			3	// this refers to a response frame
#define READING_REGISTERS_NUMBER			7	// this refers to ORDER_READING_FIRST and ORDER_READING_LAST

#define POWERING_DOWN_TIMEOUT_IN_SECONDS	8	// will actually be 2 seconds more
#define POWERING_DOWN_TIMEOUT				(POWERING_DOWN_TIMEOUT_IN_SECONDS * TIME_SYNCHRONIZATION_FREQUENCY)		// not tested for other values of TIME_SYNCHRONIZATION_FREQUENCY than 4
#define POWERING_DOWN_CURRENT_LIMIT			200		// Amperes * 100
#define POWERING_DOWN_COUNTER_MAX			30000

//.................................................................................................
// Definitions of types
//.................................................................................................

typedef struct FrameInfoStruct{
	const uint8_t *outgoingFramePtr;
	const uint8_t outgoingFrameLength;
	const uint8_t *responseFramePtr;
	const uint8_t responseFrameTotalLength;
	const uint8_t knownResponseLength;
}FrameInfo;

//.................................................................................................
// Local variables
//.................................................................................................

static uint8_t WriteNewValueFrame[WRITE_NEW_VALUE_FRAME_SIZE];

static uint8_t BufferForModbusFrames[MODBUS_FRAME_SIZE_MAX+2];

//.................................................................................................
// Local constants
//.................................................................................................

// Modbus commands are prepared in such a way that the response to a given command cannot be mistaken with the response to another command.

static const uint8_t ReadAllRegistersFrame[] =	{0x01, 0x03, 0x03, 0xE8, 0x00, 0x0E, 0x44, 0x7E}; // Registers: 1000 ... 1013
static const uint8_t ReadFirstRegistersFrame[] ={0x01, 0x03, 0x03, 0xE8, 0x00, 0x08, 0xC4, 0x7C}; // Registers: 1000 ... 1007
static const uint8_t ReadLastRegistersFrame[] = {0x01, 0x03, 0x03, 0xEF, 0x00, 0x07, 0x35, 0xB9}; // Registers: 1007 ... 1013
static const uint8_t WritePowerOnFrame[]  = 	{0x01, 0x06, 0x03, 0xe8, 0x00, 0x01, 0xc8, 0x7a};
static const uint8_t WritePowerOffFrame[] =		{0x01, 0x06, 0x03, 0xe8, 0x00, 0x00, 0x09, 0xba};

static const uint8_t WriteZeroValueFrame[]= 	{0x01, 0x06, 0x03, 0xe9, 0x00, 0x00, 0x58, 0x7a}; // Value = 0; for a different value will be a different crc
static const uint8_t ResponseOfReadAll[] = 		{0x01, 0x03, 0x1C}; // The beginning of the response
static const uint8_t ResponseOfReadFirst[] =	{0x01, 0x03, 0x10}; // The beginning of the response
static const uint8_t ResponseOfReadLast[] =		{0x01, 0x03, 0x0E}; // The beginning of the response

// See ORDER_READING_ALL and so on
static const FrameInfo FrameInfoTable[RTU_PRIMITIVE_ORDER_TOTAL_NUMBER] =
	{{ReadAllRegistersFrame,	sizeof(ReadAllRegistersFrame),	ResponseOfReadAll,	MODBUS_FRAME_SIZE_READING_ALL,	sizeof(ResponseOfReadAll)},
	{ReadFirstRegistersFrame,	sizeof(ReadFirstRegistersFrame),ResponseOfReadFirst,MODBUS_FRAME_SIZE_READING_FIRST,	sizeof(ResponseOfReadFirst)},
	{ReadLastRegistersFrame,	sizeof(ReadLastRegistersFrame),	ResponseOfReadLast,	MODBUS_FRAME_SIZE_READING_LAST,	sizeof(ResponseOfReadLast)},
	{WritePowerOnFrame,			sizeof(WritePowerOnFrame),		WritePowerOnFrame, 	sizeof(WritePowerOnFrame),	sizeof(WritePowerOnFrame)},
	{WritePowerOffFrame,		sizeof(WritePowerOffFrame),		WritePowerOffFrame, sizeof(WritePowerOffFrame),	sizeof(WritePowerOffFrame)},
	{WriteNewValueFrame,		sizeof(WriteNewValueFrame),		WriteNewValueFrame, sizeof(WriteNewValueFrame),	sizeof(WriteNewValueFrame)}
	};

//.................................................................................................
// Local function prototypes
//.................................................................................................

// This function opens and configures a serial port
static int configureSerialPort(const char *DeviceName);

// This function reads a packet of bytes from the serial port
static int16_t receiveResponse(int FileHandler, uint8_t *FrameBuffer, uint8_t ExpectedNumberOfBytes);

// This function calculates crc16 of Modbus type for a given frame
static uint16_t crc16( const uint8_t *Buffer, uint8_t Length );


//........................................................................................................
// Function definitions of class TransmissionErrorsMonitor
//........................................................................................................

TransmissionErrorsMonitor::TransmissionErrorsMonitor() {
	uint8_t J;
	for (J = 0; J < TRANSMISSION_ERRORS_TABLE; J++) {
		TransmissionErrorsHistory[J] = 0lu;
	}
	TransmissionErrorsHead = 0;
	TransmissionErrorsValue = 0;
	TransmissionErrorsSequence = 0;
}

void TransmissionErrorsMonitor::addSampleAndCalculateStatistics(bool New){
	uint16_t BitIndex;
	uint16_t LengthOfSequence, J;

	static_assert(HISTORY_ELEMENT_BITS == 8*sizeof(TransmissionErrorsHistory[0]), "assert: HISTORY_ELEMENT_BITS");

	if(New){
		TransmissionErrorsHistory[TransmissionErrorsHead/HISTORY_ELEMENT_BITS] |= (1 << (TransmissionErrorsHead % HISTORY_ELEMENT_BITS));
	}
	else{
		TransmissionErrorsHistory[TransmissionErrorsHead/HISTORY_ELEMENT_BITS] &= ~(1 << (TransmissionErrorsHead % HISTORY_ELEMENT_BITS));
	}
	TransmissionErrorsHead++;
	TransmissionErrorsHead %= 8*sizeof(TransmissionErrorsHistory);

	TransmissionErrorsValue = 0;
	TransmissionErrorsSequence = 0;
	LengthOfSequence = 0;
	BitIndex = TransmissionErrorsHead;
	for(J=0; J<TRANSMISSION_ERRORS_TABLE*HISTORY_ELEMENT_BITS; J++){
		if((TransmissionErrorsHistory[BitIndex/HISTORY_ELEMENT_BITS] & (1 << (BitIndex % HISTORY_ELEMENT_BITS))) == 0){
			LengthOfSequence = 0;
		}
		else{
			TransmissionErrorsValue++;
			LengthOfSequence++;
			if(LengthOfSequence > TransmissionErrorsSequence){
				TransmissionErrorsSequence = LengthOfSequence;
			}
		}
		BitIndex++;
		BitIndex %= 8*sizeof(TransmissionErrorsHistory);
	}
}

uint16_t TransmissionErrorsMonitor::getErrorPerMille(void){
	return ((uint32_t)1000 * (uint32_t)TransmissionErrorsValue) / ((uint32_t)8 * (uint32_t)sizeof(TransmissionErrorsHistory));
}

uint16_t TransmissionErrorsMonitor::getErrorMaxSequence(void){
	return TransmissionErrorsSequence;
}

//........................................................................................................
// Function definitions of class TransmissionChannel
//........................................................................................................

TransmissionChannel::TransmissionChannel(){
	uint8_t J;
	PowerSupplyExpectedId = 0xFFFFu;
	SerialPortHandler = -1;
	CommunicationConsecutiveErrors = 255;
	TransmissionAcknowledgement = false;
	FrameLastError = LastFrameErrorClass::UNSPECIFIED;
	for(J=0; J<MODBUS_RTU_REGISTERS_AREA; J++){
		ModbusRegisters[J] = 0;
	}
	PresentOrder = RTU_ORDER_NONE;
	PoweringDownCounter = -1;
}

TransmissionChannel::~TransmissionChannel(){
	if(-1 != SerialPortHandler){
		close(SerialPortHandler);
		SerialPortHandler = -1;
	}
}

void TransmissionChannel::open( int ChannelId ){
	int Result;
	static const char* PortNameCharPtr;

	PortNameCharPtr = PortName.c_str();
	Result = access(PortNameCharPtr, F_OK );
	if(0 == Result){
		SerialPortHandler = configureSerialPort( PortNameCharPtr );
	}
	if(-1 == SerialPortHandler){
		TableOfSharedDataForLowLevel[ChannelId].loadModbusRtuData( CommunicationStatesClass::PORT_NOT_OPEN, nullptr,
				LastFrameErrorClass::UNSPECIFIED, 0xFFFFu, 0xFFFFu, false );
	}
	CommunicationConsecutiveErrors = 255;
	TransmissionAcknowledgement = false;
}

void TransmissionChannel::singleInquiryOfSlave( int ChannelId ){
	uint16_t NewValueUint16;
    int16_t NumberOfReceivedBytes;
    int16_t NumberOfSentBytes;
    uint16_t CrcCalculated;
    bool IsDataTransmissionError;
    uint8_t ExpectedResponseLength;
    uint16_t J;
    LastFrameErrorClass FrameErrorCode;

	if (-1 == SerialPortHandler){
		return;
	}

	if(RTU_ORDER_NONE != PresentOrder){

		assert(PresentOrder < RTU_PRIMITIVE_ORDER_TOTAL_NUMBER);

		// Receiving data from the interface
		ExpectedResponseLength = FrameInfoTable[PresentOrder].responseFrameTotalLength;
		IsDataTransmissionError = false;
		NumberOfReceivedBytes = receiveResponse(SerialPortHandler, BufferForModbusFrames, ExpectedResponseLength);
		if (-1 == NumberOfReceivedBytes) {
			close(SerialPortHandler);
			SerialPortHandler = -1;
			IsDataTransmissionError = true;

			TableOfSharedDataForLowLevel[ChannelId].loadModbusRtuData( CommunicationStatesClass::PORT_NOT_OPEN, nullptr,
					LastFrameErrorClass::UNSPECIFIED, CommunicationMonitor.getErrorPerMille(),
					CommunicationMonitor.getErrorMaxSequence(), TransmissionAcknowledgement );
		}
		else{
			// Checking the formal correctness of the Modbus frame
			IsDataTransmissionError = false;
			FrameErrorCode = LastFrameErrorClass::PERFECTION;
			if(NumberOfReceivedBytes != ExpectedResponseLength){
				IsDataTransmissionError = true;
				if(0 == NumberOfReceivedBytes){
					FrameErrorCode = LastFrameErrorClass::NO_RESPONSE;
				}
				else{
					FrameErrorCode = LastFrameErrorClass::NOT_COMPLETE_FRAME;
				}
			}
			else{
				CrcCalculated = crc16(BufferForModbusFrames,NumberOfReceivedBytes-2);
				if((BufferForModbusFrames[NumberOfReceivedBytes-2] != (uint8_t)(CrcCalculated & 0xFFu) ||
						(BufferForModbusFrames[NumberOfReceivedBytes-1] != (uint8_t)(CrcCalculated >> 8))))
				{
					IsDataTransmissionError = true;
					FrameErrorCode = LastFrameErrorClass::BAD_CRC;
				}
				else{
					for(J=0; J < FrameInfoTable[PresentOrder].knownResponseLength; J++) {
						if(BufferForModbusFrames[J] != FrameInfoTable[PresentOrder].responseFramePtr[J]){
							IsDataTransmissionError = true;
							FrameErrorCode = LastFrameErrorClass::OTHER_FRAME_ERROR;
						}
					}
				}
			}
			if (LastFrameErrorClass::PERFECTION != FrameErrorCode){
				FrameLastError = FrameErrorCode;
			}
			else{
				TransmissionAcknowledgement = true;
			}

			if(IsDataTransmissionError && (0 != NumberOfReceivedBytes)){
				// in order to reset the Modbus machine after any transmission failure
				NumberOfReceivedBytes = receiveResponse(SerialPortHandler, BufferForModbusFrames, MODBUS_FRAME_SIZE_MAX);
				if (-1 == NumberOfReceivedBytes) {
					close(SerialPortHandler);
					SerialPortHandler = -1;
				}
			}
			if(!IsDataTransmissionError){
				CommunicationConsecutiveErrors = 0;
				if(RTU_ORDER_READING_FIRST == PresentOrder){
					for( J=0; J < READING_REGISTERS_NUMBER; J++ ){
						ModbusRegisters[J] = 256 * (uint16_t)BufferForModbusFrames[POSITION_OF_DATA_IN_FRAME+2*J] +
								(uint16_t)BufferForModbusFrames[POSITION_OF_DATA_IN_FRAME+2*J+1];
					}

					// information about updating the register containing the setpoint is needed
					// in the GUI to protect against fast multiclicking (the buttons '+1A' '-0.1A' ... '-1A')
					pthread_mutex_t xLock = PTHREAD_MUTEX_INITIALIZER;
					pthread_mutex_lock( &xLock );
					multiclickCountdown();
					pthread_mutex_unlock( &xLock );
				}
				if(RTU_ORDER_READING_LAST == PresentOrder){
					for( J=0; J < READING_REGISTERS_NUMBER; J++ ){
						ModbusRegisters[J+READING_REGISTERS_NUMBER] =
								256 * (uint16_t)BufferForModbusFrames[POSITION_OF_DATA_IN_FRAME+2*J] +
								(uint16_t)BufferForModbusFrames[POSITION_OF_DATA_IN_FRAME+2*J+1];
					}
				}

				TableOfSharedDataForLowLevel[ChannelId].loadModbusRtuData( CommunicationStatesClass::HEALTHY, &ModbusRegisters[0],
						FrameLastError, CommunicationMonitor.getErrorPerMille(),
						CommunicationMonitor.getErrorMaxSequence(), TransmissionAcknowledgement );

#if 0
				if(RTU_ORDER_READING_FIRST == PresentOrder){
					if (ModbusRegisters[MODBUS_ADDRES_CURRENT_FILTERED] > 200){
						printf("*");
					}
					else{
						printf(".");
					}
					fflush( stdout );
				}
#endif

			} // if(!IsResponseFrameError)
			else{

#if 0
				if((CommunicationConsecutiveErrors % 8) == 0){
					printf("c.err [%d] %d ", ChannelId, CommunicationConsecutiveErrors);
					fflush( stdout );
				}
#endif

				if(CommunicationConsecutiveErrors < 255){
					CommunicationConsecutiveErrors++;
				}
				if(CommunicationConsecutiveErrors > COMMUNICATION_ERRORS_TOLERANCE){
					TableOfSharedDataForLowLevel[ChannelId].loadModbusRtuData( CommunicationStatesClass::PERMANENT_ERRORS, nullptr,
							FrameLastError, CommunicationMonitor.getErrorPerMille(),
							CommunicationMonitor.getErrorMaxSequence(), TransmissionAcknowledgement );
				}
				else{
					if(CommunicationConsecutiveErrors > COMMUNICATION_WARNING_TOLERANCE){
						TableOfSharedDataForLowLevel[ChannelId].loadModbusRtuData( CommunicationStatesClass::TEMPORARY_ERRORS, nullptr,
								FrameLastError, CommunicationMonitor.getErrorPerMille(),
								CommunicationMonitor.getErrorMaxSequence(), TransmissionAcknowledgement );
					}
				}
			}
		} // if (-1 == NumberOfReceivedBytes){ ... }else{

		CommunicationMonitor.addSampleAndCalculateStatistics(IsDataTransmissionError);

	} // if(ORDER_NONE != PresentOrder)

	if (-1 == SerialPortHandler){
		return;
	}

	usleep(1000);

	// Preparing the order
	if((FractionOfSecond % 2) == 0){
		if ( ! TableOfSharedDataForLowLevel[ChannelId].isNewPrimitiveOrder() ){
			// common situation
			PresentOrder = RTU_ORDER_READING_LAST;
		}
		else{
			// event coming from GUI
			PresentOrder = TableOfSharedDataForLowLevel[ChannelId].takePrimitiveOrder( &NewValueUint16 );
		}
	}
	else{
		PresentOrder = RTU_ORDER_READING_FIRST;
	}

	// Sending frame
	if(RTU_ORDER_SET_VALUE == PresentOrder){
		// put together a frame with a set value
		memcpy(WriteNewValueFrame,WriteZeroValueFrame,sizeof(WriteNewValueFrame));
		WriteNewValueFrame[POSITION_OF_VALUE_IN_FRAME] = (uint8_t)(NewValueUint16 >> 8);
		WriteNewValueFrame[POSITION_OF_VALUE_IN_FRAME+1] = (uint8_t)(NewValueUint16 & 0xFFu);
		NewValueUint16 = crc16(WriteNewValueFrame,POSITION_OF_CRC_IN_FRAME);
		WriteNewValueFrame[POSITION_OF_CRC_IN_FRAME] = (uint8_t)(NewValueUint16 & 0xFFu);
		WriteNewValueFrame[POSITION_OF_CRC_IN_FRAME+1] = (uint8_t)(NewValueUint16 >> 8);
	}

	NumberOfSentBytes = write(SerialPortHandler, FrameInfoTable[PresentOrder].outgoingFramePtr, FrameInfoTable[PresentOrder].outgoingFrameLength);
	if (-1 == NumberOfSentBytes) {
		close(SerialPortHandler);
		SerialPortHandler = -1;
		return;
	}
}

bool TransmissionChannel::isOpen(void){
	if (-1 != SerialPortHandler){
		return true;
	}
	return false;
}

uint8_t TransmissionChannel::getPhisicalIdOfPowerSupply(void){
	return (uint8_t)ModbusRegisters[MODBUS_ADDRES_POWER_SOURCE_ID];
}

PoweringDownActionsClass TransmissionChannel::drivePoweringDownStateMachine( PoweringDownStatesClass *NewPoweringDownStatePtr,
		bool PossibilityOfPsuShuttingdown, uint8_t PreviewedOrder )
{

#if DEBUG_POWERING_DOWN_STATE_MACHINE
	printf( "drivePoweringDownStateMachine( %03d PoweringDownCounter=%2d %c )\n", PreviewedOrder, PoweringDownCounter, PossibilityOfPsuShuttingdown?'T':'F' );
#endif

	if (PoweringDownCounter < 0){
		if (PossibilityOfPsuShuttingdown){
			if (RTU_ORDER_DELAYED_POWER_OFF == PreviewedOrder){
				PoweringDownCounter = 0;	// start counting
				*NewPoweringDownStatePtr = PoweringDownStatesClass::CURRENT_DECELERATING;
				return PoweringDownActionsClass::NEW_STATE_PLACE_CURRENT_ZEROING;
			}
		}
	}
	else{
		if (PossibilityOfPsuShuttingdown){
			if (PoweringDownCounter < POWERING_DOWN_TIMEOUT){
				PoweringDownCounter++;
				*NewPoweringDownStatePtr = PoweringDownStatesClass::CURRENT_DECELERATING;
				if (ModbusRegisters[MODBUS_ADDRES_CURRENT_FILTERED] <= POWERING_DOWN_CURRENT_LIMIT){
					return PoweringDownActionsClass::NEW_STATE_PLACE_POWER_OFF;
				}
			}
			else{
				if (PoweringDownCounter < POWERING_DOWN_COUNTER_MAX){
					PoweringDownCounter++;
				}
				if (RTU_ORDER_CANCEL_DELAYED_POWER_OFF == PreviewedOrder){
					PoweringDownCounter = -1; // stop the state machine
					*NewPoweringDownStatePtr = PoweringDownStatesClass::INACTIVE;
					return PoweringDownActionsClass::NEW_STATE_TAKE_ORDER;
				}

				*NewPoweringDownStatePtr = PoweringDownStatesClass::TIMEOUT_EXCEEDED;
				if (RTU_ORDER_DELAYED_POWER_OFF == PreviewedOrder){
					return PoweringDownActionsClass::NEW_STATE_PLACE_POWER_OFF;
				}
				if (ModbusRegisters[MODBUS_ADDRES_CURRENT_FILTERED] <= POWERING_DOWN_CURRENT_LIMIT){
					return PoweringDownActionsClass::NEW_STATE_PLACE_POWER_OFF;
				}
				if (POWERING_DOWN_TIMEOUT+1 == PoweringDownCounter){
					return PoweringDownActionsClass::SET_NEW_STATE;
				}
			}
		}
		else{
			PoweringDownCounter = -1; // stop the state machine
			*NewPoweringDownStatePtr = PoweringDownStatesClass::INACTIVE;
			if ((RTU_ORDER_DELAYED_POWER_OFF == PreviewedOrder) ||
					(RTU_ORDER_CANCEL_DELAYED_POWER_OFF == PreviewedOrder))
			{
				return PoweringDownActionsClass::NEW_STATE_TAKE_ORDER;
			}
			return PoweringDownActionsClass::SET_NEW_STATE;
		}
	}
	return PoweringDownActionsClass::NO_ACTION;
}

//........................................................................................................
// Local function definitions
//........................................................................................................

// This function opens and configures a serial port
static int configureSerialPort(const char *DeviceName){
    int FileHandler;
    struct termios PortSettings;

    FileHandler = open(DeviceName, O_RDWR | O_NOCTTY | O_SYNC);
    if (FileHandler == -1) {
        return -1;
    }

    if (tcgetattr(FileHandler, &PortSettings) != 0) {
        close(FileHandler);
        return -1;
    }

    cfsetispeed(&PortSettings, MODBUS_RTU_HARDWARE_SPEED);
    cfsetospeed(&PortSettings, MODBUS_RTU_HARDWARE_SPEED);

    PortSettings.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | IXANY);
    PortSettings.c_oflag = 0;                            // Disable output processing
    PortSettings.c_lflag = 0;                            // Mode 'raw'

    PortSettings.c_cflag &= ~(CSIZE | CSTOPB | PARODD | CRTSCTS);
    PortSettings.c_cflag |= (CS8 | PARENB | CREAD | CLOCAL);

    PortSettings.c_cc[VMIN]  = 0;                        // Minimum number of bytes to read
    PortSettings.c_cc[VTIME] = 0;                        // Timeout in tenth of a second

    // apply the configuration
    if (tcsetattr(FileHandler, TCSANOW, &PortSettings) != 0) {
        close(FileHandler);
        return -1;
    }

    return FileHandler;
}

// This function reads a packet of bytes from the serial port
static int16_t receiveResponse(int FileHandler, uint8_t *FrameBuffer, uint8_t ExpectedNumberOfBytes) {
    ssize_t ReceivedBytes1, ReceivedBytes2;

    assert(FileHandler != -1);
    assert(ExpectedNumberOfBytes <= MODBUS_FRAME_SIZE_MAX);

	ReceivedBytes1 = read(FileHandler, FrameBuffer, ExpectedNumberOfBytes);
#if FRAME_DEBUGGING
	printf(" <%2d", (int16_t)ReceivedBytes1);
#endif
    if (ReceivedBytes1 < 0) {
#if FRAME_DEBUGGING
    	printf("> ");
#endif
        return -1;
    }
    if(ReceivedBytes1 < (ssize_t)ExpectedNumberOfBytes){
    	ReceivedBytes2 = read(FileHandler, FrameBuffer+ReceivedBytes1, ExpectedNumberOfBytes-(uint8_t)ReceivedBytes1);
#if FRAME_DEBUGGING
        printf(" %d> ", (int16_t)ReceivedBytes2);
#endif
        if (ReceivedBytes2 < 0) {
            return -1;
        }
        return (int16_t)(ReceivedBytes1+ReceivedBytes2);
    }
#if FRAME_DEBUGGING
    printf("> ");
#endif
    return (int16_t)ReceivedBytes1;
}

// This function calculates crc16 of Modbus type for a given frame
static uint16_t crc16( const uint8_t *Buffer, uint8_t Length ){
	uint16_t crc;
	uint8_t i;
	uint8_t bit;

	crc = 0xFFFFu;
	for( i = 0; i < Length; i++ ){
		crc ^= (uint16_t)Buffer[i];
		for( bit = 0; bit < 8; bit++ ){
			if((crc & 0x0001u) != 0u){
				crc >>= 1;
				crc ^= 0xA001u;
			}
			else{
				crc >>= 1;
			}
		}
	}
	return crc;
}

//........................................................................................................
