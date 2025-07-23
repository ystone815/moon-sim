#ifndef PROFILER_LATENCY_H
#define PROFILER_LATENCY_H

#include <systemc.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include "packet/base_packet.h"

// Latency profiler for measuring request-to-response latency
// Tracks packets by index and calculates latency statistics
template<typename PacketType>
SC_MODULE(ProfilerLatency) {
    SC_HAS_PROCESS(ProfilerLatency);
public:
    // Constructor
    ProfilerLatency(sc_module_name name,
                    const std::string& profiler_name,
                    sc_time reporting_period = sc_time(100, SC_MS),
                    bool debug_enable = false)
        : sc_module(name), m_profiler_name(profiler_name),
          m_reporting_period(reporting_period),
          m_debug_enable(debug_enable),
          m_total_requests(0),
          m_total_responses(0),
          m_total_latency(SC_ZERO_TIME),
          m_min_latency(sc_time(1, SC_SEC)),  // Initialize to high value
          m_max_latency(SC_ZERO_TIME),
          m_last_report_time(SC_ZERO_TIME) {
        
        // Start the reporting thread (inactive for now, similar to ProfilerBW)
        SC_THREAD(reporting_process);
        
        std::cout << sc_time_stamp() << " | " << m_profiler_name 
                  << ": Latency profiler initialized with reporting period " 
                  << m_reporting_period << std::endl;
    }
    
    // Function to profile a request packet (called when request is sent)
    void profile_request(const PacketType& packet) {
        unsigned int packet_index = get_packet_index(packet);
        sc_time current_time = sc_time_stamp();
        
        // Store request timestamp
        m_request_timestamps[packet_index] = current_time;
        m_total_requests++;
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | " << m_profiler_name 
                      << ": Request tracked, index=" << packet_index 
                      << ", timestamp=" << current_time << std::endl;
        }
        
        // Check if it's time to report
        if (current_time - m_last_report_time >= m_reporting_period) {
            report_current_period();
        }
    }
    
    // Function to profile a response packet (called when response is received)
    void profile_response(const PacketType& packet) {
        unsigned int packet_index = get_packet_index(packet);
        sc_time current_time = sc_time_stamp();
        
        // Look for matching request
        auto request_it = m_request_timestamps.find(packet_index);
        if (request_it != m_request_timestamps.end()) {
            // Calculate latency
            sc_time latency = current_time - request_it->second;
            
            // Update statistics
            m_total_responses++;
            m_total_latency += latency;
            m_current_period_latencies.push_back(latency);
            
            // Update min/max
            if (latency < m_min_latency) m_min_latency = latency;
            if (latency > m_max_latency) m_max_latency = latency;
            
            // Remove the request from tracking
            m_request_timestamps.erase(request_it);
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | " << m_profiler_name 
                          << ": Response received, index=" << packet_index 
                          << ", latency=" << latency << std::endl;
            }
        } else {
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | " << m_profiler_name 
                          << ": Warning - Response without matching request, index=" << packet_index << std::endl;
            }
        }
        
        // Check if it's time to report
        if (current_time - m_last_report_time >= m_reporting_period) {
            report_current_period();
        }
    }
    
    // Overloaded versions for shared_ptr
    void profile_request(const std::shared_ptr<PacketType>& packet) {
        if (packet) {
            profile_request(*packet);
        }
    }
    
    void profile_response(const std::shared_ptr<PacketType>& packet) {
        if (packet) {
            profile_response(*packet);
        }
    }
    
    // Manual report trigger
    void force_report() {
        report_current_period();
    }
    
    // Get current statistics
    struct LatencyStats {
        unsigned long long total_requests;
        unsigned long long total_responses;
        sc_time avg_latency;
        sc_time min_latency;
        sc_time max_latency;
        unsigned long long pending_requests;
    };
    
    LatencyStats get_stats() const {
        sc_time avg_latency = (m_total_responses > 0) ? 
            sc_time(m_total_latency.to_seconds() / m_total_responses, SC_SEC) : SC_ZERO_TIME;
            
        return {m_total_requests, m_total_responses, avg_latency, 
                m_min_latency, m_max_latency, m_request_timestamps.size()};
    }

