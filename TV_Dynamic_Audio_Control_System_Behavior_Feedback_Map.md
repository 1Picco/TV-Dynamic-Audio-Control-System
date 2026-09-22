# Behavior & Feedback Map

This document describes the TV Dynamic Audio Control System from the user's point of view: what the system does, what feedback is given, why it is given, and what the different colours and tones mean.

The firmware uses the RGB LED and audible tones as a simple status language. The normal battery colour is also reused for ordinary IR confirmation, while special events have their own colours or tone patterns.

---

## 1. Normal Operation — Battery Status

The RGB LED's normal status represents the most recently measured battery voltage.

The battery voltage is checked every **10 minutes**. During the measurement, an approximately **100 mA load** is applied to the battery to obtain a more meaningful loaded-voltage reading. The voltage is measured under load, then the load is removed and the result is used for the battery-status feedback.

| Battery voltage | LED colour |
|---|---|
| **≥ 3.56 V** | Green |
| **3.50–3.559 V** | Orange/yellow |
| **< 3.50 V** | Red |

So, during normal operation:

```text
Every 10 minutes
      ↓
Apply ~100 mA load
      ↓
Measure loaded battery voltage
      ↓
Remove load
      ↓
Update battery voltage
      ↓
Display corresponding battery colour
```

The colour therefore gives an immediate visual indication of the latest measured battery condition.

---

## 2. Valid IR Command — Normal Feedback

When a valid IR command is received, the controller resets the feedback timers.

If the command has **no special feedback**, the LED gives a short indication using the current battery-voltage colour.

```text
Valid IR command
       ↓
Is there special feedback?
       │
       └── No
            ↓
     Battery-colour blink
```

For example:

- Green battery state → green feedback
- Medium battery state → orange/yellow feedback
- Low battery state → red feedback

For a repeating/held command, the LED can remain at the corresponding battery colour instead of giving the normal short blink.

After an event triggers feedback, the temporary indication is followed by the normal feedback timing:

```text
Feedback event
      ↓
Temporary LED feedback
      ↓
LED OFF for ~1 second
      ↓
Battery-voltage colour
      ↓
Remain for ~15 seconds
      ↓
LED OFF
```

A new valid activity can restart the feedback timers.

---

## 3. Rejected / Corrupted IR Signal

Not every received IR signal is an error.

Some signals are deliberately ignored by the filter and produce no user feedback.

For an invalid, rejected, or corrupted signal that reaches the error-feedback path:

```text
Invalid / corrupted IR
        ↓
Purple LED
        +
Two short tones
```

The current error tone is:

- **3800 Hz**
- **2 × 10 ms**

Human meaning:

> Something was received, but the controller could not accept it as a valid command.

---

## 4. Manual Volume Down

A valid VOL− command sends a Samsung VOL− command to the TV.

The feedback is:

```text
VOL− pressed
    │
    ├── Samsung VOL− sent
    │
    └── LED → Red
```

A normal press gives a short red indication.

A repeating/held command uses a continuing red indication.

Here, red means:

> Volume is being commanded down.

This is different from the normal battery-status meaning of red, where red indicates a low battery.

---

## 5. Manual Volume Up

A valid VOL+ command sends a Samsung VOL+ command to the TV.

```text
VOL+ pressed
    │
    ├── Samsung VOL+ sent
    │
    └── LED → Green
```

A normal press gives a short green indication.

A repeating/held command uses a continuing green indication.

Here, green means:

> Volume is being commanded up.

---

## 6. Automatic Loud-Audio Detection

When sustained loud audio is confirmed, the automatic volume-control system takes over.

```text
Loud audio confirmed
        ↓
LED → RED
        ↓
Send VOL−
        ↓
Continue monitoring
```

The controller sends VOL− commands while the loud-audio condition requires further reduction.

The red feedback means:

> The controller has detected excessive audio and is actively reducing the TV volume.

The automatic reduction is limited to the configured maximum number of volume steps.

---

## 7. Volume Reduced — Waiting for Silence

After the required volume reduction has been made, the controller does not immediately restore the original volume.

Instead, it waits for the loud audio to end.

```text
Loud audio
    ↓
VOL− commands
    ↓
Required reduction achieved
    ↓
WAIT FOR SILENCE
    ↓
Orange/yellow fading feedback
```

The orange/yellow feedback means:

