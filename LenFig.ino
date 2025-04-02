// libraries to handle the ESP32 WiFi and web server
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

//libraries to handle the ESP32 filesystem
#include <SPIFFS.h>
#include <Preferences.h>

// library to decode and play animated GIF files without manually converting to bitmaps
#include <AnimatedGIF.h>

// libraries to display to the ST7789 1.69" TFT used
#include <SPI.h>
#include <TFT_eSPI.h>

// including the webpage for the hosted web server
#include "webpages.h"

#define FIRMWARE_VERSION "v0.0.1"
#define AP_TRIGGER_PIN 15  // used to trigger Access Point mode

AnimatedGIF gif;
File gifFile;
const char *filename = "/img.gif";

TFT_eSPI tft = TFT_eSPI();

const String default_ssid = "HomeWIFI";
const String default_wifipassword = "pass123";
const String default_httpuser = "admin";
const String default_httppassword = "admin";
const int default_webserverporthttp = 80;

// configuration structure
struct Config {
  String ssid;               // wifi ssid
  String wifipassword;       // wifi password
  String httpuser;           // username to access web admin
  String httppassword;       // password to access web admin
  int webserverporthttp;     // http port number for web admin
};

// variables
Config config;                        // configuration
volatile bool shouldReboot = false;   // schedule a reboot
AsyncWebServer *server;               // initialise webserver
bool wifiConnected = false;           // track WiFi status
bool apModeActive = false;            // track AP mode status

// function defaults
String listFiles(bool ishtml = false);
void startAPMode();

void setup() {
  Serial.begin(115200);
  Serial.print("Firmware: "); Serial.println(FIRMWARE_VERSION);
  Serial.println("Booting ...");

  // configure trigger pin (with internal pullup)
  pinMode(AP_TRIGGER_PIN, INPUT_PULLUP);
  delay(100); // let pin stabilize

  // check for immediate AP trigger (pin shorted to ground at boot)
  if (digitalRead(AP_TRIGGER_PIN) == LOW) {
    Serial.println("AP mode triggered at boot");
    startAPMode();
  }

  // initialize display with flipped orientation (rotation 3 = 180 degrees)
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM); // centered text

  // initialize Preferences to save WiFi config
  Preferences preferences;
  preferences.begin("wifi-config", true);
  String savedSSID = preferences.getString("ssid", "");
  String savedPassword = preferences.getString("password", "");
  preferences.end();

  // check if WiFi credentials are saved, otherwise start AP mode
  if (savedSSID == "") {
    startAPMode();
  }

  // mount SPIFFS (try to continue even if it fails)
  Serial.println("Mounting SPIFFS ...");
  if (!SPIFFS.begin(true)) {
    Serial.println("ERROR: Cannot mount SPIFFS, continuing anyway");
    tft.drawString("SPIFFS Error", tft.width()/2, tft.height()/2);
    delay(2000);
  }

  gif.begin(BIG_ENDIAN_PIXELS);

  // show the available space on the esp32 filesystem
  Serial.print("SPIFFS Free: "); Serial.println(humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes())));
  Serial.print("SPIFFS Used: "); Serial.println(humanReadableSize(SPIFFS.usedBytes()));
  Serial.print("SPIFFS Total: "); Serial.println(humanReadableSize(SPIFFS.totalBytes()));

  Serial.println(listFiles());

  // Load configuration
  config.ssid = savedSSID;
  config.wifipassword = savedPassword;
  config.httpuser = default_httpuser;
  config.httppassword = default_httppassword;
  config.webserverporthttp = default_webserverporthttp;

  // try to connect to WiFi but don't block if it fails
  Serial.println("Connecting to WiFi...");
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Connecting to:", tft.width()/2, tft.height()/2 - 20);
  tft.drawString(config.ssid, tft.width()/2, tft.height()/2);
  
  WiFi.begin(config.ssid.c_str(), config.wifipassword.c_str());
  
  unsigned long wifiTimeout = millis() + 10000; // timeout after 10 seconds
  while (WiFi.status() != WL_CONNECTED && millis() < wifiTimeout) {
    delay(500);
    Serial.print(".");
    tft.drawString(".", tft.width()/2 + 100 + (millis() % 1000)/200 * 10, tft.height()/2 + 20);
    
    // check for ap mode trigger during connection attempt
    if (digitalRead(AP_TRIGGER_PIN) == LOW) {
      delay(50); // Debounce
      if (digitalRead(AP_TRIGGER_PIN) == LOW) {
        startAPMode();
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nWiFi connected!");
    Serial.println(WiFi.localIP());
    
    // show ip then clear
    tft.fillScreen(TFT_BLACK);
    tft.drawString("Connected!", tft.width()/2, tft.height()/2 - 20);
    tft.drawString(WiFi.localIP().toString(), tft.width()/2, tft.height()/2);
    delay(3000);
    tft.fillScreen(TFT_BLACK);

    // configure new webserver
    Serial.println("Configuring Webserver ...");
    server = new AsyncWebServer(config.webserverporthttp);
    configureWebServer();
    Serial.println("Starting Webserver ...");
    server->begin();
  } else {
    Serial.println("\nWiFi connection failed - continuing offline");
    tft.fillScreen(TFT_BLACK);
    tft.drawString("WiFi Failed", tft.width()/2, tft.height()/2 - 10);
    tft.drawString("Offline Mode", tft.width()/2, tft.height()/2 + 10);
    delay(2000);
    tft.fillScreen(TFT_BLACK);
  }
}

// start the Access Point mode while stopping any other function
void startAPMode() {
  gif.close();
  apModeActive = true;
  
  tft.fillScreen(TFT_BLACK);
  tft.drawString("Config Mode:", tft.width()/2, tft.height()/2 - 40);
  tft.drawString("Connect to:", tft.width()/2, tft.height()/2 - 20);
  tft.drawString("Len's Window AP", tft.width()/2, tft.height()/2);
  tft.drawString("PW: configureme", tft.width()/2, tft.height()/2 + 20);
  tft.drawString("192.168.4.1", tft.width()/2, tft.height()/2 + 40);

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Len's Window AP", "configureme");

  // create configuration server
  if (server != nullptr) {
    server->end();
    delete server;
  }
  server = new AsyncWebServer(80);
  
  // simple webpage to configure WiFi
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", 
      "<html><body><h1>WiFi Setup</h1>"
      "<form action='/save' method='post'>"
      "SSID: <input name='ssid'><br>"
      "Password: <input name='pass' type='password'><br>"
      "<input type='submit' value='Save'></form></body></html>");
  });

  server->on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("ssid", true) && request->hasParam("pass", true)) {
      Preferences prefs;
      prefs.begin("wifi-config", false);
      prefs.putString("ssid", request->getParam("ssid", true)->value());
      prefs.putString("password", request->getParam("pass", true)->value());
      prefs.end();
      request->send(200, "text/plain", "WiFi configured. Device will restart.");
      delay(1000);
      ESP.restart();
    }
  });

  server->begin();

  // Wait forever in AP mode
  while(true) {
    delay(1000);
  }
}

