#include <Arduino.h>
#include <FastLED.h>

#include "leds/LEDController.h"
#include "ulp/ULP.h"

/***************************************
 * ULP
 ***************************************/
#define C1 A0
#define C2 A1
#define C3 A2
#define C4 A3
#define T1 A7

/***************************************
 * LEDs
 ***************************************/
#define NUM_LEDS 5
#define LED_DATA_PIN 8
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

f2b::LEDController LEDController = f2b::LEDController(leds, NUM_LEDS);

/***************************************
 * Timing
 ***************************************/
unsigned long sensorPreviousMillis = 0;
unsigned long sensorCurrentMillis = 0;
unsigned long tempPreviousMillis = 0;
unsigned long tempCurrentMillis = 0;

/***************************************
 * Thermistor
 ***************************************/
#define TH_PIN A7
#define TH_NOMINAL 100000
#define TEMP_NOMINAL 25
#define NUM_SAMPLES 50
#define B_COEFF 3950
#define SERIES_RESISTOR 100000
uint8_t i;
float average;
long int sample_sum;
float temp = 20;

double runTime = 0.0;

// sensor averaging times, keep these low, so that the ADC read does not
// overflow 32 bits. For example n = 5 reads ADC 4465 times which could add to
// 22bit number.
const int n = 2;  // seconds to read gas sensor
const int m = 1;  // seconds to read temperature sensor
const int s = 4;  // seconds to read all sensors, should be greater than n+m+1

// Sensitivities (as shown th_i
// SO2: 39.23 nA / ppm
// O3: ~ -60 nA +- 10 / ppm
// NO2: ~ -30 nA +- 10 / ppm
const float Sf1 = 4.47;
const float Sf2 = 39.23;
const float Sf3 = -60;
const float Sf4 = -30;

CO sensor1(C1, T1, Sf1);
SO2 sensor2(C2, T1, Sf2);
O3 sensor3(C3, T1, Sf3);
NO2 sensor4(C4, T1, Sf4);

void setup() {
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A4, INPUT);
  pinMode(A5, INPUT);
  pinMode(A6, INPUT);
  pinMode(A7, INPUT);

  FastLED.addLeds<WS2812B, LED_DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(255);
  FastLED.clear();
  FastLED.show();

  LEDController.currentLEDState = f2b::LEDState::PULSE_BLUE;

  sensor1.pVcc = 4.74;  // TODO: update if micro swapped
  sensor1.pVsup = 3.23;

  Serial.begin(9600);

  Serial.println();
  Serial.println("Setting Up.");

  Serial.print("  Vsup for all sensors = ");
  Serial.println(sensor1.pVsup);
  Serial.print("  Vcc for all sensors = ");
  Serial.println(sensor1.pVcc);
  Serial.print("  Vref for sensor 1 = ");
  Serial.println(sensor1.pVref);

  //  Using resistor values from board R1, R2, R3 are for setting pVref and
  //  Bias, while R6 sets the gain If using modified or custom boards set Vref
  //  and Gain like this

  // CO
  long int CO_R2 = 1990;  // Assumes R1 and R3 are 1 MOhms in resistor ladder
  float CO_bias = 3.0;
  sensor1.setVref(CO_bias, CO_R2);
  sensor1.pGain = 100000;  // resistor R6

  // SO2
  long int SO2_R2 = 143000;  // TODO: measured values
  float SO2_bias = 200.0;
  sensor2.setVref(SO2_bias, SO2_R2);
  sensor2.pGain = 100000;

  // O3
  long int O3_R2 = 16200;
  float O3_bias = -25;
  sensor3.setVref(O3_bias, O3_R2);
  sensor3.pGain = 499000;

  // NO2
  long int NO2_R2 = 16200;
  float NO2_bias = -25;
  sensor4.setVref(NO2_bias, NO2_R2);
  sensor4.pGain = 499000;

  // if you know the V_ref replace the following code...
  //  Serial.println("Remove CO Sensor.");
  //  if (sensor1.OCzero(n)) {
  //    Serial.print("Vref new = ");
  //    Serial.println(sensor1.pVref_set);
  //  } else {
  //    Serial.println("Recheck Settings, Zero out of range");
  //    while (1) {
  //      Serial.println(analogRead(A0));
  //      delay(1000);
  //    }sensor1
  //  }

  // Serial.println("Finished Setting Up, Replace Sensor Now.\n");
  Serial.println("\n\ns, temp, mV, nA, PPM");
  // etime = millis();

  //...with this code and your measured value of new Vref
  sensor1.pVref_set = 1613.39;

  sensorPreviousMillis = millis();
  tempPreviousMillis = millis();
}

