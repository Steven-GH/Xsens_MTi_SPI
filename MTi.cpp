#include "MTi.h"
#include <SPI.h>


bool MTi::detect(uint16_t timeout) {
  //Send goToConfig messages until goToConfigAck is received from the device
  Serial.println("Scanning for MTi.");                                                       //Clear the measurement/notification pipes (without printing) before configuring.

  long int starttime = millis();
  while ((millis() - starttime) < timeout) {
    goToConfig();
    delay(250);

    readMessages();
    if (deviceInConfigMode()) {
      Serial.println("Device detected.");
      return true;
    }
  }
  Serial.println("Failed to detect device.");
  return false;
}



void MTi::requestDeviceInfo() {
  //Request device info from the MTi using Xbus commands. Refer to the MT Low Level Communication Protocol Document for more information on the commands used here:
  //https://mtidocs.xsens.com/mt-low-level-communication-protocol-documentation

  if (!deviceInConfigMode()) {
    Serial.println("Cannot request device info. Device is not in Config Mode.");
    return;
  }
  readMessages();                                                                             //Clear the measurement/notification pipes before configuring.
  Serial.println("Requesting device info...");

  uint8_t reqProductCode[] = {0x1C, 0x00};                                                    //reqProductCode Xbus message
  sendMessage(reqProductCode, sizeof(reqProductCode));
  delay(1000);

  readMessages();

  uint8_t reqFWRev[] = {0x12, 0x00};                                                          //reqFWRev Xbus message
  sendMessage(reqFWRev, sizeof(reqFWRev));
  delay(1000);

  readMessages();
}


void MTi::configureOutputs() {
  //Configure the outputs of the MTi using Xbus commands. Refer to the MT Low Level Communication Protocol Document for more information on the commands used here:
  //https://mtidocs.xsens.com/mt-low-level-communication-protocol-documentation

  if (!deviceInConfigMode()) {
    Serial.println("Cannot configure device. Device is not in Config Mode.");
    return;
  }
  readMessages();                                                                             //Clear the measurement/notification pipes (without printing) before configuring.


  if (xbus.productCode == '1') {//MTi-1 IMU
    Serial.println("Configuring Acceleration/RateOfTurn at 1 Hz.");
    uint8_t outputConfig[] = {0xC0, 0x08, 0x40, 0x20, 0x00, 0x01, 0x80, 0x20, 0x00, 0x01};    //setOutputConfiguration Xbus message with Data fields 0x40 0x20 0x00 0x01 / 0x40 0x20 0x00 0x01 (Acceleration/RateOfTurn, 32-bit float, at 1 Hz)
    //Serial.println("Configuring Acceleration/RateOfTurn at 10 Hz.");
    //uint8_t outputConfig[] = {0xC0, 0x08, 0x40, 0x20, 0x00, 0x0A, 0x80, 0x20, 0x00, 0x0A};  //setOutputConfiguration Xbus message with Data fields 0x40 0x20 0x00 0x01 / 0x40 0x20 0x00 0x01 (Acceleration/RateOfTurn, 32-bit float, at 10 Hz)
    sendMessage(outputConfig, sizeof(outputConfig));

  } else if (xbus.productCode == '2' | xbus.productCode == '3') {//"MTi-2 VRU or MTi-3 AHRS
    Serial.println("Configuring Euler angles at 1 Hz.");
    uint8_t outputConfig[] = {0xC0, 0x04, 0x20, 0x30, 0x00, 0x01};                            //setOutputConfiguration Xbus message with Data field 0x20 0x30 0x00 0x01 (EulerAngles, 32-bit float, at 1 Hz)
    //Serial.println("Configuring Euler angles at 10 Hz.");
    //uint8_t outputConfig[] = {0xC0, 0x04, 0x20, 0x30, 0x00, 0x0A};                          //setOutputConfiguration Xbus message with Data field 0x20 0x30 0x00 0x01 (EulerAngles, 32-bit float, at 10 Hz)
    sendMessage(outputConfig, sizeof(outputConfig));

  } else if (xbus.productCode == '7' | xbus.productCode == '8') {//"MTi-7 or MTi-8 GNSS/INS
    Serial.println("Configuring Euler Angles and Latitude/Longitude at 1 Hz.");
    uint8_t outputConfig[] = {0xC0, 0x08, 0x50, 0x40, 0x00, 0x01, 0x20, 0x30, 0x00, 0x01};    //setOutputConfiguration Xbus message with Data fields 0x50 0x40 0x00 0x01 / 0x20 0x30 0x00 0x01 (LatLon/EulerAngles, 32-bit float, at 1 Hz)
    //Serial.println("Configuring Euler Angles and Latitude/Longitude at 10 Hz.");
    //uint8_t outputConfig[] = {0xC0, 0x08, 0x50, 0x40, 0x00, 0x0A, 0x20, 0x30, 0x00, 0x0A};  //setOutputConfiguration Xbus message with Data fields 0x50 0x40 0x00 0x01 / 0x20 0x30 0x00 0x01 (LatLon/EulerAngles, 32-bit float, at 10 Hz)
    sendMessage(outputConfig, sizeof(outputConfig));

  } else {
    Serial.println("Could not configure device. Device's product code is unknown.");
  }
  delay(1000);
  readMessages();

}



