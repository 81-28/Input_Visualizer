import serial
import time

# 必須: PySerial
# pip install pyserial

SERIAL_PORT_NAME = 'COM9'  # 環境に合わせて変更

BAUD_RATE = 115200

button_names = [
    "DOWN", "UP", "RIGHT", "LEFT", "", "", "L", "ZL",
    "SELECT", "START", "R3", "L3", "HOME", "CAPTURE", "HOGE", "FUGA",
    "Y", "X", "B", "A", "", "", "R", "ZR"
]

# バイト配列を16進数の文字列に変換して表示
def print_hex(data):
    print(' '.join(f'{byte:02x}' for byte in data))

def main():
    print(f"Opening serial port: {SERIAL_PORT_NAME}")
    try:
        # シリアルポートを開く
        ser = serial.Serial(SERIAL_PORT_NAME, BAUD_RATE, timeout=1)
        # 接続が安定するまで少し待つ
        time.sleep(2) 
        print("Port opened successfully. Waiting for data...")

        freq = 0
        cnt = 0
        last_cnt = 0

        start = time.time()

        while True:
            # 1. 開始バイト(0xAA)を待つ
            start_byte = ser.read(1)
            if start_byte != b'\xaa':
                continue

            # 2. データ長(1バイト)を読む
            length_byte = ser.read(1)
            if not length_byte:
                print("Warning: Timed out waiting for length.")
                continue
            data_length = int.from_bytes(length_byte, 'big')

            # 3. データ長の分だけHIDレポートを読む
            payload = ser.read(data_length)
            if len(payload) < data_length:
                print("Warning: Timed out waiting for payload.")
                continue

            # 4. 終了バイト(0xBB)を読む
            end_byte = ser.read(1)
            if end_byte == b'\xbb':
                cnt += 1
                if time.time() - freq >= 1.0:
                    # print(f"Frequency: {cnt} Hz", end="")
                    last_cnt = cnt
                    freq = time.time()
                    cnt = 0

                end = time.time()
                elapsed = end - start
                h = int(elapsed // 3600)
                m = int((elapsed % 3600) // 60)
                s = int(elapsed % 60)

                # 5. 受信成功
                print(f"Time: {h:02d}:{m:02d}:{s:02d}, Freq: {last_cnt} Hz, Received {data_length} bytes: ", end="")
                print_hex(payload)
                # ボタン状態の解析
                button_byte = payload[3:6]
                print_hex(button_byte)
                button_bits = int.from_bytes(button_byte, 'big')
                for i in range(24):
                    if button_bits & (1 << i):
                        name = button_names[i] if i < len(button_names) else f"Button {i}"
                        print(f"{name} is pressed.")
                # 左スティックの解析
                l_stick_byte = payload[6:9]
                print_hex(l_stick_byte)
                l_stick_val = int.from_bytes(l_stick_byte, 'big')
                l_stick_x = (l_stick_val >> 12) & 0xFFF
                l_stick_y = l_stick_val & 0xFFF
                print(f"Left Stick: X={l_stick_x}, Y={l_stick_y}")
                # 右スティックの解析
                r_stick_byte = payload[9:12]
                print_hex(r_stick_byte)
                r_stick_val = int.from_bytes(r_stick_byte, 'big')
                r_stick_x = (r_stick_val >> 12) & 0xFFF
                r_stick_y = r_stick_val & 0xFFF
                print(f"Right Stick: X={r_stick_x}, Y={r_stick_y}")
            else:
                print("Error: Frame end byte mismatch.")

    except serial.SerialException as e:
        print(f"Error: {e}")
    except KeyboardInterrupt:
        print("\nProgram stopped by user.")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("Serial port closed.")

if __name__ == '__main__':
    main()