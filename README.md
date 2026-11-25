# Arduino Uno Pet Feeder

This firmware controls an SG90 dispensing gate on a classic Arduino Uno (ATmega328P). An HC-SR04 detects when a pet approaches. An HX711 load cell amplifier measures the food remaining in the hopper or bowl and checks the weight change after each dispense.

The default feeding windows are 07:00-08:00, 12:00-13:00, and 17:00-18:00. During each window, a reading of 10 cm or less sustained for 4 seconds opens the gate from 0 to 90 degrees. The gate closes after 2 seconds. Each window allows one automatic dispense; the end time is exclusive.

**Before first use:** The feeder can dispense even when the scale is uncalibrated. Until calibration, it cannot display grams or verify portions. There is no real-time clock: a reset restarts the clock at the configured time (midnight by default) and clears the day's feeding record.

## Wiring

Disconnect power before wiring. The HX711 needs a separate load cell; the table assumes a four-wire full bridge.

| Device pin | Uno pin or supply | Notes |
|---|---|---|
| Mode button | D2 and GND | `KEY_POWER`: toggle standby |
| Feed button | D3 and GND | `KEY_FEED`: dispense one portion |
| SG90 signal, V+, GND | D9, external 5 V, common ground | `SERVO` drives the signal wire only |
| HX711 DT, SCK | D4, D5 | `HX711_DT` is the module's data output; `HX711_SCK` is its clock input. Do not swap them. |
| HX711 VCC, GND | External 5 V, common ground | Use a 5 V compatible module |
| Load cell excitation +/-, signal +/- | HX711 E+/E-, A+/A- respectively | No direct connection to the Uno. Follow the load cell pinout, not the wire colors. |
| HC-SR04 TRIG, ECHO | D6, D7 | `TRIG` sends the pulse; `ECHO` receives the return |
| HC-SR04 VCC, GND | External 5 V, common ground | The 5 V Echo output can connect directly to the classic Uno, not to a 3.3 V-only input. |
| 1602 LCD with PCF8574: SDA, SCL | A4, A5 | `I2C_SDA`, `I2C_SCL`; default `LCD_ADDRESS` is `0x27` |
| LCD VCC, GND | Uno 5 V, GND | Powers the I2C backpack |

- Use a regulated external 5 V supply for the servo, HX711 and HC-SR04, with a common ground to the Uno. Give the servo its own power branch to reduce scale noise. When the Uno is on USB power, do not tie the two 5 V outputs together.
- On each four-pin button, choose two pins that connect **only when pressed**. Both inputs use internal pull-ups and read LOW when pressed; no external resistors are needed.
- Other I2C devices can share A4/A5 with the LCD if their addresses differ.

## Build and upload

**PlatformIO:** Run from the repository root. PlatformIO installs LiquidCrystal_I2C, HX711 and Servo at the versions pinned in [platformio.ini](platformio.ini). The AVR framework includes Wire.

```sh
pio run -e uno
pio run -e uno -t upload
```

**Arduino IDE:**

1. Install Arduino AVR Boards 1.8.6. Select **Arduino Uno** and the connected port.
2. In Library Manager, install **LiquidCrystal I2C** (Frank de Brabander, 1.1.2), **HX711 Arduino Library** (Bogdan Necula, 0.7.5), and **Servo** (1.2.2).
3. Create an empty sketch named `PetFeeder/PetFeeder.ino` outside the repository. Copy `src/main.cpp` and all `.h` and `.cpp` files from `include/` next to the `.ino` file. Leave the `.ino` empty: `main.cpp` already defines `setup()` and `loop()`. Verify and upload.

## Setup and calibration

Edit [Config.h](include/Config.h), then rebuild and upload. In the Arduino IDE, edit the copy in the sketch directory.

