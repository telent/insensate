#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <math.h>

#include "secrets.h"

#define MQTT_TOPIC_PREFIX "sensors/"
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

WiFiClient espClient;
PubSubClient psClient(espClient);


char node_id[12];

char * set_node_id(const char * mac_address)
{
  unsigned int i;
  char *p = node_id;
  for(i=0; i < strlen(mac_address); i+=3) {
    *p++ = mac_address[i];
    *p++ = mac_address[i+1];
  }
  *p++ = '\0';
  return node_id;
}

char *make_topic(const char * suffix)
{
  static char topic[sizeof(MQTT_TOPIC_PREFIX) + sizeof(node_id) + 16];
  strcpy(topic, MQTT_TOPIC_PREFIX);
  strcat(topic, node_id);
  strncat(topic, suffix, 16);
  topic[sizeof(topic)-1] = '\0';
  return topic;
}

static void enable_sensor_power(const bool enabled)
{
#ifdef PIN_SENSOR_POWER
  // turn on power to Grove sockets
  // https://github.com/Seeed-Studio/Wio_Link/wiki/Advanced-User-Guide
  pinMode(PIN_SENSOR_POWER, OUTPUT);
  digitalWrite(PIN_SENSOR_POWER, enabled ? 1 : 0);
#endif
}

void connect_wifi()
{
  notify_progress(wifi);
  WiFi.begin(WIFISSID, WIFIPASSWORD);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  notify_complete(wifi);
  set_node_id(WiFi.macAddress().c_str());
}


void setup() {
  String mac_address;
  Serial.begin(115200);
  Serial.println("hey");
  connect_wifi();
  Serial.println(make_topic("/#"));

  psClient.setServer(MQTT_SERVER, 1883);
  notify_progress(sensor);

  notify_complete(sensor);
  notify_complete(broker);
  notify_complete(wifi);

  dht.begin();
}

bool try_connect_mqtt() {
  if(psClient.connected())
    return true;
  notify_progress(broker);

  Serial.println("connecting to mqtt");
  if(psClient.connect("ESP8266Client", MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("connected");
    Serial.println(psClient.state());
    psClient.publish(make_topic("/online"), "\1");
    notify_complete(broker);
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.print(psClient.state());
    Serial.println(", trying again in 5 seconds");
    return false;
  }
}

void publish_val(char *topic, float val) {
  char payload[100];
  int chars = snprintf(payload, sizeof payload, "%f", val);
  psClient.publish(make_topic(topic), payload, chars);
}

void loop() {
  float temperature, humidity;
  int val;
  while(! try_connect_mqtt()) {
    delay(5000);
  }
  enable_sensor_power(true);
  delay(5000);
  temperature = dht.readTemperature(false, true);
  humidity = dht.readHumidity();
  notify_complete(sensor);
  psClient.loop();
  publish_val("/voltage", ESP.getVcc()/1000.0);
  if(! isnan(temperature)){
    publish_val("/temperature", temperature);
    publish_val("/humidity", humidity);
    psClient.loop();
    psClient.disconnect(); // this ensures the messages are flushed before we sleep
    yield();
    enable_sensor_power(false);
    Serial.println("MQTT pub done, going into deep sleep");
    ESP.deepSleep(120*1e6);
  }
  else{
    Serial.println("Failed to get temperature and humidity value, retrying");
    // apparently it takes around 250ms to read the sensor, so wait longer than that
    notify_progress(sensor);
    delay(5000);
  }
}
