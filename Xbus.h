#ifndef Xbus_h
#define Xbus_h

#include <Arduino.h>

//Definition of opcodes: For more information on opcodes, refer to https://mtidocs.xsens.com/functional-description$mtssp-synchronous-serial-protocol
#define XSENS_CONTROL_PIPE 0x03       //Use this opcode for sending (configuration) commands to the MTi
#define XSENS_STATUS_PIPE 0x04        //Use this opcode for reading the status of the notification/measurement pipes
#define XSENS_NOTIF_PIPE 0x05         //Use this opcode for reading a notification message
#define XSENS_MEAS_PIPE 0x06          //Use this opcode for reading measurement data (MTData2 message)

class Xbus {
  public:
    Xbus();

    enum MesID {WAKEUP = 0x3E, GOTOCONFIGACK = 0x31, GOTOMEASUREMENTACK = 0x11, REQDID = 0x00, DEVICEID = 0x01, REQPRODUCTCODE = 0x1C,
                PRODUCTCODE = 0x1D, REQFWREV = 0x12, FIRMWAREREV = 0x13, ERROR = 0x42, WARNING = 0x43, OUTPUTCONFIGURATION = 0xC1, MTDATA2 = 0x36
               };

    enum DataID {EULERANGLES = 0x2030, ACCELERATION = 0x4020, RATEOFTURN = 0x8020, LATLON = 0x5040};

    bool read(uint8_t address);

    float euler[3];                                             //Used to store latest EulerAngle reading
    float acc[3];                                               //Used to store latest Acceleration reading
    float rot[3];                                               //Used to store latest RateOfTurn reading
    float latlon[2];                                            //Used to store latest Latitude/Longitude reading
    bool configState = false;                                   //True if MTi is in Config mode, false if MTi is in Measurement mode
    char productCode;                                           //Contains the product code (MTi-#) of the connected device

  private:
    void readPipeStatus(uint8_t address);
    void readPipeNotif(uint8_t address);
    void readPipeMeas(uint8_t address);
    void parseMTData2(uint8_t* data, uint8_t datalength);
    void parseNotification(uint8_t* data);

    void dataswapendian(uint8_t* data, uint8_t length);

    uint8_t status[4];                                            //Used to store indicators of the Status Pipe
    uint8_t datanotif[256];                                       //Used to store content read from the Notification Pipe
    uint8_t datameas[256];                                        //Used to store content read from the Measurement Pipe

    uint16_t notificationSize;
    uint16_t measurementSize;
};

#endif
