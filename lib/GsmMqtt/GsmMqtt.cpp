#include <GsmMqtt.h>

void mqtt_connect_message(uint8_t *mqtt_message, char *client_id)
{

    uint8_t i = 0;
    uint8_t client_id_length = strlen(client_id);
    uint8_t conn_packet[] = {
        0x10,                   // MQTT Message type Connect
        12u + client_id_length, // Remaining length
        0,                      // Length MSB (0)
        4,                      // Length LSB (4)
        'M',                    //
        'Q',                    //
        'T',                    //
        'T',                    //
        4,                      // Protocol level
        2,                      // Connect flag - In here only turn on clean session
        0,                      // Keep Alive MSB (0)
        15,                     // Keep Alive LSB (15)
        0,                      // ClientID Length MSB (0)
        client_id_length        // ClientID Length LSB (4)
    };
    int conn_packet_size = sizeof(conn_packet) / sizeof(uint8_t);
    for (i = 0; i < conn_packet_size; i++)
    {
        mqtt_message[i] = conn_packet[i];
    }
    for (; i < conn_packet_size + client_id_length; i++)
    {
        mqtt_message[i] = client_id[i - conn_packet_size];
    }
}

void mqtt_publish_message(uint8_t *mqtt_message, char *topic, char *message)
{

    uint8_t i = 0;
    uint8_t topic_length = strlen(topic);
    uint8_t message_length = strlen(message);

    mqtt_message[0] = 48;                                // MQTT Message type Publish
    mqtt_message[1] = 2 + topic_length + message_length; // Remaining length
    mqtt_message[2] = 0;                                 // Topic Length MSB
    mqtt_message[3] = topic_length;                      // Topic Length LSB

    // Topic
    for (i = 0; i < topic_length; i++)
    {
        mqtt_message[4 + i] = topic[i];
    }

    // Message
    for (i = 0; i < message_length; i++)
    {
        mqtt_message[4 + topic_length + i] = message[i];
    }
}

void mqtt_disconnect_message(uint8_t *mqtt_message)
{
    mqtt_message[0] = 0xE0; // msgtype = connect
    mqtt_message[1] = 0x00; // length of message (?)
}
