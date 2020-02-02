#include <EEPROM.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

#include "secrets.h"

#define MQTT_TOPIC_PREFIX "sensors/"

#define WITTY_BOARD 1

#ifdef WIOLINK_BOARD
#define DHTPIN 14     // what pin we're connected to
#define PIN_GROVE_POWER 15
#define DHTTYPE DHT22   // DHT 22  (AM2302)
#endif

#ifdef WITTY_BOARD
#define DHTPIN 14     // what pin we're connected to
#define DHTTYPE DHT11   // DHT 22
#endif

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

char *make_topic(char * suffix)
{
  static char topic[sizeof(MQTT_TOPIC_PREFIX) + sizeof(node_id) + 16];
  strcpy(topic, MQTT_TOPIC_PREFIX);
  strcat(topic, node_id);
  strncat(topic, suffix, 16);
  topic[sizeof(topic)-1] = '\0';
  return topic;
}


void setup() {
  String mac_address;
  Serial.begin(115200);
  Serial.println("hey");
  WiFi.begin(WIFISSID, WIFIPASSWORD);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  set_node_id(WiFi.macAddress().c_str());
  Serial.println(make_topic("/#"));

  psClient.setServer(MQTT_SERVER, 1883);
  pinMode(DHTPIN, INPUT);
#ifdef PIN_GROVE_POWER
  pinMode(PIN_GROVE_POWER, INPUT_PULLUP);
#endif
  dht.begin();
}

bool try_connect_mqtt(PubSubClient psClient) {
  if(psClient.connected())
    return true;

  Serial.println("connecting to mqtt");
  if(psClient.connect("ESP8266Client", MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("connected");
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.print(psClient.state());
    Serial.println(", trying again in 5 seconds");
    return false;
  }
}

void loop() {
  struct { float humidity, temperature; } readings  = {0,0 };

  while(! try_connect_mqtt(psClient)) {
    delay(5000);
  }

  if(!dht.readTempAndHumidity((float *)&readings)){
    psClient.publish(make_topic("/temperature"), String(readings.temperature).c_str());
    psClient.publish(make_topic("/humidity"), String(readings.humidity).c_str());
    psClient.disconnect(); // this ensures the messages are flushed before we sleep
    Serial.println("MQTT pub done, going into deep sleep");
    ESP.deepSleep(2 * 60 * 1e6);
  }
  else{
     Serial.println("Failed to get temperature and humidity value, retrying");
     // apparently it takes around 250ms to read the sensor, so wait longer than that
     delay(5000);
  }
}
