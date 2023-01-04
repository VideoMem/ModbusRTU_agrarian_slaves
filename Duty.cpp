#include "Duty.h"

Duty::Duty() {
    Ton.setMS(300);
    Toff.setMS(600);
    enable();
    reset();
}

void Duty::reset() {
    Ton.enable();
    Ton.reset();
    Toff.enable();
    Toff.reset();
    Output.reset();
}

void Duty::setMS(unsigned long ms) {
   Toff.setMS(ms);
   Ton.setMS(ms);
}

void Duty::setS(unsigned long s) {
   Toff.setS(s);
   Ton.setS(s);
}

void Duty::setMS_on(unsigned long ms) {
   Ton.setMS(ms);
}

void Duty::setS_on(unsigned long s) {
   Ton.setS(s);
}

void Duty::setMS_off(unsigned long ms) {
   Toff.setMS(ms);
}

void Duty::setS_off(unsigned long s) {
   Toff.setS(s);
}

void Duty::enable() {
    enabled = 1;
}

void Duty::disable() {
    enabled = 0;
}

void Duty::sync_on()  {
    Toff.reset();
    if (enabled)
        Output.set();
}

void Duty::sync_off()  {
    Ton.reset();
    if (enabled)
        Output.reset();
}

void Duty::update() {
    Ton.update();
    Toff.update();

    if (Ton.event() && !Output.value()) {
        sync_on();
    }

    if (Toff.event() && Output.value()) {
        sync_off();
    }

    if (!enabled)
        Output.reset();
}

int Duty::value() {
    return Output.value();
}

