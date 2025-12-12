#include "services/PhotoCacheService.h"

#include <SD.h>
#include <ArduinoJson.h>

#include "services/IoMutex.h"

// Static member initialization
CacheEntry PhotoCacheService::cacheEntries[CACHE_MAX_IMAGES];
uint8_t PhotoCacheService::currentCacheIndex = 0;
uint8_t PhotoCacheService::cachedPhotoCount = 0;
bool PhotoCacheService::cacheInitialized = false;

namespace {

static bool writeLine(File& f, const char* key, const String& value) {
  f.print(key);
  f.print('=');
  f.println(value);
  return true;
}

static void trimCR(String& s) {
  if (s.endsWith("\r")) {
    s.remove(s.length() - 1);
  }
}

static bool replaceFile(const char* src, const char* dst, String& outError) {
  if (SD.exists(dst)) {
    if (!SD.remove(dst)) {
      outError = String("Failed to remove existing file: ") + dst;
      return false;
    }
  }

  if (!SD.exists(src)) {
    outError = String("Source file missing: ") + src;
    return false;
  }

  if (!SD.rename(src, dst)) {
    outError = String("Failed to rename ") + src + " -> " + dst;
    return false;
  }

  return true;
}

// CRC32 lookup table for polynomial 0x04C11DB7
static const uint32_t crc32Table[256] = {
  0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
  0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
  0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
  0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
  0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
  0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
  0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
  0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
  0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
  0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
  0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c9, 0xf50fc457,
  0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
  0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
  0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7a9c,
  0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e049,
  0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
  0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
  0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
  0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
  0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
  0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
  0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
  0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
  0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
  0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
  0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
  0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
  0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
  0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
  0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
  0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd706b3, 0x54de5729, 0x23d967bf,
  0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

} // namespace

// ---------- Legacy single-image functions ----------

bool PhotoCacheService::saveMeta(const char* metaPath, const PhotoInfo& photo) {
  IoGuard guard;

  // FILE_WRITE appends on Arduino SD, so remove first to overwrite.
  if (SD.exists(metaPath)) {
    SD.remove(metaPath);
  }

  File f = SD.open(metaPath, FILE_WRITE);
  if (!f) {
    return false;
  }

  writeLine(f, "TITLE", photo.title);
  writeLine(f, "URL", photo.url);
  writeLine(f, "DESC", photo.description);

  f.close();
  return true;
}

bool PhotoCacheService::loadMeta(const char* metaPath, PhotoInfo& outPhoto) {
  IoGuard guard;

  if (!SD.exists(metaPath)) {
    return false;
  }

  File f = SD.open(metaPath, FILE_READ);
  if (!f) {
    return false;
  }

  PhotoInfo p;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    trimCR(line);
    const int eq = line.indexOf('=');
    if (eq <= 0) continue;

    const String key = line.substring(0, eq);
    const String val = line.substring(eq + 1);

    if (key == "TITLE") p.title = val;
    else if (key == "URL") p.url = val;
    else if (key == "DESC") p.description = val;
  }

  f.close();
  outPhoto = p;
  return true;
}

bool PhotoCacheService::promoteToLast(const char* srcImagePath, const char* srcMetaPath, String& outError) {
  IoGuard guard;

  if (!replaceFile(srcImagePath, kLastImagePath, outError)) {
    return false;
  }

  // Metadata is optional.
  if (srcMetaPath && SD.exists(srcMetaPath)) {
    if (!replaceFile(srcMetaPath, kLastMetaPath, outError)) {
      return false;
    }
  }

  return true;
}

// ---------- Helper functions ----------

String PhotoCacheService::getCacheImagePath(uint8_t index) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%s/photo_%03d.jpg", CACHE_DIR, index);
  return String(buf);
}

String PhotoCacheService::getCacheMetaPath(uint8_t index) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%s/photo_%03d.json", CACHE_DIR, index);
  return String(buf);
}

uint32_t PhotoCacheService::calculateCRC32(const char* filePath) {
  // NOTE: Caller must hold IoGuard - this is an internal helper
  
  if (!SD.exists(filePath)) {
    return 0;
  }
  
  File f = SD.open(filePath, FILE_READ);
  if (!f) {
    return 0;
  }
  
  uint32_t crc = 0xFFFFFFFF;
  uint8_t buffer[512];
  
  while (f.available()) {
    size_t bytesRead = f.read(buffer, sizeof(buffer));
    for (size_t i = 0; i < bytesRead; i++) {
      crc = (crc >> 8) ^ crc32Table[(crc ^ buffer[i]) & 0xFF];
    }
  }
  
  f.close();
  return crc ^ 0xFFFFFFFF;
}

