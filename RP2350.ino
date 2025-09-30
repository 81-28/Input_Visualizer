#include "pio_usb.h"
#include "Adafruit_TinyUSB.h"
#include "tusb.h"
#include <SPI.h>

// ================================================================
// SPI通信設定
// ================================================================
#define SPI_CS_PIN 1    // CS signal to ATMega32U4 Pin 7
#define SPI_SCK_PIN 2   // SPI SCK  -> ATMega32U4 Pin 15
#define SPI_MOSI_PIN 3  // SPI MOSI -> ATMega32U4 Pin 16
#define SPI_MISO_PIN 0  // SPI MISO (unused)
#define SPI_SPEED 10000 // 10kHz

// ================================================================
// スティックキャリブレーション設定
// ================================================================
constexpr int STICK_LX_MIN = 300, STICK_LX_NEUTRAL = 1841, STICK_LX_MAX = 3450;
constexpr int STICK_LY_MIN = 420, STICK_LY_NEUTRAL = 2055, STICK_LY_MAX = 3780;
constexpr int STICK_RX_MIN = 460, STICK_RX_NEUTRAL = 2075, STICK_RX_MAX = 3580;
constexpr int STICK_RY_MIN = 320, STICK_RY_NEUTRAL = 1877, STICK_RY_MAX = 3550;

// スティックのデッドゾーン設定
// ニュートラルを中心とした無反応範囲の半径をコントローラーの生の値(0-4095)で指定
constexpr int STICK_DEADZONE_RADIUS = 150;

// ================================================================
// 通信プロトコル定義
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
// Proコントローラーの定義と初期化
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
// 接続監視用変数
// ================================================================
// 接続時間ログ設定（分単位）
constexpr unsigned long CONNECTION_LOG_INTERVAL_MINUTES = 15;  // ログ出力間隔（分）
constexpr unsigned long CONNECTION_LOG_INTERVAL = CONNECTION_LOG_INTERVAL_MINUTES * 60 * 1000;  // ミリ秒に変換
constexpr unsigned long CONNECTION_TIMEOUT = 3000;  // 3秒でタイムアウト
constexpr int SHORT_REPORT_THRESHOLD = 3;  // 3回連続で切断判定
constexpr int SEND_FAILURE_THRESHOLD = 3;  // 送信失敗閾値
bool enable_connection_log = true;  // 接続時間ログの有効/無効

unsigned long last_data_time = 0;
bool device_physically_connected = false;
int send_report_failures = 0;
int short_report_count = 0;
bool controller_disconnected = false;
unsigned long last_connection_log_time = 0;

