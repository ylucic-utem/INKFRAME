#include "services/PhotoApiClient.h"

#include <ArduinoJson.h>
#include <WiFi.h>

#include "config.h"

// Constructor with default configuration from config.h
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
    _reusableClient(nullptr),
    _lastHost(""),
    _hasReusableConnection(false),
    _requestCounter(0) {
  memset(&_lastMetadata, 0, sizeof(_lastMetadata));
}

PhotoApiClient::~PhotoApiClient() {
  cancelCurrentRequest();
  if (_reusableClient) {
    _reusableClient->end();
    delete _reusableClient;
    _reusableClient = nullptr;
  }
}

// Main fetch method - delegates to fetchWithRetry
bool PhotoApiClient::fetchRandomPhoto(PhotoInfo& outPhoto, String& outError) {
  return fetchWithRetry(outPhoto, outError, nullptr);
}

// Overloaded fetch with progress callback
bool PhotoApiClient::fetchRandomPhoto(PhotoInfo& outPhoto, String& outError, ProgressCallback progressCallback) {
  return fetchWithRetry(outPhoto, outError, progressCallback);
}

// Cancel any request in progress
void PhotoApiClient::cancelCurrentRequest() {
  if (!_requestInProgress) {
    return;
  }

  Serial.println("[PhotoApiClient] Cancelling current request...");
  _cancelRequested = true;

  // If we have a reusable client, end it immediately to close the socket
  if (_reusableClient) {
    _reusableClient->end();
  }

  _requestInProgress = false;
  _hasReusableConnection = false;

  // Update metadata
  _lastMetadata.cancelled = true;
  _lastMetadata.success = false;
  _lastMetadata.endTime = millis();

  Serial.println("[PhotoApiClient] Request cancelled");
}

// Categorize HTTP status codes for specialized handling
HttpStatusCategory PhotoApiClient::categorizeHttpStatus(int httpCode) {
  if (httpCode >= 200 && httpCode < 300) {
    return HttpStatusCategory::Success;
  }
  if (httpCode == 404) {
    return HttpStatusCategory::NotFound;
  }
  if (httpCode == 429) {
    return HttpStatusCategory::RateLimited;
  }
  if (httpCode == 401 || httpCode == 403) {
    return HttpStatusCategory::Unauthorized;
  }
  if (httpCode >= 500 && httpCode < 600) {
    return HttpStatusCategory::ServerError;
  }
  if (httpCode >= 400 && httpCode < 500) {
    return HttpStatusCategory::ClientError;
  }
  // Negative codes from HTTPClient indicate network errors
  return HttpStatusCategory::NetworkError;
}

// Determine if a request should be retried based on error category
bool PhotoApiClient::shouldRetry(HttpStatusCategory category) {
  switch (category) {
    case HttpStatusCategory::ServerError:
    case HttpStatusCategory::NetworkError:
    case HttpStatusCategory::RateLimited:
      return true;
    case HttpStatusCategory::NotFound:
    case HttpStatusCategory::Unauthorized:
    case HttpStatusCategory::ClientError:
    case HttpStatusCategory::Success:
    default:
      return false;
  }
}

// Calculate retry delay with exponential backoff
uint32_t PhotoApiClient::getRetryDelay(HttpStatusCategory category, int retryAttempt, HTTPClient& client) {
  // For rate limiting, respect Retry-After header if present
  if (category == HttpStatusCategory::RateLimited) {
    String retryAfter = client.header("Retry-After");
    if (retryAfter.length() > 0) {
      int seconds = retryAfter.toInt();
      if (seconds > 0 && seconds <= 60) {  // Cap at 60 seconds
        return seconds * 1000;
      }
    }
    // Default longer delay for rate limiting
    return 5000;
  }

  // Exponential backoff: 1s, 2s, 4s...
  uint32_t delay = _retryBaseDelayMs * (1 << retryAttempt);
  return min(delay, (uint32_t)10000);  // Cap at 10 seconds
}

