#pragma once

#include <Arduino.h>

#include "services/PhotoTypes.h"
#include "services/SDCardService.h"

class ImagePipeline {
public:
  ImagePipeline(SDCardService& sd);

  // Downloads, renders, and updates the persistent "last" cache.
  // useQuality=true uses a slower, higher-quality EPD mode for the final image.
  bool renderPhoto(const PhotoInfo& photo, bool useQuality, String& outError);

  // Renders an already-downloaded prefetched image and promotes it to "last".
  bool renderPrefetchedPhoto(const PhotoInfo& photo, bool useQuality, String& outError);

  // Renders an existing image from SD (does not download or promote).
  bool renderLocalPhoto(const PhotoInfo& photo, const char* imagePath, bool useQuality, String& outError);

private:
  bool downloadToSdFile(const String& url, const char* path, size_t bufferSize, String& outError);
  bool renderJpegFromSdFile(const char* path, String& outError);
  void saveMetadata(const PhotoInfo& photo);

  SDCardService& _sd;
};
