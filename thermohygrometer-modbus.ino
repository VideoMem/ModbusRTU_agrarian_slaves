//#include <avr/wdt.h>
#include <EEPROM.h>
#include <ModbusSlave.h>
#include <Sodaq_DS3231.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include "Toggle.h"
#include "Timers.h"
#define BLINK_NO_ERROR 500
#define BLINK_ON_ERROR 50
#define BLINK_ON_POLL 75
#define BLINK_POLL_MS 1400
#define BLINK_FACTORY_RESET 3000

static Timer blinkTimer;
static Timer commandTimer;
static Toggle blinker;

unsigned char ledPin = 13;
static unsigned char onpoll = 0;
uint16_t pcounter = 0;

void pollDrive() {
    if(commandTimer.event()) {
        onpoll = 0;
    }
    commandTimer.update();
}

void poolNotify() {
    commandTimer.reset();
    onpoll = 1;
}

#define SLAVE_ID 1           // The Modbus slave ID, change to the ID you want to use.
#define RS485_CTRL_PIN 8     // Change to the pin the RE/DE pin of the RS485 controller is connected to.
#define SERIAL_BAUDRATE 9600 // Change to the baudrate you want to use for Modbus communication.
#define SERIAL_PORT Serial   // Serial port to use for RS485 communication, change to the port you're using.
#define FACTORY_RESET 2
#define RELAY0 3
#define RELAY1 4

// platform specific modules
#include "RTC.h"
#include "XY-WTH1.h"

static XYWTH thermohygrometer;

// The position in the array determines the address. Position 0 will correspond to Coil, Discrete input or Input register 0.
uint8_t digital_pins[] = {3, 4, 5, 6, 7, 8, 9, 12};    // Add the pins you want to read as a Discrete input.
uint8_t analog_pins[] = {A0, A1, A2, A3};                      // Add the pins you want to read as a Input register.

// The EEPROM layout is as follows
// The first 50 bytes are reserved for storing digital pin pinMode_setting
// Byte 51 and up are free to write any uint16_t to.

// You shouldn't have to change anything below this to get this example to work

uint8_t digital_pins_size = sizeof(digital_pins) / sizeof(digital_pins[0]); // Get the size of the digital_pins array
uint8_t analog_pins_size = sizeof(analog_pins) / sizeof(analog_pins[0]);    // Get the size of the analog_pins array

// Modbus object declaration
Modbus slave(SERIAL_PORT, SLAVE_ID, RS485_CTRL_PIN);

void setupResetDiag() {
    pinMode(FACTORY_RESET, INPUT_PULLUP);
    pinMode(FACTORY_RESET, INPUT);   
    digitalWrite(RELAY0, 1);
    digitalWrite(RELAY1, 1);    
    pinMode(RELAY0, OUTPUT);  
    pinMode(RELAY1, OUTPUT);  
    pinMode(ledPin, OUTPUT);
    pinMode(RS485_CTRL_PIN, OUTPUT);
    pinMode(SSRX, INPUT);
    pinMode(SSTX, OUTPUT);
    blinkTimer.setMS(BLINK_NO_ERROR);
    commandTimer.setMS(BLINK_POLL_MS);
    uint8_t id;
    EEPROM.get(0, id);
    slave.setUnitAddress(id);
    thermohygrometer.setup();
    //wdt_enable(WDTO_8S);
    pcounter=0;
}

// default slave ID 
void factoryDefaults() {
    EEPROM.put(0, SLAVE_ID);
    slave.setUnitAddress(SLAVE_ID);
}

void rtcInit() {
  Wire.begin();         //Inicializa la comunicacion con el RTC
  rtc.begin();          //Inicializa el RTC
}

void setup()
{
    // Set the defined digital pins to the value stored in EEPROM.
    // EEPROM slot 0 reserved for slave ID
    for (uint16_t i = 0; i < digital_pins_size; i++)
    {
        uint8_t pinMode_setting;
        // Get the pinMode_setting of this digital pin from the EEPROM.
        EEPROM.get(i+1, pinMode_setting);

        pinMode(digital_pins[i], pinMode_setting);
    }

    // Set the defined analog pins to input mode.
    for (int i = 0; i < analog_pins_size; i++)
    {
        pinMode(analog_pins[i], INPUT);
    }

    setupResetDiag();
    rtcInit();

    if (digitalRead(FACTORY_RESET) == 0) {
        blinkTimer.setMS(BLINK_FACTORY_RESET);
        digitalWrite(ledPin, 1);
        do {
            blinkTimer.update();
        } while (!blinkTimer.event());
        if (digitalRead(FACTORY_RESET) == 0) 
            factoryDefaults();
        blinkTimer.setMS(BLINK_NO_ERROR);
        blinkTimer.reset();  
    }
    
    
    // Register functions to call when a certain function code is received.
    slave.cbVector[CB_READ_COILS] = readDigital;
    slave.cbVector[CB_READ_DISCRETE_INPUTS] = readDigital;
    slave.cbVector[CB_WRITE_COILS] = writeDigitalOut;
    slave.cbVector[CB_READ_INPUT_REGISTERS] = readAnalogIn;
    slave.cbVector[CB_READ_HOLDING_REGISTERS] = readMemory;
    slave.cbVector[CB_WRITE_HOLDING_REGISTERS] = writeMemory;

    // Set the serial port and slave to the given baudrate.
    SERIAL_PORT.begin(SERIAL_BAUDRATE);
    slave.begin(SERIAL_BAUDRATE);
}

