#include "Buttons.h"
#include "Config.h"
void Buttons::begin() {
  pinMode(pin_, INPUT_PULLUP);
  stable_ = raw_ = digitalRead(pin_);
  changed_ = millis();
}
bool Buttons::pressed(unsigned long now, bool enabled) {
  const uint8_t level = digitalRead(pin_);
  if (level != raw_) { raw_ = level; changed_ = now; }
  if (!enabled && level == LOW) suppressed_ = true;
  if (now - changed_ < cfg::DEBOUNCE_MS) return false;
  if (level == HIGH) suppressed_ = false;
  if (raw_ == stable_) return false;
  stable_ = raw_;
  return stable_ == LOW && !suppressed_;
}
