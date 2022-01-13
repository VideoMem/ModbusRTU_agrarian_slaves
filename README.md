
After factory reset the slave id is 1

To change the slave id write it to the first holding register (in this case moves it to id 10)
```shell
./modpoll -b 9600 -m rtu -a 1 -t 4 -c 1 -p none -4 1 /dev/ttyUSB1 10
```

To factory reset the device ID, short pin 2 to GND and reset the device. 

For three seconds the status LED will keep steady.

The device restores factory default parameters after that (it starts blinking normally).

Then, the short to pin 2 can be removed.

# Set date/time using modpoll tool
```shell
./modpoll -b 9600 -m rtu -a 1 -t 4 -c 7 -r 50 -p none -4 1 /dev/ttyUSB1 $(date +'%Y') $(date +'%m') $(date +'%d') $(date +'%H') $(date +'%M') $(date +'%S') $(date +'%w')
```
Check RTC date
```shell
/modpoll -b 9600 -m rtu -a 1 -t 4 -c 7 -r 50 -p none -4 1 /dev/ttyUSB1
```
Set by Unix Epoch
```shell
./modpoll -b 9600 -m rtu -a 1 -t 4:int -c 1 -r 57 -p none -4 1 /dev/ttyUSB1 $(date +"%s")
```
Then, optionally set day of week
```shell
./modpoll -b 9600 -m rtu -a 1 -t 4 -c 1 -r 56 -p none -4 1 /dev/ttyUSB1 $(date +"%w")
```
