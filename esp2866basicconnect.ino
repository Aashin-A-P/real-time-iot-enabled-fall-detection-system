#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP_Mail_Client.h>
#include <DHT.h>
#include <MPU9250_asukiaaa.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// === WiFi Config ===
const char* ssid = "Nothing 2a";
const char* password = "deepi@11";

// === SMTP Config 
const char* SMTP_HOST = "smtp.gmail.com";
const int SMTP_PORT = 465;
const char* AUTHOR_EMAIL = "apaashin@gmail.com";
const char* AUTHOR_PASSWORD = "wlmo gqcg zqme dbah";
const char* RECIPIENT_EMAIL = "rajubye189@gmail.com";

// === ThingSpeak Config ===
const char* THINGSPEAK_HOST = "http://api.thingspeak.com/update";
const char* THINGSPEAK_API_KEY = "8X1UR7C7ICOBV45Z";

// === Sensor Setup ===
MPU9250_asukiaaa mpuSensor;
#define DHTPIN D5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define REPORTING_PERIOD_MS 3000
#define EMAIL_COOLDOWN_MS 60000
uint32_t tsLastReport = 0;
uint32_t tsLastEmail = 0;

String ipAddress = "N/A";
String locationInfo = "N/A";
float latitude = 0.0;
float longitude = 0.0;

// === LED and Buzzer Pins ===
#define LED_PIN D6
#define BUZZER_PIN D0
#define PULSE_PIN D3

// === Thresholds ===
#define TEMP_THRESHOLD 30.0
#define HEART_RATE_THRESHOLD 75
#define GYRO_THRESHOLD 1.0  // degrees/sec

// === WiFi Connection ===
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected!");
  ipAddress = WiFi.localIP().toString();
}

// === Location Fetching ===
void fetchLocation() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, "http://ip-api.com/json");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    locationInfo = doc["city"].as<String>() + ", " + doc["regionName"].as<String>() + ", " + doc["country"].as<String>();
    latitude = doc["lat"].as<float>();
    longitude = doc["lon"].as<float>();
  } else {
    locationInfo = "Location fetch failed, code: " + String(httpCode);
  }
  http.end();
}

// === Email Sending ===
void sendEmail(String report) {
  SMTPSession smtp;
  SMTP_Message message;

  smtp.debug(1);
  smtp.callback([](SMTP_Status status) {
    Serial.println(status.info());
  });

  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;

  message.sender.name = "ESP8266 Health Monitor";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "📧 Sensor Health Report from ESP8266";
  message.addRecipient("User", RECIPIENT_EMAIL);
  message.text.content = report.c_str();
  message.text.charSet = "utf-8";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  if (!smtp.connect(&session)) {
    Serial.println("❌ SMTP connection failed!");
    return;
  }
  if (!MailClient.sendMail(&smtp, &message, true)) {
    Serial.println("❌ Email failed: " + smtp.errorReason());
  } else {
    Serial.println("✅ Email sent!");
  }
  smtp.closeSession();
}

// === ThingSpeak Reporting ===
void sendToThingSpeak(float heartRate, float temperature, float humidity, float gyroX, float gyroY, float gyroZ) {
  WiFiClient client;
  HTTPClient http;
  String url = String(THINGSPEAK_HOST) + "?api_key=" + THINGSPEAK_API_KEY +
               "&field1=" + String(heartRate) +
               "&field2=" + String(temperature) +
               "&field3=" + String(humidity) +
               "&field4=" + String(gyroX) +
               "&field5=" + String(gyroY) +
               "&field6=" + String(gyroZ);

  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.println("ThingSpeak response: " + String(httpCode));
  } else {
    Serial.println("Failed to send data to ThingSpeak");
  }
  http.end();
}

// === Setup ===
void setup() {
  Serial.begin(9600);
  Wire.begin(D2, D1);  // SDA, SCL

  dht.begin();
  mpuSensor.setWire(&Wire);
  mpuSensor.beginAccel();
  mpuSensor.beginGyro();
  mpuSensor.beginMag();

  pinMode(PULSE_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  connectWiFi();
  fetchLocation();

  Serial.println("✅ Setup complete");
}

// === Main Loop ===
void loop() {
  mpuSensor.accelUpdate();
  mpuSensor.gyroUpdate();

  float gyroX = mpuSensor.gyroX();
  float gyroY = mpuSensor.gyroY();
  float gyroZ = mpuSensor.gyroZ();

  float accX = mpuSensor.accelX(), accY = mpuSensor.accelY(), accZ = mpuSensor.accelZ();

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Skip if temperature or humidity failed
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("❌ Failed to read from DHT sensor!");
    return;
  }

  float heartRate = analogRead(PULSE_PIN)/8;  // Simulated heart rate
  if(heartRate<65 || heartRate>75){
    heartRate = 72+random(1,10);
  }
  // Compute gyroscope magnitude
  float gyroMagnitude = sqrt(gyroX * gyroX + gyroY * gyroY + gyroZ * gyroZ);
  bool gyroViolation = gyroMagnitude > GYRO_THRESHOLD;

  bool temperatureViolation = temperature > TEMP_THRESHOLD;
  bool heartRateViolation = heartRate > HEART_RATE_THRESHOLD;

  digitalWrite(LED_PIN, temperatureViolation ? HIGH : LOW);
  digitalWrite(BUZZER_PIN, heartRateViolation ? HIGH : LOW);

  if (gyroViolation) {
    Serial.println("🚨 ALERT: Unusual movement detected (fall suspected)");
  }
  // === Serial Monitoring ===
  Serial.println("===== Live Sensor Data =====");
  Serial.println("🌡 Temperature: " + String(temperature) + " °C");
  Serial.println("💧 Humidity: " + String(humidity) + " %");
  Serial.println("💓 Pulse (simulated HR): " + String(heartRate) + " bpm");
  Serial.println("🎯 Gyroscope Magnitude: " + String(gyroMagnitude));
  Serial.println("🚀 Accelerometer: X=" + String(accX) + ", Y=" + String(accY) + ", Z=" + String(accZ));
  Serial.println("============================\n");

  // === Email Trigger ===
  if (temperatureViolation && heartRateViolation && gyroViolation && millis() - tsLastEmail > EMAIL_COOLDOWN_MS) {
    String report = "📡 ESP8266 Sensor Report\n\n";
    report += "🌐 IP: " + ipAddress + "\n";
    report += "📍 Location: " + locationInfo + "\n";
    report += "🌍 Coordinates: Latitude = " + String(latitude, 6) + ", Longitude = " + String(longitude, 6) + "\n\n";
    report += "🌡 Temperature: " + String(temperature) + " °C\n";
    report += "💧 Humidity: " + String(humidity) + " %\n";
    report += "💓 Simulated Heart Rate: " + String(heartRate) + " bpm\n";
    report += "🎯 Gyroscope Magnitude: " + String(gyroMagnitude) + "\n";
    report += "==============================\n";
    sendEmail(report);
    tsLastEmail = millis();
  }

  // === ThingSpeak Upload ===
  if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
    sendToThingSpeak(heartRate, temperature, humidity, gyroX, gyroY, gyroZ);
    tsLastReport = millis();
  }
}
