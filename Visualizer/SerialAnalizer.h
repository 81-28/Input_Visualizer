#pragma once
#include <asio.hpp>
#include <string>
#include <thread>

namespace SwitchPro
{
    static constexpr uint8_t INFO_CONN_MASK = 0xAB;
    static constexpr uint8_t INFO_BATTERY_MASK = 0x0F;

    namespace CMD
    {
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

    namespace Buttons0
    {
        static constexpr uint8_t Y = 0x01;
        static constexpr uint8_t X = 0x02;
        static constexpr uint8_t B = 0x04;
        static constexpr uint8_t A = 0x08;
        static constexpr uint8_t R = 0x40;
        static constexpr uint8_t ZR = 0x80;
    };

    namespace Buttons1
    {
        static constexpr uint8_t MINUS = 0x01;
        static constexpr uint8_t PLUS = 0x02;
        static constexpr uint8_t R3 = 0x04;
        static constexpr uint8_t L3 = 0x08;
        static constexpr uint8_t HOME = 0x10;
        static constexpr uint8_t CAPTURE = 0x20;
    };

    namespace Buttons2
    {
        static constexpr uint8_t DPAD_DOWN = 0x01;
        static constexpr uint8_t DPAD_UP = 0x02;
        static constexpr uint8_t DPAD_RIGHT = 0x04;
        static constexpr uint8_t DPAD_LEFT = 0x08;
        static constexpr uint8_t L = 0x40;
        static constexpr uint8_t ZL = 0x80;
    };

    struct InReport
    {
        uint8_t report_id;
        uint8_t timer;
        uint8_t info;
        uint8_t buttons[3];
        uint8_t joysticks[6];
    };

    struct GamePad
    {
        uint8_t A;
		uint8_t B;
		uint8_t X;
		uint8_t Y;
		uint8_t L;
		uint8_t R;
        uint8_t L3;
		uint8_t R3;
        uint8_t MINUS;
		uint8_t PLUS;
		uint8_t HOME;
		uint8_t CAPTURE;
		uint8_t DPAD_UP;
		uint8_t DPAD_DOWN;
		uint8_t DPAD_LEFT;
		uint8_t DPAD_RIGHT;
		uint8_t ZL;
		uint8_t ZR;

        int16_t LX;
		int16_t LY;
		int16_t RX;
		int16_t RY;
    };
};


class SerialAnalizer
{
public:
	SerialAnalizer(const std::string portName);
	~SerialAnalizer();

	SwitchPro::GamePad GetGamePad();
	bool IsOpen() const { return port.is_open(); }
    bool ReadOnce(int timeout_ms);

private:
	bool OpenSerialPort(std::string portName);
    void CalcNeutral();
	void ReadLoop();

    const uint8_t startByte = 0xAA;
    const uint8_t endByte = 0xBB;

	uint16_t neutral_lx = 0;
	uint16_t neutral_ly = 0;
	uint16_t neutral_rx = 0;
	uint16_t neutral_ry = 0;

	asio::io_context io;
	asio::serial_port port;
	std::thread worker;
	bool running = true;

	std::mutex mtx;
    SwitchPro::GamePad gamepad = {};
};

