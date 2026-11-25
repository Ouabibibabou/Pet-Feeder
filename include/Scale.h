#pragma once
#include "Config.h"
#include <HX711.h>
// Moving average of ready HX711 conversions, with no tare at boot.
class Scale {
 public:
  // Start the ADC without assuming the platform is empty.
  void begin();
  // Stop or resume conversions and discard all old samples.
  void enable(bool on);
  // Read at most one ready conversion and expire stale measurements.
  void tick(unsigned long now);
  // Gram values require both calibration and at least one current conversion.
  bool valid() const { return cfg::SCALE_CALIBRATED && count_ != 0; }
  // Return net grams; call only when valid.
  float grams() const { return netGrams_; }
  // Snapshot the exact ADC sum of a fresh, full, stable window within maxAge.
  bool stableSum(unsigned long now, unsigned long maxAge, long& value) const;
 private:
  HX711 adc_;
  bool on_ = false;
  bool retryPending_ = false;
  uint8_t next_ = 0, count_ = 0;
  long samples_[cfg::FILTER_SIZE] = {};
  long sum_ = 0; // Up to 255 signed 24-bit samples fit in a signed 32-bit sum.
  float netGrams_ = 0;
  unsigned long times_[cfg::FILTER_SIZE] = {};
  unsigned long last_ = 0; // Last frame attempt; a failed attempt also empties the ring.
};

// Observes one gate cycle; never issues gate commands or changes schedule quota.
class PortionCheck {
 public:
  enum class State : uint8_t { None, Feeding, Checking, Measured, CheckOutlet, Unverified };
  // Capture a baseline before the servo moves; unavailable data never prevents feeding.
  void begin(const Scale& scale, unsigned long now);
  // Start settling after closing travel, then wait a bounded time for fresh samples.
  void tick(bool closed, const Scale& scale, unsigned long now);
  // Invalidate only an interrupted observation, preserving completed results.
  void cancel();
  State state() const { return state_; }
  long grams() const { return grams_; }
 private:
  State state_ = State::None;
  unsigned long closedAt_ = 0;
  long beforeSum_ = 0;
  long grams_ = 0;
};
