// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
// Function to print remaining time until the next battery check

void PRINT_COUNTDOWN() {
  if (FIRST_VOLTAGE_READING_TAKEN) return;

  static unsigned long lastPrintMillis = 0;
  static bool labelPrinted = false;
  unsigned long currentMillis = millis();

  if (currentMillis - lastPrintMillis >= SECONDS(1)) {
    lastPrintMillis = currentMillis;

    unsigned long elapsed = currentMillis - PREVIOUS_VOLTAGE_READING;

    if (TIME_TO_READ_BATTERY_VOLTAGE > elapsed) {
      unsigned long remainingSeconds = (TIME_TO_READ_BATTERY_VOLTAGE - elapsed) / 1000UL;
      unsigned long minutes = remainingSeconds / 60;
      unsigned long seconds = remainingSeconds % 60;

      // Print the prefix label only once at the start of the countdown
      if (!labelPrinted) {
        info.print(F("Next voltage reading in: "));
        labelPrinted = true;
      }

      // \r moves cursor back to start of line, \x1B[K clears to end of line if needed
      info.print(F("\rNext voltage reading in: "));
      if (minutes < 10) info.print(F("0"));
      info.print(minutes);
      info.print(F(":"));
      if (seconds < 10) info.print(F("0"));
      info.print(seconds);
      // NOTE: Use info.print(), NOT info.println() so it stays on the same line!
    } else {
      labelPrinted = false; // Reset for next cycle
    }
  }
}

void PRINT_CHARGE_CHECK_COUNTDOWN() {
  // Only print when active charging is ongoing and not currently paused for measurement
  if (!CHARGING_ACTIVE || CHARGING_PAUSED) return;

  static unsigned long lastPrintMillis = 0;
  static bool labelPrinted = false;
  unsigned long currentMillis = millis();

  // Tick once per second
  if (currentMillis - lastPrintMillis >= SECONDS(1)) {
    lastPrintMillis = currentMillis;

    unsigned long elapsed = currentMillis - LAST_TIME_VOLTAGE_READING;

    if (TIME_TO_CHECK_BATTERY_CHARGE_STATE > elapsed) {
      unsigned long remainingSeconds = (TIME_TO_CHECK_BATTERY_CHARGE_STATE - elapsed) / 1000UL;
      unsigned long minutes = remainingSeconds / 60;
      unsigned long seconds = remainingSeconds % 60;

      // Print the prefix label only once at the start of the countdown
      if (!labelPrinted) {
        info.print(F("Next charge check in: "));
        labelPrinted = true;
      }

      // \r moves cursor back to start of line
      info.print(F("\rNext charge check in: "));
      if (minutes < 10) info.print(F("0"));
      info.print(minutes);
      info.print(F(":"));
      if (seconds < 10) info.print(F("0"));
      info.print(seconds);
    } else {
      labelPrinted = false; // Reset for next cycle
    }
  }
}
// ============================================================================
// Function to filter valid IR commands

bool SIGNAL_FILTER() {

  // Ignore Samsung TV remotes and protocol 14 (phone proximity sensor)
  if (results.decode_type == SAMSUNG || results.decode_type == 14) {
    irrecv.resume();
    return false;
  }

  // Reject unsupported (NEC), unknown, or oversized raw values
  if (results.decode_type == NEC || results.decode_type == UNKNOWN || results.value > 0xFFFFF) {
    led.flash(30, 30, 250, 35, 80, 2); // Purple error blink
    MY_TONE(3800, 10, 2);
    irrecv.resume();
    return false;
  }

  // Rate-limiting debounce check
  unsigned long now = millis();
  if ((now - LAST_PROCESSED_SIGNAL_TIME) < MIN_SIGNAL_INTERVAL) {
    irrecv.resume();
    return false;
  }

  // Convert long IR code to single-byte command (implicit truncation if FILTERED_CODE is byte)
  FILTERED_CODE = (byte)(results.value & 0xFF);

  // Detect repeating hold-down command vs new command
  if (FILTERED_CODE == LAST_IR_CODE) {
    CONSECUTIVE_IR_CODE_COUNT++;
    if (CONSECUTIVE_IR_CODE_COUNT > 9) {
      IR_CODE_REPEATING = true;
    }
  } else {
    CONSECUTIVE_IR_CODE_COUNT = 1;
    IR_CODE_REPEATING = false;
    LAST_IR_CODE = FILTERED_CODE;
  }

  // Timestamp updates
  LAST_IR_SIGNAL_TIME = now;
  LAST_PROCESSED_SIGNAL_TIME = now;

  RESET_FEEDBACK_TIMERS();

  return true; // Valid signal ready
}

