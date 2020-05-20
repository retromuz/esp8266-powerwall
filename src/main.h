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
#include <SoftwareSerial.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <ESP8266HTTPClient.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define WIFI_LED 2
#define FMT_BASIC_INFO "[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]"
#define FMT_VOLTAGES "[%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]"
#define CONTENT_TYPE_APPLICATION_JSON "application/json"

#define SLAVE_I2C_ADDR 0b00100010

#define SLAVE_I2C_CMD_WRITE_TARGET_INPUT_ADC_VAL 0
#define SLAVE_I2C_CMD_WRITE_TARGET_OUTPUT_ADC_VAL 4
#define SLAVE_I2C_CMD_READ_TARGET_INPUT_ADC_VAL 1
#define SLAVE_I2C_CMD_READ_TARGET_OUTPUT_ADC_VAL 5
#define SLAVE_I2C_CMD_READ_INPUT_ADC_VAL 2
#define SLAVE_I2C_CMD_READ_OUTPUT_ADC_VAL 3
#define SLAVE_I2C_CMD_READ_PWM_VAL 6
#define SLAVE_I2C_CMD_WRITE_FREQ 7
#define SLAVE_I2C_CMD_READ_FREQ 8

#define CMD_WRITE_AUTO_MPPT 9

#define MPPT_STEP 10
#define MIN_CURRENT_FOR_PWM_INIT 100

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	unsigned int inAdc;
	unsigned int current;
} MPPTEntry;

typedef struct {
	MPPTEntry data[3];
	volatile unsigned int status;
} MPPTData;

void setupPins();
void initMpptData();
void buildMpptData();
void analyseMpptData();
void readI2CSlave();
void writeAdc(int adc);
bool setupWiFi();
void setupWebServer();
void notFound(AsyncWebServerRequest *request);
void setupNTPClient();
void setupOTA();
void handleRoot();
String readProperty(String props, String key);

#ifdef __cplusplus
} // extern "C"
#endif
#endif /* SRC_MAIN_H_ */
