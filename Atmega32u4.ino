// ================================================================
// Proコントローラー to Switch 変換コード
// ================================================================

#include <NintendoSwitchControlLibrary.h>

// --- 通信プロトコル定義 ---
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

// --- COBS+CRC8受信関連のコード ---
uint8_t cobs_buffer[sizeof(ControllerData) + 2];
size_t cobs_buffer_index = 0;
uint8_t crc8(const uint8_t *data, int len) {
  uint8_t crc = 0;
  while(len--){crc^=*data++;for(int i=0;i<8;i++)crc=crc&0x80?(crc<<1)^0x31:crc<<1;}
  return crc;
}
void cobs_decode_and_process(const uint8_t *buffer, size_t length) {
  if (length < 2 || length > sizeof(cobs_buffer)) return;
  uint8_t decoded_buffer[length];
  size_t read_index = 0, write_index = 0;
  while(read_index<length){uint8_t code=buffer[read_index++];for(uint8_t i=1;i<code;i++){if(read_index>=length)return;decoded_buffer[write_index++]=buffer[read_index++];}if(code<0xFF&&read_index<length)decoded_buffer[write_index++]=0;}
  if (write_index != sizeof(ControllerData)) return;
  ControllerData received_data;
  memcpy(&received_data, decoded_buffer, sizeof(ControllerData));
  uint8_t received_crc = received_data.crc;
  received_data.crc = 0;
  uint8_t calculated_crc = crc8((uint8_t*)&received_data, sizeof(received_data));
  if (received_crc == calculated_crc) memcpy(&controller_input, &received_data, sizeof(ControllerData));
}

void setup() {
  Serial1.begin(115200);
  pushButton(Button::L, 20, 2);
  pushButton(Button::R, 20, 2);
  memset(&controller_input, 0, sizeof(controller_input));
  controller_input.stick_lx = 128;
  controller_input.stick_ly = 128;
  controller_input.stick_rx = 128;
  controller_input.stick_ry = 128;
  memcpy(&previous_input, &controller_input, sizeof(controller_input));
}

void loop() {
  receive_controller_data();
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
  // Y軸は多くのゲームで上下反転しているため、(255 - 値) で補正
  SwitchControlLibrary().moveLeftStick(controller_input.stick_lx, 255 - controller_input.stick_ly);
  SwitchControlLibrary().moveRightStick(controller_input.stick_rx, 255 - controller_input.stick_ry);

  // 各ボタンを個別で判定し、ライブラリの関数を直接呼び出す
  // Yボタン
  if ((controller_input.buttons & BTN_Y) && !(previous_input.buttons & BTN_Y)) SwitchControlLibrary().pressButton(Button::Y);
  else if (!(controller_input.buttons & BTN_Y) && (previous_input.buttons & BTN_Y)) SwitchControlLibrary().releaseButton(Button::Y);
  // Bボタン
  if ((controller_input.buttons & BTN_B) && !(previous_input.buttons & BTN_B)) SwitchControlLibrary().pressButton(Button::B);
  else if (!(controller_input.buttons & BTN_B) && (previous_input.buttons & BTN_B)) SwitchControlLibrary().releaseButton(Button::B);
  // Aボタン
  if ((controller_input.buttons & BTN_A) && !(previous_input.buttons & BTN_A)) SwitchControlLibrary().pressButton(Button::A);
  else if (!(controller_input.buttons & BTN_A) && (previous_input.buttons & BTN_A)) SwitchControlLibrary().releaseButton(Button::A);
  // Xボタン
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

// RP2350からのデータ受信に専念する関数
void receive_controller_data() {
  while (Serial1.available() > 0) {
    uint8_t receivedByte = Serial1.read();
    if (receivedByte == 0) {
      if (cobs_buffer_index > 0) cobs_decode_and_process(cobs_buffer, cobs_buffer_index);
      cobs_buffer_index = 0;
    } else {
      if (cobs_buffer_index < sizeof(cobs_buffer)) cobs_buffer[cobs_buffer_index++] = receivedByte;
      else cobs_buffer_index = 0;
    }
  }
}
