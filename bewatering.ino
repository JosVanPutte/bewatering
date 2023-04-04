#define BLYNK_TEMPLATE_ID "TMPLE44qhOY2"
#define BLYNK_TEMPLATE_NAME "Bewatering"
#define BLYNK_AUTH_TOKEN "---------------------------"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "ReadADC.h"
#include "GetTime.h"

#define VOLTAGEREADS 3
#define SENSORREADS 20

#define PLACES 2 // nr of places with a pump and a sensor
#define WALL 0 // pump/sensor 0 is wall
#define RAIL 1  // pump/sensor 1 is rail

typedef struct {
  String name; // the name
  gpio_num_t pin; // the gpio pin
  int virtualPin;
  unsigned long timeOn; // the time switched on
} PUMP;

PUMP pump[PLACES] = {
  { "wall", GPIO_NUM_2, V0, 0 },
  { "rail",  GPIO_NUM_4, V1, 0 }
};

typedef struct {
  String name;
  gpio_num_t pin;
  uint16_t value[SENSORREADS];
  int virtualPin;
  double dryValue;
  double wetValue;
  bool dryReported;
} SENSOR;

SENSOR sensor[PLACES] = {
  { "wall", GPIO_NUM_32, {0}, V7, 2.15, 1.12, false},
  { "rail",  GPIO_NUM_34, {0}, V8, 2.15, 1.28, false}
};

#define US_TO_S 1000000ULL

#define WATERAMOUNT 1 // liter
#define PUMPCAPACITY 240 // liter per hour
#define HYSTERESIS 0.3 // volt
#define LOWVOLTAGE 12.0 // 11.7 apprx 20% for lead battery. Way too low.
/**
 * There is a voltage reducing resistor pair
 * from the 12 V battery to gnd
 * of 100 K and 27 K (range 12.7 V maps to 2.7 V for the ESP ADC)
 * The real values are defined here but
 * there is also a correction factor to compensate for the fact that
 * the system reported 12.02 V where I measured 12.2 V at the battery
 **/

#define R1 98.3 // K Ohm
#define R2 26.5 // K Ohm
#define CORRECTION (12.2/12.02) // voltmeter value / ESP 32 reported value

#define ADCVoltagePin   35  // GPIO 35

const char* ssid = "vanPutte";
const char* password = "vanputte";

unsigned long wateringPeriod = (WATERAMOUNT * 60 * 60 * 1000) / PUMPCAPACITY; // milliseconds
double BatteryVoltage;
unsigned long secondsToSleep;
bool connected;

// RTC_DATA_ATTR vars are preserved during the sleep
RTC_DATA_ATTR bool batteryLow;
RTC_DATA_ATTR uint16_t v[VOLTAGEREADS];

BlynkTimer timer;

// This function is called every time the Virtual Pin 0 state changes
BLYNK_WRITE(V0)
{
  int value = param.asInt();
  if (value > 0) {
    pumpOn(pump[WALL]);
  }
}

// This function is called every time the Virtual Pin 1 state changes
BLYNK_WRITE(V1)
{
  int value = param.asInt();
  if (value > 0) {
    pumpOn(pump[RAIL]);
  }
}
// This function is called every time the Virtual Pin 3 state changes
BLYNK_WRITE(V3)
{
  int value = param.asInt();
  if (value > 0) {
    // switch on both pumps
    Blynk.virtualWrite(V0, 1);
    pumpOn(pump[WALL]);
    Blynk.virtualWrite(V1, 1);
    pumpOn(pump[RAIL]);
    Blynk.virtualWrite(V3, 0);
  }
}

// This function is called every time the device is connected to the Blynk.Cloud
BLYNK_CONNECTED()
{
  Serial.printf("Connected at %s", getTimeStr().c_str());
}

/**
switch pump on
*/
void pumpOn(PUMP& p) {
  if (!p.timeOn) {  
    Serial.printf("pump %s on\n", p.name.c_str());
    digitalWrite(p.pin, LOW);
    p.timeOn = millis();
  }
}
/**
switch pump off
*/
void pumpOff(PUMP& p) {
  Serial.printf("pump %s off\n", p.name.c_str());
  digitalWrite(p.pin, HIGH);
  p.timeOn = 0;
}

/**
 * read the battery voltage
 */
double readVoltage() {
  return ReadAverage(v, VOLTAGEREADS, analogRead(ADCVoltagePin)) * ((R1 + R2)/R2) * CORRECTION;  
}
/**
 * update the voltage level in the UI
 */
void updateVoltage() {
  double BatteryVoltage = readVoltage();
  Serial.printf("Battery Voltage: %4.2f V (%slow)\n", BatteryVoltage, batteryLow ? "" : "not ");
  if (connected) {
    Blynk.virtualWrite(V2, BatteryVoltage);
  }
  if (batteryLow) {
    if (BatteryVoltage > LOWVOLTAGE + HYSTERESIS) {
      batteryLow = false;
      Serial.println("Battery no longer low");
    }
  } else {
    if (BatteryVoltage < LOWVOLTAGE - HYSTERESIS) {
      batteryLow = true;
      Serial.println("Battery low now");
      if (connected) {
        Blynk.logEvent("low_battery");
      }
    }
  }
}

