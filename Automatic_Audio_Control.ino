// ============================================================================
// AUTOMATIC AUDIO CONTROL FUNCTIONS
// ============================================================================

// Separate timers for lower and restore
AsyncDelay lowerVolumeTimer,
           restoreVolumeTimer;

// ============================================================================
// Audio Processing Delay Timers

AsyncDelay FirstLoudFoundDelay,     // Timer for first loud audio detection
           DetectSilenceDelay,      // Timer for silence detection
           LoudConfirmedDelay,      // Timer for loud audio confirmation
           WaitForSilenceDelay,     // Timer for waiting for silence
           SilenceFoundDelay,       // Timer for silence confirmation
           AbortRestoreDelay;       // Timer for delaying after aborting restore

// Audio Processing Counters - Count various audio events
Counter CycleCounter,    // Counts audio processing cycles
        LoudCounter,     // Counts loud audio events
        QuietCounter,    // Counts quiet audio events
        SilenceCounter;  // Counts silence events

// State Machine Enumeration - Defines all possible states
enum State {
  Init,               // Initial state - reset all timers and counters and immediately goes to next state
  WaitForLoud,        // Waiting for loud audio to be detected
  LoudFound,          // Loud audio was found, counting to confirm
  LoudConfirmed,      // Loud audio confirmed, reducing volume
  WaitForSilence,     // Waiting for audio to become quiet
  SilenceFound,       // Silence was found, counting to confirm
  SilenceConfirmed,   // Silence confirmed, restoring volume
  AbortingRestore
};
State state = Init;  // Initialize state machine to Init state

// ============================================================================
// Restore TV volume to its original level.
// Sends VOL+ commands equal to the number of VOL- commands previously sent.
//
// Important:
// - This function is non-blocking and must be called repeatedly.
// - AUDIO_VOLUME_RESTORED becomes true ONLY when restore fully completes.
// - If loud audio happens again mid-restore, restore is aborted cleanly.
// ============================================================================

void RESTORE_VOLUME_TO_ORIGINAL_VALUE() {

  // Restore volume only when there are volume steps left to restore
  // and the required interval between Volume Up commands has elapsed.
  if (VOLUME_REDUCTION_COUNTER == 0 || !restoreVolumeTimer.Reached(VOLUME_COMMAND_INTERVAL)) return;

  // Show green feedback while restoring the original TV volume.
  // LED feedback is disabled while charging.
  if (!CHARGING_ACTIVE) led.setColor(0, 250, 0);

  // Send one Volume Up command to the Samsung TV.
  sendSamsungCode(0x07);
  restoreVolumeTimer.Reset();

  // One volume step has been restored.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    VOLUME_REDUCTION_COUNTER--;
  }

  // Show how many volume reduction steps are still left to restore.
  info.print(F("Volume + :"));
  info.println(VOLUME_REDUCTION_COUNTER);
}

// ============================================================================

void LOWER_THE_VOLUME() {

  // Lower the volume only while remote control mode is active,
  // fewer than 5 reduction steps have been made, and the required
  // interval between Volume Down commands has elapsed.
  if (VOLUME_REDUCTION_COUNTER >= 5 || !REMOTE_CONTROL_MODE_ACTIVE ||
      !lowerVolumeTimer.Reached(VOLUME_COMMAND_INTERVAL)) return;

  // Show red feedback while lowering the TV volume.
  // LED feedback is disabled while charging.
  if (!CHARGING_ACTIVE) led.setColor(250, 10, 0);

  // Send one Volume Down command to the Samsung TV.
  sendSamsungCode(0x0B);
  lowerVolumeTimer.Reset();

  // One additional volume reduction step has been made.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    VOLUME_REDUCTION_COUNTER++;
  }

  // Show how many volume reduction steps have been made.
  info.print(F("Volume - :"));
  info.println(VOLUME_REDUCTION_COUNTER);
}

// ============================================================================
// Audio Level Detection Functions - Determine if audio is loud or quiet

bool AUDIO_IS_LOUD(int audio) {
  // Check if audio level is outside the DEADBAND range around baseline
  return (audio > (AUDIO_BASELINE_HIGH)) || (audio < (AUDIO_BASELINE_LOW));  // Audio is loud if above or below threshold
}

