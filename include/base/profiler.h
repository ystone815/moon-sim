#ifndef PROFILER_H
#define PROFILER_H

#include <systemc.h>
#include <memory>
#include <string>
#include <iomanip>
#include "packet/base_packet.h"

// Throughput profiler for SystemC simulation
// Monitors packet data transfer and reports throughput periodically
template<typename PacketType>
SC_MODULE(Profiler) {
    SC_HAS_PROCESS(Profiler);
    
    // SystemC ports - pass-through design
    sc_fifo_in<std::shared_ptr<PacketType>> in;
    sc_fifo_out<std::shared_ptr<PacketType>> out;
    
    // Constructor
    Profiler(sc_module_name name,
             sc_time reporting_period = sc_time(1, SC_SEC),
             bool debug_enable = false,
             const std::string& profiler_name = "Profiler")
        : sc_module(name),
          m_reporting_period(reporting_period),
          m_debug_enable(debug_enable),
          m_profiler_name(profiler_name),
          m_total_bytes(0),
          m_total_packets(0),
          m_current_period_bytes(0),
          m_current_period_packets(0),
          m_last_report_time(SC_ZERO_TIME) {
        
        SC_THREAD(monitor_throughput);
        SC_THREAD(report_throughput);
        
        std::cout << sc_time_stamp() << " | " << m_profiler_name 
                  << ": Initialized with reporting period " << m_reporting_period << std::endl;
    }

private:
    // Configuration
    const sc_time m_reporting_period;
    const bool m_debug_enable;
    const std::string m_profiler_name;
    
    // Statistics
    unsigned long long m_total_bytes;
    unsigned long long m_total_packets;
    unsigned long long m_current_period_bytes;
    unsigned long long m_current_period_packets;
    sc_time m_last_report_time;
    
    // Events
    sc_event m_packet_processed;
    
    // Monitor packets and accumulate data
    void monitor_throughput() {
        while (true) {
            auto packet = in.read();
            
            if (!packet) {
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | " << m_profiler_name 
                              << ": Received null packet" << std::endl;
                }
                continue;
            }
            
            // Extract databyte from packet
            unsigned int databyte = get_packet_databyte(*packet);
            
            // Update statistics
            m_total_bytes += databyte;
            m_total_packets++;
            m_current_period_bytes += databyte;
            m_current_period_packets++;
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | " << m_profiler_name 
                          << ": Packet processed, databyte=" << databyte 
                          << ", total_bytes=" << m_total_bytes << std::endl;
            }
            
            // Forward packet (pass-through)
            out.write(packet);
            
            // Notify reporting thread
            m_packet_processed.notify();
        }
    }
    
    // Periodic throughput reporting
    void report_throughput() {
        while (true) {
            wait(m_reporting_period);
            
            sc_time current_time = sc_time_stamp();
            sc_time elapsed_time = current_time - m_last_report_time;
            
            if (elapsed_time > SC_ZERO_TIME && m_current_period_bytes > 0) {
                // Calculate throughput in bytes/second
                double elapsed_seconds = elapsed_time.to_seconds();
                double throughput_bps = m_current_period_bytes / elapsed_seconds;
                double throughput_mbps = throughput_bps / (1024.0 * 1024.0);
                double throughput_pps = m_current_period_packets / elapsed_seconds;
                
                // Report throughput
                std::cout << "\n" << std::string(60, '=') << std::endl;
                std::cout << sc_time_stamp() << " | " << m_profiler_name << " THROUGHPUT REPORT" << std::endl;
                std::cout << std::string(60, '=') << std::endl;
                std::cout << "  Period: " << elapsed_time << std::endl;
                std::cout << "  Bytes transferred: " << m_current_period_bytes << std::endl;
                std::cout << "  Packets processed: " << m_current_period_packets << std::endl;
                std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                          << throughput_bps << " bytes/sec" << std::endl;
                std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
                          << throughput_mbps << " MB/sec" << std::endl;
                std::cout << "  Packet rate: " << std::fixed << std::setprecision(2) 
                          << throughput_pps << " packets/sec" << std::endl;
                std::cout << "  Total bytes: " << m_total_bytes << std::endl;
                std::cout << "  Total packets: " << m_total_packets << std::endl;
                std::cout << std::string(60, '=') << std::endl;
                
                // Reset period counters
                m_current_period_bytes = 0;
                m_current_period_packets = 0;
            }
            
            m_last_report_time = current_time;
        }
    }
    
    // Extract databyte from packet - specialized for different packet types
    unsigned int get_packet_databyte(const PacketType& packet) {
        // Try to get databyte attribute from packet
        try {
            return static_cast<unsigned int>(packet.get_attribute("databyte"));
        } catch (...) {
            // If attribute doesn't exist, assume default size
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | " << m_profiler_name 
                          << ": Warning - databyte attribute not found, using default size 64" << std::endl;
            }
            return 64; // Default packet size
        }
    }
};

// Type aliases for common packet types
using BasePacketProfiler = Profiler<BasePacket>;

// Helper function to create profiler
template<typename PacketType>
Profiler<PacketType>* create_profiler(
    sc_module_name name,
    sc_time reporting_period = sc_time(1, SC_SEC),
    bool debug_enable = false,
    const std::string& profiler_name = "Profiler") {
    
    return new Profiler<PacketType>(name, reporting_period, debug_enable, profiler_name);
}

#endif