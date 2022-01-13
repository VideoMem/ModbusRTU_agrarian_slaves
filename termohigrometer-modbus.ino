

/*
    Modbus slave example.

    Control and Read Arduino I/Os using Modbus serial connection.
    
    This sketch show how to use the callback vector for reading and
    controlling Arduino I/Os.
    
    * Control digital pins mode using holding registers 0 .. 50.
    * Control digital output pins as modbus coils.
    * Read digital inputs as discreet inputs.
    * Read analog inputs as input registers.
    * Write and Read EEPROM as holding registers.

    Created 08-12-2015
    By Yaacov Zamir

    Updated 31-03-2020
    By Yorick Smilda

    https://github.com/yaacov/ArduinoModbusSlave

*/



#include <EEPROM.h>
#include <ModbusSlave.h>
#include <Sodaq_DS3231.h>
#include <Wire.h>
#include "Toggle.h"
#include "Timers.h"
#define BLINK_NO_ERROR 500
#define BLINK_ON_ERROR 50
#define BLINK_FACTORY_RESET 3000

#define SLAVE_ID 1           // The Modbus slave ID, change to the ID you want to use.
#define RS485_CTRL_PIN 8     // Change to the pin the RE/DE pin of the RS485 controller is connected to.
#define SERIAL_BAUDRATE 9600 // Change to the baudrate you want to use for Modbus communication.
#define SERIAL_PORT Serial   // Serial port to use for RS485 communication, change to the port you're using.
#define FACTORY_RESET 2
#define RELAY0 3
#define RELAY1 4
#define RTC_MEM_START 49

// The position in the array determines the address. Position 0 will correspond to Coil, Discrete input or Input register 0.
uint8_t digital_pins[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12};    // Add the pins you want to read as a Discrete input.
uint8_t analog_pins[] = {A0, A1, A2, A3};                      // Add the pins you want to read as a Input register.

// The EEPROM layout is as follows
// The first 50 bytes are reserved for storing digital pin pinMode_setting
// Byte 51 and up are free to write any uint16_t to.

// You shouldn't have to change anything below this to get this example to work

uint8_t digital_pins_size = sizeof(digital_pins) / sizeof(digital_pins[0]); // Get the size of the digital_pins array
uint8_t analog_pins_size = sizeof(analog_pins) / sizeof(analog_pins[0]);    // Get the size of the analog_pins array

// Modbus object declaration
Modbus slave(SERIAL_PORT, SLAVE_ID, RS485_CTRL_PIN);

Timer blinkTimer;
Toggle blinker;
unsigned char ledPin = 13;

void setupResetDiag() {
    pinMode(FACTORY_RESET, INPUT_PULLUP);
    pinMode(FACTORY_RESET, INPUT);   
    digitalWrite(RELAY0, 1);
    digitalWrite(RELAY1, 1);    
    pinMode(RELAY0, OUTPUT);  
    pinMode(RELAY1, OUTPUT);  
    pinMode(ledPin, OUTPUT);   
    blinkTimer.setMS(BLINK_NO_ERROR);
    uint8_t id;
    EEPROM.get(0, id);
    slave.setUnitAddress(id);
}

