#pragma once

#include <Arduino.h>

namespace aura {

struct HomeCommand {
  String requestId;
  String device;
  String state;
};

struct HomeResult {
  bool ok = false;
  int statusCode = 0;
  String response;
};

class HomeClient {
 public:
  HomeResult execute(const HomeCommand& command);
};

}  // namespace aura

