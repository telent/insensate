char * set_node_id(const char * mac_address);
char *make_topic(char *dest, int dest_bytes, const char *suffix);
int string_has_suffix(char *input, char * suffix);
uint32_t crc32(const uint8_t *data, size_t length );
WiFiClient& connect_wifi();
void setup_mqtt(PubSubClient& , MQTT_CALLBACK_SIGNATURE);
bool mqtt_reconnect(PubSubClient&);

extern char node_id[13];
