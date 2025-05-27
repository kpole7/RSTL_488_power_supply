// dataSharingInterface.cpp
//
// Threads: multi-thread

#include <string.h>
#include <pthread.h>
#include "dataSharingInterface.h"
#include "modbusRtuMaster.h"
#include <iostream>

//.................................................................................................
// Global variables
//.................................................................................................

// This variable is initialized while the configuration file is read
// IsModbusTcpSlave equals 1: Modbus TCP slave and Modbus RTU master are active
// IsModbusTcpSlave equals 0: Modbus TCP master is active and Modbus RTU is inactive
// Threads: the variable is modified only once in the peripheral thread
uint8_t IsModbusTcpSlave;

// This is Modbus TCP port number.
// This variable is set when reading the configuration file
// Threads: the variable is modified only once in the peripheral thread
uint16_t TcpPortNumber;

// The slave TCP address in text form, for instance "192.168.1.100" (used when Modbus TCP master is active)
// Threads: the variable is modified only once in the peripheral thread
char TcpSlaveAddress[20];

// If IsModbusTcpSlave == 1, then
//     if the user selects 'Local control' (using the GUI button), ControlFromGuiHere is set to '1' (the default value)
//     in the other case ControlFromGuiHere is cleared
// If IsModbusTcpSlave == 0, then
//     if the Modbus TCP master receives information that the TCP server (slave) has remote control set, ControlFromGuiHere is set to '1'
//     in the other case ControlFromGuiHere is cleared (the default value)
// Threads: main thread
uint8_t ControlFromGuiHere;

// This array is used to exchange data between different threads;
// the array is used in the peripheral thread (both in 'local computer' mode and 'remote computer' mode).
DataSharingInterface TableOfSharedDataForLowLevel[MAX_NUMBER_OF_SERIAL_PORTS];

// This array is equivalent to TableOfSharedDataForLowLevel; this array is used in the main FLTK thread
DataSharingInterface TableOfSharedDataForGui[MAX_NUMBER_OF_SERIAL_PORTS];

// This array is equivalent to TableOfSharedDataForLowLevel; this array is mainly used in the Modbus TCP server
// The array consists of sectors; the first sector contains information:
//   - whether the local computer has taken control, or has passed control to a remote computer;
//   - how many Modbus RTU channels there are;
//   - Modbus TCP server identification label
// the remaining sectors contain information about individual power supplies
uint16_t TableOfSharedDataForTcpServer[MAX_NUMBER_OF_SERIAL_PORTS+1][MODBUS_TCP_SECTOR_SIZE];

//.................................................................................................
// Local variables
//.................................................................................................

static std::string DummyPortName = "szeregowy";

//.................................................................................................
// Global function definitions
//.................................................................................................

void DataSharingInterface::initialize(){
	IsPowerSwitchOn = false;
	OrderCode = RTU_ORDER_NONE;
	OrderNewValue = 0;
	CommunicationState = CommunicationStatesClass::PORT_NOT_OPEN;
	PoweringDownState = PoweringDownStatesClass::INACTIVE;
	PowerSupplyUnitId = 0xFFFu;
	IsPsuPhysicalIdCompatibile = false;
	NameOfPortPtr = &DummyPortName;
}

CommunicationStatesClass DataSharingInterface::getStateOfCommunication(){
	return CommunicationState;
}

