#ifndef RTC_H
#define RTC_H
#define TIMERS_VERSION "0"

#define RTC_MEM_START 49

union Epoch {
    uint32_t timestamp;
    int16_t nibbles[2];
};

union Epoch readRTCEpoch(DateTime& now) {
    union Epoch epoch;
    epoch.timestamp = now.getEpoch();
    return epoch;
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
        case 7:
            return readRTCEpoch(now).nibbles[0];
            break;
        case 8:
            return readRTCEpoch(now).nibbles[1];
            break;
        default:
            blinkTimer.setMS(BLINK_ON_ERROR);
            break;
    }
}

uint8_t writeRTCYY(uint16_t year) {
    if(year >= 2000) {
        DateTime now = rtc.now();
        DateTime dt(year, now.month(), now.date(), now.hour(), now.minute(), now.second(), now.dayOfWeek());
        rtc.setDateTime(dt);
        return STATUS_OK;
    } else
        return STATUS_ILLEGAL_DATA_VALUE;
}

uint8_t writeRTCMM(uint16_t month) {
    if(month > 0 && month < 13) {
        DateTime now = rtc.now();
        DateTime dt(now.year(), month, now.date(), now.hour(), now.minute(), now.second(), now.dayOfWeek());
        rtc.setDateTime(dt);
        return STATUS_OK;
    } else
        return STATUS_ILLEGAL_DATA_VALUE;
}

uint8_t writeRTCDD(uint16_t day) {
    if(day > 0 && day < 32) {
        DateTime now = rtc.now();
        DateTime dt(now.year(), now.month(), day, now.hour(), now.minute(), now.second(), now.dayOfWeek());
        rtc.setDateTime(dt);
        return STATUS_OK;
    } else
        return STATUS_ILLEGAL_DATA_VALUE;
}

uint8_t writeRTCHH(uint16_t hour) {
    if(hour >= 0 && hour < 24) {
        DateTime now = rtc.now();
        DateTime dt(now.year(), now.month(), now.date(), hour, now.minute(), now.second(), now.dayOfWeek());
        rtc.setDateTime(dt);
        return STATUS_OK;
    } else
        return STATUS_ILLEGAL_DATA_VALUE;
}

uint8_t writeRTCmm(uint16_t minute) {
    if(minute >= 0 && minute < 60) {
        DateTime now = rtc.now();
        DateTime dt(now.year(), now.month(), now.date(), now.hour(), minute, now.second(), now.dayOfWeek());
        rtc.setDateTime(dt);
        return STATUS_OK;
    } else
        return STATUS_ILLEGAL_DATA_VALUE;
}

uint8_t writeRTCSS(uint16_t second) {
    if(second >= 0 && second < 60) {
        DateTime now = rtc.now();
        DateTime dt(now.year(), now.month(), now.date(), now.hour(), now.minute(), second, now.dayOfWeek());
        rtc.setDateTime(dt);
        return STATUS_OK;
    } else
        return STATUS_ILLEGAL_DATA_VALUE;
}

uint8_t writeRTCDW(uint16_t dayOfWeek) {
    if(dayOfWeek >= 0 && dayOfWeek < 7) {
        DateTime now = rtc.now();
        DateTime dt(now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second(), dayOfWeek);
        rtc.setDateTime(dt);
        return STATUS_OK;
    } else
        return STATUS_ILLEGAL_DATA_VALUE;
}

union Epoch writeRTCEpoch(uint16_t value, uint8_t nibble) {
    DateTime now = rtc.now();
    union Epoch epoch;
    epoch.timestamp = now.getEpoch();
    epoch.nibbles[nibble] = value;
    return epoch;
}

void commitEpoch(union Epoch &epoch) {
    rtc.setEpoch(epoch.timestamp);
}

uint8_t writeRTC(uint8_t reg, uint16_t value, union Epoch &epoch) {
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
        case 7:
            epoch = writeRTCEpoch(value, 0);
            break;
        case 8:
            epoch = writeRTCEpoch(value, 1);
            commitEpoch(epoch);
            break;
        default:
            blinkTimer.setMS(BLINK_ON_ERROR);
            break;
    }
    return STATUS_OK;
}

#endif
