# Throttle and Follow Control Design

**Status:** Design approved for implementation planning. The follow state machine will be implemented but will remain disabled and unreachable from the current user interface until an entry/exit trigger is defined.

## Goal

Add a reusable vehicle-control layer for the STM32F103C8 car that supports two selectable single-pedal throttle strategies and a PID-based fixed-distance-following state machine, while preserving the existing low-level peripheral drivers and enforcing a 150 mm forward emergency-stop limit.

## Scope

This design covers:

- Direct pedal-to-PWM control.
- Pedal-to-target-speed conversion followed by a speed PID.
- A follow controller with a 250 mm distance setpoint and a distance PID.
- Forward-only emergency braking at 150 mm, with reverse still available to move away from an obstacle.
- Stable interfaces for future encoder/FG feedback and a future follow-mode entry/exit trigger.

This design does not select or implement a follow-mode trigger, add encoder GPIO configuration, or tune final PID gains. Follow mode remains disabled in the default build.

## Existing Constraints

- The project uses Keil MDK5, STM32F103C8, and the STM32F10x Standard Peripheral Library.
- Hardware pin assignments are fixed by `Doc/readme.txt`.
- The existing `User/main.c` builds three firmware roles using `__RF24L01_TX_TEST__` and `_MAIN_ECU_`.
- The direction-wheel packet is eight bytes: wheel ADC high/low bytes, pedal value, D/R/N state, and left/right state with marker bytes `0x01`, `0x02`, and `0x03`.
- `User/motor/motor.c` already exposes independent PWM setters and a placeholder `Motor_SetCW()`.
- `User/turn`, `User/ws2812b`, `User/oled`, and `User/vl53l0/demo` expose steering, light, display, and `Distance_Update()` APIs.
- The motor documentation provides 18 FG pulses per motor revolution. The FG input pin and gearbox/output-speed calibration are not yet confirmed.

## Architecture

Add a new `User/control/` module with three responsibilities:

1. `pid.c/.h` implements a bounded discrete PID controller. It owns integral accumulation, derivative calculation, output limits, and reset behavior. The update call receives the elapsed period in milliseconds so the caller can later replace the initial fixed-period scheduler without changing PID users.
2. `vehicle_control.c/.h` converts direction-wheel input and feedback into a `DriveCommand`. It selects throttle mode 1 or 2, applies the single-pedal direction convention, invokes the speed PID when feedback is available, invokes the follow PID when follow is explicitly enabled, and applies the emergency-stop rule.
3. `speed_feedback.c/.h` defines the feedback boundary. The first implementation supplies an invalid/no-feedback result because the FG pins are not known. A later encoder implementation can replace this source without changing the throttle or follow controllers.

The low-level motor, steering, OLED, WS2812, radio, and VL53L0X drivers remain separate. `main.c` remains responsible for board initialization, radio packet validation, sensor reads, and passing snapshots into `VehicleControl_Update()`.

## Control Modes

### Throttle mode 1: direct PWM

The pedal byte (`0..255`) maps to a duty command (`0..100`). D/R/N selects the sign/direction: D applies forward, R applies reverse, and N forces zero duty. This mode is the default and does not require speed feedback.

### Throttle mode 2: target-speed PID

The pedal byte maps to a configurable maximum target speed expressed in output-shaft RPM. D produces a positive target, R a negative target, and N produces zero. The speed controller compares this target with feedback from the future FG/encoder layer and returns a bounded PWM magnitude. If feedback is invalid, the controller returns a safe stop command instead of pretending that the measured speed is zero.

The public API exposes the mode as numeric values `1` and `2`, matching the requested convention. The default compile-time setting is mode 1.

## Follow State Machine

The follow controller is present but disabled by default. Its public enable setter is retained for future integration, but no current radio field, switch, or button calls it.

When enabled:

- The state machine is forward-only. Reverse input exits the active drive command to a stopped output and remains available to the operator through the normal manual path once follow is disabled.
- The distance PID uses `250 mm` as its setpoint and returns a non-negative target speed, bounded by a configurable follow-speed limit.
- The resulting target speed is passed through the same speed-control path as throttle mode 2. This creates a cascaded distance-to-speed-to-PWM controller and keeps actuator limiting in one place.
- A valid distance at or below `150 mm` forces zero forward output. Reverse remains permitted when follow is not active, satisfying the requirement to move away from an obstacle.
- An invalid distance never causes forward motion in follow mode.

The state machine has explicit reset behavior: disabling follow clears the distance PID integral and derivative history; re-enabling starts from a stopped command and requires a fresh valid distance sample.

