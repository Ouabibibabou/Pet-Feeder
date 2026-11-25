#include <Arduino.h>
#include "Feeder.h"

namespace { FeederController app; }

// Arduino startup hook; initialize once after core setup.
void setup() { app.begin(); }

// Arduino scheduler hook; return after one service pass.
void loop() { app.tick(); }
