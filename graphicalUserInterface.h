// graphicalUserInterface.h

#ifndef GRAPHICALUSERINTERFACE_H_
#define GRAPHICALUSERINTERFACE_H_

#include <string>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H> // to eliminate flickering
#include <FL/Fl_Button.H>
#include <FL/Fl_Float_Input.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Box.H>
#include "modbusRtuMaster.h"

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define MAIN_WINDOW_WIDTH			950
#define MAIN_WINDOW_HEIGHT_MIN		200
#define MAIN_WINDOW_HEIGHT_MAX		560

#define FIRST_GROUP_OF_WIDGETS_Y	115
#define GROUPS_OF_WIDGETS_SPACING	55

#define ORDINARY_TEXT_FONT			FL_HELVETICA
#define ORDINARY_TEXT_SIZE			14
#define LARGE_TEXT_FONT				FL_HELVETICA_BOLD
#define LARGE_TEXT_SIZE				16

#define DESCRIPTION_COLOR			0x36u
#define PHYSICAL_ID_COLOR			0x32u
#define NORMAL_BUTTON_COLOR			0x34u
#define CIRCLE_MARK_GREEN			0x65u
#define CIRCLE_MARK_YELLOW			0x5Du
#define X_MARK_GRAY					0x34u
#define NAVY_BLUE_COLOR				0xE0u

//.................................................................................................
// Definitions of types
//.................................................................................................

// Graphic sign in a circle that corresponds to the state of a given channel
class StateMarkWidget : public Fl_Widget {
public:
    StateMarkWidget(int X, int Y, int W, int H, const char* L = nullptr) : Fl_Widget(X, Y, W, H, L) {}
    void draw() override;	// Original draw() has been modified in such a way that it gets data from TableOfUartToGuiData
    						// and displays relevant mark
};

class BoxWithBackground : public Fl_Box {
public:
	BoxWithBackground(int X, int Y, int W, int H, const char* L = 0) : Fl_Box(X, Y, W, H, L) {}
    void draw() override;
};

// Horizontal line one pixel thick
class HorizontalLineWidget : public Fl_Widget {
public:
	HorizontalLineWidget(int X, int Y, int W, int H, const char* L = nullptr) : Fl_Widget(X, Y, W, H, L) {}
    void draw() override;	// Original draw() has been modified
};

// Rectangle with lines one pixel thick
class RectangleFrameWidget : public Fl_Widget {
public:
	RectangleFrameWidget(int X, int Y, int W, int H, const char* L = nullptr) : Fl_Widget(X, Y, W, H, L) {}
    void draw() override;	// Original draw() has been modified
};

// Blank rectangle used to clear space after closing the set-point entry dialog or after closing the diagnostic data display
class BlankRectangleWidget : public Fl_Widget {
public:
	BlankRectangleWidget(int X, int Y, int W, int H, const char* L = nullptr) : Fl_Widget(X, Y, W, H, L) {}
    void draw() override;	// Original draw() has been modified
};

// This is a group of widgets related to one communication channel
class ChannelGuiGroup : public Fl_Group {
private:
	int GroupID;
	double LastAcceptedNumericValue;
    std::string* LastAcceptedValueStringPtr;
	std::string ChannelDescriptionArchived;
	BoxWithBackground * ChannelDescriptionPtr;
	BoxWithBackground * PhysicalIdPtr;
    Fl_Button* PowerOnButtonPtr;
    Fl_Button* PowerOffButtonPtr;
    Fl_Box * PowerOnOffStatePtr;
	Fl_Box * SetPointValuePtr;
    Fl_Button* SetValueButtonPtr;
	Fl_Box * ValueOfCurrentPtr;
    Fl_Box * PsuErrorLabelPtr;
    Fl_Box * ExternalErrorLabelPtr;
    Fl_Box * PsuIdIncompatibilityLabelPtr;
    Fl_Box * RemoteLocalLabelPtr;
    StateMarkWidget* StateMarkInCirclePtr;
    Fl_Button* DiagnosticsButton;
    HorizontalLineWidget* BottomLinePtr;
    Fl_Box * OnPowerDownWarning1Ptr;
    Fl_Box * OnPowerDownWarning2Ptr;
    PoweringDownStatesClass OldPowerDownState;
    uint8_t OldControlFromGuiHere;
public:
    ChannelGuiGroup(int X, int Y, int W, int H, const char* L = nullptr);
    void setGroupID( int NewValue );
    int getGroupID();
    double getLastAcceptedNumericValue();
    void setLastAcceptedNumericValue( double NewValue );
    const char* getLastAcceptedValueCString();
    void setLastAcceptedValueString( const char* Text );
    void refreshNumericValues( CommunicationStatesClass StateOfTransmission, double NewValueOfCurrent, double NewValueOfSetpoint );
    void refreshPowerOnOffLabel( CommunicationStatesClass StateOfTransmission, bool IsOn );
    void refreshStatusLabels( CommunicationStatesClass StateOfTransmission, uint16_t StateRegister );
    void updateSettingsButtonsAndPowerDownWigets( CommunicationStatesClass StateOfTransmission, PoweringDownStatesClass NewPowerDownState );
    void setDescriptionLabel();
    void refreshPhysicalID( uint16_t PhysicalIdRegister );
    void truncateDescription();
    void setBottomLineVisibility();
    void selectiveDeactivate();
    void selectiveActivate();
    void restoreInitialState();
    void setWidgetsOnPowerDownWarningActive();
    void setWidgetsOnPowerDownWarningInactive();
};

