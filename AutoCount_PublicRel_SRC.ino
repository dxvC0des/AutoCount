#include <SPI.h>
#include <MFRC522.h>
#include <WiFiNINA.h>

// ===================== NFC SETUP =====================
#define SS_PIN 11
#define RST_PIN 7
MFRC522 mfrc522(SS_PIN, RST_PIN);

struct Card {
  String uid;
  String name;
  String studentID;
  String photoURL;
};

// Add your student cards and photos here
//   {"Card UUID", "Student Name", "ID Number", "Image_Of_File_Location.png"},     
Card knownCards[] = {,
};
int numCards = sizeof(knownCards) / sizeof(knownCards[0]);

Card* getCardByUID(String uid) {
  for (int i = 0; i < numCards; i++) {
    if (knownCards[i].uid == uid) return &knownCards[i];
  }
  return nullptr;
}

String getCardDisplay(Card* card) {
  if (card != nullptr)
    return card->name + " [ID: " + card->studentID + "]";
  return "";
}

// ===================== WIFI / AP CONFIG =====================
char wifiSSID[] = "";     // Replace with your WiFi name
char wifiPass[] = ""; // Replace with your WiFi password  
const char* host = "dxvprojects.org";
const int serverPort = 80;

char apSSID[] = "AutoCount-x91k1g";
char apPass[] = "AutoCountDEMO";

WiFiServer server(80);

// ===================== LOG BUFFER =====================
#define MAX_LOGS 50
String logs[MAX_LOGS];
int logIndex = 0;

void addLog(String entry) {
  for (int i = MAX_LOGS - 1; i > 0; i--) logs[i] = logs[i - 1];
  logs[0] = entry;
  if (logIndex < MAX_LOGS) logIndex++;
}

void clearLogs() {
  for (int i = 0; i < MAX_LOGS; i++) logs[i] = "";
  logIndex = 0;
}

// ===================== LED MANAGEMENT =====================
bool ledActive = false;
unsigned long ledStart = 0;

void setRGB(int r, int g, int b) {
  WiFiDrv::analogWrite(25, r);
  WiFiDrv::analogWrite(26, g);
  WiFiDrv::analogWrite(27, b);
}

void breatheBlue() {
  static int brightness = 0;
  static int fadeAmount = 5;
  brightness += fadeAmount;
  if (brightness <= 0 || brightness >= 255) fadeAmount = -fadeAmount;
  setRGB(0, 0, brightness);
  delay(15);
}

// ===================== SEND TO SERVER =====================
void sendLogToServer(String name, String studentID, String photoURL) {
  bool success = false;

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    if (client.connect(host, serverPort)) {
      String postData = "name=" + name + "&id=" + studentID + "&image=" + photoURL;

      client.print("POST /ac/content/logging/logs.php HTTP/1.1\r\n");
      client.print("Host: "); client.println(host);
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: "); client.println(postData.length());
      client.println("Connection: close");
      client.println();
      client.print(postData);

      unsigned long timeout = millis();
      while (client.connected() && millis() - timeout < 2000) {
        while (client.available()) {
          String line = client.readStringUntil('\n');
          Serial.println(line);
        }
      }
      client.stop();
      success = true;
    }
  }

  if (success) setRGB(0, 255, 0);  // green on success
  else setRGB(255, 0, 0);           // red on fail

  ledStart = millis();
  ledActive = true;
}

// ===================== WIFI MANAGEMENT =====================
bool wifiConnected = false;
bool fallbackAP = false;

void connectWiFi() {
  WiFi.end();
  delay(100);
  Serial.println("[WiFi] Connecting...");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 3) {
    Serial.print("[WiFi] Attempt "); Serial.println(attempts + 1);
    WiFi.begin(wifiSSID, wifiPass);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 5000) {
      breatheBlue();
    }
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("[WiFi] Connected! IP: " + WiFi.localIP().toString());
    setRGB(0, 0, 255);
    fallbackAP = false;
  } else {
    Serial.println("[WiFi] Failed after 3 attempts. Switching to AP mode...");
    fallbackAP = true;
    int apStatus = WiFi.beginAP(apSSID, apPass);
    if (apStatus == WL_AP_LISTENING || apStatus == WL_AP_CONNECTED) {
      Serial.print("[AP] Started. SSID: "); Serial.println(apSSID);
      Serial.print("[AP] IP: "); Serial.println(WiFi.localIP());
      setRGB(255, 165, 0);
    } else {
      Serial.println("[AP] Failed to start!");
      setRGB(255, 0, 0);
    }
  }
}

