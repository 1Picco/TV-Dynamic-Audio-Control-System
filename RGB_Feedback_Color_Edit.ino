// ============================================================================
// RGB FEEDBACK COLOR EDITING FUNCTION
// ============================================================================

// ============================================================================
// RGB INTENSITY POINTER TABLE
// Allows accessing RGB values using SELECTED_COLOR index
// 0 = Red, 1 = Green, 2 = Blue
// ============================================================================

byte* RGB_VALUES[3] = { &RED_INTENSITY, &GREEN_INTENSITY, &BLUE_INTENSITY };

// ============================================================================
// RGB COLOR EDIT MODE
// Allows editing RGB intensity values using IR remote
// - Left/Right : Select color (R, G, B)
// - Numeric keys : Enter intensity value (0–255)
// - OK : Confirm entered value
// - Up/Down : Increment / Decrement selected color
// ============================================================================

void RGB_CODE_EDIT() {

  FIRST_RGB_RUN = false;

  // ==========================================================================
  // HANDLE COLOR SELECTION (LEFT / RIGHT ARROWS)

  if (FILTERED_CODE == 0x5A || FILTERED_CODE == 0x5B) {

    // Left arrow → cycle backward
    if (FILTERED_CODE == 0x5A) {
      SELECTED_COLOR = (SELECTED_COLOR + 2) % 3;
    }

    // Right arrow → cycle forward
    else {
      SELECTED_COLOR = (SELECTED_COLOR + 1) % 3;
    }

    PRINT_SELECTED_COLOR();
  }

  // ==========================================================================
  // HANDLE NUMERIC INPUT FOR COLOR INTENSITY

  int8_t digit = (FILTERED_CODE <= 9) ? FILTERED_CODE : -1;

  if (digit != -1) {

    // Get current buffer length
    byte len = strlen(INPUT_BUFFER);

    // Allow maximum 3 digits (RGB range: 0–255)
    if (len < 3) {

      // Append digit to buffer
      INPUT_BUFFER[len] = '0' + digit;
      INPUT_BUFFER[len + 1] = '\0';

      uint16_t value = atoi(INPUT_BUFFER);

      // Prevent values above RGB limit
      if (value > 255) {

        ACTIVE_COLOR();
        Serial.println(INPUT_BUFFER);
        Serial.println(F("MAX value: 255"));
        ACTIVE_COLOR();

        INPUT_BUFFER[0] = '\0';   // Clear buffer
        Serial.println(F("0"));
        AWAITING_CONFIRMATION = false;
      }
      else {

        ACTIVE_COLOR();
        Serial.println(INPUT_BUFFER);
        AWAITING_CONFIRMATION = true;
      }
    }

    // Safety check if buffer somehow exceeds expected length
    else {

      Serial.println(F("3 digits MAX."));
      ACTIVE_COLOR();

      INPUT_BUFFER[0] = '\0';
      Serial.println(F("0"));
      AWAITING_CONFIRMATION = false;
    }
    
    // Delay depending on IR repeat state
    delay(IR_CODE_REPEATING ? 1 : 200);

    return;   // Exit after handling digit input
  }

  // ==========================================================================
  // HANDLE CONFIRMATION (OK BUTTON)

  if (FILTERED_CODE == 0x5C) {

    if (AWAITING_CONFIRMATION) {

      byte value = constrain(atoi(INPUT_BUFFER), 0, 255);

      // Apply value to selected RGB channel
      *RGB_VALUES[SELECTED_COLOR] = value;

      // Update LED color
      led.setColor(RED_INTENSITY, GREEN_INTENSITY, BLUE_INTENSITY);
      PRINT_RGB_VALUES();

      // Reset input state
      INPUT_BUFFER[0] = '\0';
      AWAITING_CONFIRMATION = false;
    }
  }

  // ==========================================================================
  // HANDLE INTENSITY ADJUSTMENT (UP / DOWN BUTTONS)
  if (SELECTED_COLOR != -1) {

    // ------------------------------------------------------------------------
    // UP BUTTON (Increase intensity)

    if (FILTERED_CODE == 0x58) {

      byte increment = IR_CODE_REPEATING ? 2 : 1;

      *RGB_VALUES[SELECTED_COLOR] =
        constrain(*RGB_VALUES[SELECTED_COLOR] + increment, 0, 255);

      led.setColor(RED_INTENSITY, GREEN_INTENSITY, BLUE_INTENSITY);
      PRINT_RGB_VALUES();
    }

    // ------------------------------------------------------------------------
    // DOWN BUTTON (Decrease intensity)

    else if (FILTERED_CODE == 0x59) {

      byte decrement = IR_CODE_REPEATING ? 2 : 1;

      *RGB_VALUES[SELECTED_COLOR] =
        constrain(*RGB_VALUES[SELECTED_COLOR] - decrement, 0, 255);

      led.setColor(RED_INTENSITY, GREEN_INTENSITY, BLUE_INTENSITY);
      PRINT_RGB_VALUES();
    }
  }

  // GENERAL INPUT DELAY
  delay(IR_CODE_REPEATING ? 1 : 200);
}

// ============================================================================
// Function to print the currently selected color name
// Used for RGB mode color selection feedback

void ACTIVE_COLOR() {

  switch (SELECTED_COLOR) {
    case 0: Serial.print(F("Red:")); break;    // Print "Red: " for red selection
    case 1: Serial.print(F("Green:")); break;  // Print "Green: " for green selection
    case 2: Serial.print(F("Blue:")); break;   // Print "Blue: " for blue selection
  }
}

// ============================================================================
// Function to print the currently selected color with emphasis
// Used for RGB mode color selection display

void PRINT_SELECTED_COLOR() {

  switch (SELECTED_COLOR) {
    case 0: Serial.println(F("> Red")); break;    // Print "> Red" for red selection
    case 1: Serial.println(F("> Green")); break;  // Print "> Green" for green selection
    case 2: Serial.println(F("> Blue")); break;   // Print "> Blue" for blue selection
  }
}

// ============================================================================
// Function to print current RGB intensity values
// Used for RGB mode debugging and feedback

void PRINT_RGB_VALUES() {

  Serial.print(F("RGB values: ("));
  Serial.print(RED_INTENSITY);    // Print red intensity value
  Serial.print(F(", "));
  Serial.print(GREEN_INTENSITY);  // Print green intensity value
  Serial.print(F(", "));
  Serial.print(BLUE_INTENSITY);   // Print blue intensity value
  Serial.println(F(")"));        // Close parentheses and newline
}
