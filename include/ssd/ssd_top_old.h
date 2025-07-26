#ifndef SSD_TOP_H
#define SSD_TOP_H

#include <systemc.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include "packet/base_packet.h"
#include "packet/pcie_packet.h"
#include "common/json_config.h"
#include "common/error_handling.h"

// SSD Cache configuration
struct SSDCacheConfig {
    uint32_t cache_size_mb;          // Cache size in MB
    uint32_t cache_line_size;        // Cache line size in bytes
    uint32_t associativity;          // N-way associativity
    double hit_latency_ns;           // Cache hit latency
    double miss_penalty_ns;          // Cache miss penalty
    std::string replacement_policy;   // LRU, LFU, FIFO
    bool write_through;              // Write-through vs write-back
    bool enable_prefetch;            // Enable prefetching
    
    SSDCacheConfig() : cache_size_mb(256), cache_line_size(4096), associativity(8),
                      hit_latency_ns(50.0), miss_penalty_ns(500.0), 
                      replacement_policy("LRU"), write_through(false), enable_prefetch(true) {}
};

// SSD DRAM configuration  
struct SSDDRAMConfig {
    uint32_t dram_size_gb;           // DRAM size in GB
    double read_latency_ns;          // DRAM read latency
    double write_latency_ns;         // DRAM write latency
    double refresh_interval_ns;      // DRAM refresh interval
    uint32_t bandwidth_gbps;         // DRAM bandwidth in Gbps
    bool enable_ecc;                 // Enable ECC
    std::string dram_type;           // DDR4, DDR5, LPDDR5
    
    SSDDRAMConfig() : dram_size_gb(4), read_latency_ns(100.0), write_latency_ns(120.0),
                     refresh_interval_ns(7800000.0), bandwidth_gbps(25), enable_ecc(true), 
                     dram_type("DDR4") {}
};

// SSD Flash configuration
struct SSDFlashConfig {
    uint32_t flash_size_gb;          // Flash capacity in GB
    uint32_t num_channels;           // Number of Flash channels
    uint32_t dies_per_channel;       // Dies per channel
    uint32_t planes_per_die;         // Planes per die
    uint32_t blocks_per_plane;       // Blocks per plane
    uint32_t pages_per_block;        // Pages per block
    uint32_t page_size_kb;           // Page size in KB
    
    SSDFlashConfig() : flash_size_gb(1024), num_channels(8), dies_per_channel(4),
                      planes_per_die(4), blocks_per_plane(1024), pages_per_block(128), 
                      page_size_kb(16) {}
};

