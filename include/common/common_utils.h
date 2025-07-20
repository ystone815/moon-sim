#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <systemc.h>
#include <string>
#include <iostream>
#include <map>
#include <functional>
#include "packet/base_packet.h" // Include BasePacket
#include "packet/generic_packet.h" // Include GenericPacket for specific access
#include "common/error_handling.h" // Include error handling utilities

// Function to print packet-related logs with consistent formatting
inline void print_packet_log(std::ostream& os, const std::string& module_name, const BasePacket& p, const std::string& custom_message = "") {
    os << sc_time_stamp() << " | " << module_name << ": ";
    if (!custom_message.empty()) {
        os << custom_message << ", ";
    }
    // Use the virtual print function from BasePacket
    p.print(os);
    os << std::endl;
}

// Function to get packet attribute value dynamically - optimized version
inline double get_packet_attribute_value(const BasePacket& p, const std::string& attribute_name) {
    // Direct call to virtual method - more efficient than map lookup
    // The BasePacket's get_attribute method already handles validation
    return p.get_attribute(attribute_name);
}

#endif
