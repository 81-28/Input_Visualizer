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
			uint8_t byte;
			asio::read(port, asio::buffer(&byte, 1));
			if (byte != startByte) continue;

			uint8_t length;
			asio::read(port, asio::buffer(&length, 1));

			std::vector<uint8_t> in_report(length, 0);
			asio::read(port, asio::buffer(in_report));

			asio::read(port, asio::buffer(&byte, 1));

			if (byte != endByte) {
				std::cerr << "Invalid end byte" << std::endl;
				continue;
			}

			for (const auto& b : in_report) {
				std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)b << " ";
			}
			std::cout << std::dec << std::endl;

		}
		catch (std::exception& e) {
			std::cerr << "Serial port read error: " << e.what() << std::endl;
			running = false;
		}
	}
}