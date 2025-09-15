#pragma once
#include <asio.hpp>
#include <string>
#include <thread>

class SerialAnalizer
{
public:
	SerialAnalizer(const std::string portName);
	~SerialAnalizer();

	std::atomic<uint8_t> command{ 0 };

private:
	bool OpenSerialPort(std::string portName);
	void ReadLoop();

	asio::io_context io;
	asio::serial_port port;
	std::thread worker;
	bool running = true;
};

