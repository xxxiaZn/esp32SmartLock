#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <Arduino.h>

// WIFI配置
const char* ssid = "";
const char* password = "";

// 静态IP配置
IPAddress local_IP(192, 168, 1, 110);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// PWM配置
const int freq = 50;
const int channel = 8;
const int resolution = 8;
const int servoPin = 4;

// RFID配置, 已授权的卡片
#define RST_PIN 22
#define SS_PIN 21
MFRC522 mfrc522(SS_PIN, RST_PIN);
byte authorized_uids[][4] = {
  {0xDE, 0xD8, 0x5F, 0x7D},
  {0x1A, 0x5C, 0xB0, 0x02},
  {0x1E, 0xAA, 0x59, 0x7D},
  {0xB9, 0x78, 0x7D, 0xDC},
  {0xEE, 0x75, 0x56, 0x7D}
};
const int num_uids = sizeof(authorized_uids) / sizeof(authorized_uids[0]);

// Web服务器配置
WiFiServer server(80);
String header;
const long timeoutTime = 2000;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  ledcSetup(channel, freq, resolution);
  ledcAttachPin(servoPin, channel);

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Static IP Failed to configure");
  }

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  mfrc522.PCD_Init();
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop() {
  handleRFID();
  handleClient();
}

void handleRFID() {
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    Serial.print(F("Card UID: "));
    dump_byte_array(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
    Serial.print(F("PICC type: "));
    Serial.println(mfrc522.PICC_GetTypeName(mfrc522.PICC_GetType(mfrc522.uid.sak)));

    if (checkUID(mfrc522.uid.uidByte)) {
      Serial.println(F("Access Granted!"));
      operateServo(180, 40);
    } else {
      Serial.println(F("Access Denied!"));
    }

    mfrc522.PICC_HaltA();
  }
}

void handleClient() {
  WiFiClient client = server.available();
  if (client) {
    unsigned long previousTime = millis();
    String currentLine = "";
    while (client.connected() && millis() - previousTime <= timeoutTime) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        header += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            if (header.indexOf("GET /door/open") >= 0) {
              Serial.println("DOOR open");
              operateServo(180, 40);
            }
            sendHTML(client);
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
    Serial.println("Client disconnected.\n");
  }
}

void operateServo(int openPos, int closePos) {
  for (int pos = openPos; pos >= closePos; pos -= 1) {
    ledcWrite(channel, calculatePWM(pos));
    delay(10);
  }
  delay(1000);
  for (int pos = closePos; pos <= openPos; pos += 1) {
    ledcWrite(channel, calculatePWM(pos));
    delay(10);
  }
}

void sendHTML(WiFiClient& client) {
  client.println("<!DOCTYPE html><html>");
  client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  client.println("<link rel=\"icon\" href=\"data:,\">");
  client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
  client.println(".center { display: flex; flex-direction: column; align-items: center; justify-content: center; height: 70vh; }");
  client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
  client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
  client.println(".button2 {background-color: #555555;}</style></head>");
  client.println("<body><h1>306 Access Control</h1>");
  client.println("<div class=\"center\">");
  client.println("<p><a href=\"/door/open\"><button class=\"button\">OPEN</button></a></p>");
  client.println("</div>");
  client.println("</body></html>");
  client.println();
}

int calculatePWM(int degree) {
  const float deadZone = 6.4;
  const float max = 32;
  degree = constrain(degree, 0, 180);
  return (int)(((max - deadZone) / 180) * degree + deadZone);
}

void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

bool checkUID(byte *uid) {
  for (int i = 0; i < num_uids; i++) {
    if (memcmp(uid, authorized_uids[i], 4) == 0) {
      return true;
    }
  }
  return false;
}