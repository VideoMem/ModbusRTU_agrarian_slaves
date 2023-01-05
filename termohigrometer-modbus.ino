/*
    Modbus slave example.

    Control and Read Arduino I/Os using Modbus serial connection.

    This sketch show how to use the callback vector for reading and
    controlling Arduino I/Os.

    * Control digital output pins as modbus coils.
    * Read digital inputs as discreet inputs.
    * Read analog inputs as input registers.
    * Write and Read EEPROM as holding registers.

    Created 08-12-2015
    By Yaacov Zamir

    Updated 31-03-2020
    By Yorick Smilda

    Extended 01-01-2023
    By Sebastian Wilwerth

*/


#include <EEPROM.h>
#include <ModbusSlave.h>
#include "Duty.h"
// all times are in ms
#define BLINK_IRRIGATION_MAIN 300000UL
#define BLINK_NO_ERROR 500UL
#define BLINK_ON_ERROR 50UL
#define BLINK_FACTORY_RESET 3000UL

// The Modbus slave ID, change to the ID you want to use.
#define SLAVE_ID 1
// Change to the pin the RE/DE pin of the RS485 controller is connected to.
#define RS485_CTRL_PIN 3
// Change to the baudrate you want to use for Modbus communication.
#define SERIAL_BAUDRATE 9600
// Serial port to use for RS485 communication, change to the port you're using.
#define SERIAL_PORT Serial

// factory reset pin/jumper
#define FACTORY_RESET 2

#define DATA_START_ADDR 100
#define DIGITAL_PINS_SIZE 10

// The position in the array determines the address. Position 0 will correspond to Coil, Discrete input or Input register 0.
uint8_t digital_pins[] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13};    // Add the pins you want to read as a Discrete input.
uint8_t analog_pins[] = {A0, A1, A2, A3, A4, A5};        // Add the pins you want to read as a Input register.

// The EEPROM layout is as follows
// First byte is slave_id
// Byte 1 to 10 are pulse drive modes (0 default, 1 pulse drive)
//      6 and 10 are in pulse drive mode by default, so pin 9 and 13 (led) will blink at configured intervals
// From data addr 101 and over, there are the time on / time off constants for each drive
// Each register is a 32 bit unsigned long and stores the preset time in milliseconds
// First 10 registers are for time on values for each drive
// The same applies for the following 10 that are for the time off values.

// You shouldn't have to change anything below this to get this example to work

uint8_t digital_pins_size = sizeof(digital_pins) / sizeof(digital_pins[0]); // Get the size of the digital_pins array
uint8_t analog_pins_size = sizeof(analog_pins) / sizeof(analog_pins[0]);    // Get the size of the analog_pins array

// Modbus object declaration
Modbus slave(SERIAL_PORT, SLAVE_ID, RS485_CTRL_PIN);

Duty pulse_drives[DIGITAL_PINS_SIZE];
uint8_t outModes[DIGITAL_PINS_SIZE];
unsigned char ledPin = 13;
unsigned char pulsePin = 9;
uint8_t loopcnt = 0;

void EEPROM_writeint(uint16_t address, uint16_t value)  {
    EEPROM.write(address + 1, highByte(value));
    EEPROM.write(address, lowByte(value));
    uint16_t check = EEPROM_readint(address);
    if (check != value)
        assertErrorLoop();
}

uint16_t EEPROM_readint(uint16_t address) {
    unsigned int word = word(EEPROM.read(address + 1), EEPROM.read(address));
    return word;
}

unsigned long EEPROM_readlong(uint16_t address) {
    //use word read function for reading upper part
    unsigned long dword = EEPROM_readint(address + sizeof(uint16_t));
    dword <<= 16;
    // read lower word from EEPROM and OR it into double word
    dword = dword | EEPROM_readint(address);
    return dword;
}

