#include "Toggle.h"

Toggle::Toggle() {
    status = false;
}

void Toggle::set() {
    status = true;
}

void Toggle::reset() {
    status = false;
}

bool Toggle::value() {
    return status;
}

void Toggle::change() {
    if(status == false)
        status = true;
    else
        status = false;
}

