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
#include "Toggle.h"
#include "Timers.h"
#include "Duty.h"
// all times are in ms
#define BLINK_IRRIGATION_MAIN 315000
#define BLINK_NO_ERROR 500
#define BLINK_ON_ERROR 50
#define BLINK_FACTORY_RESET 3000

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
// Byte 1 to 36 are pulse on time values in ms (9 unsigned long registers)
// Byte 37 to 86 are pulse off time values in ms (9 unsigned long registers)
// Byte 90 to 99 are pin modes (0: output, 1: pulse_drive, 2: input)

// You shouldn't have to change anything below this to get this example to work

uint8_t digital_pins_size = sizeof(digital_pins) / sizeof(digital_pins[0]); // Get the size of the digital_pins array
uint8_t analog_pins_size = sizeof(analog_pins) / sizeof(analog_pins[0]);    // Get the size of the analog_pins array

// Modbus object declaration
Modbus slave(SERIAL_PORT, SLAVE_ID, RS485_CTRL_PIN);

Timer blinkTimer;
Toggle blinker;
Timer pulseTimer;
Toggle pulser;
Duty pulse_drives[DIGITAL_PINS_SIZE];
uint8_t outModes[DIGITAL_PINS_SIZE];
unsigned char ledPin = 13;
unsigned char pulsePin = 9;

void EEPROM_writeint(int address, int value)  {
    EEPROM.write(address, highByte(value));
    EEPROM.write(address + 1, lowByte(value));
}

unsigned int EEPROM_readint(int address) {
    unsigned int word = word(EEPROM.read(address), EEPROM.read(address + 1));
    return word;
}

unsigned long EEPROM_readlong(int address) {
    //use word read function for reading upper part
    unsigned int dword = EEPROM_readint(address);
    //shift read word up
    dword = dword << 16;
    // read lower word from EEPROM and OR it into double word
    dword = dword | EEPROM_readint(address + 2);
    return dword;
}

void EEPROM_writelong(int address, unsigned long value) {
    //truncate upper part and write lower part into EEPROM
    EEPROM_writeint(address+2, word(value));
    //shift upper part down
    value = value >> 16;
    //truncate and write
    EEPROM_writeint(address, word(value));
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

    EEPROM.put(5, 1);
}

void readPulseDriveValues() {
    unsigned long value = 0;
    size_t off_time_start_addr = (digital_pins_size * sizeof(unsigned long)) + 1;
    size_t off_time_end_addr = 2 * digital_pins_size * sizeof(unsigned long);

    for(uint8_t i = 0; i < digital_pins_size; i++) {
        int offset = i * sizeof(unsigned long);
        int addr_on = DATA_START_ADDR + offset;
        int addr_off = DATA_START_ADDR + (digital_pins_size * sizeof(unsigned long)) + offset;
        pulse_drives[i].setMS_on(EEPROM_readlong(addr_on));
        pulse_drives[i].setMS_off(EEPROM_readlong(addr_off));
    }

    for(uint8_t i = 1; i <= digital_pins_size; i++ ) {
        EEPROM.get(i, outModes[i]);
    }

}

// default slave ID
void factoryDefaults() {
    EEPROM.put(0, SLAVE_ID);
    slave.setUnitAddress(SLAVE_ID);
    defaultPulseDriveValues();
}

void setupResetDiag() {
    pinMode(FACTORY_RESET, INPUT_PULLUP);
    pinMode(FACTORY_RESET, INPUT);
    for (uint8_t i=0; i <= digital_pins_size; i++) {
        digitalWrite(digital_pins[i], 1);
        pinMode(digital_pins[i], OUTPUT);
    }
    pinMode(ledPin, OUTPUT);
    pinMode(pulsePin, OUTPUT);
    pinMode(RS485_CTRL_PIN, OUTPUT);
    blinkTimer.setMS(BLINK_NO_ERROR);
    pulseTimer.setMS(BLINK_IRRIGATION_MAIN);

    uint8_t id;
    EEPROM.get(0, id);
    slave.setUnitAddress(id);
    readPulseDriveValues();
}


void setup() {

    // Set the defined analog pins to input mode.
    for (int i = 0; i < analog_pins_size; i++) {
        pinMode(analog_pins[i], INPUT);
    }

    setupResetDiag();

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

void pulseUpdate() {
    for(uint8_t i; i < digital_pins_size; i++) {
        pulse_drives[i].update();
        if(outModes[i] == 1)
            digitalWrite(digital_pins[i], pulse_drives[i].value());
    }
}

void loop()
{
    // Listen for modbus requests on the serial port.
    // When a request is received it's going to get validated.
    // And if there is a function registered to the received function code, this function will be executed.
    slave.poll();
    ledDrive();
    //pulseDrive();
    pulseUpdate();
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

    if (address < DATA_START_ADDR && address + length > DATA_START_ADDR) {
        return STATUS_ILLEGAL_DATA_VALUE;
    }

    // Read the requested EEPROM registers.
    for (int i = 0; i < length; i++) {
        if (address + i < DATA_START_ADDR) {
            uint8_t value;
            EEPROM.get(address + i, value);
            slave.writeRegisterToBuffer(i, value);
        } else {
            uint16_t value16;
            EEPROM.get(DATA_START_ADDR + (i * 2), value16);
            slave.writeRegisterToBuffer(i, value16);
        }
    }

    return STATUS_OK;
}

// Handle the function codes Write Holding Register(s) (FC=06, FC=16) and write data to the eeprom.
uint8_t writeMemory(uint8_t fc, uint16_t address, uint16_t length) {


    // Write the received data to EEPROM.
    for (int i = 0; i < length; i++) {

        if (address + i < DATA_START_ADDR) {
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


void ledDrive() {
    if(blinker.value() == true)
        digitalWrite(ledPin, HIGH);
    else
        digitalWrite(ledPin, LOW);

    if(blinkTimer.event()) {
        blinker.change();
        blinkTimer.reset();
    }
    
    blinkTimer.update();
}

void pulseDrive() {
    if(pulser.value() == true)
        digitalWrite(pulsePin, HIGH);
    else
        digitalWrite(pulsePin, LOW);

    if(pulseTimer.event()) {
        pulser.change();
        pulseTimer.reset();
    }

    pulseTimer.update();
}