void DataSharingInterface::loadModbusRtuData( CommunicationStatesClass StateOfChannel, uint16_t* RegistersPtr,
		LastFrameErrorClass NewLastFrameError, uint16_t NewPerMilleError, uint16_t NewMaxErrorSequence, bool AcknowledgementOfTransmission )
{
	CommunicationState = StateOfChannel;

	if (RegistersPtr != nullptr){
		for (uint8_t J=0; J < MODBUS_RTU_REGISTERS_AREA; J++){
			CopiedRegisters[J] = RegistersPtr[J];
		}
		IsPowerSwitchOn = ((CopiedRegisters[MODBUS_ADDRES_SLAVE_STATUS] & MASK_OF_POWER_ON_OFF_BIT) != 0);

		IsPsuPhysicalIdCompatibile = (CopiedRegisters[MODBUS_ADDRES_POWER_SOURCE_ID] == PowerSupplyUnitId);
	}

	if ((CommunicationStatesClass::HEALTHY == StateOfChannel) && !IsPsuPhysicalIdCompatibile){
		CommunicationState = CommunicationStatesClass::WRONG_PHYSICAL_ID;
	}

	PerMilleError = NewPerMilleError;
	CopiedRegisters[MODBUS_TCP_ADDRESS_PERMILLE_ERROR] = PerMilleError;

	MaxErrorSequence = NewMaxErrorSequence;
	CopiedRegisters[MODBUS_TCP_ADDRESS_MAX_SEQUENCE] = MaxErrorSequence;

	TransmissionAcknowledged = AcknowledgementOfTransmission;
	CopiedRegisters[MODBUS_TCP_ADDRESS_COMMUNICATION_STATE] = (((uint16_t)CommunicationState) << 8)
			+ (TransmissionAcknowledged? 1 : 0);
	CopiedRegisters[MODBUS_TCP_ADDRESS_IS_POWER_ON] = (IsPowerSwitchOn? 0x100u : 0) + (IsPsuPhysicalIdCompatibile? 1 : 0);

	if (LastFrameErrorClass::UNSPECIFIED != NewLastFrameError){
		LastFrameError = NewLastFrameError;
	}
	CopiedRegisters[MODBUS_TCP_ADDRESS_LAST_FRAME_ERROR] &= (uint16_t)0xFF00u;
	CopiedRegisters[MODBUS_TCP_ADDRESS_LAST_FRAME_ERROR] |= (uint16_t)LastFrameError;
	CopiedRegisters[MODBUS_TCP_ADDRESS_EXPECTED_ID] = PowerSupplyUnitId;
}

bool DataSharingInterface::isNewOrder(){
	if (RTU_ORDER_NONE != OrderCode){
		return true;
	}
	return false;
}

bool DataSharingInterface::isNewPrimitiveOrder(){
	if (RTU_PRIMITIVE_ORDER_TOTAL_NUMBER > OrderCode){
		return true;
	}
	return false;
}

void DataSharingInterface::placeNewOrder( uint8_t Order, uint16_t NewValue ){
#if 0
	if ((void*)this == (void*)&TableOfSharedDataForLowLevel[4]){
		std::cout << " placeNewOrder [low]" << (int)OrderCode << " -> " << (int)Order << std::endl;
	}
	else if ((void*)this == (void*)&TableOfSharedDataForGui[4]){
		std::cout << " placeNewOrder [gui]" << (int)OrderCode << " -> " << (int)Order << std::endl;
	}
	else{
		std::cout << " placeNewOrder [???]" << (int)OrderCode << " -> " << (int)Order << std::endl;
	}
#endif
	OrderCode = Order;
	OrderNewValue = NewValue;
}

uint8_t DataSharingInterface::takeOrder( uint16_t* ValuePtr ){
#if 0
	if ((void*)this == (void*)&TableOfSharedDataForLowLevel[4]){
		std::cout << " takeOrder [low]" << (int)OrderCode << " -> " << (int)RTU_ORDER_NONE << std::endl;
	}
	else if ((void*)this == (void*)&TableOfSharedDataForGui[4]){
		std::cout << " takeOrder [gui]" << (int)OrderCode << " -> " << (int)RTU_ORDER_NONE << std::endl;
	}
	else{
		std::cout << " takeOrder [???]" << (int)OrderCode << " -> " << (int)RTU_ORDER_NONE << std::endl;
	}
#endif
	uint8_t ReturnValue = OrderCode;
	*ValuePtr = OrderNewValue;
	OrderCode = RTU_ORDER_NONE;
	return ReturnValue;
}

uint8_t DataSharingInterface::takePrimitiveOrder( uint16_t* ValuePtr ){
	uint8_t ReturnValue = OrderCode;
	if (ReturnValue >= RTU_PRIMITIVE_ORDER_TOTAL_NUMBER){
		ReturnValue = RTU_ORDER_NONE;
	}
	*ValuePtr = OrderNewValue;
	OrderCode = RTU_ORDER_NONE;
	return ReturnValue;
}

uint8_t DataSharingInterface::previewOrder(){
	return OrderCode;
}

