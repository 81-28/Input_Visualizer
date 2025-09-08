#include "pio_usb.h"
#include "Adafruit_TinyUSB.h"
#include "tusb.h"
#include <stdio.h>
#include "hardware/resets.h"


#define HOST_PIN_DP 12  // PIO USB D+ ピン

Adafruit_USBH_Host USBHost;

// NeoPixel (デバッグ用LED)
#include <Adafruit_NeoPixel.h>
#define DIN_PIN 16
#define LED_COUNT 1
Adafruit_NeoPixel pixels(LED_COUNT, DIN_PIN, NEO_RGB + NEO_KHZ800);

#define VID_NINTENDO 0x057e
#define PID_SWITCH_PRO 0x2009

// ====== Switch Pro Controller Init State Machine ======

namespace SwitchPro {
  static constexpr uint8_t INFO_CONN_MASK = 0xAB;
  static constexpr uint8_t INFO_BATTERY_MASK = 0x0F;

  namespace CMD {
    static constexpr uint8_t HID = 0x80;
    static constexpr uint8_t RUMBLE_ONLY = 0x10;
    static constexpr uint8_t AND_RUMBLE = 0x01;
    static constexpr uint8_t LED = 0x30;
    static constexpr uint8_t LED_HOME = 0x38;
    static constexpr uint8_t GYRO = 0x40;
    static constexpr uint8_t MODE = 0x03;
    static constexpr uint8_t FULL_REPORT_MODE = 0x30;
    static constexpr uint8_t HANDSHAKE = 0x02;
    static constexpr uint8_t DISABLE_TIMEOUT = 0x04;
  }

  namespace Buttons0 {
    static constexpr uint8_t Y = 0x01;
    static constexpr uint8_t X = 0x02;
    static constexpr uint8_t B = 0x04;
    static constexpr uint8_t A = 0x08;
    static constexpr uint8_t R = 0x40;
    static constexpr uint8_t ZR = 0x80;
  }
}

enum class InitState {
  HANDSHAKE,
  DISABLE_TIMEOUT,
  LED,
  LED_HOME,
  FULL_REPORT,
  IMU,
  DONE
};

InitState init_state = InitState::HANDSHAKE;
uint8_t seq_counter = 0;
uint8_t procon_addr = 0, procon_instance = 0;
bool is_procon = false;

struct OutputReport {
  uint8_t command;
  uint8_t sequence_counter;
  uint8_t rumble_l[4];
  uint8_t rumble_r[4];
  uint8_t sub_command;
  uint8_t sub_command_args[8];
} out_report;

// 送信ラッパ
void send_report(uint8_t size) {
  out_report.sequence_counter = seq_counter++ & 0x0F;
  if (!tuh_hid_send_report(procon_addr, procon_instance, 0, &out_report, size)) {
    Serial1.println("send_report failed");
  }
}

// 次の初期化ステップを進める
void advance_init() {
  memset(&out_report, 0, sizeof(out_report));
  uint8_t report_size = 10;

  switch (init_state) {
    case InitState::HANDSHAKE:
      report_size = 2;

      out_report.command = SwitchPro::CMD::HID;
      out_report.sequence_counter = SwitchPro::CMD::HANDSHAKE;

      if (tuh_hid_send_report(procon_addr, procon_instance, 0, &out_report, report_size)) {
        init_state = InitState::DISABLE_TIMEOUT;
      }
      break;

    case InitState::DISABLE_TIMEOUT:
      report_size = 2;

      out_report.command = SwitchPro::CMD::HID;
      out_report.sequence_counter = SwitchPro::CMD::DISABLE_TIMEOUT;

      if (tuh_hid_send_report(procon_addr, procon_instance, 0, &out_report, report_size)) {
        init_state = InitState::LED;
      }
      break;

    case InitState::LED:
      report_size = 12;

      out_report.command = SwitchPro::CMD::AND_RUMBLE;
      out_report.sub_command = SwitchPro::CMD::LED;
      out_report.sub_command_args[0] = 0x01; // Player 1
      send_report(report_size);
      init_state = InitState::LED_HOME;
      break;

    case InitState::LED_HOME:
      Serial1.println("led home");
      out_report.command = 0x01;
      out_report.sub_command = 0x38; // Home LED
      out_report.sub_command_args[0] = 0x0F;
      out_report.sub_command_args[1] = 0xF0;
      out_report.sub_command_args[2] = 0xF0;
      send_report(14);
      init_state = InitState::FULL_REPORT;
      break;

    case InitState::FULL_REPORT:
      Serial1.println("full report");
      out_report.command = 0x01;
      out_report.sub_command = 0x03; // Input report mode
      out_report.sub_command_args[0] = 0x30; // Full report mode
      send_report(12);
      init_state = InitState::IMU;
      break;

    case InitState::IMU:
      Serial1.println("imu");
      out_report.command = 0x01;
      out_report.sub_command = 0x40; // Enable IMU
      out_report.sub_command_args[0] = 0x01; // 1=enable
      send_report(12);
      init_state = InitState::DONE;
      break;

    default:
      break;
  }
}

