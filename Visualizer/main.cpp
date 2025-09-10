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
	const uint8_t stop_bytes = 0xBB;

	asio::io_context io;
	asio::serial_port port(io);

	open_serial_port(port, port_name);
	return 0;
}