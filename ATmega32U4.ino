// ================================================================
// Proコントローラー to Switch 変換コード
// ================================================================

#include <NintendoSwitchControlLibrary.h>

// 両方ともLOWで点灯
#define LED_RX 17
#define LED_TX 30

// SPI通信設定
#define SS_PIN   7    // CS signal from RP2350 Pin 1
#define MISO_PIN 14   // SPI MISO (unused)
#define SCK_PIN  15   // SPI SCK  <- RP2350 Pin 2
#define MOSI_PIN 16   // SPI MOSI <- RP2350 Pin 3

// --- 通信プロトコル定義 (RP2350側と完全に一致) ---
struct ControllerData {
  uint16_t buttons;
  uint8_t dpad;
  uint8_t stick_lx, stick_ly, stick_rx, stick_ry;
  uint8_t crc;
};

// --- ボタンのビットマスク定義 ---
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

// 現在と前回のコントローラー状態を保存する変数
ControllerData controller_input;
ControllerData previous_input;

// SPI受信用バッファ
uint8_t spi_buffer[sizeof(ControllerData)];
volatile bool data_received = false;
uint8_t crc8(const uint8_t *data, int len) {
  uint8_t crc = 0;
  while(len--){crc^=*data++;for(int i=0;i<8;i++)crc=crc&0x80?(crc<<1)^0x31:crc<<1;}
  return crc;
}

void setup() {
  Serial1.begin(115200);
  pushButton(Button::L, 20, 2);
  pushButton(Button::R, 20, 2);
  
  // SPI pin setup
  // pinMode(MISO_PIN, OUTPUT);
  pinMode(MOSI_PIN, INPUT);
  pinMode(SCK_PIN, INPUT);
  pinMode(SS_PIN, INPUT_PULLUP);
  SPCR = 0; // Disable hardware SPI
  attachInterrupt(digitalPinToInterrupt(SS_PIN), onCSFalling, FALLING);
  
  memset(&controller_input, 0, sizeof(controller_input));
  controller_input.stick_lx = 128;
  controller_input.stick_ly = 128;
  controller_input.stick_rx = 128;
  controller_input.stick_ry = 128;
  memcpy(&previous_input, &controller_input, sizeof(controller_input));
}

void loop() {
  if (data_received) {
    process_received_data();
    data_received = false;
  }
  update_switch_state();
  SwitchControlLibrary().sendReport();
  memcpy(&previous_input, &controller_input, sizeof(controller_input));
  delay(16);
}

