// powerSource200A_Svedberg.cpp
//
// 1.	The application can operate in two different modes: 'local computer' and 'remote computer'.
//		This basic concept is shown in applicationFlowDiagram_v2.06.pdf. The application's mode of operation is
//		determined by the configuration file. An application running in 'local computer' mode allows the user
//		to choose whether the control is on the 'local computer' side or on the 'remote computer' side.
//		A button at the top of the window is used for this.
// 2.	The application runs either 2 or 3 threads, depending on the mode.
// 2.1.	The GUI is handled by the main thread.
// 2.2.	If the application is running in 'local computer' mode, the peripheral thread monitors
//		the status of the power supplies (and the status of the serial links) and passes on any orders
//		from the user to the power supplies. In 'remote computer' mode, the peripheral thread
//		communicates with the 'local computer' via Modbus TCP (and collects information about the PSUs).
// 2.3.	The TCP server (slave) thread is active only in 'local computer' mode and is only used
//		to handle transmission via Modbus TCP.
//
//		PSU = power supply unit

#include <assert.h>
#include <thread>
#include <iostream>
#include <chrono>
#include <limits.h>  // for PATH_MAX
#include <libgen.h>  // for dirname
#include <cstdlib>   // for realpath

#include "multiChannel.h"
#include "graphicalUserInterface.h"
#include "dataSharingInterface.h"
#include "modbusTcpMaster.h"
#include "modbusTcpSlave.h"

//.................................................................................................
// Global variables
//.................................................................................................

// This variable is set if there is argument "-v" or "--verbose" in command line
bool VerboseMode;

// This variable is used to locate the configuration file
std::string* ConfigurationFilePathPtr;

// This variable is set when the Modbus TCP server starts (that is when the new thread starts and the socket is created)
bool ActiveModbusTcpServer(false);

WindowEscProof* ApplicationWindow;

//.................................................................................................
// Local variables
//.................................................................................................

static bool ExitingFlag(false);

//.................................................................................................
// Local function prototypes
//.................................................................................................

// Window close event is handled here
static void onMainWindowCloseCallback(Fl_Widget *Widget, void *Data);

// Operations before closing the application
static void exitProcedure(void);

// The function searches for the directory where the executable file is located
static int determineApplicationPath( char* Argv0 );

//.................................................................................................

int main(int argc, char** argv) {

	VerboseMode = false;
	ControlFromGuiHere = 1;

    for (int J = 1; J < argc; J++) {
        std::string Argument = argv[J];
        if (Argument == "-v" || Argument == "--verbose") {
        	VerboseMode = true;
        	std::cout << "Tryb \"verbose\"" << std::endl;
        }
        else {
            std::cout << "Nieznany argument: " << Argument << std::endl;
            return -1;
        }
    }

	if (0 != determineApplicationPath( argv[0] )){
		return -1;
	}

	if (VerboseMode){
    	std::cout << " Wersja   " << TcpSlaveIdentifier << std::endl;
	}

    // Main window of the application
	ApplicationWindow = new WindowEscProof(MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT_MAX, "???" );
	ApplicationWindow->begin();
	ApplicationWindow->color( FL_WHITE );
    ApplicationWindow->callback(onMainWindowCloseCallback);	// Window close event is handled
    updateMainApplicationLabel( nullptr );

    initializeMainWindowWidgets();
    intializeSharedData();
    initializeWidgetsOfChannels();

    ApplicationWindow->end();
    ApplicationWindow->show();

    Fl::lock();  // Enable multi-threading support in FLTK; register a callback function for Fl::awake()
    std::thread(peripheralThread).detach();
    return Fl::run();
}

//.................................................................................................
// Function definitions
//.................................................................................................

// Window close event is handled here
static void onMainWindowCloseCallback(Fl_Widget *Widget, void *Data) {
	(void)Widget; // intentionally unused
	(void)Data; // intentionally unused

	exitProcedure();
	exit(0); // exit from the application
}

// Operations before closing the application
static void exitProcedure(void){
	if (ActiveModbusTcpServer){
		closeModbusTcpSlave();
		ActiveModbusTcpServer = false;
	}

	closeTcpClient(); // this function checks dependencies

	ExitingFlag = true;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));

	printf("\nKoniec programu\n");
}

// The function searches for the directory where the executable file is located
static int determineApplicationPath( char* Argv0 ){
    char Path[PATH_MAX];
    ConfigurationFilePathPtr = nullptr;

    if (realpath( Argv0, Path)) {
        static std::string MyDirectory = dirname(Path);
        ConfigurationFilePathPtr = &MyDirectory;
        if (VerboseMode){
#if 0
        	std::cout << "PATH_MAX= " << PATH_MAX << std::endl;
#endif
        	std::cout << " Katalog programu: " << MyDirectory << std::endl;
    	}
    } else {
        std::cerr << "Nie udało się uzyskać ścieżki do programu." << std::endl;
        return -1;
    }
    return 0;
}

//.................................................................................................
