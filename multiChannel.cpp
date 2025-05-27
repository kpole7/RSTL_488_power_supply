// multiChannel.cpp
//
// Threads: peripheral thread

#include <fstream>
#include <iostream>
#include <string>
#include <regex>
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
#include <math.h>
#include <thread>
#include <FL/Fl.H>
#include "modbusRtuMaster.h"
#include "multiChannel.h"
#include "dataSharingInterface.h"
#include "graphicalUserInterface.h"
#include "modbusTcpMaster.h"

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#if TIME_SYNCHRONIZATION_FREQUENCY == 4
#define TIME_SYNCHRONIZATION_SHIFT		24
#endif
#if TIME_SYNCHRONIZATION_FREQUENCY == 8
#define TIME_SYNCHRONIZATION_SHIFT		23
#endif
#if TIME_SYNCHRONIZATION_FREQUENCY == 16
#define TIME_SYNCHRONIZATION_SHIFT		22
#endif

//...............................................................................................
// Global variables
//...............................................................................................

// This variable is used for time synchronization
uint8_t FractionOfSecond;

// Number of serial ports declared in the configuration file
uint8_t NumberOfChannels;

#if 0
uint8_t ExitingCounter;
#endif

bool IsExiting;

//...............................................................................................
// Local constants
//...............................................................................................

static uint8_t FractionOfSecond_Old;
static struct timespec TimeSpecification;

//...............................................................................................
// Local variables
//...............................................................................................

static TransmissionChannel TableOfTransmissionChannel[MAX_NUMBER_OF_SERIAL_PORTS];

//.................................................................................................
// Local function prototypes
//.................................................................................................

static bool possibilityOfPsuShuttingdown(uint8_t IndexOfChannel);

//.................................................................................................
// Global function definitions
//.................................................................................................

