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
    case 0x11:
      sendSamsungCode(0x0B);
      if (!CHARGING_ACTIVE) {
        IR_CODE_REPEATING ? led.setColor(250, 20, 0) : led.flash(250, 0, 0, 50, 0, 0);
      }
      break;

    // VOL+ (remote code: E0E0E01F)
    case 0x10:
      sendSamsungCode(0x07);
      if (!CHARGING_ACTIVE) {
        IR_CODE_REPEATING ? led.setColor(20, 250, 0) : led.flash(0, 250, 0, 50, 0, 0);
      }
      break;

    // DEADBAND - (shrink window)
    case 0x2B:
      if (DEADBAND_COUNTER > -MAX_SHRINK_STEPS) {
        DEADBAND_COUNTER--;
        AUDIO_BASELINE_HIGH = max(0, 660 + (DEADBAND_COUNTER * 40));
        AUDIO_BASELINE_LOW  = min(1023, 340 - (DEADBAND_COUNTER * 40));
        DEADBAND = 136 + (DEADBAND_COUNTER * 40);
      }

      if (DEADBAND_COUNTER <= -MAX_SHRINK_STEPS) {
        MY_TONE(3750, -250, 15, 2);
        led.flash(220, 220, 220, 50, 50, 2);
      } else {
        MY_TONE(3000, 40, 1);
        led.flash(100, 0, 255, 80, 50, 1);
      }
      break;

    // DEADBAND + (expand window)
    case 0x28:
      if (DEADBAND_COUNTER < MAX_EXPAND_STEPS) {
        DEADBAND_COUNTER++;
        AUDIO_BASELINE_HIGH = 660 + (DEADBAND_COUNTER * 40);
        AUDIO_BASELINE_LOW  = max(0, 340 - (DEADBAND_COUNTER * 40));
        DEADBAND = 136 + (DEADBAND_COUNTER * 40);
      }

      if (DEADBAND_COUNTER >= MAX_EXPAND_STEPS) {
        MY_TONE(3250, +250, 15, 2);
        led.flash(220, 220, 220, 50, 50, 2);
      } else {
        MY_TONE(3500, 40, 1);
        led.flash(0, 80, 250, 80, 50, 1);
      }
      break;

    // Charging manual start / stop toggle
    case 0x38: {
        unsigned long CURRENT_MILLIS = millis();
        CHARGING_CYCLE_COMPLETE = !CHARGING_CYCLE_COMPLETE;

        if (!CHARGING_CYCLE_COMPLETE) {
          digitalWrite(PIN_CHARGE_CONTROL, HIGH);
          eepromWearLevelWrite(EEPROM_CHARGING_ACTIVE_BASE, true);
          LOW_VOLTAGE_START_CHARGING = true;
          CHARGING_ACTIVE = true;
          LAST_TIME_VOLTAGE_READING = CURRENT_MILLIS;
          PAUSE_CHARGING_TO_READ_VOLTAGE = 0;
          MY_TONE(3250, +250, 20, 2);
        } else {
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
    case 0x31:
      led.flash(0, 250, 100, 50, 100, 2);
      arduinoReset();
      break;

    // Tone / Volume control toggle button
    case 0x0D:
      if (!MY_TONE_ENABLED) {
        sendSamsungCode(0x0F); // Mute
      } else {
        VOLUME_CONTROL_ACTIVE = !VOLUME_CONTROL_ACTIVE;
        VOLUME_CONTROL_ACTIVE ? led.flash(0, 250, 0, 50, 50, 2) : led.flash(250, 0, 0, 50, 50, 2);
      }
      delay(MS(250));
      break;

    // Jellyfin Toggle
    case 0x76:
      SELECT_JELLYFIN = !SELECT_JELLYFIN;
      SELECT_HDMI = false;
      eepromWearLevelWrite(EEPROM_JELLYFIN_SELECT_BASE, SELECT_JELLYFIN);
      eepromWearLevelWrite(EEPROM_HDMI_SELECT_BASE, SELECT_HDMI);

      sendSamsungCode(0x79);
      led.flash(SELECT_JELLYFIN ? 0 : 220, SELECT_JELLYFIN ? 200 : 220, SELECT_JELLYFIN ? 200 : 220, 250, 550, 1);

      for (byte repeat = 0; repeat < 2; repeat++) {
        sendSamsungCode(SELECT_JELLYFIN ? 0x65 : 0x62); // Left x2 or Right x2
        led.flash(0, 50, 200, 100, 550, 1);
      }
      sendSamsungCode(0x68);

      led.flash(SELECT_JELLYFIN ? 0 : 220, SELECT_JELLYFIN ? 200 : 220, SELECT_JELLYFIN ? 200 : 220, 50, 85, 2);
      break;

    // Sleep
    case 0x37:
      GO_TO_SLEEP(true);
      break;

    // Turn TV ON/OFF
    case 0x0C:
      sendSamsungCode(0x02);
      GO_TO_SLEEP(true);
      break;

    // Verify battery voltage
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
    case 0x3C:
      MY_TONE_ENABLED = !MY_TONE_ENABLED;
      if (MY_TONE_ENABLED) MY_TONE(4200, 75, 1);
      delay(MS(250));
      break;

    // Toggle Serial Print Data
    case 0xF5:
      SERIAL_DATA_PRINT_ACTIVE = !SERIAL_DATA_PRINT_ACTIVE;
      eepromWearLevelWrite(EEPROM_SERIAL_ACTIVE_BASE, SERIAL_DATA_PRINT_ACTIVE);
      SERIAL_DATA_PRINT_ACTIVE ? MY_TONE(3000, 30, 2) : MY_TONE(4500, 30, 1);
      delay(MS(500));
      break;

    // MULTIVIEW / HDMI Toggle
    case 0x5D:
      SELECT_HDMI = !SELECT_HDMI;
      sendSamsungCode(SELECT_HDMI ? 0x8B : 0x1B);
      eepromWearLevelWrite(EEPROM_HDMI_SELECT_BASE, SELECT_HDMI);
      break;

    // Unmatched command -> fallback to generic repeat blink
    default:
      if (!CHARGING_ACTIVE) FEEDBACK_MODE(BLINK);

      // Check PROGMEM Lookup table for non-explicit codes
      for (byte i = 0; i < IR_TO_SAMSUNG_SIZE; i++) {
        uint8_t irCode = pgm_read_byte(&PHILIPS_CODES[i]);
        if (FILTERED_CODE == irCode) {
          sendSamsungCode(i, true);
          break;
        }
      }
      break;
  }

  delay(IR_CODE_REPEATING ? 1 : 75);
}
