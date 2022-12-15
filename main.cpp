/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    TEAM - A.D.A.M.
 *******************************************************************************/

 /**
  This is a program to illustrate the use of the MQTT Client library
  on the mbed platform to read or data and send it to AWS IoT Core.
  The Client class requires two classes which mediate
  access to system interfaces for networking and timing.  As long as these two
  classes provide the required public programming interfaces, it does not matter
  what facilities they use underneath. In this program, they use the mbed
  system libraries.
 */

#include <string>
#define MQTTCLIENT_QOS1 0
#define MQTTCLIENT_QOS2 0

#include "mbed.h"
#include "NTPClient.h"
#include "TLSSocket.h"
#include "MQTTClientMbedOs.h"
#include "MQTT_server_setting.h"
#include "mbed-trace/mbed_trace.h"
#include "mbed_events.h"
#include "mbedtls/error.h"

#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
 
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <cstdio>
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_hsensor.h"

#include <cstdint>


DigitalOut Water_Turbidity_Power(D8);
DigitalOut Water_Turbidity_Ground(D9);
DigitalOut Soil_Moisture_Power(D13);

//Analog
AnalogIn Water_Ph_Analog_Output(A0);
AnalogIn Water_Temperature_Analog_Output(A1);
AnalogIn Soil_Moisture_Analog_Output(A2);
AnalogIn Water_Turbidity_Analog_Output(A3);


DigitalOut led1(LED1);
InterruptIn button(BUTTON1);
volatile int userbtn_pressed_count = 0;

// Toogle variable intialized to blink Led1
volatile bool toggleLED1 = false;
float raw_sensor_value = 0;
float voltage = 0;
float normalized_sensor_value = 0;

/* Soil moisture threshold values based on calibration */
#define soilWet 500   // Define max value we consider soil 'wet'
#define soilDry 750   // Define min value we consider soil 'dry'


#define LED_ON  MBED_CONF_APP_LED_ON
#define LED_OFF MBED_CONF_APP_LED_OFF

static volatile bool isPublish = false;

/* Flag to be set when received a message from the server. */
static volatile bool isMessageArrived = false;
/* Buffer size for a receiving message. */
const int MESSAGE_BUFFER_SIZE = 256;
/* Buffer for a receiving message. */
char messageBuffer[MESSAGE_BUFFER_SIZE];

/* Enable GPIO power for Wio target */
#if defined(TARGET_WIO_3G) || defined(TARGET_WIO_BG96)
DigitalOut GrovePower(GRO_POWR, 1);
#endif

// An event queue is a very useful structure to debounce information between contexts (e.g. ISR and normal threads)
// This is great because things such as network operations are illegal in ISR, so updating a resource in a button's fall() function is not allowed
EventQueue eventQueue;
Thread thread1;

/*
 * Callback function called when a message arrived from server.
 */
void messageArrived(MQTT::MessageData& md)
{
    // Copy payload to the buffer.
    MQTT::Message &message = md.message;
    if(message.payloadlen >= MESSAGE_BUFFER_SIZE) {
        // TODO: handling error
    } else {
        memcpy(messageBuffer, message.payload, message.payloadlen);
    }
    messageBuffer[message.payloadlen] = '\0';

    isMessageArrived = true;
}

/*
 * Callback function called when the button1 is clicked.
 */
void btn1_rise_handler() {
    isPublish = true;
}

void sensor_setup()
{
    BSP_TSENSOR_Init();
    BSP_HSENSOR_Init();
    
    Water_Turbidity_Power = 1;
    Water_Turbidity_Ground = 0;
    Soil_Moisture_Power = 1;

}

void sensor_read_status()
{
    userbtn_pressed_count ++; // Set flag to true to show button has been pressed
}

//reads the values from PH sensor
float read_water_ph()
{  

    raw_sensor_value = Water_Ph_Analog_Output.read();
    voltage = raw_sensor_value*5;
    normalized_sensor_value = 14/voltage;

    return normalized_sensor_value;
    
}

float read_water_turbidity()
{
    
    raw_sensor_value = Water_Turbidity_Analog_Output.read();
//    printf("\nTURBIDITY = %.2f * vcc\n", sensor_value);
    voltage = raw_sensor_value * (5.0);
//    printf("\nTURBIDITY Voltage = %.2f V\n", voltage);
 //   //4.2v
//    if(voltage < 2.5){
//        ntu = 3000;
//    }else{
//        ntu = -1120.4*sqrt(voltage)+5742.3*voltage-4353.8; 
//    }
//    printf("\nTURBIDITY ntu4.2 = %.2f ntu4.2\n", ntu);
    // 5v
    normalized_sensor_value = (-781.25*voltage) + 3000;
//    printf("\nTURBIDITY ntu5 = %.2f ntu5\n", ntu);
    
    // print the percentage and 16 bit normalized values
//    printf("TURBIDITY Percentage: %3.3f%%\n", turbidity.read()*100.0f);
//    printf("TURBIDITY normalized: 0x%04X \n", turbidity.read_u16());
    
    return normalized_sensor_value;
    
}

