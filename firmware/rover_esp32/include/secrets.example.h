#pragma once

// Copy this file to secrets.h. Never commit the real file.
constexpr char WIFI_SSID[] = "YOUR_WIFI_NAME";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

// Use the PC's LAN address, not localhost. Example: 192.168.1.50
constexpr char PC_SERVER_HOST[] = "192.168.1.50";
constexpr uint16_t PC_SERVER_PORT = 8000;
constexpr char ROVER_DEVICE_ID[] = "aura-rover-01";
constexpr char ROVER_TOKEN[] = "replace-with-a-long-random-token";

// Example: http://192.168.1.60
constexpr char HOME_NODE_BASE_URL[] = "http://192.168.1.60";
constexpr char HOME_NODE_TOKEN[] = "replace-with-another-long-random-token";

