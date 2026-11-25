#pragma once
#include <Arduino.h>
// Requires an uninterrupted run of valid near echoes.
class RangeSensor {
 public:
  // Leave trigger low between pings.
  void begin();
  // Forget proximity evidence without bypassing the ping rate limit.
  void reset();
  // Issue at most one due ping; report a complete near interval.
  bool nearForInterval(unsigned long now);
 private:
  // Return cm, or NO_ECHO when the bounded pulse wait expires.
  float measure();
  bool near_ = false;
  unsigned long pingAt_ = 0, since_ = 0;
};