// Simplified SSD Top Level Module
template<typename PacketType = BasePacket>
SC_MODULE(SSDTop) {
    SC_HAS_PROCESS(SSDTop);
    
    // PCIe interface ports
    sc_fifo_in<std::shared_ptr<PacketType>> pcie_in;     // From Host via PCIe
    sc_fifo_out<std::shared_ptr<PacketType>> pcie_out;   // To Host via PCIe
    
    // Internal configuration
    const bool m_debug_enable;
    const std::string m_config_file;
    
    // SSD component configurations
    SSDCacheConfig m_cache_config;
    SSDDRAMConfig m_dram_config;
    SSDFlashConfig m_flash_config;
    
    // Statistics
    uint64_t m_total_requests;
    uint64_t m_cache_hits;
    uint64_t m_cache_misses;
    uint64_t m_dram_accesses;
    uint64_t m_flash_reads;
    uint64_t m_flash_writes;
    
    // Simple hash-based cache simulation (no actual SystemC Memory components)
    std::unordered_map<uint64_t, std::shared_ptr<PacketType>> m_cache_data;
    std::unordered_map<uint64_t, std::shared_ptr<PacketType>> m_dram_data;
    
    // Main SSD processing thread
    void ssd_main_process() {
        while (true) {
            // Read request from PCIe interface
            auto packet = pcie_in.read();
            
            if (!packet) {
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | SSDTop: Received null packet, skipping" << std::endl;
                }
                continue;
            }
            
            m_total_requests++;
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | SSDTop: Processing request: " 
                          << *packet << std::endl;
            }
            
            // Process request through SSD hierarchy
            process_request(packet);
        }
    }
    
    // Process request through Cache -> DRAM -> Flash hierarchy
    void process_request(std::shared_ptr<PacketType> packet) {
        uint64_t address = packet->get_address();
        uint64_t cache_line_addr = address / m_cache_config.cache_line_size;
        
        // Step 1: Check Cache
        bool cache_hit = check_cache(cache_line_addr);
        
        if (cache_hit) {
            // Cache Hit - serve from cache
            handle_cache_hit(packet, cache_line_addr);
        } else {
            // Cache Miss - check DRAM
            handle_cache_miss(packet, cache_line_addr);
        }
        
        // Send response back to host
        pcie_out.write(packet);
    }
    
    // Check if address is in cache
    bool check_cache(uint64_t cache_line_addr) {
        // Simulate cache access latency
        wait(m_cache_config.hit_latency_ns, SC_NS);
        
        // Simple cache simulation with 80% hit rate
        static uint32_t access_count = 0;
        access_count++;
        
        // Check if data is actually in cache
        bool in_cache = (m_cache_data.find(cache_line_addr) != m_cache_data.end());
        
        // Simulate realistic hit rate (80% for sequential, lower for random)
        bool hit = in_cache || ((access_count % 5) != 0);
        
        return hit;
    }
    
    // Handle cache hit
    void handle_cache_hit(std::shared_ptr<PacketType> packet, uint64_t cache_line_addr) {
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | SSDTop: Cache HIT for address 0x" 
                      << std::hex << cache_line_addr << std::dec << std::endl;
        }
        
        m_cache_hits++;
        
        // Update cache with packet data
        m_cache_data[cache_line_addr] = packet;
        
        // For writes, may need write-through to DRAM
        if (packet->get_command() == Command::WRITE && m_cache_config.write_through) {
            // Simulate write-through latency
            wait(m_dram_config.write_latency_ns, SC_NS);
            m_dram_data[cache_line_addr] = packet;
        }
    }
    
    // Handle cache miss
    void handle_cache_miss(std::shared_ptr<PacketType> packet, uint64_t cache_line_addr) {
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | SSDTop: Cache MISS for address 0x"
                      << std::hex << cache_line_addr << std::dec 
                      << ", checking DRAM" << std::endl;
        }
        
        m_cache_misses++;
        
        // Apply cache miss penalty
        wait(m_cache_config.miss_penalty_ns, SC_NS);
        
        // Step 2: Check DRAM buffer
        bool dram_hit = check_dram_buffer(cache_line_addr);
        
        if (dram_hit) {
            handle_dram_hit(packet, cache_line_addr);
        } else {
            handle_dram_miss(packet, cache_line_addr);
        }
        
        // Update cache with new data
        m_cache_data[cache_line_addr] = packet;
    }
    
    // Check DRAM buffer
    bool check_dram_buffer(uint64_t cache_line_addr) {
        // Simulate DRAM access latency
        wait(m_dram_config.read_latency_ns, SC_NS);
        m_dram_accesses++;
        
        // Check if data is in DRAM buffer
        bool in_dram = (m_dram_data.find(cache_line_addr) != m_dram_data.end());
        
        // Simulate 90% DRAM buffer hit rate
        static uint32_t dram_access_count = 0;
        dram_access_count++;
        bool hit = in_dram || ((dram_access_count % 10) != 0);
        
        return hit;
    }
    
    // Handle DRAM buffer hit
    void handle_dram_hit(std::shared_ptr<PacketType> packet, uint64_t cache_line_addr) {
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | SSDTop: DRAM buffer HIT" << std::endl;
        }
        
        // Update DRAM with packet data
        m_dram_data[cache_line_addr] = packet;
        
        // Simulate cache line fill time
        wait(m_cache_config.cache_line_size / 1000.0, SC_NS);
    }
    
    // Handle DRAM buffer miss - need Flash access
    void handle_dram_miss(std::shared_ptr<PacketType> packet, uint64_t cache_line_addr) {
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | SSDTop: DRAM buffer MISS, accessing Flash"
                      << std::endl;
        }
        
        // Simulate Flash access with realistic latencies
        if (packet->get_command() == Command::READ) {
            // Flash read latency: 25μs
            wait(25000, SC_NS);
            m_flash_reads++;
        } else {
            // Flash program latency: 200μs
            wait(200000, SC_NS);
            m_flash_writes++;
        }
        
        // Update DRAM buffer with new data from Flash
        m_dram_data[cache_line_addr] = packet;
        
        // Simulate data transfer from Flash to DRAM
        wait(m_dram_config.write_latency_ns, SC_NS);
    }
    
    // Load configuration from JSON file
    void load_configuration() {
        try {
            JsonConfig config(m_config_file);
            
            // Load cache configuration (nested under ssd.cache)
            m_cache_config.cache_size_mb = config.get_int("ssd.cache.cache_size_mb", 256);
            m_cache_config.hit_latency_ns = config.get_double("ssd.cache.hit_latency_ns", 50.0);
            m_cache_config.miss_penalty_ns = config.get_double("ssd.cache.miss_penalty_ns", 500.0);
            std::string write_policy = config.get_string("ssd.cache.write_policy", "write_back");
            m_cache_config.write_through = (write_policy == "write_through");
            
            // Load DRAM configuration (nested under ssd.dram)
            m_dram_config.dram_size_gb = config.get_int("ssd.dram.dram_size_gb", 4);
            m_dram_config.read_latency_ns = config.get_double("ssd.dram.read_latency_ns", 100.0);
            m_dram_config.write_latency_ns = config.get_double("ssd.dram.write_latency_ns", 120.0);
            
            // Load Flash configuration (nested under ssd.flash)
            m_flash_config.flash_size_gb = config.get_int("ssd.flash.flash_size_gb", 1024);
            m_flash_config.num_channels = config.get_int("ssd.flash.num_channels", 8);
            
            if (m_debug_enable) {
                std::cout << "SSD Configuration loaded:" << std::endl;
                std::cout << "  Cache: " << m_cache_config.cache_size_mb << "MB" << std::endl;
                std::cout << "  DRAM: " << m_dram_config.dram_size_gb << "GB" << std::endl;
                std::cout << "  Flash: " << m_flash_config.flash_size_gb << "GB" << std::endl;
            }
            
        } catch (const std::exception& e) {
            if (m_debug_enable) {
                std::cout << "Warning: Could not load SSD config, using defaults: " 
                          << e.what() << std::endl;
            }
        }
    }
    
    // Constructor
    SSDTop(sc_module_name name, const std::string& config_file = "config/base/ssd_config.json", 
           bool debug_enable = false)
        : sc_module(name),
          m_debug_enable(debug_enable),
          m_config_file(config_file),
          m_total_requests(0),
          m_cache_hits(0),
          m_cache_misses(0),
          m_dram_accesses(0),
          m_flash_reads(0),
          m_flash_writes(0) {
        
        // Load configuration
        load_configuration();
        
        if (m_debug_enable) {
            std::cout << "0 s | " << basename() << ": SSD Top initialized"
                      << " (Cache: " << m_cache_config.cache_size_mb << "MB"
                      << ", DRAM: " << m_dram_config.dram_size_gb << "GB"
                      << ", Flash: " << m_flash_config.flash_size_gb << "GB)" << std::endl;
        }
        
        // Start main processing thread
        SC_THREAD(ssd_main_process);
    }
    
