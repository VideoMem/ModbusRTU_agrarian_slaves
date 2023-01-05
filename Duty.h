/*
*  Async duty cycle pulser timer.h
*
*/

#ifndef DUTY_H
#define DUTY_H
#define DUTY_VERSION "0"
#include "Timers.h"
#include "Toggle.h"

class Duty {
 public:
   Duty();
   void setMS_on(unsigned long ms);
   void setS_on(unsigned long s);
   void setMS_off(unsigned long ms);
   void setS_off(unsigned long s);
   void setMS(unsigned long ms);
   void setS(unsigned long s);
   void reset();
   void enable();
   void disable();
   void sync_on();
   void sync_off();
   void update();
   bool value();

 private:
  Timer Ton;
  Timer Toff;
  Toggle Output;
  int enabled;
};

#endif
