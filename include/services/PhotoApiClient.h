#pragma once

#include <Arduino.h>
#include <HTTPClient.h>
#include <functional>

#include "services/PhotoTypes.h"

// HTTP status code categories for specialized handling
enum class HttpStatusCategory {
  Success,         // 2xx - Success
  NotFound,        // 404 - Resource not found, don't retry
  RateLimited,     // 429 - Too many requests, respect Retry-After
  Unauthorized,    // 401 - Auth issue, don't retry
  ServerError,     // 5xx - Server error, retry with backoff
  ClientError,     // Other 4xx - Client error, don't retry
  NetworkError     // Connection/timeout errors
};

// Progress callback type: receives progress percentage (0-100)
using ProgressCallback = std::function<void(int progress)>;

// Request metadata for logging/telemetry
struct RequestMetadata {
  uint32_t requestNumber;
  uint32_t startTime;
  uint32_t endTime;
  uint32_t bytesTransferred;
  uint8_t retryCount;
  bool success;
  bool cancelled;
  int httpCode;
  String errorMessage;
};

class PhotoApiClient {
public:
  PhotoApiClient(const char* apiUrl, uint32_t timeoutMs = 15000);
  ~PhotoApiClient();

  // Core fetch method with retry logic and exponential backoff
  bool fetchRandomPhoto(PhotoInfo& outPhoto, String& outError);
  
  // Overloaded fetch with progress callback
  bool fetchRandomPhoto(PhotoInfo& outPhoto, String& outError, ProgressCallback progressCallback);

  // Request state management
  bool isRequestInProgress() const { return _requestInProgress; }
  void cancelCurrentRequest();

  // Get last request metadata for telemetry
  const RequestMetadata& getLastRequestMetadata() const { return _lastMetadata; }

  // Configuration setters
  void setRetryCount(uint8_t count) { _retryCount = count; }
  void setRetryBaseDelay(uint32_t delayMs) { _retryBaseDelayMs = delayMs; }
  void setConnectionTimeout(uint32_t timeoutMs) { _connectionTimeoutMs = timeoutMs; }

private:
  // Internal fetch implementation
  bool fetchWithRetry(PhotoInfo& outPhoto, String& outError, ProgressCallback progressCallback);
  
  // HTTP status handling
  HttpStatusCategory categorizeHttpStatus(int httpCode);
  bool shouldRetry(HttpStatusCategory category);
  uint32_t getRetryDelay(HttpStatusCategory category, int retryAttempt, HTTPClient& client);
  String getHttpErrorMessage(int httpCode);

  // Logging
  void logRequestStart();
  void logRequestEnd(bool success, const String& error = "");

  const char* _apiUrl;
  uint32_t _timeoutMs;
  uint32_t _connectionTimeoutMs;
  uint8_t _retryCount;
  uint32_t _retryBaseDelayMs;

  // Request state tracking
  volatile bool _requestInProgress;
  volatile bool _cancelRequested;
  uint32_t _lastRequestStartTime;
  uint8_t _currentRetryCount;

  // Request metadata for telemetry
  RequestMetadata _lastMetadata;
  uint32_t _requestCounter;
};