void DataSharingInterface::resetOrderCode(){
	OrderCode = RTU_ORDER_NONE;
	OrderNewValue = 0;
}

void DataSharingInterface::exportModbusRegisters( uint16_t* DestinationPtr ){
	memcpy( DestinationPtr, &CopiedRegisters[0], sizeof(CopiedRegisters) );
}

void DataSharingInterface::loadModbusTcpData( uint8_t* InputBuffer ){
	uint8_t J;
	for (J=0; J < MODBUS_TCP_SECTOR_READING_SIZE; J++){		// The last two registers remain unchanged (order code + order value)
		CopiedRegisters[J] = (((uint16_t)InputBuffer[2*J]) << 8) + (uint16_t)InputBuffer[2*J+1];
	}
	PerMilleError = CopiedRegisters[MODBUS_TCP_ADDRESS_PERMILLE_ERROR];
	MaxErrorSequence = CopiedRegisters[MODBUS_TCP_ADDRESS_MAX_SEQUENCE];
	CommunicationState = (CommunicationStatesClass)(CopiedRegisters[MODBUS_TCP_ADDRESS_COMMUNICATION_STATE] >> 8);
	TransmissionAcknowledged   = (0 != (CopiedRegisters[MODBUS_TCP_ADDRESS_COMMUNICATION_STATE] & 0xFFu  ))? true : false;
	IsPowerSwitchOn            = (0 != (CopiedRegisters[MODBUS_TCP_ADDRESS_IS_POWER_ON]         & 0xFF00u))? true : false;
	IsPsuPhysicalIdCompatibile = (0 != (CopiedRegisters[MODBUS_TCP_ADDRESS_IS_POWER_ON]         & 0xFFu  ))? true : false;
	LastFrameError    = (LastFrameErrorClass)    (CopiedRegisters[MODBUS_TCP_ADDRESS_LAST_FRAME_ERROR] & 0xFFu);
	PoweringDownState = (PoweringDownStatesClass)(CopiedRegisters[MODBUS_TCP_ADDRESS_LAST_FRAME_ERROR] >> 8);
	PowerSupplyUnitId = CopiedRegisters[MODBUS_TCP_ADDRESS_EXPECTED_ID];
}

bool DataSharingInterface::getPowerSwitchState(){
	return IsPowerSwitchOn;
}

uint16_t DataSharingInterface::getModbusRegister( uint8_t Offset ){
	return CopiedRegisters[Offset];
}

void DataSharingInterface::setDescription( std::string* NewDescriptionPtr ){
	DescriptionPtr = NewDescriptionPtr;
}

std::string* DataSharingInterface::getDescription(){
	return DescriptionPtr;
}

std::string* DataSharingInterface::getNameOfPortPtr(){
	return NameOfPortPtr;
}

void DataSharingInterface::setNameOfPortPtr( std::string* NewNamePtr ){
	NameOfPortPtr = NewNamePtr;
}

void DataSharingInterface::setPowerSupplyUnitId( uint16_t NewValue ){
	PowerSupplyUnitId = NewValue;
}

uint16_t DataSharingInterface::getPowerSupplyUnitId(){
	return PowerSupplyUnitId;
}

LastFrameErrorClass DataSharingInterface::getLastFrameError(){
	return LastFrameError;
}

uint16_t DataSharingInterface::getPerMilleError(){
	return PerMilleError;
}

uint16_t DataSharingInterface::getMaxErrorSequence(){
	return MaxErrorSequence;
}

bool DataSharingInterface::getTransmissionAcknowledgement(){
	return TransmissionAcknowledged;
}

PoweringDownStatesClass DataSharingInterface::getPoweringDownState(){
	return PoweringDownState;
}

void DataSharingInterface::setPoweringDownState( PoweringDownStatesClass NewPoweringDownState ){
	PoweringDownState = NewPoweringDownState;
	CopiedRegisters[MODBUS_TCP_ADDRESS_LAST_FRAME_ERROR] &= (uint16_t)0x00FFu;
	CopiedRegisters[MODBUS_TCP_ADDRESS_LAST_FRAME_ERROR] |= ((uint16_t)NewPoweringDownState << 8);
}

