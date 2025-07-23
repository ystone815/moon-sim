#ifndef WEB_PROFILER_H
#define WEB_PROFILER_H

#include <systemc.h>
#include <memory>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "packet/base_packet.h"
#include "cache_l1.h"
#include "dram_controller.h"

// Web-based real-time profiler for SystemC simulations
template<typename PacketType>
SC_MODULE(WebProfiler) {
    SC_HAS_PROCESS(WebProfiler);
    
    // Configuration
    const std::string m_output_file;
    const sc_time m_update_interval;
    const bool m_debug_enable;
    
    // Constructor
    WebProfiler(sc_module_name name,
               const std::string& output_file = "web_monitor/metrics.json",
               sc_time update_interval = sc_time(1, SC_SEC),
               bool debug_enable = false)
        : sc_module(name),
          m_output_file(output_file),
          m_update_interval(update_interval),
          m_debug_enable(debug_enable),
          m_packet_count(0),
          m_total_bandwidth(0),
          m_last_update_time(SC_ZERO_TIME)
    {
        // Initialize metrics
        reset_metrics();
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | WebProfiler: Initialized with output=" 
                      << m_output_file << ", interval=" << m_update_interval << std::endl;
        }
        
        SC_THREAD(update_process);
    }
    
    // Register cache for monitoring
    template<int CACHE_SIZE_KB, int LINE_SIZE, int WAYS>
    void register_cache(CacheL1<CACHE_SIZE_KB, LINE_SIZE, WAYS>* cache, const std::string& name) {
        m_caches[name] = reinterpret_cast<void*>(cache);
        m_cache_types[name] = "L1";
    }
    
    // Register DRAM controller for monitoring
    template<int NUM_BANKS, int NUM_RANKS>
    void register_dram(DramController<NUM_BANKS, NUM_RANKS>* dram, const std::string& name) {
        m_dram_controllers[name] = reinterpret_cast<void*>(dram);
    }
    
    // Profile packet processing (called by components)
    void profile_packet(const PacketType& packet, const std::string& component_name) {
        m_packet_count++;
        m_component_packets[component_name]++;
        
        // Calculate bandwidth (assuming 64-byte packets)
        m_total_bandwidth += 64;
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | WebProfiler: Profiled packet from " 
                      << component_name << std::endl;
        }
    }
    
    // Manual trigger for metrics update (for testing)
    void force_update() {
        update_metrics();
    }