// ================================================================
// メインの変換処理
// ================================================================
void update_switch_state() {
  // --- スティックの更新 ---
  // Y軸反転はRP2350.ino側で処理済み
  SwitchControlLibrary().moveLeftStick(controller_input.stick_lx, controller_input.stick_ly);
  SwitchControlLibrary().moveRightStick(controller_input.stick_rx, controller_input.stick_ry);

  // 各ボタンを個別で判定し、ライブラリの関数を直接呼び出す
  // Y / B / A / X ボタン
  if ((controller_input.buttons & BTN_Y) && !(previous_input.buttons & BTN_Y)) SwitchControlLibrary().pressButton(Button::Y);
  else if (!(controller_input.buttons & BTN_Y) && (previous_input.buttons & BTN_Y)) SwitchControlLibrary().releaseButton(Button::Y);
  if ((controller_input.buttons & BTN_B) && !(previous_input.buttons & BTN_B)) SwitchControlLibrary().pressButton(Button::B);
  else if (!(controller_input.buttons & BTN_B) && (previous_input.buttons & BTN_B)) SwitchControlLibrary().releaseButton(Button::B);
  if ((controller_input.buttons & BTN_A) && !(previous_input.buttons & BTN_A)) SwitchControlLibrary().pressButton(Button::A);
  else if (!(controller_input.buttons & BTN_A) && (previous_input.buttons & BTN_A)) SwitchControlLibrary().releaseButton(Button::A);
  if ((controller_input.buttons & BTN_X) && !(previous_input.buttons & BTN_X)) SwitchControlLibrary().pressButton(Button::X);
  else if (!(controller_input.buttons & BTN_X) && (previous_input.buttons & BTN_X)) SwitchControlLibrary().releaseButton(Button::X);
  // L / R ボタン
  if ((controller_input.buttons & BTN_L) && !(previous_input.buttons & BTN_L)) SwitchControlLibrary().pressButton(Button::L);
  else if (!(controller_input.buttons & BTN_L) && (previous_input.buttons & BTN_L)) SwitchControlLibrary().releaseButton(Button::L);
  if ((controller_input.buttons & BTN_R) && !(previous_input.buttons & BTN_R)) SwitchControlLibrary().pressButton(Button::R);
  else if (!(controller_input.buttons & BTN_R) && (previous_input.buttons & BTN_R)) SwitchControlLibrary().releaseButton(Button::R);
  // ZL / ZR ボタン
  if ((controller_input.buttons & BTN_ZL) && !(previous_input.buttons & BTN_ZL)) SwitchControlLibrary().pressButton(Button::ZL);
  else if (!(controller_input.buttons & BTN_ZL) && (previous_input.buttons & BTN_ZL)) SwitchControlLibrary().releaseButton(Button::ZL);
  if ((controller_input.buttons & BTN_ZR) && !(previous_input.buttons & BTN_ZR)) SwitchControlLibrary().pressButton(Button::ZR);
  else if (!(controller_input.buttons & BTN_ZR) && (previous_input.buttons & BTN_ZR)) SwitchControlLibrary().releaseButton(Button::ZR);
  // MINUS / PLUS ボタン
  if ((controller_input.buttons & BTN_MINUS) && !(previous_input.buttons & BTN_MINUS)) SwitchControlLibrary().pressButton(Button::MINUS);
  else if (!(controller_input.buttons & BTN_MINUS) && (previous_input.buttons & BTN_MINUS)) SwitchControlLibrary().releaseButton(Button::MINUS);
  if ((controller_input.buttons & BTN_PLUS) && !(previous_input.buttons & BTN_PLUS)) SwitchControlLibrary().pressButton(Button::PLUS);
  else if (!(controller_input.buttons & BTN_PLUS) && (previous_input.buttons & BTN_PLUS)) SwitchControlLibrary().releaseButton(Button::PLUS);
  // LCLICK / RCLICK (スティック押し込み)
  if ((controller_input.buttons & BTN_L_STICK) && !(previous_input.buttons & BTN_L_STICK)) SwitchControlLibrary().pressButton(Button::LCLICK);
  else if (!(controller_input.buttons & BTN_L_STICK) && (previous_input.buttons & BTN_L_STICK)) SwitchControlLibrary().releaseButton(Button::LCLICK);
  if ((controller_input.buttons & BTN_R_STICK) && !(previous_input.buttons & BTN_R_STICK)) SwitchControlLibrary().pressButton(Button::RCLICK);
  else if (!(controller_input.buttons & BTN_R_STICK) && (previous_input.buttons & BTN_R_STICK)) SwitchControlLibrary().releaseButton(Button::RCLICK);
  // HOME / CAPTURE ボタン
  if ((controller_input.buttons & BTN_HOME) && !(previous_input.buttons & BTN_HOME)) SwitchControlLibrary().pressButton(Button::HOME);
  else if (!(controller_input.buttons & BTN_HOME) && (previous_input.buttons & BTN_HOME)) SwitchControlLibrary().releaseButton(Button::HOME);
  if ((controller_input.buttons & BTN_CAPTURE) && !(previous_input.buttons & BTN_CAPTURE)) SwitchControlLibrary().pressButton(Button::CAPTURE);
  else if (!(controller_input.buttons & BTN_CAPTURE) && (previous_input.buttons & BTN_CAPTURE)) SwitchControlLibrary().releaseButton(Button::CAPTURE);

  // --- D-Pad(十字キー)の更新 ---
  if (controller_input.dpad != previous_input.dpad) {
    bool up = controller_input.dpad & DPAD_UP;
    bool down = controller_input.dpad & DPAD_DOWN;
    bool left = controller_input.dpad & DPAD_LEFT;
    bool right = controller_input.dpad & DPAD_RIGHT;

    // 変数に入れず、直接Hatの値を指定する
    if (up && left) SwitchControlLibrary().pressHatButton(Hat::UP_LEFT);
    else if (up && right) SwitchControlLibrary().pressHatButton(Hat::UP_RIGHT);
    else if (down && left) SwitchControlLibrary().pressHatButton(Hat::DOWN_LEFT);
    else if (down && right) SwitchControlLibrary().pressHatButton(Hat::DOWN_RIGHT);
    else if (up) SwitchControlLibrary().pressHatButton(Hat::UP);
    else if (down) SwitchControlLibrary().pressHatButton(Hat::DOWN);
    else if (left) SwitchControlLibrary().pressHatButton(Hat::LEFT);
    else if (right) SwitchControlLibrary().pressHatButton(Hat::RIGHT);
    else SwitchControlLibrary().releaseHatButton();
  }
}

// SPI割り込み処理
void onCSFalling() {
  size_t byte_index = 0;
  
  while (digitalRead(SS_PIN) == LOW && byte_index < sizeof(ControllerData)) {
    spi_buffer[byte_index] = receiveByte();
    if (digitalRead(SS_PIN) == LOW) {
      byte_index++;
    }
  }
  
  if (byte_index == sizeof(ControllerData)) {
    data_received = true;
  }
}

uint8_t receiveByte() {
  uint8_t byte_data = 0;
  
  // 各ビットを受信 (MSB first)
  for (int bit = 7; bit >= 0; bit--) {
    unsigned long timeout = micros() + 10000; // 10ms per bit
    
    // クロック立ち上がり待ち
    while (digitalRead(SCK_PIN) == LOW && digitalRead(SS_PIN) == LOW && micros() < timeout);
    
    if (digitalRead(SS_PIN) == LOW && micros() < timeout) {
      delayMicroseconds(10);
      if (digitalRead(MOSI_PIN)) byte_data |= (1 << bit);
      
      // クロック立ち下がり待ち
      timeout = micros() + 10000;
      while (digitalRead(SCK_PIN) == HIGH && digitalRead(SS_PIN) == LOW && micros() < timeout);
    } else {
      break;
    }
  }
  
  return byte_data;
}

void process_received_data() {
  ControllerData received_data;
  memcpy(&received_data, spi_buffer, sizeof(ControllerData));
  
  // CRC確認
  uint8_t received_crc = received_data.crc;
  received_data.crc = 0;
  uint8_t calculated_crc = crc8((uint8_t*)&received_data, sizeof(received_data));
  
  if (received_crc == calculated_crc) {
    memcpy(&controller_input, &received_data, sizeof(ControllerData));
    
    // CRCを復元してからCOBS形式でPCに送信
    controller_input.crc = received_crc;
    send_cobs_packet((uint8_t*)&controller_input, sizeof(controller_input));
  }
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
