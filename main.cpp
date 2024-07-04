/**
* @file main.cpp
* @brief A non modularized firmware for a water level management system.
*
* This file contains the function and data structures used for the system.
*
*/

#include "mbed.h"
#include "arm_book_lib.h"
#include <cstdint>

#define WATER_LEVEL_THRESHOLD 0.8 /**< When the level sensor is below this value, the tank needs to be filled*/
#define NUMBER_OF_AVG_SAMPLES 100 /**< Number of samples to be used to calculate the analog sensor reading average*/
#define DEBOUNCE_TIME 50000 /**< Debounce time in microseconds*/
#define MAIN_DELAY 10
#define MANUAL_MODE_BUTTON 0 /**< Button to be used to switch between manual and automatic operation*/
#define SOLENOID_VALVE_1_BUTTON 1 /**< Button used to activate/deactivate the solenoid valve 1*/
#define SOLENOID_VALVE_2_BUTTON 2 /**< Button used to activate/deactivate the solenoid valve 2*/
#define WATER_PUMP_BUTTON 3 /**< Button used to activate/deactivate the water pump*/
#define NUMBER_OF_TANKS 3 /**< Number of tanks of the system*/

/** \struct Button
*
* @brief Structure to hold buttons data.
* 
* It is used to avoid code repetition and to make the code more maintainable.
*
* The struct holds information of the digital pin of the button, the current state of the button (after the debounce
* filter), the last state of the button (that can or can't be a rebound), the timer used for the debounce function,
* and some variables to know if the button was pressed and also processed (if the button was pressed we want to process
* that fact only once, no matter if the button is still pressed).
*/
typedef struct {
    DigitalIn pin; /**< Digital pin used by the button*/
    int state; /**< State used for the debounce function*/
    int lastState; /**< Last state used for the debounce function*/
    Timer debounceTimer; /**< Timer used for the debouncer. Each button has a timer*/
    bool pressed; /**< This attribute is used to know if the button was pressed*/
    bool processed; /**< This attribute is used to know if the button pressed was already processed*/
} Button;

/** \struct Tank
*
* @brief Structure to hold tanks data.
*
* This struct holds information about the actuator that will be used to fill the tank (which digital output is used),
* which analog input is used for the water level sensor and which led will be used to indicate if the tank is being filled
* or not.
*
* For example, tank 1 is filled using water pump as the actuator. The led 1 is used to indicate to the user that the tank
* is being filled (and also for testing purposes in our case). The tank 2 is filled by gravity, opening the solenoid valve
* 1. The led 2 is used to indicate to the user that the tank 2 is being filled. 
* 
* Aditionally, some variables are defined to be used when calculating the average reading of the analog input for the
* level sensor.
*/
typedef struct {
    AnalogIn levelSensor; /**< Analog input for the level sensor of the tank*/
    DigitalOut actuator; /**< The actuator used to fill the tank when the water level is below the threshold.*/
    DigitalOut led; /**< Led indicator used to know which tank is being filled. We've used the 3 leds of the NUCLEO board*/
    uint8_t buttonIndex;
    // The variables below are used to calculate the average of the level sensors readings
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
}; /**< Array of buttons. Helps avoiding code repetition and handling the button events in an easy way.*/

Tank tanks[NUMBER_OF_TANKS] = {
    {AnalogIn(A0), DigitalOut(D2), DigitalOut(LED1), WATER_PUMP_BUTTON, 0, 0, 0, 0, 0, 0},
    {AnalogIn(A1), DigitalOut(D3), DigitalOut(LED2), SOLENOID_VALVE_1_BUTTON, 0, 0, 0, 0, 0, 0},
    {AnalogIn(A2), DigitalOut(D4), DigitalOut(LED3), SOLENOID_VALVE_2_BUTTON, 0, 0, 0, 0, 0, 0},
}; /**< Array of tanks. Helps avoiding code repetition and managing the level sensors readings and actuators activation.*/

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

/**
* @brief Iterates over the tanks array and initiate the actuators and leds with the OFF value.
*/
void outputsInit() {
    for (int i = 0; i < NUMBER_OF_TANKS; i++) {
        tanks[i].actuator = OFF;
        tanks[i].led = OFF;
    }
}

/**
* @brief Function used to calculate the water level in the tanks, saving the information in the structs of the tanks array.
*
* The sensors readings are saved into the levelSensorsReadingsArray. All the readings in the array are then averaged to
* obtain a more reliable analog measure.
*
* The arrays are filled replacing the older measures by the new ones. At the begining, when the array is empty, the measure
* is not accurate. In our case, the system gets stable once the array is filled (imperceptible time).
*
* The Tank struct and the tanks array helps to avoid code repetition here. All the measures are saved in the Tank structs
* initialized in the tanks array.
*/
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

/**
* @brief This is the management system for the automatic water level management.
*
* Iterate over all tanks in the systems and check if the water level is below the water level threshold.
* In case it is, turn on the actuator and the led of the tank. In case it is not turn them off.
*/
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

/**
* @brief Serial GUI to ask the water level management system for information about the tanks.
*/
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

/**
* @brief Available commands for the serial GUI.
*/
void availableCommands()
{
    uartUsb.write( "Available commands:\r\n", 21 );
    uartUsb.write( "Press 'T' to get the water level in all tanks.\r\n", 48 );
    uartUsb.write( "Press 'S' to get the solenoid valves and pump system state.\r\n", 61 );
}

/**
* @brief Debounce function for the buttons.
*
* \todo Change it for a FSM. I didn't use it because it is out of the assignment scope, since FSM are a topic covered
* \todo in the next chapters.
*/
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

/**
* @brief Init the buttons of the system as pull-ups. Init the timers for the buttons.
*
* \attention The timers were not introduced in the course yet. I found these tools useful for the debounce function.
* \attention Nevertheless I'll implement a delay in the main function, and check in each button if the debounce time
* \attention has ended.
*/
void buttonsInit() {
    for (int i = 0; i < 4; i++) {
        buttons[i].debounceTimer.start();
        buttons[i].pin.mode(PullUp);
    }
}

/**
* @brief Function to check if an actuator button was pressed, or to check if the manual system was deactivated.
*
* If a button of the actuator was pressed, we used that information to activate/deactivate the actuator and the led.
* Once this happens, the processed flag of the button is activated and the debounce function prevents re-entering
* the conditional on line 310 (we ensure that the program enters the conditional only once per button press). 
*/
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

/**
* @brief Check if the manual operation was activated (when working in the automatic management operation)
*/
void checkManualManagementSwitch() {
    debounceButton(&buttons[MANUAL_MODE_BUTTON]);
    if(buttons[MANUAL_MODE_BUTTON].pressed == true) {
        manualMode = !manualMode;
        buttons[MANUAL_MODE_BUTTON].processed = true;
    }       
}