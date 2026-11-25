#include "Display.h"
#include <math.h>
#include <Wire.h>

bool Display::responds() {
  Wire.beginTransmission(cfg::LCD_ADDRESS);
  return Wire.endTransmission() == 0 && !Wire.getWireTimeoutFlag();
}

void Display::begin() {
  Wire.begin();
  Wire.setWireTimeout(cfg::I2C_TIMEOUT_US, true);
  if (!responds()) return;
  lcd_.init();
  if (Wire.getWireTimeoutFlag()) return;
  lcd_.backlight();
  online_ = !Wire.getWireTimeoutFlag();
}

void Display::writeRow(uint8_t row, const char* text) {
  if (!online_) return;
  if (!responds()) { online_ = false; return; }
  lcd_.setCursor(0, row);
  for (uint8_t col = 0; col < cfg::LCD_COLS && !Wire.getWireTimeoutFlag(); ++col) {
    lcd_.write(*text ? *text++ : ' ');
  }
  online_ = !Wire.getWireTimeoutFlag();
}

void Display::show(bool on, bool inWindow, const Scale& scale, const PortionCheck& portion) {
  if (!online_) return;
  showPortion(on, portion);
  View view = View::Waiting;
  long value = 0;
  if (!on) view = View::Off;
  else if (inWindow) view = View::On;
  else if (scale.valid()) {
    float g = scale.grams();
    view = g <= cfg::LOW_FOOD_G ? View::Low : View::Weight;
    if (view == View::Weight) value = lroundf(g);
  }
  if (view == last_ && value == grams_) return;
  last_ = view; grams_ = value;
  char row[cfg::LCD_COLS + 1];
  switch (view) {
    case View::Off: strcpy_P(row, PSTR("OFF")); break;
    case View::On: strcpy_P(row, PSTR("On")); break;
    case View::Low: strcpy_P(row, PSTR("add more food")); break;
    case View::Weight:
      snprintf_P(row, sizeof(row), PSTR("Food: %ldg"), value); break;
    default: strcpy_P(row, PSTR("Scale: --")); break;
  }
  writeRow(0, row);
}

void Display::showPortion(bool on, const PortionCheck& portion) {
  using State = PortionCheck::State;
  State state = on ? portion.state() : State::None;
  long grams = state == State::Measured ? portion.grams() : 0;
  if (state == lastPortion_ && grams == lastPortionG_) return;
  lastPortion_ = state; lastPortionG_ = grams;
  char row[cfg::LCD_COLS + 1] = {};
  switch (state) {
    case State::Checking: strcpy_P(row, PSTR("Checking...")); break;
    case State::Measured:
      snprintf_P(row, sizeof(row), PSTR("Last: %ldg"), grams); break;
    case State::CheckOutlet: strcpy_P(row, PSTR("Check outlet")); break;
    case State::Unverified: strcpy_P(row, PSTR("Unverified")); break;
    default: break;
  }
  writeRow(1, row);
}
