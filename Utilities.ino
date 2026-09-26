// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// IR SIGNAL FILTER
// ============================================================================
// Filters incoming IR signals and prepares valid commands for processing.
// Rejects unwanted protocols, invalid values, and signals received too quickly.

bool SIGNAL_FILTER() {

  // Ignore Samsung TV remotes and protocol 14.
  if (results.decode_type == SAMSUNG || results.decode_type == 14) {
    irrecv.resume();
    return false;
  }

  // Reject unsupported NEC signals, unknown protocols, or oversized values.
  if (results.decode_type == NEC || results.decode_type == UNKNOWN || results.value > 0xFFFFF) {
    led.flash(30, 30, 250, 35, 80, 2); // Purple error blink
    MY_TONE(3800, 10, 2);
    irrecv.resume();
    return false;
  }

  // Ignore signals received too soon after the previous processed signal.
  unsigned long now = millis();
  if ((now - LAST_PROCESSED_SIGNAL_TIME) < MIN_SIGNAL_INTERVAL) {
    irrecv.resume();
    return false;
  }

  // Use the command byte from the received IR value.
  FILTERED_CODE = (byte)(results.value & 0xFF);

  // Detect repeated commands, such as holding down a remote button.
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

  // Record the time of the accepted signal.
  LAST_IR_SIGNAL_TIME = now;
  LAST_PROCESSED_SIGNAL_TIME = now;

  // Reset feedback timers because a valid IR command was received.
  RESET_FEEDBACK_TIMERS();

  return true; // Valid signal ready
}

// ============================================================================
// IR CODE LOOKUP TABLE
// ============================================================================
// Maps Philips remote commands to the corresponding Samsung TV commands.
// Both tables use the same index for each command pair.
// Stored in Flash memory (PROGMEM) to save RAM.

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
// Send a Samsung IR command to the TV.
// The command can be supplied directly or read from the SAMSUNG_CMD lookup table.

void sendSamsungCode(uint8_t cmd, bool use_lookup = false) {
  uint8_t command = cmd;

  // When lookup mode is used, cmd is the index of the required Samsung command.
  if (use_lookup) {
    if (cmd < IR_TO_SAMSUNG_SIZE) {
      command = pgm_read_byte(&SAMSUNG_CMD[cmd]);
    } else {
      return;  // Invalid index
    }
  }

  // Stop receiving IR while transmitting the Samsung command.
  IrReceiver.stop();

  // Send one Samsung command to the TV.
  // 0x707 is the Samsung address used by this TV.
  //delay(MS(1));
  IrSender.sendSamsung(0x707, command, 1);

  // Allow the transmission to finish before restarting IR reception.
  delay(MS(1));
  IrReceiver.start();
}

// ============================================================================
// IR CODE DECODE FUNCTION
// ============================================================================
// Decodes one received IR signal and prints its protocol, address, command,
// and raw data to Serial Monitor.
// Used to identify and inspect IR codes when testing remote controls.

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
// Reads the audio peak and filters out the quiet audio range around the
// microphone's normal output level.

uint16_t analogReadFiltered() {

  uint16_t peak = analogRead(PIN_AUDIO);

  // Treat quiet audio between 480 and 520 as the baseline level.
  // Keep the returned value within 498-502 so small baseline variations
  // do not affect the audio detection logic.
  if (peak > 480 && peak < 520) {
    return constrain(peak, 498, 502);
  }

  // Return the actual peak when it is outside the quiet audio range.
  return peak;
}

// ============================================================================
// ANALOG INPUT READING
// ============================================================================
// Reads the Reaction and Deadband potentiometers at regular intervals.
// Deadband is only updated from the potentiometer when no manual adjustment
// has been made through the remote control.

AsyncDelay READ_ANALOG_DATADelay;  // Timer for reading analog values

void READ_ANALOG_DATA(bool READ_NOW = false) {

  auto now = millis();

  // Read the potentiometers when the interval has elapsed,
  // or immediately when a forced reading is requested.
  if (READ_NOW || READ_ANALOG_DATADelay.Reached(ANALOG_READ_DELAY_MS)) {

    int16_t rawDEADBAND = analogRead(PIN_DEADBAND);
    int16_t rawREACT    = analogRead(PIN_REACT);

    // Update DEADBAND from the potentiometer only when it has not
    // been manually adjusted through the remote control.
    if (DEADBAND_COUNTER == 0) {
      DEADBAND = map(rawDEADBAND, 0, 1023, 550, 20);
      DEADBAND = constrain(DEADBAND, 20, 550);
    }

    // Convert the Reaction potentiometer position to the reaction delay.
    uint8_t newREACTION_DELAY = map(rawREACT, 0, 1023, 32, 3);

    // Give audible feedback when the reaction setting changes.
    // PREVIOUS_REACTION_DELAY == -1 prevents a tone on the first reading.
    if (newREACTION_DELAY != PREVIOUS_REACTION_DELAY && PREVIOUS_REACTION_DELAY != -1) {
      MY_TONE(4200, 50, 1);
    }

    // Store the current reaction setting for comparison with the next reading.
    PREVIOUS_REACTION_DELAY = newREACTION_DELAY;
  }
}

