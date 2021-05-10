#include <string.h>
#include <stdint.h>

// C++ functions which are useful for Arduino generally outside the
// specific requirement of dolores. Reused by copying into new
// projects

#include "secrets.h"

char node_id[13] = "000000000000";

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

char *make_topic(char *dest, int dest_bytes, const char *suffix)
{
  int prefix_length = sizeof(MQTT_TOPIC_PREFIX) + sizeof(node_id);
  char * topic = dest;
  strcpy(topic, MQTT_TOPIC_PREFIX);
  strcat(topic, node_id);
  strncat(topic, suffix, dest_bytes - prefix_length);
  topic[dest_bytes - 1] = '\0';
  return topic;
}

int string_has_suffix(char *input, char * suffix) {
  int offset = strlen(input) - strlen(suffix);
  return (offset >= 0) && !strcmp(input + offset, suffix);
}

uint32_t crc32(const uint8_t *data, size_t length ) {
  uint32_t crc = 0xffffffff;
  while( length-- ) {
    uint8_t c = *data++;
    for( uint32_t i = 0x80; i > 0; i >>= 1 ) {
      bool bit = crc & 0x80000000;
      if( c & i ) {
        bit = !bit;
      }

      crc <<= 1;
      if( bit ) {
        crc ^= 0x04c11db7;
      }
    }
  }
  return crc;
}
