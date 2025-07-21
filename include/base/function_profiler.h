#ifndef FUNCTION_PROFILER_H
#define FUNCTION_PROFILER_H

#include <systemc.h>
#include <memory>
#include <string>
#include <iomanip>
#include "packet/base_packet.h"

// Simple function-based profiler for SystemC simulation
// To be used by calling profile_packet() function
template<typename PacketType>
class FunctionProfiler {
public:
    // Constructor
    FunctionProfiler(const std::string& profiler_name,
                     sc_time reporting_period = sc_time(10, SC_MS),
                     bool debug_enable = false)
        : m_profiler_name(profiler_name),
          m_reporting_period(reporting_period),
          m_debug_enable(debug_enable),
          m_total_bytes(0),
          m_total_packets(0),
          m_current_period_bytes(0),
          m_current_period_packets(0),
          m_last_report_time(SC_ZERO_TIME) {
        
        std::cout << sc_time_stamp() << " | " << m_profiler_name 
                  << ": Function-based profiler initialized with reporting period " 
                  << m_reporting_period << std::endl;
    }
    
    // Function to profile a packet (called by the parent module)
    void profile_packet(const PacketType& packet) {
        // Extract databyte from packet
        unsigned int databyte = get_packet_databyte(packet);
        
        // Update statistics
        m_total_bytes += databyte;
        m_total_packets++;
        m_current_period_bytes += databyte;
        m_current_period_packets++;
        
        // Check if it's time to report
        sc_time current_time = sc_time_stamp();
        if (current_time - m_last_report_time >= m_reporting_period) {
            report_current_period();
        }
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | " << m_profiler_name 
                      << ": Packet profiled, databyte=" << databyte 
                      << ", total_bytes=" << m_total_bytes << std::endl;
        }
    }
    
    // Overloaded version for shared_ptr
    void profile_packet(const std::shared_ptr<PacketType>& packet) {
        if (packet) {
            profile_packet(*packet);
        }
    }
    
    // Manual report trigger (optional)
    void force_report() {
        report_current_period();
    }
    
    // Get current statistics (for external queries)
    struct ProfileStats {
        unsigned long long total_bytes;
        unsigned long long total_packets;
        unsigned long long current_period_bytes;
        unsigned long long current_period_packets;
        sc_time last_report_time;
    };
    
    ProfileStats get_stats() const {
        return {m_total_bytes, m_total_packets, m_current_period_bytes, 
                m_current_period_packets, m_last_report_time};
    }

private:
    // Configuration
    const std::string m_profiler_name;
    const sc_time m_reporting_period;
    const bool m_debug_enable;
    
    // Statistics
    unsigned long long m_total_bytes;
    unsigned long long m_total_packets;
    unsigned long long m_current_period_bytes;
    unsigned long long m_current_period_packets;
    sc_time m_last_report_time;
    
    void report_current_period() {
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
using BasePacketFunctionProfiler = FunctionProfiler<BasePacket>;

#endif