bool PhotoCacheService::loadCacheEntry(uint8_t index) {
  if (index >= CACHE_MAX_IMAGES) return false;
  
  IoGuard guard;
  
  String metaPath = getCacheMetaPath(index);
  if (!SD.exists(metaPath.c_str())) {
    cacheEntries[index].valid = false;
    return false;
  }
  
  File f = SD.open(metaPath.c_str(), FILE_READ);
  if (!f) {
    cacheEntries[index].valid = false;
    return false;
  }
  
  // Parse JSON metadata
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  
  if (err) {
    Serial.println("Cache entry JSON parse error: " + String(err.c_str()));
    cacheEntries[index].valid = false;
    return false;
  }
  
  CacheEntry& entry = cacheEntries[index];
  entry.photoInfo.title = doc["title"].as<String>();
  entry.photoInfo.url = doc["url"].as<String>();
  entry.photoInfo.description = doc["description"].as<String>();
  entry.localFilePath = getCacheImagePath(index);
  entry.metaFilePath = metaPath;
  entry.cachedTimestamp = doc["timestamp"].as<uint32_t>();
  entry.accessCount = doc["accessCount"].as<uint32_t>();
  entry.crc32 = doc["crc32"].as<uint32_t>();
  
  // Check if image file exists
  if (!SD.exists(entry.localFilePath.c_str())) {
    entry.valid = false;
    return false;
  }
  
  entry.valid = true;
  return true;
}

bool PhotoCacheService::saveCacheEntry(uint8_t index) {
  if (index >= CACHE_MAX_IMAGES) return false;
  
  IoGuard guard;
  
  const CacheEntry& entry = cacheEntries[index];
  String metaPath = getCacheMetaPath(index);
  
  // Remove existing file
  if (SD.exists(metaPath.c_str())) {
    SD.remove(metaPath.c_str());
  }
  
  File f = SD.open(metaPath.c_str(), FILE_WRITE);
  if (!f) {
    return false;
  }
  
  // Create JSON document
  StaticJsonDocument<512> doc;
  doc["title"] = entry.photoInfo.title;
  doc["url"] = entry.photoInfo.url;
  doc["description"] = entry.photoInfo.description;
  doc["timestamp"] = entry.cachedTimestamp;
  doc["accessCount"] = entry.accessCount;
  doc["crc32"] = entry.crc32;
  
  serializeJson(doc, f);
  f.close();
  
  return true;
}

void PhotoCacheService::loadCacheIndex() {
  IoGuard guard;
  
  if (!SD.exists(kCacheIndexFile)) {
    currentCacheIndex = 0;
    cachedPhotoCount = 0;
    return;
  }
  
  File f = SD.open(kCacheIndexFile, FILE_READ);
  if (!f) {
    currentCacheIndex = 0;
    cachedPhotoCount = 0;
    return;
  }
  
  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  
  if (err) {
    currentCacheIndex = 0;
    cachedPhotoCount = 0;
    return;
  }
  
  currentCacheIndex = doc["currentIndex"].as<uint8_t>();
  cachedPhotoCount = doc["count"].as<uint8_t>();
  
  // Bounds check
  if (currentCacheIndex >= CACHE_MAX_IMAGES) currentCacheIndex = 0;
  if (cachedPhotoCount > CACHE_MAX_IMAGES) cachedPhotoCount = CACHE_MAX_IMAGES;
}

void PhotoCacheService::saveCacheIndex() {
  IoGuard guard;
  
  // Remove existing file
  if (SD.exists(kCacheIndexFile)) {
    SD.remove(kCacheIndexFile);
  }
  
  File f = SD.open(kCacheIndexFile, FILE_WRITE);
  if (!f) {
    Serial.println("Failed to save cache index");
    return;
  }
  
  StaticJsonDocument<128> doc;
  doc["currentIndex"] = currentCacheIndex;
  doc["count"] = cachedPhotoCount;
  
  serializeJson(doc, f);
  f.close();
}

// ---------- Multi-image cache functions ----------

