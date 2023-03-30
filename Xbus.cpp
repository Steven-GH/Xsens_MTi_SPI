#include "Xbus.h"
#include <math.h>
#include <SPI.h>

Xbus::Xbus() {
  for (int i = 0; i < 3; ++i) {
    acc[i] = NAN;
    rot[i] = NAN;
    euler[i] = NAN;
  }
  for (int i = 0; i < 2; ++i) {
    latlon[i] = NAN;
  }
}


bool Xbus::read(uint8_t nCS) {
  readPipeStatus(nCS);
  if (notificationSize) {                                   //New notification message available to be read
    readPipeNotif(nCS);
    parseNotification(datanotif);
  }
  if (measurementSize) {                                    //New measurement packet available to be read
    readPipeMeas(nCS);
    parseMTData2(datameas, measurementSize);
    return true;                                            //Return true if new measurements were read
  } else {
    return false;
  }
}



void Xbus::readPipeStatus(uint8_t nCS) {
  uint8_t buffer[] = {XSENS_STATUS_PIPE, 0xFF, 0xFF, 0xFF};
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
  digitalWrite(nCS, LOW);
  SPI.transfer(buffer, sizeof(buffer));
  for (int i = 0; i < 4; i++) {
    status[i] = SPI.transfer(0xFF);
  }
  digitalWrite(nCS, HIGH);
  SPI.endTransaction();

  notificationSize = (uint16_t)status[0] | ((uint16_t)status[1] << 8);
  measurementSize = (uint16_t)status[2] | ((uint16_t)status[3] << 8);
}


void Xbus::readPipeNotif(uint8_t nCS) {
  uint8_t buffer[] = {XSENS_NOTIF_PIPE, 0xFF, 0xFF, 0xFF};
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
  digitalWrite(nCS, LOW);
  SPI.transfer(buffer, sizeof(buffer));
  for (int i = 0; i < notificationSize; i++) {
    datanotif[i] = SPI.transfer(0xFF);
  }
  digitalWrite(nCS, HIGH);
  SPI.endTransaction();
}



void Xbus::readPipeMeas(uint8_t nCS) {
  uint8_t buffer[] = {XSENS_MEAS_PIPE, 0xFF, 0xFF, 0xFF};
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE3));
  digitalWrite(nCS, LOW);
  SPI.transfer(buffer, sizeof(buffer));
  for (int i = 0; i < measurementSize; i++) {
    datameas[i] = SPI.transfer(0xFF);
  }
  digitalWrite(nCS, HIGH);
  SPI.endTransaction();
}



void Xbus::parseMTData2(uint8_t* data, uint8_t datalength) {
  if (datalength < 2)                                                           //Reached the end of the MTData2 message
    return;

  uint8_t XDI = data[0] ;                                                       //Xsens Data Identifier
  if (XDI == (uint8_t)MesID::MTDATA2) {                                         //Start of the MTData2 message
    uint8_t length = data[1];
    parseMTData2(data + 2, length);
  } else {
    uint8_t length = data[2];
    switch (((uint16_t)data[1] | ((uint16_t)data[0] << 8)) & (uint16_t)0xFFFF) { //Extract the 2-byte Xsens Data Identifier
      case (uint16_t)DataID::EULERANGLES:
        dataswapendian(data + 3, sizeof(float) * 3);
        memcpy(euler, data + 3, sizeof(float) * 3);
        break;
      case (uint16_t)DataID::ACCELERATION:
        dataswapendian(data + 3, sizeof(float) * 3);
        memcpy(acc, data + 3, sizeof(float) * 3);
        break;
      case (uint16_t)DataID::RATEOFTURN:
        dataswapendian(data + 3, sizeof(float) * 3);
        memcpy(rot, data + 3, sizeof(float) * 3);
        break;
      case (uint16_t)DataID::LATLON:
        dataswapendian(data + 3, sizeof(float) * 2);
        memcpy(latlon, data + 3, sizeof(float) * 2);
        break;
      default:
        break;
    }
    parseMTData2(data + length + 3, datalength - length - 3);                     //Move onto next data element within MTData2 packet
  }
}



void Xbus::parseNotification(uint8_t* notif) {                                           //Parse the most common notification messages
  uint8_t notifID = notif[0];
  switch (notifID) {
    case (uint8_t)MesID::WAKEUP: {
        Serial.println("Received WakeUp message.");
        break;
      }
    case (uint8_t)MesID::ERROR: {
        Serial.print("Received an error with code: "); Serial.println(notif[2]);
        break;
      }
    case (uint8_t)MesID::WARNING: {
        uint32_t warn = (uint32_t)notif[5] | ((uint32_t)notif[4] << 8);
        Serial.print("Received a warning with code: "); Serial.println(warn);
        break;
      }
    case (uint8_t)MesID::PRODUCTCODE: {
        Serial.print("Product code is: ");
        for (int i = 2; i < notificationSize - 1; i++) {
          Serial.print(char(notif[i]));
        }
        Serial.println();
        productCode = char(notif[6]);                                               //Store the product code (MTi-#) for later usage
        break;
      }
    case (uint8_t)MesID::FIRMWAREREV: {
        Serial.print("Firmware version is: ");
        Serial.print(notif[2]); Serial.print("."); Serial.print(notif[3]); Serial.print("."); Serial.println(notif[4]);
        break;
      }
    case (uint8_t)MesID::GOTOCONFIGACK: {
        //Serial.println("Received GoToConfigACK.");
        configState = true;
        break;
      }
    case (uint8_t)MesID::GOTOMEASUREMENTACK: {
        //Serial.println("Received GoToMeasurementACK.");
        configState = false;
        break;
      }
    case (uint8_t)MesID::OUTPUTCONFIGURATION: {
        //Serial.println("Received SetOutputConfigurationACK.");
        break;
      }
    default: {
        Serial.print("Received undefined notification: ");
        for (int i = 0; i < notificationSize - 1; i++) {
          Serial.print(notif[i], HEX); Serial.print(" ");
        }
        Serial.println();
        break;
      }
  }
}



void Xbus::dataswapendian(uint8_t* data, uint8_t length) {                          //Swap the endianness of the data such that the float value can be printed
  uint8_t cpy[length];                                                              //Create a copy of the data
  memcpy(cpy, data, length);                                                        //Create a copy of the data
  for (int i = 0; i < length / 4; i++) {
    for (int j = 0; j < 4; j++) {
      data[j + i * 4] = cpy[3 - j + i * 4];                                         //Within each 4-byte (32-bit) float, reverse the order of the individual bytes
    }
  }
}
