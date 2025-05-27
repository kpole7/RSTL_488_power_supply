// multiChannel.h

#ifndef MULTICHANNEL_H_
#define MULTICHANNEL_H_

#include <inttypes.h>
#include <time.h>
#include <string>

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define EXITING_COUNTDOWN				5

#define CONFIGURATION_FILE_NAME			"powerSource200A_Svedberg.cfg"

#define CHANNEL_DESCRIPTION_MAX_LENGTH	100

//...............................................................................................
// Global variables
//...............................................................................................

// This variable is used for time synchronization
extern uint8_t FractionOfSecond;

extern bool IsExiting;

// This variable is used to locate the configuration file
extern std::string* ConfigurationFilePathPtr;

// This variable is set if there is argument "-v" or "--verbose"
extern bool VerboseMode;

//.................................................................................................
// Global function prototypes
//.................................................................................................

// Peripheral communication thread function
void peripheralThread(void);

void intializeSharedData(void);

// This function loads the configuration file and allocates an array of objects of type TransmissionChannel
// It returns 1 on success, and 0 on failure
uint8_t configurationFileParsing(void);

// This function is designed to be called several times per second to read information
// about the status of the power supply unit and write a possible write command;
// the function works as a Modbus RTU master
void communicateAllPowerSources( void );

// This function supports the process of shutting down power supplies for multiple channels simultaneously.
// If a power down order comes in from the user, this function transmits a current zeroing order
// to the power supply. It then waits until the current drops to a sufficiently low value, at which time
// it turns off the power switch. If there is a timeout, the function transmits information to the user.
// If the user decides to turn off, or the current drops to a low enough value while waiting for the user's
// response, then it turns off the power switch.
void poweringDownTimingForAll( void );

// This function implements something like time ticks; see definition of TIME_SYNCHRONIZATION_FREQUENCY
void waitForSynchronization(void);

// This function synchronizes data between:
// TableOfSharedDataForLowLevel
// TableOfSharedDataForGui
// TableOfSharedDataForTcpServer
void synchronizeDataAcrossThreads(void);

#endif // MULTICHANNEL_H_
