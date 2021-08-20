#include "main.h"

AsyncWebServer server(80); // Create a webserver object that listens for HTTP request on port 80
Ticker ticker;

volatile uint8_t ota_loops = 0;
volatile int8_t isr_loops = 0;

String mqttServer;
String mqttUser;
String mqttPass;
String mqttId;
String mqttTopic;

HTTPClient http;
WiFiClient wifi;
PubSubClient client(wifi);

I2CData i2cData;

void ISRwatchdog() {
	readI2CSlave();
	isr_loops ++;
}

void setup(void) {
	delay(100);
	Serial.begin(115200);
	delay(100);
	setupPins();
	if (!LittleFS.begin()) {
		ESP.restart();
	}
	setupWiFi();
	digitalWrite(WIFI_LED, HIGH);
	setupWebServer();
	setupOTA();
	Wire.setClock(100000);
	Wire.begin();
	i2cData.status = 0;
	delay(500);
	ticker.attach_ms(1000, ISRwatchdog);
}

void loop(void) {
	ArduinoOTA.handle();
}

void readI2CSlave() {
	uint8_t buf[3];
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_TARGET_INPUT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		Wire.readBytes(buf, 2);
		i2cData.tiav[isr_loops] = buf[0] | buf[1] << 8;
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_TARGET_OUTPUT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		Wire.readBytes(buf, 2);
		i2cData.toav[isr_loops] = buf[0] | buf[1] << 8;
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_INPUT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		Wire.readBytes(buf, 2);
		i2cData.iav[isr_loops] = buf[0] | buf[1] << 8;
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_OUTPUT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		Wire.readBytes(buf, 2);
		i2cData.oav[isr_loops] = buf[0] | buf[1] << 8;
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_OUTPUT_CURRENT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		Wire.readBytes(buf, 2);
		unsigned int tmp = buf[0] | buf[1] << 8;
		i2cData.ocav[isr_loops] = tmp < 4000 ? tmp : i2cData.ocav[isr_loops];
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_OUTPUT_CURRENT_ESTIMATE);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 3)) {
		Wire.readBytes(buf, 3);
		unsigned long tmp = buf[0] | buf[1] << 8 | buf[2] << 16;
		i2cData.oc[isr_loops] = tmp < 100000 ? tmp : i2cData.oc[isr_loops] ;
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_INPUT_CURRENT_ESTIMATE);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 3)) {
		Wire.readBytes(buf, 3);
		unsigned long tmp = buf[0] | buf[1] << 8 | buf[2] << 16;
		i2cData.ic[isr_loops] = tmp < 100000 ? (tmp == 0 && i2cData.oc[isr_loops] > 1000 ? i2cData.ic[isr_loops] : tmp) : i2cData.ic[isr_loops];
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_PWM_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 1)) {
		i2cData.pwm = Wire.read();
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_MPPT_STATUS);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 1)) {
		Wire.readBytes(buf, 1);
		i2cData.mppt = buf[0] == 1 ? true : false;
	}
	if (isr_loops > 7) {
		i2cData.sort();
		char out[280];
		sprintf(out,
				"{\"tiav\":%d,\"iav\":%d,\"toav\":%d,\"oav\":%d,\"ocav\":%u, \"oc\":%lu, \"ic\": %lu,\"pwm\":%d,\"autoMppt\":%d}",
				i2cData.tiav[5], i2cData.iav[5], i2cData.toav[5], i2cData.oav[5], i2cData.ocav[5], i2cData.oc[5], i2cData.ic[5], i2cData.pwm, i2cData.mppt ? 1 : 0);
		client.publish(mqttTopic.c_str(), out);
		Serial.println("published to mqtt");
		if (i2cData.mppt) {
			// set autoMPPT = false, tiav=542
			Wire.beginTransmission(SLAVE_I2C_ADDR);
			Wire.write(SLAVE_I2C_CMD_WRITE_NO_MPPT);
			Wire.write(1);
			Wire.write(0);
			Wire.endTransmission();

			Wire.beginTransmission(SLAVE_I2C_ADDR);
			Wire.write(SLAVE_I2C_CMD_WRITE_TARGET_INPUT_ADC_VAL);
			Wire.write(542 & 0xff);
			Wire.write((542 & 0xff00) >> 8);
			Wire.endTransmission();
		}
		isr_loops = -1;
	}
}

void setupPins() {
	pinMode(WIFI_LED, OUTPUT);
}

String readProperty(String props, String key) {
	int index = props.indexOf(key, 0);
	if (index != -1) {
		int start = index + key.length();
		start = props.indexOf('=', start);
		if (start != -1) {
			start = start + 1;
			int end = props.indexOf('\n', start);
			String value = props.substring(start,
					end == -1 ? props.length() : end);
			value.trim();
			return value;
		}
	}
	return "";
}

bool setupWiFi() {
	Serial.println("setting up wifi");
	configureWiFi();
	if (LittleFS.exists("/firmware.properties")) {
		Serial.println("firmware.properties exist");
		File f = LittleFS.open("/firmware.properties", "r");
		if (f && f.size()) {
			String props = "";
			while (f.available()) {
				props += char(f.read());
			}
			f.close();

			String ssid = readProperty(props, "wifi.ssid");
			String password = readProperty(props, "wifi.password");
			mqttServer = readProperty(props, "mqtt.server");
			mqttId = readProperty(props, "mqtt.id");
			mqttUser = readProperty(props, "mqtt.user");
			mqttPass = readProperty(props, "mqtt.pass");
			mqttTopic = readProperty(props, "mqtt.topic");

			WiFi.softAPdisconnect(true);
			WiFi.begin(ssid.c_str(), password.c_str());

			while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
				digitalWrite(WIFI_LED, !digitalRead(WIFI_LED));
				delay(40);
				Serial.print(".");
			}
			digitalWrite(WIFI_LED, HIGH);
			Serial.println("wifi connected");
			Serial.print("IP:");
			Serial.println(WiFi.localIP());
			client.setServer(mqttServer.c_str(), 1883);
			client.connect(mqttId.c_str(), mqttUser.c_str(), mqttPass.c_str());
			return true;
		}
	}
	return false;
}

