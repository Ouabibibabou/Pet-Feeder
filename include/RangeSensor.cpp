#include "RangeSensor.h"
#include "Config.h"
void RangeSensor::begin() {
  digitalWrite(cfg::TRIG, LOW); pinMode(cfg::TRIG, OUTPUT); pinMode(cfg::ECHO, INPUT);
}
void RangeSensor::reset() { near_ = false; }
float RangeSensor::measure() {
  digitalWrite(cfg::TRIG, HIGH);
  delayMicroseconds(cfg::TRIGGER_US); // Trigger strobe, not a scheduling delay.
  digitalWrite(cfg::TRIG, LOW);
  unsigned long width = pulseIn(cfg::ECHO, HIGH, cfg::ECHO_TIMEOUT_US);
  return width ? width / cfg::US_PER_CM : cfg::NO_ECHO;
}
bool RangeSensor::nearForInterval(unsigned long now) {
  if (now - pingAt_ < cfg::PING_MS) return false;
  float distance = measure();
  const unsigned long seen = millis();
  if (seen - pingAt_ > cfg::MAX_PING_GAP_MS) reset();
  pingAt_ = seen; // Timestamp completed samples, including the time spent waiting for echo.
  if (distance == cfg::NO_ECHO || distance > cfg::NEAR_CM) { reset(); return false; }
  if (!near_) { near_ = true; since_ = seen; }
  return seen - since_ >= cfg::NEAR_MS;
}