bool AUDIO_IS_QUIET(int audio) {
  // Inclusive range prevents edge-case dead zones exactly at baseline limits
  return ((audio <= (AUDIO_BASELINE_HIGH)) && (audio >= (AUDIO_BASELINE_LOW)));
}

// ============================================================================
// Main Finite State Machine (FSM) - Core audio processing logic
// Processes audio input and controls TV volume based on audio levels

void AUDIO_CONTROL_MODE() {

  auto audio = analogReadFiltered();  // Read current audio level

  READ_ANALOG_DATA();        // Read analog values every 500ms
  PRINT_SERIAL_DATA(audio);  // Print debug data every 3ms

  switch (state) {
    case Init:  // Initial state - resets all timers and counters and immediately goes to next state

      // Reset all delay timers
      FirstLoudFoundDelay.Reset();
      DetectSilenceDelay.Reset();
      LoudConfirmedDelay.Reset();
      WaitForSilenceDelay.Reset();
      SilenceFoundDelay.Reset();
      lowerVolumeTimer.Reset();
      restoreVolumeTimer.Reset();
      AbortRestoreDelay.Reset();

      // Reset all counters
      CycleCounter.Reset();
      LoudCounter.Reset();
      QuietCounter.Reset();
      SilenceCounter.Reset();

      state = WaitForLoud;  // Transition to waiting for loud audio
      break;

    case WaitForLoud:  // Wait for a loud sound, the next state counts them
      if (DetectSilenceDelay.Reached(PREVIOUS_REACTION_DELAY))
        if (SilenceCounter.Reached(50))
          LoudCounter.Reset();

      if (AUDIO_IS_LOUD(audio))
      {
        DetectSilenceDelay.Reset();  // Reset delay for silence detection
        CycleCounter.Reset();        // Reset counter of silent cycles found
        state = LoudFound;           // Transition to loud found state
      }
      break;

    case LoudFound:  // A loud wave found from the previous state
      if (!FirstLoudFoundDelay.Reached(PREVIOUS_REACTION_DELAY))
        break;  // Exit if delay hasn't elapsed yet

      SilenceCounter.Reset();  // A loud wave was found so we reset silent counter

      if (CycleCounter.Reached(6))  // If it's the 6th cycle then..
      {
        if (LoudCounter.Reached(10))  // We increment the counter of loud waves, if it's 10 then..
        {
          state = LoudConfirmed;  // It's confirmed, loud music
          info.println(F("Lower the volume."));
          RESET_FEEDBACK_TIMERS();
        } else {
          state = WaitForLoud;  // Wait for another one
        }
      }
      break;

    case LoudConfirmed:
      if (AUDIO_IS_LOUD(audio)) {  // It's still loud

        LOWER_THE_VOLUME();

        CycleCounter.Reset();
        LoudConfirmedDelay.Reset();
      } else if (LoudConfirmedDelay.Reached(PREVIOUS_REACTION_DELAY))
      {
        if (CycleCounter.Reached(155))
        {
          state = WaitForSilence;  // Go to the state of waiting for a quiet part (dialog)
          if (VOLUME_REDUCTION_COUNTER > 0) {
            info.println(F("Volume lowered."));
          }
        }
      }
      break;

    case WaitForSilence:

      if (AUDIO_IS_LOUD(audio)) {
        state = LoudConfirmed;
        break;
      }
      else if (AUDIO_IS_QUIET(audio)) {
        state = SilenceFound;
      }
      break;

    case SilenceFound:
      if (!AUDIO_IS_QUIET(audio)) {     // Still not confirmed that it's a quiet part
        CycleCounter.Reset();           // Reset the counter of cycles the silence remained
        QuietCounter.Reset();           // Reset the counter of quiet waves
        state = WaitForSilence;         // Return to waiting for silence
      } else if (SilenceFoundDelay.Reached(PREVIOUS_REACTION_DELAY)) {
        if (CycleCounter.Reached(6)) {    // One silent wave, then...
          if (QuietCounter.Reached(25)) { // Increment counter of quiet waves

            state = SilenceConfirmed;   // No more music, silence confirmed
          }
        }
      }
      break;

    case SilenceConfirmed:
      if (AUDIO_IS_LOUD(audio)) {
        AbortRestoreDelay.Reset();  // Start a short delay
        state = AbortingRestore;    // New intermediate state
        info.println(F("Aborting volume restore!"));
        if (!CHARGING_ACTIVE) led.setColor(250, 0, 0);
        break;
      }

      RESTORE_VOLUME_TO_ORIGINAL_VALUE();

      if (VOLUME_REDUCTION_COUNTER == 0) {
        state = Init;
        info.println(F("Volume restored."));
        if (!CHARGING_ACTIVE) led.fadeOut(0, 250, 0, 30, 100);
        //RESET_FEEDBACK_TIMERS();
        FEEDBACK_MODE(BATTERY_VOLTAGE_STATUS);
      }
      break;

    case AbortingRestore:        // Pause before lowering again
      if (AbortRestoreDelay.Reached(VOLUME_COMMAND_INTERVAL)) {
        state = LoudConfirmed;   // Now proceed to lowering
      }
      break;
  }

  // Set the LED color based on system state
  // Prevent FEEDBACK_PATTERN from overriding solid LED feedback during active states
  if (state != SilenceConfirmed && state != AbortingRestore) {

    if (VOLUME_REDUCTION_COUNTER <= 4 && state == LoudConfirmed) {
      // Do nothing - keep the red LED from LOWER_THE_VOLUME()
    } else
      // If we not charging and volume was lowered warn by fading yellow color
      if (!CHARGING_ACTIVE && VOLUME_REDUCTION_COUNTER > 0) {
        FEEDBACK_LED_ACTIVE = true;
        FEEDBACK_PATTERN(25, 250, 10, 150, 0, 0, 0, 300, 0, 1000, 0, false, 0);
      } else
        // But if we charging, fade battery voltage by dynamic colors
        if (digitalRead(PIN_CHARGE_CONTROL) == HIGH)
          FEEDBACK_PATTERN(0, 0, 0, 0, 0, 0, 0, 150, 0, FEEDBACK_LED_DYNAMIC_FADE_OUT, 0, true, VOLTAGE_DURING_PAUSED_CHARGING);
  }
}

