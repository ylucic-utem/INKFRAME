#include "services/PhotoApiClient.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "config.h"

PhotoApiClient::PhotoApiClient(const char* apiUrl, uint32_t timeoutMs)
  : _apiUrl(apiUrl),
    _timeoutMs(timeoutMs),
    _connectionTimeoutMs(API_CONNECTION_TIMEOUT_MS),
    _retryCount(API_RETRY_COUNT),
    _retryBaseDelayMs(API_RETRY_BASE_DELAY_MS),
    _requestInProgress(false),
    _cancelRequested(false),
    _lastRequestStartTime(0),
    _currentRetryCount(0),
    _requestCounter(0) {
  memset(&_lastMetadata, 0, sizeof(_lastMetadata));
}

PhotoApiClient::~PhotoApiClient() = default;

bool PhotoApiClient::fetchRandomPhoto(PhotoInfo& outPhoto, String& outError) {
  return fetchWithRetry(outPhoto, outError, nullptr);
}

bool PhotoApiClient::fetchRandomPhoto(PhotoInfo& outPhoto, String& outError, ProgressCallback progressCallback) {
  return fetchWithRetry(outPhoto, outError, progressCallback);
}

void PhotoApiClient::cancelCurrentRequest() {
  if (!_requestInProgress) {
    return;
  }

  Serial.println("[PhotoApiClient] Cancelling current request...");
  _cancelRequested = true;

  _lastMetadata.cancelled = true;
  _lastMetadata.success = false;
  _lastMetadata.endTime = millis();
}

HttpStatusCategory PhotoApiClient::categorizeHttpStatus(int httpCode) {
  if (httpCode >= 200 && httpCode < 300) return HttpStatusCategory::Success;
  if (httpCode == 404) return HttpStatusCategory::NotFound;
  if (httpCode == 429) return HttpStatusCategory::RateLimited;
  if (httpCode == 401 || httpCode == 403) return HttpStatusCategory::Unauthorized;
  if (httpCode >= 500 && httpCode < 600) return HttpStatusCategory::ServerError;
  if (httpCode >= 400 && httpCode < 500) return HttpStatusCategory::ClientError;
  return HttpStatusCategory::NetworkError;
}

bool PhotoApiClient::shouldRetry(HttpStatusCategory category) {
  switch (category) {
    case HttpStatusCategory::ServerError:
    case HttpStatusCategory::NetworkError:
    case HttpStatusCategory::RateLimited:
      return true;
    default:
      return false;
  }
}

uint32_t PhotoApiClient::getRetryDelay(HttpStatusCategory category, int retryAttempt, HTTPClient& client) {
  if (category == HttpStatusCategory::RateLimited) {
    String retryAfter = client.header("Retry-After");
    if (retryAfter.length() > 0) {
      int seconds = retryAfter.toInt();
      if (seconds > 0 && seconds <= 60) {
        return seconds * 1000;
      }
    }
    return 5000;
  }

  uint32_t delayMs = _retryBaseDelayMs * (1 << retryAttempt);
  return min(delayMs, static_cast<uint32_t>(10000));
}

String PhotoApiClient::getHttpErrorMessage(int httpCode) {
  switch (httpCode) {
    case HTTPC_ERROR_CONNECTION_REFUSED: return "Connection refused";
    case HTTPC_ERROR_SEND_HEADER_FAILED: return "Failed to send request";
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED: return "Failed to send data";
    case HTTPC_ERROR_NOT_CONNECTED: return "Not connected";
    case HTTPC_ERROR_CONNECTION_LOST: return "Connection lost";
    case HTTPC_ERROR_NO_STREAM: return "No data stream";
    case HTTPC_ERROR_NO_HTTP_SERVER: return "Server not found";
    case HTTPC_ERROR_TOO_LESS_RAM: return "Low memory";
    case HTTPC_ERROR_ENCODING: return "Encoding error";
    case HTTPC_ERROR_STREAM_WRITE: return "Write error";
    case HTTPC_ERROR_READ_TIMEOUT: return "Read timeout";
    case 401: return "Unauthorized - check API credentials";
    case 403: return "Forbidden - access denied";
    case 404: return "Not found";
    case 429: return "Rate limited - too many requests";
    case 500: return "Server error";
    case 502: return "Bad gateway";
    case 503: return "Service unavailable";
    case 504: return "Gateway timeout";
    default:
      if (httpCode < 0) return String("Network error: ") + httpCode;
      return String("HTTP ") + httpCode;
  }
}

void PhotoApiClient::logRequestStart() {
  _requestCounter++;
  _lastMetadata.requestNumber = _requestCounter;
  _lastMetadata.startTime = millis();
  _lastMetadata.retryCount = 0;
  _lastMetadata.success = false;
  _lastMetadata.cancelled = false;
  _lastMetadata.bytesTransferred = 0;
  _lastMetadata.errorMessage = "";

  Serial.printf("[PhotoApiClient] Request #%lu started at %lu ms\n", _requestCounter, _lastMetadata.startTime);
}

