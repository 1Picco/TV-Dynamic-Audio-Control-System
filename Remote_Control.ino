// ============================================================================
// IR REMOTE FUNCTIONS
// ============================================================================

extern const uint8_t PHILIPS_CODES[];
extern const uint8_t SAMSUNG_CMD[];
extern const uint8_t IR_TO_SAMSUNG_SIZE;

// ============================================================================
// Function to handle IR remote control commands for TV control
// Processes IR signals and sends corresponding Samsung TV commands

void REMOTE_CONTROL_MODE() {

  switch (FILTERED_CODE) {

    // VOL- (remote code: E0E0D02F)
    // Send Samsung Volume Down and provide red LED feedback.
    // A held/repeating command uses a steady LED; a single command flashes.
    case 0x11:
      sendSamsungCode(0x0B);
      if (!CHARGING_ACTIVE) {
        IR_CODE_REPEATING ? led.setColor(250, 20, 0) : led.flash(250, 0, 0, 50, 0, 0);
      }
      break;

    // VOL+ (remote code: E0E0E01F)
    // Send Samsung Volume Up and provide green LED feedback.
    // A held/repeating command uses a steady LED; a single command flashes.
    case 0x10:
      sendSamsungCode(0x07);
      if (!CHARGING_ACTIVE) {
        IR_CODE_REPEATING ? led.setColor(20, 250, 0) : led.flash(0, 250, 0, 50, 0, 0);
      }
      break;

    // DEADBAND - (shrink window)
    // Reduce the audio deadband so the system reacts to smaller changes
    // around the current audio baseline.
    // The adjustment is limited to MAX_SHRINK_STEPS.
    case 0x2B:
      if (DEADBAND_COUNTER > -MAX_SHRINK_STEPS) {
        DEADBAND_COUNTER--;
        AUDIO_BASELINE_HIGH = max(0, 660 + (DEADBAND_COUNTER * 40));
        AUDIO_BASELINE_LOW  = min(1023, 340 - (DEADBAND_COUNTER * 40));
        DEADBAND = 136 + (DEADBAND_COUNTER * 40);
      }

      // At the minimum setting, give a different tone and white feedback
      // to indicate that no further adjustment is possible.
      if (DEADBAND_COUNTER <= -MAX_SHRINK_STEPS) {
        MY_TONE(3750, -250, 15, 2);
        led.flash(220, 220, 220, 50, 50, 2);
      } else {
        MY_TONE(3000, 40, 1);
        led.flash(100, 0, 255, 80, 50, 1);
      }
      break;

    // DEADBAND + (expand window)
    // Increase the audio deadband so larger changes are required
    // before the audio level is considered to have changed.
    // The adjustment is limited to MAX_EXPAND_STEPS.
    case 0x28:
      if (DEADBAND_COUNTER < MAX_EXPAND_STEPS) {
        DEADBAND_COUNTER++;
        AUDIO_BASELINE_HIGH = 660 + (DEADBAND_COUNTER * 40);
        AUDIO_BASELINE_LOW  = max(0, 340 - (DEADBAND_COUNTER * 40));
        DEADBAND = 136 + (DEADBAND_COUNTER * 40);
      }

      // At the maximum setting, give a different tone and white feedback
      // to indicate that no further adjustment is possible.
      if (DEADBAND_COUNTER >= MAX_EXPAND_STEPS) {
        MY_TONE(3250, +250, 15, 2);
        led.flash(220, 220, 220, 50, 50, 2);
      } else {
        MY_TONE(3500, 40, 1);
        led.flash(0, 80, 250, 80, 50, 1);
      }
      break;

    // Charging manual start / stop toggle
    // Manually starts or stops the charging cycle and stores the new
    // charging state in EEPROM.
    case 0x38: {
        unsigned long CURRENT_MILLIS = millis();
        CHARGING_CYCLE_COMPLETE = !CHARGING_CYCLE_COMPLETE;

        if (!CHARGING_CYCLE_COMPLETE) {
          // Start charging and mark the cycle as active.
          digitalWrite(PIN_CHARGE_CONTROL, HIGH);
          eepromWearLevelWrite(EEPROM_CHARGING_ACTIVE_BASE, true);
          LOW_VOLTAGE_START_CHARGING = true;
          CHARGING_ACTIVE = true;
          LAST_TIME_VOLTAGE_READING = CURRENT_MILLIS;
          PAUSE_CHARGING_TO_READ_VOLTAGE = 0;
          MY_TONE(3250, +250, 20, 2);
        } else {
          // Stop charging and clear the charging-related state.
          digitalWrite(PIN_CHARGE_CONTROL, LOW);
          eepromWearLevelWrite(EEPROM_CHARGING_ACTIVE_BASE, false);
          LOW_VOLTAGE_START_CHARGING = false;
          CHARGING_ACTIVE = false;
          PAUSE_CHARGING_TO_READ_VOLTAGE = 0;
          MY_TONE(3750, -250, 20, 2);
          led.off();
          //REQUEST_BATTERY_VOLTAGE_READING();
        }
        delay(MS(200));
        break;
      }

    // Reset
    // Flash cyan feedback and perform a hardware reset of the Arduino.
    case 0x31:
      led.flash(0, 250, 100, 50, 100, 2);
      arduinoReset();
      break;

    // Tone / Volume control toggle button
    // When the tone system is disabled, this button sends Samsung Mute.
    // Otherwise, it toggles automatic volume control on/off.
    case 0x0D:
      if (!MY_TONE_ENABLED) {
        sendSamsungCode(0x0F); // Mute
      } else {
        VOLUME_CONTROL_ACTIVE = !VOLUME_CONTROL_ACTIVE;
        VOLUME_CONTROL_ACTIVE ? led.flash(0, 250, 0, 50, 50, 2) : led.flash(250, 0, 0, 50, 50, 2);
      }
      delay(MS(250));
      break;

    // Netflix button repurposed to select Jellyfin or the TV source.
    // Uses the TV menu to select the required app/source instead of opening Netflix.
    // The current Jellyfin selection and HDMI selection state are stored in EEPROM.
    case 0x76:
      SELECT_JELLYFIN = !SELECT_JELLYFIN;
      SELECT_HDMI = false;
      eepromWearLevelWrite(EEPROM_JELLYFIN_SELECT_BASE, SELECT_JELLYFIN);
      eepromWearLevelWrite(EEPROM_HDMI_SELECT_BASE, SELECT_HDMI);

      // Open the TV menu (HOME) containing the Jellyfin app and TV source.
      sendSamsungCode(0x79);
      led.flash(SELECT_JELLYFIN ? 0 : 220, SELECT_JELLYFIN ? 200 : 220, SELECT_JELLYFIN ? 200 : 220, 250, 550, 1);

      // Move left or right twice to select Jellyfin or the TV source.
      for (byte repeat = 0; repeat < 2; repeat++) {
        sendSamsungCode(SELECT_JELLYFIN ? 0x65 : 0x62); // Left x2 or Right x2
        led.flash(0, 50, 200, 100, 550, 1);
      }

      // Confirm the selected app/source with OK button.
      sendSamsungCode(0x68);

      // Final feedback indicates the selected Jellyfin/source state.
      led.flash(SELECT_JELLYFIN ? 0 : 220, SELECT_JELLYFIN ? 200 : 220, SELECT_JELLYFIN ? 200 : 220, 50, 85, 2);
      break;

    // Sleep
    // Enter sleep mode and allow the IR system to wake the controller.
    case 0x37:
      GO_TO_SLEEP(true);
      break;

    // Turn TV ON/OFF
    // Send the Samsung Power command and then put the controller to sleep.
    case 0x0C:
      sendSamsungCode(0x02);
      GO_TO_SLEEP(true);
      break;

    // Verify battery voltage
    // Display the battery voltage using the dedicated LED feedback sequence.
    case 0x9F:
      VERIFY_BATTERY_VOLTAGE();
      break;

    // PLAY
    case 0x2C:
      sendSamsungCode(0x47);
      break;

    // PAUSE
    case 0x30:
      sendSamsungCode(0x4A);
      break;

    // Toggle Tone System
    // Enable or disable the tone feedback system.
    // When enabled, play a confirmation tone.
    case 0x3C:
      MY_TONE_ENABLED = !MY_TONE_ENABLED;
      if (MY_TONE_ENABLED) MY_TONE(4200, 75, 1);
      delay(MS(250));
      break;

    // Toggle Serial Print Data
    // Enable or disable diagnostic serial output and save the setting in EEPROM.
    // The confirmation tone differs depending on the new state.
    case 0xF5:
      SERIAL_DATA_PRINT_ACTIVE = !SERIAL_DATA_PRINT_ACTIVE;
      eepromWearLevelWrite(EEPROM_SERIAL_ACTIVE_BASE, SERIAL_DATA_PRINT_ACTIVE);
      SERIAL_DATA_PRINT_ACTIVE ? MY_TONE(3000, 30, 2) : MY_TONE(4500, 30, 1);
      delay(MS(500));
      break;

    // MULTIVIEW / HDMI Toggle
    // Toggle the HDMI selection and send the corresponding Samsung command.
    // The selected state is stored in EEPROM.
    case 0x5D:
      SELECT_HDMI = !SELECT_HDMI;
      sendSamsungCode(SELECT_HDMI ? 0x8B : 0x1B);
      eepromWearLevelWrite(EEPROM_HDMI_SELECT_BASE, SELECT_HDMI);
      break;

    // Unmatched command -> fallback to generic repeat blink
    // For commands without an explicit case above, first provide the normal
    // battery-color feedback, then check the lookup table for a Samsung
    // command mapped to the received Philips remote code.
    default:
      if (!CHARGING_ACTIVE) FEEDBACK_MODE(BLINK);

      // Check PROGMEM Lookup table for non-explicit codes
      for (byte i = 0; i < IR_TO_SAMSUNG_SIZE; i++) {
        uint8_t irCode = pgm_read_byte(&PHILIPS_CODES[i]);

        // When a matching Philips code is found, send the corresponding
        // Samsung command from the lookup table.
        if (FILTERED_CODE == irCode) {
          sendSamsungCode(i, true);
          break;
        }
      }
      break;
  }

  // Short delay after every processed IR command.
  // Repeating commands use a shorter delay so held buttons remain responsive.
  delay(IR_CODE_REPEATING ? 1 : 75);
}
