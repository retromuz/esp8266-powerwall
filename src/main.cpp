#include "main.h"

AsyncWebServer server(80); // Create a webserver object that listens for HTTP request on port 80
Ticker ticker;

volatile unsigned int ticks = 0;
volatile unsigned long totalTicks = 0;
volatile unsigned long wifiTick = 0;
volatile unsigned long mpptTick = 0;

volatile int tiav = 0;
volatile int iav = 0;
volatile int toav = 0;
volatile int oav = 0;
volatile int ocav = 0;
volatile int ocav_tmp = 0;
volatile int pwm = -1;
volatile bool autoMppt = true;

MPPTData mpptData;

HTTPClient http;
WiFiClient wifi;

void ISRwatchdog() {
	totalTicks++;
	if (++ticks > 30) {
		Serial.println("watchdog reset");
		ticks = 0;
		ESP.reset();
	}
	readI2CSlave();
}

void setup(void) {
	delay(100);
	Serial.begin(115200);
	Serial.println("Starting");
	ticker.attach_ms(333, ISRwatchdog);
	setupPins();
	if (!LittleFS.begin()) {
		Serial.println("Error setting up LittleFS!");
		ticks = 0;
		ESP.reset();
	}
	if (setupWiFi()) {
	}
	digitalWrite(WIFI_LED, HIGH);
	setupWebServer();
	setupOTA();
	Serial.print("Connected to ");
	Serial.println(WiFi.SSID());  // Tell us what network we're connected to
	Serial.print("IP address:\t");
	Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer
	Wire.setClock(100000);
	Wire.begin();
	mpptData.status = 0;
}

void loop(void) {
	ticks = 0;
	ArduinoOTA.handle();
	if (totalTicks - wifiTick > 20) {
		Serial.println("Checking WiFi connectivity.");
		Serial.println(WiFi.status());
		wifiTick = totalTicks;
		if (!WiFi.isConnected()) {
			ticks = 0;
			ESP.reset();
		}
	}

	mppt();
}

void mppt() {
	if (autoMppt && totalTicks > mpptTick && totalTicks - mpptTick > 2) {
		mpptTick = totalTicks;
		if (mpptData.status == 3) {
			analyseMpptData();
			mpptData.status = 0;
		} else {
			buildMpptData();
		}
	}
}

void buildMpptData() {
	mpptData.current = ocav;
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_TARGET_INPUT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		uint8_t buf[2];
		Wire.readBytes(buf, 2);
		int adc = buf[0] | buf[1] << 8;
		MPPTEntry e;
		e.current = ocav;
		e.inAdc = adc;
		mpptData.data[mpptData.status++] = e;
		if (mpptData.status == 1) {
			adc = adc + MPPT_STEP;
			writeAdc(adc);
		} else if (mpptData.status == 2) {
			adc = adc - (MPPT_STEP * 2);
			writeAdc(adc);
		}
	}

}

void analyseMpptData() {
	ocav_tmp = 0;
	if (mpptData.data[0].current >= mpptData.data[1].current) {
		if (mpptData.data[0].current >= mpptData.data[2].current) {
			writeAdc(mpptData.data[0].inAdc);
			ocav_tmp = ocav;
			Serial.print(
					"current MPPT conditions are the best. revert to inADC: ");
			Serial.println(mpptData.data[0].inAdc);
		} else {
			writeAdc(mpptData.data[2].inAdc);
			Serial.print("found better MPPT conditions at inADC: ");
			Serial.println(mpptData.data[2].inAdc);
		}
	} else {
		if (mpptData.data[1].current >= mpptData.data[2].current) {
			writeAdc(mpptData.data[1].inAdc);
			Serial.print("found better MPPT conditions at inADC: ");
			Serial.println(mpptData.data[1].inAdc);
		} else {
			writeAdc(mpptData.data[2].inAdc);
			Serial.print("found better MPPT conditions at inADC: ");
			Serial.println(mpptData.data[2].inAdc);
		}
	}
}

void readI2CSlave() {
	uint8_t buf[2];
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_TARGET_INPUT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		Wire.readBytes(buf, 2);
		tiav = buf[0] | buf[1] << 8;
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_TARGET_OUTPUT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		Wire.readBytes(buf, 2);
		toav = buf[0] | buf[1] << 8;
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_INPUT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		Wire.readBytes(buf, 2);
		iav = buf[0] | buf[1] << 8;
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_OUTPUT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		Wire.readBytes(buf, 2);
		oav = buf[0] | buf[1] << 8;
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_PWM_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 1)) {
		pwm = Wire.read();
	}
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_READ_OUTPUT_CURRENT_ADC_VAL);
	Wire.endTransmission();
	if (Wire.requestFrom(SLAVE_I2C_ADDR, 2)) {
		Wire.readBytes(buf, 2);
		ocav = buf[0] | buf[1] << 8;
	}
}

void writeAdc(int adc) {
	Wire.beginTransmission(SLAVE_I2C_ADDR);
	Wire.write(SLAVE_I2C_CMD_WRITE_TARGET_INPUT_ADC_VAL);
	Wire.write(adc & 0xff);
	Wire.write((adc & 0xff00) >> 8);
	Wire.endTransmission();
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
			Serial.printf("Wi-Fi mode set to WIFI_STA %s\r\n",
					WiFi.mode(WIFI_STA) ? "" : "Failed!");

			WiFi.softAPdisconnect(true);
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
	server.on("/r", HTTP_GET, [](AsyncWebServerRequest *request) {
		char out[280];
		sprintf(out,"{\"tiav\":%d,\"iav\":%d,\"toav\":%d,\"oav\":%d,\"ocav\":%d,\"pwm\":%d,\"autoMppt\":%d, \"current\":%d,\"mpptData\":{\"status\":%d,\"data\":[{\"inAdc\":%d,\"current\":%d},{\"inAdc\":%d,\"current\":%d},{\"inAdc\":%d,\"current\":%d}]}}" , tiav, iav, toav, oav, ocav, pwm, autoMppt? 1 : 0, mpptData.current, mpptData.status, mpptData.data[0].inAdc, mpptData.data[0].current, mpptData.data[1].inAdc, mpptData.data[1].current, mpptData.data[2].inAdc, mpptData.data[2].current);
		request->send_P(200, CONTENT_TYPE_APPLICATION_JSON, out);
	});
	server.on("/w", HTTP_POST, [](AsyncWebServerRequest *request) {
		try {
			AsyncWebParameter *d = request->getParam("d", true, false);
			AsyncWebParameter *v = request->getParam("v", true, false);
			if (d && v && d->value().toInt() == CMD_WRITE_AUTO_MPPT) {
				autoMppt = v->value().toInt();
				if (autoMppt) {
					mpptTick = 0;
				}
			} else if (v && d) {
				int val = v->value().toInt();
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

//	server.onNotFound(notFound);

	server.begin();
	Serial.println("HTTP server started");
}

//void notFound(AsyncWebServerRequest *request) {
//	request->send(LittleFS, "/index.htm");
//}

void setupOTA() {

	Serial.println("Setting up OTA");
	ArduinoOTA.setPassword("Sup3rSecr3t");
	ArduinoOTA.onStart([]() {
		Serial.println("Start");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
		ticks = 0;
		ESP.reset();
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
