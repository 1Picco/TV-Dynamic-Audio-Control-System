// ============================================================================
// FEEDBACK_PATTERN — Non-blocking RGB LED feedback pattern
// ============================================================================
// Creates a smooth RGB LED effect used to visually indicate system status.
// The LED fades between configurable minimum and maximum RGB values:
//
//        FADE IN → HOLD ON → FADE OUT → HOLD OFF → REPEAT
//
// The function is non-blocking and uses millis(), so it must be called
// repeatedly from loop(). The fade speed, hold times, color range and number
// of cycles are configurable.
//
// min/max RGB:
//   Define the LED color at the start/end of each fade.
//   Values may be reversed to make a channel fade down instead of up.
//
// blinkCount:
//   0 = repeat continuously
//   >0 = perform that many complete fade cycles, then stop
//
// USE_CHARGE_LOGIC:
//   false = use the supplied RGB values and fade-out time
//   true  = automatically select color and fade speed according to battery
//           voltage, providing a visual charging/battery-status indication.
//
// FEEDBACK_LED_ACTIVE provides a global timeout/enable control and immediately
// switches the LED off when inactive.
//
// lerp8() performs the actual smooth RGB interpolation during the fades.
// ============================================================================


// Fast fixed-point interpolation for smooth RGB fading.
// Calculates the value between start and end based on elapsed/duration.
// Uses >> 8 for the final /256 scaling to keep the calculation efficient.
static inline byte lerp8(byte start, byte end, uint16_t elapsed, uint16_t duration) {
  if (duration == 0) return end;
  uint16_t progress = ((uint32_t)elapsed << 8) / duration; // 0..256 scale
  if (progress > 256) progress = 256;
  int16_t delta = (int16_t)end - (int16_t)start;
  return (byte)(start + ((delta * (int16_t)progress) >> 8));
}

void FEEDBACK_PATTERN(byte minRed,    byte maxRed,
                      byte minGreen, byte maxGreen,
                      byte minBlue,   byte maxBlue,
                      byte blinkCount,
                      uint16_t fadeInTime,   uint16_t onTime,
                      uint16_t fadeOutTime,  uint16_t offTime,
                      bool USE_CHARGE_LOGIC, uint16_t voltagePausedCharging_mV) {

  // Global timeout check: turn off LEDs and block execution if FEEDBACK_ON timer has expired
  if (!FEEDBACK_LED_ACTIVE) {
    led.setColor(0, 0, 0);
    return;
  }

  enum PatternState { PATTERN_OFF, PATTERN_FADE_IN, PATTERN_ON, PATTERN_FADE_OUT, PATTERN_DONE };

  static PatternState state = PATTERN_FADE_IN; // Start directly in Fade-In or Off based on setup
  static unsigned long stateStart = 0;
  static byte cycleCount = 0;

  static bool previousChargeLogic = false;
  static bool firstVoltageReading = true;
  static uint16_t lockedVoltage_mV = 0;

  // Track the current parameters to auto-reset state if pattern configuration changes
  static byte prevBlinkCount = 0;
  if (prevBlinkCount != blinkCount) {
    cycleCount = 0;
    state = PATTERN_FADE_IN;
    stateStart = millis();
    prevBlinkCount = blinkCount;
  }

  // If we finished all requested blinks, do nothing
  if (state == PATTERN_DONE) return;

  unsigned long now = millis();
  unsigned long elapsed = now - stateStart;

  // Charge logic initialization
  if (USE_CHARGE_LOGIC && !previousChargeLogic) {
    firstVoltageReading = true;
  }
  previousChargeLogic = USE_CHARGE_LOGIC;

  if (USE_CHARGE_LOGIC && firstVoltageReading) {
    lockedVoltage_mV = VOLTAGE;
    firstVoltageReading = false;
  }

  // Selected voltage (fixed 3000mV threshold)
  uint16_t chargingVoltage_mV = (USE_CHARGE_LOGIC && (voltagePausedCharging_mV > (3000 - VOLTAGE_EPSILON_MV)))
                                ? voltagePausedCharging_mV
                                : lockedVoltage_mV;

  // Active color values
  byte activeMinR = minRed,   activeMaxR = maxRed;
  byte activeMinG = minGreen, activeMaxG = maxGreen;
  byte activeMinB = minBlue,  activeMaxB = maxBlue;
  uint16_t activeFadeOutTime = fadeOutTime;

  if (USE_CHARGE_LOGIC) {
    if (chargingVoltage_mV >= (4120 - VOLTAGE_EPSILON_MV)) {
      activeMinR = 0;  activeMaxR = 15;
      activeMinG = 30; activeMaxG = 255;
      activeMinB = 0;  activeMaxB = 0;
      activeFadeOutTime = 2000;
    } else if (chargingVoltage_mV >= (4070 - VOLTAGE_EPSILON_MV)) {
      activeMinR = 40; activeMaxR = 255;
      activeMinG = 40; activeMaxG = 150;
      activeMinB = 0;  activeMaxB = 0;
      activeFadeOutTime = 1000;
    } else {
      activeMinR = 25; activeMaxR = 255;
      activeMinG = 0;  activeMaxG = 0;
      activeMinB = 0;  activeMaxB = 0;
      activeFadeOutTime = 550;
    }
  }

  // State machine execution
  switch (state) {

    case PATTERN_FADE_IN:
      if (fadeInTime == 0 || elapsed >= fadeInTime) {
        led.setColor(activeMaxR, activeMaxG, activeMaxB);
        state = PATTERN_ON;
        stateStart = now;
      } else {
        led.setColor(lerp8(activeMinR, activeMaxR, elapsed, fadeInTime),
                     lerp8(activeMinG, activeMaxG, elapsed, fadeInTime),
                     lerp8(activeMinB, activeMaxB, elapsed, fadeInTime));
      }
      break;

    case PATTERN_ON:
      if (elapsed >= onTime) {
        state = PATTERN_FADE_OUT;
        stateStart = now;
      }
      break;

    case PATTERN_FADE_OUT:
      if (activeFadeOutTime == 0 || elapsed >= activeFadeOutTime) {
        led.setColor(activeMinR, activeMinG, activeMinB);
        cycleCount++;

        // Check blink limit immediately after fade out completes
        if (blinkCount > 0 && cycleCount >= blinkCount) {
          state = PATTERN_DONE;
        } else {
          state = PATTERN_OFF;
          stateStart = now;
        }
      } else {
        led.setColor(lerp8(activeMaxR, activeMinR, elapsed, activeFadeOutTime),
                     lerp8(activeMaxG, activeMinG, elapsed, activeFadeOutTime),
                     lerp8(activeMaxB, activeMinB, elapsed, activeFadeOutTime));
      }
      break;

    case PATTERN_OFF:
      if (elapsed >= offTime) {
        state = PATTERN_FADE_IN;
        stateStart = now;
      }
      break;

    case PATTERN_DONE:
      break;
  }
}

