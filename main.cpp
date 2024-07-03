//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"
#include <cstdint>

#define WATER_LEVEL_THRESHOLD 0.8
#define NUMBER_OF_AVG_SAMPLES 100
#define DEBOUNCE_TIME 50000
#define MAIN_DELAY 10
#define MANUAL_MODE_BUTTON 0
#define SOLENOID_VALVE_1_BUTTON 1
#define SOLENOID_VALVE_2_BUTTON 2
#define WATER_PUMP_BUTTON 3
#define NUMBER_OF_TANKS 3

typedef struct {
    DigitalIn pin;
    int state;
    int lastState;
    Timer debounceTimer;
    bool pressed;
    bool processed;
} Button;

typedef struct {
    AnalogIn levelSensor;
    DigitalOut actuator; // Solenoid Valve or Water Pump
    DigitalOut led;
    uint8_t buttonIndex;
    float levelSensorReading;
    float levelSensorReadingsAverage;
    float levelSensorReadingsSum;
    float levelSensorReadingsArray[NUMBER_OF_AVG_SAMPLES];
    float tankLevel;
    float tankCapacity;
} Tank;

Button buttons[4] = {
    {DigitalIn(D5), 1, 1, Timer(), false, false},
    {DigitalIn(D6), 1, 1, Timer(), false, false},
    {DigitalIn(D7), 1, 1, Timer(), false, false},
    {DigitalIn(D8), 1, 1, Timer(), false, false},
};

Tank tanks[NUMBER_OF_TANKS] = {
    {AnalogIn(A0), DigitalOut(D2), DigitalOut(LED1), WATER_PUMP_BUTTON, 0, 0, 0, 0, 0, 0},
    {AnalogIn(A1), DigitalOut(D3), DigitalOut(LED2), SOLENOID_VALVE_1_BUTTON, 0, 0, 0, 0, 0, 0},
    {AnalogIn(A2), DigitalOut(D4), DigitalOut(LED3), SOLENOID_VALVE_2_BUTTON, 0, 0, 0, 0, 0, 0},
};

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

bool manualMode = false;

void inputsInit();
void outputsInit();
void getTanksWaterLevel();
void manageWaterLevel();
void availableCommands();
void debounceButton(Button*);
void buttonsInit();
void uartTask();
void manualLevelManagement();
void checkManualManagementSwitch();

int main()
{
    inputsInit();
    outputsInit();
    buttonsInit();
    while(true) {
        if(manualMode == true) {
            manualLevelManagement();
        } else {
            checkManualManagementSwitch();
            getTanksWaterLevel();
            manageWaterLevel();
        }
        uartTask();
    }
}

void inputsInit() {
}

void outputsInit() {
    for (int i = 0; i < NUMBER_OF_TANKS; i++) {
        tanks[i].actuator = OFF;
        tanks[i].led = OFF;
    }
}

void getTanksWaterLevel() {
    static int levelSensorsSampleIndex = 0;

    for (int i = 0; i < NUMBER_OF_TANKS; i++){
        tanks[i].levelSensorReadingsArray[levelSensorsSampleIndex] = tanks[i].levelSensor.read();
    }

    levelSensorsSampleIndex += 1;

    if (levelSensorsSampleIndex >= NUMBER_OF_AVG_SAMPLES) {
        levelSensorsSampleIndex = 0;
    }

    for(int i = 0; i < NUMBER_OF_TANKS; i++) {
        tanks[i].levelSensorReadingsSum = 0;
    }

    for(int i = 0; i < NUMBER_OF_TANKS; i++){
        for (int j = 0; j < NUMBER_OF_AVG_SAMPLES; j++) {
            tanks[i].levelSensorReadingsSum += tanks[i].levelSensorReadingsArray[j];
        }
    }
    
    for(int i = 0; i < NUMBER_OF_TANKS; i++) {
        tanks[i].levelSensorReadingsAverage = tanks[i].levelSensorReadingsSum / NUMBER_OF_AVG_SAMPLES;
    }
}

