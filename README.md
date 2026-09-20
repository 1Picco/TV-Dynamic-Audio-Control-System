# TV Dynamic Audio Control System

*This project is still under active development, and any contributions or help are welcome!*

An Arduino-based **automatic TV-audio controller and IR remote bridge**.

The system monitors audio from a TV or other audio source and automatically lowers the TV volume when sustained loud audio is detected, then restores the original volume when the audio becomes quiet again.

It also works as an **IR remote translator**: an existing remote control can be used to control the project, while the project translates selected commands into Samsung IR commands for the TV. This makes it possible to use a remote from one manufacturer to control a Samsung TV, while also assigning additional remote buttons to functions of the project itself.

The original hardware is built around an **Arduino Pro Mini 3.3 V / 8 MHz / ATmega328P**, a single protected 18650 Li-ion cell, an electret microphone with an LM358-based peak/noise detector, an IR receiver/transmitter, and a common-anode RGB LED.

---

## Main Features

### Automatic TV volume control

The system continuously monitors the output of an audio detection circuit connected to the Arduino.

When loud audio is detected and confirmed, the controller sends Samsung **Volume Down** commands to the TV.

When the audio becomes quiet again and the quiet condition is confirmed, it sends the required number of **Volume Up** commands to restore the volume to the level it had before the automatic reduction.

The automatic volume reduction is limited to a configurable number of volume steps.

The algorithm uses a state machine with separate detection and confirmation stages for:

* loud audio detection
* loud audio confirmation
* volume reduction
* waiting for silence
* silence detection
* silence confirmation
* volume restoration
* interruption of restoration when loud audio returns

The audio processing is intentionally non-blocking during normal operation and uses timers and counters rather than long blocking delays.

---

## Audio Detection

The microphone is an **electret microphone**.

An **LM358** circuit is used primarily as a **peak/noise detector**, rather than as a conventional audio amplifier.

The LM358 output is connected directly to the Arduino's analog audio input.

The software interprets the analog signal relative to two thresholds:

* `AUDIO_BASELINE_HIGH`
* `AUDIO_BASELINE_LOW`

Audio outside this range is treated as loud, while audio inside the range is treated as quiet.

### Reaction control

A 10 kΩ potentiometer controls the **reaction setting**.

It determines how quickly the system reacts after detecting loud audio and how quickly it reacts when audio becomes quiet again.

### Deadband control

A second 10 kΩ potentiometer controls the **deadband**.

The deadband determines the sensitivity baseline used for deciding whether the detected audio represents silence or significant audio activity.

The deadband can also be adjusted from the remote control. The RGB LED provides visual feedback for positive and negative deadband adjustments.

---

## IR Remote Control

The project has two IR functions:

1. **Receive commands from a remote control**
2. **Transmit Samsung IR commands to the TV**

The receiver is connected using the standard IR receiver configuration.

The IR transmitter uses an IR LED driven by an **NPN transistor**, with:

* 1 kΩ transistor base resistor
* 47 Ω IR LED series resistor

The project was developed around the Arduino-IRremote 3.x API.

### Remote translation

The system can receive commands from another manufacturer's remote and translate them into Samsung commands.

The original implementation uses a **Philips remote** as the user interface.

For example:

| Philips remote       | Project function / Samsung command    |
| -------------------- | ------------------------------------- |
| VOL−                 | Samsung VOL−                          |
| VOL+                 | Samsung VOL+                          |
| POWER                | Samsung POWER                         |
| OK                   | Samsung OK                            |
| PLAY                 | Samsung PLAY                          |
| PAUSE                | Samsung PAUSE                         |
| Other mapped buttons | Project-specific or Samsung functions |

The project therefore acts partly as a **Samsung remote clone**.

The IR command table can be extended to support additional remote buttons and Samsung TV functions.

The current implementation also contains a Samsung command reference and Philips command reference in `Utilities.ino`.

---

## Project-Specific Remote Functions

Several remote buttons are used for functions that belong to the controller rather than being forwarded directly to the TV.

These include:

* automatic volume control enable/disable
* deadband adjustment
* charging start/stop
* controller reset
* sleep
* battery-voltage display
* tone enable/disable
* serial-data output enable/disable
* Jellyfin selection
* HDMI selection
* other mapped TV commands

The exact button assignments are defined in `Remote_Control.ino`.

---

## RGB Status Feedback

A single **common-anode RGB LED** provides visual feedback.

The LED is used for several different purposes, including:

* IR command feedback
* battery status
* deadband adjustment
* charging status
* charging-voltage indication
* automatic volume reduction/restoration indication
* sleep/wake indication
* battery-voltage display

The RGB feedback system uses both simple flashes/fades and a non-blocking RGB pattern state machine.

Battery voltage is represented visually using different colors and fade patterns.

