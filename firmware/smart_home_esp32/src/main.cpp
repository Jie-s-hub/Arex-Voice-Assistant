#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>

#include "AppConfig.h"
#include "RelayController.h"

namespace {

WebServer server(aura::home_config::kHttpPort);
aura::RelayController relays;
String recentRequestIds[8];
uint8_t recentRequestCursor = 0;
uint32_t lastWifiAttemptMs = 0;
bool wifiStarted = false;
wl_status_t previousWifiStatus = WL_NO_SHIELD;

void addState(JsonDocument& doc) {
  JsonObject devices = doc.createNestedObject("devices");
  devices["bedroom_light"] = relays.bedroomLightOn() ? "on" : "off";
  devices["fan"] = relays.fanOn() ? "on" : "off";
}

void sendJson(int statusCode, JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  server.send(statusCode, "application/json", body);
}

bool isAuthorized() {
  if (!server.hasHeader("Authorization")) {
    return false;
  }
  return server.header("Authorization") ==
         String("Bearer ") + HOME_NODE_TOKEN;
}

bool hasSeenRequest(const String& requestId) {
  for (const String& saved : recentRequestIds) {
    if (saved.length() > 0 && saved == requestId) {
      return true;
    }
  }
  return false;
}

void rememberRequest(const String& requestId) {
  recentRequestIds[recentRequestCursor] = requestId;
  recentRequestCursor = (recentRequestCursor + 1) % 8;
}

void handleHealth() {
  StaticJsonDocument<384> response;
  response["ok"] = true;
  response["service"] = "aura-smart-home";
  response["firmware"] = "1.0.0";
  response["ip"] = WiFi.localIP().toString();
  addState(response);
  sendJson(200, response);
}

void handleCommand() {
  StaticJsonDocument<512> response;
  if (!isAuthorized()) {
    response["ok"] = false;
    response["error"] = "unauthorized";
    sendJson(401, response);
    return;
  }
  if (!server.hasArg("plain") || server.arg("plain").length() > 1024) {
    response["ok"] = false;
    response["error"] = "missing or oversized JSON body";
    sendJson(400, response);
    return;
  }

  StaticJsonDocument<512> request;
  const DeserializationError error = deserializeJson(request, server.arg("plain"));
  if (error) {
    response["ok"] = false;
    response["error"] = "invalid JSON";
    sendJson(400, response);
    return;
  }

  const int version = request["version"] | 0;
  const String requestId = request["request_id"].as<String>();
  const String command = request["command"].as<String>();
  const String device = request["device"].as<String>();
  const String state = request["state"].as<String>();
  if (version != 1 || requestId.length() < 8 || command != "set") {
    response["ok"] = false;
    response["error"] = "invalid version, request_id, or command";
    sendJson(422, response);
    return;
  }

  if (hasSeenRequest(requestId)) {
    response["ok"] = true;
    response["request_id"] = requestId;
    response["duplicate"] = true;
    addState(response);
    sendJson(200, response);
    return;
  }

  if (!relays.set(device, state)) {
    response["ok"] = false;
    response["request_id"] = requestId;
    response["error"] = "unsupported device/state combination";
    sendJson(422, response);
    return;
  }

  rememberRequest(requestId);
  response["ok"] = true;
  response["request_id"] = requestId;
  response["duplicate"] = false;
  addState(response);
  sendJson(200, response);
}

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  WiFi.mode(WIFI_STA);
  if (!wifiStarted) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiStarted = true;
  } else {
    WiFi.reconnect();
  }
  lastWifiAttemptMs = millis();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  relays.begin();  // Safe/off before Wi-Fi or HTTP starts.
  connectWifi();

  const char* headers[] = {"Authorization"};
  server.collectHeaders(headers, 1);
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/api/v1/commands", HTTP_POST, handleCommand);
  server.onNotFound([] {
    server.send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
  });
  server.begin();
}

void loop() {
  server.handleClient();
  const wl_status_t wifiStatus = WiFi.status();
  if (wifiStatus != previousWifiStatus) {
    previousWifiStatus = wifiStatus;
    if (wifiStatus == WL_CONNECTED) {
      Serial.printf("Smart-home node: http://%s\n",
                    WiFi.localIP().toString().c_str());
    }
  }
  if (wifiStatus != WL_CONNECTED &&
      millis() - lastWifiAttemptMs >= aura::home_config::kWifiRetryMs) {
    connectWifi();
  }
  delay(2);
}