void EEPROM_writelong(uint16_t address, unsigned long value) {
    unsigned long original_value = value;
    uint16_t word_h = value >> 16;
    uint16_t word_l = value & 0xFFFF;
    //truncate upper part and write lower part into EEPROM
    EEPROM_writeint(address + sizeof(uint16_t), word_h);
    EEPROM_writeint(address, word_l);
    unsigned long check = EEPROM_readlong(address);
    if (check != original_value)
        assertErrorLoop();
}

void ledDriveDefaultValues() {
    EEPROM_writelong(DATA_START_ADDR + (9 * sizeof(unsigned long)), (unsigned long) BLINK_NO_ERROR);
    EEPROM_writelong(DATA_START_ADDR + ((digital_pins_size + 9) * sizeof(unsigned long)), (unsigned long) BLINK_NO_ERROR);
    EEPROM.put(10, 1);
}


void defaultPulseDriveValues() {
    unsigned long def_on_off = BLINK_IRRIGATION_MAIN;

    for(uint8_t i = 0; i < (2 * digital_pins_size); i++) {
        int addr = DATA_START_ADDR + (i * sizeof(unsigned long));
        EEPROM_writelong(addr, def_on_off);
    }

    for(uint8_t addr = 1; addr <= digital_pins_size; addr++) {
        EEPROM.put(addr, 0);
    }

    EEPROM.put(6, 1);

    ledDriveDefaultValues();
}

void assertErrorLoop() {
    while (true) {
        digitalWrite(ledPin, 1);
        delay(BLINK_ON_ERROR);
        digitalWrite(ledPin, 0);
        delay(BLINK_ON_ERROR);
    }
}

void readPulseDriveValues() {
    for(uint8_t i = 0; i < digital_pins_size; i++) {
        uint8_t offset = i * sizeof(unsigned long);
        int addr_on = DATA_START_ADDR + offset;
        int addr_off = DATA_START_ADDR + (digital_pins_size * sizeof(unsigned long)) + offset;
        unsigned long time_on = EEPROM_readlong(addr_on);
        unsigned long time_off = EEPROM_readlong(addr_off);
        pulse_drives[i].setMS_on(time_on);
        pulse_drives[i].setMS_off(time_off);
    }

    for(uint8_t i = 1; i <= digital_pins_size; i++ ) {
        uint8_t value = 0;
        EEPROM.get(i, value);
        outModes[i - 1] = value;
    }
}

// default slave ID
void factoryDefaults() {
    EEPROM.put(0, SLAVE_ID);
    slave.setUnitAddress(SLAVE_ID);
    defaultPulseDriveValues();
}

void setupResetDiag() {

    for (uint8_t i=0; i <= digital_pins_size; i++) {
        digitalWrite(digital_pins[i], 1);
        pinMode(digital_pins[i], OUTPUT);
    }
    pinMode(ledPin, OUTPUT);
    pinMode(pulsePin, OUTPUT);
    pinMode(RS485_CTRL_PIN, OUTPUT);

    uint8_t id;
    EEPROM.get(0, id);
    slave.setUnitAddress(id);
    readPulseDriveValues();
}

void check_factory_reset_pin() {
    pinMode(FACTORY_RESET, INPUT_PULLUP);
    pinMode(FACTORY_RESET, INPUT);

    loopcnt = 20;
    while (loopcnt != 0 && digitalRead(FACTORY_RESET) == 0) {
        digitalWrite(ledPin, 1);
        delay(BLINK_ON_ERROR);
        digitalWrite(ledPin, 0);
        delay(BLINK_ON_ERROR);
        loopcnt--;
    }
    if (digitalRead(FACTORY_RESET) == 0)
        factoryDefaults();
}