// ================================================================
// スティックの値変換関数 (デッドゾーン対応版)
// ================================================================
uint8_t map_stick_axis(int value, int min_in, int neutral_in, int max_in, uint8_t newtral = 128) {
  // 1. 値がデッドゾーンの範囲内かチェック
  if (abs(value - neutral_in) < STICK_DEADZONE_RADIUS) {
    return newtral; // 範囲内ならニュートラルを返す
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

void send_report(uint8_t size) {
  out_report.sequence_counter = seq_counter++ & 0x0F;
  bool success = tuh_hid_send_report(procon_addr, procon_instance, 0, (uint8_t*)&out_report, size);
  
  if (!success) {
    send_report_failures++;
    if (send_report_failures > SEND_FAILURE_THRESHOLD) {
      if (Serial && Serial.availableForWrite() > 0) {
        Serial.println("[" + getTimeString() + "] Pro Controller disconnected (send failures)");
        Serial.println("To reconnect:");
        Serial.println("1. Reconnect Pro Controller USB cable");
        Serial.println("2. Reconnect this device to PC");
      }
      controller_disconnected = true;
      device_physically_connected = false;
    }
  } else {
    send_report_failures = 0;  // 成功したらカウンタリセット
  }
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
      if (Serial && Serial.availableForWrite() > 0) {
        Serial.println("[" + getTimeString() + "] Pro Controller initialization completed");
      }
      pixels.setPixelColor(0, pixels.Color(0, 15, 0)); pixels.show();
      break;
    default: break;
  }
}

// ================================================================
// 時刻表示関数
// ================================================================
String getTimeString() {
  unsigned long ms = millis();
  unsigned long seconds = ms / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  ms %= 1000;
  seconds %= 60;
  minutes %= 60;
  hours %= 24;
  
  return String(hours) + "h" + String(minutes) + "m" + String(seconds) + "s" + String(ms) + "ms";
}

// ================================================================
// 状態リセット関数
// ================================================================
void reset_controller_state() {
  is_procon = false;
  procon_addr = 0;
  procon_instance = 0;
  init_state = InitState::HANDSHAKE;
  seq_counter = 0;
  last_data_time = 0;
  device_physically_connected = false;
  send_report_failures = 0;
  short_report_count = 0;
  last_connection_log_time = 0;
}

uint8_t crc8(const uint8_t *data, int len) {
  uint8_t crc = 0;
  while (len--) { crc ^= *data++; for (int i = 0; i < 8; i++) crc = crc & 0x80 ? (crc << 1) ^ 0x31 : crc << 1; }
  return crc;
}

void send_spi_packet(const uint8_t *data, size_t length) {
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(SPI_CS_PIN, LOW);
  delayMicroseconds(100);
  
  for (size_t i = 0; i < length; i++) {
    SPI.transfer(data[i]);
    delayMicroseconds(10);
  }
  
  delayMicroseconds(100);
  digitalWrite(SPI_CS_PIN, HIGH);
  SPI.endTransaction();
}

void process_and_send_report(uint8_t const* report) {
  ControllerData data;
  uint8_t r = report[3], m = report[4], l = report[5];
  data.buttons=0; if(r&1)data.buttons|=BTN_Y; if(r&2)data.buttons|=BTN_X; if(r&4)data.buttons|=BTN_B; if(r&8)data.buttons|=BTN_A; if(r&0x40)data.buttons|=BTN_R; if(r&0x80)data.buttons|=BTN_ZR; if(l&0x40)data.buttons|=BTN_L; if(l&0x80)data.buttons|=BTN_ZL; if(m&1)data.buttons|=BTN_MINUS; if(m&2)data.buttons|=BTN_PLUS; if(m&4)data.buttons|=BTN_R_STICK; if(m&8)data.buttons|=BTN_L_STICK; if(m&0x10)data.buttons|=BTN_HOME; if(m&0x20)data.buttons|=BTN_CAPTURE;
  data.dpad=0; if(l&1)data.dpad|=DPAD_DOWN; if(l&2)data.dpad|=DPAD_UP; if(l&4)data.dpad|=DPAD_RIGHT; if(l&8)data.dpad|=DPAD_LEFT;
  
  data.stick_lx = map_stick_axis(report[6]|((report[7]&0xF)<<8), STICK_LX_MIN, STICK_LX_NEUTRAL, STICK_LX_MAX);
  data.stick_ly = 255 - map_stick_axis((report[7]>>4)|(report[8]<<4), STICK_LY_MIN, STICK_LY_NEUTRAL, STICK_LY_MAX, 127);
  data.stick_rx = map_stick_axis(report[9]|((report[10]&0xF)<<8), STICK_RX_MIN, STICK_RX_NEUTRAL, STICK_RX_MAX);
  data.stick_ry = 255 - map_stick_axis((report[10]>>4)|(report[11]<<4), STICK_RY_MIN, STICK_RY_NEUTRAL, STICK_RY_MAX, 127);

  data.crc = 0;
  data.crc = crc8((uint8_t*)&data, sizeof(data));
  send_spi_packet((uint8_t*)&data, sizeof(data));
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc, uint16_t len) {
  uint16_t vid, pid;
  tuh_vid_pid_get(dev_addr, &vid, &pid);
  
  if (vid == VID_NINTENDO && pid == PID_SWITCH_PRO) {
    // 切断状態の場合は新しい接続を拒否
    if (controller_disconnected) {
      if (Serial && Serial.availableForWrite() > 0) {
        Serial.println("[" + getTimeString() + "] Controller connection blocked - restart required");
      }
      tuh_hid_receive_report(dev_addr, instance);
      return;
    }
    
    procon_addr = dev_addr; procon_instance = instance; is_procon = true;
    init_state = InitState::HANDSHAKE;
    last_data_time = millis();
    device_physically_connected = true;
    last_connection_log_time = millis();  // ログ時刻を初期化
    if (Serial && Serial.availableForWrite() > 0) {
      Serial.println("[" + getTimeString() + "] Pro Controller connected");
    }
    pixels.setPixelColor(0, pixels.Color(15, 15, 0)); pixels.show();
    advance_init();
  }
  
  tuh_hid_receive_report(dev_addr, instance);
}
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
  // 切断状態の場合は処理を停止
  if (controller_disconnected) {
    tuh_hid_receive_report(dev_addr, instance);
    return;
  }
  
  if (is_procon && dev_addr == procon_addr) {
    // レポートの長さで切断を判定
    if (len < 12) {  // 正常なプロコンレポートは12バイト以上
      short_report_count++;
      
      // 短いレポートが連続した場合、切断と判定
      if (short_report_count >= SHORT_REPORT_THRESHOLD) {
        if (Serial && Serial.availableForWrite() > 0) {
          Serial.println("[" + getTimeString() + "] Pro Controller disconnected");
          Serial.println("To reconnect:");
          Serial.println("1. Reconnect Pro Controller USB cable");
          Serial.println("2. Reconnect this device to PC");
        }
        controller_disconnected = true;
        device_physically_connected = false;
        is_procon = false;
        pixels.setPixelColor(0, pixels.Color(15, 0, 0)); pixels.show();
        tuh_hid_receive_report(dev_addr, instance);
        return;
      }
    } else {
      // 正常なレポートを受信したらカウンタをリセット
      short_report_count = 0;
      last_data_time = millis();
      
      if (init_state != InitState::DONE) {
        advance_init();
      } else {
        process_and_send_report(report);
      }
    }
  }
  tuh_hid_receive_report(dev_addr, instance);
}
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  if (dev_addr == procon_addr) {
    if (Serial && Serial.availableForWrite() > 0) {
      Serial.println("[" + getTimeString() + "] Pro Controller disconnected");
      Serial.println("To reconnect:");
      Serial.println("1. Reconnect Pro Controller USB cable");
      Serial.println("2. Reconnect this device to PC");
    }
    controller_disconnected = true;
    device_physically_connected = false;
    is_procon = false;
    pixels.setPixelColor(0, pixels.Color(15, 0, 0)); pixels.show();
  }
}
// ================================================================
// 接続状態チェック関数
// ================================================================
void check_connection_status() {
  static unsigned long last_check = 0;
  static bool timeout_reported = false;
  
  // 1秒ごとにチェック
  if (millis() - last_check < 1000) return;
  last_check = millis();
  
  if (is_procon && device_physically_connected) {
    unsigned long time_since_last_data = millis() - last_data_time;
    
    // 定期的な接続状態ログ出力
    if (enable_connection_log && (millis() - last_connection_log_time >= CONNECTION_LOG_INTERVAL)) {
      if (Serial && Serial.availableForWrite() > 0) {
        Serial.println("[" + getTimeString() + "] Pro Controller still connected");
      }
      last_connection_log_time = millis();
    }
    
    // 送信失敗が多い場合は即座に切断判定
    if (send_report_failures > SEND_FAILURE_THRESHOLD) {
      if (Serial && Serial.availableForWrite() > 0) {
        Serial.println("[" + getTimeString() + "] *** TOO MANY SEND FAILURES - CONTROLLER DISCONNECTED ***");
      }
      device_physically_connected = false;
      reset_controller_state();
      pixels.setPixelColor(0, pixels.Color(15, 0, 0)); pixels.show();
      return;
    }
    
    // 補助的なタイムアウトチェック（レポート長検知が主）
    if (time_since_last_data > 15000 && !timeout_reported) {
      timeout_reported = true;
      if (Serial && Serial.availableForWrite() > 0) {
        Serial.println("[" + getTimeString() + "] Pro Controller disconnected (timeout)");
        Serial.println("To reconnect:");
        Serial.println("1. Reconnect Pro Controller USB cable");
        Serial.println("2. Reconnect this device to PC");
      }
      controller_disconnected = true;
      device_physically_connected = false;
      
      reset_controller_state();
      pixels.setPixelColor(0, pixels.Color(0, 0, 15)); pixels.show(); // 待機状態に戻す
    }
  }
}

void setup() {}
void loop() {}
void setup1() {
  // シリアル通信初期化
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // 最大3秒待機
  if (Serial && Serial.availableForWrite() > 0) {
    Serial.println("[" + getTimeString() + "] System started - Waiting for Pro Controller...");
  }
  
  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(0, 0, 15)); pixels.show(); // 待機中は青
  delay(500);
  
  // SPI初期化
  SPI.setSCK(SPI_SCK_PIN);
  SPI.setTX(SPI_MOSI_PIN);
  SPI.setRX(SPI_MISO_PIN);
  SPI.begin();
  pinMode(SPI_CS_PIN, OUTPUT);
  digitalWrite(SPI_CS_PIN, HIGH);
  
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = HOST_PIN_DP;
  USBHost.configure_pio_usb(1, &pio_cfg);
  USBHost.begin(1);
}
void loop1() { 
  USBHost.task();
  
  // 切断状態の場合は処理を停止
  if (controller_disconnected) {
    return;
  }
  
  // 正常時は接続状態をチェック
  check_connection_status();
}
