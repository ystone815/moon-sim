#ifndef GENERIC_PACKET_H
#define GENERIC_PACKET_H

#include <systemc.h>
#include <string>
#include <iostream>
#include "packet/base_packet.h" // Include the BasePacket header
#include "common/error_handling.h" // Include error handling utilities

// GenericPacket inherits from BasePacket
struct GenericPacket : public BasePacket {
    Command command;
    int address;
    int data;
    unsigned char databyte;
    int index; // Packet index for tracking

    // Implementation of virtual get_attribute from BasePacket
    double get_attribute(const std::string& attribute_name) const override {
        if (attribute_name == "databyte") {
            return static_cast<double>(databyte);
        } else if (attribute_name == "data") {
            return static_cast<double>(data);
        } else if (attribute_name == "address") {
            return static_cast<double>(address);
        } else if (attribute_name == "index") {
            return static_cast<double>(index);
        } else {
            SOC_SIM_WARNING("GenericPacket", soc_sim::error::codes::INVALID_ATTRIBUTE,
                           "Unknown attribute name: " + attribute_name + ". Returning 0.0.");
            return 0.0; // Default or error value
        }
    }

    // Implementation of virtual print from BasePacket
    void print(std::ostream& os) const override {
        os << "cmd: " << (command == Command::READ ? "READ" : "WRITE")
           << ", addr: " << std::hex << address
           << ", data: " << std::dec << data
           << ", databyte: " << (int)databyte
           << ", index: " << index;
    }

    // Implementation of virtual sc_trace_impl from BasePacket
    void sc_trace_impl(sc_trace_file* tf, const std::string& name) const override {
        int command_value = static_cast<int>(command);
        sc_trace(tf, command_value, name + ".command");
        sc_trace(tf, address, name + ".address");
        sc_trace(tf, data, name + ".data");
        sc_trace(tf, databyte, name + ".databyte");
        sc_trace(tf, index, name + ".index");
    }

    // Implementation of memory operation methods
    Command get_command() const override { return command; }
    int get_address() const override { return address; }
    int get_data() const override { return data; }
    unsigned char get_databyte() const override { return databyte; }
    void set_data(int new_data) override { data = new_data; }
    void set_databyte(unsigned char new_databyte) override { databyte = new_databyte; }

    // Implementation of virtual set_attribute from BasePacket
    void set_attribute(const std::string& attribute_name, double value) override {
        if (attribute_name == "databyte") {
            databyte = static_cast<unsigned char>(value);
        } else if (attribute_name == "data") {
            data = static_cast<int>(value);
        } else if (attribute_name == "address") {
            address = static_cast<int>(value);
        } else if (attribute_name == "index") {
            index = static_cast<int>(value);
        } else {
            SOC_SIM_WARNING("GenericPacket", soc_sim::error::codes::INVALID_ATTRIBUTE,
                           "Unknown attribute name: " + attribute_name + ". Ignoring set operation.");
        }
    }
};

#endif