void mainRuntimeLevel() {
    // Listen for modbus requests on the serial port.
    // When a request is received it's going to get validated.
    // And if there is a function registered to the received function code, this function will be executed.
    slave.poll();
}

void notificationRuntimeLevel() {
    if (thermohygrometer.isCapturing())
        thermohygrometer.setCaptureEpoch(rtc.now().getEpoch());
    ledDrive();
    pollDrive();
}

void userspaceRuntimeLevel() {
    thermohygrometer.update();
}

void loop() {
    if (pcounter % 2 == 0) {
        mainRuntimeLevel();
    } else if (pcounter % 3 == 0) {
        userspaceRuntimeLevel();
    } else if (pcounter % 5 == 0) {
        notificationRuntimeLevel();
    }
    
    pcounter++;
    //wdt_reset();
}


// Modbus handler functions
// The handler functions must return an uint8_t and take the following parameters:
//     uint8_t  fc - function code
//     uint16_t address - first register/coil address
//     uint16_t length/status - length of data / coil status

// Handle the function codes Read Input Status (FC=01/02) and write back the values from the digital pins (input status).
uint8_t readDigital(uint8_t fc, uint16_t address, uint16_t length)
{
    poolNotify();
    // Check if the requested addresses exist in the array
    if (address > digital_pins_size || (address + length) > digital_pins_size)
    {
        if (address >= 20 && address + length <= 23) {
            for (int i = 0; i < length; i++) {
                switch(address + i) {
                    case 20:
                        slave.writeCoilToBuffer(i, thermohygrometer.isTempRelayOn());
                        break;
                    case 21:
                        slave.writeCoilToBuffer(i, thermohygrometer.isHumidRelayOn());
                        break;
                    case 22:
                        slave.writeCoilToBuffer(i, thermohygrometer.isCapturing());
                        break;
                }
            }
            return STATUS_OK;
        } else {
            return STATUS_ILLEGAL_DATA_ADDRESS;
        }
    }

    // Read the digital inputs.
    for (int i = 0; i < length; i++)
    {
        // Write the state of the digital pin to the response buffer.
        slave.writeCoilToBuffer(i, digitalRead(digital_pins[address + i]));
    }

    return STATUS_OK;
}

// Handle the function code Read Input Registers (FC=04) and write back the values from the analog input pins (input registers).
uint8_t readAnalogIn(uint8_t fc, uint16_t address, uint16_t length)
{
    poolNotify();
    // Check if the requested addresses exist in the array
    if (address > analog_pins_size || (address + length) > analog_pins_size)
    {
        return STATUS_ILLEGAL_DATA_ADDRESS;
    }

    // Read the analog inputs
    for (int i = 0; i < length; i++)
    {
        // Write the state of the analog pin to the response buffer.
        slave.writeRegisterToBuffer(i, analogRead(analog_pins[address + i]));
    }

    return STATUS_OK;
}

// Handle the function codes Force Single Coil (FC=05) and Force Multiple Coils (FC=15) and set the digital output pins (coils).
uint8_t writeDigitalOut(uint8_t fc, uint16_t address, uint16_t length)
{
    poolNotify();

    if (address >= 20 && address + length <= 23) {
        for (int i = 0; i < length; i++) {
            if (slave.readCoilFromBuffer(i) == 1) {
                switch (address + i) {
                    case 20:
                        thermohygrometer.enableTempRelay();
                        break;
                    case 21:
                        thermohygrometer.enableHumidityRelay();
                        break;
                    case 22:
                        thermohygrometer.startCapture();
                        break;
                }
            } else {
                switch (address + i) {
                    case 20:
                        thermohygrometer.disableTempRelay();
                        break;
                    case 21:
                        thermohygrometer.disableHumidityRelay();
                        break;
                    case 22:
                        thermohygrometer.stopCapture();
                        break;
                }
            }
        }
        return STATUS_OK;
    // Check if the requested addresses exist in the array
    } else if (address > digital_pins_size || (address + length) > digital_pins_size) {
        return STATUS_ILLEGAL_DATA_ADDRESS;
    }

    // Set the output pins to the given state.
    for (int i = 0; i < length; i++)
    {
        // Write the value in the input buffer to the digital pin.
        digitalWrite(digital_pins[address + i], slave.readCoilFromBuffer(i));
    }

    return STATUS_OK;
}

