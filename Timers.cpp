#include "Timers.h"
#include <Arduino.h>

Timer::Timer() {
  elapsed = 0;
  enabled = 0;
  preset = 0;
  last_read = 0;
  trigged = 0;
}

void Timer::check() {
 if (preset == 0)
    enabled = 0; 
 else
    if (elapsed > preset) {
      trigged = 1;
      elapsed = 0;
      enabled = 0;
    } 
}

void Timer::update() {
   unsigned long read = millis(); 

   if (enabled == 1) {   
     // read overflow
     if (read < last_read)
       elapsed += read;
     else {
       elapsed += read - last_read;
     }     
   }
   
   last_read = read;
   check();
}

unsigned long Timer::valueMS() {
  return elapsed; 
}

unsigned long Timer::valueS() {
  return elapsed / 1000; 
}

int Timer::event() {
  if (trigged == 1) {
     trigged = 0;
     enabled = 1;
     return 1;
  }
  return 0;
}

void Timer::setMS(unsigned long value) {
   preset = value; 
   enabled = 1;
}

void Timer::setS(unsigned int s) {
  unsigned long aux = s * 1000;
  setMS(aux);
}

void Timer::reset() {
  update();
  elapsed = 0;
  trigged = 0;
}

void Timer::enable() {
  reset();
}

void Timer::disable() {
  reset();
  enabled = 0;
}

