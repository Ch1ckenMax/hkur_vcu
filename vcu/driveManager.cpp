#include "driveManager.h"
#include "readyToDriveSound.h"
#include <DFRobot_MCP2515.h>

void DriveManager::setDataPacket(unsigned int torque, int angularVelocity, bool directionForward, bool inverter, bool inverterDischarge, bool speedMode, int torqueLimit) {
    motorControllerPacket[0] = torque % 256;
    motorControllerPacket[1] = torque / 256;
    motorControllerPacket[2] = angularVelocity % 256;
    motorControllerPacket[3] = angularVelocity / 256;
    motorControllerPacket[4] = (directionForward ? 1 : 0);
    motorControllerPacket[5] = (inverter ? 1 : 0) + 2 * (inverterDischarge ? 1 : 0) + 4 * (speedMode ? 1 : 0);
    motorControllerPacket[6] = torqueLimit % 256;
    motorControllerPacket[7] = torqueLimit / 256;
}

DriveManager::DriveManager(uint8_t driveModePinNumber, uint8_t reverseModePinNumber, uint8_t throttleAPinNumber, uint8_t throttleBPinNumber){
    //Initialize the private variables
    for(int i = 0; i < 8; i++) this->motorControllerPacket[i] = 0;

    //Set the pins
    this->driveModePin = driveModePinNumber;
    this->reverseModePin = reverseModePinNumber;
    this->throttlePinA = throttleAPinNumber;
    this->throttlePinB = throttleBPinNumber;   
}

void DriveManager::initializePinMode(){
    pinMode(driveModePin, INPUT_PULLUP);
    pinMode(reverseModePin, INPUT_PULLUP);
}

void DriveManager::readDriveInput(){
    //Get the values
    throttleSensorValues[0] = analogRead(throttlePinA);
    throttleSensorValues[1] = analogRead(throttlePinB);
}

void DriveManager::mapThrottle(int throttleMinA, int throttleMaxA, int throttleMinB, int throttleMaxB, int maxTorque){
    throttle_A = map(throttleSensorValues[0], throttleMinA, throttleMaxA, 0, maxTorque);
    throttle_B = map(throttleSensorValues[1], throttleMinB, throttleMaxB, 0, maxTorque);
    throttle = (throttle_A + throttle_B) / 2;
}

void DriveManager::processDriveInput(ReadyToDriveSound* r2DSound, int maxTorque){
    // Prevent overflow and apply deadzone
    if (throttle > maxTorque * 0.975) { //Deadzone 2.5%
        throttle = maxTorque;
    }
    else if (throttle < maxTorque * 0.025) { // Deadzone 2.5%
        throttle = 0;
    }

    Serial.print("Final throttle: ");
    Serial.print(throttle);
    Serial.print("\n");

    //Set-up the drive mode
    if (!digitalRead(driveModePin)) {
        driveMode = DriveManager::DRIVE_MODE_DRIVE;
        inverterEnabled = throttle >= maxTorque * 0.025; //only turn on the inverter if there is throttle signal for accelerating
        driveForward = true;
    }
    else if (!digitalRead(reverseModePin)) {
        driveMode = DriveManager::DRIVE_MODE_REVERSE;
        inverterEnabled = throttle >= maxTorque * 0.025; //only turn on the inverter if there is throttle signal for accelerating
        driveForward = false;
    }
    else {
        driveMode = DriveManager::DRIVE_MODE_NEUTRAL;
        inverterEnabled = false;
        driveForward = true;

        //Reset the ready to drive sound
        r2DSound->turnOffIfBeeping();
        r2DSound->setBeepState(ReadyToDriveSound::BEEP_NOT_STARTED);
    }

}

uint8_t DriveManager::sendPacketToMotorController(DFRobot_MCP2515* can){
    setDataPacket(this->throttle, 0, this->driveForward, this->inverterEnabled, !(this->inverterEnabled), false, 0);
    return can->sendMsgBuf(0x0c0, 0, 8, motorControllerPacket); //send. 0x0c0 is defined by the docs of the motor controller
}

uint8_t DriveManager::sendStopEnginePacket(DFRobot_MCP2515* can){
    setDataPacket(0, 0, this->driveForward, false, true, false, 0);
    return can->sendMsgBuf(0x0c0, 0, 8, motorControllerPacket);
}

uint8_t DriveManager::getDriveMode(){
    return this->driveMode;
}

unsigned int* DriveManager::getThrottleSensorValues(){
    return this->throttleSensorValues;
}

long DriveManager::getThrottle(){
    return this->throttle;
}

void DriveManager::printData(){
      Serial.print("Sensor A: ");
      Serial.print(this->throttleSensorValues[0]);
      Serial.print(". Sensor B: ");
      Serial.print(this->throttleSensorValues[1]);
      Serial.print(". Throttle A: ");
      Serial.print(this->throttle_A);
      Serial.print(". Throttle B: ");
      Serial.print(this->throttle_B);
      Serial.print(". Throttle: ");
      Serial.print(this->throttle);
      Serial.print("\n");
}