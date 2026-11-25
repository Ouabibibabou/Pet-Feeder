#pragma once
#include <LiquidCrystal_I2C.h>
#include "Scale.h"
// LCD arbitration: standby, window, then weight; redraw only on change.
class Display {
 public:
  // Initialize the two-line LCD.
  void begin();
  // Round only for display, never for the low-food decision.
  void show(bool on, bool inWindow, const Scale& scale, const PortionCheck& portion);
 private:
  // Probe only before a changed row; latch display faults until reset.
  bool responds();
  // Pad or truncate a row and abandon it on the first timed-out character.
  void writeRow(uint8_t row, const char* text);
  // Refresh the portion row independently; blank it in standby.
  void showPortion(bool on, const PortionCheck& portion);
  PortionCheck::State lastPortion_ = PortionCheck::State::None;
  long lastPortionG_ = 0;
  enum class View : uint8_t { None, Off, On, Waiting, Low, Weight };
  LiquidCrystal_I2C lcd_{cfg::LCD_ADDRESS, cfg::LCD_COLS, cfg::LCD_ROWS};
  View last_ = View::None;
  bool online_ = false;
  long grams_ = 0;
};
