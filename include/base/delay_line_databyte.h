#ifndef DELAY_LINE_DATABYTE_H
#define DELAY_LINE_DATABYTE_H

#include <systemc.h>
#include "packet/base_packet.h" // Include BasePacket
#include <string> // Required for std::string
#include <map>    // Required for std::map
#include <functional> // Required for std::function
#include <memory> // For std::unique_ptr

SC_MODULE(DelayLineDatabyte) {
    SC_HAS_PROCESS(DelayLineDatabyte);
    sc_fifo_in<std::shared_ptr<BasePacket>> in;
    sc_fifo_out<std::shared_ptr<BasePacket>> out;

    const unsigned int m_width_byte; // Data path width in bytes
    const sc_time m_clk_period; // Clock period
    const std::string m_attribute_name; // Use string directly for attribute name

    void process_packets();

    DelayLineDatabyte(sc_module_name name, unsigned int width_byte, sc_time clk_period, const std::string& attribute_name);
};

#endif
