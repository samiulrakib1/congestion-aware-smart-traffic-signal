/*
  Project: Congestion-Aware Smart Traffic Signal
  Platform: Arduino (e.g., Arduino Mega / compatible board)
  Sensors: 3 x HC-SR04 ultrasonic sensors
  Display: 16x2 I2C LCD
  Language: Arduino C++

  Description:
  Each road has one ultrasonic sensor. If a vehicle is detected within
  CAR_DISTANCE, that road receives GREEN_CAR seconds of green time.
  Otherwise, it receives GREEN_NO_CAR seconds.

  The controller serves roads in a fixed round-robin order:
  Road 1 -> Road 2 -> Road 3 -> repeat.

  Important:
  This is an educational prototype, not a certified traffic-control system.
  Do not connect it to real road traffic signals.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD configuration: I2C address 0x27, 16 columns, 2 rows.
// Some LCD modules use address 0x3F instead.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// -----------------------------------------------------------------------------
// TRAFFIC LIGHT LED PINS
// Each road has one red, one yellow, and one green LED.
// -----------------------------------------------------------------------------

// Road 1
const byte R1 = 2;
const byte Y1 = 3;
const byte G1 = 4;

// Road 2
const byte R2 = 5;
const byte Y2 = 6;
const byte G2 = 7;

// Road 3
const byte R3 = 8;
const byte Y3 = 9;
const byte G3 = 10;

// -----------------------------------------------------------------------------
// ULTRASONIC SENSOR PINS
// TRIG pins send the measurement pulse; ECHO pins receive the reflected pulse.
// -----------------------------------------------------------------------------

// Road 1 sensor
const byte TRIG1 = A3;
const byte ECHO1 = 13;

// Road 2 sensor
const byte TRIG2 = A0;
const byte ECHO2 = A2;

// Road 3 sensor
const byte TRIG3 = 11;
const byte ECHO3 = 12;

// -----------------------------------------------------------------------------
// TIMING AND DETECTION SETTINGS
// -----------------------------------------------------------------------------

const int GREEN_CAR = 12;       // Green duration when a vehicle is detected
const int GREEN_NO_CAR = 5;     // Green duration when no vehicle is detected
const int YELLOW_TIME = 2;      // Yellow transition duration
const int CAR_DISTANCE = 10;    // Vehicle detection threshold in centimetres

const byte SENSOR_READINGS = 5; // Number of readings used for averaging
const unsigned long ECHO_TIMEOUT = 15000UL; // pulseIn timeout in microseconds

// Store the latest measured distances (centimetres).
long d1 = 999;
long d2 = 999;
long d3 = 999;

// The first road to receive a green signal.
byte currentLane = 1;

// -----------------------------------------------------------------------------
// FUNCTION: getDistance()
// Takes several ultrasonic readings and returns their average distance.
//
// A timeout or missing echo is represented as 999 cm. This value is treated
// as "no vehicle detected" by the detection function.
// -----------------------------------------------------------------------------

long getDistance(byte trigPin, byte echoPin) {
  long totalDistance = 0;

  for (byte i = 0; i < SENSOR_READINGS; i++) {
    // Ensure the trigger pin starts LOW.
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);

    // Send a 10-microsecond trigger pulse.
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    // Measure how long the ECHO pin stays HIGH.
    unsigned long duration = pulseIn(echoPin, HIGH, ECHO_TIMEOUT);

    long distance;

    if (duration == 0) {
      // No valid echo was received before the timeout.
      distance = 999;
    } else {
      // Distance = (time x speed of sound) / 2.
      // Divide by 2 because the sound travels to the object and back.
      distance = (long)(duration * 0.034 / 2.0);
    }

    totalDistance += distance;
    delay(10);
  }

  return totalDistance / SENSOR_READINGS;
}

// -----------------------------------------------------------------------------
// FUNCTION: updateSensors()
// Reads all three sensors and updates the global distance variables.
// -----------------------------------------------------------------------------

void updateSensors() {
  d1 = getDistance(TRIG1, ECHO1);
  d2 = getDistance(TRIG2, ECHO2);
  d3 = getDistance(TRIG3, ECHO3);
}

// -----------------------------------------------------------------------------
// FUNCTION: carDetected()
// Returns true if the measured distance is within the vehicle threshold.
// -----------------------------------------------------------------------------

bool carDetected(long distance) {
  return (distance > 0 && distance <= CAR_DISTANCE);
}

// -----------------------------------------------------------------------------
// FUNCTION: getGreenTime()
// Selects the green duration according to vehicle detection.
// -----------------------------------------------------------------------------

int getGreenTime(long distance) {
  if (carDetected(distance)) {
    return GREEN_CAR;
  }

  return GREEN_NO_CAR;
}

// -----------------------------------------------------------------------------
// FUNCTION: allRed()
// Sets all roads to RED and switches OFF every yellow and green LED.
// This is used before changing the active green signal.
// -----------------------------------------------------------------------------

void allRed() {
  // Turn ON all red LEDs.
  digitalWrite(R1, HIGH);
  digitalWrite(R2, HIGH);
  digitalWrite(R3, HIGH);

  // Turn OFF all green LEDs.
  digitalWrite(G1, LOW);
  digitalWrite(G2, LOW);
  digitalWrite(G3, LOW);

  // Turn OFF all yellow LEDs.
  digitalWrite(Y1, LOW);
  digitalWrite(Y2, LOW);
  digitalWrite(Y3, LOW);
}

// -----------------------------------------------------------------------------
// FUNCTION: runGreen()
// Activates the green signal for the selected road for the given duration.
// The LCD displays the road number and remaining time.
// Sensor readings are refreshed during the countdown for monitoring.
// -----------------------------------------------------------------------------

void runGreen(byte lane, int seconds) {
  // Reset the signals before enabling the selected road.
  allRed();
  delay(10);

  // Turn OFF the selected road's red LED and turn ON its green LED.
  if (lane == 1) {
    digitalWrite(R1, LOW);
    digitalWrite(G1, HIGH);
  } else if (lane == 2) {
    digitalWrite(R2, LOW);
    digitalWrite(G2, HIGH);
  } else if (lane == 3) {
    digitalWrite(R3, LOW);
    digitalWrite(G3, HIGH);
  } else {
    // Invalid road number: keep the system in the all-red state.
    return;
  }

  // Report the reason for the selected green duration.
  Serial.print("Road ");
  Serial.print(lane);

  if (seconds == GREEN_CAR) {
    Serial.println(" GREEN - Vehicle detected (12s)");
  } else {
    Serial.println(" GREEN - No vehicle detected (5s)");
  }

  // Update the LCD once per second until the green period ends.
  for (int remaining = seconds; remaining > 0; remaining--) {
    updateSensors();

    lcd.setCursor(0, 0);
    lcd.print("Road ");
    lcd.print(lane);
    lcd.print(" GREEN   ");

    lcd.setCursor(0, 1);
    lcd.print("Time: ");
    lcd.print(remaining);
    lcd.print("s       ");

    // Print the latest sensor readings to the Serial Monitor.
    Serial.print("D1: ");
    Serial.print(d1);
    Serial.print(" cm | D2: ");
    Serial.print(d2);
    Serial.print(" cm | D3: ");
    Serial.print(d3);
    Serial.print(" cm | Time: ");
    Serial.print(remaining);
    Serial.println("s");

    delay(1000);
  }
}

// -----------------------------------------------------------------------------
// FUNCTION: runYellow()
// Switches the selected road to YELLOW for the transition period.
// The other two roads remain RED.
// -----------------------------------------------------------------------------

void runYellow(byte lane) {
  // Switch OFF all green and yellow LEDs before setting the transition state.
  digitalWrite(G1, LOW);
  digitalWrite(G2, LOW);
  digitalWrite(G3, LOW);

  digitalWrite(Y1, LOW);
  digitalWrite(Y2, LOW);
  digitalWrite(Y3, LOW);

  // Keep the selected road's red LED OFF and activate its yellow LED.
  // The other roads remain red.
  if (lane == 1) {
    digitalWrite(R1, LOW);
    digitalWrite(R2, HIGH);
    digitalWrite(R3, HIGH);
    digitalWrite(Y1, HIGH);
  } else if (lane == 2) {
    digitalWrite(R2, LOW);
    digitalWrite(R1, HIGH);
    digitalWrite(R3, HIGH);
    digitalWrite(Y2, HIGH);
  } else if (lane == 3) {
    digitalWrite(R3, LOW);
    digitalWrite(R1, HIGH);
    digitalWrite(R2, HIGH);
    digitalWrite(Y3, HIGH);
  } else {
    allRed();
    return;
  }

  Serial.print("Road ");
  Serial.print(lane);
  Serial.println(" YELLOW (2s)");

  // Display the yellow countdown.
  for (int remaining = YELLOW_TIME; remaining > 0; remaining--) {
    lcd.setCursor(0, 0);
    lcd.print("Road ");
    lcd.print(lane);
    lcd.print(" YELLOW  ");

    lcd.setCursor(0, 1);
    lcd.print("Wait: ");
    lcd.print(remaining);
    lcd.print("s       ");

    Serial.print("Yellow remaining: ");
    Serial.print(remaining);
    Serial.println("s");

    delay(1000);
  }

  // Turn OFF all yellow LEDs after the transition.
  digitalWrite(Y1, LOW);
  digitalWrite(Y2, LOW);
  digitalWrite(Y3, LOW);

  // Return to the all-red state before the next road is served.
  allRed();
}

// -----------------------------------------------------------------------------
// FUNCTION: setup()
// Initializes the LCD, LED pins, sensor pins, and Serial Monitor.
// The system starts with all roads RED.
// -----------------------------------------------------------------------------

void setup() {
  lcd.init();
  lcd.backlight();

  // Configure all traffic light LED pins as outputs.
  pinMode(R1, OUTPUT);
  pinMode(Y1, OUTPUT);
  pinMode(G1, OUTPUT);

  pinMode(R2, OUTPUT);
  pinMode(Y2, OUTPUT);
  pinMode(G2, OUTPUT);

  pinMode(R3, OUTPUT);
  pinMode(Y3, OUTPUT);
  pinMode(G3, OUTPUT);

  // Configure ultrasonic sensor trigger pins as outputs
  // and echo pins as inputs.
  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);

  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);

  pinMode(TRIG3, OUTPUT);
  pinMode(ECHO3, INPUT);

  // Start serial communication for debugging and monitoring.
  Serial.begin(9600);
  Serial.println("SMART TRAFFIC SYSTEM READY");

  // Initialize the LCD message and safe starting signal state.
  lcd.setCursor(0, 0);
  lcd.print("Smart Traffic");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");

  allRed();
  delay(1000);
}

// -----------------------------------------------------------------------------
// FUNCTION: loop()
// Repeatedly measures traffic, selects green durations, and serves roads
// in round-robin order: Road 1 -> Road 2 -> Road 3 -> repeat.
// -----------------------------------------------------------------------------

void loop() {
  // Read the traffic conditions before starting the next road's turn.
  updateSensors();

  // Print the current distance measurements.
  Serial.print("ROAD 1: ");
  Serial.print(d1);
  Serial.print(" cm | ROAD 2: ");
  Serial.print(d2);
  Serial.print(" cm | ROAD 3: ");
  Serial.print(d3);
  Serial.println(" cm");

  // Calculate the green duration for each road from its latest reading.
  int t1 = getGreenTime(d1);
  int t2 = getGreenTime(d2);
  int t3 = getGreenTime(d3);

  // Serve one road, show yellow, and move to the next road.
  if (currentLane == 1) {
    runGreen(1, t1);
    runYellow(1);
    currentLane = 2;
  } else if (currentLane == 2) {
    runGreen(2, t2);
    runYellow(2);
    currentLane = 3;
  } else { // currentLane == 3
    runGreen(3, t3);
    runYellow(3);
    currentLane = 1;
  }
}
