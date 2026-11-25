#pragma once
#include <Arduino.h>
#ifndef DEBUG
#define DEBUG 0
#endif
namespace cfg {
constexpr uint8_t KEY_POWER = 2, KEY_FEED = 3, SERVO = 9;
constexpr uint8_t I2C_SDA = A4, I2C_SCL = A5; // Fixed by Uno Wire hardware.
constexpr uint8_t HX711_DT = 4, HX711_SCK = 5, TRIG = 6, ECHO = 7;
constexpr uint8_t LCD_ADDRESS = 0x27, LCD_COLS = 16, LCD_ROWS = 2;
static_assert(LCD_COLS >= sizeof("add more food") - 1 && LCD_ROWS >= 2, "Display too small for status messages");
constexpr unsigned long I2C_TIMEOUT_US = 3000;
constexpr unsigned long SERIAL_BAUD = 115200, DEBOUNCE_MS = 30;
constexpr uint8_t CLOSED_DEG = 0, OPEN_DEG = 90;
constexpr unsigned long FEED_MS = 2000, CLOSE_TRAVEL_MS = 400;
constexpr unsigned long PING_MS = 60, ECHO_TIMEOUT_US = 30000, NEAR_MS = 4000;
constexpr unsigned long MAX_PING_GAP_MS = 180;
static_assert(PING_MS >= 60 && MAX_PING_GAP_MS > PING_MS && MAX_PING_GAP_MS < NEAR_MS, "Invalid ping timing");
constexpr unsigned int TRIGGER_US = 10;
constexpr float US_PER_CM = 58.0f, NEAR_CM = 10.0f, NO_ECHO = -1.0f;
constexpr uint8_t FILTER_SIZE = 5;
constexpr unsigned long SCALE_STALE_MS = 1500;
constexpr unsigned long SCALE_RETRY_MS = 100; // Backoff after a malformed HX711 frame.
constexpr float TARE_G = 70.0f, LOW_FOOD_G = 50.0f;
constexpr long ZERO_RAW = 0; // Bare platform without the container.
// Load a known mass; write (loaded raw - ZERO_RAW) / grams here, retaining sign.
constexpr float SCALE = 1.0f; // Counts/gram; placeholder until calibrated.
constexpr uint8_t HX711_DATA_BITS = 24; // Channel A, gain 128: one extra clock after data.
constexpr long HX711_RAW_MIN = -(1L << (HX711_DATA_BITS - 1));
constexpr long HX711_RAW_MAX = -HX711_RAW_MIN - 1;
static_assert(ZERO_RAW > HX711_RAW_MIN && ZERO_RAW < HX711_RAW_MAX, "ZERO_RAW must be an unsaturated ADC reading");
// Select Hopper (mass falls) or Bowl (mass rises) only after calibration.
enum class ScaleMount : uint8_t { Unconfigured, Hopper, Bowl };
#ifdef FEEDER_SIM_MOUNT
constexpr bool SCALE_CALIBRATED = true; // Synthetic input, never a deployment calibration.
constexpr ScaleMount SCALE_MOUNT = ScaleMount::FEEDER_SIM_MOUNT;
#else
constexpr bool SCALE_CALIBRATED = false; // Enable gram-based decisions only after calibration.
constexpr ScaleMount SCALE_MOUNT = ScaleMount::Unconfigured;
#endif
static_assert(SCALE_CALIBRATED || SCALE_MOUNT == ScaleMount::Unconfigured, "Calibrate before selecting a scale mount");
constexpr float STABLE_SPAN_G = 1.0f, MIN_PORTION_G = 3.0f, MAX_PORTION_G = 200.0f;
constexpr unsigned long SAMPLE_FRESH_MS = 250, STABLE_WINDOW_MS = 750;
constexpr unsigned long PORTION_SETTLE_MS = 700, PORTION_TIMEOUT_MS = 5000;
static_assert(FILTER_SIZE >= 2 && STABLE_SPAN_G < MIN_PORTION_G && MIN_PORTION_G <= MAX_PORTION_G,
              "Invalid portion thresholds");
static_assert(PORTION_TIMEOUT_MS > PORTION_SETTLE_MS + STABLE_WINDOW_MS, "Insufficient weighing time");
constexpr uint8_t START_HOUR = 0, START_MINUTE = 0, MIN_PER_HOUR = 60, HOURS_PER_DAY = 24;
constexpr unsigned long MINUTE_MS = 60000;
constexpr uint16_t MIN_PER_DAY = HOURS_PER_DAY * MIN_PER_HOUR;
// Chronological [start, end) pairs in minutes since midnight; adjacent slots are valid.
constexpr uint16_t WINDOWS[][2] = {{7*MIN_PER_HOUR, 8*MIN_PER_HOUR},
                                    {12*MIN_PER_HOUR, 13*MIN_PER_HOUR},
                                    {17*MIN_PER_HOUR, 18*MIN_PER_HOUR}};
constexpr size_t WINDOW_COUNT = sizeof(WINDOWS) / sizeof(WINDOWS[0]);
// C++11 constexpr recursion: validate order, duration and range without firmware overhead.
constexpr bool validWindows(size_t i = 0) {
  return i == WINDOW_COUNT ||
         (WINDOWS[i][0] < WINDOWS[i][1] && WINDOWS[i][1] <= MIN_PER_DAY &&
          (i == 0 || WINDOWS[i - 1][1] <= WINDOWS[i][0]) && validWindows(i + 1));
}
constexpr int8_t NO_WINDOW = -1;
static_assert(WINDOW_COUNT > 0 && WINDOW_COUNT <= 8, "WINDOWS must contain 1 to 8 slots");
static_assert(validWindows(), "WINDOWS must be chronological, non-overlapping, and satisfy 0 <= start < end <= 1440");
static_assert(FILTER_SIZE > 0 && SCALE != 0, "Invalid scale configuration");
static_assert(START_HOUR < HOURS_PER_DAY && START_MINUTE < MIN_PER_HOUR, "Invalid start time");
}
