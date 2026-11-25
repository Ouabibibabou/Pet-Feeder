#pragma once
#include <Arduino.h>

// One independently debounced active-low key.
class Buttons {
 public:
  // Bind an input without touching hardware before setup.
  explicit Buttons(uint8_t pin) : pin_(pin) {}
  // Adopt the initial level so a key held at boot does not fire.
  void begin();
  // Ignore a press observed while disabled until the key is stably released.
  bool pressed(unsigned long now, bool enabled = true);
 private:
  uint8_t pin_, raw_, stable_;
  bool suppressed_ = false;
  unsigned long changed_;
};
