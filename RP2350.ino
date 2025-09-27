#include "pio_usb.h"
#include "Adafruit_TinyUSB.h"
#include "tusb.h"

// ================================================================
// スティックキャリブレーション設定
// ================================================================
const int STICK_LX_MIN = 300, STICK_LX_NEUTRAL = 1841, STICK_LX_MAX = 3450;
const int STICK_LY_MIN = 420, STICK_LY_NEUTRAL = 2055, STICK_LY_MAX = 3780;
const int STICK_RX_MIN = 460, STICK_RX_NEUTRAL = 2075, STICK_RX_MAX = 3580;
const int STICK_RY_MIN = 320, STICK_RY_NEUTRAL = 1877, STICK_RY_MAX = 3550;

// [ここが重要] スティックのデッドゾーン設定
// ニュートラルを中心とした無反応範囲の半径をコントローラーの生の値(0-4095)で指定します。
// 150は、ニュートラルから上下左右に約7%の範囲を無反応にします。(150 / (2048-200))
// この値を大きくするとデッドゾーンが広がり、小さくすると狭まります。
const int STICK_DEADZONE_RADIUS = 100;

// ================================================================
// 通信プロトコル定義 (変更なし)
// ================================================================
struct ControllerData {
  uint16_t buttons;
  uint8_t dpad;
  uint8_t stick_lx, stick_ly, stick_rx, stick_ry;
  uint8_t crc;
};
#define BTN_A       (1 << 0)
#define BTN_B       (1 << 1)
#define BTN_X       (1 << 2)
#define BTN_Y       (1 << 3)
#define BTN_L       (1 << 4)
#define BTN_R       (1 << 5)
#define BTN_ZL      (1 << 6)
#define BTN_ZR      (1 << 7)
#define BTN_MINUS   (1 << 8)
#define BTN_PLUS    (1 << 9)
#define BTN_L_STICK (1 << 10)
#define BTN_R_STICK (1 << 11)
#define BTN_HOME    (1 << 12)
#define BTN_CAPTURE (1 << 13)
#define DPAD_UP    (1 << 0)
#define DPAD_DOWN  (1 << 1)
#define DPAD_LEFT  (1 << 2)
#define DPAD_RIGHT (1 << 3)

// ================================================================
// Proコントローラーの定義と初期化 (変更なし)
// ================================================================
#define HOST_PIN_DP 12
Adafruit_USBH_Host USBHost;
#include <Adafruit_NeoPixel.h>
#define DIN_PIN 16
Adafruit_NeoPixel pixels(1, DIN_PIN, NEO_RGB + NEO_KHZ800);
#define VID_NINTENDO 0x057e
#define PID_SWITCH_PRO 0x2009

enum class InitState { HANDSHAKE, DISABLE_TIMEOUT, LED, LED_HOME, FULL_REPORT, DONE };
InitState init_state = InitState::HANDSHAKE;
uint8_t procon_addr = 0, procon_instance = 0, seq_counter = 0;
bool is_procon = false;
struct OutputReport { uint8_t command, sequence_counter, rumble_l[4], rumble_r[4], sub_command, sub_command_args[8]; };
OutputReport out_report;

// ================================================================
// スティックの値変換関数 (デッドゾーン対応版)
// ================================================================
uint8_t map_stick_axis(int value, int min_in, int neutral_in, int max_in) {
  // 1. 値がデッドゾーンの範囲内かチェック
  if (abs(value - neutral_in) < STICK_DEADZONE_RADIUS) {
    return 128; // 範囲内ならニュートラル(128)を返す
  }

  // 2. デッドゾーン外の値をマッピング
  value = constrain(value, min_in, max_in);
  if (value < neutral_in) {
    // 最小値から、デッドゾーンの手前までを 0-127 にマッピング
    return map(value, min_in, neutral_in - STICK_DEADZONE_RADIUS, 0, 127);
  } else {
    // デッドゾーンの先から、最大値までを 129-255 にマッピング
    return map(value, neutral_in + STICK_DEADZONE_RADIUS, max_in, 129, 255);
  }
}

// (以下、以前の安定版コードから変更なし)

void send_report(uint8_t size) {
  out_report.sequence_counter = seq_counter++ & 0x0F;
  tuh_hid_send_report(procon_addr, procon_instance, 0, (uint8_t*)&out_report, size);
}

