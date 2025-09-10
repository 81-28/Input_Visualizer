#include <iostream>
#include <string>
#include <iomanip>
#include <asio.hpp>


void open_serial_port(asio::serial_port& port, std::string port_name) {
	try {
		// ポートを開く
		port.open(port_name);

		// 各種ポートの設定
		port.set_option(asio::serial_port_base::baud_rate(115200));
		port.set_option(asio::serial_port_base::character_size(8));
		port.set_option(asio::serial_port_base::stop_bits(asio::serial_port_base::stop_bits::one));
		port.set_option(asio::serial_port_base::parity(asio::serial_port_base::parity::none));
		port.set_option(asio::serial_port_base::flow_control(asio::serial_port_base::flow_control::none));

		std::cout << "Port opened successfully on " << port_name << "at 115200 baud." << std::endl;
	}
	catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		exit(1);
	}
}


int main() {
	std::string port_name = "COM9";
	const uint8_t start_byte = 0xAA;
	const uint8_t end_byte   = 0xBB;

	asio::io_context io;
	asio::serial_port port(io);

	open_serial_port(port, port_name);

	while (true) {
		try {
			// 1. 開始バイトを待つ
			uint8_t current_byte;
			asio::read(port, asio::buffer(&current_byte, 1));
			if (current_byte != start_byte) continue;

			// 2. データ長を読む
			uint8_t length;
			asio::read(port, asio::buffer(&length, 1));

			// 3. HIDデータを読む
			std::vector<uint8_t> in_report(length, 0);
			asio::read(port, asio::buffer(in_report));

			// 4. 終了バイトか検証
			asio::read(port, asio::buffer(&current_byte, 1));

			// 終了バイト出ない場合は再度開始バイトを探す
			if (current_byte != end_byte) {
				std::cerr << "Warning: Invalid end byte. Packet discarded." << std::endl;
				continue;
			}

			std::cout << "Received " << (int)length << " bytes";
			for (const auto& byte : in_report) {
				std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
			}
			std::cout << std::dec << std::endl;

		}
		catch (const std::exception& e) {
			std::cerr << "Error: " << e.what() << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}

	open_serial_port(port, port_name);
	return 0;
}