// ============================================================================
// Function to provide LED feedback based on system state and battery voltage.
// Controls RGB LED colors and patterns for different system modes.
// ============================================================================

// Display RGB feedback based on battery voltage and operating mode.

void FEEDBACK_MODE(byte mode) {

  // Do not show battery feedback while charging, while charging is paused,
  // when remote control mode is inactive, when volume control is inactive,
  // or while the TV volume is being automatically restored.
  if (CHARGING_ACTIVE || CHARGING_PAUSED || !REMOTE_CONTROL_MODE_ACTIVE ||
      !VOLUME_CONTROL_ACTIVE || VOLUME_REDUCTION_COUNTER > 0) return;

  // Battery status feedback is only updated when the deadband counter
  // has expired, preventing unnecessary repeated LED updates.
  if (mode == BATTERY_VOLTAGE_STATUS && DEADBAND_COUNTER != 0) return;

  // Use the most recently measured battery voltage.
  uint16_t currentVoltage_mV = VOLTAGE;

  switch (mode) {

    // ------------------------------------------------------------------------
    // BLINK
    // Used for IR feedback.
    // Repeating IR commands show a steady battery-status color.
    // Non-repeating valid IR commands produce a short color flash.
    // ------------------------------------------------------------------------

    case BLINK:
      if (IR_CODE_REPEATING) {

        // Battery voltage >= 3.56 V: green
        // Battery voltage >= 3.50 V: yellow
        // Battery voltage <  3.50 V: red
        if (currentVoltage_mV >= 3560) led.setColor(0, 200, 20);
        else if (currentVoltage_mV >= 3500) led.setColor(200, 200, 0);
        else led.setColor(200, 20, 30);

      } else {

        // Valid non-repeating IR command: briefly flash the
        // color corresponding to the current battery voltage.
        if (currentVoltage_mV >= 3560) led.flash(0, 255, 0, 30, 0, 0);
        else if (currentVoltage_mV >= 3500) led.flash(150, 140, 0, 30, 0, 0);
        else led.flash(250, 0, 0, 30, 0, 0);
      }
      break;

    // ------------------------------------------------------------------------
    // BATTERY_VOLTAGE_STATUS
    // Display the current battery status using fade-in to color.
    // ------------------------------------------------------------------------

    case BATTERY_VOLTAGE_STATUS:

      // Battery voltage >= 3.56 V: green
      // Battery voltage >= 3.50 V: orange
      // Battery voltage <  3.50 V: red
      if (currentVoltage_mV >= 3560) led.fadeIn(0, 100, 10, 35, 165);
      else if (currentVoltage_mV >= 3500) led.fadeIn(100, 50, 0, 35, 165);
      else led.fadeIn(100, 10, 0, 35, 165);
      break;

    // ------------------------------------------------------------------------
    // RGB_LED_OFF
    // Fade out the current battery-status color.
    // The color used for the fade-out corresponds to the current voltage.
    // ------------------------------------------------------------------------

    case RGB_LED_OFF:

      // Battery voltage >= 3.56 V: green
      // Battery voltage >= 3.50 V: orange
      // Battery voltage <  3.50 V: red
      if (currentVoltage_mV >= 3560) led.fadeOut(0, 100, 10, 30, 1000);
      else if (currentVoltage_mV >= 3500) led.fadeOut(100, 50, 0, 30, 1000);
      else led.fadeOut(100, 10, 0, 30, 1000);
      break;
  }
}
// ============================================================================
// DEADBAND_FEEDBACK — shows battery charge level as an LED color: green for
// high voltage, orange for mid voltage, red for low voltage.
// Within each level, the LED fades to cyan/teal if DEADBAND_COUNTER is positive, or to
// purple if it's negative.
// ============================================================================
// Wrapper macro/function for standardized 5.75s LED fade pulse sequence.
// ============================================================================
static inline void FADE(
  byte minRed,   byte maxRed,
  byte minGreen, byte maxGreen,
  byte minBlue,  byte maxBlue
) {
  FEEDBACK_PATTERN(
    minRed,   maxRed,
    minGreen, maxGreen,
    minBlue,  maxBlue,
    0,                                           // blinkCount
    //Fade IN  Stay ON   Fade OFF   Stay OFF
    MS(300),   MS(500),   MS(300),  SECONDS(2),
    false, 0                                     // Charge logic disabled
  );
}

