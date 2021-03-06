#ifndef XYWTH_H
#define XYWTH_H
#define XYWTH_VERSION "0"

#include <Arduino.h>
#define SSRX 10
#define SSTX 11
#define COMMAND_DELAY 300
#define RX_BUFFER_SIZE 20
#define RX_TIMEOUT_MS 500
#define CAPTURE_HOLD_SIZE 20
#define TH_MEM_START 69

static SoftwareSerial softSerial(SSRX, SSTX);

typedef struct {
    union Epoch epoch;
    int16_t temperature;
    int16_t humidity;
    uint8_t relayState[2];
} CaptureRegister;

class XYWTH {
    public:
        void setCaptureEpoch(uint32_t epoch) { currentEpoch = epoch; }
        boolean isCapturing() { return capturing; }
        void setStopHumidity(float temp);
        void setStopTemperature(float temp);
        void setStartHumidity(float temp);
        void setStartTemperature(float temp);
        void enableTempRelay();
        void disableTempRelay();
        void enableHumidityRelay();
        void disableHumidityRelay();
        void startCapture();
        void stopCapture();
        void resumeCapture();
        void suspendCapture();
        void setup();
        void update();
        void reset();
        CaptureRegister getMark(uint16_t pos) { return capture[pos % CAPTURE_HOLD_SIZE]; }
        boolean isTempRelayOn() { return currentCapture.relayState[0] == 1; }
        boolean isHumidRelayOn() { return currentCapture.relayState[1] == 1; }
    protected:
        void waitReply();
        char buffer[RX_BUFFER_SIZE];
        unsigned char readP;
        unsigned char capturing;
        unsigned char captureP;
        CaptureRegister capture[CAPTURE_HOLD_SIZE];
        CaptureRegister currentCapture;
        Timer rxTimedOut;
        boolean parseCapture();
        uint32_t currentEpoch;
        void clearBuffer();
        boolean isPrintable(uint8_t chr);
        boolean isControl(uint8_t chr);
        boolean isStorable(uint8_t chr);
        boolean isDigit(uint8_t chr);
        boolean copyLine(char line[], char buffer[], uint8_t& start, uint8_t end);
        boolean contains(char line[], char pattern[], uint8_t linelen, uint8_t plen);
        boolean copyDigits(char number[], char line[], uint8_t linelen);
        int digitsToInt(char digits[]);
        int getNumber(char line[], uint8_t linelen);
        void processLine(char line[], uint8_t linelen);
        boolean isDataLine(char line[], uint8_t linelen);
        void clearBuffer(char b[], uint8_t blen);
        void pushRegister(CaptureRegister& reg);
        void handleRelays(boolean isHumidity, boolean relayOFF);
        void captureTX(const char command[]);
};

void XYWTH::reset() {
    captureP = 0;
    clearBuffer();
    stopCapture();
    disableTempRelay();
    disableHumidityRelay();
    setStartTemperature(59.0);
    setStartHumidity(99.0);
    setStopTemperature(60.0);
    setStopHumidity(99.9);
    rxTimedOut.setMS(RX_TIMEOUT_MS);
    currentCapture.relayState[0] = 0;
    currentCapture.relayState[1] = 0;
}

void XYWTH::setup() {
    softSerial.begin(9600);
    reset();
}

void XYWTH::clearBuffer(char b[], uint8_t blen) {
    memset(b, '\0', blen * sizeof(char));
}

void XYWTH::clearBuffer() {
    clearBuffer(buffer, RX_BUFFER_SIZE);
    readP = 0;
}

boolean XYWTH::isPrintable(uint8_t chr) {
    return chr >= ' ' && chr <= '~';
}

boolean XYWTH::isControl(uint8_t chr) {
    return chr == '\r' || chr == '\n';
}

boolean XYWTH::isStorable(uint8_t chr) {
    return isPrintable(chr) || isControl(chr);
}

boolean XYWTH::isDigit(uint8_t chr) {
    return chr >= '0' && chr <= '9';
}

boolean XYWTH::copyDigits(char number[], char line[], uint8_t linelen) {
    uint8_t copyPos = 0;
    boolean numberFound = false;
    for (uint8_t j = 0; j < linelen && line[j] != '\0'; j++) {
        if (isDigit(line[j])) {
            numberFound = true;
            number[copyPos] = line[j];
            copyPos++;
        }
    }
    number[copyPos] = '\0';
    return numberFound;
}

int XYWTH::digitsToInt(char digits[]) {
    int i = 0;
    sscanf(digits, "%d", &i);
    return i;
}

boolean XYWTH::copyLine(char line[], char buffer[], uint8_t& start, uint8_t end) {
    uint8_t linepos = 0;
    boolean found = false;
    for(uint8_t i = start; i < end; i ++) {
        if(isControl(buffer[i]) && (i - start) > 2) {
            found = true;
            start = i;
            line[linepos] = '\0';
            break;
        } else if (isPrintable(buffer[i])) {
            line[linepos] = buffer[i];
            linepos++;
        }
    }
    return found;
}