> The TV volume has been reduced, and the controller is waiting to determine whether the audio has really become quiet.

The system requires the quiet condition to remain long enough to be confirmed.

---

## 8. Silence Detection

The first quiet reading is not enough to restore the volume.

```text
Quiet audio detected
        ↓
Silence confirmation begins
        │
        ├── Loud audio returns
        │       ↓
        │   Cancel silence detection
        │       ↓
        │   Return to loud-audio handling
        │
        └── Remains quiet
                ↓
        Silence eventually confirmed
```

The orange/yellow feedback therefore also communicates:

> The controller is still waiting for confirmed silence.

---

## 9. Silence Confirmed — Restore Volume

Once silence has been confirmed:

```text
Silence confirmed
       ↓
LED → GREEN
       ↓
VOL+
       ↓
VOL+
       ↓
...
       ↓
Original volume restored
```

The number of VOL+ commands corresponds to the number of automatic VOL− commands previously sent.

The green feedback means:

> The quiet period has been confirmed and the controller is restoring the TV to its original volume.

When restoration is complete, normal battery-voltage feedback resumes.

---

## 10. Loud Audio Returns During Restoration

If loud audio returns while the controller is restoring the volume:

```text
Restoring volume
      ↓
Loud audio returns
      ↓
STOP restoration
      ↓
RED feedback
      ↓
Return to loud-audio handling
```

The controller does not blindly finish restoring the original volume.

Human meaning:

> The audio became loud again while the volume was being restored, so restoration was stopped and the controller returned to volume reduction.

---

## 11. Deadband Adjustment

The deadband controls the range around the normal audio level that is treated as quiet.

### Deadband +

Increasing the deadband widens the accepted quiet range.

```text
DEADBAND +
     ↓
Deadband increased
     ↓
Blue/cyan feedback
     +
Confirmation tone
```

At the adjustment limit, a special limit-warning feedback is given.

### Deadband −

Decreasing the deadband narrows the accepted quiet range.

```text
DEADBAND −
     ↓
Deadband decreased
     ↓
Blue/purple feedback
     +
Confirmation tone
```

At the adjustment limit, a special limit-warning feedback is given.

The deadband feedback also uses the battery condition as part of the resulting colour, so the battery state remains represented while the adjustment direction is shown.

---

## 12. Automatic Volume Control ON/OFF

The dedicated automatic-volume button has explicit feedback.

### Enabled

```text
Automatic volume control ON
          ↓
Two green flashes
```

### Disabled

```text
Automatic volume control OFF
          ↓
Two red flashes
```

This gives immediate confirmation of whether the automatic audio-control function is enabled.

---

## 13. Charging Starts

Charging can start automatically when the measured battery voltage reaches the configured low-voltage threshold, or manually through the remote.

Once charging is active, charging has its own feedback behaviour instead of using the normal battery-status indication.

---

## 14. Charging Pause / Voltage Check

While charging, the controller periodically pauses charging to obtain a meaningful battery-voltage measurement.

```text
Charging
   ↓
Pause charging
   ↓
Wait for voltage stabilization
   ↓
Measure battery voltage
   ↓
Decision
   ├── Not full → Resume charging
   └── Full → Finish charging
```

The charging-pause event gives a white LED indication together with a two-tone ascending sequence.

Human meaning:

> Charging is temporarily paused so the controller can check the actual battery voltage.

---

## 15. Charging Voltage Feedback

During charging, the LED colour indicates how close the measured voltage is to the upper charging range.

| Measured voltage | LED colour |
|---|---|
| **Below ~4.07 V** | Red |
| **~4.07–4.119 V** | Orange |
| **≥ ~4.12 V** | Green |

The fade timing also changes with the voltage:

- Red → faster fade
- Orange → medium fade
- Green → slower fade

So the charging indication progresses visually:

```text
RED
 ↓
Battery still relatively far from the upper charging voltage

ORANGE
 ↓
Battery approaching the upper charging voltage

GREEN
 ↓
Battery near the charging limit
```

---

## 16. Charging Resumes

If the voltage check shows that the battery is not yet full:

```text
Voltage below full-charge threshold
              ↓
       Charging resumes
              ↓
       White feedback
              +
       Descending tone sequence
```

Human meaning:

> The battery still needs charging, so the charging cycle continues.

---

## 17. Charging Complete

