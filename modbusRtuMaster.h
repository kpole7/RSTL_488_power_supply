// modbusRtuMaster.h

#ifndef MODBUSRTUMASTER_H_
#define MODBUSRTUMASTER_H_

#include <string>
#include <inttypes.h>
#include <assert.h>
#include <time.h>

#include "modbusTcpSlave.h"
#include "multiChannel.h"

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define MODBUS_ADDRES_REQUIRED_STATUS		0
#define MODBUS_ADDRES_REQUIRED_VALUE		1
#define MODBUS_ADDRES_POWER_SOURCE_ID		2
#define MODBUS_ADDRES_SLAVE_STATUS			3
#define MODBUS_ADDRES_CURRENT_MEAN			4
#define MODBUS_ADDRES_CURRENT_MEDIAN		5
#define MODBUS_ADDRES_CURRENT_FILTERED		6
#define MODBUS_ADDRES_CURRENT_PEAK_TO_PEAK	7
#define MODBUS_ADDRES_CURRENT_STD_DEVIATION	8
#define MODBUS_ADDRES_VOLTAGE_MEAN			9
#define MODBUS_ADDRES_VOLTAGE_MEDIAN		10
#define MODBUS_ADDRES_VOLTAGE_FILTERED		11
#define MODBUS_ADDRES_VOLTAGE_PEAK_TO_PEAK	12
#define MODBUS_ADDRES_VOLTAGE_STD_DEVIATION	13

// this mask refers to both the required and received power on/off state
#define MASK_OF_POWER_ON_OFF_BIT			1

// these masks refer to the slave state
#define MASK_OF_LOCAL_REMOTE_BIT			2
#define MASK_OF_EXT_ERROR_BIT				4
#define MASK_OF_SUM_ERROR_BIT				8

// This is the frequency of querying the interface for the status of the power current source
// Some frequencies were tested: 4Hz, 8Hz, 16Hz
#define TIME_SYNCHRONIZATION_FREQUENCY		4

#define TRANSMISSION_ERRORS_TABLE			16

#define COMMUNICATION_WARNING_TOLERANCE	(1*TIME_SYNCHRONIZATION_FREQUENCY)
#define COMMUNICATION_ERRORS_TOLERANCE	(4*TIME_SYNCHRONIZATION_FREQUENCY)


#define DEBUG_POWERING_DOWN_STATE_MACHINE	0


static_assert(COMMUNICATION_WARNING_TOLERANCE < COMMUNICATION_ERRORS_TOLERANCE, "assert: COMMUNICATION_WARNING_TOLERANCE");
static_assert(COMMUNICATION_ERRORS_TOLERANCE <= 255, "assert: COMMUNICATION_ERRORS_TOLERANCE");

//.................................................................................................
// Definitions of types
//.................................................................................................

// This class relates to the status of the communication channel
enum class CommunicationStatesClass{
	PORT_NOT_OPEN		= 0,
	WRONG_PHYSICAL_ID	= 1,
	PERMANENT_ERRORS	= 2,
	TEMPORARY_ERRORS	= 3,
	HEALTHY				= 4		// there is a possibility of PSU physical Id incompatibility
};

enum class LastFrameErrorClass{
	UNSPECIFIED			= 0,
	NO_RESPONSE			= 1,
	NOT_COMPLETE_FRAME	= 2,
	BAD_CRC				= 3,
	OTHER_FRAME_ERROR	= 4,
	PERFECTION			= 5
};

enum class PoweringDownStatesClass{
	INACTIVE				= 0,
	CURRENT_DECELERATING	= 1,
	TIMEOUT_EXCEEDED		= 2
};

enum class PoweringDownActionsClass{
	NO_ACTION							= 0,
	SET_NEW_STATE						= 1,
	NEW_STATE_TAKE_ORDER				= 2,
	NEW_STATE_PLACE_CURRENT_ZEROING		= 3,
	NEW_STATE_PLACE_POWER_OFF			= 4
};

class TransmissionErrorsMonitor {
private:
	uint32_t TransmissionErrorsHistory[TRANSMISSION_ERRORS_TABLE]; // This is a bit field, where '1' = transmission error, '0' = correct response
	uint16_t TransmissionErrorsHead;	// bit index, where a current value will be written
	uint16_t TransmissionErrorsValue;	// percentage of errors
	uint16_t TransmissionErrorsSequence;// the longest sequence of errors
public:
	TransmissionErrorsMonitor();
	void addSampleAndCalculateStatistics(bool New);
	uint16_t getErrorPerMille(void);
	uint16_t getErrorMaxSequence(void);
};

// This class is associated with each power supply.
// It represents the state of the serial link and the state of the power supply itself.
// The number of objects of this type is specified in the configuration file.
class TransmissionChannel {
private:
	uint16_t PowerSupplyExpectedId;
	std::string PortName;
	std::string Descriptor;
	int SerialPortHandler;
	uint8_t CommunicationConsecutiveErrors;
	bool TransmissionAcknowledgement;
	LastFrameErrorClass FrameLastError;
	uint16_t ModbusRegisters[MODBUS_RTU_REGISTERS_AREA];
	TransmissionErrorsMonitor CommunicationMonitor;
	uint8_t PresentOrder;

	// positive number: counting (from 0 upwards) from pressing the power supply shutdown button;
	// negative number: inactive status
	int16_t PoweringDownCounter;
public:
	TransmissionChannel();
	~TransmissionChannel();
	void open( int ChannelId );
	void singleInquiryOfSlave( int ChannelId );
	bool isOpen(void);
	uint8_t getPhisicalIdOfPowerSupply(void);
	PoweringDownActionsClass drivePoweringDownStateMachine( PoweringDownStatesClass *NewPoweringDownStatePtr,
			bool PossibilityOfPsuShuttingdown, uint8_t PreviewedOrder );

	friend uint8_t configurationFileParsing(void);
};

#endif // MODBUSRTUMASTER_H_
