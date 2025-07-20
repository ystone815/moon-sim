#ifndef DELAY_LINE_DATABYTE_H
#define DELAY_LINE_DATABYTE_H

#include <systemc.h>
#include <memory>
#include <string>
#include <functional>
#include "common/error_handling.h"

// Template-based DelayLineDatabyte that works with any packet type
template<typename PacketType>
SC_MODULE(DelayLineDatabyte) {
    SC_HAS_PROCESS(DelayLineDatabyte);
    
    // Template-based SystemC ports
    sc_fifo_in<std::shared_ptr<PacketType>> in;
    sc_fifo_out<std::shared_ptr<PacketType>> out;

    // Configuration parameters
    const unsigned int m_width_byte;
    const sc_time m_clk_period;
    const std::string m_attribute_name;
    
    // Attribute accessor function
    std::function<double(const PacketType&)> m_attribute_accessor;

    // Process method
    void process_packets() {
        while (true) {
            auto packet = in.read();
            
            if (!packet) {
                SOC_SIM_ERROR("DelayLineDatabyte", soc_sim::error::codes::INVALID_PACKET_TYPE, 
                             "Received null packet");
                continue;
            }
            
            // Get attribute value using the accessor function
            double attribute_value = m_attribute_accessor(*packet);
            
            // Calculate delay based on attribute, width, and clock period
            sc_time calculated_delay = sc_time((attribute_value / m_width_byte) * m_clk_period.to_double(), SC_SEC);
            
            // Apply calculated delay
            wait(calculated_delay);
            
            // Log packet processing
            std::cout << sc_time_stamp() << " | DelayLineDatabyte: Packet processed after " 
                      << calculated_delay.to_string() << ", " << *packet << std::endl;
            
            // Forward packet
            out.write(packet);
        }
    }

    // Constructor with attribute accessor function
    DelayLineDatabyte(sc_module_name name, 
                     unsigned int width_byte,
                     sc_time clk_period,
                     std::function<double(const PacketType&)> attribute_accessor)
        : sc_module(name), 
          m_width_byte(width_byte), 
          m_clk_period(clk_period),
          m_attribute_name("custom"),
          m_attribute_accessor(attribute_accessor) {
        SC_THREAD(process_packets);
    }
    
    // Constructor with attribute name (requires PacketType to have get_attribute method)
    DelayLineDatabyte(sc_module_name name,
                     unsigned int width_byte,
                     sc_time clk_period,
                     const std::string& attribute_name)
        : sc_module(name),
          m_width_byte(width_byte),
          m_clk_period(clk_period),
          m_attribute_name(attribute_name) {
        
        // Set up attribute accessor for types with get_attribute method
        m_attribute_accessor = [attribute_name](const PacketType& packet) -> double {
            return packet.get_attribute(attribute_name);
        };
        
        SC_THREAD(process_packets);
    }
};

// Type alias for backward compatibility
using BasePacketDelayLineDatabyte = DelayLineDatabyte<BasePacket>;

// Helper function to create DelayLineDatabyte with custom accessor
template<typename PacketType>
DelayLineDatabyte<PacketType>* create_delay_line_databyte(
    sc_module_name name,
    unsigned int width_byte,
    sc_time clk_period,
    std::function<double(const PacketType&)> accessor) {
    
    return new DelayLineDatabyte<PacketType>(name, width_byte, clk_period, accessor);
}

// Helper function for common attribute names
template<typename PacketType>
DelayLineDatabyte<PacketType>* create_delay_line_databyte_by_name(
    sc_module_name name,
    unsigned int width_byte,
    sc_time clk_period,
    const std::string& attribute_name) {
    
    return new DelayLineDatabyte<PacketType>(name, width_byte, clk_period, attribute_name);
}

#endif