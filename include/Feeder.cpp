#include "Feeder.h"
void Feeder::enable(bool on, unsigned long now) {
  on_ = on;
  servo_.write(cfg::CLOSED_DEG);
  if (!servo_.attached()) servo_.attach(cfg::SERVO);
  state_ = State::Closing; since_ = now;
}
bool Feeder::start(unsigned long now) {
  if (!ready()) return false;
  servo_.write(cfg::OPEN_DEG);
  state_ = State::Open; since_ = now;
#if DEBUG
  Serial.println(F("feed start"));
#endif
  return true;
}
void Feeder::tick(unsigned long now) {
  if (state_ == State::Open && now - since_ >= cfg::FEED_MS) enable(on_, now);
  if (state_ != State::Closing || now - since_ < cfg::CLOSE_TRAVEL_MS) return;
  state_ = State::Idle;
  if (!on_) { servo_.detach(); digitalWrite(cfg::SERVO, LOW); }
}
void FeederController::begin() {
#if DEBUG
  Serial.begin(cfg::SERIAL_BAUD);
#endif
  gate_.enable(true, millis());
  power_.begin(); manual_.begin(); range_.begin(); scale_.begin(); display_.begin();
  // DS3231: initialize the RTC here instead of seeding from START_*.
  clock_.set(cfg::START_HOUR, cfg::START_MINUTE, millis());
}
void FeederController::tick() {
  unsigned long now = millis();
  const bool toggle = power_.pressed(now);
  const bool feed = manual_.pressed(now, !toggle && canFeed());
  if (toggle) {
    on_ = !on_;
    gate_.enable(on_, now); scale_.enable(on_); range_.reset();
    portion_.cancel();
  }
  gate_.tick(now); clock_.tick(now);
  int8_t slot = clock_.window();
  if (slot != previous_) range_.reset();
  previous_ = slot;
  if (on_) {
    if (gate_.ready()) scale_.tick(now);
    portion_.tick(gate_.ready(), scale_, now);
    if (!toggle && feed) startPortion(now);
    if (canFeed() && clock_.available(slot)) {
      bool qualified = range_.nearForInterval(millis());
      now = millis(); clock_.tick(now); // A pulse wait may have crossed the end boundary.
      if (qualified && clock_.window() == slot && startPortion(now)) clock_.consume(slot);
    } else range_.reset();
  }
  // In standby, detach before touching a potentially faulty I2C bus.
  if (!gate_.motionPending()) display_.show(on_, clock_.window() != cfg::NO_WINDOW, scale_, portion_);
}

bool FeederController::canFeed() const {
  return gate_.ready() && portion_.state() != PortionCheck::State::Checking;
}

bool FeederController::startPortion(unsigned long now) {
  if (!canFeed()) return false;
  portion_.begin(scale_, now);
  return gate_.start(now);
}
