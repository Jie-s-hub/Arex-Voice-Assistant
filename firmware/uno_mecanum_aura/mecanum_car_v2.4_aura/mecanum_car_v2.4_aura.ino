#include <PS2X_lib.h>
#include <NewPing.h>

PS2X ps2x;

// ================================================================
// AURA Rover Uno firmware based on mecanum_car_v2.3.ino
//
// Preserved:
//   - Original PS2 control behavior
//   - Original mecanum calculations and smoothing
//   - Original L298N motor control
//
// Required hardware change:
//   - Move the RF_IN1 wire from Uno D7 to Uno D3.
//   - D7 becomes the divided 3.3 V event output to ESP32 GPIO25.
//   - HC-SR04 uses NewPing one-pin mode on D11.
// ================================================================

// ================= PWM =================
#define RF_EN   9
#define RR_EN   10
#define LF_EN   5
#define LR_EN   6

// ================= RIGHT FRONT =================
#define RF_IN1  3   // Was D7 in v2.3; physically move this one wire to D3.
#define RF_IN2  8

// ================= RIGHT REAR =================
#define RR_IN1  2
#define RR_IN2  4

// ================= LEFT FRONT =================
#define LF_IN1  A0
#define LF_IN2  A1

// ================= LEFT REAR =================
#define LR_IN1  A2
#define LR_IN2  A3

// ================= PS2 =================
#define PS2_DAT A5
#define PS2_CMD A4
#define PS2_SEL 12
#define PS2_CLK 13

// ================= AURA SAFETY =================
#define AURA_ALERT_PIN 7
#define SONAR_PIN      11

const unsigned int STOP_DISTANCE_CM  = 25;
const unsigned int CLEAR_DISTANCE_CM = 32;
const unsigned int MAX_DISTANCE_CM   = 200;
const unsigned long SONAR_PERIOD_MS  = 50;

// NewPing automatically enables one-pin mode when trigger and echo use the
// same Arduino pin. See docs/YOUR_MECANUM_CODE_INTEGRATION.md for wiring.
NewPing sonar(SONAR_PIN, SONAR_PIN, MAX_DISTANCE_CM);

// ================= DRIVE CONSTANTS =================
const int   BASE_SPEED  = 180;
const int   DIAG_SPEED  = BASE_SPEED * 0.7;  // Approximately 126
const int   ROT_SPEED   = 150;
const int   DEADZONE    = 30;
const float SMOOTH      = 0.2;

int error = -1;

// ================= SMOOTH STATE =================
float smoothY = 0;
float smoothX = 0;
float smoothR = 0;

// ================= SAFETY STATE =================
enum AuraSignalState {
  SIGNAL_IDLE,
  SIGNAL_VOICE_HIGH_1,
  SIGNAL_VOICE_GAP,
  SIGNAL_VOICE_HIGH_2,
  SIGNAL_OBSTACLE_HIGH
};

AuraSignalState auraSignalState = SIGNAL_IDLE;
unsigned long lastSonarMs = 0;
unsigned long signalStateStartedMs = 0;
unsigned int lastDistanceCm = 0;
byte clearSampleCount = 0;
bool obstacleActive = false;
bool voiceButtonWasPressed = false;
bool requireNeutralAfterObstacle = false;

const unsigned long VOICE_HIGH_MS = 40;
const unsigned long VOICE_GAP_MS = 40;
const unsigned long OBSTACLE_MIN_HIGH_MS = 500;

// ================= FUNCTION DECLARATIONS =================
void mecanumDrive(float y, float x, float rotation);
void setMotor(int in1, int in2, int pwmPin, int speed);
void stopAllMotors();
void resetSmoothMotion();
void updateAuraSafety(bool voiceButtonPressed);
void updateSonar(unsigned long nowMs);
void updateAuraSignal(unsigned long nowMs);
void requestVoiceSignal(unsigned long nowMs);
void requestObstacleSignal(unsigned long nowMs);

// ===================== SETUP =========================
void setup() {
  Serial.begin(9600);

  // Motor direction pins
  pinMode(RF_IN1, OUTPUT); pinMode(RF_IN2, OUTPUT);
  pinMode(RR_IN1, OUTPUT); pinMode(RR_IN2, OUTPUT);
  pinMode(LF_IN1, OUTPUT); pinMode(LF_IN2, OUTPUT);
  pinMode(LR_IN1, OUTPUT); pinMode(LR_IN2, OUTPUT);

  // Motor enable (PWM) pins
  pinMode(RF_EN, OUTPUT); pinMode(RR_EN, OUTPUT);
  pinMode(LF_EN, OUTPUT); pinMode(LR_EN, OUTPUT);

  // Uno D7 is LOW when no event is being sent to the ESP32.
  pinMode(AURA_ALERT_PIN, OUTPUT);
  digitalWrite(AURA_ALERT_PIN, LOW);

  stopAllMotors();

  // Wait for PS2 to power up, retry up to 10 times.
  delay(300);
  for (int i = 0; i < 10; i++) {
    error = ps2x.config_gamepad(
        PS2_CLK, PS2_CMD, PS2_SEL, PS2_DAT, false, false);
    if (error == 0) {
      Serial.println("PS2 Connected");
      break;
    }
    Serial.print("PS2 Retry ");
    Serial.println(i + 1);
    delay(100);
  }

  if (error != 0) {
    Serial.println("PS2 Connection Failed - check wiring");
  }

  resetSmoothMotion();
  lastSonarMs = millis() - SONAR_PERIOD_MS;
}

