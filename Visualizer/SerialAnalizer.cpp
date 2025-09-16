#include "SerialAnalizer.h"
#include <iostream>
#include <vector>
#include <iomanip>


SerialAnalizer::SerialAnalizer(const std::string portName) : port(io) {
	if (!OpenSerialPort(portName)) {
		throw std::runtime_error("Failed to open serial port");
	}
	CalcNeutral();
	worker = std::thread(&SerialAnalizer::ReadLoop, this);
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
		std::cout << "port opened" << std::endl;
	}
	catch (std::exception& e) {
		std::cerr << "Serial port open error: " << e.what() << std::endl;
		return false;
	}
	return true;
}

bool SerialAnalizer::ReadOnce(int timeout_ms) {
	char buf;
	bool timed_out = false;
	bool read_ok = false;

	// 1. タイマーを設定
	asio::steady_timer timer(io);
	timer.expires_after(std::chrono::milliseconds(timeout_ms));
	timer.async_wait([&](const asio::error_code& ec) {
		if (!ec) { // 正常にタイマー発火
			timed_out = true;
			port.cancel();
		}
	});

	// 2. 非同期読み込みを開始
	asio::async_read(port, asio::buffer(&buf, 1),
		[&](const asio::error_code& ec, std::size_t /*length*/) {
			if (!ec) {
				read_ok = true;
			}
		});

	// 3. io_context を実行
	io.restart();
	while (io.run_one()) {
		if (read_ok) {
			// 読み込み成功 → タイマーキャンセル
			timer.cancel();
		}
		else if (timed_out) {
			// タイムアウト → 読み込みキャンセル済み
		}
	}

	// 4. 結果を返す
	return read_ok && !timed_out;
}

void SerialAnalizer::CalcNeutral() {
	std::cout << "Calculating neutral position..." << std::endl;

	if (!SerialAnalizer::ReadOnce(200)) {
		std::cerr << "Timeout waiting for data." << std::endl;
		running = false;
		return;
	}

	int buf_lx[128] = { 0 };
	int buf_ly[128] = { 0 };
	int buf_rx[128] = { 0 };
	int buf_ry[128] = { 0 };

	// 20回のデータでニュートラルを計算
	int n;
	for (n = 0; n < 20; ++n) {
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
			std::copy(in_report.begin() + 6, in_report.begin() + 12, rep.joysticks);

			uint16_t lx = rep.joysticks[0] | ((rep.joysticks[1] & 0x0F) << 8);
			uint16_t ly = (rep.joysticks[1] >> 4) | (rep.joysticks[2] << 4);
			uint16_t rx = rep.joysticks[3] | ((rep.joysticks[4] & 0x0F) << 8);
			uint16_t ry = (rep.joysticks[4] >> 4) | (rep.joysticks[5] << 4);

			buf_lx[n] = lx - 2048;
			buf_ly[n] = ly - 2048;
			buf_rx[n] = rx - 2048;
			buf_ry[n] = ry - 2048;
		}
		catch (std::exception& e) {
			std::cerr << "Serial port read error: " << e.what() << std::endl;
			running = false;
		}
	}
	int sum_lx = 0;
	int sum_ly = 0;
	int sum_rx = 0;
	int sum_ry = 0;

	for (int i = 0; i < n; ++i) {
		sum_lx += buf_lx[i];
		sum_ly += buf_ly[i];
		sum_rx += buf_rx[i];
		sum_ry += buf_ry[i];
	}

	std::cout << "Neutral LX: " << sum_lx / n << std::endl;
	std::cout << "Neutral LY: " << sum_ly / n << std::endl;
	std::cout << "Neutral RX: " << sum_rx / n << std::endl;
	std::cout << "Neutral RY: " << sum_ry / n << std::endl;

	neutral_lx = sum_lx / n;
	neutral_ly = sum_ly / n;
	neutral_rx = sum_rx / n;
	neutral_ry = sum_ry / n;
}

void SerialAnalizer::ReadLoop() {


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

			gp.LX = lx - 2048 - neutral_lx;
			gp.LY = ly - 2048 - neutral_ly;
			gp.RX = rx - 2048 - neutral_rx;
			gp.RY = ry - 2048 - neutral_ry;
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