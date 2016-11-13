// Copyright 2016 Patrick Dowling
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Flash-based preset storage
// To avoid keeping all the presets (kNumPresetSlots * size of SettingsData) in
// memory, a read-only pointer to the data in flash is used. This means the nice
// mechanisms from stmlib::Storage aren't used, but similarly implemented. The
// current implementation is slightly wasteful and doesn't pack versions into
// pages, but stores each update into a new page.
//
// The existing settings data is untouched, the presets are located in separate
// pages (below them).

#ifndef BRAIDS_PRESET_STORAGE_H_
#define BRAIDS_PRESET_STORAGE_H_

#include "settings.h"
#include "stmlib/system/storage.h"

namespace braids {

static const uint16_t kNumPresetSlots = 10;

struct PresetSlot {
  char name[4];
  SettingsData settings_data;
};

struct PresetData {
  PresetSlot slots[kNumPresetSlots];
};

// For convenient access in flash
struct PresetDataPage {
  PresetData preset_data;
  uint32_t magic;
  uint32_t generation;
  uint16_t checksum;
  uint16_t padding;
};

class PresetStorage {
public:

  static const uint16_t kNumPages = 4;
  static const uint32_t FLASH_STORAGE_BASE = 0x08004000 - kNumPages * PAGE_SIZE; // 0x08004000 == end of bootloader flash
  static const uint32_t kMagic = stmlib::FourCC<'B','R','D','1'>::value;

  PresetStorage() { }
  ~PresetStorage() { }

  void Init();
  void Save(uint16_t slot, const SettingsData *settings_data, const char *name);
  void Load(uint16_t slot, SettingsData *settings_data, bool load_calibration);

  const char *preset_name(uint16_t slot) const {
    // note: name not 0-terminated
    return current_page()->preset_data.slots[slot].name;
  }

private:

  uint16_t page_index_;

  void Reset();

  static inline uint32_t page_start(uint16_t index) {
    return FLASH_STORAGE_BASE + index * PAGE_SIZE;
  }

  inline const PresetDataPage *current_page() const {
    return (const PresetDataPage *)page_start(page_index_);
  }

  DISALLOW_COPY_AND_ASSIGN(PresetStorage);
};

extern PresetStorage preset_storage;

};

#endif // BRAIDS_PRESET_STORAGE_H_