boolean XYWTH::contains(char line[], char pattern[], uint8_t linelen, uint8_t plen) {
    for(uint8_t i = 0; i < linelen; i++) {
        if (line[i] == pattern[0]) {
            boolean complete = true;
            for (uint8_t j = 0; j < plen && j + i < linelen; j++) {
                if (line[i + j] != pattern[j]) {
                    complete = false;
                    break;
                }
            }
            if(complete) return true;
        }
    }
    return false;
}

int XYWTH::getNumber(char line[], uint8_t linelen) {
    char number[RX_BUFFER_SIZE];
    clearBuffer(number, RX_BUFFER_SIZE);
    copyDigits(number, line, linelen);
    return digitsToInt(number);
}

void XYWTH::pushRegister(CaptureRegister &reg) {
    memcpy(&capture[captureP % CAPTURE_HOLD_SIZE], &reg, sizeof(CaptureRegister));
    captureP++;
}

boolean XYWTH::isDataLine(char line[], uint8_t linelen) {
    return contains(line, "%", linelen, 1);
}

void XYWTH::processLine(char line[], uint8_t linelen) {
    boolean isHumidity = contains(line, "%", linelen, 1);
    boolean relayOFF = contains(line, "OFF", linelen, 3);
    int number = getNumber(line, linelen);

    if(isHumidity) {
        // SERIAL_PORT.print("Humedad    : ");
        currentCapture.humidity = (int16_t) number;
        currentCapture.epoch.timestamp = currentEpoch;
        if (relayOFF) {
            currentCapture.relayState[1] = 0;
        } else {
            currentCapture.relayState[1] = 1;
        }
        pushRegister(currentCapture);
    } else {
        // SERIAL_PORT.print("Temperatura: ");
        currentCapture.temperature = (int16_t) number;
        currentCapture.epoch.timestamp = currentEpoch;
        if (relayOFF) {
            currentCapture.relayState[0] = 0;
        } else {
            currentCapture.relayState[0] = 1;
        }
    }
    // SERIAL_PORT.print((float) number / 10);
}


boolean XYWTH::parseCapture() {
    waitReply();
    char line[RX_BUFFER_SIZE];
    clearBuffer(line, RX_BUFFER_SIZE);
    uint8_t startPos = 0;
    boolean gotDataLine = false;
    while(copyLine(line, buffer, startPos, RX_BUFFER_SIZE)) {
        processLine(line, RX_BUFFER_SIZE);
        if (isDataLine(line, RX_BUFFER_SIZE))
            gotDataLine = true;
        // SERIAL_PORT.println();
    }
    clearBuffer();
    return gotDataLine;
}

void XYWTH::update() {
    if(capturing)
        parseCapture();
}

void XYWTH::waitReply() {
    boolean loop = true;
    rxTimedOut.reset();
    while(softSerial.available() > 0 || loop) {
        uint8_t chr = softSerial.read();
        if (isStorable(chr)) {
            buffer[readP % RX_BUFFER_SIZE] = chr;
            if (chr == '\n')
                loop = false;

            readP++;
        }
        if(rxTimedOut.event())
            loop = false;
        rxTimedOut.update();
    }
    rxTimedOut.reset();
}

void XYWTH::captureTX(const char command[]) {
    boolean wascapturing = isCapturing();
    stopCapture();
    softSerial.print(command);
    waitReply();
    startCapture();
    while(!parseCapture());
    if(!wascapturing) stopCapture();
}

void XYWTH::enableTempRelay() {
    captureTX("T:ON");
}

void XYWTH::disableTempRelay() {
    captureTX("T:OFF");
}

void XYWTH::enableHumidityRelay() {
    captureTX("H:ON");
}

void XYWTH::disableHumidityRelay() {
    captureTX("H:OFF");
}

void XYWTH::setStopTemperature(float temp) {
    if (isCapturing()) suspendCapture();
    char str[10];
    sprintf(str, "TP:%d.%d", (int) temp, (int) abs(round(temp * 10) % 10));
    softSerial.print(str);
    waitReply();
    if (isCapturing()) resumeCapture();
}

void XYWTH::setStopHumidity(float temp) {
    if (isCapturing()) suspendCapture();
    char str[10];
    sprintf(str, "HP:%d.%d", (int) temp, (int) abs(round(temp * 10) % 10));
    softSerial.print(str);
    waitReply();
    if (isCapturing()) resumeCapture();
}

void XYWTH::setStartTemperature(float temp) {
    if (isCapturing()) suspendCapture();
    char str[10];
    sprintf(str, "TS:%d.%d", (int) temp, (int) abs(round(temp * 10) % 10));
    softSerial.print(str);
    waitReply();
    if (isCapturing()) resumeCapture();
}

void XYWTH::setStartHumidity(float temp) {
    if (isCapturing()) suspendCapture();
    char str[10];
    sprintf(str, "HS:%d.%d", (int) temp, (int) abs(round(temp * 10) % 10));
    softSerial.print(str);
    waitReply();
    if (isCapturing()) resumeCapture();
}

void XYWTH::resumeCapture() {
    softSerial.print("start");
    waitReply();
}

void XYWTH::startCapture() {
    resumeCapture();
    capturing = true;
}

void XYWTH::suspendCapture() {
    softSerial.print("stop");
    waitReply();
}

void XYWTH::stopCapture() {
    suspendCapture();
    capturing = false;
}


#endif