// ============================================================================
// Triggers LED color feedback based on battery voltage and deadband polarity.
//
// Early Exit:
// - Returns if VOLUME_REDUCTION_COUNTER != 0 or DEADBAND_COUNTER == 0.
//
// Pattern Mapping:
// - Positive Deadband ( > 0 ) : Uses max values for Green (75) and Blue (250).
// - Negative Deadband ( < 0 ) : Uses max values for Red (75) and Blue (150).
// - High Volts ( >= 3560 mV ) : Green-dominant (Cyan tint).
// - Med Volts  ( >= 3500 mV ) : Red + Green (Yellow tint).
// - Low Volts  ( < 3500 mV )  : Red-dominant.
// ============================================================================
void DEADBAND_FEEDBACK() {

  if (!VOLUME_CONTROL_ACTIVE        || CHARGING_ACTIVE ||
      VOLUME_REDUCTION_COUNTER != 0 || DEADBAND_COUNTER == 0) return;

  uint16_t voltage = VOLTAGE;

  /*                                    -- RED -- - GREEN - -- BLUE --    */
  if (voltage >= 3560) {            //  min  max  min  max  min  max
    if      (DEADBAND_COUNTER > 0) FADE(  0,   0, 100,  75,  5, 250);
    else if (DEADBAND_COUNTER < 0) FADE(  0,  75, 100,   0,  5, 150);
  }
  else if (voltage >= 3500) {
    if      (DEADBAND_COUNTER > 0) FADE(100,   0,  50,  75,   0, 250);
    else if (DEADBAND_COUNTER < 0) FADE(100,  75,  50,   0,   0, 150);
  }
  else {
    if      (DEADBAND_COUNTER > 0) FADE(100,   0,  10,  75,   0, 250);
    else if (DEADBAND_COUNTER < 0) FADE(100,  75,  10,   0,   0, 150);
  }

}

// ============================================================================
// Function to control the delay before returning to battery-status feedback.
// After the specified time following an IR command, restore the normal
// battery-voltage LED feedback.
// ============================================================================

void FEEDBACK_OFF(uint32_t MINUTES = 0, uint32_t SECONDS = 0) {
  uint32_t FEEDBACK_LED_OFF_TIME = (MINUTES + SECONDS);

  if (FEEDBACK_OFF_ACTIVE) {

    // Calculate the elapsed time since the feedback-off timer was started.
    FEEDBACK_OFF_AFTER_IR_DETECTED_TIME = millis() - FEEDBACK_OFF_START_TIME;

    // When the delay has elapsed, stop the timer and restore
    // the normal battery-status LED feedback.
    if (FEEDBACK_OFF_AFTER_IR_DETECTED_TIME >= FEEDBACK_LED_OFF_TIME) {
      FEEDBACK_OFF_ACTIVE = false;
      FEEDBACK_MODE(BATTERY_VOLTAGE_STATUS);
    }
  }
}

// ============================================================================
// Function to control the duration of temporary LED feedback.
// After the specified time, turn off the current feedback indication.
// ============================================================================

void FEEDBACK_ON(uint32_t MINUTES = 0, uint32_t SECONDS = 0) {
  uint32_t FEEDBACK_LED_ON_TIME = (MINUTES + SECONDS);

  if (FEEDBACK_LED_ACTIVE) {

    // Calculate the elapsed time since the feedback-on timer was started.
    RGB_FEEDBACK_ACTIVE_TIME = millis() - RGB_FEEDBACK_START_TIME;

    // When the feedback duration has elapsed, stop the timer and
    // fade out the current LED feedback.
    if (RGB_FEEDBACK_ACTIVE_TIME >= FEEDBACK_LED_ON_TIME) {
      FEEDBACK_LED_ACTIVE = false;
      FEEDBACK_MODE(RGB_LED_OFF);
    }
  }
}
