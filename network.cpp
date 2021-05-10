#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

#include "network.h"
#include "secrets.h"

static WiFiClient espClient;

// https://www.bakke.online/index.php/2017/06/24/esp8266-wifi-power-reduction-avoiding-network-scan/

// The ESP8266 RTC memory is arranged into blocks of 4 bytes. The
// access methods read and write 4 bytes at a time, so the RTC data
// structure should be padded to a 4-byte multiple.
struct wifi_settings {
  uint32_t crc32;   // 4 bytes
  uint8_t channel;  // 1 byte,   5 in total
  uint8_t bssid[6]; // 6 bytes, 11 in total
  uint8_t padding;  // 1 byte,  12 in total
} wifi_settings;

static struct wifi_settings * read_wifi_settings() {
  static bool rtc_valid = false;
  if(!rtc_valid) {
    if(ESP.rtcUserMemoryRead(0, (uint32_t*)&wifi_settings,
			     sizeof (wifi_settings))) {
      // Calculate the CRC; skip the first 4 bytes as that's the
      // checksum itself.
      uint32_t crc = crc32(((uint8_t*)&wifi_settings) + 4,
			   sizeof (wifi_settings) - 4);
      if( crc == wifi_settings.crc32 ) {
	rtc_valid = true;
      }
    }
  }
  return rtc_valid ? &wifi_settings : (struct wifi_settings *) 0;
}

static void write_wifi_settings() {
  uint32_t previous_crc = wifi_settings.crc32;
  // Write current connection info back to RTC
  wifi_settings.channel = WiFi.channel();
  memcpy(wifi_settings.bssid, WiFi.BSSID(), 6);
  wifi_settings.crc32 = crc32(((uint8_t*) &wifi_settings) + 4,
			      (sizeof wifi_settings) - 4);
  if(wifi_settings.crc32 != previous_crc) {
    ESP.rtcUserMemoryWrite(0,
			   (uint32_t*) &wifi_settings,
			   sizeof wifi_settings);
  }
}

static bool attempt_wifi_connect(struct wifi_settings * settings){
  delay(10);

  Serial.print("Connecting to ");
  Serial.println(WIFISSID);
  int retries = 0;

  WiFi.mode(WIFI_STA);

  if(settings) {
    WiFi.begin(WIFISSID, WIFIPASSWORD,
	       settings->channel, settings->bssid, true );
  } else {
      WiFi.begin(WIFISSID, WIFIPASSWORD);
  }
  while (WiFi.status() != WL_CONNECTED) {
    if((retries > 100) && settings) {
      WiFi.disconnect(); delay(10);
      WiFi.forceSleepBegin(); delay(10);
      WiFi.forceSleepWake(); delay(10);
      return false;
    }
    if(retries > 600) {
      WiFi.disconnect(true);
      delay(1);
      WiFi.mode(WIFI_OFF);
      ESP.deepSleep(60 * 5 * 1000 * 1000, WAKE_RF_DISABLED );
      return false; // notreached
    }
    delay(50);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  return true;
}

WiFiClient& connect_wifi() {
  attempt_wifi_connect(read_wifi_settings()) || attempt_wifi_connect(0);
  write_wifi_settings();
  return espClient;
}

void setup_mqtt(PubSubClient& mqttClient, MQTT_CALLBACK_SIGNATURE) {
  set_node_id(WiFi.macAddress().c_str());
  mqttClient.setServer(MQTT_SERVER, 1883);
  if(callback) {
      Serial.println("mqtt callack");
      mqttClient.setCallback(callback);
  }
}

bool mqtt_reconnect(PubSubClient& mqttClient) {
  char topic[80];
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(node_id, MQTT_USER, MQTT_PASSWORD )) {
      mqttClient.publish(make_topic(topic, sizeof topic, "/online"),
			 WiFi.localIP().toString().c_str());
      Serial.print("success");
      return true;
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