// ============================================================================
// IR CODE LOOKUP TABLE - Stored in Flash memory (PROGMEM) to save RAM
// ============================================================================

const uint8_t PHILIPS_CODES[] PROGMEM = {
  0x21, 0x20, 0x40, 0x54, 0x5C, 0x58,
  0x59, 0x5A, 0x5B, 0x0A, 0x4B, 0xD2,
  0xCC, 0x0F, 0x01, 0x02, 0x03, 0x04,
  0x05, 0x06, 0x07, 0x08, 0x09, 0x00
};

const uint8_t SAMSUNG_CMD[] PROGMEM = {
  0x10, 0x12, 0x1A, 0x79, 0x68, 0x60,
  0x61, 0x65, 0x62, 0x58, 0x25, 0x6B,
  0x4F, 0x1F, 0x04, 0x05, 0x06, 0x08,
  0x09, 0x0A, 0x0C, 0x0D, 0x0E, 0x11
};

const byte IR_TO_SAMSUNG_SIZE = 24;

// ============================================================================
// REMOTE CONTROL FUNCTIONS
// ============================================================================
// Function to send Samsung TV IR commands

void sendSamsungCode(uint8_t cmd, bool use_lookup = false) {
  uint8_t command = cmd;

  if (use_lookup) {
    if (cmd < IR_TO_SAMSUNG_SIZE) {
      command = pgm_read_byte(&SAMSUNG_CMD[cmd]);
    } else {
      return;  // Invalid index
    }
  }
  IrReceiver.stop();
  //delay(MS(1));
  IrSender.sendSamsung(0x707, command, 1);
  delay(MS(1));
  IrReceiver.start();
}

// ============================================================================
// Function to decode IR remote signal
// Prints protocol, address, command and decodedRawData

void rawCode() {
  if (IrReceiver.decode()) {

    Serial.print("Protocol: ");
    Serial.println(IrReceiver.decodedIRData.protocol);

    Serial.print("Address: 0x");
    Serial.println(IrReceiver.decodedIRData.address, HEX);

    Serial.print("Command: 0x");
    Serial.println(IrReceiver.decodedIRData.command, HEX);

    Serial.print("Raw: 0x");
    Serial.println(IrReceiver.decodedIRData.decodedRawData, HEX);

    Serial.println("-------------------");
    delay(MS(500));
    IrReceiver.resume();
  }
}

// ============================================================================
// Function to reset all system timers
// Called when activity is detected to extend system awake time

void RESET_FEEDBACK_TIMERS() {
  unsigned long now = millis();

  FEEDBACK_OFF_START_TIME = now;            // Reset IR feedback timer
  FEEDBACK_OFF_ACTIVE = true;               // Enable IR feedback timer

  RGB_FEEDBACK_START_TIME = now;            // Reset LED feedback timer
  FEEDBACK_LED_ACTIVE = true;               // Enable LED feedback timer

  GO_TO_SLEEP_START_TIME = now;             // Reset sleep timer
  ARDUINO_WAITING_TO_SLEEP = true;          // Enable sleep timer
}

// ============================================================================
// MY_TONE - Original version
// Plays the same frequency for all beeps.
//
// Example:
// MY_TONE(3500, 15, 2);
// ============================================================================

void MY_TONE(uint16_t frequency, uint8_t duration, uint8_t beepCount) {

  MY_TONE(frequency, 0, duration, beepCount);
}