void loop() {
  // check for pin short (trigger AP mode)
  if (digitalRead(AP_TRIGGER_PIN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(AP_TRIGGER_PIN) == LOW) {
      Serial.println("AP mode triggered by pin");
      startAPMode();
    }
  }

  // normal GIF playback if not in AP mode
  if (!apModeActive) {
    if (gif.open(filename, fileOpen, fileClose, fileRead, fileSeek, GIFDraw)) {
      tft.startWrite();
      while (gif.playFrame(true, NULL)) {
        yield();
        // Continually check for pin short during playback
        if (digitalRead(AP_TRIGGER_PIN) == LOW) {
          delay(50);
          if (digitalRead(AP_TRIGGER_PIN) == LOW) {
            gif.close();
            tft.endWrite();
            startAPMode();
          }
        }
        if (shouldReboot) break;
      }
      gif.close();
      tft.endWrite();
    } else {
      static unsigned long lastErrorTime = 0;
      if (millis() - lastErrorTime > 5000) {
        tft.fillScreen(TFT_BLACK);
        tft.drawString("No GIF Found", tft.width()/2, tft.height()/2 - 10);
        if (wifiConnected) {
          tft.drawString("Upload via Web", tft.width()/2, tft.height()/2 + 10);
        } else {
          tft.drawString("Connect to WiFi", tft.width()/2, tft.height()/2 + 10);
        }
        lastErrorTime = millis();
      }
    }
  }
  
  if (shouldReboot) {
    delay(1000);
    rebootESP("Rebooting...");
  }
}

// callbacks for file operations for the Animated GIF Library
void *fileOpen(const char *filename, int32_t *pFileSize) {
  gifFile = SPIFFS.open(filename, FILE_READ);
  *pFileSize = gifFile.size();
  if (!gifFile) {
    Serial.println("Failed to open GIF file from SPIFFS!");
  }
  return &gifFile;
}

void fileClose(void *pHandle) {
  gifFile.close();
}

int32_t fileRead(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  int32_t iBytesRead = min(iLen, pFile->iSize - pFile->iPos);
  if (iBytesRead <= 0) return 0;
  
  gifFile.seek(pFile->iPos);
  iBytesRead = gifFile.read(pBuf, iBytesRead);
  pFile->iPos += iBytesRead;
  return iBytesRead;
}

int32_t fileSeek(GIFFILE *pFile, int32_t iPosition) {
  iPosition = constrain(iPosition, 0, pFile->iSize - 1);
  pFile->iPos = iPosition;
  gifFile.seek(pFile->iPos);
  return iPosition;
}

void rebootESP(String message) {
  Serial.print("Rebooting ESP32: "); Serial.println(message);
  ESP.restart();
}

String listFiles(bool ishtml) {
  String returnText = "";
  Serial.println("Listing files stored on SPIFFS");
  File root = SPIFFS.open("/");
  File foundfile = root.openNextFile();
  if (ishtml) {
    returnText += "<table><tr><th align='left'>Name</th><th align='left'>Size</th><th></th><th></th></tr>";
  }
  while (foundfile) {
    if (ishtml) {
      returnText += "<tr align='left'><td>" + String(foundfile.name()) + "</td><td>" + humanReadableSize(foundfile.size()) + "</td>";
      returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'download\')\">Download</button>";
      returnText += "<td><button onclick=\"downloadDeleteButton(\'" + String(foundfile.name()) + "\', \'delete\')\">Delete</button></tr>";
    } else {
      returnText += "File: " + String(foundfile.name()) + " Size: " + humanReadableSize(foundfile.size()) + "\n";
    }
    foundfile = root.openNextFile();
  }
  if (ishtml) {
    returnText += "</table>";
  }
  root.close();
  foundfile.close();
  return returnText;
}

String humanReadableSize(const size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0) + " MB";
  else return String(bytes / 1024.0 / 1024.0 / 1024.0) + " GB";
}