#include "Schedule.h"
void Schedule::clear(unsigned long now) { used_ = 0; set(0, 0, now); }
bool Schedule::set(uint8_t hour, uint8_t minute, unsigned long now) {
  if (hour >= cfg::HOURS_PER_DAY || minute >= cfg::MIN_PER_HOUR) return false;
  hour_ = hour; minute_ = minute; anchor_ = now;
  return true;
}
void Schedule::tick(unsigned long now) {
  // DS3231: replace only the time update here; clear used_ on date change.
  // Initialize the RTC on Wire at startup instead of setting a simulated time.
  unsigned long step = (now - anchor_) / cfg::MINUTE_MS;
  if (!step) return;
  anchor_ += step * cfg::MINUTE_MS;
  unsigned long total = hour_ * cfg::MIN_PER_HOUR + minute_ + step;
  if (total >= cfg::MIN_PER_DAY) used_ = 0;
  total %= cfg::MIN_PER_DAY;
  hour_ = total / cfg::MIN_PER_HOUR; minute_ = total % cfg::MIN_PER_HOUR;
}
int8_t Schedule::window() const {
  const uint16_t m = hour_ * cfg::MIN_PER_HOUR + minute_;
  for (uint8_t i = 0; i < cfg::WINDOW_COUNT; ++i)
    if (m >= cfg::WINDOWS[i][0] && m < cfg::WINDOWS[i][1]) return i;
  return cfg::NO_WINDOW;
}
bool Schedule::available(int8_t slot) const {
  return slot >= 0 && static_cast<size_t>(slot) < cfg::WINDOW_COUNT && !(used_ & (1U << slot));
}
void Schedule::consume(int8_t slot) { if (available(slot)) used_ |= 1U << slot; }