// ============================================================================
// MY_TONE - Frequency sequence version
//
// frequency      = starting frequency
// frequencyStep  = frequency change after each beep (+/-)
// duration       = duration of each beep in milliseconds
// beepCount      = number of beeps
//
// Examples:
//
// MY_TONE(3500, 0, 15, 2);
// -> 3500, 3500 Hz
//
// MY_TONE(3000, +250, 15, 3);
// -> 3000, 3250, 3500 Hz
//
// MY_TONE(3000, -250, 15, 3);
// -> 3000, 2750, 2500 Hz
// ============================================================================

void MY_TONE(uint16_t frequency, int16_t frequencyStep,
             uint8_t duration, uint8_t beepCount) {

  if (!MY_TONE_ENABLED) return;             // Exit if tone is disabled
  if (beepCount == 0) return;               // Nothing to play
  if (frequency == 0) return;               // Invalid starting frequency

  const uint8_t pauseBetweenBeeps = 85;     // Pause between beeps in ms

  pinMode(11, OUTPUT);

  int32_t currentFrequency = frequency;

  for (byte i = 0; i < beepCount; i++) {

    // Prevent frequency from becoming zero or negative
    if (currentFrequency < 1) currentFrequency = 1;

    uint16_t halfPeriodMicros = 500000L / currentFrequency;
    unsigned long endTime = millis() + duration;

    // Generate the current beep
    while (millis() < endTime) {

      digitalWrite(11, HIGH);
      delayMicroseconds(halfPeriodMicros);

      digitalWrite(11, LOW);
      delayMicroseconds(halfPeriodMicros);
    }

    // Pause between beeps
    if (i < beepCount - 1) delay(pauseBetweenBeeps);

    // Change frequency for the next beep
    currentFrequency += frequencyStep;
  }

  pinMode(11, INPUT);                        // Turn off the pin
}

// ============================================================================
// Function to play custom audio tone for feedback and repeated intervals

void MY_TONE_REPEAT(uint16_t frequency, uint8_t duration, uint8_t beepCount, unsigned long interval) {
  static unsigned long lastTrigger = 0;
  static bool played = false;

  if (interval == 0) {          // One-shot mode
    if (!played) {
      played = true;
      MY_TONE(frequency, duration, beepCount);
    }
    return;
  }

  played = false;               // Allow one-shot again after leaving one-shot mode

  if (millis() - lastTrigger >= interval) {
    lastTrigger = millis();
    MY_TONE(frequency, duration, beepCount);
  }
}

// ============================================================================
// ADC PEAK DETECTION - Optimized for dynamic audio content
// ============================================================================

uint16_t analogReadFiltered() {

  uint16_t peak = analogRead(PIN_AUDIO);

  // filter: if peak is within noise range, treat as 0
  if (peak > 480 && peak < 520) {
    return constrain(peak, 498, 502);  // filtered out noise
  }
  // loud audio, return real peak
  return peak;
}

// ============================================================================
AsyncDelay READ_ANALOG_DATADelay;  // Timer for reading analog values
// Function to read analog values from potentiometers
// Reads reactivity and DEADBAND potentiometers and updates system parameters

void READ_ANALOG_DATA(bool READ_NOW = false) {

  auto now = millis();

  if (READ_NOW || READ_ANALOG_DATADelay.Reached(ANALOG_READ_DELAY_MS)) {
    // Read and validate potentiometer values
    int16_t rawDEADBAND = analogRead(PIN_DEADBAND);
    int16_t rawREACT = analogRead(PIN_REACT);

    // Only update DEADBAND from potentiometer if no manual adjustments have been made
    // DEADBAND_COUNTER != 0 means user has manually adjusted DEADBAND via remote
    if (DEADBAND_COUNTER == 0) {
      DEADBAND = map(rawDEADBAND, 0, 1023, 550, 20);  // Map potentiometer to DEADBAND range
      DEADBAND = constrain(DEADBAND, 20, 550);  // Ensure the DEADBAND stays within the desired range
    }

    uint8_t newREACTION_DELAY = map(rawREACT, 0, 1023, 32, 3);  // Map potentiometer to reaction delay
    // Check if REACTION_DELAY value has changed and play a tone for feedback
    if (newREACTION_DELAY != PREVIOUS_REACTION_DELAY && PREVIOUS_REACTION_DELAY != -1) {
      MY_TONE(4200, 50, 1);  // Play high-pitched tone when reactivity changes
    }
    // Update previous values for future comparisons
    PREVIOUS_REACTION_DELAY = newREACTION_DELAY;
  }
}

