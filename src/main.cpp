#include "main.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
AsyncWebServer server(80); // Create a webserver object that listens for HTTP request on port 80
Ticker ticker;

volatile unsigned int ticks = 0;
volatile unsigned long int epochWiFi = 0;

volatile bool mppt = false;
volatile unsigned long int epochMppt = 0;

MPPTData mpptData;

void ISRwatchdog() {
	if (++ticks > 30) {
		Serial.println("watchdog reset");
		ESP.reset();
	}
}

void setup(void) {
	Serial.begin(115200);
	delay(100);
	Serial.println("Starting");
	ticker.attach(1, ISRwatchdog);
	setupPins();
	if (!LittleFS.begin()) {
		Serial.println("Error setting up LittleFS!");
		ESP.reset();
	}
	if (setupWiFi()) {
		digitalWrite(WIFI_LED, HIGH);
		setupWebServer();
		setupNTPClient();
		setupOTA();
		Serial.print("Connected to ");
		Serial.println(WiFi.SSID());  // Tell us what network we're connected to
		Serial.print("IP address:\t");
		Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer
	}
	Wire.setClock(100000);
	Wire.begin();
}

void loop(void) {
	ticks = 0;
	ArduinoOTA.handle();
	if (timeClient.getEpochTime() - epochWiFi > 20) {
		Serial.println("Checking WiFi connectivity.");
		epochWiFi = timeClient.getEpochTime();
		if (!WiFi.isConnected()) {
			Serial.println("WiFi reconnecting...");
			WiFi.disconnect(true);
			delay(500);
			setupWiFi();
		}
	}
	if (mppt && timeClient.getEpochTime() - epochMppt > 2) {
		HTTPClient http;
		WiFiClient wifi;
		if (http.begin(wifi, "http://bms.karunadheera.com/c")) {
			int code = http.GET();
			if (code == 200) {

			} else {
				Serial.print("error http code: ");
				Serial.println(code);
			}
		}
	}
}

void initMpptData() {
	mpptData.status = 0;
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

	if (LittleFS.exists("/firmware.properties")) {
		File f = LittleFS.open("/firmware.properties", "r");
		if (f && f.size()) {
			Serial.println("Reading firmware.properties");
			String props = "";
			while (f.available()) {
				props += char(f.read());
			}
			f.close();

			String ssid = readProperty(props, "wifi.ssid");
			String password = readProperty(props, "wifi.password");
			Serial.printf(
					"firmware properties wifi.ssid: %s; wifi.password: %s\r\n",
					ssid.c_str(), password.c_str());
			WiFi.begin(ssid, password);

			Serial.printf("Connecting");
			while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
				ticks = 0;
				Serial.print('.');
				digitalWrite(WIFI_LED, !digitalRead(WIFI_LED));
				delay(40);
			}
			Serial.println();
			return true;
		}
	} else {
		Serial.println("Cannot find /firmware.properties file");
	}
	return false;
}

void setupWebServer() {

	server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(LittleFS, "/index.htm");
	});
	server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(LittleFS, "/index.js");
	});
	server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(LittleFS, "/favicon.ico");
	});
	server.on("/rit", HTTP_GET, [](AsyncWebServerRequest *request) {
		char out[4];
		Wire.beginTransmission(SLAVE_I2C_ADDR);
		Wire.write(SLAVE_I2C_CMD_READ_TARGET_INPUT_ADC_VAL);
		Wire.endTransmission();
		if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
			uint8_t buf[2];
			Wire.readBytes(buf, 2);
			sprintf(out, "%d", buf[0] | buf[1] << 8);
		}
		request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, out);
	});
	server.on("/rot", HTTP_GET, [](AsyncWebServerRequest *request) {
		char out[4];
		Wire.beginTransmission(SLAVE_I2C_ADDR);
		Wire.write(SLAVE_I2C_CMD_READ_TARGET_OUTPUT_ADC_VAL);
		Wire.endTransmission();
		if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
			uint8_t buf[2];
			Wire.readBytes(buf, 2);
			sprintf(out, "%d", buf[0] | buf[1] << 8);
		}
		request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, out);
	}
	);
	server.on("/ri", HTTP_GET, [](AsyncWebServerRequest *request) {
		char out[4];
		Wire.beginTransmission(SLAVE_I2C_ADDR);
		Wire.write(SLAVE_I2C_CMD_READ_INPUT_ADC_VAL);
		Wire.endTransmission();
		if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
			uint8_t buf[2];
			Wire.readBytes(buf, 2);
			sprintf(out, "%d", buf[0] | buf[1] << 8);
		}
		request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, out);
	});
	server.on("/ro", HTTP_GET, [](AsyncWebServerRequest *request) {
		char out[4];
		Wire.beginTransmission(SLAVE_I2C_ADDR);
		Wire.write(SLAVE_I2C_CMD_READ_OUTPUT_ADC_VAL);
		Wire.endTransmission();
		if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
			uint8_t buf[2];
			Wire.readBytes(buf, 2);
			sprintf(out, "%d", buf[0] | buf[1] << 8);
		}
		request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, out);
	});
	server.on("/rp", HTTP_GET, [](AsyncWebServerRequest *request) {
		char out[2];
		Wire.beginTransmission(SLAVE_I2C_ADDR);
		Wire.write(SLAVE_I2C_CMD_READ_PWM_VAL);
		Wire.endTransmission();
		if (Wire.requestFrom(SLAVE_I2C_ADDR, 1)) {
			sprintf(out, "%d", Wire.read());
		}
		request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, out);
	});
	server.on("/ra", HTTP_GET, [](AsyncWebServerRequest *request) {
		char out[2];
		sprintf(out, "%d", mppt ? 1 : 0);
		request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, out);
	});
	server.on("/w", HTTP_POST, [](AsyncWebServerRequest *request) {
		try {
			AsyncWebParameter *d = request->getParam("d", true, false);
			AsyncWebParameter *v = request->getParam("v", true, false);
			if (d && v && d->value().toInt() == CMD_WRITE_AUTO_MPPT) {
				mppt = v->value().toInt() != 0;
			} else if (v && d) {
				int val = v->value().toInt();
				Serial.println(val);
				Wire.beginTransmission(SLAVE_I2C_ADDR);
				Wire.write(d->value().toInt());
				Wire.write(val & 0xff);
				Wire.write((val & 0xff00) >> 8);
				Wire.endTransmission();
			} else {
				Serial.println("parameter write fail");
			}
			request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, "0");
		} catch (...) {
			Serial.println("exception");
			request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, "-1");
		}
	});

	server.onNotFound(notFound);

	server.begin();
	Serial.println("HTTP server started");
}

void notFound(AsyncWebServerRequest *request) {
	request->send(404, "text/plain", "Not found");
}

void setupNTPClient() {
	Serial.println("Synchronizing time with NTP");
	timeClient.begin();
	timeClient.setTimeOffset(39600);
	while (!timeClient.update()) {
		timeClient.forceUpdate();
	}
	Serial.println(timeClient.getFormattedDate());
}

void setupOTA() {

	Serial.println("Setting up OTA");
	ArduinoOTA.setPassword("Sup3rSecr3t");
	ArduinoOTA.onStart([]() {
		Serial.println("Start");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR)
			Serial.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR)
			Serial.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR)
			Serial.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR)
			Serial.println("Receive Failed");
		else if (error == OTA_END_ERROR)
			Serial.println("End Failed");
	});
	ArduinoOTA.begin();
}