private:
    // Metrics storage
    uint64_t m_packet_count;
    uint64_t m_total_bandwidth;
    std::map<std::string, uint64_t> m_component_packets;
    std::map<std::string, void*> m_caches;
    std::map<std::string, std::string> m_cache_types;
    std::map<std::string, void*> m_dram_controllers;
    sc_time m_last_update_time;
    
    // Update process - runs periodically
    void update_process() {
        while (true) {
            wait(m_update_interval);
            update_metrics();
        }
    }
    
    // Reset metrics for next period
    void reset_metrics() {
        m_packet_count = 0;
        m_total_bandwidth = 0;
        m_component_packets.clear();
    }
    
    // Update and output metrics to JSON file
    void update_metrics() {
        sc_time current_time = sc_time_stamp();
        double elapsed_seconds = (current_time - m_last_update_time).to_seconds();
        
        if (elapsed_seconds <= 0) {
            elapsed_seconds = m_update_interval.to_seconds();
        }
        
        // Calculate rates
        double packet_rate = m_packet_count / elapsed_seconds;
        double bandwidth_mbps = (m_total_bandwidth * 8.0) / (elapsed_seconds * 1e6);
        
        // Generate JSON output
        std::string json_output = generate_json_metrics(current_time, packet_rate, bandwidth_mbps);
        
        // Write to file
        write_json_file(json_output);
        
        // Reset for next period
        m_last_update_time = current_time;
        reset_metrics();
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | WebProfiler: Updated metrics - " 
                      << packet_rate << " pps, " << bandwidth_mbps << " Mbps" << std::endl;
        }
    }
    
    // Generate JSON metrics string
    std::string generate_json_metrics(sc_time timestamp, double packet_rate, double bandwidth_mbps) {
        std::ostringstream json;
        json << std::fixed << std::setprecision(2);
        
        // Get current timestamp in milliseconds
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        json << "{\\n";
        json << "  \"timestamp\": " << ms << ",\\n";
        json << "  \"simulation_time_ns\": " << timestamp.to_seconds() * 1e9 << ",\\n";
        json << "  \"metrics\": {\\n";
        
        // Overall performance
        json << "    \"performance\": {\\n";
        json << "      \"packet_rate_pps\": " << packet_rate << ",\\n";
        json << "      \"bandwidth_mbps\": " << bandwidth_mbps << ",\\n";
        json << "      \"total_packets\": " << m_packet_count << "\\n";
        json << "    },\\n";
        
        // Cache statistics
        json << "    \"caches\": {\\n";
        bool first_cache = true;
        for (const auto& cache_pair : m_caches) {
            if (!first_cache) json << ",\\n";
            first_cache = false;
            
            json << "      \"" << cache_pair.first << "\": ";
            json << get_cache_json(cache_pair.second, cache_pair.first);
        }
        json << "\\n    },\\n";
        
        // DRAM statistics
        json << "    \"dram\": {\\n";
        bool first_dram = true;
        for (const auto& dram_pair : m_dram_controllers) {
            if (!first_dram) json << ",\\n";
            first_dram = false;
            
            json << "      \"" << dram_pair.first << "\": ";
            json << get_dram_json(dram_pair.second, dram_pair.first);
        }
        json << "\\n    },\\n";
        
        // Component breakdown
        json << "    \"components\": {\\n";
        bool first_comp = true;
        for (const auto& comp_pair : m_component_packets) {
            if (!first_comp) json << ",\\n";
            first_comp = false;
            
            json << "      \"" << comp_pair.first << "\": " << comp_pair.second;
        }
        json << "\\n    }\\n";
        
        json << "  }\\n";
        json << "}";
        
        return json.str();
    }
    
    // Get cache statistics as JSON string
    std::string get_cache_json(void* cache_ptr, const std::string& name) {
        std::ostringstream json;
        json << std::fixed << std::setprecision(2);
        
        // For now, return placeholder data
        // In real implementation, we'd cast cache_ptr and get actual stats
        json << "{\\n";
        json << "        \"type\": \"L1\",\\n";
        json << "        \"size_kb\": 32,\\n";
        json << "        \"hits\": 0,\\n";
        json << "        \"misses\": 0,\\n";
        json << "        \"hit_rate\": 0.0,\\n";
        json << "        \"evictions\": 0,\\n";
        json << "        \"writebacks\": 0\\n";
        json << "      }";
        
        return json.str();
    }
    
    // Get DRAM statistics as JSON string
    std::string get_dram_json(void* dram_ptr, const std::string& name) {
        std::ostringstream json;
        json << std::fixed << std::setprecision(2);
        
        // For now, return placeholder data
        // In real implementation, we'd cast dram_ptr and get actual stats
        json << "{\\n";
        json << "        \"total_requests\": 0,\\n";
        json << "        \"read_requests\": 0,\\n";
        json << "        \"write_requests\": 0,\\n";
        json << "        \"row_hits\": 0,\\n";
        json << "        \"row_misses\": 0,\\n";
        json << "        \"row_hit_rate\": 0.0,\\n";
        json << "        \"bank_conflicts\": 0,\\n";
        json << "        \"avg_read_latency_ns\": 0.0,\\n";
        json << "        \"avg_write_latency_ns\": 0.0\\n";
        json << "      }";
        
        return json.str();
    }
    
    // Write JSON to file
    void write_json_file(const std::string& json_content) {
        // Create directory if it doesn't exist
        std::string dir_path = m_output_file.substr(0, m_output_file.find_last_of('/'));
        if (!dir_path.empty()) {
            std::string mkdir_cmd = "mkdir -p " + dir_path;
            system(mkdir_cmd.c_str());
        }
        
        std::ofstream file(m_output_file);
        if (file.is_open()) {
            file << json_content;
            file.close();
        } else if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | WebProfiler: Failed to write " << m_output_file << std::endl;
        }
    }
};

#endif