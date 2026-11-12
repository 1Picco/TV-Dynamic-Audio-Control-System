
// ============================================================================
// RGB SERIAL COMMUNICATION
// ============================================================================
// Receives RGB colour commands from the browser-based colour editor.
//
// Commands:
// RGB_GET                 - Request current RGB values
// RGB_SET,R,G,B           - Set new RGB values (0-255)
// BATTERY_GET             - Request stored charging thresholds
// BATTERY_SET,START,STOP  - Set charging thresholds in millivolts
//
// Responses:
// RGB_STATE,R,G,B          - Current RGB values
// BATTERY_STATE,START,STOP - Current charging thresholds in millivolts
// BATTERY_SAVED,START,STOP - Charging thresholds saved to EEPROM
// BATTERY_ERROR,reason     - Charging threshold command rejected
// ============================================================================

const byte RGB_SERIAL_BUFFER_SIZE = 24;

char RGB_SERIAL_BUFFER[RGB_SERIAL_BUFFER_SIZE];
byte RGB_SERIAL_LENGTH = 0;
bool RGB_SERIAL_OVERFLOW = false;

// ============================================================================
// Send current RGB values to the browser
// ============================================================================

void PRINT_RGB_SERIAL_STATE() {

  Serial.print(F("RGB_STATE,"));
  Serial.print(RED_INTENSITY);
  Serial.print(',');
  Serial.print(GREEN_INTENSITY);
  Serial.print(',');
  Serial.println(BLUE_INTENSITY);
}

// ============================================================================
// Validate and convert an RGB intensity value (0-255)
// ============================================================================

bool PARSE_RGB_SERIAL_VALUE(const char* text, byte& value) {

  if (*text == '\0') return false;

  uint16_t parsedValue = 0;

  while (*text) {

    if (*text < '0' || *text > '9') return false;

    parsedValue = parsedValue * 10 + (*text - '0');

    if (parsedValue > 255) return false;

    text++;
  }

  value = (byte)parsedValue;
  return true;
}

// ============================================================================
// Process a complete RGB serial command
// ============================================================================
// ============================================================================
// Parse a battery threshold value received over serial (0-65535)
// ============================================================================

bool PARSE_BATTERY_SERIAL_VALUE(const char* text, uint16_t& value) {

  if (*text == '\0') return false;

  uint32_t parsedValue = 0;

  while (*text) {

    if (*text < '0' || *text > '9') return false;

    parsedValue = parsedValue * 10UL + (*text - '0');

    if (parsedValue > 65535UL) return false;

    text++;
  }

  value = (uint16_t)parsedValue;
  return true;
}

// ============================================================================
// Send the active charging thresholds to the browser
// ============================================================================

void PRINT_BATTERY_SERIAL_STATE(const __FlashStringHelper* response) {

  Serial.print(response);
  Serial.print(START_CHARGING_VOLTAGE);
  Serial.print(',');
  Serial.println(STOP_CHARGING_VOLTAGE);
}

// ============================================================================
// Process battery threshold commands from the browser
// ============================================================================

bool PROCESS_BATTERY_SERIAL_COMMAND() {

  if (strcmp(RGB_SERIAL_BUFFER, "BATTERY_GET") == 0) {
    PRINT_BATTERY_SERIAL_STATE(F("BATTERY_STATE,"));
    return true;
  }

  if (strncmp(RGB_SERIAL_BUFFER, "BATTERY_SET,", 12) != 0) return false;

  char* startText = RGB_SERIAL_BUFFER + 12;
  char* comma = strchr(startText, ',');

  if (!comma) {
    Serial.println(F("BATTERY_ERROR,INVALID_FORMAT"));
    return true;
  }

  *comma = '\0';

  char* stopText = comma + 1;

  if (strchr(stopText, ',') != NULL) {
    Serial.println(F("BATTERY_ERROR,INVALID_FORMAT"));
    return true;
  }

  uint16_t startVoltage, stopVoltage;

  if (!PARSE_BATTERY_SERIAL_VALUE(startText, startVoltage) ||
      !PARSE_BATTERY_SERIAL_VALUE(stopText, stopVoltage)) {
    Serial.println(F("BATTERY_ERROR,INVALID_FORMAT"));
    return true;
  }

  if (!VALIDATE_CHARGING_THRESHOLDS(startVoltage, stopVoltage)) {
    Serial.println(F("BATTERY_ERROR,INVALID_THRESHOLDS"));
    return true;
  }

  if (!STORE_CHARGING_THRESHOLDS(startVoltage, stopVoltage)) {
    Serial.println(F("BATTERY_ERROR,EEPROM_WRITE_FAILED"));
    return true;
  }

  START_CHARGING_VOLTAGE = startVoltage;
  STOP_CHARGING_VOLTAGE = stopVoltage;

  PRINT_BATTERY_SERIAL_STATE(F("BATTERY_SAVED,"));
  return true;
}