If the measured voltage reaches the charging-stop threshold:

```text
Battery reaches full-charge threshold
              ↓
       Charging stops
              ↓
       Completion tone sequence
```

The completion tone is a descending sequence.

Human meaning:

> The battery has reached the charging limit and charging has finished.

---

## 18. Manual Charging ON/OFF

The charging command also provides audible confirmation.

### Charging ON

```text
Charging enabled
      ↓
Rising two-tone sequence
```

Human meaning:

> Charging has been enabled.

### Charging OFF

```text
Charging disabled
      ↓
Falling two-tone sequence
```

Human meaning:

> Charging has been disabled.

---

## 19. Exact Battery-Voltage Display

The dedicated battery-voltage command is different from the normal battery-colour indication.

Instead of simply showing a colour representing the battery condition, the actual voltage is encoded using different colours for different digits.

```text
Start
 ↓
WHITE
 ↓
Whole volts → GREEN
 ↓
Tenths → YELLOW
 ↓
Hundredths → RED
 ↓
WHITE
```

This allows the user to read the measured voltage from the LED sequence.

---

## 20. Reaction Setting Feedback

Changing the reaction potentiometer produces an audible confirmation when the effective reaction-delay value changes.

```text
Reaction potentiometer changed
          ↓
New reaction value detected
          ↓
Short 4200 Hz tone
```

Human meaning:

> The reaction setting has changed.

---

## 21. Tone Feedback — Audible Status Language

The tone system complements the RGB feedback.

Important examples include:

| Event | Audible feedback |
|---|---|
| Invalid IR | 2 × short high-frequency tones |
| Deadband adjustment | Short confirmation tone |
| Deadband limit | Special limit-warning sequence |
| Charging started | Rising two-tone sequence |
| Charging stopped | Falling two-tone sequence |
| Charging paused | Rising two-tone sequence |
| Charging resumed | Falling two-tone sequence |
| Charging complete | Three-tone descending sequence |
| Reaction setting changed | Short 4200 Hz tone |

The tone system can be disabled. When disabled, these audible feedback signals are suppressed.

---

## 22. Sleep / Wake

After the configured period without activity, the controller enters sleep mode to reduce power consumption.

```text
No activity
     ↓
Sleep timeout
     ↓
Sleep mode
     ↓
IR receiver remains available
     ↓
IR activity
     ↓
Wake
     ↓
Process command
     ↓
Normal operation
```

The same mechanism is also used when sleep is explicitly requested.

---

# The Core Automatic-Volume Feedback Sequence

The most important feedback sequence in the project is the automatic volume-control cycle:

```text
                 NORMAL OPERATION
                       │
                       │
                Loud audio confirmed
                       │
                       ▼
                   🔴 RED
                       │
                       │ VOL− commands
                       ▼
              VOLUME REDUCED
                       │
                       ▼
             🟠 ORANGE / YELLOW
                       │
                       │ waiting for
                       │ confirmed silence
                       ▼
              SILENCE CONFIRMED
                       │
                       ▼
                   🟢 GREEN
                       │
                       │ VOL+ commands
                       ▼
             ORIGINAL VOLUME
                RESTORED
                       │
                       ▼
              BATTERY COLOUR
                       │
                       ▼
                 NORMAL
```

The colours therefore have a clear human meaning during automatic audio control:

- **Red** — loud audio detected; volume is being reduced.
- **Orange/yellow** — volume has been reduced; waiting for confirmed silence.
- **Green** — silence confirmed; original volume is being restored.
- **Battery colour** — automatic action is finished; normal operation.

---

# Feedback Priority

The RGB LED has several possible reasons to display a colour. A feedback event can therefore temporarily replace the normal battery-colour indication.

The important principle is:

```text
Normal state
     ↓
Battery-voltage colour
     │
     ├── Valid IR → temporary command feedback
     ├── Invalid IR → purple + tone
     ├── Loud audio → red
     ├── Waiting for silence → orange/yellow
     ├── Restoring volume → green
     ├── Charging → charging colour
     └── Other special event → its dedicated feedback
```

After the temporary feedback has finished, the controller returns to the appropriate normal status indication.

The RGB LED is therefore not simply a battery indicator. It acts as a compact visual status display for the entire system, while the tone generator provides additional confirmation and warning information when a visual indication alone is not enough.
