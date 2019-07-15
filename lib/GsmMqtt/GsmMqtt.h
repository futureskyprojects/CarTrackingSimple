#ifndef GSM_MQTT_H_INCLUDED
#define GSM_MQTT_H_INCLUDED

#include <string.h>
#include <stdint.h>

void mqtt_connect_message(uint8_t *, char *);

void mqtt_connect_message(uint8_t *, char *, char *, char *);

void mqtt_publish_message(uint8_t *, char *, char *);

void mqtt_disconnect_message(uint8_t *);

#endif // MQTT_H_INCLUDED