// ====== TinyUSB Callbacks ======

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const* desc_report, uint16_t desc_len) {
  uint16_t vid, pid;
  if (tuh_vid_pid_get(dev_addr, &vid, &pid)) {
    Serial1.printf("VID:%04X PID:%04X\r\n", vid, pid);

    if (vid == VID_NINTENDO && pid == PID_SWITCH_PRO) {
      Serial1.println("Switch Pro Controller detected!");
      procon_addr = dev_addr;
      procon_instance = instance;
      is_procon = true;
      init_state = InitState::HANDSHAKE;
      advance_init();  // 初期化開始
    }
  }
  tuh_hid_receive_report(dev_addr, instance); // 最初の受信開始
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const* report, uint16_t len) {
  if (len == 0) return;

  // Serial1.print("HID Report: ");
  // for (uint16_t i = 0; i < len; i++) {
  //   Serial1.printf("%02X ", report[i]);
  // }
  // Serial1.println();

  const uint8_t start_byte = 0xAA;
  const uint8_t end_byte = 0xBB;

  Serial1.write(start_byte);
  Serial1.write((uint8_t)len);
  Serial1.write(report, len);
  Serial1.write(end_byte);

  // 初期化シーケンスを進める
  if (is_procon && init_state != InitState::DONE) {
    advance_init();
  }

  // 常に次のレポートを要求
  tuh_hid_receive_report(dev_addr, instance);
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  Serial1.printf("HID unmounted: addr=%d instance=%d\n", dev_addr, instance);

  // 切断されたデバイスが、現在接続されているProコントローラーか確認
  if (dev_addr == procon_addr && instance == procon_instance) {
    Serial1.println("Pro Controller disconnected. Resetting state.");
    is_procon = false;
    procon_addr = 0;
    procon_instance = 0;
    init_state = InitState::HANDSHAKE; // 初期化状態を最初に戻す
    seq_counter = 0;                   // シーケンスカウンターもリセット
  }
}

void send_keepalive() {
  memset(&out_report, 0, sizeof(out_report));
  out_report.command = 0x10;  // Rumble only
  out_report.sequence_counter = seq_counter++ & 0x0F;

  out_report.rumble_l[0] = 0x00;
  out_report.rumble_l[1] = 0x01;
  out_report.rumble_l[2] = 0x40;
  out_report.rumble_l[3] = 0x40;
  out_report.rumble_r[0] = 0x00;
  out_report.rumble_r[1] = 0x01;
  out_report.rumble_r[2] = 0x40;
  out_report.rumble_r[3] = 0x40;

  tuh_hid_send_report(procon_addr, procon_instance, 0, &out_report, 10);
}

// ====== Hardware Timer ======
bool repeating_keep_alive_cb(struct repeating_timer *t) {
  if (is_procon && init_state == InitState::DONE) {
    send_keepalive();
  }
  return true;
}
struct repeating_timer timer;


// ====== Core 0: main logic ======

void setup() {

}

void loop() {

}

// ====== Core1: Process USB Host ======

void init_usb_host() {
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = HOST_PIN_DP;
  USBHost.configure_pio_usb(1, &pio_cfg);
  USBHost.begin(1);
  Serial1.println("USB host init done");
}

void setup1() {
  // UARTでシリアル通信開始
  Serial1.begin(115200);
  pixels.begin();

  // 青色LED 
  pixels.setPixelColor(0, pixels.Color(0, 0, 10));

  // 電源等の安定待ち (これがないとProコンがコネクションを確立できない)
  delay(500);
  pixels.show();
  init_usb_host();

  // ハードウェアタイマー初期化
  // param1: 実行間隔 (μs), -16666 -> 60Hz
  // param2: コールバック関数
  // param3: コールバック関数の引数 (nullptr)
  // param4: タイマー構造体のポインタ
  // add_repeating_timer_us(-16666, repeating_keep_alive_cb, nullptr, &timer);
}

// これ以外の処理は走らせない!!! 接続が不安定になる原因となり得る!!!
void loop1() {
  USBHost.task();
}
