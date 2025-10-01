// ================================================================
// 簡素化されたProコントローラー to Switch 変換コード
// RP2350からのSPIデータをそのままUSBで転送し、PCからの再生データをSwitchに出力
// 最適化版：再生中もPC側に状態を送信、遅延を削減
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

// 再生モード制御（PC側からの指示による）
volatile bool playback_mode = false;

// プリロード関連
ControllerData preloaded_frame;
volatile bool frame_preloaded = false;

// UART受信関連
#define UART_BUFFER_SIZE 64
uint8_t uart_buffer[UART_BUFFER_SIZE];
volatile int uart_buffer_index = 0;

// プロトコル定義（簡素化）
#define CMD_PLAYBACK_START 0xA0
#define CMD_PLAYBACK_DATA 0xA1
#define CMD_PLAYBACK_STOP 0xA2
#define CMD_PRELOAD_FRAME 0xA3      // プリロード専用コマンド
#define CMD_DATA_PACKET 0xB0        // PC側へのデータ送信

uint8_t crc8(const uint8_t *data, int len) {
  uint8_t crc = 0;
  while(len--){crc^=*data++;for(int i=0;i<8;i++)crc=crc&0x80?(crc<<1)^0x31:crc<<1;}
  return crc;
}

void setup() {
  Serial1.begin(115200);
  pushButton(Button::L, 20, 2);
  pushButton(Button::R, 20, 2);
  
  // LED設定
  pinMode(LED_RX, OUTPUT);
  pinMode(LED_TX, OUTPUT);
  digitalWrite(LED_RX, HIGH); // 消灯（LOWで点灯）
  digitalWrite(LED_TX, HIGH); // 消灯
  
  // SPI pin setup
  pinMode(MOSI_PIN, INPUT);
  pinMode(SCK_PIN, INPUT);
  pinMode(SS_PIN, INPUT_PULLUP);
  SPCR = 0; // Disable hardware SPI
  attachInterrupt(digitalPinToInterrupt(SS_PIN), onCSFalling, FALLING);
  
  // 初期状態設定
  memset(&controller_input, 0, sizeof(controller_input));
  controller_input.stick_lx = 128;
  controller_input.stick_ly = 128;
  controller_input.stick_rx = 128;
  controller_input.stick_ry = 128;
  memcpy(&previous_input, &controller_input, sizeof(controller_input));
}

void loop() {
  // UART受信チェック（優先度高）
  check_uart_data();
  
  // SPIデータ受信チェック
  if (data_received && !playback_mode) {
    // 通常モード時のみSPIデータを処理
    process_received_data();
    data_received = false;
  }
  
  // LED制御（再生モード時にLED_RXを点灯）
  digitalWrite(LED_RX, playback_mode ? LOW : HIGH);
  
  // Switchへの出力
  update_switch_state();
  SwitchControlLibrary().sendReport();
  
  // PC側への状態送信（再生中は頻度を下げる）
  static uint8_t send_counter = 0;
  if (!playback_mode || (++send_counter % 3 == 0)) {
    send_data_to_pc((uint8_t*)&controller_input, sizeof(controller_input));
  }
  
  memcpy(&previous_input, &controller_input, sizeof(controller_input));
  
  // 遅延を最適化（再生時はより高速に）
  delay(playback_mode ? 1 : 3);
}

