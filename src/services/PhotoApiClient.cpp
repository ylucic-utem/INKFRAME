#include "services/PhotoApiClient.h"

#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "config.h"

PhotoApiClient::PhotoApiClient(const char* apiUrl, uint32_t timeoutMs)
  : _apiUrl(apiUrl), _timeoutMs(timeoutMs) {}

bool PhotoApiClient::fetchRandomPhoto(PhotoInfo& outPhoto, String& outError) {
  HTTPClient http;
  http.setTimeout(_timeoutMs);

  http.begin(_apiUrl);
  http.addHeader("accept", "application/json");

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    outError = String("HTTP Error: ") + httpCode;
    http.end();
    return false;
  }

  StaticJsonDocument<JSON_DOCUMENT_SIZE> doc;
  WiFiClient* stream = http.getStreamPtr();
  const DeserializationError error = deserializeJson(doc, *stream);
  http.end();
  if (error) {
    outError = String("JSON Parse Error: ") + error.c_str();
    return false;
  }

  if (!doc["success"].is<bool>() || !doc["success"].as<bool>()) {
    outError = "API returned success=false";
    return false;
  }

  if (!doc["photos"].is<JsonArray>() || doc["photos"].size() == 0) {
    outError = "No photos found";
    return false;
  }

  JsonObject photo = doc["photos"][0];

  outPhoto.url = photo["url"].as<String>();
  outPhoto.title = photo["title"].as<String>();
  outPhoto.description = photo["description"].as<String>();

  if (outPhoto.url.length() == 0) {
    outError = "Missing photo URL";
    return false;
  }

  return true;
}
