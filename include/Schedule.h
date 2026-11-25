#pragma once
#include "Config.h"
// Simulated civil clock and per-day automatic-feed ledger.
class Schedule {
 public:
  // Start a new simulated day at midnight; discard the feed ledger.
  void clear(unsigned long now);
  // Set hour/minute within the same day; ignore invalid input, retain ledger.
  bool set(uint8_t hour, uint8_t minute, unsigned long now);
  // Advance whole minutes, retaining fractional time across millis rollover.
  void tick(unsigned long now);
  // Return an active [start,end) window index, or NO_WINDOW.
  int8_t window() const;
  // True only for an unconsumed, valid window index.
  bool available(int8_t slot) const;
  // Consume quota when the gate starts, even if subsequently interrupted.
  void consume(int8_t slot);
 private:
  uint8_t hour_ = 0, minute_ = 0, used_ = 0;
  unsigned long anchor_ = 0;
};
