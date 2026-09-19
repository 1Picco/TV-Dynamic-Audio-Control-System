// ============================================================================
// BATTERY MANAGEMENT FUNCTIONS
// ============================================================================


// Measure battery voltage and return the result in millivolts (mV)
uint16_t GET_BATTERY_VOLTAGE_mV() {
  uint32_t raw_mV = ((uint32_t)analogRead(PIN_BATTERY_VOLTAGE) * 3360UL) / 1023UL;
  return (uint16_t)((raw_mV * 13260UL) / 10000UL);
}

// ============================================================================

// Function to read and calculate battery voltage in millivolts
// Reads analog voltage, applies voltage divider calculation, and updates global VOLTAGE variable (mV)

uint16_t READ_BATTERY_VOLTAGE() {

  if (CHARGING_ACTIVE) return;

  unsigned long CURRENT_MILLIS = millis();

  if (!LOAD_TEST_ACTIVE &&
      (FIRST_VOLTAGE_READING_TAKEN ||
       (CURRENT_MILLIS - PREVIOUS_VOLTAGE_READING >= TIME_TO_READ_BATTERY_VOLTAGE))) {

    LOAD_TEST_ACTIVE           = true;
    BATTERY_MEASUREMENT_ACTIVE = true;
    VOLTAGE_CAPTURED           = false;

    LOAD_TEST_TIMER            = CURRENT_MILLIS;

    digitalWrite(PIN_BATTERY_LOAD, HIGH);
    // MY_TONE(3000, 15, 1);
    // led.fadeIn(220, 220, 220, 15, 150);
  }

  // Read voltage after 2.9 seconds
  //===========================================================================

  if (LOAD_TEST_ACTIVE && !VOLTAGE_CAPTURED &&
      (CURRENT_MILLIS - LOAD_TEST_TIMER >= 2900UL)) {

    VOLTAGE_CAPTURED = true;

    // Integer-only: ADC→mV at divider output, then apply (R1+R2)/R2 = 13260/10000
    VOLTAGE = GET_BATTERY_VOLTAGE_mV();

    if (!CHARGING_ACTIVE) {

      Serial.print(F("Battery state: "));
      Serial.print(VOLTAGE / 1000);                          // whole number
      Serial.print('.');
      Serial.print((VOLTAGE % 1000) / 100);                   // tenths
      Serial.print((VOLTAGE % 100) / 10);                     // hundredths
      Serial.println(F("V"));

    }
  }

  // Finish measurement after 3 seconds
  //===========================================================================

  if (LOAD_TEST_ACTIVE &&
      (CURRENT_MILLIS - LOAD_TEST_TIMER >= 3000UL)) {

    digitalWrite(PIN_BATTERY_LOAD, LOW);
    // led.fadeOut(220, 220, 220, 15, 150);

    LOAD_TEST_ACTIVE            = false;
    BATTERY_MEASUREMENT_ACTIVE  = false;

    if (!CHARGING_ACTIVE && FIRST_VOLTAGE_READING_TAKEN) {
      FEEDBACK_LED_ACTIVE = true;
      FEEDBACK_MODE(BATTERY_VOLTAGE_STATUS);
    }

    FIRST_VOLTAGE_READING_TAKEN = false;
    PREVIOUS_VOLTAGE_READING = CURRENT_MILLIS;
  }
  return VOLTAGE;
}

// ============================================================================
// Function to manage battery charging cycle
// Monitors battery VOLTAGE and controls charging process with stabilization pauses

void BATTERY_CHARGE_CHECK() {
  unsigned long CURRENT_MILLIS = millis();

  VOLTAGE = READ_BATTERY_VOLTAGE();

  // Wait until the battery measurement is finished.
  if (BATTERY_MEASUREMENT_ACTIVE) {
    return;
  }

  // If battery voltage drops below 3450mV (3.45V) and charging is complete, start charging
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

  // Periodically check if we need to pause charging and measure the battery voltage
  if (CURRENT_MILLIS - LAST_TIME_VOLTAGE_READING >= TIME_TO_CHECK_BATTERY_CHARGE_STATE && !CHARGING_PAUSED && LOW_VOLTAGE_START_CHARGING) {
    digitalWrite(PIN_CHARGE_CONTROL, LOW);            // Stop charging temporarily to measure battery voltage
    //digitalWrite(PIN_BATTERY_LOAD, HIGH);             // Apply load to battery for accurate voltage measurement
    //MY_TONE(3500, 15, 2);
    MY_TONE(3250, +250, 20, 2);

    // Feedback to indicate charging pause
    led.fadeIn(220, 220, 220, 15, 150);

    info.println(F("Charging PAUSED and load applied."));
    CHARGING_PAUSED = true;                           // Set pause flag
    PAUSE_CHARGING_TO_READ_VOLTAGE = CURRENT_MILLIS;  // Record pause start time
  }

  // If charging was paused, wait for stabilization time to pass before measuring VOLTAGE again
  if (CHARGING_PAUSED && CURRENT_MILLIS - PAUSE_CHARGING_TO_READ_VOLTAGE >= WAITE_FOR_VOLTAGE_STABILIZATION) {

    // Measure the battery voltage again after stabilization (integer-only, result in mV)
    VOLTAGE_DURING_PAUSED_CHARGING = GET_BATTERY_VOLTAGE_mV();

    info.print(F("Stabilized battery voltage under load: "));
    //info.print(VOLTAGE_DURING_PAUSED_CHARGING / 1000.0, 2);
    info.print(VOLTAGE_DURING_PAUSED_CHARGING / 1000);
    info.print('.');
    info.print((VOLTAGE_DURING_PAUSED_CHARGING % 1000) / 100);
    info.print((VOLTAGE_DURING_PAUSED_CHARGING % 100) / 10);
    info.println(F("V"));

    // Check if voltage has reached the maximum charge level
    if (VOLTAGE_DURING_PAUSED_CHARGING >= STOP_CHARGING_VOLTAGE) {
      digitalWrite(PIN_CHARGE_CONTROL, LOW);         // Turn off charging when fully charged
      //digitalWrite(PIN_BATTERY_LOAD, LOW);           // Remove load from battery
      eepromWearLevelWrite(EEPROM_CHARGING_ACTIVE_BASE, false);
      info.println(F("Charging complete."));
      //MY_TONE(3500, 15, 2);
      MY_TONE(3750, -250, 20, 3);
      CHARGING_ACTIVE = false;                       // Clear charging state
      CHARGING_CYCLE_COMPLETE = true;
      LOW_VOLTAGE_START_CHARGING = false;
    } else if (VOLTAGE_DURING_PAUSED_CHARGING < STOP_CHARGING_VOLTAGE && LOW_VOLTAGE_START_CHARGING) {
      //digitalWrite(PIN_BATTERY_LOAD, LOW);           // Remove load before resuming charging
      digitalWrite(PIN_CHARGE_CONTROL, HIGH);        // Resume charging if still below max voltage
      info.println(F("Resume charging."));
      led.fadeOut(220, 220, 220, 15, 150);
      MY_TONE(3750, -250, 20, 2);
    }
    CHARGING_PAUSED = false;                         // Reset the charging pause flag
    //digitalWrite(PIN_BATTERY_LOAD, LOW);             // Ensure load is off if not already
    LAST_TIME_VOLTAGE_READING = CURRENT_MILLIS;      // Update the last CHARGE_CONTROL check time
  }
}

