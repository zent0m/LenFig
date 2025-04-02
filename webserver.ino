// parses and processes webpages
// if the webpage has %SOMETHING% or %SOMETHINGELSE% it will replace those strings with the ones defined
String processor(const String& var) {
  if (var == "FIRMWARE") {
    return FIRMWARE_VERSION;
  }

  if (var == "FREESPIFFS") {
    return humanReadableSize((SPIFFS.totalBytes() - SPIFFS.usedBytes()));
  }

  if (var == "USEDSPIFFS") {
    return humanReadableSize(SPIFFS.usedBytes());
  }

  if (var == "TOTALSPIFFS") {
    return humanReadableSize(SPIFFS.totalBytes());
  }
  return String();
}

void configureWebServer() {
  // Add WiFi reset endpoint
  server->on("/resetwifi", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (checkUserWebAuth(request)) {
      Preferences preferences;
      preferences.begin("wifi-config", false);
      preferences.remove("ssid");
      preferences.remove("password");
      preferences.end();
      request->send(200, "text/plain", "WiFi configuration reset. Restarting...");
      delay(1000);
      ESP.restart();
    } else {
      return request->requestAuthentication();
    }
  });

  // if url isn't found
  server->onNotFound(notFound);

  // run handleUpload function when any file is uploaded
  server->onFileUpload(handleUpload);

  // visiting this page will cause you to be logged out
  server->on("/logout", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->requestAuthentication();
    request->send(401);
  });

  // presents a "you are now logged out webpage
  server->on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    Serial.println(logmessage);
    request->send_P(401, "text/html", logout_html, processor);
  });

  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + + " " + request->url();

    if (checkUserWebAuth(request)) {
      logmessage += " Auth: Success";
      Serial.println(logmessage);
      request->send_P(200, "text/html", index_html, processor);
    } else {
      logmessage += " Auth: Failed";
      Serial.println(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

    if (checkUserWebAuth(request)) {
      request->send(200, "text/html", reboot_html);
      logmessage += " Auth: Success";
      Serial.println(logmessage);
      shouldReboot = true;
    } else {
      logmessage += " Auth: Failed";
      Serial.println(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/listfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    if (checkUserWebAuth(request)) {
      logmessage += " Auth: Success";
      Serial.println(logmessage);
      request->send(200, "text/plain", listFiles(true));
    } else {
      logmessage += " Auth: Failed";
      Serial.println(logmessage);
      return request->requestAuthentication();
    }
  });

  server->on("/file", HTTP_GET, [](AsyncWebServerRequest *request) {
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    
    if (checkUserWebAuth(request)) {
        logmessage += " Auth: Success";
        Serial.println(logmessage);

        if (request->hasParam("name") && request->hasParam("action")) {
            String fileName = request->getParam("name")->value();
            String fileAction = request->getParam("action")->value();
            
            if (!fileName.startsWith("/")) fileName = "/" + fileName;

            logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url() + "?name=" + fileName + "&action=" + fileAction;

            if (!SPIFFS.exists(fileName)) {
                Serial.println(logmessage + " ERROR: file does not exist");
                request->send(400, "text/plain", "ERROR: file does not exist");
            } else {
                Serial.println(logmessage + " file exists");
                if (fileAction == "download") {
                    logmessage += " downloaded";
                    request->send(SPIFFS, fileName, "application/octet-stream");
                } else if (fileAction == "delete") {
                    logmessage += " deleted";
                    SPIFFS.remove(fileName);
                    request->send(200, "text/plain", "Deleted File: " + fileName);
                } else {
                    logmessage += " ERROR: invalid action param supplied";
                    request->send(400, "text/plain", "ERROR: invalid action param supplied");
                }
                Serial.println(logmessage);
            }
        } else {
            request->send(400, "text/plain", "ERROR: name and action params required");
        }
    } else {
        logmessage += " Auth: Failed";
        Serial.println(logmessage);
        return request->requestAuthentication();
    }
  });
}

void notFound(AsyncWebServerRequest *request) {
  String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
  Serial.println(logmessage);
  request->send(404, "text/plain", "Not found");
}

// used by server.on functions to discern whether a user has the correct httpapitoken OR is authenticated by username and password
bool checkUserWebAuth(AsyncWebServerRequest * request) {
  bool isAuthenticated = false;

  if (request->authenticate(config.httpuser.c_str(), config.httppassword.c_str())) {
    //Serial.println("Authenticated via username and password");
    isAuthenticated = true;
  }
  return isAuthenticated;
}

// handles uploads to the filserver
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  static size_t totalSize = 0;
  static size_t lastProgress = 0;
  static size_t fileSize = 0;
  const size_t MAX_FILE_SIZE = 1048576; // 1MB

  if (!checkUserWebAuth(request)) {
    request->requestAuthentication();
    return;
  }

  if (!index) {
    // First packet - initialize
    totalSize = 0;
    lastProgress = 0;
    
    // Check Content-Length header first
    if (request->hasHeader("Content-Length")) {
      fileSize = request->header("Content-Length").toInt();
      if (fileSize > MAX_FILE_SIZE) {
        request->send(413, "text/plain", "ERROR: File exceeds 1MB limit");
        Serial.printf("Rejected upfront: %u bytes\n", fileSize);
        return;
      }
      Serial.printf("Starting upload (total size: %u bytes)\n", fileSize);
    } else {
      Serial.println("Starting upload (unknown total size)");
    }

    // Delete old file if exists
    if (SPIFFS.exists("/img.gif")) {
      SPIFFS.remove("/img.gif");
    }

    request->_tempFile = SPIFFS.open("/img.gif", "w");
    if (!request->_tempFile) {
      request->send(500, "text/plain", "ERROR: Couldn't create file");
      return;
    }
  }

  // Track size during upload (for clients that don't send Content-Length)
  totalSize += len;
  
  size_t progress = (totalSize * 100) / fileSize;
  if (progress >= lastProgress + 1) {
    Serial.printf("Upload progress: %u%% (%u bytes)\n", progress, totalSize);
    lastProgress = progress;
  }

  if (totalSize > MAX_FILE_SIZE) {
    request->_tempFile.close();
    SPIFFS.remove("/img.gif");
    request->send(413, "text/plain", "ERROR: File exceeded 1MB during upload");
    Serial.printf("Aborted during upload: %u/%u bytes\n", totalSize, MAX_FILE_SIZE);
    totalSize = 0;
    return;
  }

  // Write data
  if (len) {
    if (request->_tempFile.write(data, len) != len) {
      request->_tempFile.close();
      SPIFFS.remove("/img.gif");
      request->send(500, "text/plain", "ERROR: Write failed");
      totalSize = 0;
      return;
    }
  }

  // Final packet
  if (final) {
    request->_tempFile.close();
    Serial.printf("Upload complete: %u bytes\n", totalSize);
    totalSize = 0;
    request->send(200, "text/plain", "OK: File saved as /img.gif");
  }
}