bool PhotoCacheService::initCache() {
  if (cacheInitialized) {
    return true;
  }
  
  // NOTE: Do NOT use IoGuard here - called functions acquire their own guards
  // Using IoGuard here would cause a deadlock with loadCacheIndex/loadCacheEntry
  
  Serial.println("Initializing multi-image cache...");
  
  // Create cache directory if it doesn't exist
  {
    IoGuard guard;
    if (!SD.exists(CACHE_DIR)) {
      if (!SD.mkdir(CACHE_DIR)) {
        Serial.println("Failed to create cache directory");
        cacheInitialized = true;  // Mark as initialized anyway to prevent retry loops
        cachedPhotoCount = 0;
        currentCacheIndex = 0;
        return false;
      }
      Serial.println("Created cache directory");
    }
  }
  
  // Load cache index (currentCacheIndex and count) - has its own IoGuard
  loadCacheIndex();
  Serial.printf("Loaded cache index: count=%d, currentIndex=%d\n", cachedPhotoCount, currentCacheIndex);
  
  // Scan and load existing cache entries - each loadCacheEntry has its own IoGuard
  uint8_t validCount = 0;
  for (uint8_t i = 0; i < CACHE_MAX_IMAGES; i++) {
    Serial.printf("Checking cache slot %d...\n", i);
    if (loadCacheEntry(i)) {
      validCount++;
      Serial.printf("  Loaded: %s\n", cacheEntries[i].photoInfo.title.c_str());
    } else {
      Serial.printf("  Empty or invalid\n");
    }
  }
  
  // Update cached photo count based on what we actually found
  cachedPhotoCount = validCount;
  
  Serial.printf("Cache initialized with %d photos, current index: %d\n", cachedPhotoCount, currentCacheIndex);
  
  cacheInitialized = true;
  return true;
}

bool PhotoCacheService::saveToCache(const PhotoInfo& photo, const char* imagePath) {
  Serial.println("saveToCache: Starting...");
  
  if (!cacheInitialized) {
    Serial.println("saveToCache: Cache not initialized, initializing...");
    if (!initCache()) {
      Serial.println("saveToCache: Failed to initialize cache");
      return false;
    }
  }
  
  Serial.println("saveToCache: Acquiring IoGuard...");
  IoGuard guard;
  Serial.println("saveToCache: IoGuard acquired");
  
  // Determine the next cache slot (circular buffer)
  uint8_t targetIndex = currentCacheIndex;
  Serial.printf("saveToCache: Target slot %d\n", targetIndex);
  
  // Get file paths for the target slot
  String targetImagePath = getCacheImagePath(targetIndex);
  String targetMetaPath = getCacheMetaPath(targetIndex);
  
  // If slot already contains a cached photo, remove old files
  if (cacheEntries[targetIndex].valid) {
    Serial.printf("saveToCache: Evicting cache slot %d\n", targetIndex);
    if (SD.exists(targetImagePath.c_str())) {
      SD.remove(targetImagePath.c_str());
    }
    if (SD.exists(targetMetaPath.c_str())) {
      SD.remove(targetMetaPath.c_str());
    }
  }
  
  // Copy image file to cache location
  Serial.printf("saveToCache: Opening source file %s\n", imagePath);
  File srcFile = SD.open(imagePath, FILE_READ);
  if (!srcFile) {
    Serial.println("saveToCache: Failed to open source image");
    return false;
  }
  
  // Remove target if it somehow exists
  if (SD.exists(targetImagePath.c_str())) {
    SD.remove(targetImagePath.c_str());
  }
  
  Serial.printf("saveToCache: Creating target file %s\n", targetImagePath.c_str());
  File dstFile = SD.open(targetImagePath.c_str(), FILE_WRITE);
  if (!dstFile) {
    srcFile.close();
    Serial.println("saveToCache: Failed to create cache image file");
    return false;
  }
  
  // Copy in chunks and calculate CRC32 inline (avoid nested IoGuard)
  Serial.println("saveToCache: Copying file and calculating CRC...");
  uint8_t buffer[512];
  uint32_t crc = 0xFFFFFFFF;
  size_t totalBytes = 0;
  
  while (srcFile.available()) {
    size_t bytesRead = srcFile.read(buffer, sizeof(buffer));
    dstFile.write(buffer, bytesRead);
    totalBytes += bytesRead;
    
    // Calculate CRC inline
    for (size_t i = 0; i < bytesRead; i++) {
      crc = (crc >> 8) ^ crc32Table[(crc ^ buffer[i]) & 0xFF];
    }
  }
  crc ^= 0xFFFFFFFF;
  
  srcFile.close();
  dstFile.close();
  Serial.printf("saveToCache: Copied %zu bytes, CRC: 0x%08X\n", totalBytes, crc);
  
  // Update cache entry
  CacheEntry& entry = cacheEntries[targetIndex];
  entry.photoInfo = photo;
  entry.localFilePath = targetImagePath;
  entry.metaFilePath = targetMetaPath;
  entry.cachedTimestamp = millis();
  entry.accessCount = 1;
  entry.crc32 = crc;
  entry.valid = true;
  
  // Save metadata JSON inline (avoid nested IoGuard from saveCacheEntry)
  Serial.println("saveToCache: Saving metadata JSON...");
  if (SD.exists(targetMetaPath.c_str())) {
    SD.remove(targetMetaPath.c_str());
  }
  
  File metaFile = SD.open(targetMetaPath.c_str(), FILE_WRITE);
  if (!metaFile) {
    Serial.println("saveToCache: Failed to create metadata file");
    return false;
  }
  
  StaticJsonDocument<512> doc;
  doc["title"] = entry.photoInfo.title;
  doc["url"] = entry.photoInfo.url;
  doc["description"] = entry.photoInfo.description;
  doc["timestamp"] = entry.cachedTimestamp;
  doc["accessCount"] = entry.accessCount;
  doc["crc32"] = entry.crc32;
  
  serializeJson(doc, metaFile);
  metaFile.close();
  Serial.println("saveToCache: Metadata saved");
  
  // Update circular buffer index
  currentCacheIndex = (targetIndex + 1) % CACHE_MAX_IMAGES;
  
  // Update count (only if we added a new photo, not replaced)
  if (cachedPhotoCount < CACHE_MAX_IMAGES) {
    cachedPhotoCount++;
  }
  
  // Save cache index inline (avoid nested IoGuard from saveCacheIndex)
  Serial.println("saveToCache: Saving cache index...");
  if (SD.exists(kCacheIndexFile)) {
    SD.remove(kCacheIndexFile);
  }
  
  File indexFile = SD.open(kCacheIndexFile, FILE_WRITE);
  if (indexFile) {
    StaticJsonDocument<128> indexDoc;
    indexDoc["currentIndex"] = currentCacheIndex;
    indexDoc["count"] = cachedPhotoCount;
    serializeJson(indexDoc, indexFile);
    indexFile.close();
  }
  
  Serial.printf("saveToCache: SUCCESS - slot %d, count: %d, next index: %d\n", 
                targetIndex, cachedPhotoCount, currentCacheIndex);
  
  return true;
}

