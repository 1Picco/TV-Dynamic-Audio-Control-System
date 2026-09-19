// ============================================================================
// SLEEP MANAGEMENT FUNCTIONS
// ============================================================================
// The interrupt handler for wake up from sleep mode
// Called when IR receiver detects a signal during sleep

void TIME_TO_WAKE_UP() {
  WAKE_UP_TRIGGERED = true;  // Set flag to indicate wake-up was triggered
}

// ============================================================================// Function to put Arduino into deep sleep mode for power saving
// Implements sleep/wake cycle with IR wake-up detection and proper system restoration

void GO_TO_SLEEP(bool SLEEP_NOW = false, uint32_t HOURS = 0, uint32_t MINUTES = 0, uint32_t SECONDS = 0) {
  uint32_t timeToSleep = (HOURS + MINUTES + SECONDS);

  if (SLEEP_NOW || (ARDUINO_WAITING_TO_SLEEP && (millis() - GO_TO_SLEEP_START_TIME >= timeToSleep))) {
    ARDUINO_WAITING_TO_SLEEP = false;

    // Flash white indicator before shutting down
    led.flash(220, 220, 220, 30, 85, 3);
    led.off(); // Turn off LED to prevent current leakage in sleep

    // Main sleep/wake cycle
    do {
      WAKE_UP_TRIGGERED = false;
      STAY_AWAKE = false;

      // Disable peripherals to maximize power savings
      power_adc_disable();
      power_usart0_disable();
      power_spi_disable();
      power_timer0_disable();
      power_timer1_disable();
      power_twi_disable();

      // Configure low-power sleep mode
      set_sleep_mode(SLEEP_MODE_PWR_DOWN);

      cli();
      sleep_enable();       // Enable sleep hardware first
      sleep_bod_disable();  // Disable BOD (timed 4-clock cycle window)

      // Configure wake up interrupt before sleeping
      detachInterrupt(digitalPinToInterrupt(PIN_IR_RECEIVER));
      attachInterrupt(digitalPinToInterrupt(PIN_IR_RECEIVER), TIME_TO_WAKE_UP, FALLING);

      sei();                // Re-enable interrupts
      sleep_enable();       // Enable sleep hardware first
      sleep_mode();         // Sleep immediately on the next clock cycle

      // --- WAKE UP EXECUTION RESUMES HERE ---
      sleep_disable();
      detachInterrupt(digitalPinToInterrupt(PIN_IR_RECEIVER));

      // Re-enable minimal clock timers for millis() and IR decoding
      //power_adc_enable();
      power_timer0_enable();
      power_timer1_enable();

      // Small stabilization delay for clock oscillator
      //delayMicroseconds(500);

      // Initialize IR peripheral to process the incoming pulse stream
      irrecv.enableIRIn();
      IrSender.begin(PIN_IR_LED, DISABLE_LED_FEEDBACK);

      unsigned long WAIT_BEFORE_RECEIVE_IR = millis();
      bool IR_CODE_RECEIVED = false;

      // Listen for up to VOLUME_COMMAND_INTERVAL for a valid code
      while (millis() - WAIT_BEFORE_RECEIVE_IR < VOLUME_COMMAND_INTERVAL) {

        if (irrecv.decode(&results)) {
          uint8_t code = results.value;
          uint8_t sendCode = 0;

          switch (code) {
            case 0x0C: sendCode = 0x02; break; // Power
            case 0x5C: sendCode = 0x68; break; // OK
            case 0x11: sendCode = 0x0B; break; // Vol -
            case 0x10: sendCode = 0x07; break; // Vol +
            default: break;
          }

          if (sendCode != 0) {
            sendSamsungCode(sendCode);
            STAY_AWAKE = true;
            IR_CODE_RECEIVED = true;
          }

          irrecv.resume();
          if (STAY_AWAKE) {
            break;
          }
        }
      }

      // Flash red feedback if false alarm / invalid code
      if (!IR_CODE_RECEIVED) {
        led.flash(250, 0, 0, 50, 80, 2);
        led.off();
      }

    } while (!STAY_AWAKE);

    // Re-enable all peripherals for normal active mode operation
    power_adc_enable();
    power_usart0_enable();
    power_spi_enable();
    power_timer0_enable();
    power_timer1_enable();
    power_twi_enable();

    irrecv.enableIRIn();

    FEEDBACK_OFF_ACTIVE = false;
    REQUEST_BATTERY_VOLTAGE_READING();
    led.flash(220, 220, 220, 500, 5, 1);
  }
}