// This is Esc-proof window
class WindowEscProof : public Fl_Double_Window {
public:
	WindowEscProof(int W, int H, const char* title) : Fl_Double_Window(W, H, title) {
		size_range( MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT_MIN, MAIN_WINDOW_WIDTH, MAIN_WINDOW_HEIGHT_MAX );
	}
    int handle(int event) override;
};

// This is a group of widgets related to set-point input dialog
// There is to be only one object of this type
class SetPointInputGroup : public Fl_Group {
private:
	int16_t ChannelThatDisplaysInputValueDialog;
	Fl_Float_Input* InputDialogPtr;
	std::string* LastValidInputStringPtr;
	Fl_Button* AcceptButtonPtr;
	Fl_Button* CancelButtonPtr;
	HorizontalLineWidget* BottomLinePtr;
	Fl_Button* CloseButtonPtr;
	Fl_Button* IncreaseBy1;
	Fl_Button* IncreaseBy01;
	Fl_Button* DecreaseBy01;
	Fl_Button* DecreaseBy1;
	uint16_t OfflineSetpointValue;	// multi-click machine (relates to fast multi-click of '+1A' '-0.1A' ... '-1A' buttons)
	uint16_t MulticlickCounter;		// multi-click machine
	// if MulticlickCounter == 0 then multi-click machine is in idle state (OfflineSetpointValue is replaced
	//                                         by Modbus register with address MODBUS_ADDRES_REQUIRED_VALUE)
	// if MulticlickCounter != 0 then multi-click machine blocks reading from Modbus register with address MODBUS_ADDRES_REQUIRED_VALUE
public:
	SetPointInputGroup(int X, int Y, int W, int H, const char* L = nullptr);
	void setChannelDisplayingSetPointEntryDialog( int16_t NewValue );
	int16_t getChannelDisplayingSetPointEntryDialog();
	void openDialog( int X, int Y );
	void closeDialog();
	const char* getLastValidInputCStringPtr();
	void setLastValidInputString( const char* NewText );

	void resetMulticlick();
	uint16_t getMulticlickProofSetpointValue( uint8_t ChannelIndex );
	void setOfflineSetpointValue( uint16_t NewValue );

	friend void multiclickCountdown(void);
};

// This is a group of widgets related to diagnostics data
// There is to be only one object of this type
class DiagnosticsGroup : public Fl_Group {
private:
	int16_t ChannelThatDisplaysDiagnostics;
	Fl_Box* DiagnosticTextBoxPtr;
	HorizontalLineWidget* BottomLinePtr;
public:
	DiagnosticsGroup(int X, int Y, int W, int H, const char* L = nullptr);
	void updateDataAndWidgets();
	int16_t getChannelDisplayingDiagnostics();
	void setChannelDisplayingDiagnostics( int16_t NewValue );
	void closeGroup();
	void enableAtNewPosition( int X, int Y );
};

//.................................................................................................
// Global variables
//.................................................................................................

// This variable is set when the Modbus TCP server starts (that is when the new thread starts and the socket is created)
extern bool ActiveModbusTcpServer;

extern WindowEscProof* ApplicationWindow;
extern ChannelGuiGroup* TableOfGroupsPtr[MAX_NUMBER_OF_SERIAL_PORTS];
extern Fl_Box* LargeErrorMessage;
extern bool UpdateConfigurableWidgets;

//.................................................................................................
// Function prototypes
//.................................................................................................

void initializeMainWindowWidgets(void);

void initializeWidgetsOfChannels(void);

// The function is used for cyclic refreshing
void updateChannelWidgets(void* Data);

void displayConfigurationFileErrorMessage(void* Data);

void displayTcpConnectionErrorMessage(void* Data);

void displayChannelWidgets(void* Data);

void restoreChannelWidgets(void* Data);

void displayTcpConnectionErrorMessageAndHideChannelWidgets(void* Data);

// This function hides *SetPointInputGroupPtr, *DiagnosticsGroupPtr and *BlankRectanglePtr
void hideAuxiliaryGroups(void);

// This function sets the application label depending on the flags: IsModbusTcpSlave and ControlFromGuiHere
void updateMainApplicationLabel(void* Data);

// This function closes set-point dialog if it is open
void closeSetpointDialogIfActive(void* Data);

#endif /* GRAPHICALUSERINTERFACE_H_ */