// ================================================================
// メインの変換処理
// ================================================================
void update_switch_state() {
  // --- スティックの更新 ---
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
  // + / - ボタン
  if ((controller_input.buttons & BTN_PLUS) && !(previous_input.buttons & BTN_PLUS)) SwitchControlLibrary().pressButton(Button::PLUS);
  else if (!(controller_input.buttons & BTN_PLUS) && (previous_input.buttons & BTN_PLUS)) SwitchControlLibrary().releaseButton(Button::PLUS);
  if ((controller_input.buttons & BTN_MINUS) && !(previous_input.buttons & BTN_MINUS)) SwitchControlLibrary().pressButton(Button::MINUS);
  else if (!(controller_input.buttons & BTN_MINUS) && (previous_input.buttons & BTN_MINUS)) SwitchControlLibrary().releaseButton(Button::MINUS);
  // HOME / CAPTURE ボタン
  if ((controller_input.buttons & BTN_HOME) && !(previous_input.buttons & BTN_HOME)) SwitchControlLibrary().pressButton(Button::HOME);
  else if (!(controller_input.buttons & BTN_HOME) && (previous_input.buttons & BTN_HOME)) SwitchControlLibrary().releaseButton(Button::HOME);
  if ((controller_input.buttons & BTN_CAPTURE) && !(previous_input.buttons & BTN_CAPTURE)) SwitchControlLibrary().pressButton(Button::CAPTURE);
  else if (!(controller_input.buttons & BTN_CAPTURE) && (previous_input.buttons & BTN_CAPTURE)) SwitchControlLibrary().releaseButton(Button::CAPTURE);
  // Stick ボタン
  if ((controller_input.buttons & BTN_L_STICK) && !(previous_input.buttons & BTN_L_STICK)) SwitchControlLibrary().pressButton(Button::LCLICK);
  else if (!(controller_input.buttons & BTN_L_STICK) && (previous_input.buttons & BTN_L_STICK)) SwitchControlLibrary().releaseButton(Button::LCLICK);
  if ((controller_input.buttons & BTN_R_STICK) && !(previous_input.buttons & BTN_R_STICK)) SwitchControlLibrary().pressButton(Button::RCLICK);
  else if (!(controller_input.buttons & BTN_R_STICK) && (previous_input.buttons & BTN_R_STICK)) SwitchControlLibrary().releaseButton(Button::RCLICK);

  // D-pad
  if ((controller_input.dpad & DPAD_UP) && !(previous_input.dpad & DPAD_UP)) SwitchControlLibrary().pressHatButton(Hat::UP);
  else if (!(controller_input.dpad & DPAD_UP) && (previous_input.dpad & DPAD_UP)) SwitchControlLibrary().releaseHatButton();
  if ((controller_input.dpad & DPAD_DOWN) && !(previous_input.dpad & DPAD_DOWN)) SwitchControlLibrary().pressHatButton(Hat::DOWN);
  else if (!(controller_input.dpad & DPAD_DOWN) && (previous_input.dpad & DPAD_DOWN)) SwitchControlLibrary().releaseHatButton();
  if ((controller_input.dpad & DPAD_LEFT) && !(previous_input.dpad & DPAD_LEFT)) SwitchControlLibrary().pressHatButton(Hat::LEFT);
  else if (!(controller_input.dpad & DPAD_LEFT) && (previous_input.dpad & DPAD_LEFT)) SwitchControlLibrary().releaseHatButton();
  if ((controller_input.dpad & DPAD_RIGHT) && !(previous_input.dpad & DPAD_RIGHT)) SwitchControlLibrary().pressHatButton(Hat::RIGHT);
  else if (!(controller_input.dpad & DPAD_RIGHT) && (previous_input.dpad & DPAD_RIGHT)) SwitchControlLibrary().releaseHatButton();
}