void manageWaterLevel() {
    for(int i = 0; i < NUMBER_OF_TANKS; i++) {
        if (tanks[i].levelSensorReadingsAverage < WATER_LEVEL_THRESHOLD) {
            tanks[i].actuator = ON;
            tanks[i].led = ON;
        } else {
            tanks[i].actuator = OFF;
            tanks[i].led = OFF;
        }
    }
}

void uartTask()
{
    char receivedChar = '\0';
    char str[100];
    int stringLength;
    if( uartUsb.readable() ) {
        uartUsb.read( &receivedChar, 1 );
        switch (receivedChar) {
        case 'T':
            uartUsb.write( "\n", 1);
            sprintf ( str, "Tank 1 level %.2f\r\n", tanks[0].levelSensorReadingsAverage );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength);

            sprintf ( str, "Tank 2 level %.2f\r\n", tanks[1].levelSensorReadingsAverage );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength);

            sprintf ( str, "Tank 3 level %.2f\r\n", tanks[2].levelSensorReadingsAverage );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength);
            break;

        case 'S':
            uartUsb.write( "\n", 1);
            if (tanks[0].actuator || tanks[1].actuator || tanks[2].actuator) {
                if (tanks[0].actuator)
                    uartUsb.write( "Tank 1 is being filled.\r\n", 25);
                
                if (tanks[1].actuator)
                    uartUsb.write( "Tank 2 is being filled.\r\n", 25);
                
                if (tanks[2].actuator)
                    uartUsb.write( "Tank 3 is being filled.\r\n", 25);
            } else {
                uartUsb.write( "All tanks are full of water.\r\n", 30);
            }
            break;

        default:
            availableCommands();
            break;

        }
    }
}

void availableCommands()
{
    uartUsb.write( "Available commands:\r\n", 21 );
    uartUsb.write( "Press 'T' to get the water level in all tanks.\r\n", 48 );
    uartUsb.write( "Press 'S' to get the solenoid valves and pump system state.\r\n", 61 );
}

void debounceButton(Button *button) {
    uint8_t reading = button->pin.read();

    if(reading != button->lastState) {
        button->debounceTimer.reset();
    }

    if(button->debounceTimer.elapsed_time().count() >= DEBOUNCE_TIME) {
        if(reading != button->state) {
            button->state = reading;
            button->processed = false;
            button->pressed = false;
        }

        if(button->state == OFF && button->processed == false) {
            button->pressed = true;
        } else {
            button->pressed = false;
        }
    }

    button->lastState = reading;
}

void buttonsInit() {
    for (int i = 0; i < 4; i++) {
        buttons[i].debounceTimer.start();
        buttons[i].pin.mode(PullUp);
    }
}

void manualLevelManagement() {
    for(int i = 0; i < 4; i++) {
        debounceButton(&buttons[i]);
    }

    if(buttons[MANUAL_MODE_BUTTON].pressed == true) {
        manualMode = !manualMode;
        buttons[MANUAL_MODE_BUTTON].processed = true;
    } else {
        for(int i = 0; i < NUMBER_OF_TANKS; i++) {
            if(buttons[tanks[i].buttonIndex].pressed == true) {
                tanks[i].actuator == 1 ? tanks[i].actuator = 0 : tanks[i].actuator = 1;
                tanks[i].led == 1 ? tanks[i].led = 0 : tanks[i].led = 1;
                buttons[tanks[i].buttonIndex].processed = true;
            }
        }
    }
}

void checkManualManagementSwitch() {
    debounceButton(&buttons[MANUAL_MODE_BUTTON]);
    if(buttons[MANUAL_MODE_BUTTON].pressed == true) {
        manualMode = !manualMode;
        buttons[MANUAL_MODE_BUTTON].processed = true;
    }       
}