// graphicalUserInterface.cpp
//
// Threads: main thread

#include "graphicalUserInterface.h"
#include "dataSharingInterface.h"
#include <iostream>

//.................................................................................................
// Preprocessor directives
//.................................................................................................

#define ERROR_LABELS_Y1						4
#define LOCAL_REMOTE_LABEL_Y0				17

#define CHANNEL_DESCRIPTION_X				0
#define CHANNEL_DESCRIPTION_WIDTH			160
#define PHYSICAL_ID_NUMBER_X				CHANNEL_DESCRIPTION_WIDTH
#define POWER_SWITCH_BUTTON_X				210
#define POWER_ON_OFF_STATE_X				340
#define SET_VALUE_INPUT_FIELD_X				450
#define AMPERE_UNIT_X						(SET_VALUE_INPUT_FIELD_X+65)
#define SET_VALUE_BUTTON_X					SET_VALUE_INPUT_FIELD_X
#define MEASURED_VALUE_OF_CURRENT_X			650
#define ID_INCOMPATIBILITY_LABEL_X			665
#define ERROR_LABELS_X						695
#define LOCAL_REMOTE_LABEL_X				ERROR_LABELS_X
#define STATE_MARK_IN_CIRCLE_X				850
#define SHOW_DIAGNOSTICS_BUTTON_X			910
#define HIGH_CURRENT_ON_POWER_DOWN_TEXT_X	300

#define RELATIVE_INCREAMENT_PLUS_1			0
#define RELATIVE_INCREAMENT_PLUS_01			1
#define RELATIVE_INCREAMENT_MINUS_01		2
#define RELATIVE_INCREAMENT_MINUS_1			3

#define BUTTON_BOX_SHAPE					FL_BORDER_BOX

//.................................................................................................
// Local constants
//.................................................................................................

static const char MainWindowNameInModeLocalLowercase[36]  = "Zasilacze 200A (Sterowanie lokalne)";
static const char MainWindowNameInModeRemoteLowercase[36] = "Zasilacze 200A (Sterowanie zdalne) ";
static const char MainWindowNameInModeLocalUppercase[36]  = "Zasilacze 200A (STEROWANIE LOKALNE)";
static const char MainWindowNameInModeRemoteUppercase[36] = "Zasilacze 200A (STEROWANIE ZDALNE) ";

static const char ModbusError_0[]        = "nie zarejestrowano";
static const char ModbusError_x[]        = "brak odpowiedzi   ";
static const char ModbusError_n[]        = "niekompletna ramka";
static const char ModbusError_c[]        = "błędne crc ramki  ";
static const char ModbusError_f[]        = "inny błąd ramki   ";
static const char InternalErrorMessage[] = "internal error    ";

static const char ModeUnknown[] = "Tryb  ------    ";
static const char ModeLocal[]   = "Tryb lokalny";
static const char ModeRemote[]  = "Tryb zdalny ";

static const int16_t RelativeIncreamentsTable[4] = { 328, 33, -33, -328 };

static const char TcpErrorText0[]	= " Łączenie z serwerem \n        (slave'em) Modbusa TCP      ";
static const char TcpErrorText1[]	= "   internal error    \n                                    ";
static const char TcpErrorText2[]	= "Błąd komunikacji TCP:\n               select()             ";
static const char TcpErrorText3[]	= "Błąd komunikacji TCP:\n Brak odpowiedzi w oznaczonym czasie";
static const char TcpErrorText4[]	= "Błąd komunikacji TCP:\n      Nie można utworzyć gniazda    ";
static const char TcpErrorText5[]	= "Błąd komunikacji TCP:\n        Nieprawidłowy adres IP      ";
static const char TcpErrorText6[]	= "Błąd komunikacji TCP:\n  Nie można połączyć się z serwerem ";
static const char TcpErrorText7[]	= "Błąd komunikacji TCP:\n   Nie udało się wysłać zapytania   ";
static const char TcpErrorText8[]	= "Błąd komunikacji TCP:\n       Nie odebrano odpowiedzi      ";
static const char TcpErrorText9[]	= "Błąd komunikacji TCP:\n      Niepełna ramka Modbus TCP     ";
static const char TcpErrorText10[]	= "Błąd komunikacji TCP:\n     Błąd ze slave'a Modbus TCP     ";
static const char TcpErrorText11[]	= "Błąd komunikacji TCP:\n Nieprawidłowa liczba danych w ramce";
static const char TcpErrorText12[]	= "Błąd komunikacji TCP:\n  Nieznana odpowiedź (kod funkcji)  ";
static const char TcpErrorText13[]	= "Błąd komunikacji TCP:\n Niezgodna etykieta identyfikacyjna ";
static const char TcpErrorText14[]	= "Błąd komunikacji TCP:\n Niedozwolona zmiana liczby kanałów ";
static const char TcpErrorText15[]	= "Błąd komunikacji TCP:\n     Nieprawidłowa liczba kanałów   ";
static const char TcpErrorText16[]	= "Błąd komunikacji TCP:\n  Nieprawidłowa odpowiedź na zapis  ";

static const char* TableTcpErrorTexts[17] = {
		TcpErrorText0,
		TcpErrorText1,
		TcpErrorText2,
		TcpErrorText3,
		TcpErrorText4,
		TcpErrorText5,
		TcpErrorText6,
		TcpErrorText7,
		TcpErrorText8,
		TcpErrorText9,
		TcpErrorText10,
		TcpErrorText11,
		TcpErrorText12,
		TcpErrorText13,
		TcpErrorText14,
		TcpErrorText15,
		TcpErrorText16
};

//.................................................................................................
// Global variables
//.................................................................................................

ChannelGuiGroup* TableOfGroupsPtr[MAX_NUMBER_OF_SERIAL_PORTS];

Fl_Box* LargeErrorMessage;

bool UpdateConfigurableWidgets(false);

//.................................................................................................
// Local constants
//.................................................................................................

static const char ChannelsHeaderText1[] = "Nazwa";
static const char ChannelsHeaderText2[] = "ID";
static const char ChannelsHeaderText3[] = "Kontrola";
static const char ChannelsHeaderText4[] = "Nastaw";
static const char ChannelsHeaderText5[] = "Pomiar";
static const char ChannelsHeaderText6[] = "Status zasilacza";
static const char ChannelsHeaderText7[] = "Info";

static const char *ChannelsHeaderTexts[7] = {
		ChannelsHeaderText1,
		ChannelsHeaderText2,
		ChannelsHeaderText3,
		ChannelsHeaderText4,
		ChannelsHeaderText5,
		ChannelsHeaderText6,
		ChannelsHeaderText7
};

static const uint16_t ChannelsHeaderTextX[7] = { 60, 165, 260, 455, 600, 750, 910 };

static const char TextOfComputerLabel[2][45] =
	{{" Konfiguracja\ndo połączenia\nz zasilaczami"},
	 {" Konfiguracja\n  do pracy   \n  w sterowni  "}};

static const char TextOfRemoteComputerControlButton[2][33] =
	{{"Przełącz na\nsterowanie zdalne"},
	 {"Przełącz na\nsterowanie lokalne"}};

//.................................................................................................
// Local variables
//.................................................................................................

static SetPointInputGroup* SetPointInputGroupPtr;
static DiagnosticsGroup* DiagnosticsGroupPtr;
static BlankRectangleWidget* BlankRectanglePtr;

static Fl_Box* ChannelsHeaderPtr[7];
static HorizontalLineWidget* HeaderLine1Ptr;
static HorizontalLineWidget* HeaderLine2Ptr;
static HorizontalLineWidget* HeaderLine3Ptr;

static Fl_Box* ComputerLabelPtr;

static Fl_Button* RemoteComputerControlButton;

//.................................................................................................
// Local function prototypes
//.................................................................................................

static void powerOnCallback(Fl_Widget* Widget, void* Data);

static void powerOffCallback(Fl_Widget* Widget, void* Data);

// Callback function called when the 'change' button is pressed
static void changeSetPointValueButtonCallback(Fl_Widget* Widget, void* Data);

// Callback function called when the close button is pressed in set-point dialog
static void closeSetPointDialogCallback(Fl_Widget* Widget, void* Data);

static void closeSetPointDialogInternals(void);

// Callback function called when the 'OK' button is pressed in set-point dialog
static void acceptSetPointDialogCallback(Fl_Widget* Widget, void* Data);

// Callback function called when the cancel button is pressed in set-point dialog
static void cancelSetPointDialogCallback(Fl_Widget* Widget, void* Data);

// The function checks if the entered text is a valid non-negative number from the range of 0 ... 999.99
static bool isValidInputNumber(const std::string& TextUnderTest);

// Callback function called when the user finished modifying the set point value
static void inputFieldCallback(Fl_Widget* Widget, void* Data);

// Callback function called when the '+1A' '-0.1A' ... '-1A'  buttons are pressed in set-point dialog
static void relativeChangeOfSetPointCallback(Fl_Widget* Widget, void* Data);

// Callback function called when the 'i' button is pressed
static void diagnosticsButtonCallback(Fl_Widget* Widget, void* Data);