// Handle the function code Read Holding Registers (FC=03) and write back the values from the EEPROM (holding registers).
uint8_t readMemory(uint8_t fc, uint16_t address, uint16_t length)
{
    poolNotify();
    // Read the requested EEPROM registers.
    DateTime now = rtc.now();
    for (int i = 0; i < length; i++)
    {
        // Below RTC_MEM_START is reserved for pinModes
        if (address + i < RTC_MEM_START)
        {
            uint8_t value;

            // Read a value from the EEPROM.
            EEPROM.get((address + i), value);

            // Write the pinMode from EEPROM to the response buffer.
            slave.writeRegisterToBuffer(i, value);
        }
        else if (address + i >= RTC_MEM_START && address + i < RTC_MEM_START + 9) {
            //handles RTC
            uint8_t offset = address + i - RTC_MEM_START;
            uint16_t value = readRTC(now, offset);
            slave.writeRegisterToBuffer(i, value);
        } else if (address + i >= TH_MEM_START && address + i < TH_MEM_START + CAPTURE_HOLD_SIZE) {
            //handles temperature
            uint16_t offset = address + i - TH_MEM_START;
            CaptureRegister reg = thermohygrometer.getMark(offset);
            slave.writeRegisterToBuffer(i, reg.temperature);
        } else if (address + i >= TH_MEM_START + CAPTURE_HOLD_SIZE && address + i < TH_MEM_START + CAPTURE_HOLD_SIZE * 2) {
            //handles humidity
            uint16_t offset = address + i - (TH_MEM_START + CAPTURE_HOLD_SIZE);
            CaptureRegister reg = thermohygrometer.getMark(offset);
            slave.writeRegisterToBuffer(i, reg.humidity);
        } else if (address + i >= TH_MEM_START + CAPTURE_HOLD_SIZE * 2 && address + i < TH_MEM_START + CAPTURE_HOLD_SIZE * 4) {
            //handles timestamps
            uint16_t wordpos = address + i - (TH_MEM_START + CAPTURE_HOLD_SIZE * 2);
            uint16_t capturepos =  wordpos / 2;
            CaptureRegister reg = thermohygrometer.getMark(capturepos);
            if (wordpos % 2 == 0) {
                slave.writeRegisterToBuffer(i, reg.epoch.nibbles[0]);
            } else {
                slave.writeRegisterToBuffer(i, reg.epoch.nibbles[1]);
            }
        }
        else {
            uint16_t value;

            // Read a value from the EEPROM.
            EEPROM.get(address + (i * 2), value);

            // Write the value from EEPROM to the response buffer.
            slave.writeRegisterToBuffer(i, value);
        }
    }

    return STATUS_OK;
}

// Handle the function codes Write Holding Register(s) (FC=06, FC=16) and write data to the eeprom.
uint8_t writeMemory(uint8_t fc, uint16_t address, uint16_t length)
{
    poolNotify();
    union Epoch epoch;
    DateTime now = rtc.now();

    // Write the received data to EEPROM.
    for (int i = 0; i < length; i++)
    {
        if (address + i < RTC_MEM_START)
        {
            // Read the value from the input buffer.
            uint8_t value = slave.readRegisterFromBuffer(i);

            // Check if the value is 0 (INPUT) or 1 (OUTPUT).
            if (value != INPUT && value != OUTPUT && address > 0)
            {
                return STATUS_ILLEGAL_DATA_VALUE;
            }

            if ((value < 1 || value > 247) && address == 0)
            {
                return STATUS_ILLEGAL_DATA_VALUE;
            }

            if (address == 0)
                slave.setUnitAddress(value);

            // Store the received value in the EEPROM.
            EEPROM.put(address + i, value);

            // Set the pinmode to the received value.
            pinMode(digital_pins[address + i], value);

        }
        else if (address + i >= RTC_MEM_START && address + i < RTC_MEM_START + 9) {
            //handles RTC
            uint8_t offset = address + i - RTC_MEM_START;
            uint16_t value = slave.readRegisterFromBuffer(i);
            if (writeRTC(now, offset, value, epoch) != STATUS_OK)
                return STATUS_ILLEGAL_DATA_VALUE;
        } else {
            // Read the value from the input buffer.
            uint16_t value = slave.readRegisterFromBuffer(i);

            // Store the received value in the EEPROM.
            EEPROM.put(address + (i * 2), value);
        }
    }

    return STATUS_OK;
}


void ledDrive() {
    if(blinker.value() == true)
        digitalWrite(ledPin, HIGH);   // set the LED on
    else
        digitalWrite(ledPin, LOW);    // set the LED off

    if(blinkTimer.event()) {
        blinker.change();
        blinkTimer.reset();
        if(onpoll == 1) {
            blinkTimer.setMS(BLINK_ON_POLL);
        } else {
            blinkTimer.setMS(BLINK_NO_ERROR);
        }
    }
    
    blinkTimer.update();
}

