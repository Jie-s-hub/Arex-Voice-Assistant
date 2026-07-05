#include "HomeClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "AppConfig.h"

namespace aura {

HomeResult HomeClient::execute(const HomeCommand& command) {
  HomeResult result;
  HTTPClient http;
  const String url = String(HOME_NODE_BASE_URL) + "/api/v1/commands";
  if (!http.begin(url)) {
    result.response = "invalid home-node URL";
    return result;
  }
  http.setTimeout(1500);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + HOME_NODE_TOKEN);

  StaticJsonDocument<384> doc;
  doc["version"] = 1;
  doc["request_id"] = command.requestId;
  doc["source"] = ROVER_DEVICE_ID;
  doc["command"] = "set";
  doc["device"] = command.device;
  doc["state"] = command.state;
  String body;
  serializeJson(doc, body);

  result.statusCode = http.POST(body);
  result.response = http.getString();
  result.ok = result.statusCode >= 200 && result.statusCode < 300;
  http.end();
  return result;
}

}  // namespace aura