// ===================== DOUBLE-TAP PREVENTION =====================
String lastUID = "";
unsigned long lastUIDTime = 0;
const unsigned long UID_IGNORE_MS = 1000;

// ===================== SETUP =====================
void setup() {
  Serial.begin(9600);
  while (!Serial);

  SPI.begin();
  mfrc522.PCD_Init();
  setRGB(0, 0, 0);

  Serial.println("[SYS] Booting...");
  server.begin();

  connectWiFi();
}

String urlencode(String str) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  for (int i = 0; i < str.length(); i++){
    c = str.charAt(i);
    if (c == ' ') encoded += '+';
    else if (isalnum(c)) encoded += c;
    else {
      code1=(c & 0xf)+'0';
      if ((c & 0xf) >9) code1=(c & 0xf) - 10 + 'A';
      c = (c>>4)&0xf;
      code0=c+'0';
      if (c>9) code0=c-10+'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}


// ===================== LOOP =====================
void loop() {
  // --- Wi-Fi reconnect ---
  if (!fallbackAP && WiFi.status() != WL_CONNECTED && millis() % 5000 < 50) {
    Serial.println("[WiFi] Disconnected. Reconnecting...");
    connectWiFi();
  }

  // --- LED timeout management ---
  if (ledActive && millis() - ledStart > 1000) {
    if (WiFi.status() == WL_CONNECTED) setRGB(0, 0, 255);
    else setRGB(255, 165, 0);
    ledActive = false;
  }

  // --- NFC scanning ---
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uidStr = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      if (mfrc522.uid.uidByte[i] < 0x10) uidStr += "0";
      uidStr += String(mfrc522.uid.uidByte[i], HEX);
      if (i < mfrc522.uid.size - 1) uidStr += ":";
    }
    uidStr.toUpperCase();

    unsigned long now = millis();
    if (uidStr != lastUID || now - lastUIDTime > UID_IGNORE_MS) {
      lastUID = uidStr;
      lastUIDTime = now;

      Card* card = getCardByUID(uidStr);
      if (card != nullptr) {
        Serial.print("[DEBUG] Student Scanned Card | ");

        // --- Open location_select.php in browser ---
        String url = "https://dxvprojects.org/ac/location_select.php?";
        url += "uid=" + card->uid;
        url += "&name=" + card->name;
        url += "&student_id=" + card->studentID;
        url += "&avatar=" + card->photoURL;


        Serial.println("Automatically Opening:");
        Serial.println(url);

        setRGB(0, 255, 0);  // Green
        ledStart = millis();
        ledActive = true;
      } else {
        Serial.print("[NFC] Unknown card: "); Serial.println(uidStr);
        setRGB(255, 0, 0);  // Red
        ledStart = millis();
        ledActive = true;
      }

      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }

  // --- Web server client ---
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    String request = "";
    while (client.connected() && client.available()) {
      char c = client.read();
      request += c;
      if (c == '\n') {
        if (currentLine.length() == 0) {
          if (request.indexOf("GET /logs") >= 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/plain");
            client.println("Connection: close");
            client.println();
            for (int i = 0; i < logIndex; i++) client.println(logs[i]);
          } else if (request.indexOf("GET /clear") >= 0) {
            clearLogs();
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/plain");
            client.println("Connection: close");
            client.println("CLEARED");
          } else {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: text/plain");
            client.println("Connection: close");
            client.println("AutoCount AP System - NOT RECOMMENDED");
          }
          break;
        } else currentLine = "";
      } else if (c != '\r') currentLine += c;
    }
    delay(1);
    client.stop();
  }
}