// ===================== LOOP ==========================
void loop() {
  // Safety continues running even when the PS2 controller is disconnected.
  if (error != 0) {
    updateAuraSafety(false);
    stopAllMotors();
    delay(5);
    return;
  }

  ps2x.read_gamepad(false, false);

  // L1 was unused in mecanum_car_v2.3, so it is now the dedicated voice key.
  const bool voicePressed = ps2x.Button(PSB_L1);
  updateAuraSafety(voicePressed);

  if (obstacleActive) {
    requireNeutralAfterObstacle = true;
    resetSmoothMotion();
    stopAllMotors();
    delay(5);
    return;
  }

  float targetY = 0;
  float targetX = 0;
  float targetR = 0;

  // ================= BUTTON CONTROL =================
  if (ps2x.Button(PSB_PAD_UP)) {
    targetY = BASE_SPEED;
  }
  else if (ps2x.Button(PSB_PAD_DOWN)) {
    targetY = -BASE_SPEED;
  }
  else if (ps2x.Button(PSB_PAD_LEFT)) {
    targetX = -BASE_SPEED;
  }
  else if (ps2x.Button(PSB_PAD_RIGHT)) {
    targetX = BASE_SPEED;
  }
  else if (ps2x.Button(PSB_TRIANGLE)) {
    targetY =  DIAG_SPEED;
    targetX = -DIAG_SPEED;
  }
  else if (ps2x.Button(PSB_CROSS)) {
    targetY = DIAG_SPEED;
    targetX = DIAG_SPEED;
  }
  else if (ps2x.Button(PSB_SQUARE)) {
    targetR = -ROT_SPEED;
  }
  else if (ps2x.Button(PSB_CIRCLE)) {
    targetR =  ROT_SPEED;
  }

  // ================= JOYSTICK CONTROL =================
  else {
    int LX = ps2x.Analog(PSS_LX) - 128;
    int LY = 128 - ps2x.Analog(PSS_LY);
    int RX = ps2x.Analog(PSS_RX) - 128;

    // Deadzone
    if (abs(LX) < DEADZONE) LX = 0;
    if (abs(LY) < DEADZONE) LY = 0;
    if (abs(RX) < DEADZONE) RX = 0;

    // Non-linear (quadratic) scaling for finer low-speed control.
    LX = LX * abs(LX) / 128;
    LY = LY * abs(LY) / 128;
    RX = RX * abs(RX) / 128;

    targetX = LX;
    targetY = LY;
    targetR = RX;
  }

  // After an obstacle clears, require the driver to release all movement
  // controls once. This prevents automatic movement from a held stick/button.
  if (requireNeutralAfterObstacle) {
    const bool movementRequested =
        abs(targetX) > 0.5 || abs(targetY) > 0.5 || abs(targetR) > 0.5;
    resetSmoothMotion();
    stopAllMotors();
    if (!movementRequested) {
      requireNeutralAfterObstacle = false;
    }
    delay(5);
    return;
  }

  // ================= SMOOTHING =================
  smoothY += (targetY - smoothY) * SMOOTH;
  smoothX += (targetX - smoothX) * SMOOTH;
  smoothR += (targetR - smoothR) * SMOOTH;

  mecanumDrive(smoothY, smoothX, smoothR);
  delay(5);
}

// =============== AURA SAFETY UPDATE =================
void updateAuraSafety(bool voiceButtonPressed) {
  const unsigned long nowMs = millis();
  updateSonar(nowMs);

  // Rising-edge only: holding L1 sends only one voice request.
  if (voiceButtonPressed && !voiceButtonWasPressed && !obstacleActive) {
    requestVoiceSignal(nowMs);
  }
  voiceButtonWasPressed = voiceButtonPressed;

  if (obstacleActive) {
    stopAllMotors();
  }
  updateAuraSignal(nowMs);
}

