/*
 * TV Dynamic Audio Control System
 * Copyright (C) 2026 Attila
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

// TV Dynamic Audio Control System
// This Arduino project automatically adjusts TV volume based on audio levels
// and provides IR remote control functionality for Samsung TVs

#include <util/atomic.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <IRremote.h>
#include <RGBLed.h>
#include <EEPROM.h>

// ============================================================================
// Global Serial information output switch
// Set DEBUG to 1 to enable info.print()/info.println() messages.
// Set DEBUG to 0 to disable them.
// ============================================================================
#define DEBUG 0

#if DEBUG

#define info Serial

#else
struct Info {
  template <typename... T> void print(T...) {}
  template <typename... T> void println(T...) {}
};

Info info;
#endif

// ============================================================================
// PIN DEFINITIONS - Hardware connections
// ============================================================================

const uint8_t PIN_AUDIO           = A0;  // Audio input pin - connects to microphone/audio sensor
const uint8_t PIN_REACT           = A1;  // Reactivity potentiometer pin - controls system responsiveness
const uint8_t PIN_DEADBAND        = A2;  // DEADBAND potentiometer pin - controls audio sensitivity threshold
const uint8_t PIN_BATTERY_VOLTAGE = A3;  // Battery VOLTAGE monitoring pin - VOLTAGE divider input
const uint8_t PIN_IR_RECEIVER     = 2;   // IR receiver pin - receives remote control signals
const uint8_t PIN_IR_LED          = 3;   // IR LED pin - sends commands to TV
const uint8_t PIN_CHARGE_CONTROL  = 4;   // Battery CHARGE_CONTROL control pin - enables/disables charging
const uint8_t RESET_PIN           = 5;   // Arduino RESET pin
const uint8_t PIN_BATTERY_LOAD    = 7;   // Battery load pin - applies load when measuring voltage during charging pause

// ============================================================================
// HARDWARE OBJECTS - IR and LED interfaces
// ============================================================================

RGBLed led(10, 6, 9, RGBLed::COMMON_ANODE);      // RGB LED object - pins 10,6,9 for R,G,B

IRrecv irrecv(PIN_IR_RECEIVER);                  // IR receiver object - listens for remote signals
decode_results results;                          // IR code results object - stores decoded IR data

// ============================================================================
// STATE DEFINITIONS - LED feedback modes
// ============================================================================
// LED feedback modes should NOT be #define macros.
// Using an enum prevents name collisions (especially with the word "IR") and
// makes debugging easier.
// ============================================================================

enum FEEDBACK_MODE : uint8_t {
  BLINK = 0,                // IR detected state - LED feedback when IR signal received
  BATTERY_VOLTAGE_STATUS,   // Battery VOLTAGE status - LED shows current battery level
  RGB_LED_OFF               // LED feedback turned off state
};

// ============================================================================
// HARDWARE CONSTANTS
// ============================================================================
// const float R1 = 3260.0f;    // 3.26K Resistor connected to battery positive terminal (VOLTAGE divider)
// const float R2 = 10000.0f;   // 10K Resistor connected to ground (VOLTAGE divider)

// ============================================================================
// AUDIO PROCESSING CONSTANTS
// ============================================================================
// Audio processing constants should NOT be #define macros.
// Using const makes them type-safe and easier to debug.
// ============================================================================

#define MAX_EXPAND_STEPS 6  // 8 steps * 40 = 320 max range
#define MAX_SHRINK_STEPS 3   // 3 steps * 40 = 120 min range (or whatever you want)

uint16_t AUDIO_BASELINE_HIGH        = 660;      // Upper threshold for loud audio detection
uint16_t AUDIO_BASELINE_LOW         = 340;      // Lower threshold for loud audio detection
const uint16_t ANALOG_READ_DELAY_MS = 500;      // Delay between analog readings

// ============================================================================
// EEPROM WEAR LEVELING - Spread writes to reduce wear
// ============================================================================
// EEPROM addresses and sizes should NOT be #define macros.
// Using const makes them type-safe and prevents macro name collisions.
// ============================================================================

const uint8_t EEPROM_JELLYFIN_SELECT_BASE = 0;  // Base address for JELLYFIN select (uses 0,1,2)
const uint8_t EEPROM_HDMI_SELECT_BASE     = 3;  // Base address for HDMI select (uses 3,4,5)
const uint8_t EEPROM_SERIAL_ACTIVE_BASE   = 6;  // Base address for serial active (uses 6,7,8)
const uint8_t EEPROM_CHARGING_ACTIVE_BASE = 9;  // Base address for charging active state (uses 9,10,11)
const uint8_t EEPROM_CYCLE_SIZE           = 3;  // Addresses per setting for wear leveling

// ============================================================================
// REGISTER BIT MANIPULATION MACROS
// ============================================================================
// Helper macros used to set or clear individual bits in AVR hardware registers
// for low-level control and configuration.
// ============================================================================

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

// ============================================================================
// TIMING CONSTANTS
// ============================================================================
// Battery Management Timing

#define MS(x)       ((unsigned long)(x))
#define SECONDS(x)  ((unsigned long)(x) * 1000UL)
#define MINUTES(x)  ((unsigned long)(x) * 60UL * 1000UL)
#define HOURS(x)    ((unsigned long)(x) * 60UL * 60UL * 1000UL)


const unsigned long TIME_TO_CHECK_BATTERY_CHARGE_STATE = MINUTES(15);
const unsigned long WAITE_FOR_VOLTAGE_STABILIZATION    = SECONDS(5);
const unsigned long TIME_TO_READ_BATTERY_VOLTAGE       = MINUTES(10);

const uint16_t STOP_CHARGING_VOLTAGE                   = 4150;  // Stop charging if voltage is  4.14V
const uint16_t START_CHARGING_VOLTAGE                  = 3450;  // Start charging if voltage is 3.45V

// ============================================================================
// BATTERY VARIABLES (all voltage values in millivolts)
// ============================================================================

static bool LOAD_TEST_ACTIVE         = false;
static bool VOLTAGE_CAPTURED         = false;
static unsigned long LOAD_TEST_TIMER = 0;

uint16_t RAW_VOLTAGE                         = 0;  // Raw battery voltage in mV
uint16_t VOLTAGE                             = 0;  // Battery voltage in mV
uint16_t VOLTAGE_DURING_PAUSED_CHARGING      = 0;  // Battery voltage during charging pause, in mV
unsigned long PREVIOUS_VOLTAGE_READING       = 0;
unsigned long LAST_TIME_VOLTAGE_READING      = 0;
unsigned long PAUSE_CHARGING_TO_READ_VOLTAGE = 0;

uint16_t READ_BATTERY_VOLTAGE();
void     BATTERY_CHARGE_CHECK();

const uint8_t VOLTAGE_EPSILON_MV        = 10;    // Millivolt comparison tolerance
static bool FIRST_VOLTAGE_READING_TAKEN = true;
static bool CHARGING_ACTIVE             = false;
static bool CHARGING_PAUSED             = false;
static bool LOW_VOLTAGE_START_CHARGING  = false;
static bool CHARGING_CYCLE_COMPLETE     = false;
bool BATTERY_MEASUREMENT_ACTIVE         = false;

// ============================================================================
// LED CONTROL VARIABLES
// ============================================================================

byte LED_BRIGHTNESS = 0;
byte RED_VALUE      = 0, GREEN_VALUE       = 0, BLUE_VALUE     = 0;
byte RED_INTENSITY  = 0, GREEN_INTENSITY   = 0, BLUE_INTENSITY = 0;

unsigned int FEEDBACK_LED_DYNAMIC_FADE_OUT = 0;
unsigned long PREVIOUS_MILLIS              = 0;

unsigned long RGB_FEEDBACK_START_TIME             = 0;
unsigned long RGB_FEEDBACK_ACTIVE_TIME            = 0;
unsigned long FEEDBACK_OFF_START_TIME             = 0;
unsigned long FEEDBACK_OFF_AFTER_IR_DETECTED_TIME = 0;

static bool FEEDBACK_LED_ACTIVE    = false;
static bool FEEDBACK_OFF_ACTIVE    = false;

// ============================================================================
// RGB MODE VARIABLES
// ============================================================================

static char INPUT_BUFFER[4]            = "";
static byte SELECTED_COLOR             = 0;
static bool AWAITING_CONFIRMATION      = false;
static bool FIRST_RGB_RUN              = true;
static bool REMOTE_CONTROL_MODE_ACTIVE = true;

// ============================================================================
// AUDIO CONTROL VARIABLES
// ============================================================================

int16_t DEADBAND                = 0;
int8_t DEADBAND_COUNTER         = 0;
int8_t VOLUME_REDUCTION_COUNTER = 0;
int8_t REACTION_DELAY           = 0;
int8_t PREVIOUS_REACTION_DELAY  = -1;

void RESTORE_VOLUME_TO_ORIGINAL_VALUE();
void LOWER_THE_VOLUME();
bool AUDIO_IS_LOUD(int audio);
bool AUDIO_IS_QUIET(int audio);
void AUDIO_CONTROL_MODE();
void PRINT_SERIAL_DATA();

volatile bool VOLUME_CONTROL_ACTIVE         = true;
bool stillLoudAudio                         = true;
const unsigned long VOLUME_COMMAND_INTERVAL = 200;  // 200 ms between commands

// ============================================================================
// IR CONTROL VARIABLES
// ============================================================================

uint8_t FILTERED_CODE                    = 0;
uint8_t LAST_IR_CODE                     = 0;
uint8_t CONSECUTIVE_IR_CODE_COUNT        = 0;
unsigned long LAST_IR_SIGNAL_TIME        = 0;
unsigned long LAST_PROCESSED_SIGNAL_TIME = 0;
const uint8_t MIN_SIGNAL_INTERVAL        = 15;
static bool IR_CODE_REPEATING            = false;

// ============================================================================
// SYSTEM STATE FLAGS
// ============================================================================

static bool SERIAL_DATA_PRINT_ACTIVE = false;
static bool SELECT_JELLYFIN          = false;
static bool SELECT_HDMI              = false;
bool MY_TONE_ENABLED                 = true;

// ============================================================================
// POWER MANAGEMENT VARIABLES
// ============================================================================

uint32_t GO_TO_SLEEP_START_TIME      = 0;
static bool ARDUINO_WAITING_TO_SLEEP = false;
volatile bool WAKE_UP_TRIGGERED      = false;
volatile bool STAY_AWAKE             = false;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================
// Utility function declarations

void RESET_FEEDBACK_TIMERS();
void READ_ANALOG_DATA(bool READ_NOW = false);

void MY_TONE(uint16_t frequency, uint8_t duration, uint8_t beepCount);
void MY_TONE(uint16_t frequency, int16_t frequencyStep, uint8_t duration, uint8_t beepCount);
void MY_TONE_REPEAT(uint16_t frequency, uint8_t duration, uint8_t beepCount, unsigned long interval);

// ============================================================================
// LED feedback function declarations

void FEEDBACK_ON( uint32_t MINUTES = 0, uint32_t SECONDS = 0);
void FEEDBACK_OFF(uint32_t MINUTES = 0, uint32_t SECONDS = 0);

void FEEDBACK_PATTERN(byte minRed,   byte maxRed,
                      byte minGreen, byte maxGreen,
                      byte minBlue,  byte maxBlue,
                      byte blinkCount,
                      uint16_t fadeInTime, uint16_t onTime,
                      uint16_t fadeOutTime, uint16_t offTime,
                      bool USE_CHARGE_LOGIC, uint16_t voltagePausedCharging_mV);

// ============================================================================
// Remote control function declarations

bool SIGNAL_FILTER();
void RGB_CODE_EDIT();
void FUNCTION_TOGGLE();
void REMOTE_CONTROL_MODE();
void FUNCTION_MODE_TOGGLE();
void sendSamsungCode(uint8_t cmd, bool use_lookup = false);

// ============================================================================
// Power management function declarations

void TIME_TO_WAKE_UP();
void GO_TO_SLEEP(bool SLEEP_NOW = false, uint32_t HOURS = 0, uint32_t MINUTES = 0, uint32_t SECONDS = 0);

// ============================================================================
// AsyncDelay Class - Non-blocking delay timer for state machine timing
// Provides asynchronous delay functionality without blocking the main loop

class AsyncDelay {
  private:
    unsigned long startedAt;  // Timestamp when delay was started
    bool running;             // Flag indicating if delay is currently active

  public:
    AsyncDelay() {
      Reset();  // Initialize delay timer
    }

    void Reset() {
      startedAt = 0;  // Reset start time
      running = false;  // Mark as not running
    }

    bool Reached(unsigned long ms) {
      if (!running) {
        startedAt = millis();  // Start timing when first called
        running = true;         // Mark as running
      }
      auto result = (millis() >= startedAt + ms);  // Check if delay time has elapsed
      if (result)
        Reset();  // Reset timer when delay is complete
      return result;  // Return true if delay time has been reached
    }
};

// ============================================================================
// Counter Class - Event counter for state machine logic
// Counts events and resets when target value is reached

class Counter {
  private:
    uint8_t currentValue;  // Current count value

  public:
    Counter() {
      Reset();  // Initialize counter
    }
    void Reset() {
      currentValue = 0;  // Reset counter to zero
    }

    bool Reached(uint8_t value) {
      auto result = (++currentValue >= value);  // Increment and check if target reached
      if (result)
        Reset();  // Reset counter when target is reached
      return result;  // Return true if target value has been reached
    }
};

// ============================================================================
// Function pointer to perform Arduino reset when invoked.

void arduinoReset() {
  digitalWrite(RESET_PIN, LOW);  // Triggers Arduino reset
}

// ============================================================================
// Update voltage and show battery color when finished
void REQUEST_BATTERY_VOLTAGE_READING() {
  // Force the 10-minute interval check to pass instantly
  PREVIOUS_VOLTAGE_READING = millis() - TIME_TO_READ_BATTERY_VOLTAGE;
  // (Optional) If you were relying on this flag, you can set it too
  FIRST_VOLTAGE_READING_TAKEN = true;
}

// ============================================================================
// Setup function - Initialize all hardware and system variables
// Called once at startup to configure pins, load settings, and prepare system

// ============================================================================
// Initialize hardware, system state and persistent settings
// ============================================================================
void setup() {

  // Initial LED state
  led.off();

  // Pin configuration
  pinMode(PIN_CHARGE_CONTROL, OUTPUT);   // Set CHARGE_CONTROL pin as output for battery charging control
  pinMode(PIN_BATTERY_VOLTAGE, INPUT);   // Set BATTERY_VOLTAGE as input for VOLTAGE monitoring
  pinMode(PIN_BATTERY_LOAD, OUTPUT);     // Set BATTERY_LOAD pin as output for applying load during voltage measurement
  pinMode(RESET_PIN, INPUT_PULLUP);      // Pin HIGH by default via internal pull-up
  pinMode(PIN_DEADBAND, INPUT);           // Set PIN_DEADBAND as input for DEADBAND potentiometer
  pinMode(RESET_PIN, OUTPUT);             // Then make it output - stays HIGH
  pinMode(PIN_REACT, INPUT);              // Set PIN_REACT as input for reactivity potentiometer
  pinMode(PIN_AUDIO, INPUT);              // Set PIN_AUDIO as input for audio sensor

  digitalWrite(PIN_BATTERY_LOAD, LOW);   // Keep load off initially

  // Serial communication and ADC configuration
  Serial.begin(115200);

  // ADC prescaler = 32
  // 8 MHz / 32 = 250 kHz ADC clock
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);

  // Initialize timing variables
  PREVIOUS_VOLTAGE_READING = millis();
  LAST_TIME_VOLTAGE_READING = millis();
  PAUSE_CHARGING_TO_READ_VOLTAGE = millis();
  FIRST_VOLTAGE_READING_TAKEN = true;

  // IR communication initialization
  IrSender.begin(PIN_IR_LED, DISABLE_LED_FEEDBACK);
  IrReceiver.start();

  // Restore persistent settings
  RESTORE_EEPROM_SETTINGS();
  RESTORE_CHARGING_STATE();

  // Initialize analog parameters
  DEADBAND = map(analogRead(PIN_DEADBAND), 0, 1023, 550, 20);

  // Final startup initialization
  WAKE_UP_TRIGGERED = false;
  READ_ANALOG_DATA(true);        // Force initial analog reading
}

// ============================================================================
// Restore persistent settings from EEPROM
// ============================================================================
void RESTORE_EEPROM_SETTINGS() {

  SELECT_JELLYFIN = eepromWearLevelRead(EEPROM_JELLYFIN_SELECT_BASE);
  SELECT_HDMI = eepromWearLevelRead(EEPROM_HDMI_SELECT_BASE);
  SERIAL_DATA_PRINT_ACTIVE = eepromWearLevelRead(EEPROM_SERIAL_ACTIVE_BASE);
}

// ============================================================================
// Restore charging state from EEPROM
// ============================================================================
void RESTORE_CHARGING_STATE() {

  bool chargingWasActive =
    eepromWearLevelRead(EEPROM_CHARGING_ACTIVE_BASE);

  if (chargingWasActive) {

    digitalWrite(PIN_CHARGE_CONTROL, HIGH);

    FIRST_VOLTAGE_READING_TAKEN = false;
    LOW_VOLTAGE_START_CHARGING  = true;
    CHARGING_CYCLE_COMPLETE     = false;
    CHARGING_ACTIVE             = true;

  } else {

    digitalWrite(PIN_CHARGE_CONTROL, LOW);

    FIRST_VOLTAGE_READING_TAKEN = true;
    LOW_VOLTAGE_START_CHARGING  = false;
    CHARGING_CYCLE_COMPLETE     = true;
    CHARGING_ACTIVE             = false;
  }
}

// ============================================================================
// Main loop function - Core system operation
// ============================================================================
void loop() {

  HANDLE_AUDIO_CONTROL();            // Automatic audio control or volume-control feedback
  BATTERY_CHARGE_CHECK();            // System maintenance
  HANDLE_FEEDBACK();                 // Feedback and deadband handling
  HANDLE_STATUS();                   // RGB status and periodic serial-data indication
  HANDLE_IR();                       // IR reception and repeat timeout handling
  //COUNTDOWN();                     // Countdown till next voltae reading

  GO_TO_SLEEP(false, HOURS(1));      // Put Arduino to sleep after 1 hour of inactivity
}

// ============================================================================
// Countdown till next voltae reading
// ============================================================================
void COUNTDOWN() {
  // 1. Idle countdown (between regular 10-min battery checks)
  if (!LOAD_TEST_ACTIVE && !CHARGING_ACTIVE) {
    PRINT_COUNTDOWN();
  }

  // 2. Charging countdown (between 15-min charge pause checks)
  if (CHARGING_ACTIVE && !CHARGING_PAUSED) {
    PRINT_CHARGE_CHECK_COUNTDOWN();
  }
}

// ============================================================================
// Handle feedback LED and deadband indication
// ============================================================================

void HANDLE_FEEDBACK() {

  // 1. Run deadband pulse pattern if active
  if (DEADBAND_COUNTER != 0) DEADBAND_FEEDBACK();

  // 2. Handle Wake-up trigger reset
  if (WAKE_UP_TRIGGERED) {
    WAKE_UP_TRIGGERED = false;    // Clear wake-up flag
  }
  // 3. Feedback timer active only if not charging
  if (!CHARGING_ACTIVE && VOLUME_CONTROL_ACTIVE) {
    FEEDBACK_ON(SECONDS(15));
    FEEDBACK_OFF(SECONDS(1));
  }
}

// ============================================================================
// Handle IR reception, filtering and repeat timeout
// ============================================================================
void HANDLE_IR() {

  if (irrecv.decode(&results)) {

    if (!SIGNAL_FILTER()) {
      irrecv.resume();
      return;
    }

    FUNCTION_MODE_TOGGLE();
    irrecv.resume();
    delay(MS(5));
  }

  // Check for IR repeat timeout (2 second)
  //if (IR_CODE_REPEATING && (millis() - LAST_IR_SIGNAL_TIME) > SECONDS(2)) {
  if ((millis() - LAST_IR_SIGNAL_TIME) > SECONDS(2)) {
    // Timeout - user released the button
    IR_CODE_REPEATING = false;
    CONSECUTIVE_IR_CODE_COUNT = 1;
    LAST_IR_CODE = 0;               // Reset last IR code
    LAST_IR_SIGNAL_TIME = 0;        // Reset IR signal timer
  }
}

// ============================================================================
// Handle automatic audio control and volume-control feedback
// ============================================================================
void HANDLE_AUDIO_CONTROL() {

  if (!VOLUME_CONTROL_ACTIVE)
    // Microphone listening disabled - show volume-control feedback pattern
    FEEDBACK_PATTERN(30, 220, 0, 220, 0, 220, 0, MS(300), MS(300), MS(500), SECONDS(2), false, 0);
  else
    // Microphone listening enabled - automatic audio adjustment
    AUDIO_CONTROL_MODE();
}

// ============================================================================
// Handle RGB status indication and periodic serial-data tone
// ============================================================================
void HANDLE_STATUS() {

  if (!REMOTE_CONTROL_MODE_ACTIVE) {
    if (!FIRST_RGB_RUN) {
      led.setColor(RED_INTENSITY, GREEN_INTENSITY, BLUE_INTENSITY);
    }
  } else if (SERIAL_DATA_PRINT_ACTIVE) {
    // Beep every 5 minutes while serial-data indication is active
    MY_TONE_REPEAT(3800, 20, 2, MINUTES(5));
  }
}