void advance_init() {
  memset(&out_report, 0, sizeof(out_report));
  uint8_t report_size = 10;
  out_report.rumble_l[0]=0; out_report.rumble_l[1]=1; out_report.rumble_l[2]=0x40; out_report.rumble_l[3]=0x40;
  memcpy(out_report.rumble_r, out_report.rumble_l, 4);
  switch (init_state) {
    case InitState::HANDSHAKE:
      report_size = 2; out_report.command = 0x80; out_report.sequence_counter = 0x02;
      if (tuh_hid_send_report(procon_addr, procon_instance, 0, (uint8_t*)&out_report, report_size)) init_state = InitState::DISABLE_TIMEOUT;
      break;
    case InitState::DISABLE_TIMEOUT:
      report_size = 2; out_report.command = 0x80; out_report.sequence_counter = 0x04;
      if (tuh_hid_send_report(procon_addr, procon_instance, 0, (uint8_t*)&out_report, report_size)) init_state = InitState::LED;
      break;
    case InitState::LED:
      report_size = 12; out_report.command = 0x01; out_report.sub_command = 0x30; out_report.sub_command_args[0] = 0x01;
      send_report(report_size); init_state = InitState::LED_HOME;
      break;
    case InitState::LED_HOME:
      report_size = 14; out_report.command = 0x01; out_report.sub_command = 0x38; out_report.sub_command_args[0] = 0x0F; out_report.sub_command_args[1] = 0xF0; out_report.sub_command_args[2] = 0xF0;
      send_report(report_size); init_state = InitState::FULL_REPORT;
      break;
    case InitState::FULL_REPORT:
      report_size = 12; out_report.command = 0x01; out_report.sub_command = 0x03; out_report.sub_command_args[0] = 0x30;
      send_report(report_size); init_state = InitState::DONE;
      pixels.setPixelColor(0, pixels.Color(0, 15, 0)); pixels.show();
      break;
    default: break;
  }
}

uint8_t crc8(const uint8_t *data, int len) {
  uint8_t crc = 0;
  while (len--) { crc ^= *data++; for (int i = 0; i < 8; i++) crc = crc & 0x80 ? (crc << 1) ^ 0x31 : crc << 1; }
  return crc;
}

void send_cobs_packet(const uint8_t *data, size_t length) {
  uint8_t encoded_buffer[length + 2];
  size_t read_index = 0, write_index = 1, code_index = 0;
  uint8_t code = 1;
  while (read_index < length) {
    if (data[read_index] == 0) {
      encoded_buffer[code_index] = code; code = 1; code_index = write_index++; read_index++;
    } else {
      encoded_buffer[write_index++] = data[read_index++]; code++;
      if (code == 0xFF) { encoded_buffer[code_index] = code; code = 1; code_index = write_index++; }
    }
  }
  encoded_buffer[code_index] = code;
  Serial1.write(encoded_buffer, write_index);
  Serial1.write((uint8_t)0x00);
}

void process_and_send_report(uint8_t const* report) {
  ControllerData data;
  uint8_t r = report[3], m = report[4], l = report[5];
  data.buttons=0; if(r&1)data.buttons|=BTN_Y; if(r&2)data.buttons|=BTN_X; if(r&4)data.buttons|=BTN_B; if(r&8)data.buttons|=BTN_A; if(r&0x40)data.buttons|=BTN_R; if(r&0x80)data.buttons|=BTN_ZR; if(l&0x40)data.buttons|=BTN_L; if(l&0x80)data.buttons|=BTN_ZL; if(m&1)data.buttons|=BTN_MINUS; if(m&2)data.buttons|=BTN_PLUS; if(m&4)data.buttons|=BTN_R_STICK; if(m&8)data.buttons|=BTN_L_STICK; if(m&0x10)data.buttons|=BTN_HOME; if(m&0x20)data.buttons|=BTN_CAPTURE;
  data.dpad=0; if(l&1)data.dpad|=DPAD_DOWN; if(l&2)data.dpad|=DPAD_UP; if(l&4)data.dpad|=DPAD_RIGHT; if(l&8)data.dpad|=DPAD_LEFT;
  
  data.stick_lx = map_stick_axis(report[6]|((report[7]&0xF)<<8), STICK_LX_MIN, STICK_LX_NEUTRAL, STICK_LX_MAX);
  data.stick_ly = map_stick_axis((report[7]>>4)|(report[8]<<4), STICK_LY_MIN, STICK_LY_NEUTRAL, STICK_LY_MAX);
  data.stick_rx = map_stick_axis(report[9]|((report[10]&0xF)<<8), STICK_RX_MIN, STICK_RX_NEUTRAL, STICK_RX_MAX);
  data.stick_ry = map_stick_axis((report[10]>>4)|(report[11]<<4), STICK_RY_MIN, STICK_RY_NEUTRAL, STICK_RY_MAX);
  
  data.crc = 0;
  data.crc = crc8((uint8_t*)&data, sizeof(data));
  send_cobs_packet((uint8_t*)&data, sizeof(data));
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc, uint16_t len) {
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);
  if (vid == VID_NINTENDO && pid == PID_SWITCH_PRO) {
    procon_addr = dev_addr; procon_instance = instance; is_procon = true; init_state = InitState::HANDSHAKE;
    pixels.setPixelColor(0, pixels.Color(20, 20, 0)); pixels.show();
    advance_init();
  }
  tuh_hid_receive_report(dev_addr, instance);
}
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  if (is_procon && dev_addr == procon_addr) {
    if (init_state != InitState::DONE) advance_init();
    else if (len >= 12) process_and_send_report(report);
  }
  tuh_hid_receive_report(dev_addr, instance);
}
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  if (dev_addr == procon_addr) { is_procon = false; procon_addr = 0; pixels.setPixelColor(0, pixels.Color(20, 0, 0)); pixels.show(); }
}
void setup() {}
void loop() {}
void setup1() {
  Serial1.begin(115200);
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(0, 0, 15)); pixels.show();
  delay(500);
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = HOST_PIN_DP;
  USBHost.configure_pio_usb(1, &pio_cfg);
  USBHost.begin(1);
}
void loop1() { USBHost.task(); }
