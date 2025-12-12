#include "services/PhotoCacheService.h"

#include <SD.h>

#include "services/IoMutex.h"

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

} // namespace

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
