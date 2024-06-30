//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"
#include <cstdint>

#define WATER_LEVEL_THRESHOLD 0.8
#define NUMBER_OF_AVG_SAMPLES 100
#define DEBOUNCE_TIME 30000
#define MAIN_DELAY 10
#define MANUAL_MODE_BUTTON 0
#define SOLENOID_VALVE_1_BUTTON 1
#define SOLENOID_VALVE_2_BUTTON 2
#define WATER_PUMP_BUTTON 3

typedef struct {
    DigitalIn pin;
    int state;
    int lastState;
    Timer debounceTimer;
    bool pressed;
} Button;

Button buttons[4] = {
    {DigitalIn(D5), 1, 1, Timer(), false},
    {DigitalIn(D6), 1, 1, Timer(), false},
    {DigitalIn(D7), 1, 1, Timer(), false},
    {DigitalIn(D8), 1, 1, Timer(), false},
};

DigitalOut solenoidValve1(D2);
DigitalOut solenoidValve2(D3);
DigitalOut waterPump(D4);

DigitalOut ld1(LED1);
DigitalOut ld2(LED2);
DigitalOut ld3(LED3);

AnalogIn levelSensor1(A0);
AnalogIn levelSensor2(A1);
AnalogIn levelSensor3(A2);

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

float levelSensor1Reading = 0.0;
float levelSensor1ReadingsAverage  = 0.0;
float levelSensor1ReadingsSum = 0.0;
float levelSensor1ReadingsArray[NUMBER_OF_AVG_SAMPLES];
float tank1Level = 0.0;

float levelSensor2Reading = 0.0;
float levelSensor2ReadingsAverage  = 0.0;
float levelSensor2ReadingsSum = 0.0;
float levelSensor2ReadingsArray[NUMBER_OF_AVG_SAMPLES];
float tank2Level = 0.0;

float levelSensor3Reading = 0.0;
float levelSensor3ReadingsAverage  = 0.0;
float levelSensor3ReadingsSum = 0.0;
float levelSensor3ReadingsArray[NUMBER_OF_AVG_SAMPLES];
float tank3Level = 0.0;

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
            getTanksWaterLevel();
            manageWaterLevel();
        }
        uartTask();
    }
}

void inputsInit() {
}

void outputsInit() {
    solenoidValve1 = OFF;
    solenoidValve2 = OFF;
    waterPump = OFF;
}

void getTanksWaterLevel() {
    static int levelSensorsSampleIndex = 0;

    levelSensor1ReadingsArray[levelSensorsSampleIndex] = levelSensor1.read();
    levelSensor2ReadingsArray[levelSensorsSampleIndex] = levelSensor2.read();
    levelSensor3ReadingsArray[levelSensorsSampleIndex] = levelSensor3.read();

    levelSensorsSampleIndex += 1;

    if (levelSensorsSampleIndex >= NUMBER_OF_AVG_SAMPLES) {
        levelSensorsSampleIndex = 0;
    }

    levelSensor1ReadingsSum = 0;
    levelSensor2ReadingsSum = 0;
    levelSensor3ReadingsSum = 0;

    for (int i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        levelSensor1ReadingsSum += levelSensor1ReadingsArray[i];
        levelSensor2ReadingsSum += levelSensor2ReadingsArray[i];
        levelSensor3ReadingsSum += levelSensor3ReadingsArray[i];
    }

    levelSensor1ReadingsAverage = levelSensor1ReadingsSum / NUMBER_OF_AVG_SAMPLES;
    levelSensor2ReadingsAverage = levelSensor2ReadingsSum / NUMBER_OF_AVG_SAMPLES;
    levelSensor3ReadingsAverage = levelSensor3ReadingsSum / NUMBER_OF_AVG_SAMPLES;
}

void manageWaterLevel() {
    if (levelSensor1ReadingsAverage < WATER_LEVEL_THRESHOLD) {
        waterPump = ON;
        ld1 = ON;
    } else {
        waterPump = OFF;
        ld1 = OFF;
    }

    if (levelSensor2ReadingsAverage < WATER_LEVEL_THRESHOLD) {
        solenoidValve1 = ON;
        ld2 = ON;
    } else {
        solenoidValve1 = OFF;
        ld2 = OFF;
    }

    if (levelSensor3ReadingsAverage < WATER_LEVEL_THRESHOLD) {
        solenoidValve2 = ON;
        ld3 = ON;
    } else {
        solenoidValve2 = OFF;
        ld3 = OFF;
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
            sprintf ( str, "Tank 1 level %.2f\r\n", levelSensor1ReadingsAverage );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength);

            sprintf ( str, "Tank 2 level %.2f\r\n", levelSensor2ReadingsAverage );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength);

            sprintf ( str, "Tank 3 level %.2f\r\n", levelSensor3ReadingsAverage );
            stringLength = strlen(str);
            uartUsb.write( str, stringLength);
            break;

        case 'S':
            uartUsb.write( "\n", 1);
            if (waterPump || solenoidValve1 || solenoidValve2) {
                if (waterPump)
                    uartUsb.write( "Tank 1 is being filled.\r\n", 25);
                
                if (solenoidValve1)
                    uartUsb.write( "Tank 2 is being filled.\r\n", 25);
                
                if (solenoidValve2)
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
        }

        if(button->state == LOW) {
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
    } else if(buttons[SOLENOID_VALVE_1_BUTTON].pressed == true) {
        solenoidValve1 == 1 ? solenoidValve1 = 0 : solenoidValve1 = 1;
        ld1 == 1 ? ld1 = 0: ld1 = 1;
    } else if(buttons[SOLENOID_VALVE_2_BUTTON].pressed == true) {
        solenoidValve2 == 1 ? solenoidValve2 = 0 : solenoidValve2 = 1;
        ld2 == 1 ? ld2 = 0: ld2 = 1;
    } else if(buttons[WATER_PUMP_BUTTON].pressed == true) {
        waterPump == 1 ? waterPump = 0 : waterPump = 1;
        ld3 == 1 ? ld3 = 0: ld3 = 1;
    }
}

void checkManualManagementSwitch() {
    debounceButton(&buttons[MANUAL_MODE_BUTTON]);
    if(buttons[MANUAL_MODE_BUTTON].pressed == true)
        manualMode = !manualMode;
}