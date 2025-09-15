#include "SerialAnalizer.h"
#include <iostream>
#include <vector>
#include <iomanip>


SerialAnalizer::SerialAnalizer(const std::string portName) : port(io) {
	if (!OpenSerialPort(portName)) {
		throw std::runtime_error("Failed to open serial port");
	}
}

SerialAnalizer::~SerialAnalizer() {
	running = false;
	if (worker.joinable()) worker.join();
	if (port.is_open()) port.close();
}

bool SerialAnalizer::OpenSerialPort(std::string portName) {
	try {
		// ポートを開く
		port.open(portName);

		// ポートの設定
		port.set_option(asio::serial_port_base::baud_rate(115200));
		port.set_option(asio::serial_port_base::character_size(8));
		port.set_option(asio::serial_port_base::stop_bits(asio::serial_port_base::stop_bits::one));
		port.set_option(asio::serial_port_base::parity(asio::serial_port_base::parity::none));
		port.set_option(asio::serial_port_base::flow_control(asio::serial_port_base::flow_control::none));

		worker = std::thread(&SerialAnalizer::ReadLoop, this);
	}
	catch (std::exception& e) {
		std::cerr << "Serial port open error: " << e.what() << std::endl;
		return false;
	}
	return true;
}

void SerialAnalizer::ReadLoop() {
	const uint8_t startByte = 0xAA;
	const uint8_t endByte = 0xBB;

	while (running) {
		try {
			uint8_t current_byte;
			asio::read(port, asio::buffer(&current_byte, 1));
			if (current_byte != startByte) continue;

			uint8_t length;
			asio::read(port, asio::buffer(&length, 1));

			std::vector<uint8_t> in_report(length, 0);
			asio::read(port, asio::buffer(in_report));

			asio::read(port, asio::buffer(&current_byte, 1));

			if (current_byte != endByte) {
				std::cerr << "Invalid end byte" << std::endl;
				continue;
			}

			SwitchPro::InReport rep;
			SwitchPro::GamePad gp;

			rep.report_id = in_report[0];
			rep.timer     = in_report[1];
			rep.info      = in_report[2];
			std::copy(in_report.begin() + 3, in_report.begin() + 6, rep.buttons);
			std::copy(in_report.begin() + 6, in_report.begin() + 12, rep.joysticks);

			rep.buttons[0] & SwitchPro::Buttons0::A ? gp.A = 1 : gp.A = 0;
			rep.buttons[0] & SwitchPro::Buttons0::B ? gp.B = 1 : gp.B = 0;
			rep.buttons[0] & SwitchPro::Buttons0::X ? gp.X = 1 : gp.X = 0;
			rep.buttons[0] & SwitchPro::Buttons0::Y ? gp.Y = 1 : gp.Y = 0;
			rep.buttons[2] & SwitchPro::Buttons2::L ? gp.L = 1 : gp.L = 0;
			rep.buttons[0] & SwitchPro::Buttons0::R ? gp.R = 1 : gp.R = 0;
			rep.buttons[1] & SwitchPro::Buttons1::L3 ? gp.L3 = 1 : gp.L3 = 0;
			rep.buttons[1] & SwitchPro::Buttons1::R3 ? gp.R3 = 1 : gp.R3 = 0;
			rep.buttons[1] & SwitchPro::Buttons1::MINUS ? gp.MINUS = 1 : gp.MINUS = 0;
			rep.buttons[1] & SwitchPro::Buttons1::PLUS ? gp.PLUS = 1 : gp.PLUS = 0;
			rep.buttons[1] & SwitchPro::Buttons1::HOME ? gp.HOME = 1 : gp.HOME = 0;
			rep.buttons[1] & SwitchPro::Buttons1::CAPTURE ? gp.CAPTURE = 1 : gp.CAPTURE = 0;

			rep.buttons[2] & SwitchPro::Buttons2::DPAD_UP ? gp.DPAD_UP = 1 : gp.DPAD_UP = 0;
			rep.buttons[2] & SwitchPro::Buttons2::DPAD_DOWN ? gp.DPAD_DOWN = 1 : gp.DPAD_DOWN = 0;
			rep.buttons[2] & SwitchPro::Buttons2::DPAD_LEFT ? gp.DPAD_LEFT = 1 : gp.DPAD_LEFT = 0;
			rep.buttons[2] & SwitchPro::Buttons2::DPAD_RIGHT ? gp.DPAD_RIGHT = 1 : gp.DPAD_RIGHT = 0;

			rep.buttons[2] & SwitchPro::Buttons2::ZL ? gp.ZL = 1 : gp.ZL = 0;
			rep.buttons[0] & SwitchPro::Buttons0::ZR ? gp.ZR = 1 : gp.ZR = 0;

			uint16_t lx = rep.joysticks[0] | ((rep.joysticks[1] & 0x0F) << 8);
			uint16_t ly = (rep.joysticks[1] >> 4) | (rep.joysticks[2] << 4);
			uint16_t rx = rep.joysticks[3] | ((rep.joysticks[4] & 0x0F) << 8);
			uint16_t ry = (rep.joysticks[4] >> 4) | (rep.joysticks[5] << 4);

			gp.LX = lx - 2048 - 23;
			gp.LY = ly - 2048 - 45;
			gp.RX = rx - 2048;
			gp.RY = ry - 2048;
			{
				std::lock_guard<std::mutex> lock(mtx);
				gamepad = gp;
			}

			/*for (const auto& byte : in_report) {
				std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
			}
			std::cout << std::dec << std::endl;*/

		}
		catch (std::exception& e) {
			std::cerr << "Serial port read error: " << e.what() << std::endl;
			running = false;
		}
	}
}

SwitchPro::GamePad SerialAnalizer::GetGamePad() {
	std::lock_guard<std::mutex> lock(mtx);
	return gamepad;
}