bool PhotoCacheService::getFromCache(uint8_t cacheIndex, PhotoInfo& outPhoto, String& outImagePath) {
  if (!cacheInitialized) {
    if (!initCache()) {
      return false;
    }
  }
  
  if (cacheIndex >= CACHE_MAX_IMAGES) {
    return false;
  }
  
  const CacheEntry& entry = cacheEntries[cacheIndex];
  if (!entry.valid) {
    return false;
  }
  
  // Check if file still exists (with proper locking)
  {
    IoGuard guard;
    if (!SD.exists(entry.localFilePath.c_str())) {
      cacheEntries[cacheIndex].valid = false;
      return false;
    }
  }
  
  outPhoto = entry.photoInfo;
  outImagePath = entry.localFilePath;
  return true;
}

uint8_t PhotoCacheService::getCachedPhotoCount() {
  if (!cacheInitialized) {
    initCache();
  }
  return cachedPhotoCount;
}

uint8_t PhotoCacheService::getCurrentCacheIndex() {
  if (!cacheInitialized) {
    initCache();
  }
  // Return the index of the most recently cached photo
  // (currentCacheIndex points to next slot to write, so most recent is previous)
  if (cachedPhotoCount == 0) return 0;
  return (currentCacheIndex + CACHE_MAX_IMAGES - 1) % CACHE_MAX_IMAGES;
}

bool PhotoCacheService::validateCacheEntry(uint8_t cacheIndex) {
  if (cacheIndex >= CACHE_MAX_IMAGES) {
    return false;
  }
  
  CacheEntry& entry = cacheEntries[cacheIndex];
  if (!entry.valid) {
    return false;
  }
  
  IoGuard guard;
  
  // Check if file exists
  if (!SD.exists(entry.localFilePath.c_str())) {
    Serial.printf("Cache entry %d: file missing\n", cacheIndex);
    entry.valid = false;
    return false;
  }
  
  // Calculate CRC32 and compare (calculateCRC32 expects caller to hold guard)
  uint32_t currentCRC = calculateCRC32(entry.localFilePath.c_str());
  if (currentCRC != entry.crc32) {
    Serial.printf("Cache entry %d: CRC mismatch (stored: 0x%08X, actual: 0x%08X)\n", 
                  cacheIndex, entry.crc32, currentCRC);
    entry.valid = false;
    return false;
  }
  
  return true;
}

