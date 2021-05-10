#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <math.h>

#include "secrets.h"
#include "network.h"

#define DHTTYPE DHT22

#define WITTY_BOARD 1
#define WIOLINK_BOARD 2

// #define BOARD_TYPE WITTY_BOARD
#define BOARD_TYPE WIOLINK_BOARD

#if BOARD_TYPE == WIOLINK_BOARD
#define DHTPIN 14     // what pin we're connected to
#define PIN_SENSOR_POWER 15
#define notify_progress(c) do {} while(0)
#define notify_complete(c) do {} while(0)

#elif BOARD_TYPE == WITTY_BOARD
#define DHTPIN 5     // what pin we're connected to
#define PIN_SENSOR_POWER 14

// per http://www.icstation.com/esp8266-serial-wifi-witty-cloud-development-board-module-mini-wifi-module-smart-home-p-8154.html
// these are the pins for the onboard led
typedef enum {
	      sensor = 12, broker = 13, wifi = 15
} progress_notifier;

static inline void notify_progress(progress_notifier c) {
  pinMode((int) c, OUTPUT);
  digitalWrite((int) c, 1);
}
static inline void notify_complete(progress_notifier c) {
  pinMode((int) c, OUTPUT);
  digitalWrite((int) c, 0);
}
#endif

ADC_MODE(ADC_VCC);

DHT dht(DHTPIN, DHTTYPE);

static void enable_sensor_power(const bool enabled)
{
#ifdef PIN_SENSOR_POWER
  // turn on power to Grove sockets
  // https://github.com/Seeed-Studio/Wio_Link/wiki/Advanced-User-Guide
  pinMode(PIN_SENSOR_POWER, OUTPUT);
  digitalWrite(PIN_SENSOR_POWER, enabled ? 1 : 0);
#endif
}

// extern WiFiClient espClient;
static PubSubClient mqtt_client;

void setup() {
  char topic[80];

  Serial.begin(115200);
  Serial.println("hey");
  mqtt_client.setClient(connect_wifi());

  setup_mqtt(mqtt_client, NULL);
  Serial.println(make_topic(topic, sizeof topic, "/#"));

  notify_progress(sensor);

  notify_complete(sensor);
  notify_complete(broker);
  notify_complete(wifi);

  dht.begin();
}

void publish_val(char *suffix, float val) {
  char payload[100];
  char topic[80];
  int chars = snprintf(payload, sizeof payload, "%f", val);
  mqtt_client.publish(make_topic(topic, sizeof topic, suffix), payload);
}

void loop() {
  float temperature, humidity;

  if(!mqtt_client.loop()) mqtt_reconnect(mqtt_client);
  enable_sensor_power(true);
  delay(5000);

  publish_val("/voltage", ESP.getVcc()/1000.0);
  temperature = dht.readTemperature(false, true);
  humidity = dht.readHumidity();
  notify_complete(sensor);
  mqtt_client.loop();
  if(! isnan(temperature)){
    publish_val("/temperature", temperature);
    publish_val("/humidity", humidity);
    mqtt_client.loop();
    mqtt_client.disconnect(); // this ensures the messages are flushed before we sleep
    yield();
    enable_sensor_power(false);
    Serial.println("MQTT pub done, going into deep sleep");
    ESP.deepSleep(120*1e6);
  }
  else{
    Serial.println("Failed to get temperature and humidity value, retrying");
    // apparently it takes around 250ms to read the sensor, so wait longer than that
    notify_progress(sensor);
    mqtt_client.disconnect(); // this ensures the messages are flushed before we sleep
    delay(5000);
  }
}
