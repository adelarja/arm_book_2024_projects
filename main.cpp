//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#define WATER_LEVEL_THRESHOLD 0.8
#define NUMBER_OF_AVG_SAMPLES 100

DigitalOut solenoidValve1(D2);
DigitalOut solenoidValve2(D3);
DigitalOut waterPump(D4);

AnalogIn levelSensor1(A0);
AnalogIn levelSensor2(A1);
AnalogIn levelSensor3(A2);

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

void inputsInit();
void outputsInit();
void getTanksWaterLevel();
void manageWaterLevel();

int main()
{
    inputsInit();
    outputsInit();
    while(true) {
        getTanksWaterLevel();
        manageWaterLevel();
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

    if (levelSensorsSampleIndex >= NUMBER_OF_AVG_SAMPLES) {
        levelSensorsSampleIndex = 0;
    }

    levelSensor1ReadingsSum = 0;
    levelSensor2ReadingsSum = 0;
    levelSensor3ReadingsSum = 0;

    for (int i = 0; i < NUMBER_OF_AVG_SAMPLES; i++) {
        levelSensor1ReadingsSum += levelSensor1ReadingsArray[i];
        levelSensor2ReadingsSum += levelSensor2ReadingsArray[i];
        levelSensor2ReadingsSum += levelSensor2ReadingsArray[i];
    }

    levelSensor1ReadingsAverage = levelSensor1ReadingsSum / NUMBER_OF_AVG_SAMPLES;
    levelSensor2ReadingsAverage = levelSensor2ReadingsSum / NUMBER_OF_AVG_SAMPLES;
    levelSensor3ReadingsAverage = levelSensor3ReadingsSum / NUMBER_OF_AVG_SAMPLES;
}

void manageWaterLevel() {
    if (levelSensor1ReadingsAverage < WATER_LEVEL_THRESHOLD) {
        waterPump = ON;
    } else {
        waterPump = OFF;
    }

    if (levelSensor2ReadingsAverage < WATER_LEVEL_THRESHOLD) {
        solenoidValve1 = ON;
    } else {
        solenoidValve1 = OFF;
    }

    if (levelSensor3ReadingsAverage < WATER_LEVEL_THRESHOLD) {
        solenoidValve2 = ON;
    } else {
        solenoidValve2 = OFF;
    }
}