float read_soil_moisture()
{  
    
    raw_sensor_value = Soil_Moisture_Analog_Output.read();
    voltage = 1-raw_sensor_value;
    normalized_sensor_value = voltage*100;

    return normalized_sensor_value;

}

float read_atmospheric_temperature()
{  
    raw_sensor_value = BSP_TSENSOR_ReadTemp();

    return raw_sensor_value;

}

float read_atmospheric_humidity()
{  
    raw_sensor_value = BSP_HSENSOR_ReadHumidity();
   
    return raw_sensor_value;
    
}

//initial values
float humidity = 0;
float sensor_value = 0;

int main(int argc, char* argv[])
{
sensor_setup();
BSP_TSENSOR_Init();
BSP_HSENSOR_Init();
    float w_ph;
    float w_tur;
    float s_mois;
    float a_temp;
    float a_hum;

    thread_sleep_for(100);
    mbed_trace_init();
    
    const float version = 1.0;
    bool isSubscribed = false;

    NetworkInterface* network = NULL;
    TLSSocket *socket = new TLSSocket; // Allocate on heap to avoid stack overflow.
    MQTTClient* mqttClient = NULL;

    DigitalOut led(MBED_CONF_APP_LED_PIN, LED_ON);

    printf("HelloMQTT: version is %.2f\r\n", version);
    printf("\r\n");

#ifdef MBED_MAJOR_VERSION
    printf("Mbed OS version %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#endif

    printf("Opening network interface...\r\n");
    {
        network = NetworkInterface::get_default_instance();
        if (!network) {
            printf("Error! No network inteface found.\n");
            return -1;
        }

        printf("Connecting to network\n");
        nsapi_size_or_error_t ret = network->connect();
        if (ret) {
            printf("Unable to connect! returned %d\n", ret);
            return -1;
        }
    }
    printf("Network interface opened successfully.\r\n");
    printf("\r\n");

    // sync the real time clock (RTC)
    NTPClient ntp(network);
    ntp.set_server("time.google.com", 123);
    time_t now = ntp.get_timestamp();
    set_time(now);
    printf("Time is now %s", ctime(&now));

    printf("Connecting to host %s:%d ...\r\n", MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT);
    {
        nsapi_error_t ret = socket->open(network);
        if (ret != NSAPI_ERROR_OK) {
            printf("Could not open socket! Returned %d\n", ret);
            return -1;
        }
        ret = socket->set_root_ca_cert(SSL_CA_PEM);
        if (ret != NSAPI_ERROR_OK) {
            printf("Could not set ca cert! Returned %d\n", ret);
            return -1;
        }
        ret = socket->set_client_cert_key(SSL_CLIENT_CERT_PEM, SSL_CLIENT_PRIVATE_KEY_PEM);
        if (ret != NSAPI_ERROR_OK) {
            printf("Could not set keys! Returned %d\n", ret);
            return -1;
        }
        ret = socket->connect(MQTT_SERVER_HOST_NAME, MQTT_SERVER_PORT);
        if (ret != NSAPI_ERROR_OK) {
            printf("Could not connect! Returned %d\n", ret);
            return -1;
        }
    }
    printf("Connection established.\r\n");
    printf("\r\n");

    printf("MQTT client is trying to connect the server ...\r\n");
    {
        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.MQTTVersion = 3;
        data.clientID.cstring = (char *)MQTT_CLIENT_ID;
        data.username.cstring = (char *)MQTT_USERNAME;
        data.password.cstring = (char *)MQTT_PASSWORD;

        mqttClient = new MQTTClient(socket);
        int rc = mqttClient->connect(data);
        if (rc != MQTT::SUCCESS) {
            printf("ERROR: rc from MQTT connect is %d\r\n", rc);
            return -1;
        }
    }
    printf("Client connected.\r\n");
    printf("\r\n");


    printf("Client is trying to subscribe a topic \"%s\".\r\n", MQTT_TOPIC_SUB);
    {
        int rc = mqttClient->subscribe(MQTT_TOPIC_SUB, MQTT::QOS0, messageArrived);
        if (rc != MQTT::SUCCESS) {
            printf("ERROR: rc from MQTT subscribe is %d\r\n", rc);
            return -1;
        }
        isSubscribed = true;
    }
    printf("Client has subscribed a topic \"%s\".\r\n", MQTT_TOPIC_SUB);
    printf("\r\n");

    // Enable button 1
    InterruptIn btn1(PC_13);
    btn1.rise(btn1_rise_handler);
    
    printf("To send a packet, push the button 1 on your board.\r\n\r\n");

    // Turn off the LED to let users know connection process done.
    led = LED_OFF;
    int countr=  0;
    int ret  = 0;
    while(1) {
        

        /* Check connection */
        if(!mqttClient->isConnected()){
            break;
        }
        /* Pass control to other thread. */
        if(mqttClient->yield() != MQTT::SUCCESS) {
            break;
        }
        /* Received a control message. */
        if(isMessageArrived) {
            isMessageArrived = false;
            // Just print it out here.
            printf("\r\nMessage arrived:\r\n%s\r\n\r\n", messageBuffer);
        }
        /* Publish data */
        if(isPublish) {
            
            //uncoment  this out if you want to send messages with the BUTTON
            //isPublish = false;
            static unsigned short id = 0;
            static unsigned int count = 0;

            count++;

            // When sending a message, LED lights blue.
            led = LED_ON;

            MQTT::Message message;
            message.retained = false;
            message.dup = false;

            const size_t buf_size = 400;
            char *buf = new char[buf_size];
            message.payload = (void*)buf;

            w_ph = read_water_ph();  // reads and writes water PH to the  variable
            w_tur = read_water_turbidity();  // reads and writes turbidity to the  variable
            s_mois = read_soil_moisture();
            a_temp = read_atmospheric_temperature();
            a_hum = read_atmospheric_humidity();

            // // / JSON string 
            // // const string json = " { \"temp\" : "+ to_string(a_temp) + ", \"humidity\" : "+ to_string(a_hum) + " , \"n\" : "+ to_string(N) + ", \"p\": "+ to_string(P) + ", \"k\": "+ to_string(K) + ", \"ph\": 8.0 } ";
            // const string json = " { \"atmosphericTemp\" : "+ to_string(a_temp) + ", \"atmosphericHumidity\" : "+ to_string(a_hum) + ", \"soilMoisture\" : "+ to_string(s_mois) + " , \"soilN\" : "+ to_string(s_nit) + ", \"soilP\": "+ to_string(s_pho) + ", \"soilK\": "+ to_string(s_pot) + ", \"soilPH\": "+ to_string(s_ph) +"} ";  
            // //const string json = " { \"atmosphericTemp\" : "+ to_string(a_temp) + ", \"atmosphericHumidity\" : "+ to_string(a_hum) + ",  \"waterTurbidity\" : "+ to_string(w_tur) + ", \"waterPH\" : "+ to_string(w_ph) + ", \"soilMoisture\" : "+ to_string(s_mois) + " , \"soilN\" : "+ to_string(s_nit) + ", \"soilP\": "+ to_string(s_pho) + ", \"soilK\": "+ to_string(s_pot) + ", \"soilPH\": "+ to_string(s_ph) +"} ";  
            // const string json2 = " { \"waterTurbidity\" : "+ to_string(w_tur) + ", \"waterPH\" : "+ to_string(w_ph) +"} ";  
            const string json = " { \"atmosphericTemp\" : "+ to_string(a_temp) + ", \"atmosphericHumidity\" : "+ to_string(a_hum) + ", \"soilMoisture\" : "+ to_string(s_mois) + " ,\"waterTurbidity\" : "+ to_string(w_tur) + ", \"waterPH\" : "+ to_string(w_ph) +" } ";  
                       
                
            printf("%s", json.c_str());
            message.qos = MQTT::QOS0;
            // message.id = id++;
            
            // if (countr % 2 == 0) {
            // ret = snprintf(buf, buf_size, "%s", json.c_str());
            // }
            // else {
            //     ret = snprintf(buf, buf_size, "%s", json2.c_str());
            // }
            // countr ++;
            int ret = snprintf(buf, buf_size, "%s", json.c_str());
            // ret = snprintf(buf, buf_size, "%s", json2.c_str());
            if(ret < 0) {
                printf("ERROR: snprintf() returns %d.", ret);
                continue;
            }

            message.payloadlen = ret;
            // Publish a message.
            printf("Publishing message.\r\n");
            int rc = mqttClient->publish(MQTT_TOPIC_SUB, message);
            
            if(rc != MQTT::SUCCESS) {
                printf("ERROR: rc from MQTT publish is %d\r\n", rc);
            }
            printf("Message published.\r\n");
            delete[] buf;    

            thread_sleep_for(3000);
            led = LED_OFF;
        }
    }

    printf("The client has disconnected.\r\n");

    if(mqttClient) {
        if(isSubscribed) {
            mqttClient->unsubscribe(MQTT_TOPIC_SUB);
        }
        if(mqttClient->isConnected()) 
            mqttClient->disconnect();
        delete mqttClient;
    }
    if(socket) {
        socket->close();
    }
    if(network) {
        network->disconnect();
        // network is not created by new.
    }
}
