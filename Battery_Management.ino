// ============================================================================
// BATTERY MANAGEMENT FUNCTIONS
// ============================================================================

// Measure battery voltage and return the result in millivolts (mV).
// Reads the ADC voltage at the divider output and applies the voltage
// divider ratio to calculate the actual battery voltage.
uint16_t GET_BATTERY_VOLTAGE_mV() {
  uint32_t raw_mV = ((uint32_t)analogRead(PIN_BATTERY_VOLTAGE) * 3360UL) / 1023UL;
  return (uint16_t)((raw_mV * 13260UL) / 10000UL);
}

// ============================================================================

// Read the battery voltage under load and update the global VOLTAGE value.
// A 100 mA load is applied during the measurement to obtain the battery
// voltage under a consistent load condition.

uint16_t READ_BATTERY_VOLTAGE() {

  // Do not perform a battery measurement while charging.
  if (CHARGING_ACTIVE) return;

  unsigned long CURRENT_MILLIS = millis();

  // Start a new battery measurement when the required interval has elapsed.
  // The first measurement is performed immediately.
  if (!LOAD_TEST_ACTIVE &&
      (FIRST_VOLTAGE_READING_TAKEN ||
       (CURRENT_MILLIS - PREVIOUS_VOLTAGE_READING >= TIME_TO_READ_BATTERY_VOLTAGE))) {

    LOAD_TEST_ACTIVE           = true;
    BATTERY_MEASUREMENT_ACTIVE = true;
    VOLTAGE_CAPTURED           = false;

    LOAD_TEST_TIMER            = CURRENT_MILLIS;

    // Apply the 100 mA battery load and allow the voltage to settle.
    digitalWrite(PIN_BATTERY_LOAD, HIGH);
    // MY_TONE(3000, 15, 1);
    // led.fadeIn(220, 220, 220, 15, 150);
  }

  // Read the battery voltage after 2.9 seconds under load.
  // ===========================================================================

  if (LOAD_TEST_ACTIVE && !VOLTAGE_CAPTURED &&
      (CURRENT_MILLIS - LOAD_TEST_TIMER >= 2900UL)) {

    VOLTAGE_CAPTURED = true;

    // Read the loaded battery voltage and store the result in mV.
    // The calculation is integer-only: ADC → mV at divider output,
    // then apply the voltage divider ratio (R1 + R2) / R2 = 13260 / 10000.
    VOLTAGE = GET_BATTERY_VOLTAGE_mV();

    if (!CHARGING_ACTIVE) {

      Serial.print(F("Battery state: "));
      Serial.print(VOLTAGE / 1000);                          // whole number
      Serial.print('.');
      Serial.print((VOLTAGE % 1000) / 100);                  // tenths
      Serial.print((VOLTAGE % 100) / 10);                    // hundredths
      Serial.println(F("V"));

    }
  }

  // Finish the measurement after 3 seconds.
  // The load remains active for an additional 100 ms after the voltage
  // capture, then it is removed.
  // ===========================================================================

  if (LOAD_TEST_ACTIVE &&
      (CURRENT_MILLIS - LOAD_TEST_TIMER >= 3000UL)) {

    // Remove the battery load.
    digitalWrite(PIN_BATTERY_LOAD, LOW);
    // led.fadeOut(220, 220, 220, 15, 150);

    LOAD_TEST_ACTIVE            = false;
    BATTERY_MEASUREMENT_ACTIVE  = false;

    // After the initial reading, update the LED feedback using the
    // newly measured battery voltage status.
    if (!CHARGING_ACTIVE && FIRST_VOLTAGE_READING_TAKEN) {
      FEEDBACK_LED_ACTIVE = true;
      FEEDBACK_MODE(BATTERY_VOLTAGE_STATUS);
    }

    // Mark this measurement as completed and start the interval timer
    // for the next battery voltage measurement.
    FIRST_VOLTAGE_READING_TAKEN = false;
    PREVIOUS_VOLTAGE_READING = CURRENT_MILLIS;
  }

  return VOLTAGE;
}

