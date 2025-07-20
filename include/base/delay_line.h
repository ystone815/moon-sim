#ifndef DELAY_LINE_H
#define DELAY_LINE_H

#include <systemc.h>
#include <memory>
#include <type_traits>
#include "common/error_handling.h"

// Template-based DelayLine that works with any packet type
template<typename PacketType>
SC_MODULE(DelayLine) {
    SC_HAS_PROCESS(DelayLine);
    
    // Template-based SystemC ports
    sc_fifo_in<std::shared_ptr<PacketType>> in;
    sc_fifo_out<std::shared_ptr<PacketType>> out;

    // Configuration
    const sc_time m_delay;
    const bool m_debug_enable;

    // Process method
    void process_packets() {
        while (true) {
            auto packet = in.read();
            
            if (!packet) {
                SOC_SIM_ERROR("DelayLine", soc_sim::error::codes::INVALID_PACKET_TYPE, 
                             "Received null packet");
                continue;
            }
            
            // Apply delay
            wait(m_delay);
            
            // Log packet processing if debug enabled
            if (m_debug_enable) {
                log_packet_processing(*packet);
            }
            
            // Forward packet
            out.write(packet);
        }
    }

    // Constructor
    DelayLine(sc_module_name name, sc_time delay, bool debug_enable = false) 
        : sc_module(name), m_delay(delay), m_debug_enable(debug_enable) {
        SC_THREAD(process_packets);
    }

private:
    // Simple logging that works with any packet type
    void log_packet_processing(const PacketType& packet) {
        std::cout << sc_time_stamp() << " | DelayLine: Packet processed after " 
                  << m_delay.to_string() << ", " << packet << std::endl;
    }
};

// Type alias for backward compatibility
using BasePacketDelayLine = DelayLine<BasePacket>;

#endif