// Callback function called when the 'remote control turn on' button is pressed
static void remoteComputerControlCallback(Fl_Widget* Widget, void* Data);

static void drawMarkInCircle( int x, int y, int w, int h, CommunicationStatesClass TransmissionState );

// This function returns vertical position of a group of widgets that relates to a given channel
static int channelVerticalPosition( uint16_t ChannelIndex );

//.................................................................................................
// Function definitions
//.................................................................................................

void initializeMainWindowWidgets(void){

	for (int J = 0; J < (int)(sizeof(ChannelsHeaderTexts)/sizeof(ChannelsHeaderTexts[0])); J++ ){
	    ChannelsHeaderPtr[J] = new Fl_Box( ChannelsHeaderTextX[J], FIRST_GROUP_OF_WIDGETS_Y-ORDINARY_TEXT_SIZE/2, 40, 30, ChannelsHeaderTexts[J] );
	    ChannelsHeaderPtr[J]->labelcolor( FL_BLACK );
	    ChannelsHeaderPtr[J]->align( FL_ALIGN_TOP_LEFT );
	    ChannelsHeaderPtr[J]->labelfont( ORDINARY_TEXT_FONT );
	    ChannelsHeaderPtr[J]->labelsize( ORDINARY_TEXT_SIZE );
	}

    HeaderLine1Ptr = new HorizontalLineWidget( 0, FIRST_GROUP_OF_WIDGETS_Y-30, MAIN_WINDOW_WIDTH, 1 );
    HeaderLine2Ptr = new HorizontalLineWidget( 0, FIRST_GROUP_OF_WIDGETS_Y-4, MAIN_WINDOW_WIDTH, 1 );
    HeaderLine3Ptr = new HorizontalLineWidget( 0, FIRST_GROUP_OF_WIDGETS_Y-1, MAIN_WINDOW_WIDTH, 1 );

    LargeErrorMessage = new Fl_Box(30, 200, MAIN_WINDOW_WIDTH-60, 200,
    		"Problem z plikiem konfiguracyjnym\nuruchom program w konsoli\nz parametrem -v");
    LargeErrorMessage->hide();
    LargeErrorMessage->labelcolor( NAVY_BLUE_COLOR );
    LargeErrorMessage->labelfont( FL_BOLD );
    LargeErrorMessage->labelsize( 30 );

    ComputerLabelPtr = new Fl_Box( 0, 0, 75, 4, TextOfComputerLabel[0] );
    ComputerLabelPtr->hide();
    ComputerLabelPtr->labelcolor( FL_DARK3 );
    ComputerLabelPtr->align( FL_ALIGN_BOTTOM );
    ComputerLabelPtr->labelfont( ORDINARY_TEXT_FONT );
    ComputerLabelPtr->labelsize( 10 );

    RemoteComputerControlButton = new Fl_Button(780, 5, 150, 43, TextOfRemoteComputerControlButton[0] );
    RemoteComputerControlButton->hide();
    RemoteComputerControlButton->box( BUTTON_BOX_SHAPE );
    RemoteComputerControlButton->color(NORMAL_BUTTON_COLOR);
    RemoteComputerControlButton->labelfont( ORDINARY_TEXT_FONT );
    RemoteComputerControlButton->labelsize( ORDINARY_TEXT_SIZE );
    RemoteComputerControlButton->callback(remoteComputerControlCallback, nullptr);

}

void initializeWidgetsOfChannels(void){
    SetPointInputGroupPtr = new SetPointInputGroup(0, 0, MAIN_WINDOW_WIDTH, 2*GROUPS_OF_WIDGETS_SPACING-1);
    SetPointInputGroupPtr->hide();

    DiagnosticsGroupPtr = new DiagnosticsGroup(0, 0, MAIN_WINDOW_WIDTH, 2*GROUPS_OF_WIDGETS_SPACING-1);
    DiagnosticsGroupPtr->hide();

    BlankRectanglePtr = new BlankRectangleWidget(0, 0, MAIN_WINDOW_WIDTH, 2*GROUPS_OF_WIDGETS_SPACING-1);
    BlankRectanglePtr->hide();

    for (int J = 0; J < MAX_NUMBER_OF_SERIAL_PORTS; J++) {
    	TableOfGroupsPtr[J] = new ChannelGuiGroup(0, channelVerticalPosition(J), MAIN_WINDOW_WIDTH, GROUPS_OF_WIDGETS_SPACING-1);
    	TableOfGroupsPtr[J]->hide();
    	ApplicationWindow->add( TableOfGroupsPtr[J] );
    	TableOfGroupsPtr[J]->setGroupID(J);
    }

}

void StateMarkWidget::draw(){
    ChannelGuiGroup* parentGroup;
    parentGroup = (ChannelGuiGroup*)(this->parent());
	drawMarkInCircle( x(), y(), w(), h(), TableOfSharedDataForGui[parentGroup->getGroupID()].getStateOfCommunication() );
}

void BoxWithBackground::draw(){
    fl_color( color() );
    fl_rectf(x(), y(), w(), h());

    fl_color(FL_BLACK);
    fl_font( ORDINARY_TEXT_FONT, ORDINARY_TEXT_SIZE );
    fl_draw(label(), x(), y(), w(), h(), FL_ALIGN_CENTER);
}

void HorizontalLineWidget::draw(){
    fl_color(FL_BLACK);
	fl_xyline( x(), y(), x()+w() );
}

void RectangleFrameWidget::draw(){
    fl_color( 0x2Fu ); // gray
	fl_xyline( x(), y(), x()+w(), y() );
	fl_xyline( x()+w(), y(), x()+w(), y()+h() );
	fl_xyline( x(), y()+h(), x()+w(), y()+h() );
	fl_xyline( x(), y(), x(), y()+h() );
}

void BlankRectangleWidget::draw(){
    fl_color(FL_WHITE);
    fl_rectf(x(), y(), w(), h());
}