// ============================================================================
// BATTERY CHARGING MANAGEMENT
// Monitors battery voltage and controls the charging cycle.
// Charging is periodically paused so the battery voltage can be measured
// without the charger affecting the reading.
// ============================================================================

void BATTERY_CHARGE_CHECK() {
  unsigned long CURRENT_MILLIS = millis();

  // Update the battery voltage when a scheduled measurement is available.
  VOLTAGE = READ_BATTERY_VOLTAGE();

  // Wait until an active battery measurement has finished.
  if (BATTERY_MEASUREMENT_ACTIVE) {
    return;
  }

  // Start charging when the battery has dropped to the configured
  // start voltage and the previous charging cycle is complete.
  if (CHARGING_CYCLE_COMPLETE) {
    if (VOLTAGE <= START_CHARGING_VOLTAGE && !LOW_VOLTAGE_START_CHARGING) {
      digitalWrite(PIN_CHARGE_CONTROL, HIGH);
      eepromWearLevelWrite(EEPROM_CHARGING_ACTIVE_BASE, true);
      info.println(F("Low voltage! Charging started."));

      LOW_VOLTAGE_START_CHARGING = true;
      CHARGING_CYCLE_COMPLETE = false;
      CHARGING_ACTIVE = true;
    }
  }

  // Periodically pause charging so the battery voltage can be checked.
  // The charger is disabled before measuring to avoid measuring the
  // charging voltage instead of the actual battery voltage.
  if (CURRENT_MILLIS - LAST_TIME_VOLTAGE_READING >= TIME_TO_CHECK_BATTERY_CHARGE_STATE &&
      !CHARGING_PAUSED && LOW_VOLTAGE_START_CHARGING) {

    digitalWrite(PIN_CHARGE_CONTROL, LOW);

    //digitalWrite(PIN_BATTERY_LOAD, HIGH);
    //MY_TONE(3500, 15, 2);
    MY_TONE(3250, +250, 20, 2);

    // White fade indicates that charging is temporarily paused
    // for a battery voltage measurement.
    led.fadeIn(220, 220, 220, 15, 150);

    info.println(F("Charging PAUSED and load applied."));

    CHARGING_PAUSED = true;
    PAUSE_CHARGING_TO_READ_VOLTAGE = CURRENT_MILLIS;
  }

  // After charging has been paused long enough for the battery voltage
  // to stabilize, measure the battery voltage again.
  if (CHARGING_PAUSED &&
      CURRENT_MILLIS - PAUSE_CHARGING_TO_READ_VOLTAGE >= WAITE_FOR_VOLTAGE_STABILIZATION) {

    VOLTAGE_DURING_PAUSED_CHARGING = GET_BATTERY_VOLTAGE_mV();

    info.print(F("Stabilized battery voltage under load: "));
    info.print(VOLTAGE_DURING_PAUSED_CHARGING / 1000);
    info.print('.');
    info.print((VOLTAGE_DURING_PAUSED_CHARGING % 1000) / 100);
    info.print((VOLTAGE_DURING_PAUSED_CHARGING % 100) / 10);
    info.println(F("V"));

    // If the stabilized battery voltage has reached the configured
    // charging limit, finish the charging cycle.
    if (VOLTAGE_DURING_PAUSED_CHARGING >= STOP_CHARGING_VOLTAGE) {

      digitalWrite(PIN_CHARGE_CONTROL, LOW);
      eepromWearLevelWrite(EEPROM_CHARGING_ACTIVE_BASE, false);
      info.println(F("Charging complete."));

      //MY_TONE(3500, 15, 2);
      MY_TONE(3750, -250, 20, 3);

      CHARGING_ACTIVE = false;
      CHARGING_CYCLE_COMPLETE = true;
      LOW_VOLTAGE_START_CHARGING = false;

      // If the battery has not reached the charging limit, resume charging.
    } else if (VOLTAGE_DURING_PAUSED_CHARGING < STOP_CHARGING_VOLTAGE &&
               LOW_VOLTAGE_START_CHARGING) {

      digitalWrite(PIN_CHARGE_CONTROL, HIGH);
      info.println(F("Resume charging."));

      led.fadeOut(220, 220, 220, 15, 150);
      MY_TONE(3750, -250, 20, 2);
    }

    // Finish the charging pause and start the timer for the next
    // periodic charging-state check.
    CHARGING_PAUSED = false;
    LAST_TIME_VOLTAGE_READING = CURRENT_MILLIS;
  }
}
// ============================================================================
// Function to display battery VOLTAGE using RGB LED patterns.
// Uses the appropriate voltage value depending on whether the battery
// is currently charging, then displays the voltage digit by digit.
// ============================================================================