// Peripheral communication thread function
void peripheralThread(void) {
    struct timespec TimeSpecification0, TimeSpecification1;
    uint8_t J, TimeDivider, TemporaryControlFromGuiHere;
	pthread_mutex_t xLock = PTHREAD_MUTEX_INITIALIZER;

    TimeDivider = 0;

    J = configurationFileParsing();
	if (0 == J){
		Fl::awake( displayConfigurationFileErrorMessage, nullptr );
		return;
	}

	pthread_mutex_lock( &xLock );
	UpdateConfigurableWidgets = true;
	pthread_mutex_unlock( &xLock );

	synchronizeDataAcrossThreads();

	if (IsModbusTcpSlave){

		// Start yet another thread for Modbus TCP server (slave)
		J = (uint8_t)initializeModbusTcpSlave();
	    if (J != 0){
	    	pthread_mutex_lock( &xLock );
	    	ActiveModbusTcpServer = true;
	    	pthread_mutex_unlock( &xLock );
	    }
	    else{
	    	Fl::awake( displayTcpConnectionErrorMessage, nullptr );
			return;
	    }
	}
	else{
		initializeTcpClientVariables();
	}

	clock_gettime(CLOCK_REALTIME, &TimeSpecification0);

	// Displays GUI for the channels specified in the configuration file
    for (J = 0; J < NumberOfChannels; J++) {
    	Fl::awake( displayChannelWidgets, (void*)TableOfGroupsPtr[J] );
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    while (true) {
		clock_gettime(CLOCK_REALTIME, &TimeSpecification1);
		waitForSynchronization();

#if 0
		if (0 == FractionOfSecond % 2){
			printf( "\r\n%X %4ld.%03ld\t", FractionOfSecond, TimeSpecification1.tv_sec-TimeSpecification0.tv_sec,
					TimeSpecification1.tv_nsec/1000000ul );
		}
#endif

		synchronizeDataAcrossThreads();

		if (IsModbusTcpSlave){
			// support for the process of shutting down power supply units for multiple channels
			poweringDownTimingForAll();
			// Communication with the power supply units
			communicateAllPowerSources();
		}
		else{
			if (0 == TimeDivider){
				// This application works in 'remote control' or 'remote monitoring' mode
				// and communicates with a 'local computer' working as a Modbus TCP slave
				bool RefreshDescriptions = communicateTcpServer();
				if (RefreshDescriptions){
					Fl::awake( updateMainApplicationLabel, nullptr );
				    for (J = 0; J < NumberOfChannels; J++) {
				    	Fl::awake( displayChannelWidgets, (void*)TableOfGroupsPtr[J] );
				    }

					pthread_mutex_lock( &xLock );
					TemporaryControlFromGuiHere = ControlFromGuiHere;
					pthread_mutex_unlock( &xLock );
				    if (0 == TemporaryControlFromGuiHere){
				    	Fl::awake( closeSetpointDialogIfActive, nullptr );
				    }
				}
				if (NewStateOfModbusTcpInterface){
					NewStateOfModbusTcpInterface = false;

					if (ModbusTcpClientStateClass::NO_ERROR == ModbusTcpCommunicationState){
						restoreChannelWidgets( nullptr );
					}
					else{
						static uint8_t ErrorCode = (uint8_t)ModbusTcpCommunicationState;
						Fl::awake( displayTcpConnectionErrorMessageAndHideChannelWidgets, (void*)&ErrorCode );

						// to redraw ConfigurationFileErrorMessage and so on, especially when NumberOfChannels==0
						Fl::awake(updateChannelWidgets, (void*)TableOfGroupsPtr[0]);
					}
				}
			}
		}
		synchronizeDataAcrossThreads();

		if (0 == TimeDivider){
			// Sending a message to the main FLTK thread (to refresh GUI widgets)
			for( uint16_t ChannelIndex = 0; ChannelIndex < NumberOfChannels; ChannelIndex++ ){
				Fl::awake(updateChannelWidgets, (void*)TableOfGroupsPtr[ChannelIndex]);
			}
		}

		if (0 == TimeDivider){
			TimeDivider = 2;
		}
		TimeDivider--;
    }
}

void intializeSharedData(void){
    for (int J = 0; J < MAX_NUMBER_OF_SERIAL_PORTS; J++) {
    	TableOfSharedDataForLowLevel[J].initialize();
    	TableOfSharedDataForGui[J].initialize();
    }
}

// This function loads the configuration file and allocates an array of objects of type TransmissionChannel
// It returns 1 on success, and 0 on failure
uint8_t configurationFileParsing(void) {
    int LineNumber = 1;
    std::string Line;
    std::smatch Matches;
    char* TemporaryEndPtr;
    unsigned long int TemporaryLongInteger;

	NumberOfChannels = 0;

    // the configuration file is looked for in the directory where the executable is located, rather than in the working directory
	*ConfigurationFilePathPtr += "/";
	*ConfigurationFilePathPtr += CONFIGURATION_FILE_NAME;

	// Check if the configuration file exists
	std::ifstream File( ConfigurationFilePathPtr->c_str() ); // open file
    if (!File.is_open()) {
        std::cout << "Nie można otworzyć pliku: " << CONFIGURATION_FILE_NAME << std::endl;
        return 0;
    }
    if (VerboseMode){
    	std::cout << "Plik: " << CONFIGURATION_FILE_NAME << std::endl;
    }

    // Looking in the configuration file for information on what mode the application will work in
    std::regex PatternTcpSlave(R"([Tt]ryb_[Pp]racy_[Kk]omputera\s*=\s*[Ll]okalny\s*(?:#.*)?)");
    std::regex PatternTcpMaster(R"([Tt]ryb_[Pp]racy_[Kk]omputera\s*=\s*[Zz]dalny\s*(?:#.*)?)");;
    bool MatchesTcpSlave, MatchesTcpMaster;
    while (std::getline(File, Line)) {
        if (VerboseMode){
        	std::cout << "Linijka " << LineNumber << std::endl;
        }

        MatchesTcpSlave = std::regex_match(Line, Matches, PatternTcpSlave);
        if (!MatchesTcpSlave){
        	MatchesTcpMaster = std::regex_match(Line, Matches, PatternTcpMaster);
        }
        else{
        	MatchesTcpMaster = false;
        }
        if (MatchesTcpSlave || MatchesTcpMaster){
            if (VerboseMode){
            	if (MatchesTcpSlave){
                	std::cout << " Odczytano parametr tryb pracy komputera: lokalny (tj. przy zasilaczach) " << std::endl;
            	}
            	else{
                	std::cout << " Odczytano parametr tryb pracy komputera: zdalny (tj. w sterowni) " << std::endl;
            	}
            }
            LineNumber++;
        	break;
        }
        else{
            if (VerboseMode){
            	std::cout << " Nie znaleziono frazy 'tryb_pracy_komputera=lokalny' albo 'tryb_pracy_komputera=zdalny' na początku linii: [" << Line << "]" << std::endl;
            }
        }
        LineNumber++;
    }
    if (!MatchesTcpSlave && !MatchesTcpMaster){
    	std::cout << " Brak prawidłowych danych w pliku konfiguracyjnym (Modbus_TCP=...) " << std::endl;
        File.close();
    	return 0;
    }
    IsModbusTcpSlave = (MatchesTcpSlave? 1 : 0);

    pthread_mutex_t xLock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock( &xLock );
    ControlFromGuiHere = IsModbusTcpSlave; // the default value
	pthread_mutex_unlock( &xLock );

    TableOfSharedDataForTcpServer[0][TCP_SERVER_ADDRESS_IS_REMOTE_CONTROL] = 0;

    // Looking in the configuration file for information on which port to use for Modbus TCP
    std::regex PatternTcpPortNumber(R"([Nn]umer_portu_[Tt][Cc][Pp]\s*=\s*(\d+)\s*(?:#.*)?)");;
    std::string TcpPortText;
    bool MatchesTcpPortNumber;
    while (std::getline(File, Line)) {
        if (VerboseMode){
        	std::cout << "Linijka " << LineNumber << std::endl;
        }

        MatchesTcpPortNumber = std::regex_match(Line, Matches, PatternTcpPortNumber);
        if (MatchesTcpPortNumber){
        	TcpPortText = Matches[1];
        	TemporaryLongInteger = strtoul( TcpPortText.c_str(), &TemporaryEndPtr, 10 );
        	if (TemporaryLongInteger > 0xFFFFu){
            	std::cout << " Nieprawidłowe dane w pliku konfiguracyjnym (numer portu TCP)" << std::endl;
                File.close();
            	return 0;
        	}
        	TcpPortNumber = (uint16_t)TemporaryLongInteger;
            if (VerboseMode){
               	std::cout << " Odczytano parametr port Modbus TCP = " << TcpPortNumber << std::endl;
            }
            LineNumber++;
        	break;
        }
        else{
            if (VerboseMode){
            	std::cout << " Nie znaleziono numeru portu TCP w linii: [" << Line << "]" << std::endl;
            }
        }
        LineNumber++;
    }
    if (!MatchesTcpPortNumber){
    	std::cout << " Brak prawidłowych danych w pliku konfiguracyjnym (numer portu TCP) " << std::endl;
        File.close();
    	return 0;
    }

    // Further parsing depends on what mode the application is running in
    if (MatchesTcpSlave){

        // Looking in the configuration file for information on power supply units and the interfaces they use
        std::regex PatternWithDecimalId(R"([Ii][Dd]=(\d+)\s+[Pp]ort='([^']*)'\s+[Oo]pis='([^']*)'\s*(?:#.*)?)");;
        std::regex PatternWithHexId(R"([Ii][Dd]=0x([0-9a-fA-F]+)\s+[Pp]ort='([^']*)'\s+[Oo]pis='([^']*)'\s*(?:#.*)?)");;
        std::string PhysicalIdText;
        bool MatchesDecimalPattern, MatchesHexPattern;
        while (std::getline(File, Line)) {
            if (VerboseMode){
            	std::cout << "Linijka " << LineNumber << std::endl;
            }

            MatchesDecimalPattern = std::regex_match(Line, Matches, PatternWithDecimalId);
            if (!MatchesDecimalPattern){
            	MatchesHexPattern = std::regex_match(Line, Matches, PatternWithHexId);
            }
            else{
                MatchesHexPattern = false;
            }

            if (MatchesDecimalPattern || MatchesHexPattern) {
    			// matches[0] includes all matching text
                // matches[1] includes value of 'id'
                // matches[2] includes value of 'port'
                // matches[3] includes value of 'opis'
            	PhysicalIdText = Matches[1];
            	TemporaryLongInteger = strtoul( PhysicalIdText.c_str(), &TemporaryEndPtr, MatchesDecimalPattern? 10 : 16 );
            	if (TemporaryLongInteger > 0xFFu){
                	std::cout << " Nieprawidłowe dane w pliku konfiguracyjnym (numer ID zasilacza) " << std::endl;
                    File.close();
                	return 0;
            	}
            	TableOfTransmissionChannel[NumberOfChannels].PowerSupplyExpectedId = TemporaryLongInteger;

                TableOfTransmissionChannel[NumberOfChannels].PortName = Matches[2];

                TableOfTransmissionChannel[NumberOfChannels].Descriptor = Matches[3];
    			if (TableOfTransmissionChannel[NumberOfChannels].Descriptor.length() > CHANNEL_DESCRIPTION_MAX_LENGTH){ // too many anyway
    				TableOfTransmissionChannel[NumberOfChannels].Descriptor.resize( CHANNEL_DESCRIPTION_MAX_LENGTH );
    			}

    			if ((TableOfTransmissionChannel[NumberOfChannels].PowerSupplyExpectedId > 0) &&
    					(TableOfTransmissionChannel[NumberOfChannels].PowerSupplyExpectedId < 256))
    			{
    	            TableOfSharedDataForLowLevel[NumberOfChannels].setNameOfPortPtr( &TableOfTransmissionChannel[NumberOfChannels].PortName );
    				TableOfSharedDataForLowLevel[NumberOfChannels].setDescription( &TableOfTransmissionChannel[NumberOfChannels].Descriptor );
    				TableOfSharedDataForLowLevel[NumberOfChannels].setPowerSupplyUnitId( TableOfTransmissionChannel[NumberOfChannels].PowerSupplyExpectedId );

    	            if (VerboseMode){
    	                char TemporaryHexadecimalText[12];
    	                snprintf( TemporaryHexadecimalText, sizeof(TemporaryHexadecimalText)-1, " = 0x%02X",
    	                		TableOfTransmissionChannel[NumberOfChannels].PowerSupplyExpectedId );
    					std::cout << " Id: "   << TableOfTransmissionChannel[NumberOfChannels].PowerSupplyExpectedId << TemporaryHexadecimalText << std::endl;
    					std::cout << " Port: " << TableOfTransmissionChannel[NumberOfChannels].PortName << std::endl;
    					std::cout << " Opis: " << TableOfTransmissionChannel[NumberOfChannels].Descriptor << std::endl;
    	            }
    				NumberOfChannels++;
    			}
    			else{
    				std::cout << " Nieprawidłowy numer ID zasilacza w linii: " << Line << std::endl;
    			}
            }
            else {
                if (VerboseMode){
                	std::cout << " Nie znaleziono opisu portu szeregowego w linii: [" << Line << "]" << std::endl;
                }
            }
            LineNumber++;
        }

    	TableOfSharedDataForTcpServer[0][TCP_SERVER_ADDRESS_NUMBER_OF_CHANNELS] = NumberOfChannels;

        // Summary of the configuration file parsing
        if (VerboseMode){
        	std::cout << "Liczba zapamiętanych pozycji: " << (int)NumberOfChannels << std::endl;
        }
        if (0 == NumberOfChannels){
        	std::cout << " Brak prawidłowych danych w pliku konfiguracyjnym (opis portu szeregowego) " << std::endl;
            File.close();
        	return 0;
        }

        for (uint8_t J=0; J < MAX_NUMBER_OF_SERIAL_PORTS; J++ ){
        	ChannelDescriptionLength[J] = 0;
        	ChannelDescriptionTextsPtr[J] = nullptr;
        }

        uint16_t SumOfLengths = 0;
        for (uint8_t J=0; J < MAX_NUMBER_OF_SERIAL_PORTS; J++ ){
        	ChannelDescriptionLength[J] = (uint16_t)TableOfTransmissionChannel[J].Descriptor.length();
        	if (ChannelDescriptionLength[J] != 0){
        		ChannelDescriptionLength[J]++;	// space for null (termination mark)
        	}
        	if (ChannelDescriptionLength[J] % 2 == 1){
        		ChannelDescriptionLength[J]++;	// each text must be aligned to 2 bytes
        	}
        	SumOfLengths += ChannelDescriptionLength[J];
        }
        ChannelDescriptionPlainTextsPtr = new char[SumOfLengths];
        memset( ChannelDescriptionPlainTextsPtr, 0, SumOfLengths );

        uint16_t N = 0;
        for (uint8_t J=0; J < MAX_NUMBER_OF_SERIAL_PORTS; J++ ){
        	if (ChannelDescriptionLength[J] != 0){
            	ChannelDescriptionTextsPtr[J] = &ChannelDescriptionPlainTextsPtr[N];
            	N += ChannelDescriptionLength[J];
            	assert( N <= SumOfLengths );
            	memcpy( ChannelDescriptionTextsPtr[J], TableOfTransmissionChannel[J].Descriptor.c_str(), ChannelDescriptionLength[J] );
            	if (0 == ChannelDescriptionPlainTextsPtr[ N-2 ]){
            		ChannelDescriptionPlainTextsPtr[ N-1 ] = 0;
            	}
        	}
        	else{
        		ChannelDescriptionTextsPtr[J] = nullptr;
        	}
#if 0	// debugging
        	for( int M = 0; M < ChannelDescriptionLength[J]; M++ ){
        		printf("%02X ", (ChannelDescriptionTextsPtr[J])[M] );
        	}
    		printf("  [%s] ptr=%lu  Len=%u\n", ChannelDescriptionTextsPtr[J], (uint64_t)ChannelDescriptionTextsPtr[J], ChannelDescriptionLength[J] );
#endif
        }
    }
    else{
        // Looking in the configuration file for information on address of the Modbus TCP slave
        std::regex PatternTcpAddress(R"([Aa]dres_[Tt][Cc][Pp]\s*=\s*(\d+\.\d+\.\d+\.\d+)\s*(?:#.*)?)");;
        std::string TcpAddressText;
        bool MatchesTcpAddress;
        while (std::getline(File, Line)) {
            if (VerboseMode){
            	std::cout << "Linijka " << LineNumber << std::endl;
            }

            MatchesTcpAddress = std::regex_match(Line, Matches, PatternTcpAddress);
            if (MatchesTcpAddress){
            	TcpAddressText = Matches[1];
               	if (TcpAddressText.length() > 15){
                   	std::cout << " Nieprawidłowy adres TCP w linii: [" << Line << "]" << std::endl;
                   	File.close();
                   	return 0;
               	}
               	strncpy( TcpSlaveAddress, TcpAddressText.c_str(), sizeof(TcpSlaveAddress)-1 );


                if (VerboseMode){
                   	std::cout << " Odczytano parametr adres TCP = " << TcpSlaveAddress << std::endl;
                }
                LineNumber++;
            	break;
            }
            else{
                if (VerboseMode){
                	std::cout << " Nie znaleziono adresu TCP w linii: [" << Line << "]" << std::endl;
                }
            }
            LineNumber++;
        }
        if (!MatchesTcpAddress){
        	std::cout << " Brak prawidłowych danych w pliku konfiguracyjnym (adres TCP) " << std::endl;
            File.close();
        	return 0;
        }
    }

    File.close();
	return 1;
}

// This function is designed to be called several times per second to read information
// about the status of the power supply unit and write a possible write command;
// the function works as a Modbus RTU master
void communicateAllPowerSources( void ){
	uint8_t CurrentChannel;
	for( CurrentChannel=0; CurrentChannel<NumberOfChannels; CurrentChannel++ ){
		if(TableOfTransmissionChannel[CurrentChannel].isOpen()){
			TableOfTransmissionChannel[CurrentChannel].singleInquiryOfSlave( CurrentChannel );
		}
		else{
			if(0 == FractionOfSecond){
				TableOfTransmissionChannel[CurrentChannel].open( CurrentChannel );
			}
		}
	}
}

// This function supports the process of shutting down power supply units for multiple channels simultaneously.
// If a power down order comes in from the user, this function transmits a current zeroing order
// to the power supply. It then waits until the current drops to a sufficiently low value, at which time
// it turns off the power switch. If there is a timeout, the function transmits information to the user.
// If the user decides to turn off, or the current drops to a low enough value while waiting for the user's
// response, then it turns off the power switch.
void poweringDownTimingForAll( void ){
	uint8_t CurrentChannel;
	for( CurrentChannel=0; CurrentChannel<NumberOfChannels; CurrentChannel++ ){
		PoweringDownStatesClass TemporaryPoweringDownState;
		PoweringDownActionsClass NewPoweringDownAction = TableOfTransmissionChannel[CurrentChannel].drivePoweringDownStateMachine(
				&TemporaryPoweringDownState,
				possibilityOfPsuShuttingdown(CurrentChannel),
				TableOfSharedDataForLowLevel[CurrentChannel].previewOrder() );
		if (PoweringDownActionsClass::NO_ACTION != NewPoweringDownAction){

#if DEBUG_POWERING_DOWN_STATE_MACHINE
			std::cout << "poweringDownTimingForAll; CurrentChannel " << (int)CurrentChannel << " action= " <<
					(int)NewPoweringDownAction << "; state= " << (int)TemporaryPoweringDownState << std::endl;
#endif

			TableOfSharedDataForLowLevel[CurrentChannel].setPoweringDownState( TemporaryPoweringDownState );
		}
		if ((PoweringDownActionsClass::NEW_STATE_TAKE_ORDER == NewPoweringDownAction) ||
				(PoweringDownActionsClass::NEW_STATE_PLACE_CURRENT_ZEROING == NewPoweringDownAction) ||
				(PoweringDownActionsClass::NEW_STATE_PLACE_POWER_OFF == NewPoweringDownAction))
		{
			uint16_t TemporaryUint16;
			(void) TableOfSharedDataForLowLevel[CurrentChannel].takeOrder( &TemporaryUint16 );
		}
		if (PoweringDownActionsClass::NEW_STATE_PLACE_CURRENT_ZEROING == NewPoweringDownAction){
			TableOfSharedDataForLowLevel[CurrentChannel].placeNewOrder( RTU_ORDER_SET_VALUE, 0 );
		}
		if (PoweringDownActionsClass::NEW_STATE_PLACE_POWER_OFF == NewPoweringDownAction){
			TableOfSharedDataForLowLevel[CurrentChannel].placeNewOrder( RTU_ORDER_POWER_OFF, 0 );
		}
	}
}

bool possibilityOfPsuShuttingdown(uint8_t IndexOfChannel){

#if DEBUG_POWERING_DOWN_STATE_MACHINE
	printf( "possibilityOfPsuShuttingdown( I=%06.2f; status=%04X ) ",
			0.01*(double)TableOfSharedDataForLowLevel[IndexOfChannel].getModbusRegister(MODBUS_ADDRES_CURRENT_FILTERED),
			TableOfSharedDataForLowLevel[IndexOfChannel].getModbusRegister(MODBUS_ADDRES_SLAVE_STATUS) );
#endif

	if((CommunicationStatesClass::HEALTHY != TableOfSharedDataForLowLevel[IndexOfChannel].getStateOfCommunication()) &&
			(CommunicationStatesClass::TEMPORARY_ERRORS != TableOfSharedDataForLowLevel[IndexOfChannel].getStateOfCommunication()))
	{
		// if there is no updated information on the PSU
		return false;
	}
	else{
		if ( !TableOfSharedDataForLowLevel[IndexOfChannel].getPowerSwitchState() ){
			// if the power switch is off for any reason or can not be modified due to PSU errors or due to manual control
			return false;
		}
	}
	// there is a potential possibility that the PSU could be in the process of shutting down
	return true;
}

// This function implements something like time ticks; see definition of TIME_SYNCHRONIZATION_FREQUENCY
void waitForSynchronization(void){
	do{
		// 3ms; delay so as not to overload the processor core;  1/3ms = 333Hz
		usleep(3000);
		clock_gettime(CLOCK_REALTIME, &TimeSpecification);
		FractionOfSecond = (uint8_t)(TimeSpecification.tv_nsec >> TIME_SYNCHRONIZATION_SHIFT);
		FractionOfSecond = (FractionOfSecond+1) / 15;
		if(FractionOfSecond >= TIME_SYNCHRONIZATION_FREQUENCY){
			FractionOfSecond = 0;
		}
	}while(FractionOfSecond == FractionOfSecond_Old);
	FractionOfSecond_Old = FractionOfSecond;
}

// This function synchronizes data between:
// TableOfSharedDataForLowLevel
// TableOfSharedDataForGui
// TableOfSharedDataForTcpServer
void synchronizeDataAcrossThreads(void){
    for ( int J = 0; J < NumberOfChannels; J++) {
    	// Data synchronization between TableOfSharedDataForLowLevel and TableOfSharedDataForGui
    	pthread_mutex_t xLock = PTHREAD_MUTEX_INITIALIZER;
    	pthread_mutex_lock( &xLock );
    	if (0 != ControlFromGuiHere){
    		// Checking if there is a new order from the user to the power supply unit
    		if (TableOfSharedDataForGui[J].isNewOrder()){
    			uint16_t TemporaryValue;
    			uint8_t TemporaryOrder = TableOfSharedDataForGui[J].takeOrder( &TemporaryValue );
    			TableOfSharedDataForLowLevel[J].placeNewOrder( TemporaryOrder, TemporaryValue );
    		}
    	}
    	// copying data to GUI
    	memcpy( &(TableOfSharedDataForGui[J]), &(TableOfSharedDataForLowLevel[J]), sizeof(DataSharingInterface) );
    	TableOfSharedDataForGui[J].resetOrderCode(); // because memcpy overwrote TableOfSharedDataForGui[J].OrderCode with a wrong value
    	pthread_mutex_unlock( &xLock );

    	if (0 != IsModbusTcpSlave){
    		// the TCP server is active
        	// Data synchronization between TableOfSharedDataForLowLevel and TableOfSharedDataForTcpServer
        	pthread_mutex_lock( &xLock );
        	if (0 == ControlFromGuiHere){
        		// Remote control via Modbus TCP is active.
        		// Checking if there is a new order from the remote computer
        		if (RTU_ORDER_NONE != (uint8_t)(TableOfSharedDataForTcpServer[J+1][MODBUS_TCP_ADDRESS_ORDER_CODE])){
        			TableOfSharedDataForLowLevel[J].placeNewOrder(
        					(uint8_t)(TableOfSharedDataForTcpServer[J+1][MODBUS_TCP_ADDRESS_ORDER_CODE]),
							TableOfSharedDataForTcpServer[J+1][MODBUS_TCP_ADDRESS_ORDER_VALUE] );
        		}
        	}
        	// copying data to the remote computer
        	TableOfSharedDataForLowLevel[J].exportModbusRegisters( &TableOfSharedDataForTcpServer[J+1][0] );
    		TableOfSharedDataForTcpServer[J+1][MODBUS_TCP_ADDRESS_ORDER_CODE] = RTU_ORDER_NONE;
    		TableOfSharedDataForTcpServer[J+1][MODBUS_TCP_ADDRESS_ORDER_VALUE] = 0;
        	pthread_mutex_unlock( &xLock );
    	}
    }
}

//...............................................................................................