ChannelGuiGroup::ChannelGuiGroup(int X, int Y, int W, int H, const char* L) : Fl_Group(X, Y, W, H, L) {

	LastAcceptedNumericValue = 0.0;
	LastAcceptedValueStringPtr = new std::string("");

	ChannelDescriptionPtr = new BoxWithBackground( X+CHANNEL_DESCRIPTION_X, Y, CHANNEL_DESCRIPTION_WIDTH, GROUPS_OF_WIDGETS_SPACING-1, " ");
	ChannelDescriptionPtr->color( DESCRIPTION_COLOR );

	PhysicalIdPtr = new BoxWithBackground(X+PHYSICAL_ID_NUMBER_X, Y, 30, GROUPS_OF_WIDGETS_SPACING-1, "?");
	PhysicalIdPtr->color( PHYSICAL_ID_COLOR );

	PowerOnButtonPtr = new Fl_Button(X + POWER_SWITCH_BUTTON_X, Y+3, 70, 23, "Włącz" );
	PowerOnButtonPtr->box( BUTTON_BOX_SHAPE );
	PowerOnButtonPtr->color(NORMAL_BUTTON_COLOR);
	PowerOnButtonPtr->labelfont( ORDINARY_TEXT_FONT );
	PowerOnButtonPtr->labelsize( ORDINARY_TEXT_SIZE );
	PowerOnButtonPtr->callback(powerOnCallback, nullptr);

	PowerOffButtonPtr = new Fl_Button(X + POWER_SWITCH_BUTTON_X, Y+28, 70, 23, "Wyłącz" );
	PowerOffButtonPtr->box( BUTTON_BOX_SHAPE );
	PowerOffButtonPtr->color(NORMAL_BUTTON_COLOR);
	PowerOffButtonPtr->labelfont( ORDINARY_TEXT_FONT );
	PowerOffButtonPtr->labelsize( ORDINARY_TEXT_SIZE );
	PowerOffButtonPtr->callback(powerOffCallback, nullptr);

	PowerOnOffStatePtr = new Fl_Box(X+POWER_ON_OFF_STATE_X, Y+11, 10, 30, "------");
	PowerOnOffStatePtr->labelfont( LARGE_TEXT_FONT );
	PowerOnOffStatePtr->labelsize( LARGE_TEXT_SIZE );
	PowerOnOffStatePtr->align(FL_ALIGN_CENTER);
	PowerOnOffStatePtr->labelcolor( FL_BLACK );

	SetPointValuePtr = new Fl_Box(X+SET_VALUE_BUTTON_X+55, Y+4, 10, 30, "-?-?-"); // it should be overwritten
	SetPointValuePtr->labelfont( ORDINARY_TEXT_FONT );
	SetPointValuePtr->labelsize( ORDINARY_TEXT_SIZE );
	SetPointValuePtr->align(FL_ALIGN_LEFT);
	SetPointValuePtr->labelcolor( FL_BLACK );


	SetValueButtonPtr = new Fl_Button(X + SET_VALUE_BUTTON_X, Y+30, 60, 19, "zmień");
	SetValueButtonPtr->box(BUTTON_BOX_SHAPE);
	SetValueButtonPtr->color(NORMAL_BUTTON_COLOR);
	SetValueButtonPtr->labelfont( ORDINARY_TEXT_FONT );
	SetValueButtonPtr->labelsize( ORDINARY_TEXT_SIZE );
	SetValueButtonPtr->callback(changeSetPointValueButtonCallback, nullptr );
	SetValueButtonPtr->deactivate();

	ValueOfCurrentPtr = new Fl_Box(X+MEASURED_VALUE_OF_CURRENT_X, Y+11, 10, 30, "-----  ");
	ValueOfCurrentPtr->labelfont( LARGE_TEXT_FONT );
	ValueOfCurrentPtr->labelsize( LARGE_TEXT_SIZE );
	ValueOfCurrentPtr->align(FL_ALIGN_LEFT);
	ValueOfCurrentPtr->labelcolor( FL_BLACK );

	PsuErrorLabelPtr  = new Fl_Box(X + ERROR_LABELS_X, Y+ERROR_LABELS_Y1, 130, 20, "BŁĘDY ZASILACZA");
	PsuErrorLabelPtr->box(FL_FLAT_BOX);
	PsuErrorLabelPtr->color(FL_RED);
	PsuErrorLabelPtr->labelsize(ORDINARY_TEXT_SIZE);
	PsuErrorLabelPtr->hide();

	ExternalErrorLabelPtr  = new Fl_Box(X + ERROR_LABELS_X, Y+ERROR_LABELS_Y1, 130, 20, "BŁĘDY INNE");
	ExternalErrorLabelPtr->box(FL_FLAT_BOX);
	ExternalErrorLabelPtr->color(FL_RED);
	ExternalErrorLabelPtr->labelsize(ORDINARY_TEXT_SIZE);
	ExternalErrorLabelPtr->hide();

	PsuIdIncompatibilityLabelPtr  = new Fl_Box(X + ID_INCOMPATIBILITY_LABEL_X, Y+ERROR_LABELS_Y1, 160, 20, "Zasilacz nie odpowiada");
	PsuIdIncompatibilityLabelPtr->box(FL_FLAT_BOX);
	PsuIdIncompatibilityLabelPtr->color(FL_RED);
	PsuIdIncompatibilityLabelPtr->labelsize(ORDINARY_TEXT_SIZE);
	PsuIdIncompatibilityLabelPtr->hide();

	RemoteLocalLabelPtr  = new Fl_Box(X + LOCAL_REMOTE_LABEL_X, Y+LOCAL_REMOTE_LABEL_Y0, 130, 18, ModeUnknown );
	RemoteLocalLabelPtr->box(FL_FLAT_BOX);
	RemoteLocalLabelPtr->color( FL_WHITE );
	RemoteLocalLabelPtr->labelfont( ORDINARY_TEXT_FONT );
	RemoteLocalLabelPtr->labelsize( ORDINARY_TEXT_SIZE );

	StateMarkInCirclePtr = new StateMarkWidget(X + STATE_MARK_IN_CIRCLE_X, Y+13, 26, 26);

	DiagnosticsButton = new Fl_Button(X + SHOW_DIAGNOSTICS_BUTTON_X, Y+14, 30, 25, "i");
	DiagnosticsButton->box( FL_ROUNDED_BOX );
	DiagnosticsButton->color(NORMAL_BUTTON_COLOR);
	DiagnosticsButton->labelfont( FL_TIMES_BOLD_ITALIC );
	DiagnosticsButton->labelsize( ORDINARY_TEXT_SIZE );
	DiagnosticsButton->callback( diagnosticsButtonCallback, nullptr );

	BottomLinePtr = new HorizontalLineWidget( X, Y+GROUPS_OF_WIDGETS_SPACING-1, MAIN_WINDOW_WIDTH, 1 );

//	OnPowerDownWarning1Ptr  = new Fl_Box(X + HIGH_CURRENT_ON_POWER_DOWN_TEXT_X, Y+19, 400, 35, "UWAGA     Prąd nie wyzerował się\nŻeby wyłączyć pomimo to, naciśnij ponownie \"Wyłącz\"");
	OnPowerDownWarning1Ptr  = new Fl_Box(X + HIGH_CURRENT_ON_POWER_DOWN_TEXT_X, Y+19, 350, 18, "UWAGA     Prąd nie wyzerował się");
	OnPowerDownWarning1Ptr->box(FL_FLAT_BOX);
	OnPowerDownWarning1Ptr->color(FL_YELLOW);
	OnPowerDownWarning1Ptr->labelsize(ORDINARY_TEXT_SIZE);
	OnPowerDownWarning1Ptr->hide();

	OnPowerDownWarning2Ptr  = new Fl_Box(X + HIGH_CURRENT_ON_POWER_DOWN_TEXT_X, Y+19+18, 350, 17, "Żeby wyłączyć pomimo to, naciśnij ponownie \"Wyłącz\"");
	OnPowerDownWarning2Ptr->box(FL_FLAT_BOX);
	OnPowerDownWarning2Ptr->color(FL_YELLOW);
	OnPowerDownWarning2Ptr->labelsize(11);
	OnPowerDownWarning2Ptr->hide();

	GroupID = 0;

	OldPowerDownState = PoweringDownStatesClass::INACTIVE;
	OldControlFromGuiHere = 0;

    // Add widgets to group
    this->end();
}

// This function sets the sequence number of a group of widgets that relates to one channel
void ChannelGuiGroup::setGroupID( int NewValue ){
	GroupID = NewValue;
}

int ChannelGuiGroup::getGroupID(){
	return GroupID;
}

double ChannelGuiGroup::getLastAcceptedNumericValue(){
	return LastAcceptedNumericValue;
}

void ChannelGuiGroup::setLastAcceptedNumericValue( double NewValue ){
	LastAcceptedNumericValue = NewValue;
}

const char* ChannelGuiGroup::getLastAcceptedValueCString(){
	return LastAcceptedValueStringPtr->c_str();
}

void ChannelGuiGroup::setLastAcceptedValueString( const char* Text ){
	*LastAcceptedValueStringPtr = Text;
}

void ChannelGuiGroup::refreshNumericValues( CommunicationStatesClass StateOfTransmission, double NewValueOfCurrent, double NewValueOfSetpoint ){
	static char ValueOfCurrentOutputText[MAX_NUMBER_OF_SERIAL_PORTS][10];
	static char SetPointOutputText[MAX_NUMBER_OF_SERIAL_PORTS][10];
	if(GroupID >= MAX_NUMBER_OF_SERIAL_PORTS){
		return;
	}
	if ((CommunicationStatesClass::HEALTHY == StateOfTransmission) || (CommunicationStatesClass::TEMPORARY_ERRORS == StateOfTransmission)){
		snprintf( ValueOfCurrentOutputText[GroupID], sizeof(ValueOfCurrentOutputText[GroupID])-1, "%.2f A", NewValueOfCurrent );
		snprintf( SetPointOutputText[GroupID], sizeof(SetPointOutputText[GroupID])-1, "%.2f A", NewValueOfSetpoint );
	}
	else{
		snprintf( ValueOfCurrentOutputText[GroupID], sizeof(ValueOfCurrentOutputText[GroupID])-1, "------ A" );
		snprintf( SetPointOutputText[GroupID], sizeof(SetPointOutputText[GroupID])-1, "------ A" );
	}
	ValueOfCurrentPtr->label(ValueOfCurrentOutputText[GroupID]);
	SetPointValuePtr->label(SetPointOutputText[GroupID]);
}

void ChannelGuiGroup::refreshPowerOnOffLabel( CommunicationStatesClass StateOfTransmission, bool IsOn ){
	if ((CommunicationStatesClass::HEALTHY == StateOfTransmission) || (CommunicationStatesClass::TEMPORARY_ERRORS == StateOfTransmission)){
		if (IsOn){
			PowerOnOffStatePtr->label("WŁĄCZONY");
		}
		else{
			PowerOnOffStatePtr->label("WYŁĄCZONY");
		}
	}
	else{
		PowerOnOffStatePtr->label("------");
	}
}