void PROCESS_RGB_SERIAL_COMMAND() {

  RGB_SERIAL_BUFFER[RGB_SERIAL_LENGTH] = '\0';

  if (strcmp(RGB_SERIAL_BUFFER, "RGB_EDITOR_HEARTBEAT") == 0) {

    RGB_EDITOR_LAST_HEARTBEAT = millis();

    if (REMOTE_CONTROL_MODE_ACTIVE) {
      REMOTE_CONTROL_MODE_ACTIVE = false;
      RESET_FEEDBACK_TIMERS();
      FEEDBACK_LED_ACTIVE = false;
      FEEDBACK_OFF_ACTIVE = false;
      led.setColor(RED_INTENSITY, GREEN_INTENSITY, BLUE_INTENSITY);
    }

    Serial.println(F("RGB_EDITOR_ACTIVE"));
    PRINT_RGB_SERIAL_STATE();
    return;
  }

  if (strcmp(RGB_SERIAL_BUFFER, "RGB_EDITOR_OFF") == 0) {

    if (!REMOTE_CONTROL_MODE_ACTIVE) {
      REMOTE_CONTROL_MODE_ACTIVE = true;
      RESET_FEEDBACK_TIMERS();
      FEEDBACK_MODE(BATTERY_VOLTAGE_STATUS);
    }

    return;
  }

  if (REMOTE_CONTROL_MODE_ACTIVE) return;

  if (PROCESS_BATTERY_SERIAL_COMMAND()) return;

  if (strcmp(RGB_SERIAL_BUFFER, "RGB_GET") == 0) {

    PRINT_RGB_SERIAL_STATE();
    return;
  }

  if (strncmp(RGB_SERIAL_BUFFER, "RGB_SET,", 8) == 0) {

    char* redText = RGB_SERIAL_BUFFER + 8;
    char* comma1 = strchr(redText, ',');

    if (!comma1) return;

    *comma1 = '\0';

    char* greenText = comma1 + 1;
    char* comma2 = strchr(greenText, ',');

    if (!comma2) return;

    *comma2 = '\0';

    char* blueText = comma2 + 1;

    byte red, green, blue;

    if (PARSE_RGB_SERIAL_VALUE(redText, red) && PARSE_RGB_SERIAL_VALUE(greenText, green) && PARSE_RGB_SERIAL_VALUE(blueText, blue)) {

      RED_INTENSITY = red;
      GREEN_INTENSITY = green;
      BLUE_INTENSITY = blue;

      led.setColor(RED_INTENSITY, GREEN_INTENSITY, BLUE_INTENSITY);

      PRINT_RGB_SERIAL_STATE();
    }
  }
}

// ============================================================================
// Read and process incoming browser serial commands
// ============================================================================

void HANDLE_RGB_SERIAL() {

  while (Serial.available()) {

    char incoming = Serial.read();

    if (incoming == '\r') continue;

    if (incoming == '\n') {

      if (!RGB_SERIAL_OVERFLOW && RGB_SERIAL_LENGTH > 0) {
        PROCESS_RGB_SERIAL_COMMAND();
      }

      RGB_SERIAL_LENGTH = 0;
      RGB_SERIAL_OVERFLOW = false;
    }

    else if (!RGB_SERIAL_OVERFLOW) {

      if (RGB_SERIAL_LENGTH < RGB_SERIAL_BUFFER_SIZE - 1) {
        RGB_SERIAL_BUFFER[RGB_SERIAL_LENGTH++] = incoming;
      }

      else {
        RGB_SERIAL_OVERFLOW = true;
      }
    }
  }
}
