#include "Scale.h"
#include <math.h>
#include <util/atomic.h>

// Clock one ready conversion without re-entering HX711::read()'s unbounded wait.
// Restore the IRQ mask after each pulse so Servo interrupts cannot stretch SCK high.
static bool readReadyRaw(long& value) {
  long raw = 0;
  for (uint8_t bit = 0; bit < cfg::HX711_DATA_BITS; ++bit) {
    uint8_t level;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      digitalWrite(cfg::HX711_SCK, HIGH);
      level = digitalRead(cfg::HX711_DT);
      digitalWrite(cfg::HX711_SCK, LOW);
    }
    raw = (raw << 1) | level;
  }
  bool released;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    digitalWrite(cfg::HX711_SCK, HIGH); // Select channel A, gain 128 for next conversion.
    digitalWrite(cfg::HX711_SCK, LOW);
    released = digitalRead(cfg::HX711_DT) == HIGH;
  }
  if (!released) return false; // Pulse 25 must release DOUT, even for an all-zero word.
  const long modulus = 1L << cfg::HX711_DATA_BITS;
  value = raw >= modulus / 2 ? raw - modulus : raw;
  // At either rail the ADC cannot distinguish the endpoint from an overload.
  return value != cfg::HX711_RAW_MIN && value != cfg::HX711_RAW_MAX;
}
void Scale::begin() { adc_.begin(cfg::HX711_DT, cfg::HX711_SCK); enable(true); }
void Scale::enable(bool on) {
  on_ = on;
  retryPending_ = false;
  next_ = count_ = 0;
  if (on) adc_.power_up(); else adc_.power_down();
}
void Scale::tick(unsigned long now) {
  if (!on_) return;
  if (retryPending_ && now - last_ < cfg::SCALE_RETRY_MS) return;
  if (now - last_ >= cfg::SCALE_STALE_MS) next_ = count_ = 0;
  if (!adc_.is_ready()) return;
  long raw;
  last_ = now;
  retryPending_ = !readReadyRaw(raw);
  if (retryPending_) {
    next_ = count_ = 0;
    return;
  }
  if (!count_) sum_ = 0;
  else if (count_ == cfg::FILTER_SIZE) sum_ -= samples_[next_];
  sum_ += raw;
  samples_[next_] = raw;
  times_[next_] = now;
  next_ = (next_ + 1) % cfg::FILTER_SIZE;
  if (count_ < cfg::FILTER_SIZE) ++count_;
  if (cfg::SCALE_CALIBRATED) {
    // Subtract the platform offset before converting the mean to AVR float.
    const long whole = sum_ / count_ - cfg::ZERO_RAW;
    const float fraction = (sum_ % count_) / static_cast<float>(count_);
    netGrams_ = (whole + fraction) / cfg::SCALE - cfg::TARE_G;
  }
#if DEBUG
  Serial.print(F("raw=")); Serial.println(raw);
#endif
}
bool Scale::stableSum(unsigned long now, unsigned long maxAge, long& value) const {
  if (!valid() || count_ != cfg::FILTER_SIZE || now - last_ > cfg::SAMPLE_FRESH_MS) return false;
  // When full, next_ points to the oldest sample in the ring.
  unsigned long oldestAge = now - times_[next_];
  if (oldestAge > cfg::STABLE_WINDOW_MS || oldestAge > maxAge) return false;
  long low = samples_[0], high = low;
  for (uint8_t i = 1; i < count_; ++i) {
    low = min(low, samples_[i]);
    high = max(high, samples_[i]);
  }
  if ((high - low) > cfg::STABLE_SPAN_G * fabsf(cfg::SCALE)) return false;
  value = sum_;
  return true;
}

void PortionCheck::begin(const Scale& scale, unsigned long now) {
  grams_ = 0;
  state_ = cfg::SCALE_MOUNT != cfg::ScaleMount::Unconfigured &&
           scale.stableSum(now, cfg::STABLE_WINDOW_MS, beforeSum_) ? State::Feeding : State::Unverified;
}

void PortionCheck::tick(bool closed, const Scale& scale, unsigned long now) {
  if (state_ == State::Feeding && closed) {
    state_ = State::Checking;
    closedAt_ = now;
  }
  if (state_ != State::Checking) return;
  unsigned long elapsed = now - closedAt_;
  if (elapsed >= cfg::PORTION_TIMEOUT_MS) { state_ = State::Unverified; return; }
  if (elapsed < cfg::PORTION_SETTLE_MS) return;
  long afterSum;
  if (!scale.stableSum(now, elapsed - cfg::PORTION_SETTLE_MS, afterSum)) return;
  // Full windows have equal length: zero and tare cancel. Widen before subtraction.
  const int64_t change = static_cast<int64_t>(afterSum) - beforeSum_;
  const float delta = (cfg::SCALE_MOUNT == cfg::ScaleMount::Hopper ? -change : change) /
                      (cfg::FILTER_SIZE * cfg::SCALE);
  if (fabsf(delta) < cfg::MIN_PORTION_G) state_ = State::CheckOutlet;
  else if (delta < 0 || delta > cfg::MAX_PORTION_G) state_ = State::Unverified;
  else { grams_ = lroundf(delta); state_ = State::Measured; }
}

void PortionCheck::cancel() {
  if (state_ == State::Feeding || state_ == State::Checking) state_ = State::Unverified;
}