void ChannelGuiGroup::refreshStatusLabels( CommunicationStatesClass StateOfTransmission, uint16_t StateRegister ){
	uint8_t NumerOfErrors;
	int32_t LocalRemoteLabelY, PsuErrorsLabelY, ExternalErrorLabelY;

	LocalRemoteLabelY   = y() + 16;
	PsuErrorsLabelY     = y() + 6;
	ExternalErrorLabelY = y() + 6;

	if ((CommunicationStatesClass::HEALTHY != StateOfTransmission) && (CommunicationStatesClass::TEMPORARY_ERRORS != StateOfTransmission)){
		PsuErrorLabelPtr->hide();
		ExternalErrorLabelPtr->hide();

		if ((CommunicationStatesClass::PERMANENT_ERRORS == StateOfTransmission) ||
				(CommunicationStatesClass::PORT_NOT_OPEN == StateOfTransmission))
		{
			PsuIdIncompatibilityLabelPtr->hide();

			RemoteLocalLabelPtr->label( ModeUnknown );
			RemoteLocalLabelPtr->position( RemoteLocalLabelPtr->x(), LocalRemoteLabelY );
			RemoteLocalLabelPtr->color( FL_WHITE );
		}
		else{
			PsuIdIncompatibilityLabelPtr->show();
			LocalRemoteLabelY += 14;
			RemoteLocalLabelPtr->position( RemoteLocalLabelPtr->x(), LocalRemoteLabelY );
		}

		return;
	}
	PsuIdIncompatibilityLabelPtr->hide();

	NumerOfErrors = 0;
	if (0 != (StateRegister & MASK_OF_SUM_ERROR_BIT)){
		PsuErrorLabelPtr->show();
		NumerOfErrors++;
	}
	else{
		PsuErrorLabelPtr->hide();
	}
	if (0 != (StateRegister & MASK_OF_EXT_ERROR_BIT)){
		ExternalErrorLabelPtr->show();
		NumerOfErrors++;
	}
	else{
		ExternalErrorLabelPtr->hide();
	}

	if (0 == (StateRegister & MASK_OF_LOCAL_REMOTE_BIT)){
		RemoteLocalLabelPtr->label( ModeLocal );
		RemoteLocalLabelPtr->color( FL_YELLOW );
	}
	else{
		RemoteLocalLabelPtr->label( ModeRemote );
		RemoteLocalLabelPtr->color( FL_WHITE );
	}

	if (1 == NumerOfErrors){
		LocalRemoteLabelY += 14;
	}
	if (2 == NumerOfErrors){
		PsuErrorsLabelY     -= 6;
		ExternalErrorLabelY += 11;
		LocalRemoteLabelY   += 21;
	}
	PsuErrorLabelPtr->position( PsuErrorLabelPtr->x(), PsuErrorsLabelY );
	ExternalErrorLabelPtr->position( ExternalErrorLabelPtr->x(), ExternalErrorLabelY );
	RemoteLocalLabelPtr->position( RemoteLocalLabelPtr->x(), LocalRemoteLabelY );
}

void ChannelGuiGroup::updateSettingsButtonsAndPowerDownWigets( CommunicationStatesClass StateOfTransmission,
		PoweringDownStatesClass NewPowerDownState )
{
	uint8_t TemporaryControlFromGuiHere = ControlFromGuiHere;

	if (((CommunicationStatesClass::HEALTHY == StateOfTransmission) ||
			(CommunicationStatesClass::TEMPORARY_ERRORS == StateOfTransmission)) &&
			(0 != TemporaryControlFromGuiHere) &&
			(0 > SetPointInputGroupPtr->getChannelDisplayingSetPointEntryDialog()))
	{
		if ((NewPowerDownState != OldPowerDownState) ||
				(TemporaryControlFromGuiHere != OldControlFromGuiHere))
		{
#if DEBUG_POWERING_DOWN_STATE_MACHINE
			std::cout << " updateChannelWidgets; zasilacz " << (int)GroupID << " automat w stanie " << (int)NewPowerDownState << std::endl;
#endif
			if (PoweringDownStatesClass::TIMEOUT_EXCEEDED == NewPowerDownState){
				PowerOnButtonPtr->deactivate();
				PowerOffButtonPtr->activate();
				SetValueButtonPtr->deactivate();

				setWidgetsOnPowerDownWarningActive();
			}
			else{
				setWidgetsOnPowerDownWarningInactive();	// restore default form of widgets

				if (PoweringDownStatesClass::CURRENT_DECELERATING == NewPowerDownState){
					PowerOnButtonPtr->deactivate();
					PowerOffButtonPtr->deactivate();
					SetValueButtonPtr->deactivate();
				}
			}
		}
		if (PoweringDownStatesClass::INACTIVE == NewPowerDownState){
			PowerOnButtonPtr->activate();
			PowerOffButtonPtr->activate();
			SetValueButtonPtr->activate();
		}
	}
	else{
		PowerOnButtonPtr->deactivate();
		PowerOffButtonPtr->deactivate();
		SetValueButtonPtr->deactivate();
		if ((PoweringDownStatesClass::TIMEOUT_EXCEEDED == OldPowerDownState) &&
				((NewPowerDownState != OldPowerDownState) ||
				(TemporaryControlFromGuiHere != OldControlFromGuiHere)))
		{
			setWidgetsOnPowerDownWarningInactive();	// restore default form of widgets
		}
	}
	OldControlFromGuiHere = TemporaryControlFromGuiHere;
	OldPowerDownState = NewPowerDownState;
}

void ChannelGuiGroup::setDescriptionLabel(){
	if (nullptr != TableOfSharedDataForGui[GroupID].getDescription()){
		ChannelDescriptionPtr->label( (*TableOfSharedDataForGui[GroupID].getDescription()).c_str() );
	}
}

void ChannelGuiGroup::refreshPhysicalID( uint16_t PhysicalIdRegister ){
	static char PhysicalIdText[MAX_NUMBER_OF_SERIAL_PORTS][8];
	if(GroupID >= MAX_NUMBER_OF_SERIAL_PORTS){
		return;
	}
	snprintf( PhysicalIdText[GroupID], sizeof(PhysicalIdText[GroupID])-1, "%02X", PhysicalIdRegister );
	PhysicalIdPtr->label(PhysicalIdText[GroupID]);
}

void ChannelGuiGroup::truncateDescription() {
    std::string Label = ChannelDescriptionPtr->label(); // get current label
    int MaxWidth = ChannelDescriptionPtr->w() - 10;    // Maximum text width (with margin)

    fl_font( ORDINARY_TEXT_FONT, ORDINARY_TEXT_SIZE );
    int TextWidth = fl_width(Label.c_str());   // Measuring the width of the text

    if (TextWidth > MaxWidth) {
        for (size_t J = Label.length(); J > 0; --J) {
            std::string Temporary = Label.substr(0, J) + "..."; // Shortening the text
            if (fl_width(Temporary.c_str()) <= MaxWidth) {
                Label = Temporary;
                break;
            }
        }
    }
    ChannelDescriptionArchived = Label;
    ChannelDescriptionPtr->copy_label(ChannelDescriptionArchived.c_str()); // Set up the abbreviated Label
}

void ChannelGuiGroup::setBottomLineVisibility(){
	if (((SetPointInputGroupPtr->getChannelDisplayingSetPointEntryDialog()) == GroupID) ||
			((DiagnosticsGroupPtr->getChannelDisplayingDiagnostics()) == GroupID))
	{
		BottomLinePtr->hide();
	}
	else{
		BottomLinePtr->show();
	}
}

void ChannelGuiGroup::selectiveDeactivate(){
	PowerOnButtonPtr->deactivate();
	PowerOffButtonPtr->deactivate();
	SetValueButtonPtr->deactivate();
	DiagnosticsButton->deactivate();
}

void ChannelGuiGroup::selectiveActivate(){
	PowerOnButtonPtr->activate();
	PowerOffButtonPtr->activate();
	SetValueButtonPtr->activate();
	DiagnosticsButton->activate();
}

void ChannelGuiGroup::restoreInitialState(){
	this->show();
	this->position( 0, channelVerticalPosition(this->getGroupID()) );
	this->setBottomLineVisibility();
	this->selectiveActivate();
}

void ChannelGuiGroup::setWidgetsOnPowerDownWarningActive(){
	SetValueButtonPtr->hide();

	// move some text widgets upward
	PowerOnOffStatePtr->position( PowerOnOffStatePtr->x(), y() - 6);
	SetPointValuePtr->position(   SetPointValuePtr->x(),   y() - 7 );
	ValueOfCurrentPtr->position(  ValueOfCurrentPtr->x(),  y() - 6 );

	OnPowerDownWarning1Ptr->show();
	OnPowerDownWarning2Ptr->show();
}

void ChannelGuiGroup::setWidgetsOnPowerDownWarningInactive(){
	OnPowerDownWarning1Ptr->hide();
	OnPowerDownWarning2Ptr->hide();
	SetValueButtonPtr->show();
	PowerOnOffStatePtr->position( PowerOnOffStatePtr->x(), y() + 11);
	SetPointValuePtr->position(   SetPointValuePtr->x(),   y() + 4 );
	ValueOfCurrentPtr->position(  ValueOfCurrentPtr->x(),  y() + 11 );
}

// Overlay handle() method
int WindowEscProof::handle(int event){
    if (event == FL_KEYDOWN) {  // Check if it is a key event
        if (Fl::event_key() == FL_Escape) {  // Check if it is the Esc key
            return 1;  // Block the default behavior
        }
    }
    return Fl_Window::handle(event);  // For other events, call the default handler
}

