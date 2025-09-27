#pragma once
#include <vector>
#include <string>

class SerialUtils
{
public:
    struct SerialPortInfo {
        std::string port;
        std::string description;
    };
    
    static std::vector<SerialPortInfo> AvailablePorts();
};