// default slave ID 
void factoryDefaults() {
    EEPROM.put(0, SLAVE_ID);
    slave.setUnitAddress(SLAVE_ID);
    DateTime dt(2022, 1, 13, 0, 0, 0, 4);
    rtc.setDateTime(dt);  //Establece fecha y hora cargada en "dt"
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

void loop()
{
    // Listen for modbus requests on the serial port.
    // When a request is received it's going to get validated.
    // And if there is a function registered to the received function code, this function will be executed.
    slave.poll();
    ledDrive();
}

// Modbus handler functions
// The handler functions must return an uint8_t and take the following parameters:
//     uint8_t  fc - function code
//     uint16_t address - first register/coil address
//     uint16_t length/status - length of data / coil status

// Handle the function codes Read Input Status (FC=01/02) and write back the values from the digital pins (input status).
uint8_t readDigital(uint8_t fc, uint16_t address, uint16_t length)
{
    // Check if the requested addresses exist in the array
    if (address > digital_pins_size || (address + length) > digital_pins_size)
    {
        return STATUS_ILLEGAL_DATA_ADDRESS;
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
    // Check if the requested addresses exist in the array
    if (address > digital_pins_size || (address + length) > digital_pins_size)
    {
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

uint16_t readRTC(DateTime& now, uint8_t reg) {
    switch(reg) {
        case 0:
            return now.year();
        break;
        case 1:
            return now.month();
        break;
        case 2:
            return now.date();
        break;
        case 3:
            return now.hour();
        break;
        case 4:
            return now.minute();
        break;
        case 5:
            return now.second();
        break;
        case 6:
            return now.dayOfWeek();
        break;
    }
}

uint8_t writeRTCYY(uint16_t year) {
    DateTime now = rtc.now();
    DateTime dt(year, now.month(), now.date(), now.hour(), now.minute(), now.second(), now.dayOfWeek());
    rtc.setDateTime(dt);
    return STATUS_OK;
}

uint8_t writeRTCMM(uint16_t month) {
    DateTime now = rtc.now();
    DateTime dt(now.year(), month, now.date(), now.hour(), now.minute(), now.second(), now.dayOfWeek());
    rtc.setDateTime(dt);
    return STATUS_OK;
}

uint8_t writeRTCDD(uint16_t day) {
    DateTime now = rtc.now();
    DateTime dt(now.year(), now.month(), day, now.hour(), now.minute(), now.second(), now.dayOfWeek());
    rtc.setDateTime(dt);
    return STATUS_OK;
}

uint8_t writeRTCHH(uint16_t hour) {
    DateTime now = rtc.now();
    DateTime dt(now.year(), now.month(), now.date(), hour, now.minute(), now.second(), now.dayOfWeek());
    rtc.setDateTime(dt);
    return STATUS_OK;
}

uint8_t writeRTCmm(uint16_t minute) {
    DateTime now = rtc.now();
    DateTime dt(now.year(), now.month(), now.date(), now.hour(), minute, now.second(), now.dayOfWeek());
    rtc.setDateTime(dt);
    return STATUS_OK;
}

uint8_t writeRTCSS(uint16_t second) {
    DateTime now = rtc.now();
    DateTime dt(now.year(), now.month(), now.date(), now.hour(), now.minute(), second, now.dayOfWeek());
    rtc.setDateTime(dt);
    return STATUS_OK;
}

uint8_t writeRTCDW(uint16_t dayOfWeek) {
    DateTime now = rtc.now();
    DateTime dt(now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second(), dayOfWeek);
    rtc.setDateTime(dt);
    return STATUS_OK;
}

uint8_t writeRTC(uint8_t reg, uint16_t value) {
    DateTime now = rtc.now();
    switch(reg) {
        case 0:
            writeRTCYY(value);
            break;
        case 1:
            writeRTCMM(value);
            break;
        case 2:
            writeRTCDD(value);
            break;
        case 3:
            writeRTCHH(value);
            break;
        case 4:
            writeRTCmm(value);
            break;
        case 5:
            writeRTCSS(value);
            break;
        case 6:
            writeRTCDW(value);
            break;
        default:
            blinkTimer.setMS(BLINK_ON_ERROR);
            break;
    }
    return STATUS_OK;
}

// Handle the function code Read Holding Registers (FC=03) and write back the values from the EEPROM (holding registers).
uint8_t readMemory(uint8_t fc, uint16_t address, uint16_t length)
{
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
        else if (address + i >= RTC_MEM_START && address + i < RTC_MEM_START + 7) {
            //handles RTC
            uint8_t offset = address + i - RTC_MEM_START;
            uint16_t value = readRTC(now, offset);
            slave.writeRegisterToBuffer(i, value);
        } else {
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
        else if (address + i >= RTC_MEM_START && address + i < RTC_MEM_START + 7) {
            //handles RTC
            uint8_t offset = address + i - RTC_MEM_START;
            uint16_t value = slave.readRegisterFromBuffer(i);
            if (writeRTC(offset, value) != STATUS_OK)
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
    }
    
    blinkTimer.update();
}