public:
    // Statistics methods
    uint64_t get_total_requests() const { return m_total_requests; }
    uint64_t get_cache_hits() const { return m_cache_hits; }
    uint64_t get_cache_misses() const { return m_cache_misses; }
    double get_cache_hit_rate() const { 
        return (m_total_requests > 0) ? (static_cast<double>(m_cache_hits) / m_total_requests) : 0.0; 
    }
    
    uint64_t get_dram_accesses() const { return m_dram_accesses; }
    uint64_t get_flash_reads() const { return m_flash_reads; }
    uint64_t get_flash_writes() const { return m_flash_writes; }
    
    // Configuration access
    const SSDCacheConfig& get_cache_config() const { return m_cache_config; }
    const SSDDRAMConfig& get_dram_config() const { return m_dram_config; }
    const SSDFlashConfig& get_flash_config() const { return m_flash_config; }
    
    // Performance metrics
    void print_statistics() const {
        std::cout << "\n========== SSD Statistics ==========" << std::endl;
        std::cout << "Total Requests: " << m_total_requests << std::endl;
        std::cout << "Cache Hits: " << m_cache_hits << " (" 
                  << std::fixed << std::setprecision(1) << get_cache_hit_rate() * 100.0 << "%)" << std::endl;
        std::cout << "Cache Misses: " << m_cache_misses << std::endl;
        std::cout << "DRAM Accesses: " << m_dram_accesses << std::endl;
        std::cout << "Flash Reads: " << m_flash_reads << std::endl;
        std::cout << "Flash Writes: " << m_flash_writes << std::endl;
        std::cout << "====================================" << std::endl;
    }
};

// Type aliases for common configurations
using BasePacketSSDTop = SSDTop<BasePacket>;
using PCIePacketSSDTop = SSDTop<PCIePacket>;

#endif // SSD_TOP_H