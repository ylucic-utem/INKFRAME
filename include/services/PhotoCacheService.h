#pragma once

#include <Arduino.h>

#include "services/PhotoTypes.h"

// ============ Cache Configuration Constants ============
static constexpr uint8_t CACHE_MAX_IMAGES = 10;  // Maximum number of cached photos
static constexpr const char* CACHE_DIR = "/cache";  // Cache directory on SD card

/**
 * Structure representing a single cached photo entry.
 * Contains all metadata needed for cache management and validation.
 */
struct CacheEntry {
  PhotoInfo photoInfo;       // Photo metadata (title, url, description)
  String localFilePath;      // Path to cached image file
  String metaFilePath;       // Path to metadata JSON file
  uint32_t cachedTimestamp;  // Timestamp when cached (millis or epoch)
  uint32_t accessCount;      // Access count for LRU tracking
  uint32_t crc32;            // CRC32 checksum for validation
  bool valid;                // Whether this entry contains valid data
  
  CacheEntry() : cachedTimestamp(0), accessCount(0), crc32(0), valid(false) {}
};

/**
 * Multi-image cache service for photo browsing.
 * Provides persistent storage of multiple photos enabling offline browsing
 * and navigation between previously viewed photos.
 */
class PhotoCacheService {
public:
  // Legacy paths (keeping for backward compatibility)
  static constexpr const char* kPhotosDir = "/PHOTOS";
  static constexpr const char* kLastImagePath = "/PHOTOS/last.jpg";
  static constexpr const char* kLastMetaPath  = "/PHOTOS/last.meta";

  // Working files (staging during downloads)
  static constexpr const char* kStagingImagePath = "/PHOTOS/staging.jpg";
  static constexpr const char* kPrefetchImagePath = "/PHOTOS/prefetch.jpg";
  static constexpr const char* kPrefetchMetaPath  = "/PHOTOS/prefetch.meta";
  
  // Cache file naming
  static constexpr const char* kCacheIndexFile = "/cache/index.json";

  // ---------- Legacy single-image functions ----------
  // Reads PhotoInfo from a simple key/value text file. Returns false if missing or unreadable.
  static bool loadMeta(const char* metaPath, PhotoInfo& outPhoto);

  // Writes PhotoInfo to a key/value text file.
  static bool saveMeta(const char* metaPath, const PhotoInfo& photo);

  // Promotes a downloaded image (and optional metadata) to be the new last image.
  // This is done by remove(dst) + rename(src->dst) to avoid corrupting the old last image.
  static bool promoteToLast(const char* srcImagePath, const char* srcMetaPath, String& outError);

  // ---------- Multi-image cache functions ----------
  
  /**
   * Initialize the cache system. Creates cache directory if needed,
   * scans for existing cached files, and restores state from previous sessions.
   * Should be called once during setup.
   * @return true if initialization succeeded
   */
  static bool initCache();
  
  /**
   * Save a photo to the cache using circular buffer logic.
   * Automatically manages cache size and removes old entries when full.
   * @param photo PhotoInfo metadata to cache
   * @param imagePath Path to the image file to cache
   * @return true if saved successfully
   */
  static bool saveToCache(const PhotoInfo& photo, const char* imagePath);
  
  /**
   * Get a cached photo by its cache index.
   * @param cacheIndex Index in the cache (0 to getCachedPhotoCount()-1)
   * @param outPhoto Output PhotoInfo
   * @param outImagePath Output path to the cached image file
   * @return true if the entry exists and is valid
   */
  static bool getFromCache(uint8_t cacheIndex, PhotoInfo& outPhoto, String& outImagePath);
  
  /**
   * Get number of photos currently in the cache.
   * @return Count of valid cached photos (0 to CACHE_MAX_IMAGES)
   */
  static uint8_t getCachedPhotoCount();
  
  /**
   * Get the current cache write index (position of most recent photo).
   * @return Current cache position
   */
  static uint8_t getCurrentCacheIndex();
  
  /**
   * Validate a cache entry by checking its CRC32 checksum.
   * @param cacheIndex Index of the cache entry to validate
   * @return true if the entry is valid and not corrupted
   */
  static bool validateCacheEntry(uint8_t cacheIndex);
  
  /**
   * Remove corrupted or invalid cache entries.
   * This is called during maintenance to clean up the cache.
   */
  static void purgeInvalidEntries();
  
  /**
   * Get a cache entry by index (for advanced operations).
   * @param cacheIndex Index of the entry
   * @return Pointer to the cache entry or nullptr if invalid
   */
  static const CacheEntry* getCacheEntry(uint8_t cacheIndex);
  
  /**
   * Record an access to a cached photo (for LRU tracking).
   * @param cacheIndex Index of the accessed entry
   */
  static void recordAccess(uint8_t cacheIndex);
  
  /**
   * Check if offline mode should be enabled (WiFi unavailable but cache has photos).
   * @return true if offline mode is available
   */
  static bool hasOfflineContent();
  
  /**
   * Save cache index to SD for persistence across reboots.
   */
  static void saveCacheIndex();
  
  /**
   * Run cache maintenance (validation, cleanup). Call during idle time.
   * @param maxTimeMs Maximum time to spend on maintenance
   */
  static void runMaintenance(uint32_t maxTimeMs = 1000);

private:
  // Cache state
  static CacheEntry cacheEntries[CACHE_MAX_IMAGES];
  static uint8_t currentCacheIndex;
  static uint8_t cachedPhotoCount;
  static bool cacheInitialized;
  
  // Helper functions
  static String getCacheImagePath(uint8_t index);
  static String getCacheMetaPath(uint8_t index);
  static uint32_t calculateCRC32(const char* filePath);
  static bool loadCacheEntry(uint8_t index);
  static bool saveCacheEntry(uint8_t index);
  static void loadCacheIndex();
};