void VERIFY_BATTERY_VOLTAGE() {

  // While charging, use the stabilized voltage measured during the
  // charging pause. Otherwise, perform a normal battery voltage reading.
  if (CHARGING_ACTIVE) {
    VOLTAGE = VOLTAGE_DURING_PAUSED_CHARGING;
  } else {
    VOLTAGE = READ_BATTERY_VOLTAGE();
  }

  // Convert the voltage from mV to hundredths of a volt.
  // Example: 3450 mV -> 345 -> 3.45 V
  uint16_t   millivolt    = VOLTAGE   / 10;
  const byte wholeNumber = millivolt  / 100;
  const byte tenths      = (millivolt / 10) % 10;
  const byte hundredths  = millivolt  % 10;

  // Display the three voltage digits using the corresponding LED colors.
  BLINK_BATTERY_VOLTAGE(wholeNumber, tenths, hundredths);
}

// ============================================================================
// Blink one voltage digit using the specified RGB color.
// The complete blink pattern is finished before the function returns.
// ============================================================================

void BLINK_DIGIT(byte count, byte red, byte green, byte blue, uint16_t pauseAfter) {

  // A zero digit is represented by no blinks, followed by the requested pause.
  if (count == 0) {
    if (pauseAfter > 0) delay(pauseAfter);
    return;
  }

  // Timing of one complete blink cycle.
  const uint16_t fadeIn  = 200;
  const uint16_t onTime  = 200;
  const uint16_t fadeOut = 500;
  const uint16_t offTime = 200;

  // Calculate the total time required for all blinks.
  unsigned long singleCycleDuration = (unsigned long)fadeIn + onTime + fadeOut + offTime;
  unsigned long totalAnimationTime  = singleCycleDuration * count;

  unsigned long startTime = millis();

  // Repeat the blink pattern until the requested number of blinks is complete.
  while (millis() - startTime < totalAnimationTime) {
    FEEDBACK_PATTERN(0, red, 0, green, 0, blue, count,
                     fadeIn, onTime, fadeOut, offTime, false, 0);
    delay(5);
  }

  // Ensure the LED is off before continuing to the next digit.
  led.setColor(0, 0, 0);

  // Pause between voltage digits when requested.
  if (pauseAfter > 0) {
    delay(pauseAfter);
  }
}

// ============================================================================
// Display the battery VOLTAGE as a sequence of colored LED blinks.
//
// Whole volts     = green
// Tenths          = yellow
// Hundredths      = red
//
// A white sequence marks the beginning and end of the voltage display.
// ============================================================================

void BLINK_BATTERY_VOLTAGE(byte wholeNumber, byte tenths, byte hundredths) {

  // Start indication: white LED sequence for 2 seconds.
  unsigned long startSeqDuration = 2000;
  unsigned long startSeqBegin = millis();

  while (millis() - startSeqBegin < startSeqDuration) {
    FEEDBACK_PATTERN(0, 220, 0, 220, 0, 220,
                     1, 200, 400, 200, 1500, false, 0);
  }

  // Whole volts: green.
  BLINK_DIGIT(wholeNumber, 0, 250, 0, SECONDS(1));

  // Tenths: yellow.
  BLINK_DIGIT(tenths, 255, 250, 0, SECONDS(1));

  // Hundredths: red.
  BLINK_DIGIT(hundredths, 255, 0, 0, SECONDS(1));

  // End indication: two short white flashes.
  led.flash(220, 220, 220, 30, 50, 2);

  // Turn the LED off and reset the feedback timers after the voltage
  // display sequence has completed.
  led.setColor(0, 0, 0);
  RESET_FEEDBACK_TIMERS();
}
