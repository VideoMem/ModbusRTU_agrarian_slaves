#include "Duty.h"

Duty::Duty() {
    Ton.setMS(300);
    Toff.setMS(600);
    enable();
    reset();
}

void Duty::reset() {
    Ton.reset();
    Toff.reset();
    Output.reset();
}

void Duty::setMS(unsigned long ms) {
    setMS_on(ms);
    setMS_off(ms);
}

void Duty::setS(unsigned long s) {
    setMS(s * 1000);
}

void Duty::setMS_on(unsigned long ms) {
   Ton.setMS(ms);
}

void Duty::setS_on(unsigned long s) {
   setMS_on(s * 1000);
}

void Duty::setMS_off(unsigned long ms) {
   Toff.setMS(ms + Ton.preset_value());
}

void Duty::setS_off(unsigned long s) {
   setMS_off(s * 1000);
}

void Duty::enable() {
    enabled = 1;
}

void Duty::disable() {
    enabled = 0;
}

void Duty::sync_on()  {
    Ton.reset();
    Toff.reset();
    if (enabled)
        Output.set();
}

void Duty::sync_off()  {
    if (enabled)
        Output.reset();
}

void Duty::update() {
    Ton.update();
    Toff.update();

    if (Ton.event() && Output.value() == true) {
        sync_off();
    }

    if (Toff.event()) {
        sync_on();
    }

    if (!enabled)
        Output.reset();
}

bool Duty::value() {
    return Output.value();
}