// ============================================================================
// Function to toggle between IR mode and RGB mode
// Switches between TV remote control mode and LED color control mode

void FUNCTION_MODE_TOGGLE() {
  // Toggle mode if button 0xBE pressed
  if (FILTERED_CODE == 0xBE) {
    REMOTE_CONTROL_MODE_ACTIVE = !REMOTE_CONTROL_MODE_ACTIVE;

    if (REMOTE_CONTROL_MODE_ACTIVE) {
      // Entering REMOTE_CONTROL_MODE
      MY_TONE(4000, 30, 1);
      FIRST_RGB_RUN = true;
      RESET_FEEDBACK_TIMERS();
      led.flash(220, 220, 220, 100, 100, 2);
    } else {
      // Entering RGB_CODE_EDIT mode
      SERIAL_DATA_PRINT_ACTIVE = false;
      FEEDBACK_LED_ACTIVE = false;
      MY_TONE(3000, 30, 2);
      PRINT_SELECTED_COLOR();   // Print the initial selected color to serial
      led.setColor(50, 0, 0);
      info.println(F("50, 0, 0"));
      //delay(MS(50));
    }
    return;  // Exit after toggling, don't execute mode functions
  }

  // Execute active mode handler for all other button codes
  if (REMOTE_CONTROL_MODE_ACTIVE) {
    REMOTE_CONTROL_MODE();
  } else {
    RGB_CODE_EDIT();
  }
}

// ============================================================================
// EEPROM Wear Leveling Functions - Reduce EEPROM wear
// ============================================================================
// Each slot stores a compact 1-byte record:
// bit 0   -> boolean value
// bits 1-7 -> sequence counter (8-bit wrap-around counter used for newest-value selection)


// Write with wear leveling - spreads writes across multiple addresses
void eepromWearLevelWrite(int8_t baseAddress, bool value) {
  byte valueToWrite = value ? 1 : 0;

  // Check current state of all addresses
  bool allMatch = true;
  bool allUninitialized = true;

  for (byte i = 0; i < EEPROM_CYCLE_SIZE; i++) {
    byte currentValue = EEPROM.read(baseAddress + i);
    if (currentValue != 0xFF) {
      allUninitialized = false;
    }
    if (currentValue != valueToWrite) {
      allMatch = false;
    }
  }

  // If all addresses already have the correct value, no write needed
  if (allMatch && !allUninitialized) {
    return;
  }

  // Write to all addresses for consistency and redundancy
  // This ensures all addresses are synchronized
  for (byte i = 0; i < EEPROM_CYCLE_SIZE; i++) {
    int address = baseAddress + i;
    byte currentValue = EEPROM.read(address);
    // Only write if value is different (EEPROM.update does this, but being explicit)
    if (currentValue != valueToWrite) {
      EEPROM.update(address, valueToWrite);
    }
  }
}

// Read with wear leveling - checks all addresses and returns majority value
bool eepromWearLevelRead(int8_t baseAddress) {
  int8_t trueCount = 0;
  int8_t falseCount = 0;
  int8_t initializedCount = 0;

  // Check all addresses in the wear leveling cycle
  for (byte i = 0; i < EEPROM_CYCLE_SIZE; i++) {
    int address = baseAddress + i;
    byte value = EEPROM.read(address);

    // Only count initialized values (0 or 1), ignore 0xFF (uninitialized)
    if (value == 1) {
      trueCount++;
      initializedCount++;
    } else if (value == 0) {
      falseCount++;
      initializedCount++;
    }
    // 0xFF (uninitialized) is ignored
  }

  // If no addresses are initialized, return default false
  if (initializedCount == 0) {
    return false;
  }

  // Return majority value among initialized addresses
  return (trueCount > falseCount);
}