void PhotoApiClient::logRequestEnd(bool success, const String& error) {
  _lastMetadata.endTime = millis();
  _lastMetadata.success = success;
  _lastMetadata.retryCount = _currentRetryCount;
  if (!success && error.length() > 0) {
    _lastMetadata.errorMessage = error;
  }

  const uint32_t duration = _lastMetadata.endTime - _lastMetadata.startTime;
  Serial.printf("[PhotoApiClient] Request #%lu completed in %lu ms\n", _lastMetadata.requestNumber, duration);
  Serial.printf("  Success: %s, Retries: %d, Bytes: %lu\n",
                success ? "YES" : "NO",
                _lastMetadata.retryCount,
                _lastMetadata.bytesTransferred);

  if (!success) {
    Serial.printf("  Error: %s\n", _lastMetadata.errorMessage.c_str());
  }
}

bool PhotoApiClient::fetchWithRetry(PhotoInfo& outPhoto, String& outError, ProgressCallback progressCallback) {
  if (_requestInProgress) {
    outError = "Request already in progress";
    return false;
  }

  _requestInProgress = true;
  _cancelRequested = false;
  _currentRetryCount = 0;
  _lastRequestStartTime = millis();

  logRequestStart();

  bool success = false;
  int lastHttpCode = 0;
  HttpStatusCategory lastCategory = HttpStatusCategory::NetworkError;
  HTTPClient client;

  for (uint8_t attempt = 0; attempt <= _retryCount && !_cancelRequested; ++attempt) {
    _currentRetryCount = attempt;

    if (attempt > 0) {
      const uint32_t delayMs = getRetryDelay(lastCategory, attempt - 1, client);
      Serial.printf("[PhotoApiClient] Retry %d/%d after %lu ms delay\n", attempt, _retryCount, delayMs);

      const uint32_t delayStart = millis();
      while (millis() - delayStart < delayMs) {
        if (_cancelRequested) {
          Serial.println("[PhotoApiClient] Cancelled during retry delay");
          break;
        }
        delay(50);
      }
      if (_cancelRequested) break;
    }

    client.setTimeout(_timeoutMs);
    client.setConnectTimeout(_connectionTimeoutMs);

    const char* headerKeys[] = {"Retry-After", "Content-Length"};
    client.collectHeaders(headerKeys, 2);

    if (!client.begin(_apiUrl)) {
      outError = "Failed to begin HTTP request";
      lastHttpCode = HTTPC_ERROR_NOT_CONNECTED;
      lastCategory = HttpStatusCategory::NetworkError;
      continue;
    }

    client.addHeader("accept", "application/json");

    if (_cancelRequested) {
      client.end();
      break;
    }

    lastHttpCode = client.GET();
    _lastMetadata.httpCode = lastHttpCode;
    lastCategory = categorizeHttpStatus(lastHttpCode);

    if (lastCategory == HttpStatusCategory::Success) {
      WiFiClient* stream = client.getStreamPtr();
      if (!stream) {
        outError = "No response stream";
        lastCategory = HttpStatusCategory::NetworkError;
        client.end();
        continue;
      }

      int contentLength = client.getSize();
      _lastMetadata.bytesTransferred = contentLength > 0 ? contentLength : 0;

      if (progressCallback) progressCallback(50);

      StaticJsonDocument<JSON_DOCUMENT_SIZE> doc;
      DeserializationError jsonError = deserializeJson(doc, *stream);

      if (progressCallback) progressCallback(75);

      if (jsonError) {
        outError = String("JSON Parse Error: ") + jsonError.c_str();
        client.end();
        break;
      }

      if (!doc["success"].is<bool>() || !doc["success"].as<bool>()) {
        outError = "API returned success=false";
        client.end();
        break;
      }

      if (!doc["photos"].is<JsonArray>() || doc["photos"].size() == 0) {
        outError = "No photos found";
        client.end();
        break;
      }

      JsonObject photo = doc["photos"][0];
      outPhoto.url = photo["url"].as<String>();
      outPhoto.title = photo["title"].as<String>();
      outPhoto.description = photo["description"].as<String>();

      if (outPhoto.url.length() == 0) {
        outError = "Missing photo URL";
        client.end();
        break;
      }

      success = true;
      if (progressCallback) progressCallback(100);
      client.end();
      break;
    }

    outError = getHttpErrorMessage(lastHttpCode);

    if (lastCategory == HttpStatusCategory::NotFound) {
      Serial.println("[PhotoApiClient] 404 Not Found - not retrying");
      client.end();
      break;
    }

    if (lastCategory == HttpStatusCategory::Unauthorized) {
      Serial.println("[PhotoApiClient] Auth error - not retrying");
      client.end();
      break;
    }

    if (lastCategory == HttpStatusCategory::ClientError) {
      Serial.printf("[PhotoApiClient] Client error %d - not retrying\n", lastHttpCode);
      client.end();
      break;
    }

    client.end();
    if (!shouldRetry(lastCategory)) {
      break;
    }
  }

  if (_cancelRequested) {
    outError = "Request cancelled";
    success = false;
    _lastMetadata.cancelled = true;
  }

  logRequestEnd(success, outError);
  _requestInProgress = false;

  return success;
}
