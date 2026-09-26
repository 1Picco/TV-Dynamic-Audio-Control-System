// ============================================================================
// SLEEP MANAGEMENT FUNCTIONS
// ============================================================================

// Interrupt handler used to wake the Arduino from sleep mode.
// The interrupt only sets the wake-up flag; the actual wake-up processing
// is performed after sleep_mode() returns.
void TIME_TO_WAKE_UP() {
  WAKE_UP_TRIGGERED = true;  // Set flag to indicate wake-up was triggered
}

// ============================================================================
// Function to put Arduino into deep sleep mode for power saving
// Implements sleep/wake cycle with IR wake-up detection and proper system restoration

void GO_TO_SLEEP(bool SLEEP_NOW = false, uint32_t HOURS = 0, uint32_t MINUTES = 0, uint32_t SECONDS = 0) {
  uint32_t timeToSleep = (HOURS + MINUTES + SECONDS);

  // Enter sleep immediately when requested, or when the configured waiting
  // period has elapsed.
  if (SLEEP_NOW || (ARDUINO_WAITING_TO_SLEEP && (millis() - GO_TO_SLEEP_START_TIME >= timeToSleep))) {
    ARDUINO_WAITING_TO_SLEEP = false;

    // Flash white indicator before shutting down.
    // Turn the LED off afterwards to minimize current consumption during sleep.
    led.flash(220, 220, 220, 30, 85, 3);
    led.off();

    // Main sleep/wake cycle.
    // If no valid wake-up command is received, the Arduino can return to sleep.
    do {
      WAKE_UP_TRIGGERED = false;
      STAY_AWAKE = false;

      // Disable peripherals to maximize power savings during sleep.
      power_adc_disable();
      power_usart0_disable();
      power_spi_disable();
      power_timer0_disable();
      power_timer1_disable();
      power_twi_disable();

      // Configure the ATmega328P for its lowest-power sleep mode.
      set_sleep_mode(SLEEP_MODE_PWR_DOWN);

      cli();
      sleep_enable();       // Enable sleep hardware before entering sleep
      sleep_bod_disable();  // Disable brown-out detection during sleep

      // Configure the IR receiver interrupt as the wake-up source.
      // A falling edge from the IR receiver wakes the Arduino.
      detachInterrupt(digitalPinToInterrupt(PIN_IR_RECEIVER));
      attachInterrupt(digitalPinToInterrupt(PIN_IR_RECEIVER), TIME_TO_WAKE_UP, FALLING);

      sei();                // Re-enable interrupts
      sleep_enable();       // Enable sleep hardware
      sleep_mode();         // Enter sleep; execution resumes here after wake-up

      // --- WAKE UP EXECUTION RESUMES HERE ---

      // Disable sleep mode and remove the wake-up interrupt.
      sleep_disable();
      detachInterrupt(digitalPinToInterrupt(PIN_IR_RECEIVER));

      // Re-enable the timers required for millis() and IR processing.
      // ADC remains disabled until the Arduino leaves the sleep cycle.
      //power_adc_enable();
      power_timer0_enable();
      power_timer1_enable();

      // Small stabilization delay for the clock oscillator.
      //delayMicroseconds(500);

      // Initialize the IR receiver and transmitter so the incoming
      // IR command can be decoded and, when required, forwarded to the TV.
      irrecv.enableIRIn();
      IrSender.begin(PIN_IR_LED, DISABLE_LED_FEEDBACK);

      unsigned long WAIT_BEFORE_RECEIVE_IR = millis();
      bool IR_CODE_RECEIVED = false;

      // After waking, listen for an IR command for up to
      // VOLUME_COMMAND_INTERVAL.
      while (millis() - WAIT_BEFORE_RECEIVE_IR < VOLUME_COMMAND_INTERVAL) {

        if (irrecv.decode(&results)) {
          uint8_t code = results.value;
          uint8_t sendCode = 0;

          // Only these commands are processed during the wake-up window.
          // After processing one, the Arduino stays awake in normal operating mode
          // so further remote actions can be received and loud audio detection is active again.
          // Other IR codes do not keep the Arduino awake.
          switch (code) {

            // Power: controls the TV power state.
            // The normal power handling also puts the Arduino to sleep or
            // wakes it, so the Power button must be recognized from sleep.
            case 0x0C: sendCode = 0x02; break; // Power

            // OK: allows confirmation of a TV on-screen event while the Arduino is asleep.
            case 0x5C: sendCode = 0x68; break; // OK

            // Volume Down: allows TV volume adjustment while asleep.
            case 0x11: sendCode = 0x0B; break; // Vol -

            // Volume Up: allows TV volume adjustment while asleep.
            case 0x10: sendCode = 0x07; break; // Vol +

            default: break;
          }

          // A recognized command is forwarded to the Samsung TV and
          // keeps the Arduino awake for normal operation.
          if (sendCode != 0) {
            sendSamsungCode(sendCode);
            STAY_AWAKE = true;
            IR_CODE_RECEIVED = true;
          }

          // Prepare the receiver for the next IR signal.
          irrecv.resume();

          // Stop listening once a valid wake-up command has been processed.
          if (STAY_AWAKE) {
            break;
          }
        }
      }

      // No recognized IR command was received during the wake-up window.
      // Give red feedback before returning to sleep.
      if (!IR_CODE_RECEIVED) {
        led.flash(250, 0, 0, 50, 80, 2);
        led.off();
      }

      // Return to sleep unless a valid command requested normal operation.
    } while (!STAY_AWAKE);

    // Re-enable all peripherals required for normal active-mode operation.
    power_adc_enable();
    power_usart0_enable();
    power_spi_enable();
    power_timer0_enable();
    power_timer1_enable();
    power_twi_enable();

    // Reinitialize the IR receiver after leaving sleep mode.
    irrecv.enableIRIn();

    // Cancel any pending temporary IR feedback timeout.
    FEEDBACK_OFF_ACTIVE = false;

    // Request a fresh battery-voltage reading after waking.
    REQUEST_BATTERY_VOLTAGE_READING();

    // White flash indicates that normal active operation has resumed.
    led.flash(220, 220, 220, 500, 5, 1);
  }
}