// ============================================================================
// Macro and state names for debug output

#define VarName(var) (#var)

char *StrStates[] = { VarName(Init),
                      VarName(WaitForLoud),
                      VarName(LoudFound),
                      VarName(LoudConfirmed),
                      VarName(WaitForSilence),
                      VarName(SilenceFound),
                      VarName(SilenceConfirmed),
                      VarName(AbortingRestore)
                    };

AsyncDelay PRINT_SERIAL_DATA_DELAY;

// ============================================================================
// Function to print debug data to serial monitor
void PRINT_SERIAL_DATA(uint16_t audio) {

  // Print diagnostic audio and state information when serial output is enabled.
  // Data is printed at most once every 3 ms to avoid excessive serial output.
  if (SERIAL_DATA_PRINT_ACTIVE && PRINT_SERIAL_DATA_DELAY.Reached(3)) {

    // Display the current reaction delay setting in milliseconds.
    Serial.print(F(" REACTION_DELAY:"));
    Serial.print(PREVIOUS_REACTION_DELAY * 10);

    Serial.print(F(" State:"));

    // Print the current state name.
    // If the state value is outside the valid array range, print UNKNOWN
    // instead of accessing an invalid array element.
    if (state >= 0 && state < (sizeof(StrStates) / sizeof(StrStates[0]))) {
      Serial.print(StrStates[state]);
    } else {
      Serial.print(F("UNKNOWN"));
    }

    // Display the current audio level.
    Serial.print(F(" Audio:"));
    Serial.print(audio);

    // Display the upper and lower audio baseline limits.
    Serial.print(F(" Baseline HIGH:"));
    Serial.print(AUDIO_BASELINE_HIGH);

    Serial.print(F(" Baseline LOW:"));
    Serial.print(AUDIO_BASELINE_LOW);

    // Display the current deadband used for audio level evaluation.
    Serial.print(F(" DEADBAND:"));
    Serial.println(DEADBAND);
  }
}