/* 
               TV model: SAMSUNG UE43RU7102KXXH
               
                  ACCES SAMSUNG SERVICE MENU 
      INFO      +    SETTINGS    +      MUTE      +    POWER
   0xE0E0F807   +   0xE0E058A7   +   0xE0E0F00F   +  0xE0E06798


        SAMSUNG Parsed IR Parameters / Decoded IR Fields
      
 Parsed IR Parameters for Power button:
  0x707 - Address (The Samsung device identifier, standard for Samsung TVs)
   0x2  - Command Code (The specific button payload, e.g., Digit 2 or Source selection depending on the remote)
    0   - Repeats / Flags (Indicates standard transmission with no extra key-hold repeat bursts)
  
  POWER .... 0xE0E06798  <~>  IrSender.sendSamsung(0x707, 0x2, 0);
  SOURCE ... 0xE0E0807F  <~>  IrSender.sendSamsung(0x707, 0x1, 0);
  1 ........ 0xE0E020DF  <~>  IrSender.sendSamsung(0x707, 0x4, 0);
  2 ........ 0xE0E0A05F  <~>  IrSender.sendSamsung(0x707, 0x5, 0);
  3 ........ 0xE0E0609F  <~>  IrSender.sendSamsung(0x707, 0x6, 0);
  4 ........ 0xE0E010EF  <~>  IrSender.sendSamsung(0x707, 0x8, 0);
  5 ........ 0xE0E0906F  <~>  IrSender.sendSamsung(0x707, 0x9, 0);
  6 ........ 0xE0E050AF  <~>  IrSender.sendSamsung(0x707, 0xA, 0);
  7 ........ 0xE0E030CF  <~>  IrSender.sendSamsung(0x707, 0xC, 0);
  8 ........ 0xE0E0B04F  <~>  IrSender.sendSamsung(0x707, 0xD, 0);
  9 ........ 0xE0E0708F  <~>  IrSender.sendSamsung(0x707, 0xE, 0);
  0 ........ 0xE0E08877  <~>  IrSender.sendSamsung(0x707, 0x11, 0);
  TTX/MIX .. 0xE0E034CB  <~>  IrSender.sendSamsung(0x707, 0x2C, 0);
  PRE-CH ... 0xE0E0C837  <~>  IrSender.sendSamsung(0x707, 0x13, 0);
  VOL+ ..... 0xE0E0E01F  <~>  IrSender.sendSamsung(0x707, 0x7, 0);
  VOL- ..... 0xE0E0D02F  <~>  IrSender.sendSamsung(0x707, 0xB, 0);
  CH+ ...... 0xE0E048B7  <~>  IrSender.sendSamsung(0x707, 0x12, 0);
  CH- ...... 0xE0E008F7  <~>  IrSender.sendSamsung(0x707, 0x10, 0);
  MUTE ..... 0xE0E0F00F  <~>  IrSender.sendSamsung(0x707, 0xF, 0);
  CH LIST .. 0xE0E0D629  <~>  IrSender.sendSamsung(0x707, 0x6B, 0);
  NETFLIX .. 0xE0E0CF30  <~>  IrSender.sendSamsung(0x707, 0xF3, 0);
  RAKUTEN .. 0xE0E03DC2  <~>  IrSender.sendSamsung(0x707, 0xBC, 0);
  PRIME .... 0xE0E02FD0  <~>  IrSender.sendSamsung(0x707, 0xF4, 0);
  HDMI1 .... 0xE0E0D12E  <~>  IrSender.sendSamsung(0x707, 0x8B, 0);
  TOOLS .... 0xE0E0D22D  <~>  IrSender.sendSamsung(0x707, 0x4B, 0);
  MEDIA.P .. 0xE0E031CE  <~>  IrSender.sendSamsung(0x707, 0x8C, 0);
  HOME ..... 0xE0E09E61  <~>  IrSender.sendSamsung(0x707, 0x79, 0);
  GUIDE .... 0xE0E0F20D  <~>  IrSender.sendSamsung(0x707, 0x4F, 0);
  UP ....... 0xE0E006F9  <~>  IrSender.sendSamsung(0x707, 0x60, 0);
  DOWN ..... 0xE0E08679  <~>  IrSender.sendSamsung(0x707, 0x61, 0);
  LEFT ..... 0xE0E0A659  <~>  IrSender.sendSamsung(0x707, 0x65, 0);
  RIGHT .... 0xE0E046B9  <~>  IrSender.sendSamsung(0x707, 0x62, 0);
  OK ....... 0xE0E016E9  <~>  IrSender.sendSamsung(0x707, 0x68, 0);
  RETURN ... 0xE0E01AE5  <~>  IrSender.sendSamsung(0x707, 0x58, 0);
  EXIT ..... 0xE0E0B44B  <~>  IrSender.sendSamsung(0x707, 0x2D, 0);
  A ........ 0xE0E083EC  <~>  IrSender.sendSamsung(0x707, 0x6C, 0);
  B ........ 0xE0E028D7  <~>  IrSender.sendSamsung(0x707, 0x14, 0);
  C ........ 0xE0E0A857  <~>  IrSender.sendSamsung(0x707, 0x15, 0);
  D ........ 0xE0E06897  <~>  IrSender.sendSamsung(0x707, 0x16, 0);
  SETTINGS . 0xE0E058A7  <~>  IrSender.sendSamsung(0x707, 0x1A, 0);
  INFO ..... 0xE0E0F807  <~>  IrSender.sendSamsung(0x707, 0x1F, 0);
  AD/SUBT .. 0xE0E0A45B  <~>  IrSender.sendSamsung(0x707, 0x25, 0);
  PLAY ..... 0xE0E0E21D  <~>  IrSender.sendSamsung(0x707, 0x47, 0);
  PAUSE .... 0xE0E052AD  <~>  IrSender.sendSamsung(0x707, 0x4A, 0);
  STOP ..... 0xE0E0629D  <~>  IrSender.sendSamsung(0x707, 0x46, 0);
  |<< ...... 0xE0E0A25D  <~>  IrSender.sendSamsung(0x707, 0x45, 0);
  >>| ...... 0xE0E012ED  <~>  IrSender.sendSamsung(0x707, 0x48, 0);


                              PHILIPS

  POWER    -->  1000C          C           |    SOURCE   -->  10038          38
  1        -->  10001          1           |    2        -->  10002          2
  3        -->  10003          3           |    4        -->  10004          4
  5        -->  10005          5           |    6        -->  10006          6
  7        -->  10007          7           |    8        -->  10008          8
  9        -->  10009          9           |    0        -->  10000          0
  TTX/MIX  -->  1003C          3C          |    BACK     -->  1000A          A
  VOL+     -->  10010          10          |    VOL-     -->  10011          11
  CH+      -->  10020          20          |    CH-      -->  10021          21
  MUTE     -->  1000D          D           |    CH LIST  -->  100D2          D2
  NETFLIX  -->  10076          76          |    MULTIVI  -->  1005D          5D
  SMART    -->  100BE          BE          |    HOME     -->  10054          54
  GUIDE    -->  100CC          CC          |    UP       -->  10058          58
  DOWN     -->  10059          59          |    LEFT     -->  1005A          5A
  RIGHT    -->  1005B          5B          |    OK       -->  1005C          5C
  OPTOINNS -->  10040          40          |    EXIT     -->  1009F          9F
  A        -->  1006D          6D          |    B        -->  1006E          6E
  C        -->  1006F          6F          |    D        -->  10070          70
  SETTINGS -->  100BF          BF          |    INFO     -->  1000F          F
  AD/SUBT. -->  1004B          4B          |    PLAY     -->  1002C          2C
  PAUSE    -->  10030          30          |    STOP     -->  10031          31
  |<<      -->  1002B          2B          |    >>|      -->  10028          28
  STREAM   -->  100F5          F5          |    SEARCH   -->  100B4          B4
  RECORD   -->  10037          37

*/