SetPointInputGroup::SetPointInputGroup(int X, int Y, int W, int H, const char* L) : Fl_Group(X, Y, W, H, L) {
	ChannelThatDisplaysInputValueDialog = -1;

	RectangleFrameWidget* RectangleFrameWidgetPtr1 = new RectangleFrameWidget( X+(2*W)/8-2, Y+(1*H)/16-2, (4*W)/8+4, (14*H)/16+4 );
	(void)RectangleFrameWidgetPtr1;	 // so the compiler doesn't complain

	RectangleFrameWidget* RectangleFrameWidgetPtr2 = new RectangleFrameWidget( X+(2*W)/8, Y+(1*H)/16, (4*W)/8, (14*H)/16 );
	(void)RectangleFrameWidgetPtr2;	 // so the compiler doesn't complain

	InputDialogPtr = new Fl_Float_Input( X+(15*W)/32, Y+(15*H)/32-11, 60, 23 );
	InputDialogPtr->box(FL_BORDER_BOX);
	InputDialogPtr->color( DESCRIPTION_COLOR );
	InputDialogPtr->callback(inputFieldCallback, nullptr);
	InputDialogPtr->maximum_size(7);  // Maximum length of text

	LastValidInputStringPtr = new std::string("-?-?-"); // this text should be overwritten

	Fl_Box *AmpereUnitTextPtr = new Fl_Box( X+(15*W)/32+65, Y+(15*H)/32-11, 10, 23, "A");
	(void)AmpereUnitTextPtr; // so the compiler doesn't complain

	AcceptButtonPtr = new Fl_Button( X+(19*W)/32, Y+(11*H)/32-11, 60, 19, "OK" );
	AcceptButtonPtr->box(BUTTON_BOX_SHAPE);
	AcceptButtonPtr->color(NORMAL_BUTTON_COLOR);
	AcceptButtonPtr->labelfont( ORDINARY_TEXT_FONT );
	AcceptButtonPtr->labelsize( ORDINARY_TEXT_SIZE );
	AcceptButtonPtr->callback( acceptSetPointDialogCallback, nullptr );

	CancelButtonPtr = new Fl_Button( X+(19*W)/32, Y+(19*H)/32-11, 60, 19, "Anuluj" );
	CancelButtonPtr->box(BUTTON_BOX_SHAPE);
	CancelButtonPtr->color(NORMAL_BUTTON_COLOR);
	CancelButtonPtr->labelfont( ORDINARY_TEXT_FONT );
	CancelButtonPtr->labelsize( ORDINARY_TEXT_SIZE );
	CancelButtonPtr->callback( cancelSetPointDialogCallback, nullptr );

	CloseButtonPtr = new Fl_Button( X+(43*W)/64, Y+(25*H)/32-11, 60, 19, "Zamknij" );
	CloseButtonPtr->box(BUTTON_BOX_SHAPE);
	CloseButtonPtr->color(NORMAL_BUTTON_COLOR);
	CloseButtonPtr->labelfont( ORDINARY_TEXT_FONT );
	CloseButtonPtr->labelsize( ORDINARY_TEXT_SIZE );
	CloseButtonPtr->callback( closeSetPointDialogCallback, nullptr );

	IncreaseBy1 = AcceptButtonPtr = new Fl_Button( X+(11*W)/32, Y+(6*H)/32-11, 60, 19, "+1A" );
	IncreaseBy1->box(BUTTON_BOX_SHAPE);
	IncreaseBy1->color(NORMAL_BUTTON_COLOR);
	IncreaseBy1->labelfont( ORDINARY_TEXT_FONT );
	IncreaseBy1->labelsize( ORDINARY_TEXT_SIZE );
	IncreaseBy1->callback( relativeChangeOfSetPointCallback, (void*)&RelativeIncreamentsTable[RELATIVE_INCREAMENT_PLUS_1] );

	IncreaseBy01 = AcceptButtonPtr = new Fl_Button( X+(11*W)/32, Y+(13*H)/32-11, 60, 19, "+0.1A" );
	IncreaseBy01->box(BUTTON_BOX_SHAPE);
	IncreaseBy01->color(NORMAL_BUTTON_COLOR);
	IncreaseBy01->labelfont( ORDINARY_TEXT_FONT );
	IncreaseBy01->labelsize( ORDINARY_TEXT_SIZE );
	IncreaseBy01->callback( relativeChangeOfSetPointCallback, (void*)&RelativeIncreamentsTable[RELATIVE_INCREAMENT_PLUS_01] );

	DecreaseBy01 = AcceptButtonPtr = new Fl_Button( X+(11*W)/32, Y+(20*H)/32-11, 60, 19, "-0.1A" );
	DecreaseBy01->box(BUTTON_BOX_SHAPE);
	DecreaseBy01->color(NORMAL_BUTTON_COLOR);
	DecreaseBy01->labelfont( ORDINARY_TEXT_FONT );
	DecreaseBy01->labelsize( ORDINARY_TEXT_SIZE );
	DecreaseBy01->callback( relativeChangeOfSetPointCallback, (void*)&RelativeIncreamentsTable[RELATIVE_INCREAMENT_MINUS_01] );

	DecreaseBy1 = AcceptButtonPtr = new Fl_Button( X+(11*W)/32, Y+(27*H)/32-11, 60, 19, "-1A" );
	DecreaseBy1->box(BUTTON_BOX_SHAPE);
	DecreaseBy1->color(NORMAL_BUTTON_COLOR);
	DecreaseBy1->labelfont( ORDINARY_TEXT_FONT );
	DecreaseBy1->labelsize( ORDINARY_TEXT_SIZE );
	DecreaseBy1->callback( relativeChangeOfSetPointCallback, (void*)&RelativeIncreamentsTable[RELATIVE_INCREAMENT_MINUS_1] );

	BottomLinePtr = new HorizontalLineWidget( X, Y+2*GROUPS_OF_WIDGETS_SPACING-2, W, 1 );

	OfflineSetpointValue = 0;
	MulticlickCounter = 0;

    // Add widgets to group
    this->end();
}

void SetPointInputGroup::setChannelDisplayingSetPointEntryDialog( int16_t NewValue ){
	if ((0 <= NewValue) && (NewValue < NumberOfChannels)){
		ChannelThatDisplaysInputValueDialog = NewValue;
	}
	else{
		ChannelThatDisplaysInputValueDialog = -1;
	}
}

int16_t SetPointInputGroup::getChannelDisplayingSetPointEntryDialog(){
	return ChannelThatDisplaysInputValueDialog;
}

void SetPointInputGroup::openDialog( int X, int Y ){
	this->position( X, Y );
	this->show();
	InputDialogPtr->take_focus();
	MulticlickCounter = 0;
}

void SetPointInputGroup::closeDialog(){
	if( -1 != ChannelThatDisplaysInputValueDialog){
		ChannelThatDisplaysInputValueDialog = -1;
		this->hide();
	}
}

const char* SetPointInputGroup::getLastValidInputCStringPtr(){
	return LastValidInputStringPtr->c_str();
}

void SetPointInputGroup::setLastValidInputString( const char* NewText ){
	*LastValidInputStringPtr = NewText;
	InputDialogPtr->value( NewText );
}

void SetPointInputGroup::resetMulticlick(){
	MulticlickCounter = 0;
}

// This function protects against fast multiclicking of the buttons '+1A' '-0.1A' ... '-1A'.
// If the user clicks the '+1A' '-0.1A' ... '-1A' slower than the time it takes to read the setpoint
// twice from the RaspberryPi Pico interface via Modbus RTU, this function makes no change;
// however, if the user clicks faster, the function buffers the setpoint to keep up with the clicks;
// when the user stops clicking, the buffered value is synchronized with the Modbus RTU register value.
uint16_t SetPointInputGroup::getMulticlickProofSetpointValue( uint8_t ChannelIndex ){
	DataSharingInterface* InterfaceDataPtr;

	if (0 == MulticlickCounter){
		InterfaceDataPtr = &TableOfSharedDataForGui[ChannelIndex];
		OfflineSetpointValue = InterfaceDataPtr->getModbusRegister(MODBUS_ADDRES_REQUIRED_VALUE);
	}
	MulticlickCounter = 2;
	return OfflineSetpointValue;
}

void SetPointInputGroup::setOfflineSetpointValue( uint16_t NewValue ){
	OfflineSetpointValue = NewValue;
}

// This function transfers information about updating the Modbus RTU register containing the setpoint to GUI.
// The information is needed in to protect against fast multiclicking of the buttons '+1A' '-0.1A' ... '-1A'
void multiclickCountdown(void){
	if (0 < SetPointInputGroupPtr->MulticlickCounter){
		SetPointInputGroupPtr->MulticlickCounter--;
	}
}

DiagnosticsGroup::DiagnosticsGroup(int X, int Y, int W, int H, const char* L) : Fl_Group(X, Y, W, H, L) {
	ChannelThatDisplaysDiagnostics = -1;

	DiagnosticTextBoxPtr = new Fl_Box(X, Y, W, H, "tu powinny być dane diagnostyczne");
	DiagnosticTextBoxPtr->labelfont( FL_COURIER );
	DiagnosticTextBoxPtr->labelsize( 11 );
	DiagnosticTextBoxPtr->labelcolor( FL_BLACK );

	BottomLinePtr = new HorizontalLineWidget( X, Y+2*GROUPS_OF_WIDGETS_SPACING-1, W, 1 );

    // Add widgets to group
    this->end();
}

