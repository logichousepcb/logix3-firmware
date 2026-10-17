#pragma once

// Small LittleFS helpers for fixed-layout settings structs. Each struct must be packed
// and start with `uint32_t magic` and end with `uint32_t checksum`.

#include <Arduino.h>
#include <LittleFS.h>

namespace SettingsFile {

// FNV-1a over everything before the trailing checksum field.
template <typename T>
uint32_t checksum(const T& s) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&s);
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < offsetof(T, checksum); i++) {
    hash ^= p[i];
    hash *= 16777619u;
  }
  return hash;
}

template <typename T>
bool read(File& f, T& out, uint32_t magic) {
  f.seek(0);
  return f.size() == sizeof(T) && f.read(reinterpret_cast<uint8_t*>(&out), sizeof(T)) == sizeof(T) &&
         out.magic == magic && out.checksum == checksum(out);
}

template <typename T>
bool write(const char* path, T& s, uint32_t magic) {
  s.magic = magic;
  s.checksum = checksum(s);

  String tmpPath = String(path) + ".tmp";
  File f = LittleFS.open(tmpPath, "w");
  if (!f) return false;
  bool ok = f.write(reinterpret_cast<const uint8_t*>(&s), sizeof(s)) == sizeof(s);
  f.close();
  // Write-then-rename so a power cut never leaves a half-written settings file.
  if (!ok || !LittleFS.rename(tmpPath, path)) {
    LittleFS.remove(tmpPath);
    return false;
  }
  return true;
}

}  // namespace SettingsFile
