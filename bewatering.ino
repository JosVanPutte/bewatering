#define BLYNK_TEMPLATE_ID "TMPLE44qhOY2"
#define BLYNK_TEMPLATE_NAME "Bewatering"
#define BLYNK_AUTH_TOKEN "-----------------------"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "ReadADC.h"
#include "GetTime.h"

typedef struct {
  String name; // the name
  gpio_num_t pin; // the gpio pin
  unsigned long timeOn; // the time switched on
} PUMP;

#define MUUR 0 // pump 0 is muur
#define HEK 1  // pump 1 is hek
#define PUMPS 2 // nr of pumps
PUMP pump[PUMPS] = {
  { "muur", GPIO_NUM_0, 0 },
  { "hek", GPIO_NUM_4, 0 }
};

#define US_TO_S 1000000ULL

#define WATERAMOUNT 1 // liter
#define PUMPCAPACITY 240 // liter per hour
#define HYSTERESIS 0.5 // volt
#define LOWVOLTAGE 10.8

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

#define ADCVoltagePin 35  // GPIO 35 (Analog input, ADC1_CH7))

const char* ssid = "-----";
const char* password = "*******";

unsigned long wateringPeriod = (WATERAMOUNT * 60 * 60 * 1000) / PUMPCAPACITY; // milliseconds
unsigned long secondsToSleep = 30; // default sleep half a minute
double BatteryVoltage;
int uptime;

// RTC_DATA vars are preserved during the sleep
RTC_DATA_ATTR bool batteryLow;
RTC_DATA_ATTR uint16_t v[3]; // 3 battery voltage readings

RTC_DATA_ATTR unsigned long sleepSeconds;
RTC_DATA_ATTR unsigned long awakeSeconds;

BlynkTimer timer;

// This function is called every time the Virtual Pin 0 state changes
BLYNK_WRITE(V0)
{
  int value = param.asInt();
  if (value > 0) {
    pumpOn(pump[MUUR]);
  }
}

// This function is called every time the Virtual Pin 1 state changes
BLYNK_WRITE(V1)
{
  int value = param.asInt();
  if (value > 0) {
    pumpOn(pump[HEK]);
  }
}
// This function is called every time the Virtual Pin 3 state changes
BLYNK_WRITE(V3)
{
  int value = param.asInt();
  if (value > 0) {
    // switch on both pumps
    Blynk.virtualWrite(V0, 1);
    pumpOn(pump[MUUR]);
    Blynk.virtualWrite(V1, 1);
    pumpOn(pump[HEK]);
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
  Serial.printf("pump %s on\n", p.name.c_str());
  digitalWrite(p.pin, LOW);
  p.timeOn = millis();
}
/**
switch pump off
*/
void pumpOff(PUMP& p) {
  Serial.printf("pump %s off\n", p.name.c_str());
  digitalWrite(p.pin, HIGH);
  p.timeOn = 0;
}

void updateVoltage() {
  double BatteryVoltage = ReadAverage(v, 3, analogRead(ADCVoltagePin)) * ((R1 + R2)/R2) * CORRECTION;
  Serial.printf("Battery Voltage: %4.2f V (%slow)\n", BatteryVoltage, batteryLow ? "" : "not ");
  Blynk.virtualWrite(V2, BatteryVoltage);
  if (batteryLow) {
    if (BatteryVoltage > LOWVOLTAGE + HYSTERESIS) {
      batteryLow = false;
      Serial.println("Battery no longer low");
    }
  } else {
    if (BatteryVoltage < LOWVOLTAGE - HYSTERESIS) {
      batteryLow = true;
      Serial.println("Battery low now");
      Blynk.logEvent("low_battery");
    }
  }
}
bool batteryConnected() {
  for (int i = 0; i < 3; i++) {
    if (v[i] == 0) return false;
  }
  return true;
}
// This function is called every second when awake.
void myTimerEvent()
{
  uptime = millis() / 1000;
  bool canSleep = (uptime > 1);
  Serial.printf("up %d s.\n", uptime);
  Blynk.virtualWrite(V5, makeTimeString(awakeSeconds + uptime));
  if (!batteryConnected()) {
    if (!batteryLow) {
      Serial.printf("Battery not connected. Not sleeping.\n");
      batteryLow = true;
    }
    canSleep = false;
  }
  updateVoltage();
  for (int p = 0; p < PUMPS; p++) {
    unsigned long timeOn = pump[p].timeOn;
    // this pump is on
    if (timeOn != 0) {
      Serial.printf("%s on\n", pump[p].name);
      canSleep = false;
      if (millis() > timeOn + wateringPeriod) {
        // switch off pump
        pumpOff(pump[p]);
        // sync state to web
        Blynk.virtualWrite(p, 0);
      }
    }
  }
  if (canSleep) {
    awakeSeconds += millis() / 1000;
    sleepSeconds += secondsToSleep;
    Serial.printf("Going to sleep now for %d sec\n", secondsToSleep);
    Serial.flush();
    Blynk.virtualWrite(V4, 0);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
    esp_deep_sleep_start();
  }
}

/**
 * zeropad time values
 */
String zeroPad(int n) {
  String retval = String(n);
  if (n >= 10) {
    return retval;
  }
  return String("0") + retval;
}

/**
 * make a readable time string
 */
String makeTimeString(unsigned long units) {
  int s = units % 60;
  units = units / 60;
  int m = units % 60;
  units = units / 60;
  int h = units % 24;
  units = units / 24;
  String timeString = String(units) + " days " + zeroPad(h) + ":" + zeroPad(m) + ":" + zeroPad(s);
  Serial.printf("%d d, %d h, %d m, %d s -> %s\n", units, h, m , s, timeString.c_str());
  return  timeString;
}

void setup() {
  Serial.begin(115200);
  Serial.printf("pump capacity %d L/H. Wateringamount %d L --> %d seconds on.\n", PUMPCAPACITY, WATERAMOUNT, wateringPeriod / 1000);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, password);
  Blynk.virtualWrite(V4, 1);
  for (int p = 0; p < PUMPS; p++) {
    // before setting the pinmode set the output high (low is active)
    digitalWrite(pump[p].pin, HIGH);
    pinMode(pump[p].pin, OUTPUT);
  }
  // get the state from the web
  Blynk.syncVirtual(V0, V1, V3);
  Blynk.virtualWrite(V5, makeTimeString(awakeSeconds));
  Blynk.virtualWrite(V6, makeTimeString(sleepSeconds));
  // myTimerEvent to be called every second
  timer.setInterval(1000L, myTimerEvent);
  if (atNight()) {
    Serial.println("It is night so sleep for an hour.");
    secondsToSleep = 3600; // an hour
  }
  esp_sleep_enable_timer_wakeup(secondsToSleep * US_TO_S);
  Serial.printf("set wakeup timer for %d seconds.\n", secondsToSleep);
}

void loop() {
  Blynk.run();
  timer.run();
}