void onWiFiEvent(WiFiEventSoftAPModeStationDisconnected event) {
	Serial.printf("WiFiEventSoftAPModeStationDisconnected, mac: %d; aid:%d", (int) event.mac, (int) event.aid);
	if (isr_loops > 100) {
		setupWiFi();
		setupWebServer();
	}
}

void configureWiFi() {
	WiFi.persistent(false);
	WiFi.disconnect(true);
	WiFi.mode(WIFI_OFF);
	WiFi.mode(WIFI_STA);
	WiFi.onSoftAPModeStationDisconnected(onWiFiEvent);
}

void setupWebServer() {

	server.on("/", HTTP_GET,
			[](AsyncWebServerRequest *request) {
				AsyncWebServerResponse *response = request->beginResponse(
						LittleFS, "/index.htm", "text/html");
				response->addHeader("Cache-Control", "max-age=18144000");
				request->send(response);
			});
	server.on("/index.js", HTTP_GET,
			[](AsyncWebServerRequest *request) {
				AsyncWebServerResponse *response = request->beginResponse(
						LittleFS, "/index.js", "application/javascript");
				response->addHeader("Cache-Control", "max-age=18144000");
				request->send(response);
			});
	server.on("/jquery.min.js", HTTP_GET,
			[](AsyncWebServerRequest *request) {
				AsyncWebServerResponse *response = request->beginResponse(
						LittleFS, "/jquery.min.js", "application/javascript");
				response->addHeader("Cache-Control", "max-age=18144000");
				request->send(response);
			});
	server.on("/svg.min.js", HTTP_GET,
			[](AsyncWebServerRequest *request) {
				AsyncWebServerResponse *response = request->beginResponse(
						LittleFS, "/svg.min.js", "application/javascript");
				response->addHeader("Cache-Control", "max-age=18144000");
				request->send(response);
			});
	server.on("/materialize.min.css", HTTP_GET,
			[](AsyncWebServerRequest *request) {
				AsyncWebServerResponse *response = request->beginResponse(
						LittleFS, "/materialize.min.css", "text/css");
				response->addHeader("Cache-Control", "max-age=18144000");
				request->send(response);
			});
	server.on("/favicon.ico", HTTP_GET,
			[](AsyncWebServerRequest *request) {
				AsyncWebServerResponse *response = request->beginResponse(
						LittleFS, "/favicon.ico");
				response->addHeader("Cache-Control", "max-age=18144000");
				request->send(response);
			});
	server.on("/r", HTTP_GET, [](AsyncWebServerRequest *request) {
		char out[280];
		sprintf(out,"{\"tiav\":%d,\"iav\":%d,\"toav\":%d,\"oav\":%d,\"ocav\":%u, \"oc\":%lu, \"ic\": %lu,\"pwm\":%d,\"autoMppt\":%d}" , i2cData.tiav[5], i2cData.iav[5], i2cData.toav[5], i2cData.oav[5], i2cData.ocav[5], i2cData.oc[5], i2cData.ic[5], i2cData.pwm, i2cData.mppt ? 1 : 0);
		request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, out);
	});
	server.on("/w", HTTP_POST, [](AsyncWebServerRequest *request) {
		AsyncWebParameter *d = request->getParam("d", true, false);
		AsyncWebParameter *v = request->getParam("v", true, false);
		if (v && d && (SLAVE_I2C_CMD_WRITE_TARGET_INPUT_ADC_VAL == d->value().toInt() || SLAVE_I2C_CMD_WRITE_TARGET_OUTPUT_ADC_VAL == d->value().toInt() || SLAVE_I2C_CMD_WRITE_NO_MPPT == d->value().toInt())) {
			int val = v->value().toInt();
			Wire.beginTransmission(SLAVE_I2C_ADDR);
			Wire.write(d->value().toInt());
			Wire.write(val & 0xff);
			Wire.write((val & 0xff00) >> 8);
			Wire.endTransmission();
		}
		request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, "0");
	});

	server.onNotFound(notFound);

	server.begin();
}

void notFound(AsyncWebServerRequest *request) {
	request->send(LittleFS, "/index.htm");
}

void setupOTA() {

	ArduinoOTA.setPassword("Sup3rSecr3t");
	ArduinoOTA.onStart([]() {
	});
	ArduinoOTA.onEnd([]() {
		ESP.reset();
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		yield();
		if (ota_loops++ % 4 == 0) {
			readI2CSlave();
		}
	});
	ArduinoOTA.onError([](ota_error_t error) {
//		if (error == OTA_AUTH_ERROR)
//			Serial.println("Auth Failed");
//		else if (error == OTA_BEGIN_ERROR)
//			Serial.println("Begin Failed");
//		else if (error == OTA_CONNECT_ERROR)
//			Serial.println("Connect Failed");
//		else if (error == OTA_RECEIVE_ERROR)
//			Serial.println("Receive Failed");
//		else if (error == OTA_END_ERROR)
//			Serial.println("End Failed");
	});
	ArduinoOTA.begin();
}
