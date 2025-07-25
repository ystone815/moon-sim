#ifndef FLASH_PACKET_H
#define FLASH_PACKET_H

#include <systemc.h>
#include <string>
#include <iostream>
#include <memory>
#include "packet/base_packet.h"

// Flash-specific command types
enum class FlashCommand {
    READ = 0,
    PROGRAM = 1,
    ERASE = 2
};

// NAND Flash 5-dimensional addressing structure
struct FlashAddress {
    uint8_t plane;      // Plane (0-3 for 4-plane)
    uint16_t block;     // Block address
    uint8_t wl;         // WordLine (0-127 typical)
    uint8_t ssl;        // String Select Line
    uint16_t page;      // Page address (16KiB units)
    
    FlashAddress() : plane(0), block(0), wl(0), ssl(0), page(0) {}
    
    FlashAddress(uint8_t p, uint16_t b, uint8_t w, uint8_t s, uint16_t pg)
        : plane(p), block(b), wl(w), ssl(s), page(pg) {}
    
    // Convert to string for debugging
    std::string to_string() const {
        return "P" + std::to_string(plane) + 
               "B" + std::to_string(block) +
               "W" + std::to_string(wl) +
               "S" + std::to_string(ssl) +
               "Pg" + std::to_string(page);
    }
};

// FlashPacket extends BasePacket with NAND Flash specific fields
struct FlashPacket : public BasePacket {
    FlashCommand flash_command;
    FlashAddress flash_address;
    uint32_t data_size;                    // Data size in bytes
    int index;                             // Packet tracking index
    
    // Preserve original BasePacket information
    std::shared_ptr<BasePacket> original_packet;
    
    // Default constructor
    FlashPacket() : flash_command(FlashCommand::READ), data_size(0), index(-1) {}
    
    // Constructor with BasePacket
    FlashPacket(std::shared_ptr<BasePacket> orig_packet) 
        : flash_command(FlashCommand::READ), data_size(0), index(-1), original_packet(orig_packet) {
        if (orig_packet) {
            // Copy basic attributes from original packet
            index = static_cast<int>(orig_packet->get_attribute("index"));
        }
    }
    
    // Implementation of virtual get_attribute from BasePacket
    double get_attribute(const std::string& attribute_name) const override {
        if (attribute_name == "plane") {
            return static_cast<double>(flash_address.plane);
        } else if (attribute_name == "block") {
            return static_cast<double>(flash_address.block);
        } else if (attribute_name == "wl" || attribute_name == "wordline") {
            return static_cast<double>(flash_address.wl);
        } else if (attribute_name == "ssl") {
            return static_cast<double>(flash_address.ssl);
        } else if (attribute_name == "page") {
            return static_cast<double>(flash_address.page);
        } else if (attribute_name == "data_size") {
            return static_cast<double>(data_size);
        } else if (attribute_name == "index") {
            return static_cast<double>(index);
        } else if (attribute_name == "databyte") {
            // For compatibility with existing profiler
            return static_cast<double>(data_size);
        } else if (original_packet) {
            // Delegate to original packet if available
            return original_packet->get_attribute(attribute_name);
        } else {
            return 0.0;
        }
    }
    
    // Implementation of virtual set_attribute from BasePacket
    void set_attribute(const std::string& attribute_name, double value) override {
        if (attribute_name == "plane") {
            flash_address.plane = static_cast<uint8_t>(value);
        } else if (attribute_name == "block") {
            flash_address.block = static_cast<uint16_t>(value);
        } else if (attribute_name == "wl" || attribute_name == "wordline") {
            flash_address.wl = static_cast<uint8_t>(value);
        } else if (attribute_name == "ssl") {
            flash_address.ssl = static_cast<uint8_t>(value);
        } else if (attribute_name == "page") {
            flash_address.page = static_cast<uint16_t>(value);
        } else if (attribute_name == "data_size") {
            data_size = static_cast<uint32_t>(value);
        } else if (attribute_name == "index") {
            index = static_cast<int>(value);
        } else if (original_packet) {
            // Delegate to original packet if available
            original_packet->set_attribute(attribute_name, value);
        }
    }
    
    // Implementation of virtual print from BasePacket
    void print(std::ostream& os) const override {
        const char* cmd_str = (flash_command == FlashCommand::READ) ? "READ" :
                              (flash_command == FlashCommand::PROGRAM) ? "PROGRAM" : "ERASE";
        
        os << "FlashCmd: " << cmd_str 
           << ", Addr: " << flash_address.to_string()
           << ", Size: " << data_size << "B"
           << ", Index: " << index;
    }
    
    // Implementation of virtual sc_trace_impl from BasePacket
    void sc_trace_impl(sc_trace_file* tf, const std::string& name) const override {
        int command_value = static_cast<int>(flash_command);
        sc_trace(tf, command_value, name + ".flash_command");
        sc_trace(tf, flash_address.plane, name + ".plane");
        sc_trace(tf, flash_address.block, name + ".block");
        sc_trace(tf, flash_address.wl, name + ".wl");
        sc_trace(tf, flash_address.ssl, name + ".ssl");
        sc_trace(tf, flash_address.page, name + ".page");
        sc_trace(tf, data_size, name + ".data_size");
        sc_trace(tf, index, name + ".index");
    }
    
    // Implementation of memory operation methods (for compatibility)
    Command get_command() const override { 
        return (flash_command == FlashCommand::READ) ? Command::READ : Command::WRITE; 
    }
    
    int get_address() const override { 
        // Return a linearized address for compatibility
        return (flash_address.plane << 20) | (flash_address.block << 8) | 
               (flash_address.wl << 4) | (flash_address.ssl << 2) | flash_address.page;
    }
    
    int get_data() const override { 
        return original_packet ? original_packet->get_data() : 0; 
    }
    
    unsigned char get_databyte() const override { 
        return original_packet ? original_packet->get_databyte() : static_cast<unsigned char>(data_size); 
    }
    
    void set_data(int data) override { 
        if (original_packet) original_packet->set_data(data); 
    }
    
    void set_databyte(unsigned char databyte) override { 
        if (original_packet) original_packet->set_databyte(databyte); 
        data_size = static_cast<uint32_t>(databyte);
    }
    
    // Flash-specific methods
    FlashCommand get_flash_command() const { return flash_command; }
    const FlashAddress& get_flash_address() const { return flash_address; }
    uint32_t get_data_size() const { return data_size; }
    
    void set_flash_command(FlashCommand cmd) { flash_command = cmd; }
    void set_flash_address(const FlashAddress& addr) { flash_address = addr; }
    void set_data_size(uint32_t size) { data_size = size; }
};

#endif