// Generate user-friendly error messages for HTTP codes
String PhotoApiClient::getHttpErrorMessage(int httpCode) {
  switch (httpCode) {
    case HTTPC_ERROR_CONNECTION_REFUSED:
      return "Connection refused";
    case HTTPC_ERROR_SEND_HEADER_FAILED:
      return "Failed to send request";
    case HTTPC_ERROR_SEND_PAYLOAD_FAILED:
      return "Failed to send data";
    case HTTPC_ERROR_NOT_CONNECTED:
      return "Not connected";
    case HTTPC_ERROR_CONNECTION_LOST:
      return "Connection lost";
    case HTTPC_ERROR_NO_STREAM:
      return "No data stream";
    case HTTPC_ERROR_NO_HTTP_SERVER:
      return "Server not found";
    case HTTPC_ERROR_TOO_LESS_RAM:
      return "Low memory";
    case HTTPC_ERROR_ENCODING:
      return "Encoding error";
    case HTTPC_ERROR_STREAM_WRITE:
      return "Write error";
    case HTTPC_ERROR_READ_TIMEOUT:
      return "Read timeout";
    case 401:
      return "Unauthorized - check API credentials";
    case 403:
      return "Forbidden - access denied";
    case 404:
      return "Not found";
    case 429:
      return "Rate limited - too many requests";
    case 500:
      return "Server error";
    case 502:
      return "Bad gateway";
    case 503:
      return "Service unavailable";
    case 504:
      return "Gateway timeout";
    default:
      if (httpCode < 0) {
        return String("Network error: ") + httpCode;
      }
      return String("HTTP ") + httpCode;
  }
}

// Try to reuse existing connection
bool PhotoApiClient::tryReuseConnection() {
  if (!_hasReusableConnection || !_reusableClient) {
    return false;
  }

  // Check if connection is still valid
  if (_reusableClient->connected()) {
    Serial.println("[PhotoApiClient] Reusing existing connection");
    return true;
  }

  // Connection dropped, clean up
  Serial.println("[PhotoApiClient] Previous connection dropped, creating new one");
  _reusableClient->end();
  _hasReusableConnection = false;
  return false;
}

// Save connection for potential reuse
void PhotoApiClient::saveConnectionForReuse() {
  _hasReusableConnection = true;
  Serial.println("[PhotoApiClient] Connection saved for reuse");
}

// Log request start for telemetry
void PhotoApiClient::logRequestStart() {
  _requestCounter++;
  _lastMetadata.requestNumber = _requestCounter;
  _lastMetadata.startTime = millis();
  _lastMetadata.retryCount = 0;
  _lastMetadata.success = false;
  _lastMetadata.cancelled = false;
  _lastMetadata.bytesTransferred = 0;
  _lastMetadata.errorMessage = "";

  Serial.printf("[PhotoApiClient] Request #%lu started at %lu ms\n", 
                _requestCounter, _lastMetadata.startTime);
}

// Log request end for telemetry
void PhotoApiClient::logRequestEnd(bool success, const String& error) {
  _lastMetadata.endTime = millis();
  _lastMetadata.success = success;
  _lastMetadata.retryCount = _currentRetryCount;
  if (!success && error.length() > 0) {
    _lastMetadata.errorMessage = error;
  }

  uint32_t duration = _lastMetadata.endTime - _lastMetadata.startTime;
  
  Serial.printf("[PhotoApiClient] Request #%lu completed in %lu ms\n", 
                _lastMetadata.requestNumber, duration);
  Serial.printf("  Success: %s, Retries: %d, Bytes: %lu\n",
                success ? "YES" : "NO", 
                _lastMetadata.retryCount,
                _lastMetadata.bytesTransferred);
  
  if (!success) {
    Serial.printf("  Error: %s\n", _lastMetadata.errorMessage.c_str());
  }
}