### Battery voltage display

A dedicated remote command can display the measured battery voltage using the RGB LED.

The voltage is displayed as three digits:

* whole volts — green
* tenths — yellow
* hundredths — red

White sequences are used to mark the beginning and end of the display.

---

## Modified RGBLed Library

This project includes a modified version of the **RGBLed Arduino library** originally created by **wilmouths**.

## License

This project is licensed under the **GNU General Public License v3.0** (GPL-3.0) — see the [LICENSE](LICENSE) file for details.

### Included Third-Party Libraries

* **RGBLed**: Based on the original library by [wilmouths](https://github.com/wilmouths/RGBLed.git) under **GNU GPLv3**. Includes custom performance and memory optimizations for AVR microcontrollers.
* **Arduino-IRremote**: Used under its respective open-source license.


### Modifications

#### 1. RGB color constants changed to `uint8_t` and `const`

The predefined RGB colors were changed from mutable `int` arrays to constant 8-bit values.

This better represents RGB data, which is inherently limited to the range `0–255`, and avoids allocating unnecessary storage for larger integer types.

#### 2. RGB and pin parameters changed to `uint8_t`

RGB values, brightness values, and LED pin parameters were changed from `int` to `uint8_t` where appropriate.

This more accurately represents the actual hardware values and reduces memory requirements on the ATmega328P.

#### 3. Brightness calculations changed to integer arithmetic

Brightness calculations were changed to use integer arithmetic instead of unnecessary floating-point operations.

The RGB LED only requires integer values between `0` and `255`, so floating-point precision provides no practical benefit here.

#### 4. Floating-point calculations removed from `fade()`

The original fade calculations used floating-point arithmetic.

The modified implementation performs the fade using integer calculations instead.

This reduces processing overhead and avoids pulling floating-point operations into a small AVR microcontroller application.

#### 5. `flash()` supports multiple flashes

The modified library adds a `count` parameter to the flash functions.

Examples:

```cpp
led.flash(RGBLed::RED, 200);
led.flash(RGBLed::RED, 200, 3);

led.flash(150, 140, 0, 30);
led.flash(150, 140, 0, 30, 100);
led.flash(150, 140, 0, 30, 100, 3);
```

The extended form allows separate control of:

* red
* green
* blue
* LED ON duration
* LED OFF duration
* number of flashes

For example:

```cpp
led.flash(150, 140, 0, 30, 0, 0);
           │    │   │   │   │  │
           │    │   │   │   │  └── count = 0 → automatically changed to 1
           │    │   │   │   └───── duration = 0 ms OFF time
           │    │   │   └───────── onDuration = 30 ms ON time
           │    │   └───────────── blue = 0
           │    └───────────────── green = 140
           └────────────────────── red = 150
```

This produces one flash using RGB `(150, 140, 0)`, with the LED ON for 30 ms and no OFF delay.

A `count` value below 1 is automatically treated as one flash.

#### 6. Flash timing changed to `uint16_t`

Flash timing values were changed to `uint16_t`.

This provides a suitable range for millisecond timing while using an appropriate unsigned integer type for the application.

#### 7. Fade loop modified for unsigned counters

The fade loops were adjusted to use unsigned counters where appropriate.

This better matches the non-negative nature of RGB brightness and timing values.

#### 8. `crossFade()` converted from floating point to fixed-point/integer arithmetic

The original `crossFade()` implementation used floating-point calculations.

The modified implementation uses integer/fixed-point calculations instead.

This significantly reduces floating-point processing on the ATmega328P while retaining the intended visual transition.

#### 9. `crossFade()` protected against `steps = 0`

The modified implementation explicitly protects against a zero-step fade.

This prevents invalid calculations such as division by zero.

#### 10. `gradient()` converted from floating point to integer arithmetic

The gradient calculations were also converted from floating-point calculations to integer arithmetic.

Again, this is sufficient because the final RGB output is limited to 8-bit values.

#### 11. Additional RGB range protection

The modified implementation includes explicit protection against RGB values exceeding the valid `0–255` range during calculations.

### Result

The modified RGBLed library retains the original library's basic functionality while being better suited to this project's resource-constrained AVR environment.

The main goals of the modifications are:

* lower memory usage
* less unnecessary computation
* elimination of floating-point calculations where they provide no useful benefit
* safer handling of edge cases
* additional flash-count functionality
* compatibility with the project's RGB feedback system

The modifications are project-specific and are maintained as part of this repository.

---

## Battery

The controller is powered by **one 18650 Li-ion cell**.

The charging circuit is based on a **TP4056 charging board with protection circuitry**. The protection circuitry on the TP4056 board provides the battery protection.

Charging is controlled by the Arduino through the TP4056 **enable/control input**.

The Arduino can therefore:

* start charging automatically when the battery is low
* stop charging when the battery reaches the configured full-charge threshold
* temporarily disable charging
* measure battery voltage while charging is paused
* manually toggle charging from the remote
* remember the charging state through EEPROM

### Why the system is battery powered

The battery is not used simply because the project is intended to be portable.

During development, several different **5 V phone chargers/power supplies were tested as direct power sources for the Arduino**.

The tested power supplies introduced enough electrical/digital noise into the system to cause practical problems, including:

* unstable Arduino behavior
* false audio loudness triggers
* interference with IR decoding
* interference with IR transmission

The audio detector and IR system are particularly sensitive to this type of interference.

Using the 18650 battery as the actual power source provides a much cleaner and more stable supply for the controller.

The phone charger is therefore used **only to charge the battery**. The Arduino continues to operate from the battery while charging is taking place.

This arrangement provides a stable power input while still allowing the system to remain connected to a charger for automatic battery charging.

### Battery voltage measurement

Battery voltage is measured through a resistor divider connected to analog input `A3`.

For a more meaningful battery measurement, the Arduino switches an approximately **100 mA load across the battery** using a MOSFET.

The voltage is measured while the load is active.

The load is controlled by the `PIN_BATTERY_LOAD` output.

The normal measurement cycle:

1. Enable the approximately 100 mA battery load.
2. Allow the battery/load voltage to settle.
3. Measure the battery voltage.
4. Remove the load.
5. Store the measured voltage for use by the rest of the system.

Normal battery measurements are performed periodically rather than continuously.

### Charging control

The current firmware uses:

* approximately **3.45 V** as the low-voltage charging-start threshold
* approximately **4.15 V** as the charging-stop threshold

During charging, the controller periodically pauses charging, allows the battery voltage to stabilize, measures the battery voltage, and decides whether charging should resume or finish.

The RGB LED and tone feedback are also used to indicate charging-related events.

---

## Power Management

The controller includes a low-power sleep system.

After a configurable period of inactivity, the ATmega328P enters:

`SLEEP_MODE_PWR_DOWN`

Unused peripherals are disabled before sleep to reduce power consumption.

The IR receiver is used as the wake-up source.

After waking, the controller temporarily restores the required peripherals, processes the incoming IR signal, and then returns to normal operation.

The sleep system also supports an immediate sleep command from the remote.

A physical wake/reset mechanism is provided through the IR system and hardware reset circuitry.

---

## Hardware Reset

The Arduino reset line is controlled through a transistor.

The transistor physically shorts the reset-button connection on the Arduino board when activated.

This provides a **real hardware reset of the Arduino**, rather than merely resetting selected software variables or reinitializing individual subsystems.

A dedicated Arduino output controls this reset circuit.

---

## Persistent Settings

Several controller settings are stored in EEPROM so they survive a power cycle.

The current firmware stores settings including:

* Jellyfin selection
* HDMI selection
* serial-data output state
* charging state

The project uses EEPROM update operations to avoid unnecessary writes when a stored value has not changed.

---

## Hardware Connections

The reference hardware uses the following connections:

| Arduino pin | Function                         |
| ----------- | -------------------------------- |
| `A0`        | LM358 audio/peak-detector output |
| `A1`        | 10 kΩ Reaction potentiometer     |
| `A2`        | 10 kΩ Deadband potentiometer     |
| `A3`        | Battery-voltage divider          |
| `D2`        | IR receiver                      |
| `D3`        | IR transmitter                   |
| `D4`        | TP4056 charging control          |
| `D5`        | Hardware reset transistor        |
| `D7`        | Battery-load MOSFET              |
| `D10`       | RGB LED red                      |
| `D6`        | RGB LED green                    |
| `D9`        | RGB LED blue                     |
| `D11`       | Tone output                      |

The RGB LED is a single **common-anode RGB LED**; no RGB driver module is required.

---

## Reference Hardware

### Controller

* Arduino Pro Mini
* ATmega328P
* 3.3 V
* 8 MHz

The software uses AVR-specific functionality for sleep and peripheral power control. The project can be adapted to other Arduino-compatible hardware, but the current source is written around the AVR/ATmega328P architecture.

### Audio

* Electret microphone
* LM358 peak/noise detector
* Audio detector output connected to `A0`

### Battery

* 1 × 18650 Li-ion cell
* TP4056 charging/protection board
* MOSFET-controlled approximately 100 mA load
* Battery-voltage resistor divider

### IR

* Standard IR receiver
* IR LED
* NPN transistor driver
* 1 kΩ transistor base resistor
* 47 Ω IR LED resistor

### User interface

* 1 × common-anode RGB LED
* 2 × 10 kΩ potentiometers
* Remote control

---

## Software Structure

The project is split into several `.ino` files for easier maintenance.

| File                                     | Purpose                                                                       |
| ---------------------------------------- | ----------------------------------------------------------------------------- |
| `TV_Audio_Control_V10.1_SEP.11.2026.ino` | Main program, hardware definitions, global state and main loop                |
| `Automatic_Audio_Control.ino`            | Audio detection and automatic volume state machine                            |
| `Battery_Management.ino`                 | Battery measurement and charging management                                   |
| `RGB_Feedback.ino`                       | RGB feedback patterns and status indication                                   |
| `RGB_Feedback_Color_Edit.ino`            | RGB color editing functionality                                               |
| `Remote_Control.ino`                     | Incoming remote command handling and Samsung command translation              |
| `Sleep_Management.ino`                   | Low-power sleep and IR wake-up handling                                       |
| `Utilities.ino`                          | Shared utilities, EEPROM handling, IR command tables and supporting functions |

The main loop is intentionally simple and calls the major subsystems repeatedly:

```text
Audio control
      ↓
Battery / charging management
      ↓
RGB feedback
      ↓
Status handling
      ↓
IR processing
      ↓
Sleep management
```

The timing-sensitive parts of the system use `millis()`-based asynchronous timers and state machines where appropriate.

---

## Main Operating Concept

The overall system can be viewed as four cooperating subsystems:

```text
                   ┌──────────────────────┐
                   │   Electret Microphone│
                   └──────────┬───────────┘
                              │
                         LM358 detector
                              │
                              ▼
                    ┌──────────────────┐
                    │ Arduino / ATmega │
                    │                  │
                    │ Audio processing │
                    │ Battery manager  │
                    │ IR translator    │
                    │ Power management │
                    │ RGB feedback     │
                    └───────┬─────┬────┘
                            │     │
                     Samsung IR   │
                            │     │
                            ▼     ▼
                           TV    RGB LED
```

The IR remote provides both TV-control commands and commands for the controller itself.

The controller can therefore function simultaneously as:

* an automatic TV volume controller
* a Samsung IR transmitter
* an IR remote translator
* a battery-powered standalone device
* a charging-managed portable controller
* a visual status interface

---

## Dependencies

The current project uses:

* **Arduino-IRremote**
* **RGBLed** — based on the original library by [wilmouths](https://github.com/wilmouths/RGBLed.git)
* standard Arduino `EEPROM`
* AVR sleep/power functionality

The current source uses the Arduino-IRremote 3.x programming interface.

The RGBLed source included with this project contains project-specific modifications described in the [Modified RGBLed Library](#modified-rgbled-library) section.

---

## Configuration

Important operating parameters are defined in the main sketch and supporting modules.

Examples include:

* audio high/low thresholds
* maximum automatic volume reduction
* reaction timing
* deadband range
* battery-voltage thresholds
* battery measurement interval
* charging-check interval
* IR timing
* sleep timeout
* RGB feedback timing

These values can be adjusted to suit different hardware, room acoustics, TV behavior, battery characteristics, and user preferences.

---

## Important Notes

### Samsung compatibility

The Samsung IR command set used by this project has been developed and tested with the following TV model:

**Samsung UE43RU7102KXXH**

The Samsung IR codes used by the controller are therefore specifically verified with this model. Compatibility with other Samsung TV models may vary depending on the IR command set implemented by the particular TV.

### Remote compatibility

The receiver is not limited conceptually to Philips remotes. The current firmware contains Philips command mappings, but the command-processing system can be adapted to other IR remote protocols and command codes.

### Battery hardware

The charging and protection circuitry must be appropriate for a single-cell 18650 Li-ion battery.

The approximately 100 mA load is part of the battery-voltage measurement system and should be implemented with suitable components and wiring.

The battery is intentionally used as the controller's power source because direct operation from the tested 5 V phone chargers introduced unacceptable electrical noise into the system.

### Hardware-specific pins

Changing the Arduino board or pin assignment requires corresponding changes in the source code.

The current reference implementation is specifically developed around an **ATmega328P-based 3.3 V / 8 MHz Arduino Pro Mini** and its AVR peripherals.

### RGBLed library attribution

The RGBLed library used as the basis for the project's implementation was originally created by **wilmouths**.

Original repository:

https://github.com/wilmouths/RGBLed.git

The RGBLed code in this project has been modified as described above. The original project remains credited as the basis of the library.

---

## Project Status

This is a personal/hobby electronics project developed iteratively around real hardware.

The current V10.1 implementation focuses on:

* reliable automatic audio-based volume control
* practical Samsung IR control
* remote-control translation
* battery management
* low-power operation
* RGB user feedback
* persistent configuration

The design intentionally favors a relatively direct Arduino implementation over large software abstractions, making the behavior of the physical device easier to inspect and modify.