void PhotoCacheService::purgeInvalidEntries() {
  IoGuard guard;
  
  uint8_t purgedCount = 0;
  
  for (uint8_t i = 0; i < CACHE_MAX_IMAGES; i++) {
    CacheEntry& entry = cacheEntries[i];
    if (!entry.valid) continue;
    
    // Remove files for invalid entries
    if (!SD.exists(entry.localFilePath.c_str())) {
      entry.valid = false;
      if (SD.exists(entry.metaFilePath.c_str())) {
        SD.remove(entry.metaFilePath.c_str());
      }
      purgedCount++;
    }
  }
  
  // Recount valid entries
  cachedPhotoCount = 0;
  for (uint8_t i = 0; i < CACHE_MAX_IMAGES; i++) {
    if (cacheEntries[i].valid) {
      cachedPhotoCount++;
    }
  }
  
  if (purgedCount > 0) {
    Serial.printf("Purged %d invalid cache entries, %d remaining\n", purgedCount, cachedPhotoCount);
    saveCacheIndex();
  }
}

const CacheEntry* PhotoCacheService::getCacheEntry(uint8_t cacheIndex) {
  if (cacheIndex >= CACHE_MAX_IMAGES) {
    return nullptr;
  }
  
  if (!cacheEntries[cacheIndex].valid) {
    return nullptr;
  }
  
  return &cacheEntries[cacheIndex];
}

void PhotoCacheService::recordAccess(uint8_t cacheIndex) {
  if (cacheIndex >= CACHE_MAX_IMAGES) return;
  
  CacheEntry& entry = cacheEntries[cacheIndex];
  if (!entry.valid) return;
  
  entry.accessCount++;
  
  // Save updated metadata periodically (every 5 accesses)
  if (entry.accessCount % 5 == 0) {
    saveCacheEntry(cacheIndex);
  }
}

bool PhotoCacheService::hasOfflineContent() {
  if (!cacheInitialized) {
    initCache();
  }
  return cachedPhotoCount > 0;
}

void PhotoCacheService::runMaintenance(uint32_t maxTimeMs) {
  if (!cacheInitialized) return;
  
  uint32_t startTime = millis();
  uint8_t entriesChecked = 0;
  uint8_t entriesInvalidated = 0;
  
  Serial.println("Running cache maintenance...");
  
  // Simple validation: just check file existence (skip CRC for speed during maintenance)
  IoGuard guard;
  
  for (uint8_t i = 0; i < CACHE_MAX_IMAGES && (millis() - startTime) < maxTimeMs; i++) {
    if (!cacheEntries[i].valid) continue;
    
    entriesChecked++;
    
    // Check if file exists
    if (!SD.exists(cacheEntries[i].localFilePath.c_str())) {
      Serial.printf("Cache entry %d: file missing, invalidating\n", i);
      cacheEntries[i].valid = false;
      entriesInvalidated++;
      
      // Remove metadata file if it exists
      String metaPath = getCacheMetaPath(i);
      if (SD.exists(metaPath.c_str())) {
        SD.remove(metaPath.c_str());
      }
    }
  }
  
  // Update cached count if entries were invalidated
  if (entriesInvalidated > 0) {
    cachedPhotoCount = 0;
    for (uint8_t i = 0; i < CACHE_MAX_IMAGES; i++) {
      if (cacheEntries[i].valid) {
        cachedPhotoCount++;
      }
    }
    
    // Save cache index inline (avoid calling saveCacheIndex which has its own guard)
    if (SD.exists(kCacheIndexFile)) {
      SD.remove(kCacheIndexFile);
    }
    
    File indexFile = SD.open(kCacheIndexFile, FILE_WRITE);
    if (indexFile) {
      StaticJsonDocument<128> indexDoc;
      indexDoc["currentIndex"] = currentCacheIndex;
      indexDoc["count"] = cachedPhotoCount;
      serializeJson(indexDoc, indexFile);
      indexFile.close();
    }
  }
  
  Serial.printf("Maintenance complete: checked %d, invalidated %d, valid count: %d\n",
                entriesChecked, entriesInvalidated, cachedPhotoCount);
}