// ============================================================================
// FUNCTION MODE TOGGLE
// ============================================================================
// Toggles between remote-control mode and RGB color-edit mode.
// Button 0xBE is used only for switching modes; all other codes are passed
// to the handler for the currently active mode.

void FUNCTION_MODE_TOGGLE() {

  // Toggle mode when button 0xBE is received.
  if (FILTERED_CODE == 0xBE) {
    REMOTE_CONTROL_MODE_ACTIVE = !REMOTE_CONTROL_MODE_ACTIVE;

    if (REMOTE_CONTROL_MODE_ACTIVE) {
      // Enter remote-control mode.
      // Reset feedback state and prepare the first RGB feedback display.
      MY_TONE(4000, 30, 1);
      FIRST_RGB_RUN = true;
      RESET_FEEDBACK_TIMERS();
      led.flash(220, 220, 220, 100, 100, 2);

    } else {
      // Enter RGB color-edit mode.
      // Disable normal serial and LED feedback so the selected color can be edited.
      SERIAL_DATA_PRINT_ACTIVE = false;
      FEEDBACK_LED_ACTIVE = false;
      MY_TONE(3000, 30, 2);

      // Show the currently selected color and initialize the edit color to red.
      PRINT_SELECTED_COLOR();
      led.setColor(50, 0, 0);
      info.println(F("50, 0, 0"));
      //delay(MS(50));
    }

    return;  // Mode-switch command has been handled; do not process it further.
  }

  // Process all other remote commands according to the active mode.
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
   ===================================================================================
                           TV MODEL: SAMSUNG UE43RU7102KXXH
   ===================================================================================

                  ACCESS SAMSUNG HIDDEN SERVICE MENU (FACTORY MODE)
              Requirement: Execute sequence while TV is in STANDBY (OFF)

   -----------------------------------------------------------------------------------
      STEP 1         STEP 2           STEP 3          STEP 4            RESULT
   -----------------------------------------------------------------------------------
       INFO     +   SETTINGS    +      MUTE      +     POWER     ==>  SERVICE MENU
    0xE0E0F807     0xE0E058A7       0xE0E0F00F       0xE0E06798

   -----------------------------------------------------------------------------------
    IR REMOTE LIBRARY FUNCTION CALLS (200ms DELAY BETWEEN STEPS):
   -----------------------------------------------------------------------------------
        1. IrSender.sendSamsung(0x707, 0x1F, 0);         // INFO     (0xE0E0F807)
        2. IrSender.sendSamsung(0x707, 0x1A, 0);         // SETTINGS (0xE0E058A7)
        3. IrSender.sendSamsung(0x707, 0x0F, 0);         // MUTE     (0xE0E0F00F)
        4. IrSender.sendSamsung(0x707, 0x02, 0);         // POWER    (0xE0E06798)

   ===================================================================================
                         SAMSUNG IR REMOTE KEY MAP DESCRIPTION
   ===================================================================================

       Protocol: Samsung 32-bit Pulse Distance (NEC Variant)

     RAW HEX   : The 32-bit decoded pulse sequence. Samsung uses a fixed 16-bit 
                 Customer ID (0xE0E0) followed by 8 bits of command data and 8 bits 
                 of bitwise-inverted command data for error checking.
                 Unlike Philips RC-5, Samsung IR does NOT use a toggle bit.

     <~>       : Denotes mapping between raw captured hex code and IRremote library 
                 transmission call parameters.

     SEND CMD  : IrSender.sendSamsung(Address, Command, Repeats)
                 - Address (0x707): Standard 16-bit Samsung TV Device Identifier.
                 - Command (0xXX) : Clean extracted 8-bit command payload (LSB-first).
                 - Repeats (0)    : Number of additional repeat frames (0 = single press).
   
   ===================================================================================

    BUTTON       RAW HEX               IR REMOTE TRANSMISSION CALL
   -----------------------------------------------------------------------------------
    POWER ..... 0xE0E06798   <~>   IrSender.sendSamsung(0x707, 0x02, 0);
    SOURCE .... 0xE0E0807F   <~>   IrSender.sendSamsung(0x707, 0x01, 0);
    1 ......... 0xE0E020DF   <~>   IrSender.sendSamsung(0x707, 0x04, 0);
    2 ......... 0xE0E0A05F   <~>   IrSender.sendSamsung(0x707, 0x05, 0);
    3 ......... 0xE0E0609F   <~>   IrSender.sendSamsung(0x707, 0x06, 0);
    4 ......... 0xE0E010EF   <~>   IrSender.sendSamsung(0x707, 0x08, 0);
    5 ......... 0xE0E0906F   <~>   IrSender.sendSamsung(0x707, 0x09, 0);
    6 ......... 0xE0E050AF   <~>   IrSender.sendSamsung(0x707, 0x0A, 0);
    7 ......... 0xE0E030CF   <~>   IrSender.sendSamsung(0x707, 0x0C, 0);
    8 ......... 0xE0E0B04F   <~>   IrSender.sendSamsung(0x707, 0x0D, 0);
    9 ......... 0xE0E0708F   <~>   IrSender.sendSamsung(0x707, 0x0E, 0);
    0 ......... 0xE0E08877   <~>   IrSender.sendSamsung(0x707, 0x11, 0);
    TTX/MIX ... 0xE0E034CB   <~>   IrSender.sendSamsung(0x707, 0x2C, 0);
    PRE-CH .... 0xE0E0C837   <~>   IrSender.sendSamsung(0x707, 0x13, 0);
    VOL+ ...... 0xE0E0E01F   <~>   IrSender.sendSamsung(0x707, 0x07, 0);
    VOL- ...... 0xE0E0D02F   <~>   IrSender.sendSamsung(0x707, 0x0B, 0);
    CH+ ....... 0xE0E048B7   <~>   IrSender.sendSamsung(0x707, 0x12, 0);
    CH- ....... 0xE0E008F7   <~>   IrSender.sendSamsung(0x707, 0x10, 0);
    MUTE ...... 0xE0E0F00F   <~>   IrSender.sendSamsung(0x707, 0x0F, 0);
    CH LIST ... 0xE0E0D629   <~>   IrSender.sendSamsung(0x707, 0x6B, 0);
    NETFLIX ... 0xE0E0CF30   <~>   IrSender.sendSamsung(0x707, 0xF3, 0);
    RAKUTEN ... 0xE0E03DC2   <~>   IrSender.sendSamsung(0x707, 0xBC, 0);
    PRIME ..... 0xE0E02FD0   <~>   IrSender.sendSamsung(0x707, 0xF4, 0);
    HDMI1 ..... 0xE0E0D12E   <~>   IrSender.sendSamsung(0x707, 0x8B, 0);
    TOOLS ..... 0xE0E0D22D   <~>   IrSender.sendSamsung(0x707, 0x4B, 0);
    MEDIA.P ... 0xE0E031CE   <~>   IrSender.sendSamsung(0x707, 0x8C, 0);
    HOME ...... 0xE0E09E61   <~>   IrSender.sendSamsung(0x707, 0x79, 0);
    GUIDE ..... 0xE0E0F20D   <~>   IrSender.sendSamsung(0x707, 0x4F, 0);
    UP ........ 0xE0E006F9   <~>   IrSender.sendSamsung(0x707, 0x60, 0);
    DOWN ...... 0xE0E08679   <~>   IrSender.sendSamsung(0x707, 0x61, 0);
    LEFT ...... 0xE0E0A659   <~>   IrSender.sendSamsung(0x707, 0x65, 0);
    RIGHT ..... 0xE0E046B9   <~>   IrSender.sendSamsung(0x707, 0x62, 0);
    OK ........ 0xE0E016E9   <~>   IrSender.sendSamsung(0x707, 0x68, 0);
    RETURN .... 0xE0E01AE5   <~>   IrSender.sendSamsung(0x707, 0x58, 0);
    EXIT ...... 0xE0E0B44B   <~>   IrSender.sendSamsung(0x707, 0x2D, 0);
    A ......... 0xE0E083EC   <~>   IrSender.sendSamsung(0x707, 0x6C, 0);
    B ......... 0xE0E028D7   <~>   IrSender.sendSamsung(0x707, 0x14, 0);
    C ......... 0xE0E0A857   <~>   IrSender.sendSamsung(0x707, 0x15, 0);
    D ......... 0xE0E06897   <~>   IrSender.sendSamsung(0x707, 0x16, 0);
    SETTINGS .. 0xE0E058A7   <~>   IrSender.sendSamsung(0x707, 0x1A, 0);
    INFO ...... 0xE0E0F807   <~>   IrSender.sendSamsung(0x707, 0x1F, 0);
    AD/SUBT ... 0xE0E0A45B   <~>   IrSender.sendSamsung(0x707, 0x25, 0);
    PLAY ...... 0xE0E0E21D   <~>   IrSender.sendSamsung(0x707, 0x47, 0);
    PAUSE ..... 0xE0E052AD   <~>   IrSender.sendSamsung(0x707, 0x4A, 0);
    STOP ...... 0xE0E0629D   <~>   IrSender.sendSamsung(0x707, 0x46, 0);
    |<< ....... 0xE0E0A25D   <~>   IrSender.sendSamsung(0x707, 0x45, 0);
    >>| ....... 0xE0E012ED   <~>   IrSender.sendSamsung(0x707, 0x48, 0);
   ===================================================================================



   ===================================================================================
                          PHILIPS IR REMOTE KEY MAP DESCRIPTION
   ===================================================================================
   FULL HEX : Full 17-bit raw frame with active toggle bit (0x10000). On repeated key 
              presses, the toggle bit flips between 0x1XXXX and 0x0XXXX.
    <~>     : Toggle bit state transition between consecutive button presses.
    SHORT   : Extracted command byte (rawCode & 0xFF), independent of toggle bit state.

    * NOTE:    These codes were captured using a WELL aftermarket replacement remote.
             Original Philips OEM remotes or other clone models may exhibit subtle
             differences in timing, bit depth, or default toggle bit state.
   ===================================================================================
   
    BUTTON      FULL HEX     SHORT      |      BUTTON      FULL HEX     SHORT 
   -------------------------------------+---------------------------------------------
    POWER ..... 0x1000C  <~>  0x0C      |      HOME ...... 0x10054  <~>  0x54
    SOURCE .... 0x10038  <~>  0x38      |      GUIDE ..... 0x100CC  <~>  0xCC
    1 ......... 0x10001  <~>  0x01      |      UP ........ 0x10058  <~>  0x58
    2 ......... 0x10002  <~>  0x02      |      DOWN ...... 0x10059  <~>  0x59
    3 ......... 0x10003  <~>  0x03      |      LEFT ...... 0x1005A  <~>  0x5A
    4 ......... 0x10004  <~>  0x04      |      RIGHT ..... 0x1005B  <~>  0x5B
    5 ......... 0x10005  <~>  0x05      |      OK ........ 0x1005C  <~>  0x5C
    6 ......... 0x10006  <~>  0x06      |      OPTIONS ... 0x10040  <~>  0x40
    7 ......... 0x10007  <~>  0x07      |      EXIT ...... 0x1009F  <~>  0x9F
    8 ......... 0x10008  <~>  0x08      |      A ......... 0x1006D  <~>  0x6D
    9 ......... 0x10009  <~>  0x09      |      B ......... 0x1006E  <~>  0x6E
    0 ......... 0x10000  <~>  0x00      |      C ......... 0x1006F  <~>  0x6F
    TTX/MIX     0x1003C  <~>  0x3C      |      D ......... 0x10070  <~>  0x70
    BACK        0x1000A  <~>  0x0A      |      SETTINGS .. 0x100BF  <~>  0xBF
    VOL+ ...... 0x10010  <~>  0x10      |      INFO ...... 0x1000F  <~>  0x0F
    VOL- ...... 0x10011  <~>  0x11      |      AD/SUBT ... 0x1004B  <~>  0x4B
    CH+ ....... 0x10020  <~>  0x20      |      PLAY ...... 0x1002C  <~>  0x2C
    CH- ....... 0x10021  <~>  0x21      |      PAUSE ..... 0x10030  <~>  0x30
    MUTE ...... 0x1000D  <~>  0x0D      |      STOP ...... 0x10031  <~>  0x31
    CH LIST ... 0x100D2  <~>  0xD2      |      |<< ....... 0x1002B  <~>  0x2B
    NETFLIX ... 0x10076  <~>  0x76      |      >>| ....... 0x10028  <~>  0x28
    MULTIVI ... 0x1005D  <~>  0x5D      |      STREAM .... 0x100F5  <~>  0xF5
    SMART ..... 0x100BE  <~>  0xBE      |      RECORD .... 0x10037  <~>  0x37
    SEARCH .... 0x100B4  <~>  0xB4      |
   ===================================================================================
  
*/
