#include <Arduino.h>

class O3Sensor {
 private:
  const int resValue = 9870;  // TODO
  const float Vref = 1.1;
  const float Sf =
      -30;  // sensitivity in nA/ppm, ~1/2 stated sensitivity of -60 nA/ppm

  int i = 0;
  const int numPolls = 2048;
  bool updateAvg = false;

  long int sensorReadSum = 0;
  long int refReadSum = 0;
  float sensorValAvg = 0;
  float refValAvg = 0;
  float amountO3 = 0;
  float current = 0;
  float sensorSensitivity = 0;
  float PPM = 0;

  unsigned long previousMillis = 0;
  unsigned long currentMillis = 0;
  unsigned waitTime = 3;

 public:
  int analogInPin;
  int refInPin;

  /**
   * @brief Construct a new O3Sensor object
   *
   * @param analogInPin: analog pin connected to sensor output
   * @param refInPin: analog pin connected to sensor bias voltage
   */
  O3Sensor(uint8_t analogInPin, uint8_t refInPin) {
    analogInPin = analogInPin;
    refInPin = refInPin;
  }

  /**
   * @brief Get current PPM reading from sensor. PPM only updates each
   * {numPolls} iterations.
   *
   * @return float: PPM
   */
  float read() {
    currentMillis = millis();
    if (currentMillis - previousMillis >= waitTime) {
      sensorReadSum += analogRead(analogInPin);
      refReadSum += analogRead(refInPin);
      previousMillis = currentMillis;
      i++;
    }
    if (i >= numPolls) {
      sensorReadSum -= refReadSum;
      sensorValAvg = (float)sensorReadSum / numPolls;
      amountO3 = sensorValAvg / 1024;
      current = Vref / resValue;
      sensorSensitivity = Sf / 1000000000;

      PPM = amountO3 * current / sensorSensitivity;  // TODO

      // print results
      Serial.print("PPM = ");
      Serial.print(PPM);
      Serial.print("\tnA = ");
      Serial.print(current * amountO3 * 1000000000);  // TODO
      Serial.print("\tCounts = ");
      Serial.println(sensorReadSum);

      sensorReadSum = 0;
      refReadSum = 0;
      i = 0;
    }

    return PPM;
  }
};