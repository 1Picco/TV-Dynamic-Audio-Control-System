
// ============================================================================
// RGB SERIAL COMMUNICATION
// ============================================================================
// Receives RGB colour commands from the browser-based colour editor.
//
// Commands:
// RGB_GET         - Request current RGB values
// RGB_SET,R,G,B   - Set new RGB values (0-255)
//
// Responses:
// RGB_STATE,R,G,B - Current RGB values
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