1. **Test the empty gate.** Press Feed and check that the gate opens and closes. Open it again, then press Mode while it is open. The gate should start closing and stay closed after the servo pulses stop. Press Mode again to resume.
2. **Calibrate the bare platform.** Set `DEBUG=1`, upload, and open the serial monitor at 115200 baud. Remove the container. Average several stable `raw=` readings and record the result as `ZERO_RAW`. Add a known mass and average the loaded readings. Set `SCALE = (loaded_average - ZERO_RAW) / mass_in_grams`. Keep the sign; the result must be nonzero.
3. **Set the tare and scale location.** Set `TARE_G` to the empty container's measured mass (default: 70 g) and `SCALE_CALIBRATED` to `true`. Set `SCALE_MOUNT` to `ScaleMount::Hopper` if dispensing removes weight from the scale, or `ScaleMount::Bowl` if it adds weight. If both sit on the same scale, leave the mount `Unconfigured`: total net weight is available, but a portion cannot be verified.
4. **Set the clock and check a portion.** Set `START_HOUR` and `START_MINUTE`, disable `DEBUG`, and upload. Outside a feeding window, check the display with a known weight. Load the intended food, check that the gate closes under load, and weigh a dispensed portion on a separate scale.

| Setting | Configuration |
|---|---|
| Feeding windows | `WINDOWS` uses minutes since midnight. List windows in start-time order, without overlaps. A single window cannot cross midnight. |
| Proximity | `NEAR_CM` sets the maximum distance; `NEAR_MS` sets the required time at that distance. |
| Gate travel | `CLOSED_DEG` and `OPEN_DEG` set the endpoints. `FEED_MS` sets the time before closing; `CLOSE_TRAVEL_MS` allows time for the gate to return. |
| Weight thresholds | `LOW_FOOD_G` defaults to 50 g. `MIN_PORTION_G` and `MAX_PORTION_G` bound the accepted portion change (3 to 200 g inclusive by default). |

## Controls and display

Feed works outside the scheduled windows and does not use a window's automatic dispense. Requests received while the gate is moving, dispensing, or waiting for the portion check are not queued. Mode takes priority: standby stops ranging and weighing, commands the gate closed, and allows 400 ms for it to return before stopping servo pulses. **Standby does not disconnect power.** The clock keeps running, and resuming does not grant another dispense in the same window.

| LCD | Meaning |
|---|---|
| `OFF` / `On` | `OFF` means standby. `On` replaces the weight during a feeding window; it does not mean the gate is open. |
| `Food: 85g` / `add more food` | Net weight on the scale. The refill message appears at or below 50 g. In bowl mode this is food in the bowl, not hopper inventory. |
| `Scale: --` | The scale is not calibrated, or no valid reading is available. |
| `Checking...` / `Last: 18g` | The firmware waits up to 5 seconds after the gate returns for a stable reading. `Last` reports the weight change in the expected direction, not how much the pet ate. |
| `Check outlet` / `Unverified` | `Check outlet` means the absolute weight change was below the minimum portion; check the hopper and outlet. `Unverified` means the portion could not be confirmed. Neither result triggers another dispense. |

## Tests and hardware limits

Run the AVR simulation tests with the repository defaults, Node.js 20.19 or later, and npm. `sim_hopper` and `sim_bowl` are test environments; do not upload them to hardware.

```sh
pio run -e uno -e sim_hopper -e sim_bowl
npm --prefix test install --no-package-lock
npm --prefix test test
```

Hardware acceptance testing is outstanding. The simulated clock can drift and does not retain time across power loss. The loop uses `millis()` to schedule work, but a single ultrasonic echo read can block for about 30 ms. The HC-SR04 has a blind zone of about 2 cm and is not a pinch safeguard. The SG90 has no position feedback, so the firmware cannot confirm that the gate has closed. Eating, refilling, or touching the container can also change the measured weight; a portion reading alone cannot diagnose an obstruction.

## Repository layout and license

[src/main.cpp](src/main.cpp) is the firmware entry point. [include/](include/) contains the configuration, hardware drivers and scheduling code; [lib/](lib/) documents third-party dependencies; [test/](test/) contains the simulation tests. The firmware is released under the [MIT License](LICENSE). Third-party libraries retain their own licenses.
