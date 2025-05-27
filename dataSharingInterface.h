// dataSharingInterface.h

#ifndef DATASHARINGINTERFACE_H_
#define DATASHARINGINTERFACE_H_

#include <inttypes.h>
#include <string>
#include "multiChannel.h"
#include "modbusRtuMaster.h"

//.................................................................................................
// Definitions of types
//.................................................................................................

// This class is used to exchange data between ChannelGuiGroup and TransmissionChannel, which are in different threads
class DataSharingInterface{
private:
	uint16_t CopiedRegisters[MODBUS_TCP_SECTOR_SIZE];	// Registers 0 .. MODBUS_ADDRES_VOLTAGE_STD_DEVIATION can only be modified by the lower layer;
													// other registers are copies of certain variables declared below
	uint16_t PerMilleError;							// Can only be modified by the lower layer
	uint16_t MaxErrorSequence;						// Can only be modified by the lower layer
	CommunicationStatesClass CommunicationState;	// Can only be modified by the lower layer
	bool TransmissionAcknowledged;					// Can only be modified by the lower layer
	bool IsPowerSwitchOn;							// Can only be modified by the lower layer
	bool IsPsuPhysicalIdCompatibile;				// Can only be modified by the lower layer
	LastFrameErrorClass LastFrameError;				// Can only be modified by the lower layer
	PoweringDownStatesClass PoweringDownState;		// Can only be modified by the lower layer

	uint16_t PowerSupplyUnitId;						// is modified only once while the configuration file is being read
	std::string* DescriptionPtr;					// is modified only once while the configuration file is being read
	std::string* NameOfPortPtr;						// is modified only once while the configuration file is being read

	uint8_t OrderCode;								// Can only be set by the higher layer and reset by the lower layer
	uint16_t OrderNewValue;							// Can only be set by the higher layer and reset by the lower layer

public:
	void initialize();
	CommunicationStatesClass getStateOfCommunication();
	void loadModbusRtuData( CommunicationStatesClass StateOfChannel, uint16_t* RegistersPtr, LastFrameErrorClass NewLastFrameError,
			uint16_t NewPerMilleError, uint16_t NewMaxErrorSequence, bool AcknowledgementOfTransmission );

	bool isNewOrder();
	bool isNewPrimitiveOrder();
	void placeNewOrder( uint8_t Order, uint16_t NewValue );
	uint8_t takeOrder( uint16_t* ValuePtr );
	uint8_t takePrimitiveOrder( uint16_t* ValuePtr );
	uint8_t previewOrder();
	void resetOrderCode();
	void exportModbusRegisters( uint16_t* DestinationPtr );

	void loadModbusTcpData( uint8_t* InputBuffer );
	bool getPowerSwitchState();
	void setPortState( bool CurrentState );
	uint16_t getModbusRegister( uint8_t Offset );
	void setDescription( std::string* NewDescriptionPtr );
	std::string* getDescription();
	std::string* getNameOfPortPtr();
	void setNameOfPortPtr( std::string* NewNamePtr );
	void setPowerSupplyUnitId( uint16_t NewValue );
	uint16_t getPowerSupplyUnitId();
	LastFrameErrorClass getLastFrameError();
	uint16_t getPerMilleError();
	uint16_t getMaxErrorSequence();
	bool getTransmissionAcknowledgement();

	PoweringDownStatesClass getPoweringDownState();
	void setPoweringDownState( PoweringDownStatesClass NewPoweringDownState );
};

//...............................................................................................
// Global variables
//...............................................................................................

// This array is used to exchange data between different threads;
// the array is used in the peripheral thread (both in 'local computer' mode and 'remote computer' mode).
extern DataSharingInterface TableOfSharedDataForLowLevel[MAX_NUMBER_OF_SERIAL_PORTS];

// This array is equivalent to TableOfSharedDataForLowLevel;
// this array is used in the main FLTK thread
extern DataSharingInterface TableOfSharedDataForGui[MAX_NUMBER_OF_SERIAL_PORTS];

//.................................................................................................
// Global function prototypes
//.................................................................................................

// This function transfers information about updating the Modbus RTU register containing the setpoint to GUI.
// The information is needed in to protect against fast multiclicking of the buttons '+1A' '-0.1A' ... '-1A'
void multiclickCountdown(void);

#endif /* DATASHARINGINTERFACE_H_ */
