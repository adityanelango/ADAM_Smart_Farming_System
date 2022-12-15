#ifndef __MQTT_SERVER_SETTING_H__
#define __MQTT_SERVER_SETTING_H__

//Dans
const char MQTT_SERVER_HOST_NAME[] = "";
const char MQTT_CLIENT_ID[] = "";
const char MQTT_USERNAME[] = "";
const char MQTT_PASSWORD[] = "";
const char MQTT_TOPIC_PUB[] = "sdkTest/sub";
const char MQTT_TOPIC_SUB[] = "sdkTest/sub";

const int MQTT_SERVER_PORT = 8883;

/*
 * Root CA certificate here in PEM format.
 * "-----BEGIN CERTIFICATE-----\n"
 * ...
 * "-----END CERTIFICATE-----\n";
 */
const char* SSL_CA_PEM =
/* Amazon Root CA 1 */
"-----BEGIN CERTIFICATE-----\n"

"-----END CERTIFICATE-----\n"
/* */
/* Verysign */
"-----BEGIN CERTIFICATE-----\n"

"-----END CERTIFICATE-----\n"
/* */;

/*
 * (optional) Client certificate here in PEM format.
 * Set NULL if you don't use.
 * "-----BEGIN CERTIFICATE-----\n"
 * ...
 * "-----END CERTIFICATE-----\n";
 */
//Mike's

// "-----END CERTIFICATE-----\n";

//Dan's
const char* SSL_CLIENT_CERT_PEM = 
"-----BEGIN CERTIFICATE-----\n"

"-----END CERTIFICATE-----";


/*
 * (optional) Client private key here in PEM format.
 * Set NULL if you don't use.
 * "-----BEGIN RSA PRIVATE KEY-----\n"
 * ...
 * "-----END RSA PRIVATE KEY-----\n";
 */
 //Mike's
// const char* SSL_CLIENT_PRIVATE_KEY_PEM = 
// "-----BEGIN RSA PRIVATE KEY-----\n"

// "-----END RSA PRIVATE KEY-----\n";

//Dan's
const char* SSL_CLIENT_PRIVATE_KEY_PEM = 
"-----BEGIN RSA PRIVATE KEY-----\n"

"-----END RSA PRIVATE KEY-----";


#endif /* __MQTT_SERVER_SETTING_H__ */