## Public Interfaces

The planned interfaces are:

```c
typedef enum {
    THROTTLE_DIRECT_PWM = 1,
    THROTTLE_TARGET_SPEED_PID = 2
} ThrottleMode;

typedef enum {
    VEHICLE_CONTROL_MANUAL = 0,
    VEHICLE_CONTROL_FOLLOW = 1
} VehicleControlState;

typedef struct {
    uint16_t wheel_adc;
    uint8_t pedal;
    uint8_t dnr;
    uint8_t left_right;
} VehicleInput;

typedef struct {
    int16_t left_rpm;
    int16_t right_rpm;
    uint8_t speed_valid;
    uint16_t distance_mm;
    uint8_t distance_valid;
} VehicleFeedback;

typedef struct {
    uint8_t direction;
    uint8_t left_duty;
    uint8_t right_duty;
    uint8_t emergency_stop;
} DriveCommand;

void VehicleControl_Init(void);
void VehicleControl_SetThrottleMode(ThrottleMode mode);
ThrottleMode VehicleControl_GetThrottleMode(void);
void VehicleControl_SetFollowEnabled(uint8_t enabled);
VehicleControlState VehicleControl_GetState(void);
void VehicleControl_Update(const VehicleInput *input,
                           const VehicleFeedback *feedback,
                           uint32_t dt_ms,
                           DriveCommand *command);

void PID_Init(PID_Controller *pid, float kp, float ki, float kd,
              float output_min, float output_max);
void PID_Reset(PID_Controller *pid);
float PID_Update(PID_Controller *pid, float setpoint,
                 float measurement, float dt_s);
```

The exact enum types for D/R/N and left/right will reuse the existing `switch.h` definitions or be converted at the `main.c` boundary; the control module will not duplicate hardware GPIO definitions.

## Configuration

Place control constants in `vehicle_control.h` or a dedicated configuration header:

- `CONTROL_DEFAULT_THROTTLE_MODE = 1`.
- `CONTROL_AEB_STOP_DISTANCE_MM = 150`.
- `CONTROL_FOLLOW_DISTANCE_MM = 250`.
- `CONTROL_FG_PULSES_PER_REV = 18`.
- Maximum target RPM and PID gains are explicit calibration parameters. They must not be inferred from the motor name or from the drawing dimensions.

All outputs are clamped to the existing 0..100 duty range. Negative speed targets are represented by the direction field plus a positive PWM magnitude, so the motor driver never receives a signed unsigned value by accident.

## Main-loop Integration

The main ECU path will eventually:

1. Initialize control state after `Motor_Init()`, `Turn_Init()`, `OLED_Init()`, and `WS2812_Init()`.
2. Validate the eight-byte NRF24L01 frame before constructing `VehicleInput`.
3. Read the latest distance sample from `Distance_Update()` when the distance ECU data path is available.
4. Obtain speed feedback from the feedback module, initially marked invalid.
5. Call `VehicleControl_Update()` with the elapsed control period.
6. Apply the returned direction and PWM values through `Motor_SetCW()`, `Motor1_SetDuty()`, and `Motor2_SetDuty()`, and update steering/lights/display separately.

The initial integration keeps follow disabled and uses direct PWM, so the existing basic driving path can be tested before any encoder-dependent mode is selected.

## Safety and Failure Handling

- Invalid radio frames are ignored and do not overwrite the last valid command.
- Neutral or zero pedal produces zero duty.
- A forward command at or below 150 mm is forced to zero duty and sets `emergency_stop` for display/diagnostics.
- Reverse commands remain available for obstacle escape when manual control is active.
- Invalid speed feedback stops mode 2 rather than treating missing feedback as zero speed.
- Invalid distance stops follow mode.
- PID integral is clamped to the configured output range and reset whenever the controller is disabled or the requested direction changes through neutral.

## Verification Strategy

Before hardware testing, add host-side pure-function tests for:

- Pedal mapping endpoints and midpoint.
- D/R/N sign selection.
- Mode 1 duty output.
- Mode 2 output limiting and invalid-feedback stop.
- Follow PID output direction around 250 mm.
- AEB behavior at 150 mm and reverse escape behavior.
- Follow enable/disable reset behavior.

On hardware, verify with the rear wheels raised first: neutral, forward, reverse, steering center, AEB stop at a measured distance no greater than 150 mm, and recovery by reverse input. Only after these checks should follow be enabled manually for a controlled straight-line test.

