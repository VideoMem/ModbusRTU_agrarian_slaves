/*
*  timers.h
*
*/

#ifndef TIMERS_H
#define TIMERS_H
#define TIMERS_VERSION "0"

class Timer {
 public: 
   Timer();
   void setMS(unsigned long ms);
   void setS(unsigned long s);
   void reset();
   void enable();
   void disable();
   void update();
   unsigned long valueMS();
   unsigned long valueS();
   int event();
   unsigned long preset_value();
   
 private:
  void check();
  unsigned long last_read;
  unsigned long elapsed;
  unsigned long preset;
  int enabled; 
  int trigged;
};

#endif