void DiagnosticsGroup::updateDataAndWidgets(){
	static char DiagnosticsText[400];
	char* LastErrorTextPtr;
	CommunicationStatesClass CommunicationPerformance;

	CommunicationPerformance = TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getStateOfCommunication();

	if ((LastFrameErrorClass::PERFECTION == TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getLastFrameError()) ||
		(LastFrameErrorClass::UNSPECIFIED == TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getLastFrameError()))
	{
		LastErrorTextPtr = (char*)ModbusError_0;
	}
	else if (LastFrameErrorClass::NO_RESPONSE == TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getLastFrameError()){
		LastErrorTextPtr = (char*)ModbusError_x;
	}
	else if (LastFrameErrorClass::NOT_COMPLETE_FRAME == TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getLastFrameError()){
		LastErrorTextPtr = (char*)ModbusError_n;
	}
	else if (LastFrameErrorClass::BAD_CRC == TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getLastFrameError()){
		LastErrorTextPtr = (char*)ModbusError_c;
	}
	else if (LastFrameErrorClass::OTHER_FRAME_ERROR == TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getLastFrameError()){
		LastErrorTextPtr = (char*)ModbusError_f;
	}
	else{
		LastErrorTextPtr = (char*)InternalErrorMessage;
	}
	if ((CommunicationStatesClass::HEALTHY == CommunicationPerformance) ||
			(CommunicationStatesClass::TEMPORARY_ERRORS == CommunicationPerformance))
	{
		// display the communication performance data and PSU data
		snprintf( DiagnosticsText, sizeof(DiagnosticsText)-1,
				//          1234567  1234567  1234567  1234567  1234567
				"           Średnia  Mediana ŚredniaM  Pik-Pik Odch.std  \n"
				"    Prąd   %7.2f  %7.2f  %7.2f  %7.2f  %7.2f  \n"
				"Napięcie   %7.2f  %7.2f  %7.2f  %7.2f  %7.2f  \n\n"
				"Błędy transmisji Modbus RTU: %3d.%d%%     Najdłuższy ciąg %3d\n"
				"Ostatni błąd Modbusa RTU: %s               ",
				0.01*(double)TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_CURRENT_MEAN),
				0.01*(double)TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_CURRENT_MEDIAN),
				0.01*(double)TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_CURRENT_FILTERED),
				0.01*(double)TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_CURRENT_PEAK_TO_PEAK),
				0.01*(double)TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_CURRENT_STD_DEVIATION),

				0.01*(double)TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_VOLTAGE_MEAN),
				0.01*(double)TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_VOLTAGE_MEDIAN),
				0.01*(double)TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_VOLTAGE_FILTERED),
				0.01*(double)TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_VOLTAGE_PEAK_TO_PEAK),
				0.01*(double)TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_VOLTAGE_STD_DEVIATION),
				TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getPerMilleError() / 10,
				TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getPerMilleError() % 10,
				TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getMaxErrorSequence(),
				LastErrorTextPtr );
	}
	else if (CommunicationStatesClass::PERMANENT_ERRORS == CommunicationPerformance){
		// display information about communication errors
		if (TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getTransmissionAcknowledgement()){
			snprintf( DiagnosticsText, sizeof(DiagnosticsText)-1,
					"Brak komunikacji\n"
					"Port  %s  jest prawidłowo skonfigurowany.\n\n\n"
					"Błędy transmisji Modbus: %3d.%d%%    Najdłuższy ciąg błędów %3d    \n"
					"Ostatni błąd Modbusa: %s                         ",
					(TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getNameOfPortPtr())->c_str(),
					TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getPerMilleError() / 10,
					TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getPerMilleError() % 10,
					TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getMaxErrorSequence(),
					LastErrorTextPtr );
		}
		else{
			snprintf( DiagnosticsText, sizeof(DiagnosticsText)-1,
					"Nie nawiązano komunikacji (brak odpowiedzi z interfejsu zasilacza)\n"
					"Port  %s  jest prawidłowo skonfigurowany",
					(TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getNameOfPortPtr())->c_str());
		}
	}
	else if (CommunicationStatesClass::WRONG_PHYSICAL_ID == CommunicationPerformance){
		// display information about PSU physical ID
		snprintf( DiagnosticsText, sizeof(DiagnosticsText)-1,
				"Niezgodność numeru ID zasilacza\nOczekiwano %u (%X hex), odczytano %u (%X hex)\n\n\n"
				"Błędy transmisji Modbus: %3d.%d%%    Najdłuższy ciąg błędów %3d    \n"
				"Ostatni błąd Modbusa: %s                         ",
				TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getPowerSupplyUnitId(),
				TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getPowerSupplyUnitId(),
				TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_POWER_SOURCE_ID),
				TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getModbusRegister(MODBUS_ADDRES_POWER_SOURCE_ID),
				TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getPerMilleError() / 10,
				TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getPerMilleError() % 10,
				TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getMaxErrorSequence(),
				LastErrorTextPtr );
	}
	else{
		// display information about serial port state
		snprintf( DiagnosticsText, sizeof(DiagnosticsText)-1,
				"Port  %s  nie istnieje albo nie daje się skonfigurować",
				(TableOfSharedDataForGui[ChannelThatDisplaysDiagnostics].getNameOfPortPtr())->c_str() );
	}
	DiagnosticTextBoxPtr->label( DiagnosticsText );
}

int16_t DiagnosticsGroup::getChannelDisplayingDiagnostics(){
	return ChannelThatDisplaysDiagnostics;
}

void DiagnosticsGroup::setChannelDisplayingDiagnostics( int16_t NewValue ){
	if ((0 <= NewValue) && (NewValue < NumberOfChannels)){
		ChannelThatDisplaysDiagnostics = NewValue;
	}
	else{
		ChannelThatDisplaysDiagnostics = -1;
	}
}

void DiagnosticsGroup::closeGroup(){
	if( -1 != ChannelThatDisplaysDiagnostics){
		ChannelThatDisplaysDiagnostics = -1;
		this->hide();
	}
}

void DiagnosticsGroup::enableAtNewPosition( int X, int Y ){
	this->position( X, Y );
	this->show();
}

static void powerOnCallback(Fl_Widget* Widget, void* Data){
	int16_t Channel;
	ChannelGuiGroup* MyGroup;

	(void)Data; // intentionally unused

	MyGroup = (ChannelGuiGroup*)(Widget->parent());
	Channel = MyGroup->getGroupID();

	if ((TableOfSharedDataForGui[Channel].getStateOfCommunication() == CommunicationStatesClass::HEALTHY) ||
			(TableOfSharedDataForGui[Channel].getStateOfCommunication() == CommunicationStatesClass::TEMPORARY_ERRORS))
	{
		TableOfSharedDataForGui[Channel].placeNewOrder( RTU_ORDER_POWER_ON, 0 );
	}
}

static void powerOffCallback(Fl_Widget* Widget, void* Data){
	int16_t Channel;
	ChannelGuiGroup* MyGroup;

	(void)Data; // intentionally unused

	MyGroup = (ChannelGuiGroup*)(Widget->parent());
	Channel = MyGroup->getGroupID();

	if (PoweringDownStatesClass::TIMEOUT_EXCEEDED != TableOfSharedDataForGui[Channel].getPoweringDownState()){
		TableOfSharedDataForGui[Channel].placeNewOrder( RTU_ORDER_DELAYED_POWER_OFF, 0 );
	}
	else{
		TableOfSharedDataForGui[Channel].placeNewOrder( RTU_ORDER_POWER_OFF, 0 );
	}

#if DEBUG_POWERING_DOWN_STATE_MACHINE
	std::cout << " powerOffCallback; zasilacz " << (int)Channel << " automat w stanie " << (int)TableOfSharedDataForGui[Channel].getPoweringDownState() << std::endl;
#endif
}

// Callback function called when the 'change' button is pressed
static void changeSetPointValueButtonCallback(Fl_Widget* Widget, void* Data) {
	ChannelGuiGroup* MyGroup;
	uint8_t IndexOfGroup;

	(void)Data; // intentionally unused

	MyGroup = (ChannelGuiGroup*)(Widget->parent());
	IndexOfGroup = MyGroup->getGroupID();

	DiagnosticsGroupPtr->closeGroup();
	BlankRectanglePtr->hide();
	SetPointInputGroupPtr->setChannelDisplayingSetPointEntryDialog( IndexOfGroup );

	SetPointInputGroupPtr->setLastValidInputString(	MyGroup->getLastAcceptedValueCString() );

	for (int J = 0; J < NumberOfChannels; J++) {
    	if (J < IndexOfGroup){
			if(TableOfGroupsPtr[J] != nullptr){
				TableOfGroupsPtr[J]->position( 0, channelVerticalPosition(J) );
				TableOfGroupsPtr[J]->setBottomLineVisibility();
				TableOfGroupsPtr[J]->selectiveDeactivate();
			}
    	}
    	else{
	    	if (J == IndexOfGroup){
	    		MyGroup->position( 0, channelVerticalPosition(J) );
	    		MyGroup->setBottomLineVisibility();
	    		MyGroup->selectiveDeactivate();
	    		SetPointInputGroupPtr->openDialog( 0, channelVerticalPosition(J+1) );
	    	}
	    	else{
				if(TableOfGroupsPtr[J] != nullptr){
					TableOfGroupsPtr[J]->position( 0, channelVerticalPosition(J+2) );
					TableOfGroupsPtr[J]->setBottomLineVisibility();
					TableOfGroupsPtr[J]->selectiveDeactivate();
				}
	    	}
    	}
    }
	SetPointInputGroupPtr->resetMulticlick();
}