void setup() {

    // Set the defined analog pins to input mode.
    for (int i = 0; i < analog_pins_size; i++) {
        pinMode(analog_pins[i], INPUT);
    }

    check_factory_reset_pin();
    setupResetDiag();

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

void pulseUpdate() {
    for(uint8_t i = 0; i < digital_pins_size; i++) {
        pulse_drives[i].update();
        if(outModes[i] == 1)
            digitalWrite(digital_pins[i], pulse_drives[i].value());
    }
}

void refresh() {
    readPulseDriveValues();
}

void loop() {
    // Listen for modbus requests on the serial port.
    // When a request is received it's going to get validated.
    // And if there is a function registered to the received function code, this function will be executed.
    slave.poll();
    //ledDrive();
    //pulseDrive();
    if (loopcnt % 100 == 0)
        refresh();
    pulseUpdate();
    loopcnt++;
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
    if (address > digital_pins_size || (address + length) > digital_pins_size) {
        return STATUS_ILLEGAL_DATA_ADDRESS;
    }

    // Read the digital inputs.
    for (int i = 0; i < length; i++) {
        // Write the state of the digital pin to the response buffer.
        slave.writeCoilToBuffer(i, digitalRead(digital_pins[address + i]));
    }

    return STATUS_OK;
}

// Handle the function code Read Input Registers (FC=04) and write back the values from the analog input pins (input registers).
uint8_t readAnalogIn(uint8_t fc, uint16_t address, uint16_t length) {
    // Check if the requested addresses exist in the array
    if (address > analog_pins_size || (address + length) > analog_pins_size) {
        return STATUS_ILLEGAL_DATA_ADDRESS;
    }

    // Read the analog inputs
    for (int i = 0; i < length; i++) {
        // Write the state of the analog pin to the response buffer.
        slave.writeRegisterToBuffer(i, analogRead(analog_pins[address + i]));
    }

    return STATUS_OK;
}

// Handle the function codes Force Single Coil (FC=05) and Force Multiple Coils (FC=15) and set the digital output pins (coils).
uint8_t writeDigitalOut(uint8_t fc, uint16_t address, uint16_t length) {

    // Check if the requested addresses exist in the array
    if (address > digital_pins_size || (address + length) > digital_pins_size) {
        return STATUS_ILLEGAL_DATA_ADDRESS;
    }

    // Set the output pins to the given state.
    for (int i = 0; i < length; i++) {
        // Write the value in the input buffer to the digital pin.
        if (outModes[address + i] == 0) {
            digitalWrite(digital_pins[address + i], slave.readCoilFromBuffer(i));
        } else {
            if (slave.readCoilFromBuffer(i) == 0)
                pulse_drives[address + i].sync_off();
            else
                pulse_drives[address + i].sync_on();
        }

    }

    return STATUS_OK;
}

// Handle the function code Read Holding Registers (FC=03) and write back the values from the EEPROM (holding registers).
uint8_t readMemory(uint8_t fc, uint16_t address, uint16_t length) {

    // Read the requested EEPROM registers.
    for (int i = 0; i < length; i++) {
        if ((address + i) < DATA_START_ADDR) {
            uint8_t value;
            EEPROM.get(address + i, value);
            slave.writeRegisterToBuffer(i, value);
        } else {
            uint16_t value16;
            EEPROM.get(address + (i * sizeof(uint16_t)), value16);
            slave.writeRegisterToBuffer(i, value16);
        }
    }

    return STATUS_OK;
}

// Handle the function codes Write Holding Register(s) (FC=06, FC=16) and write data to the eeprom.
uint8_t writeMemory(uint8_t fc, uint16_t address, uint16_t length) {


    // Write the received data to EEPROM.
    for (int i = 0; i < length; i++) {

        if ((address + i) < DATA_START_ADDR) {
            uint8_t value = slave.readRegisterFromBuffer(i);
            if (address + i  == 0) {
                if ((value < 1 || value > 247)) {
                    return STATUS_ILLEGAL_DATA_VALUE;
                }
               slave.setUnitAddress(value);
            }
           EEPROM.put(address + i, value);
        } else {
           uint16_t value16 = slave.readRegisterFromBuffer(i);
           EEPROM.put(address + (i * sizeof(uint16_t)), value16);
        }
    }

    return STATUS_OK;
}
