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

#include "preset_storage.h"

namespace braids {
using namespace stmlib;

PresetStorage preset_storage;

// From stmlib/system/page_storage.h
static uint16_t Checksum(uint16_t checksum, const void *data, size_t size) {
  uint16_t s = checksum;
  const uint8_t* d = static_cast<const uint8_t*>(data);
  while (size--) {
    s += *d++;
  }
  return s;
}

STATIC_ASSERT(96 == sizeof(SettingsData), settings_data_size);
STATIC_ASSERT(100 == sizeof(PresetSlot), preset_data_size);
STATIC_ASSERT(0 == (sizeof(PresetData) & 0x3), preset_data_alignment);
STATIC_ASSERT(0 == (sizeof(PresetDataPage) & 0x1), preset_data_page_alignment);
STATIC_ASSERT(0x400 == PAGE_SIZE, page_size_1024);
STATIC_ASSERT(sizeof(PresetDataPage) < PAGE_SIZE, preset_data_page_size);
STATIC_ASSERT(PresetStorage::kNumPages > 1, preset_storage_num_pages);

class FlashWriteStream {
public:
  FlashWriteStream(uint32_t address)
  : address_(address), checksum_(0) { }
  ~FlashWriteStream() { }

  void Append(const void *data, size_t length);
  void Finalize(uint32_t magic, uint32_t generation);

private:
  uint32_t address_;
  uint16_t checksum_;
};

void FlashWriteStream::Append(const void *data, size_t length) {
  uint32_t address = address_;
  const uint32_t end = address + length;
  const uint32_t *src = (const uint32_t *)data;
  while (address < end) {
    FLASH_ProgramWord(address, *src++);
    address += 4;
  }

  address_ = address;
  checksum_ = Checksum(checksum_, data, length);
}

void FlashWriteStream::Finalize(uint32_t magic, uint32_t generation) {
  FLASH_ProgramWord(address_, magic);
  FLASH_ProgramWord(address_ + 4, generation);
  FLASH_ProgramHalfWord(address_ + 8, checksum_);
}

void PresetStorage::Init() {

  // Scan storage and find the best valid page. If no valid pages are found,
  // erase the flash and write a bank of defaults as defined in braids::Settings.

  page_index_ = 0;
  uint32_t generation = 0;
  for (int16_t page_index = 0; page_index < kNumPages; ++page_index) {
    const PresetDataPage *page = (const PresetDataPage *)page_start(page_index);

    if (kMagic != page->magic) continue;
    if (page->checksum != Checksum(0, page, sizeof(PresetData))) continue;
    if (generation && page->generation < generation) continue;

    page_index_ = page_index;
    generation = page->generation;
  }

  if (!generation)
    Reset();
}

void PresetStorage::Save(uint16_t slot, const SettingsData *settings_data, const char *name) {

  uint16_t source_page = page_index_;
  uint16_t target_page = (source_page + 1) % kNumPages;
  uint32_t target_address = page_start(target_page);
  FLASH_Unlock();
  FLASH_ErasePage(target_address);

  const PresetDataPage *page = current_page();

  FlashWriteStream flash_writer(target_address);
  if (slot > 0)
    flash_writer.Append(&page->preset_data.slots[0], slot * sizeof(PresetSlot));

  flash_writer.Append(name, 4);
  flash_writer.Append(settings_data, sizeof(SettingsData));

  if (slot < kNumPresetSlots - 1)
    flash_writer.Append(&page->preset_data.slots[slot + 1], (kNumPresetSlots - 1 - slot) * sizeof(PresetSlot));
  flash_writer.Finalize(kMagic, page->generation + 1);

  page_index_ = target_page;
}

void PresetStorage::Load(uint16_t slot, SettingsData *settings_data, bool load_calibration) {
  int32_t pitch_cv_offset = settings_data->pitch_cv_offset;
  int32_t pitch_cv_scale = settings_data->pitch_cv_scale;
  int32_t fm_cv_offset = settings_data->fm_cv_offset;

  const PresetDataPage *page = current_page();

  memcpy(settings_data, &page->preset_data.slots[slot].settings_data, sizeof(SettingsData));

  if (!load_calibration) {
    settings_data->pitch_cv_offset = pitch_cv_offset;
    settings_data->pitch_cv_scale = pitch_cv_scale;
    settings_data->fm_cv_offset = fm_cv_offset;
  }
}

void PresetStorage::Reset() {
  FLASH_Unlock();
  for (size_t i = 0; i < kNumPages; ++i)
    FLASH_ErasePage(page_start(i));

  FlashWriteStream flash_writer(page_start(0));
  for (size_t i = 0; i < kNumPresetSlots; ++i) {
    flash_writer.Append("\0\0\0\0", 4);
    flash_writer.Append(&kInitSettings, sizeof(SettingsData));
  }
  flash_writer.Finalize(kMagic, 1);

  page_index_ = 0;
}

};
