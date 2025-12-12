#pragma once

#include <Arduino.h>

#include "services/PhotoTypes.h"

class PhotoCacheService {
public:
  static constexpr const char* kPhotosDir = "/PHOTOS";

  // Persistent (survives reboot)
  static constexpr const char* kLastImagePath = "/PHOTOS/last.jpg";
  static constexpr const char* kLastMetaPath  = "/PHOTOS/last.meta";

  // Working files
  static constexpr const char* kStagingImagePath = "/PHOTOS/staging.jpg";
  static constexpr const char* kPrefetchImagePath = "/PHOTOS/prefetch.jpg";
  static constexpr const char* kPrefetchMetaPath  = "/PHOTOS/prefetch.meta";

  // Reads PhotoInfo from a simple key/value text file. Returns false if missing or unreadable.
  static bool loadMeta(const char* metaPath, PhotoInfo& outPhoto);

  // Writes PhotoInfo to a key/value text file.
  static bool saveMeta(const char* metaPath, const PhotoInfo& photo);

  // Promotes a downloaded image (and optional metadata) to be the new last image.
  // This is done by remove(dst) + rename(src->dst) to avoid corrupting the old last image.
  static bool promoteToLast(const char* srcImagePath, const char* srcMetaPath, String& outError);
};