private:
    // Configuration
    const std::string m_profiler_name;
    const sc_time m_reporting_period;
    const bool m_debug_enable;
    
    // Statistics
    unsigned long long m_total_requests;
    unsigned long long m_total_responses;
    sc_time m_total_latency;
    sc_time m_min_latency;
    sc_time m_max_latency;
    sc_time m_last_report_time;
    
    // Request tracking
    std::unordered_map<unsigned int, sc_time> m_request_timestamps;
    
    // Current period tracking
    std::vector<sc_time> m_current_period_latencies;
    
    // Calculate percentile from sorted latency vector
    sc_time calculate_percentile(const std::vector<sc_time>& sorted_latencies, double percentile) const {
        if (sorted_latencies.empty()) return SC_ZERO_TIME;
        
        double index = (percentile / 100.0) * (sorted_latencies.size() - 1);
        size_t lower_index = static_cast<size_t>(std::floor(index));
        size_t upper_index = static_cast<size_t>(std::ceil(index));
        
        if (lower_index == upper_index) {
            return sorted_latencies[lower_index];
        }
        
        // Linear interpolation between two values
        double weight = index - lower_index;
        double lower_ns = sorted_latencies[lower_index].to_seconds() * 1000000000;
        double upper_ns = sorted_latencies[upper_index].to_seconds() * 1000000000;
        double interpolated_ns = lower_ns + weight * (upper_ns - lower_ns);
        
        return sc_time(interpolated_ns / 1000000000, SC_SEC);
    }
    
    // Calculate standard deviation
    double calculate_std_deviation(const std::vector<sc_time>& latencies, sc_time mean) const {
        if (latencies.size() <= 1) return 0.0;
        
        double sum_squared_diff = 0.0;
        double mean_ns = mean.to_seconds() * 1000000000;
        
        for (const auto& latency : latencies) {
            double latency_ns = latency.to_seconds() * 1000000000;
            double diff = latency_ns - mean_ns;
            sum_squared_diff += diff * diff;
        }
        
        return std::sqrt(sum_squared_diff / (latencies.size() - 1));
    }
    
    // Periodic reporting process thread (inactive - similar to ProfilerBW)
    void reporting_process() {
        // This process is primarily for future extension
        // For now, periodic reporting is handled in profile_request/response()
        wait(); // Wait indefinitely - this process is inactive
    }
    
    void report_current_period() {
        sc_time current_time = sc_time_stamp();
        
        if (!m_current_period_latencies.empty()) {
            // Calculate period statistics
            sc_time period_total(SC_ZERO_TIME);
            sc_time period_min = m_current_period_latencies[0];
            sc_time period_max = m_current_period_latencies[0];
            
            for (const auto& latency : m_current_period_latencies) {
                period_total += latency;
                if (latency < period_min) period_min = latency;
                if (latency > period_max) period_max = latency;
            }
            
            sc_time period_avg(period_total.to_seconds() / m_current_period_latencies.size(), SC_SEC);
            
            // Sort latencies for percentile calculations
            std::vector<sc_time> sorted_latencies = m_current_period_latencies;
            std::sort(sorted_latencies.begin(), sorted_latencies.end());
            
            // Calculate percentiles
            sc_time period_median = calculate_percentile(sorted_latencies, 50.0);
            sc_time period_p95 = calculate_percentile(sorted_latencies, 95.0);
            sc_time period_p99 = calculate_percentile(sorted_latencies, 99.0);
            
            // Calculate standard deviation
            double period_stddev = calculate_std_deviation(m_current_period_latencies, period_avg);
            
            // Overall average
            sc_time overall_avg = (m_total_responses > 0) ? 
                sc_time(m_total_latency.to_seconds() / m_total_responses, SC_SEC) : SC_ZERO_TIME;
            
            // Report latency statistics
            std::cout << "\n" << std::string(60, '=') << std::endl;
            std::cout << sc_time_stamp() << " | " << m_profiler_name << " LATENCY REPORT" << std::endl;
            std::cout << std::string(60, '=') << std::endl;
            std::cout << "  Period responses: " << m_current_period_latencies.size() << std::endl;
            std::cout << "  Period avg latency: " << std::fixed << std::setprecision(1) 
                      << period_avg.to_seconds() * 1000000000 << " ns" << std::endl;
            std::cout << "  Period min latency: " << std::fixed << std::setprecision(1) 
                      << period_min.to_seconds() * 1000000000 << " ns" << std::endl;
            std::cout << "  Period max latency: " << std::fixed << std::setprecision(1) 
                      << period_max.to_seconds() * 1000000000 << " ns" << std::endl;
            std::cout << "  Period median latency: " << std::fixed << std::setprecision(1) 
                      << period_median.to_seconds() * 1000000000 << " ns" << std::endl;
            std::cout << "  Period 95th percentile: " << std::fixed << std::setprecision(1) 
                      << period_p95.to_seconds() * 1000000000 << " ns" << std::endl;
            std::cout << "  Period 99th percentile: " << std::fixed << std::setprecision(1) 
                      << period_p99.to_seconds() * 1000000000 << " ns" << std::endl;
            std::cout << "  Period std deviation: " << std::fixed << std::setprecision(1) 
                      << period_stddev << " ns" << std::endl;
            std::cout << "  Overall avg latency: " << std::fixed << std::setprecision(1) 
                      << overall_avg.to_seconds() * 1000000000 << " ns" << std::endl;
            std::cout << "  Total requests: " << m_total_requests << std::endl;
            std::cout << "  Total responses: " << m_total_responses << std::endl;
            std::cout << "  Pending requests: " << m_request_timestamps.size() << std::endl;
            std::cout << std::string(60, '=') << std::endl;
            
            // Reset period counters
            m_current_period_latencies.clear();
        }
        
        m_last_report_time = current_time;
    }
    
    // Extract index from packet for tracking
    unsigned int get_packet_index(const PacketType& packet) {
        try {
            return static_cast<unsigned int>(packet.get_attribute("index"));
        } catch (...) {
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | " << m_profiler_name 
                          << ": Warning - index attribute not found, using default 0" << std::endl;
            }
            return 0; // Default index if not found
        }
    }
};

// Type aliases for common packet types
using BasePacketProfilerLatency = ProfilerLatency<BasePacket>;

#endif