// Callback function called when the close button is pressed in set-point dialog
static void closeSetPointDialogCallback(Fl_Widget* Widget, void* Data){
	(void)Widget; // intentionally unused
	(void)Data; // intentionally unused
	closeSetPointDialogInternals();
}

static void closeSetPointDialogInternals(void){
	SetPointInputGroupPtr->closeDialog();
	for (int J = 0; J < NumberOfChannels; J++) {
		if(TableOfGroupsPtr[J] != nullptr){
			TableOfGroupsPtr[J]->position( 0, channelVerticalPosition(J) );
			TableOfGroupsPtr[J]->setBottomLineVisibility();
			TableOfGroupsPtr[J]->selectiveActivate();
		}
	}
	BlankRectanglePtr->position( 0, channelVerticalPosition(NumberOfChannels) );
	BlankRectanglePtr->show();
}

// Callback function called when the 'OK' button is pressed in set-point dialog
static void acceptSetPointDialogCallback(Fl_Widget* Widget, void* Data){
	int16_t Channel;
	uint16_t NewValueUint16;

	(void)Widget; // intentionally unused
	(void)Data; // intentionally unused

	Channel = SetPointInputGroupPtr->getChannelDisplayingSetPointEntryDialog();
	if ((0 > Channel) || (Channel >= NumberOfChannels)){
		return; // assurance
	}

	if (strlen(SetPointInputGroupPtr->getLastValidInputCStringPtr()) == 0){
		return; // the empty text does nothing
	}

	TableOfGroupsPtr[Channel]->setLastAcceptedValueString( SetPointInputGroupPtr->getLastValidInputCStringPtr() );
	TableOfGroupsPtr[Channel]->setLastAcceptedNumericValue( atof( SetPointInputGroupPtr->getLastValidInputCStringPtr() ));

	// the calculation is described in the documentation
	NewValueUint16 = (uint16_t)(TableOfGroupsPtr[Channel]->getLastAcceptedNumericValue() * (0.005*65536.0) + 0.5);
	if (TableOfGroupsPtr[Channel]->getLastAcceptedNumericValue() >= 199.997){
		NewValueUint16 = 0xFFFFu; // to avoid overflow
	}

	TableOfSharedDataForGui[Channel].placeNewOrder( RTU_ORDER_SET_VALUE, NewValueUint16 );

	SetPointInputGroupPtr->resetMulticlick();
}

// Callback function called when the 'cancel' button is pressed in set-point dialog
static void cancelSetPointDialogCallback(Fl_Widget* Widget, void* Data){
	int16_t Channel;

	(void)Widget; // intentionally unused
	(void)Data; // intentionally unused

	Channel = SetPointInputGroupPtr->getChannelDisplayingSetPointEntryDialog();
	if ((0 > Channel) || (Channel >= NumberOfChannels)){
		return; // assurance
	}
	SetPointInputGroupPtr->setLastValidInputString(	TableOfGroupsPtr[Channel]->getLastAcceptedValueCString() );
	SetPointInputGroupPtr->resetMulticlick();
}

// The function checks if the entered text is a valid non-negative number from the range of 0 ... 200
static bool isValidInputNumber(const std::string& TextUnderTest) {
    if (TextUnderTest.empty()){
    	return true;  // A blank field is acceptable
    }

    // Check if the text contains only numbers and at most one dot
    int DotCount = 0;
    for (char ch : TextUnderTest) {
        if (ch == '.') {
            DotCount++;
            if (DotCount > 1){
            	return false;  // More then one dot
            }
        }
        else if (ch < '0') {
            return false;  // Character is not a digit or a dot
        }
        else if (ch > '9') {
            return false;  // Character is not a digit or a dot
        }
        else{
        }
    }
    double Value = std::stod(TextUnderTest);
    if ((Value < 0.0) || (Value > 200.0)){
    	return false;
    }
    return true;
}

// Callback function called when the user finished modifying the set point value
static void inputFieldCallback(Fl_Widget* Widget, void* Data) {
    Fl_Float_Input* InputField = (Fl_Float_Input*)Widget;
    static const char* InputText = InputField->value();

	(void)Data; // intentionally unused

	// Check the correctness of the entered value
    if (!isValidInputNumber(InputText)) {
        // If the new value is not correct, restore the previous value
        InputField->value( SetPointInputGroupPtr->getLastValidInputCStringPtr() );
    } else {
        // Remember the current value as the last correct one
    	SetPointInputGroupPtr->setLastValidInputString( InputText );
    }
	SetPointInputGroupPtr->resetMulticlick();
}

// Callback function called when the '+1A' '-0.1A' ... '-1A'  buttons are pressed in set-point dialog
static void relativeChangeOfSetPointCallback(Fl_Widget* Widget, void* Data){
	int16_t Channel, RelativeChange;
	int32_t SetPointValue;
	(void)Widget; // intentionally unused

	RelativeChange = *((int16_t*)Data);
	Channel = SetPointInputGroupPtr->getChannelDisplayingSetPointEntryDialog();
	if ((0 > Channel) || (Channel >= NumberOfChannels)){
		return; // assurance
	}

	SetPointInputGroupPtr->setLastValidInputString( "" );
	SetPointValue = (int32_t)(SetPointInputGroupPtr->getMulticlickProofSetpointValue( Channel ));

	SetPointValue += RelativeChange;

	if (SetPointValue > 0xFFFF){
		SetPointValue = 0xFFFF;
	}
	if (SetPointValue < 0){
		SetPointValue = 0;
	}
	SetPointInputGroupPtr->setOfflineSetpointValue( (uint16_t)SetPointValue );
	TableOfSharedDataForGui[Channel].placeNewOrder( RTU_ORDER_SET_VALUE, (uint16_t)SetPointValue );
}

// Callback function called when the 'i' button is pressed
static void diagnosticsButtonCallback(Fl_Widget* Widget, void* Data){
	ChannelGuiGroup* MyGroup;
	uint8_t IndexOfGroup;

	(void)Data; // intentionally unused

	MyGroup = (ChannelGuiGroup*)(Widget->parent());
	IndexOfGroup = MyGroup->getGroupID();

	if (IndexOfGroup == DiagnosticsGroupPtr->getChannelDisplayingDiagnostics()){
		DiagnosticsGroupPtr->closeGroup();
	    for (int J = 0; J < NumberOfChannels; J++) {
			if(TableOfGroupsPtr[J] != nullptr){
				TableOfGroupsPtr[J]->position( 0, channelVerticalPosition(J) );
				TableOfGroupsPtr[J]->setBottomLineVisibility();
			}
	    }
	    BlankRectanglePtr->position( 0, channelVerticalPosition(NumberOfChannels) );
	    BlankRectanglePtr->show();
	}
	else{
		BlankRectanglePtr->hide();
		DiagnosticsGroupPtr->setChannelDisplayingDiagnostics( IndexOfGroup );
	    for (int J = 0; J < NumberOfChannels; J++) {
	    	if (J < IndexOfGroup){
				if(TableOfGroupsPtr[J] != nullptr){
					TableOfGroupsPtr[J]->position( 0, channelVerticalPosition(J) );
					TableOfGroupsPtr[J]->setBottomLineVisibility();
				}
	    	}
	    	else{
		    	if (J == IndexOfGroup){
		    		MyGroup->position( 0, channelVerticalPosition(J) );
		    		MyGroup->setBottomLineVisibility();

		    		DiagnosticsGroupPtr->updateDataAndWidgets();
		    		DiagnosticsGroupPtr->enableAtNewPosition( 0, channelVerticalPosition(J+1) );
		    	}
		    	else{
					if(TableOfGroupsPtr[J] != nullptr){
						TableOfGroupsPtr[J]->position( 0, channelVerticalPosition(J+2) );
						TableOfGroupsPtr[J]->setBottomLineVisibility();
					}
		    	}
	    	}
	    }
	}
}

static void remoteComputerControlCallback(Fl_Widget* Widget, void* Data){
	(void)Widget; // intentionally unused
	(void)Data; // intentionally unused

	assert( 0 != IsModbusTcpSlave );

	if (0 == ControlFromGuiHere){
		ControlFromGuiHere = 1;
	}
	else{
		ControlFromGuiHere = 0;
	}

	pthread_mutex_t xLock = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_lock( &xLock );
	TableOfSharedDataForTcpServer[0][TCP_SERVER_ADDRESS_IS_REMOTE_CONTROL] = ControlFromGuiHere? 0:1;
	pthread_mutex_unlock( &xLock );

	updateMainApplicationLabel( nullptr );
	RemoteComputerControlButton->label( TextOfRemoteComputerControlButton[ ControlFromGuiHere? 0:1 ] );
	RemoteComputerControlButton->redraw();

	if (0 <= SetPointInputGroupPtr->getChannelDisplayingSetPointEntryDialog()){
		closeSetPointDialogInternals();
	}
}