/**
 * moisture level 0-100 %
 * voltage goes down when wet..
 */
int moistLevel(SENSOR& s) {
    uint16_t value = analogRead(s.pin);
    double voltage = ReadAverage(s.value, SENSORREADS, value);
    double percentage = 100 - (100.0 * (voltage - s.wetValue) / (s.dryValue - s.wetValue));
    return percentage;  
}

/**
 * update the moistlevel in the UI
 */
void updateSensors() {
  for (int p = 0; p < PLACES; p++) {
    SENSOR s = sensor[p];
    int level = moistLevel(s);
    bool dry = (level < 5);
    Serial.printf("sensor %s %d\% -> (%s dry)\n", s.name.c_str(), level, dry ? "too" : "not");
    if (connected) {
      Blynk.virtualWrite(s.virtualPin, level);
      int vpin = pump[p].virtualPin;
      if (dry && !s.dryReported) {
        Blynk.virtualWrite(vpin, 1);
        Blynk.logEvent((p==RAIL) ? "hekdroog" : "muurdroog");
        s.dryReported = true;           
      }
      // no longer dry
      if (!dry && s.dryReported) {
        Blynk.virtualWrite(vpin, 0);
        s.dryReported = false;
      }
    } else {
      if (dry) {
        pumpOn(pump[p]);
      }
    }
  }
}

// This function is called every second when awake.
void myTimerEvent()
{
  int uptime = millis() / 1000;
  bool canSleep = (uptime > 1);
  Serial.printf("up %d s.\n", uptime);
  if (connected) {
    // makeTimePeriodString(awakeSeconds + uptime)
    Blynk.virtualWrite(V5, getTimeStr());
  }
  updateVoltage();
  updateSensors();
  for (int p = 0; p < PLACES; p++) {
    unsigned long timeOn = pump[p].timeOn;
    // this pump is on
    if (timeOn != 0) {
      Serial.printf("%s on\n", pump[p].name);
      canSleep = false;
      if (millis() > timeOn + wateringPeriod) {
        // switch off pump
        pumpOff(pump[p]);
        if (connected) {
          // sync state to web
          Blynk.virtualWrite(p, 0);
        }
      }
    }
  }
  if (canSleep) {
    Serial.printf("Going to sleep now for %d sec\n", secondsToSleep);
    pumpsDisable();
    Serial.flush();
    if (connected) {
      Blynk.virtualWrite(V4, 0);
    }
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    esp_deep_sleep_start();
  }
}
void pumpsEnable() {
  for (int p = 0; p < PLACES; p++) {
    // before setting the pinmode set the output high (low is active)
    digitalWrite(pump[p].pin, HIGH);
    pinMode(pump[p].pin, OUTPUT);
  }
}
void pumpsDisable() {
  for (int p = 0; p < PLACES; p++) {
    // before setting the pinmode set the output high (low is active)
    digitalWrite(pump[p].pin, HIGH);
    pinMode(pump[p].pin, INPUT);
  }
}

/**
 * try connect to the WiFi for 3 seconds
 */
bool tryConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  for (int i=0; i<5; i++) {
    BlynkDelay(500);
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
  }
  connected = WiFi.status() == WL_CONNECTED;
  Serial.printf("%sconnected to the WiFi.\n", connected ? "" : "NOT ");
  return connected;
}

void setup() {
  Serial.begin(115200);
  Serial.printf("pump capacity %d L/H. Wateringamount %d L --> %d seconds on.\n", PUMPCAPACITY, WATERAMOUNT, wateringPeriod / 1000);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pumpsEnable();
  if (tryConnect()) {
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
    Blynk.virtualWrite(V4, 1);
    // get the state from the web
    Blynk.syncVirtual(V0, V1, V3);
    Blynk.virtualWrite(V5, getTimeStr());
    Blynk.virtualWrite(V6, timePeriodStringToSunup());
  }
  // myTimerEvent to be called every second
  timer.setInterval(1000L, myTimerEvent);
  secondsToSleep = sleepDuration();
  esp_sleep_enable_timer_wakeup(secondsToSleep * US_TO_S);
  Serial.printf("set wakeup timer for %d seconds.\n", secondsToSleep);
}

/**
 * read both sensors
 */
void readSensors() {
  for (int s = 0; s < PLACES; s++) {
    uint16_t value = analogRead(sensor[s].pin);
    ReadAverage(sensor[s].value, SENSORREADS, value);
  }
}

/**
 * loop
 */
void loop() {
  Blynk.run();
  timer.run();
  readVoltage();
  readSensors();
}