void updateSonar(unsigned long nowMs) {
  if (nowMs - lastSonarMs < SONAR_PERIOD_MS) {
    return;
  }
  lastSonarMs = nowMs;

  // NewPing returns zero if it receives no echo.
  lastDistanceCm = sonar.ping_cm();
  const bool tooClose =
      lastDistanceCm > 0 && lastDistanceCm <= STOP_DISTANCE_CM;

  if (tooClose) {
    clearSampleCount = 0;
    stopAllMotors();
    if (!obstacleActive) {
      obstacleActive = true;
      resetSmoothMotion();
      requestObstacleSignal(nowMs);
      Serial.print("Obstacle: ");
      Serial.print(lastDistanceCm);
      Serial.println(" cm - motors stopped");
    }
    return;
  }

  if (!obstacleActive) {
    return;
  }

  // Three clear readings beyond the hysteresis distance release the latch.
  if (lastDistanceCm == 0 || lastDistanceCm >= CLEAR_DISTANCE_CM) {
    clearSampleCount++;
    if (clearSampleCount >= 3) {
      obstacleActive = false;
      clearSampleCount = 0;
      Serial.println("Obstacle cleared - release movement controls");
    }
  } else {
    clearSampleCount = 0;
  }
}

// =============== ONE-WIRE EVENT OUTPUT =================
void requestVoiceSignal(unsigned long nowMs) {
  if (auraSignalState != SIGNAL_IDLE) {
    return;
  }
  auraSignalState = SIGNAL_VOICE_HIGH_1;
  signalStateStartedMs = nowMs;
  digitalWrite(AURA_ALERT_PIN, HIGH);
  Serial.println("AURA voice request");
}

void requestObstacleSignal(unsigned long nowMs) {
  // An obstacle always overrides a voice pulse pattern.
  auraSignalState = SIGNAL_OBSTACLE_HIGH;
  signalStateStartedMs = nowMs;
  digitalWrite(AURA_ALERT_PIN, HIGH);
}

void updateAuraSignal(unsigned long nowMs) {
  const unsigned long elapsed = nowMs - signalStateStartedMs;

  switch (auraSignalState) {
    case SIGNAL_IDLE:
      break;

    case SIGNAL_VOICE_HIGH_1:
      if (elapsed >= VOICE_HIGH_MS) {
        digitalWrite(AURA_ALERT_PIN, LOW);
        auraSignalState = SIGNAL_VOICE_GAP;
        signalStateStartedMs = nowMs;
      }
      break;

    case SIGNAL_VOICE_GAP:
      if (elapsed >= VOICE_GAP_MS) {
        digitalWrite(AURA_ALERT_PIN, HIGH);
        auraSignalState = SIGNAL_VOICE_HIGH_2;
        signalStateStartedMs = nowMs;
      }
      break;

    case SIGNAL_VOICE_HIGH_2:
      if (elapsed >= VOICE_HIGH_MS) {
        digitalWrite(AURA_ALERT_PIN, LOW);
        auraSignalState = SIGNAL_IDLE;
      }
      break;

    case SIGNAL_OBSTACLE_HIGH:
      // Keep D7 HIGH while the obstacle latch is active so the ESP32 cannot
      // miss the event while busy with audio or a smart-home HTTP request.
      if (elapsed >= OBSTACLE_MIN_HIGH_MS && !obstacleActive) {
        digitalWrite(AURA_ALERT_PIN, LOW);
        auraSignalState = SIGNAL_IDLE;
      }
      break;
  }
}

void resetSmoothMotion() {
  smoothY = 0;
  smoothX = 0;
  smoothR = 0;
}

// =============== MECANUM CALCULATION =================
void mecanumDrive(float y, float x, float rotation) {
  float frontLeft  = y + x + rotation;
  float frontRight = y - x - rotation;
  float rearLeft   = y - x + rotation;
  float rearRight  = y + x - rotation;

  // Normalize if any wheel exceeds 255.
  float maxValue = max(max(abs(frontLeft), abs(frontRight)),
                       max(abs(rearLeft),  abs(rearRight)));

  if (maxValue > 255) {
    frontLeft  = frontLeft  * 255.0 / maxValue;
    frontRight = frontRight * 255.0 / maxValue;
    rearLeft   = rearLeft   * 255.0 / maxValue;
    rearRight  = rearRight  * 255.0 / maxValue;
  }

  setMotor(LF_IN1, LF_IN2, LF_EN, (int)frontLeft);
  setMotor(RF_IN1, RF_IN2, RF_EN, (int)frontRight);
  setMotor(LR_IN1, LR_IN2, LR_EN, (int)rearLeft);
  setMotor(RR_IN1, RR_IN2, RR_EN, (int)rearRight);
}

// ================= MOTOR FUNCTION ====================
void setMotor(int in1, int in2, int pwmPin, int speed) {
  if (speed > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  }
  else if (speed < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  }
  else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }
  analogWrite(pwmPin, abs(speed));
}

// ================= STOP ALL MOTORS ===================
void stopAllMotors() {
  digitalWrite(RF_IN1, LOW); digitalWrite(RF_IN2, LOW);
  digitalWrite(RR_IN1, LOW); digitalWrite(RR_IN2, LOW);
  digitalWrite(LF_IN1, LOW); digitalWrite(LF_IN2, LOW);
  digitalWrite(LR_IN1, LOW); digitalWrite(LR_IN2, LOW);
  analogWrite(RF_EN, 0);
  analogWrite(RR_EN, 0);
  analogWrite(LF_EN, 0);
  analogWrite(LR_EN, 0);
}