static void drawMarkInCircle( int x, int y, int w, int h, CommunicationStatesClass TransmissionState ){
	if ((CommunicationStatesClass::HEALTHY == TransmissionState) || (CommunicationStatesClass::TEMPORARY_ERRORS == TransmissionState) ||
			(CommunicationStatesClass::WRONG_PHYSICAL_ID == TransmissionState))
	{
		if (CommunicationStatesClass::HEALTHY == TransmissionState){
			fl_color( CIRCLE_MARK_GREEN );
		}
		else if(CommunicationStatesClass::TEMPORARY_ERRORS == TransmissionState){
			fl_color( CIRCLE_MARK_YELLOW );
		}
		else{
			fl_color( FL_RED );
		}
        fl_pie(x, y, w, h, 0, 360);
        fl_color(FL_WHITE);
        // to improve the symmetry of the drawing
        int x1, xw1, xw2, y1, y2, yw1, yw2;
        x1 = x+(20*w)/100;
        xw1 = (40*w)/100;
        xw2 = (20*w)/100;
        y1 = y+(38*h)/100;
        y2 = y+(62*h)/100;
        yw1 = 1+h/50;
        yw2 = 3*yw1;
        fl_polygon( x1,     y1+yw1, x1+xw1,     y1+yw1, x1+xw1,     y1,     x1,     y1 );
        fl_polygon( x1+xw1, y1-yw2, x1+xw1+xw2, y1+yw1, x1+xw1,     y1+yw1 );
        fl_polygon( x1+xw2, y2,     x1+xw2+xw1, y2,     x1+xw2+xw1, y2-yw1, x1+xw2, y2-yw1 );
        fl_polygon( x1, 	y2-yw1, x1+xw2,     y2+yw2, x1+xw2,     y2-yw1 );
	}
	else if ((CommunicationStatesClass::PORT_NOT_OPEN == TransmissionState) ||
			(CommunicationStatesClass::PERMANENT_ERRORS == TransmissionState))
	{ // 'x' mark
        fl_color( X_MARK_GRAY );
        fl_pie(x, y, w, h, 0, 360);
        fl_color( FL_WHITE );
        fl_polygon( x+11*w/40, y+27*h/40, x+13*w/40, y+29*h/40, x+29*w/40, y+13*h/40, x+27*w/40, y+11*h/40 );
        fl_polygon( x+11*w/40, y+13*h/40, x+13*w/40, y+11*h/40, x+29*w/40, y+27*h/40, x+27*w/40, y+29*h/40 );
	}
	else{
		// internal error
        fl_color(FL_RED); // '-' mark
        fl_pie(x, y, w, h, 0, 360);
        fl_color(FL_BLACK);
        fl_polygon( x+12*w/40, y+18*h/40, x+28*w/40, y+18*h/40, x+28*w/40, y+22*h/40, x+12*w/40, y+22*h/40 );
	}
}

// This function returns vertical position of a group of widgets that relates to a given channel
static int channelVerticalPosition( uint16_t ChannelIndex ){
	return FIRST_GROUP_OF_WIDGETS_Y + ChannelIndex * GROUPS_OF_WIDGETS_SPACING;
}

// Function called by Fl::awake() to refresh the widgets in a given group
void updateChannelWidgets(void* Data) {
	ChannelGuiGroup* GroupPtr;
	uint16_t Channel;
	double FloatingPointValueOfCurrent, FloatingPointValueSetPoint;
	DataSharingInterface* InterfaceDataPtr;

	if (nullptr == Data){
		return;
	}
	GroupPtr = (ChannelGuiGroup*)Data;
	Channel = GroupPtr->getGroupID();
	InterfaceDataPtr = &TableOfSharedDataForGui[Channel];

	if (0 != GroupPtr->visible()){

		GroupPtr->refreshPowerOnOffLabel( InterfaceDataPtr->getStateOfCommunication(), InterfaceDataPtr->getPowerSwitchState() );

		// the calculation is described in the documentation
		FloatingPointValueOfCurrent = 0.01 * (double)(InterfaceDataPtr->getModbusRegister(MODBUS_ADDRES_CURRENT_FILTERED));

		FloatingPointValueSetPoint = (200.0/65536.0) * (double)(InterfaceDataPtr->getModbusRegister(MODBUS_ADDRES_REQUIRED_VALUE));

		GroupPtr->refreshNumericValues( InterfaceDataPtr->getStateOfCommunication(), FloatingPointValueOfCurrent, FloatingPointValueSetPoint );

		GroupPtr->refreshStatusLabels( InterfaceDataPtr->getStateOfCommunication(),
				InterfaceDataPtr->getModbusRegister(MODBUS_ADDRES_SLAVE_STATUS) );

		GroupPtr->refreshPhysicalID( InterfaceDataPtr->getPowerSupplyUnitId() );

		GroupPtr->updateSettingsButtonsAndPowerDownWigets( InterfaceDataPtr->getStateOfCommunication(), InterfaceDataPtr->getPoweringDownState() );
	}
	else{
		if (0 == Channel){
			if (0 != LargeErrorMessage->visible()){
				LargeErrorMessage->redraw();
			}
		}
	}

	if (DiagnosticsGroupPtr->getChannelDisplayingDiagnostics() == Channel){
		if (0 != DiagnosticsGroupPtr->visible()){
			DiagnosticsGroupPtr->updateDataAndWidgets();
		}
		DiagnosticsGroupPtr->redraw();
	}

	if (SetPointInputGroupPtr->getChannelDisplayingSetPointEntryDialog() == Channel){
		SetPointInputGroupPtr->redraw();
	}

	GroupPtr->redraw();

	if (UpdateConfigurableWidgets){
		UpdateConfigurableWidgets = false;
		if (IsModbusTcpSlave){
			RemoteComputerControlButton->show();
		}
		else{
			ComputerLabelPtr->label(TextOfComputerLabel[1]);
		}
		ComputerLabelPtr->show();
		updateMainApplicationLabel( nullptr );
	}
}

void displayConfigurationFileErrorMessage(void* Data){
	(void)Data; // intentionally unused
	LargeErrorMessage->show();
	LargeErrorMessage->redraw();
}

void displayTcpConnectionErrorMessage(void* Data){
	(void)Data; // intentionally unused
	LargeErrorMessage->label(
			"Problem z połączeniem TCP\nJeżeli połączenie przed chwilą działało,\nodczekaj 1 minutę i uruchom aplikację ponownie\nW przeciwnym razie sprawdź połączenie (np. ping)");
	LargeErrorMessage->show();
	LargeErrorMessage->redraw();
}

void displayChannelWidgets(void* Data){
	ChannelGuiGroup* TemporaryGroupPtr = (ChannelGuiGroup*)Data;
	TemporaryGroupPtr->setDescriptionLabel();
	TemporaryGroupPtr->truncateDescription();
	TemporaryGroupPtr->show();
}

void restoreChannelWidgets(void* Data){
	(void)Data; // intentionally unused
	uint8_t J;
	LargeErrorMessage->hide();
	for (J = 0; J < NumberOfChannels; J++) {
		TableOfGroupsPtr[J]->restoreInitialState();
	}
	for ( ; J < MAX_NUMBER_OF_SERIAL_PORTS; J++) {
		TableOfGroupsPtr[J]->hide();
	}
}

void displayTcpConnectionErrorMessageAndHideChannelWidgets(void* Data){
	uint8_t J;
	uint8_t ErrorCode = *(uint8_t*)Data;
	for (J = 0; J < MAX_NUMBER_OF_SERIAL_PORTS; J++) {
		if(0 != TableOfGroupsPtr[J]->visible()){
			TableOfGroupsPtr[J]->hide();
		}
	}
	hideAuxiliaryGroups();
	if (ErrorCode < (uint8_t)(sizeof(TableTcpErrorTexts)/sizeof(TableTcpErrorTexts[0]))){
		LargeErrorMessage->label( TableTcpErrorTexts[ErrorCode] );
	}
	else{
		LargeErrorMessage->label( " Internal error " );
	}
	LargeErrorMessage->show();
}

// This function hides *SetPointInputGroupPtr, *DiagnosticsGroupPtr and *BlankRectanglePtr
void hideAuxiliaryGroups(void){
	SetPointInputGroupPtr->closeDialog();
	DiagnosticsGroupPtr->closeGroup();
	if( 0 != BlankRectanglePtr->visible() ){
		BlankRectanglePtr->hide();
	}
}

// This function sets the application label depending on the flags: IsModbusTcpSlave and ControlFromGuiHere
void updateMainApplicationLabel( void* Data ){
	(void)Data; // intentionally unused
	if (0 != IsModbusTcpSlave){
		if (0 != ControlFromGuiHere){
			ApplicationWindow->label(MainWindowNameInModeLocalUppercase);
		}
		else{
			ApplicationWindow->label(MainWindowNameInModeRemoteLowercase);
		}
	}
	else{
		if (0 != ControlFromGuiHere){
			ApplicationWindow->label(MainWindowNameInModeRemoteUppercase);
		}
		else{
			ApplicationWindow->label(MainWindowNameInModeLocalLowercase);
		}
	}
}

// This function closes set-point dialog if it is open
void closeSetpointDialogIfActive(void* Data){
	(void)Data; // intentionally unused
    if (0 <= SetPointInputGroupPtr->getChannelDisplayingSetPointEntryDialog()){
    	closeSetPointDialogInternals();
    }
}

//.................................................................................................

