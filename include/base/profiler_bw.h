#ifndef PROFILER_BW_H
#define PROFILER_BW_H

#include <systemc.h>
#include <memory>
#include <string>
#include <iomanip>
#include "packet/base_packet.h"

// Bandwidth profiler with dedicated reporting thread
// To be used by calling profile_packet() function
template<typename PacketType>
SC_MODULE(ProfilerBW) {
    SC_HAS_PROCESS(ProfilerBW);
public:
    // Constructor
    ProfilerBW(sc_module_name name,
               const std::string& profiler_name,
               sc_time reporting_period = sc_time(10, SC_MS),
               bool debug_enable = false)
        : sc_module(name), m_profiler_name(profiler_name),
          m_reporting_period(reporting_period),
          m_debug_enable(debug_enable),
          m_total_bytes(0),
          m_total_packets(0),
          m_current_period_bytes(0),
          m_current_period_packets(0),
          m_last_report_time(SC_ZERO_TIME) {
        
        // Start the reporting thread
        SC_THREAD(reporting_process);
        
        std::cout << sc_time_stamp() << " | " << m_profiler_name 
                  << ": Bandwidth profiler initialized with reporting period " 
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
        
        // Periodic reporting is now handled by reporting_process()
        // No need to check time here as the dedicated process handles it
        
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
    
    // Manual report trigger (force final report regardless of current period bytes)
    void force_report() {
        sc_time current_time = sc_time_stamp();
        
        
        // Calculate overall statistics  
        // Always report if we have processed any packets (even with 0 bytes)
        if (m_total_packets > 0 && current_time > SC_ZERO_TIME) {
            double elapsed_seconds = current_time.to_seconds();
            double avg_throughput_bps = m_total_bytes / elapsed_seconds;
            double avg_throughput_mbps = avg_throughput_bps / (1024.0 * 1024.0);
            double avg_pps = m_total_packets / elapsed_seconds;
            
            // Report overall throughput
            std::cout << "\n" << std::string(60, '=') << std::endl;
            std::cout << sc_time_stamp() << " | " << m_profiler_name << " FINAL THROUGHPUT REPORT" << std::endl;
            std::cout << std::string(60, '=') << std::endl;
            std::cout << "  Total simulation time: " << current_time << std::endl;
            std::cout << "  Total bytes transferred: " << m_total_bytes << std::endl;
            std::cout << "  Total packets processed: " << m_total_packets << std::endl;
            std::cout << "  Average throughput: " << std::fixed << std::setprecision(2) 
                      << avg_throughput_bps << " bytes/sec" << std::endl;
            std::cout << "  Average throughput: " << std::fixed << std::setprecision(2) 
                      << avg_throughput_mbps << " MB/sec" << std::endl;
            std::cout << "  Average packet rate: " << std::fixed << std::setprecision(2) 
                      << avg_pps << " packets/sec" << std::endl;
            std::cout << std::string(60, '=') << std::endl;
            std::cout.flush(); // Ensure output is written to file
        }
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
    
    // Periodic reporting process thread
    void reporting_process() {
        // Start reporting after one period to have some data
        wait(m_reporting_period);
        
        while (true) {
            // Generate periodic report
            report_current_period();
            
            // Reset period counters
            m_current_period_bytes = 0;
            m_current_period_packets = 0;
            m_last_report_time = sc_time_stamp();
            
            // Wait for next reporting period
            wait(m_reporting_period);
        }
    }
    
    void report_current_period() {
        sc_time current_time = sc_time_stamp();
        
        if (m_current_period_bytes > 0) {
            // Calculate throughput in bytes/second
            double elapsed_seconds = m_reporting_period.to_seconds();
            double throughput_bps = m_current_period_bytes / elapsed_seconds;
            double throughput_mbps = throughput_bps / (1024.0 * 1024.0);
            double throughput_pps = m_current_period_packets / elapsed_seconds;
            
            // Report throughput
            std::cout << "\n" << std::string(60, '=') << std::endl;
            std::cout << sc_time_stamp() << " | " << m_profiler_name << " THROUGHPUT REPORT" << std::endl;
            std::cout << std::string(60, '=') << std::endl;
            std::cout << "  Period: " << m_reporting_period << std::endl;
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
    
    void report_final_summary() {
        sc_time sim_time = sc_time_stamp();
        if (m_total_bytes > 0 && sim_time > SC_ZERO_TIME) {
            double total_seconds = sim_time.to_seconds();
            double avg_throughput_bps = m_total_bytes / total_seconds;
            double avg_throughput_mbps = avg_throughput_bps / (1024.0 * 1024.0);
            double avg_pps = m_total_packets / total_seconds;
            
            std::cout << "\n" << std::string(60, '=') << std::endl;
            std::cout << sc_time_stamp() << " | " << m_profiler_name << " FINAL SUMMARY" << std::endl;
            std::cout << std::string(60, '=') << std::endl;
            std::cout << "  Total simulation time: " << sim_time << std::endl;
            std::cout << "  Total bytes transferred: " << m_total_bytes << std::endl;
            std::cout << "  Total packets processed: " << m_total_packets << std::endl;
            std::cout << "  Average throughput: " << std::fixed << std::setprecision(2) 
                      << avg_throughput_bps << " bytes/sec" << std::endl;
            std::cout << "  Average throughput: " << std::fixed << std::setprecision(2) 
                      << avg_throughput_mbps << " MB/sec" << std::endl;
            std::cout << "  Average packet rate: " << std::fixed << std::setprecision(2) 
                      << avg_pps << " packets/sec" << std::endl;
            std::cout << std::string(60, '=') << std::endl;
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
using BasePacketProfilerBW = ProfilerBW<BasePacket>;

#endif