// ============================================================================
// Function to display battery VOLTAGE using RGB LED patterns
// Separates VOLTAGE into digits and shows them with LED blinks

void VERIFY_BATTERY_VOLTAGE() {

  if (CHARGING_ACTIVE) {
    VOLTAGE = VOLTAGE_DURING_PAUSED_CHARGING;  // Use charging VOLTAGE when battery is charging
  } else {
    VOLTAGE = READ_BATTERY_VOLTAGE();   // Use normal VOLTAGE reading when not charging
  }

  // Extract digits from voltage in mV (e.g., 3450 -> whole=3, tenths=4, hundredths=5)
  uint16_t   millivolt    = VOLTAGE   / 10;
  const byte wholeNumber = millivolt  / 100;
  const byte tenths      = (millivolt / 10) % 10;
  const byte hundredths  = millivolt  % 10;

  // Display VOLTAGE using RGB LEDs
  BLINK_BATTERY_VOLTAGE(wholeNumber, tenths, hundredths);  // Show VOLTAGE with LED patterns
}

// ============================================================================
// Blink a voltage digit using the specified RGB color, running the pattern to completion.

void BLINK_DIGIT(byte count, byte red, byte green, byte blue, uint16_t pauseAfter) {
  if (count == 0) {
    if (pauseAfter > 0) delay(pauseAfter);
    return;
  }

  // Define pattern timing for a single digit blink
  const uint16_t fadeIn  = 200;
  const uint16_t onTime  = 200;
  const uint16_t fadeOut = 500;
  const uint16_t offTime = 200;

  // Total duration of a single blink cycle in ms
  unsigned long singleCycleDuration = (unsigned long)fadeIn + onTime + fadeOut + offTime;
  unsigned long totalAnimationTime  = singleCycleDuration * count;

  unsigned long startTime = millis();

  // Run state machine continuously until all blinks complete
  while (millis() - startTime < totalAnimationTime) {
    FEEDBACK_PATTERN(0, red, 0, green, 0, blue, count, fadeIn, onTime, fadeOut, offTime, false, 0);
    delay(5); // Small yield to avoid spinning too fast on hardware loops
  }

  // Ensure LED is turned off cleanly at the end of the digit
  led.setColor(0, 0, 0);

  if (pauseAfter > 0) {
    delay(pauseAfter);
  }
}

// ============================================================================
// Function to display VOLTAGE digits using RGB LED patterns
// Shows VOLTAGE as a sequence of LED blinks: green (whole), yellow (tenths), red (hundredths)

void BLINK_BATTERY_VOLTAGE(byte wholeNumber, byte tenths, byte hundredths) {

  // Start indication pattern (White sequence)
  unsigned long startSeqDuration = 2000;
  unsigned long startSeqBegin = millis();
  while (millis() - startSeqBegin < startSeqDuration) {
    FEEDBACK_PATTERN(0, 220, 0, 220, 0, 220, 1, 200, 400, 200, 1500, false, 0);
  }

  // Whole volts (green)
  BLINK_DIGIT(wholeNumber, 0, 250, 0, SECONDS(1));

  // Tenths (yellow)
  BLINK_DIGIT(tenths, 255, 250, 0, SECONDS(1));

  // Hundredths (red)
  BLINK_DIGIT(hundredths, 255, 0, 0, SECONDS(1));

  // End indication pattern (White sequence)
  led.flash(220, 220, 220, 30, 50, 2);

  // Reset LED output and call system feedback timer resets
  led.setColor(0, 0, 0);
  RESET_FEEDBACK_TIMERS();
}