// ================================================================
// SPI受信処理
// ================================================================
void onCSFalling() {
  static volatile int byte_index = 0;
  byte_index = 0;
  
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

// SPI受信データ処理（最適化）
void process_received_data() {
  ControllerData received_data;
  memcpy(&received_data, spi_buffer, sizeof(ControllerData));
  
  // CRC確認
  uint8_t received_crc = received_data.crc;
  received_data.crc = 0;
  uint8_t calculated_crc = crc8((uint8_t*)&received_data, sizeof(received_data));
  
  if (received_crc == calculated_crc) {
    // 通常モードでは受信データをそのまま使用
    controller_input = received_data;
    controller_input.crc = received_crc;
    // PC送信はloop()で統一処理
  }
}

// UART受信チェック（簡素化）
void check_uart_data() {
  while (Serial1.available()) {
    uint8_t byte = Serial1.read();
    
    if (byte == 0x00) { // COBSの終端
      if (uart_buffer_index > 0) {
        process_uart_command();
        uart_buffer_index = 0;
      }
    } else if (uart_buffer_index < UART_BUFFER_SIZE - 1) {
      uart_buffer[uart_buffer_index++] = byte;
    }
  }
}

// UART受信コマンド処理（簡素化）
void process_uart_command() {
  if (uart_buffer_index < 2) return;
  
  uint8_t decoded[UART_BUFFER_SIZE];
  size_t decoded_length;
  if (!decode_cobs(uart_buffer, uart_buffer_index, decoded, &decoded_length)) {
    return;
  }
  
  if (decoded_length < 1) return;
  
  uint8_t cmd = decoded[0];
  
  switch (cmd) {
    case CMD_PLAYBACK_START:
      playback_mode = true;
      // プリロード済みフレームがある場合は即座適用
      if (frame_preloaded) {
        controller_input = preloaded_frame;
      }
      break;
      
    case CMD_PLAYBACK_DATA:
      if (decoded_length >= sizeof(ControllerData) + 1 && playback_mode) {
        handle_playback_data(decoded + 1);
      }
      break;
      
    case CMD_PLAYBACK_STOP:
      playback_mode = false;
      frame_preloaded = false;  // プリロード状態もリセット
      break;
      
    case CMD_PRELOAD_FRAME:
      if (decoded_length >= sizeof(ControllerData) + 1) {
        // プリロードのみ（モード切り替えなし）
        handle_preload_frame(decoded + 1);
      }
      break;
  }
}

// 再生データ処理（最適化）
void handle_playback_data(const uint8_t* data) {
  // メモリアクセスを最小化して高速化
  ControllerData* playback_data_ptr = (ControllerData*)data;
  
  // CRC確認を簡略化（速度優先）
  uint8_t received_crc = playback_data_ptr->crc;
  playback_data_ptr->crc = 0;
  uint8_t calculated_crc = crc8((uint8_t*)playback_data_ptr, sizeof(ControllerData));
  
  if (received_crc == calculated_crc) {
    // 直接コピーで最高速化
    controller_input = *playback_data_ptr;
    controller_input.crc = received_crc;
  }
  // CRCエラーのログを最小化（パフォーマンス優先）
}

// プリロードフレーム処理
void handle_preload_frame(const uint8_t* data) {
  ControllerData preload_data;
  memcpy(&preload_data, data, sizeof(ControllerData));
  
  // CRC確認
  uint8_t received_crc = preload_data.crc;
  preload_data.crc = 0;
  uint8_t calculated_crc = crc8((uint8_t*)&preload_data, sizeof(preload_data));
  
  if (received_crc == calculated_crc) {
    // プリロードバッファに保存（まだ適用しない）
    preloaded_frame = preload_data;
    preloaded_frame.crc = received_crc;
    frame_preloaded = true;
  }
}

// PCへのデータ送信
void send_data_to_pc(const uint8_t *data, size_t length) {
  uint8_t packet[length + 1];
  packet[0] = CMD_DATA_PACKET;
  memcpy(packet + 1, data, length);
  send_cobs_packet(packet, sizeof(packet));
}

// COBSデコード
bool decode_cobs(const uint8_t* encoded, size_t length, uint8_t* decoded, size_t* decoded_length) {
  *decoded_length = 0;
  size_t read_index = 0;
  
  while (read_index < length) {
    uint8_t code = encoded[read_index++];
    if (code == 0) break;
    
    for (uint8_t i = 1; i < code && read_index < length; i++) {
      decoded[(*decoded_length)++] = encoded[read_index++];
    }
    
    if (code < 0xFF && read_index <= length) {
      decoded[(*decoded_length)++] = 0x00;
    }
  }
  
  return true;
}

// COBS送信
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