void MTi::goToConfig() {
  Serial.println("Entering configuration mode.");
  uint8_t goToConfig[] = {0x30, 0x00};                                                        //goToConfig Xbus message
  sendMessage(goToConfig, sizeof(goToConfig));
}


void MTi::goToMeasurement() {
  Serial.println("Entering measurement mode.");
  uint8_t goToMeas[] = {0x10, 0x00};                                                          //goToMeasurement Xbus message
  sendMessage(goToMeas, sizeof(goToMeas));
}


void MTi::sendMessage(uint8_t *message, uint8_t numBytes) {
  //Compute the checksum for the Xbus message to be sent. See https://mtidocs.xsens.com/messages for details.
  uint8_t checksum = 0x01;
  for (int i = 0; i < numBytes; i++) {
    checksum -= message[i];
  }
  message[numBytes] = checksum;                                                                     //Add the checksum at the end of the Xbus message

  uint8_t buffer[] = {XSENS_CONTROL_PIPE, 0xFF, 0xFF, 0xFF};

  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
  digitalWrite(nCS, LOW);
  SPI.transfer(buffer, sizeof(buffer));
  SPI.transfer(message, numBytes + 1);
  digitalWrite(nCS, HIGH);
  SPI.endTransaction();
}


void MTi::readMessages() {                                                                           //read new messages until measurement/notification pipes are empty
  while (digitalRead(drdy)) {
    xbus.read(nCS);
  }
}


void MTi::printData() {
  if (!isnan(getAcceleration()[0])) {                                                                       //Only true if this data has been received once before
    Serial.println("Acceleration [m/s^2]:");
    for (int i = 0 ; i < 3; ++i) {
      Serial.print(getAcceleration()[i]);                                                                   //Print the last read value
      Serial.print(" ");
    }
    Serial.println(" ");
  }

  if (!isnan(getRateOfTurn()[0])) {                                                                         //Only true if this data has been received once before
    Serial.println("Rate Of Turn [deg/s]:");
    for (int i = 0 ; i < 3; ++i) {
      Serial.print(getRateOfTurn()[i] * 180 / PI);                                                          //Print the last read value
      Serial.print(" ");
    }
    Serial.println(" ");
  }

  if (!isnan(getEulerAngles()[0])) {                                                                        //Only true if this data has been received once before
    Serial.println("Euler angles [deg]:");
    for (int i = 0 ; i < 3; ++i) {
      Serial.print(getEulerAngles()[i]);                                                                    //Print the last read value
      Serial.print(" ");
    }
    Serial.println(" ");
  }

  if (xbus.productCode == '7' | xbus.productCode == '8') {//MTi-7 or MTi-8 GNSS/INS
    Serial.println("Lat/Lon [deg]:");
    for (int i = 0 ; i < 2; ++i) {
      Serial.print(getLatLon()[i], 5);                                                                      //Print the last read value. Will be "NaN" as long as there is no GNSS fix
      Serial.print(" ");
    }
    Serial.println(" ");
  }
}
