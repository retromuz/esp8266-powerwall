/*
 * main.h
 *
 *  Created on: Mar 18, 2020
 *      Author: prageeth
 */

#ifndef SRC_MAIN_H_
#define SRC_MAIN_H_

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <LittleFS.h>
#include <Ticker.h>
#include <WiFiClient.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <ArduinoSort.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define WIFI_LED 2
#define CONTENT_TYPE_APPLICATION_JSON "application/json"

#define SLAVE_I2C_ADDR 0b00100010

#define SLAVE_I2C_CMD_WRITE_TARGET_INPUT_ADC_VAL 0
#define SLAVE_I2C_CMD_WRITE_TARGET_OUTPUT_ADC_VAL 4
#define SLAVE_I2C_CMD_WRITE_NO_MPPT 14
#define SLAVE_I2C_CMD_READ_TARGET_INPUT_ADC_VAL 1
#define SLAVE_I2C_CMD_READ_TARGET_OUTPUT_ADC_VAL 5
#define SLAVE_I2C_CMD_READ_INPUT_ADC_VAL 2
#define SLAVE_I2C_CMD_READ_OUTPUT_ADC_VAL 3
#define SLAVE_I2C_CMD_READ_PWM_VAL 6
#define SLAVE_I2C_CMD_READ_FREQ 8
#define SLAVE_I2C_CMD_READ_OUTPUT_CURRENT_ADC_VAL 10
#define SLAVE_I2C_CMD_READ_INPUT_CURRENT_ESTIMATE 11
#define SLAVE_I2C_CMD_READ_OUTPUT_CURRENT_ESTIMATE 12
#define SLAVE_I2C_CMD_READ_MPPT_STATUS 13

#define CMD_WRITE_AUTO_MPPT 9

#define MPPT_STEP 8
#define MIN_CURRENT_FOR_PWM_INIT -1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	volatile int tiav[9];
	volatile int iav[9];
	volatile int toav[9];
	volatile int oav[9];
	volatile unsigned int ocav[9];
	volatile unsigned long ic[9];
	volatile unsigned long oc[9];
	volatile int pwm;
	volatile bool mppt;
	volatile unsigned int status;
	void sort() {
		sortArray(tiav, 9);
		sortArray(iav, 9);
		sortArray(toav, 9);
		sortArray(oav, 9);
		sortArray(ocav, 9);
		sortArray(ic, 9);
		sortArray(oc, 9);
	}
} I2CData;

void setupPins();
void readI2CSlave();
//void writeAdc(int adc);
bool setupWiFi();
void configureWiFi();
void setupWebServer();
void notFound(AsyncWebServerRequest *request);
void setupOTA();
//void handleRoot();
String readProperty(String props, String key);

#ifdef __cplusplus
} // extern "C"
#endif
#endif /* SRC_MAIN_H_ */