// Main fetch implementation with retry logic
bool PhotoApiClient::fetchWithRetry(PhotoInfo& outPhoto, String& outError, ProgressCallback progressCallback) {
  // Check if already processing a request
  if (_requestInProgress) {
    outError = "Request already in progress";
    return false;
  }

  // Initialize request state
  _requestInProgress = true;
  _cancelRequested = false;
  _currentRetryCount = 0;
  _lastRequestStartTime = millis();

  logRequestStart();

  // Create or reuse HTTPClient
  HTTPClient* client;
  bool ownsClient = false;

  if (tryReuseConnection()) {
    client = _reusableClient;
  } else {
    if (_reusableClient) {
      _reusableClient->end();
      delete _reusableClient;
    }
    _reusableClient = new HTTPClient();
    client = _reusableClient;
    ownsClient = true;
  }

  bool success = false;
  int lastHttpCode = 0;
  HttpStatusCategory lastCategory = HttpStatusCategory::NetworkError;

  // Retry loop
  for (uint8_t attempt = 0; attempt <= _retryCount && !_cancelRequested; attempt++) {
    _currentRetryCount = attempt;

    if (attempt > 0) {
      // Calculate delay with exponential backoff
      uint32_t delayMs = getRetryDelay(lastCategory, attempt - 1, *client);
      Serial.printf("[PhotoApiClient] Retry %d/%d after %lu ms delay\n", 
                    attempt, _retryCount, delayMs);
      
      // Check for cancellation during delay
      uint32_t delayStart = millis();
      while (millis() - delayStart < delayMs) {
        if (_cancelRequested) {
          Serial.println("[PhotoApiClient] Cancelled during retry delay");
          break;
        }
        delay(50);
      }
      if (_cancelRequested) break;
    }

    // Configure timeouts
    client->setTimeout(_timeoutMs);
    client->setConnectTimeout(_connectionTimeoutMs);

    // Collect response headers for Retry-After
    const char* headerKeys[] = {"Retry-After", "Content-Length"};
    client->collectHeaders(headerKeys, 2);

    // Begin connection
    if (!client->begin(_apiUrl)) {
      outError = "Failed to begin HTTP request";
      lastHttpCode = HTTPC_ERROR_NOT_CONNECTED;
      lastCategory = HttpStatusCategory::NetworkError;
      continue;
    }

    client->addHeader("accept", "application/json");

    // Check for cancellation before request
    if (_cancelRequested) {
      client->end();
      break;
    }

    // Make the request
    lastHttpCode = client->GET();
    _lastMetadata.httpCode = lastHttpCode;
    lastCategory = categorizeHttpStatus(lastHttpCode);

    // Handle based on response category
    if (lastCategory == HttpStatusCategory::Success) {
      // Parse JSON response
      WiFiClient* stream = client->getStreamPtr();
      if (!stream) {
        outError = "No response stream";
        lastCategory = HttpStatusCategory::NetworkError;
        client->end();
        _hasReusableConnection = false;
        continue;
      }

      // Get content length for progress tracking
      int contentLength = client->getSize();
      _lastMetadata.bytesTransferred = contentLength > 0 ? contentLength : 0;

      // Report progress
      if (progressCallback) {
        progressCallback(50);  // Indicate response received
      }

      StaticJsonDocument<JSON_DOCUMENT_SIZE> doc;
      DeserializationError jsonError = deserializeJson(doc, *stream);

      // Report progress
      if (progressCallback) {
        progressCallback(75);  // Indicate JSON parsed
      }

      if (jsonError) {
        outError = String("JSON Parse Error: ") + jsonError.c_str();
        client->end();
        _hasReusableConnection = false;
        // Don't retry JSON errors - they won't fix themselves
        break;
      }

      // Validate response structure
      if (!doc["success"].is<bool>() || !doc["success"].as<bool>()) {
        outError = "API returned success=false";
        client->end();
        _hasReusableConnection = false;
        // Don't retry API logic errors
        break;
      }

      if (!doc["photos"].is<JsonArray>() || doc["photos"].size() == 0) {
        outError = "No photos found";
        client->end();
        _hasReusableConnection = false;
        // Don't retry - endpoint has no photos
        break;
      }

      // Extract photo info
      JsonObject photo = doc["photos"][0];
      outPhoto.url = photo["url"].as<String>();
      outPhoto.title = photo["title"].as<String>();
      outPhoto.description = photo["description"].as<String>();

      if (outPhoto.url.length() == 0) {
        outError = "Missing photo URL";
        client->end();
        _hasReusableConnection = false;
        break;
      }

      // Success!
      success = true;
      saveConnectionForReuse();

      if (progressCallback) {
        progressCallback(100);
      }
      break;
    }

    // Handle specific error categories
    if (lastCategory == HttpStatusCategory::NotFound) {
      // 404 - Don't retry, request a different photo
      outError = getHttpErrorMessage(lastHttpCode);
      Serial.println("[PhotoApiClient] 404 Not Found - not retrying");
      client->end();
      _hasReusableConnection = false;
      break;
    }

    if (lastCategory == HttpStatusCategory::Unauthorized) {
      // Auth error - Don't retry
      outError = getHttpErrorMessage(lastHttpCode);
      Serial.println("[PhotoApiClient] Auth error - not retrying");
      client->end();
      _hasReusableConnection = false;
      break;
    }

    if (lastCategory == HttpStatusCategory::ClientError) {
      // Other 4xx - Don't retry
      outError = getHttpErrorMessage(lastHttpCode);
      Serial.printf("[PhotoApiClient] Client error %d - not retrying\n", lastHttpCode);
      client->end();
      _hasReusableConnection = false;
      break;
    }

    // For retryable errors (5xx, network, rate limit), continue to next attempt
    outError = getHttpErrorMessage(lastHttpCode);
    client->end();
    _hasReusableConnection = false;

    if (!shouldRetry(lastCategory)) {
      break;
    }
  }

  // Handle cancellation
  if (_cancelRequested) {
    outError = "Request cancelled";
    success = false;
    _lastMetadata.cancelled = true;
  }

  // Log and cleanup
  logRequestEnd(success, outError);
  _requestInProgress = false;

  return success;
}
