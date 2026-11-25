#pragma once
#include <Servo.h>
#include "Buttons.h"
#include "Display.h"
#include "RangeSensor.h"
#include "Schedule.h"

// Gate motion only; closing travel is busy, including during standby.
class Feeder {
 public:
  // Close at startup or on power changes; detach only after closing in standby.
  void enable(bool on, unsigned long now);
  // Start only from the fully closed, enabled state.
  bool start(unsigned long now);
  // Advance one due motion transition without waiting.
  void tick(unsigned long now);
  // False while moving, open, or disabled.
  bool ready() const { return on_ && !motionPending(); }
  // True until both the open interval and commanded closing travel are over.
  bool motionPending() const { return state_ != State::Idle; }
 private:
  enum class State : uint8_t { Idle, Open, Closing };
  Servo servo_;
  State state_ = State::Idle;
  bool on_ = false;
  unsigned long since_ = 0;
};

// Application coordinator; device drivers do not depend on scheduling policy.
class FeederController {
 public:
  // Initialize drivers, close the gate and seed the simulated clock.
  void begin();
  // Process power first; a manual request takes precedence over automatic feed.
  void tick();
 private:
  // A closed gate alone is not sufficient while the previous portion is being weighed.
  bool canFeed() const;
  // Both trigger paths share the same pre-motion snapshot and admission rule.
  bool startPortion(unsigned long now);
  Buttons power_{cfg::KEY_POWER}, manual_{cfg::KEY_FEED};
  Feeder gate_;
  Scale scale_;
  PortionCheck portion_;
  RangeSensor range_;
  Schedule clock_;
  Display display_;
  bool on_ = true;
  int8_t previous_ = cfg::NO_WINDOW;
};