void loop() {
  runTime = millis() / 1000.0;

  // if zero command is sent, zero the sensor
  while (Serial.available()) {
    if (Serial.read() == 'Z') {
      Serial.println("Zeroing");
      sensor1.zero();
      Serial.print("  sensor1: ");
      Serial.print(sensor1.pIzero);
      Serial.print(", ");
      Serial.println(sensor1.pTzero);

      // sensor2.zero();
      // Serial.print("  sensor2: ");
      // Serial.print(sensor2.pIzero);
      // Serial.print(", ");
      // Serial.println(sensor2.pTzero);

      // Serial.print("  sensor3: ");
      // Serial.print(sensor3.pIzero);
      // Serial.print(", ");
      // Serial.println(sensor3.pTzero);

      // Serial.print("  sensor4: ");
      // Serial.print(sensor4.pIzero);
      // Serial.print(", ");
      // Serial.println(sensor4.pTzero);
    }
  }

  // update temperature
  if (millis() - tempPreviousMillis > 50) {
    sample_sum += analogRead(T1);
    i++;

    if (i >= NUM_SAMPLES) {
      average = sample_sum / i;

      // convert to resistance
      average = 1023 / average - 1;
      average = SERIES_RESISTOR / average;
      // Serial.print("Thermistor resistance: ");
      // Serial.println(average);

      float steinhart;
      steinhart = average / TH_NOMINAL;            // (R/Ro)
      steinhart = log(steinhart);                  // ln(R/Ro)
      steinhart /= B_COEFF;                        // 1/B * ln(R/Ro)
      steinhart += 1.0 / (TEMP_NOMINAL + 273.15);  // + (1/To)
      steinhart = 1.0 / steinhart;                 // Invert
      steinhart -= 273.15;                         // convert absolute temp to C

      // Serial.print("Temperature: ");
      // Serial.print(steinhart);
      // Serial.println(" *C\n\n");

      temp = steinhart;

      sample_sum = 0;
      i = 0;
      average = 0;
    }
    tempPreviousMillis = millis();
  }

  // loop, read sensor values
  if (millis() - sensorPreviousMillis > (s * 1000)) {
    sensorPreviousMillis = millis();

    sensor1.getIgas(n);
    sensor1.setTemp(temp);
    sensor1.getConc(temp);

    // sensor2.getIgas(n);
    // sensor2.getTemp(m);
    // sensor2.getConc(20);

    // sensor3.getIgas(n);
    // sensor3.getTemp(m);
    // sensor3.getConc(20);

    // sensor4.getIgas(n);
    // sensor4.getTemp(m);
    // sensor4.getConc(20);

    Serial.print(runTime);
    Serial.print(", ");
    Serial.print(sensor1.convertT('C'));  // use 'C' or 'F' for units
    Serial.print(", ");
    Serial.print(sensor1.pVgas);
    Serial.print(", ");
    Serial.print(sensor1.pInA);
    Serial.print(", ");
    Serial.println(sensor1.convertX('M'));
  }

  // if (coPPM > 10) {
  //   LEDController.currentLEDState = f2b::LEDState::PULSE_RED;
  // } else if (coPPM > 5) {
  //   LEDController.currentLEDState = f2b::LEDState::PULSE_YELLOW;
  // } else {
  //   LEDController.currentLEDState = f2b::LEDState::LOADING_SPIN;
  // }

  LEDController.UpdateLEDs();
}