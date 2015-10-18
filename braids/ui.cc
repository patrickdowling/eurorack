// Copyright 2012 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
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
// User interface.

#include "braids/ui.h"
#include "braids/cv_scaler.h"

#include <cstring>

#include "stmlib/system/system_clock.h"

namespace braids {

using namespace stmlib;

const uint32_t kEncoderLongPressTime = 800;

void Ui::Init(CvScaler *cv_scaler) {
  display_.Init();
  display_.set_brightness(settings.GetValue(SETTING_BRIGHTNESS) + 1);
  encoder_.Init();
  leds_.Init();
  switches_.Init();
  queue_.Init();
  cv_scaler_ = cv_scaler;
  sub_clock_ = 0;
  refresh_display_ = true;
  mode_ = MODE_SPLASH;
  setting_ = SETTING_OSCILLATOR_SHAPE;
  setting_index_ = 0;
  cv_index_ = 0;
  gate_ = false;
  gate_led_time_ = 0;
  pot_mode_ = POTMODE_NORMAL;
}

void Ui::Poll() {
  system_clock.Tick();  // Tick global ms counter.
  ++sub_clock_;
  encoder_.Debounce();
  if (encoder_.just_pressed()) {
    encoder_press_time_ = system_clock.milliseconds();
    inhibit_further_switch_events_ = false;
  } 
  if (!inhibit_further_switch_events_) {
    if (encoder_.pressed()) {
      uint32_t duration = system_clock.milliseconds() - encoder_press_time_;
      if (duration >= kEncoderLongPressTime) {
        queue_.AddEvent(CONTROL_ENCODER_LONG_CLICK, 0, 0);
        inhibit_further_switch_events_ = true;
      }
    } else if (encoder_.released()) {
      queue_.AddEvent(CONTROL_ENCODER_CLICK, 0, 0);
    }
  }
  int32_t increment = encoder_.increment();
  if (increment != 0) {
    queue_.AddEvent(CONTROL_ENCODER, 0, increment);
  }

  switches_.Debounce();
  for (size_t i = 0; i < kNumSwitches; ++i) {
    if (switches_.just_pressed(i)) {
      queue_.AddEvent(CONTROL_SWITCH, i, 0);
      switch_press_time_[i] = system_clock.milliseconds();
    }
    if (switches_.released(i)) {
      queue_.AddEvent(CONTROL_SWITCH, i,
                      system_clock.milliseconds() - switch_press_time_[i] + 1);
    }
  }

  if ((sub_clock_ & 1) == 0) {
    display_.Refresh();
  }
  if (gate_led_time_)
    --gate_led_time_;

  leds_.set_gate(gate_ || gate_led_time_);
  leds_.set_status(mode_ != MODE_EDIT || setting_ != SETTING_OSCILLATOR_SHAPE);
  leds_.Write();
}

struct PotSettingMapping {
  Potentiometer pot;
  Setting setting;
  float range; // Most settings are 0-based anyway
};

static const PotSettingMapping PotMappings[POT_LAST] =
{
  { POT_FINE, SETTING_AD_ATTACK, 16.f },
  { POT_PITCH, SETTING_AD_DECAY, 16.f },
  { POT_FM, SETTING_AD_FM, 16.f },
  { POT_PARAM1, SETTING_AD_TIMBRE, 16.f },
  { POT_MOD, SETTING_AD_VCA, 1.f },
  { POT_PARAM2, SETTING_AD_COLOR, 16.f }
};

void Ui::PollPots() {
  if (POTMODE_AD == pot_mode_) {

    // TODO This make editing with the encoder impossible since the value is just overwriten
    for (size_t s = 0; s < POT_LAST; ++s) {
      const PotSettingMapping &mapping = PotMappings[s];
      const float value = cv_scaler_->ReadLockedPot(mapping.pot) * mapping.range;
      settings.SafeSetValue(mapping.setting, static_cast<int16_t>(value + .5f));
      if (setting_ == mapping.setting && mode_ == MODE_EDIT)
        refresh_display_ = true;
    }
  }
}

void Ui::FlushEvents() {
  queue_.Flush();
}

void Ui::RefreshDisplay() {
  uint16_t decimal_hex = 0; // TODO dirty ;)
  switch (mode_) {
    case MODE_SPLASH:
      {
        char text[] = "    ";
        text[0] = '\x98' + (splash_frame_ & 0x7);
        display_.Print(text);
      }
      break;
    
    case MODE_EDIT:
      {
        uint8_t value = settings.GetValue(setting_);
        if (setting_ == SETTING_OSCILLATOR_SHAPE &&
            settings.meta_modulation()) {
          value = meta_shape_;
        }
        display_.Print(settings.metadata(setting_).strings[value]);
        if (pot_mode_)
          decimal_hex = 0x1 << (3 - pot_mode_); // LTR
      }
      break;
      
    case MODE_MENU:
      {
        if (setting_ == SETTING_CV_TESTER) {
          char text[] = "    ";
          if (!blink_) {
            for (uint8_t i = 0; i < kDisplayWidth; ++i) {
              text[i] = '\x90' + ((cv_scaler_->cv_value(i) >> 4) * 7 >> 12);
            }
          }
          display_.Print(text);
        } else if (setting_ == SETTING_CV_DEBUG) {
          if (!blink_) {
            if (cv_index_ < CV_ADC_CHANNEL_LAST)
              PrintDebugHex(cv_scaler_->cv_value(cv_index_));
            else
              PrintDebugHex(cv_scaler_->pot_value(cv_index_ - CV_ADC_CHANNEL_LAST));
            decimal_hex = cv_index_ + 1;
          }
        } else if (setting_ == SETTING_MARQUEE) {
          uint8_t length = strlen(settings.marquee_text());
          uint8_t padded_length = length + 2 * kDisplayWidth - 4;
          uint8_t position = ((cv_scaler_->cv_value(0) >> 4 >> 4) * (padded_length - 1)) >> 8;
          position += (marquee_step_ % padded_length);
          position += 1;
          char text[] = "    ";
          for (uint8_t i = 0; i < kDisplayWidth; ++i) {
            uint8_t index = (position + i) % padded_length;
            if (index >= kDisplayWidth && index < kDisplayWidth + length) {
              text[i] = settings.marquee_text()[index - kDisplayWidth];
            }
          }
          display_.Print(text);
        } else {
          display_.Print(settings.metadata(setting_).name);
        }
      }
      break;

    case MODE_CALIBRATION_STEP_1:
      display_.Print(">POT");
      break;

    case MODE_CALIBRATION_STEP_2:
      display_.Print(">C2 "); // 1V
      break;

    case MODE_CALIBRATION_STEP_3:
      display_.Print(">C4 "); // 3V
      break;

    case MODE_MARQUEE_EDITOR:
      {
        char text[] = "    ";
        for (uint8_t i = 0; i < kDisplayWidth; ++i) {
          if ((marquee_character_ + i) >= kDisplayWidth - 1) {
            const char* marquee_text = settings.marquee_text();
            text[i] = marquee_text[marquee_character_ + i - kDisplayWidth + 1];
          }
        }
        display_.Print(text);
      }
      break;

    default:
      break;
  }
  display_.set_decimal_hex(decimal_hex);
}

void Ui::OnLongClick() {
  switch (mode_) {
    case MODE_MARQUEE_EDITOR:
      settings.SaveState();
      mode_ = MODE_MENU;
      break;

    case MODE_MENU:
      if (setting_ == SETTING_CALIBRATION) {
        mode_ = MODE_CALIBRATION_STEP_1;
      } else if (setting_ == SETTING_MARQUEE) {
        mode_ = MODE_MARQUEE_EDITOR;
        marquee_character_ = 0;
        marquee_dirty_character_ = false;
        uint8_t length = strlen(settings.marquee_text());
        settings.mutable_marquee_text()[length] = '\xA0';
      } else if (setting_ == SETTING_VERSION) {
        settings.Reset();
        settings.SaveState();
      }
      break;
    
    default:
      break;
  }
}

void Ui::OnClick() {
  switch (mode_) {
    case MODE_EDIT:
      mode_ = MODE_MENU;
      break;
      
    case MODE_MARQUEE_EDITOR:
      {
        if (marquee_character_ == 61) {
          ++marquee_character_;
          settings.mutable_marquee_text()[marquee_character_] = '\0';
        } else if (settings.marquee_text()[marquee_character_] == '\xA0') {
          settings.mutable_marquee_text()[marquee_character_] = '\0';
        } else {
          if (marquee_dirty_character_) {
            settings.mutable_marquee_text()[marquee_character_ + 1] = \
                settings.marquee_text()[marquee_character_];
          }
          ++marquee_character_;
          marquee_dirty_character_ = false;
        }
        if (settings.marquee_text()[marquee_character_] == '\0') {
          settings.SaveState();
          mode_ = MODE_MENU;
        }
      }
      break;
      
    case MODE_MENU:
      if (setting_ <= SETTING_LAST_EDITABLE_SETTING) {
        mode_ = MODE_EDIT;
        if (setting_ == SETTING_OSCILLATOR_SHAPE) {
          settings.SaveState();
        }
      } else if (setting_ == SETTING_VERSION) {
        mode_ = MODE_SPLASH;
      } else if (setting_ == SETTING_CV_TESTER) {
        // just shows CV values
        cv_index_ = 0;
      } else if (setting_ == SETTING_CV_DEBUG) {
        cv_index_ = (cv_index_ + 1) % (CV_ADC_CHANNEL_LAST + POT_LAST);
      }
      break;

    case MODE_CALIBRATION_STEP_1:
      cv_scaler_->CalibrateOffsets();
      mode_ = MODE_CALIBRATION_STEP_2;
      break;
      
    case MODE_CALIBRATION_STEP_2:
      cv_scaler_->Calibrate1V();
      mode_ = MODE_CALIBRATION_STEP_3;
      break;

    case MODE_CALIBRATION_STEP_3:
      if (cv_scaler_->Calibrate3V()) {
        settings.SaveCalibration();
        mode_ = MODE_MENU;
      } else {
        mode_ = MODE_CALIBRATION_STEP_1;
      }
      break;

    default:
      break;
  }
}

void Ui::OnIncrement(const Event& e) {
  switch (mode_) {
    case MODE_MARQUEE_EDITOR:
      {
        char c = settings.marquee_text()[marquee_character_];
        c += e.data;
        if (c <= ' ') {
          c = ' ';
        } else if (c >= '\xA0') {
          c = '\xA0';
        }
        settings.mutable_marquee_text()[marquee_character_] = c;
        marquee_dirty_character_ = true;
      }
      break;

    case MODE_EDIT:
      {
        int16_t value = settings.GetValue(setting_);
        value = settings.metadata(setting_).Clip(value + e.data);
        settings.SetValue(setting_, value);
        display_.set_brightness(settings.GetValue(SETTING_BRIGHTNESS) + 1);
      }
      break;
      
    case MODE_MENU:
      {
        setting_index_ += e.data;
        if (setting_index_ < 0) {
          setting_index_ = 0;
        } else if (setting_index_ >= SETTING_LAST) {
          setting_index_ = SETTING_LAST - 1;
        }
        setting_ = settings.setting_at_index(setting_index_);
        marquee_step_ = 0;
        cv_index_ = 0;
      }
      break;
      
    default:
      break;
  }
}

static const uint16_t LockedPotMask[POTMODE_LAST] = {
  0, // POTMODE_NORMAL
  0x3f // POTMODE_AD
};

/*
static int32_t setting_to_pot(Setting setting) {
  int32_t value = settings.GetValue(setting);
  const SettingMetadata &metadata = settings.metadata(setting);
  int32_t range = metadata.max - metadata.min + 1;

  return (value - metadata.min) * 65536 / range;
}
*/
void Ui::OnSwitchPressed(const stmlib::Event &e) {
  switch (static_cast<SwitchIndex>(e.control_id)) {
    case SWITCH_S1:
      // a bit brute forcey
      if (POTMODE_NORMAL == pot_mode_) {
        pot_mode_ = POTMODE_AD;
        int32_t values[POT_LAST];
        for (size_t s = 0; s < POT_LAST; ++s ) {
          const PotSettingMapping &mapping = PotMappings[s];
          const float value = static_cast<float>(settings.GetValue(mapping.setting)) / mapping.range * 65536.f;
          values[mapping.pot] = static_cast<int32_t>(value + 0.f);            
        }

        cv_scaler_->LockPots(LockedPotMask[POTMODE_AD], values);
      } else {
        pot_mode_ = POTMODE_NORMAL;
        cv_scaler_->UnlockPots();
      }
      break;
    case SWITCH_GATE:
      gate_ = true;
      break;
    default: break;
  }
}

void Ui::OnSwitchReleased(const stmlib::Event &e) {
  switch (static_cast<SwitchIndex>(e.control_id)) {
    case SWITCH_S1: break;
    case SWITCH_GATE:
      gate_ = false;
      break;
  }
}

void Ui::DoEvents() {
  while (queue_.available()) {
    Event e = queue_.PullEvent();
    if (e.control_type == CONTROL_ENCODER_CLICK) {
      OnClick();
    } else if (e.control_type == CONTROL_ENCODER_LONG_CLICK) {
      OnLongClick();
    } else if (e.control_type == CONTROL_ENCODER) {
      OnIncrement(e);
    } else if (e.control_type == CONTROL_SWITCH) {
      if (e.data == 0) {
        OnSwitchPressed(e);
      } else {
        OnSwitchReleased(e);
      }
    }
    refresh_display_ = true;
  }
  if (queue_.idle_time() > 1000) {
    refresh_display_ = true;
  }
  if (queue_.idle_time() >= 50 && mode_ == MODE_SPLASH) {
    ++splash_frame_;
    if (splash_frame_ == 8) {
      splash_frame_ = 0;
      mode_ = MODE_EDIT;
      setting_ = SETTING_OSCILLATOR_SHAPE;
    }
    refresh_display_ = true;
  }
  if (queue_.idle_time() >= 50 &&
      (setting_ == SETTING_CV_TESTER ||
      setting_ == SETTING_CV_DEBUG ||
      setting_ == SETTING_MARQUEE)) {
    refresh_display_ = true;
  }
  if (queue_.idle_time() >= 50 &&
      setting_ == SETTING_OSCILLATOR_SHAPE &&
      mode_ == MODE_EDIT &&
      settings.meta_modulation()) {
    refresh_display_ = true;
  }
  if (refresh_display_) {
    queue_.Touch();
    RefreshDisplay();
    blink_ = false;
    refresh_display_ = false;
  }
}

}  // namespace braids
