# ModbusRTU_agrarian_slaves
Firmware for small modbus RTU slave devices intended for agricultural automation and remote monitoring.

## Features:
It currently supports XY-WTH1 thermohygrometer as temperature/humidity acquisition device for data logging.
It fetches its data and stores with UNIX Epoch timestamp on the device's RAM.
Later, that data can be retrieved using the read holding registers call (And then, flushed from RAM).

The capture mode can be started and stopped writing on reserved 'digital output' addresses.

It can drive the internal output relays of the thermohygrometer like local output device relays.

The RTC can be set remotely via modbus, and it keeps the date/time with a holding battery in case of power failure.


## Supports:

Atmel Atmega328p, (aka arduino nano/mini) as slave devices using RS485 serial adapters.

9600 bauds, 8, N, 1 RTU mode.

##Requires:

http://jeelabs.net/projects/cafe/wiki/RTClib

https://github.com/yaacov/ArduinoModbusSlave

(modpoll tool, optional) https://www.modbusdriver.com/modpoll.html

## Notes:

After factory reset the slave id is 1

To change the slave id write it to the first holding register (in this case moves it to id 10)
```shell
./modpoll -b 9600 -m rtu -a 1 -t 4 -c 1 -p none -4 1 /dev/ttyUSB0 10
```

To factory reset the device ID, short pin 2 to GND and reset the device. 

For three seconds the status LED will keep steady.

The device restores factory default parameters after that (it starts blinking normally).

Then, the short to pin 2 can be removed.

# Set date/time using modpoll tool
```shell
modpoll -b 9600 -m rtu -a 1 -t 4 -c 7 -r 50 -p none -4 1 /dev/ttyUSB0 $(date +'%Y') $(date +'%m') $(date +'%d') $(date +'%H') $(date +'%M') $(date +'%S') $(date +'%w')
```
Check RTC date
```shell
modpoll -b 9600 -m rtu -a 1 -t 4 -c 7 -r 50 -p none -4 1 /dev/ttyUSB0
```
Set by Unix Epoch
```shell
modpoll -b 9600 -m rtu -a 1 -t 4:int -c 1 -r 57 -p none -4 1 /dev/ttyUSB0 $(date +"%s")
```
Then, optionally set day of week
```shell
modpoll -b 9600 -m rtu -a 1 -t 4 -c 1 -r 56 -p none -4 1 /dev/ttyUSB0 $(date +"%w")
```

Or use the setDateTime.sh script included.
```shell
./setDateTime.sh 10 /dev/ttyUSB0
```
This example sets the Date/Time and day of week on slave ID 10 on /dev/ttyUSB0 network.

For displaying the current set time epoch
```shell
modpoll -b 9600 -m rtu -a 1 -t 4:int -c 1 -r 57 -p none -4 1 -1 /dev/ttyUSB0 
```
