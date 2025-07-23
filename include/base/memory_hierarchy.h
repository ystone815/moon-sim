#ifndef MEMORY_HIERARCHY_H
#define MEMORY_HIERARCHY_H

#include <systemc.h>
#include <memory>
#include <iomanip>
#include "cache_l1.h"
#include "dram_controller.h"
#include "memory.h"
#include "common/json_config.h"

// Memory hierarchy that connects L1 Cache -> DRAM Controller -> Memory
template<int L1_SIZE_KB = 32, int L1_LINE_SIZE = 64, int L1_WAYS = 4,
         int DRAM_BANKS = 8, int DRAM_RANKS = 1,
         int MEM_SIZE = 65536>
SC_MODULE(MemoryHierarchy) {
    SC_HAS_PROCESS(MemoryHierarchy);
    
    // External ports (connect to HostSystem)
    sc_fifo_in<std::shared_ptr<BasePacket>> cpu_in;    // From CPU/HostSystem
    sc_fifo_out<std::shared_ptr<BasePacket>> cpu_out;  // To CPU/HostSystem
    
    // Internal components
    CacheL1<L1_SIZE_KB, L1_LINE_SIZE, L1_WAYS>* m_l1_cache;
    DramController<DRAM_BANKS, DRAM_RANKS>* m_dram_controller;
    // Note: For now, DRAM Controller acts as final storage
    // Memory<PacketType, DataType, MEM_SIZE> would need template updates
    
    // Internal FIFOs for component connections
    sc_fifo<std::shared_ptr<BasePacket>>* m_l1_to_dram_fifo;
    sc_fifo<std::shared_ptr<BasePacket>>* m_dram_to_l1_fifo;
    
    // Configuration
    const bool m_debug_enable;
    
    // Constructor with individual configurations
    MemoryHierarchy(sc_module_name name,
                   const JsonConfig& l1_config,
                   const JsonConfig& dram_config,
                   bool debug_enable = false)
        : sc_module(name), m_debug_enable(debug_enable)
    {
        // Create internal FIFOs
        m_l1_to_dram_fifo = new sc_fifo<std::shared_ptr<BasePacket>>(16);
        m_dram_to_l1_fifo = new sc_fifo<std::shared_ptr<BasePacket>>(16);
        
        // Create L1 Cache
        m_l1_cache = new CacheL1<L1_SIZE_KB, L1_LINE_SIZE, L1_WAYS>("L1_Cache", l1_config);
        
        // Create DRAM Controller
        m_dram_controller = new DramController<DRAM_BANKS, DRAM_RANKS>("DRAM_Controller", dram_config);
        
        // Connect components
        connect_components();
        
        if (m_debug_enable) {
            std::cout << "MemoryHierarchy: Initialized with L1(" << L1_SIZE_KB 
                      << "KB) -> DRAM(" << DRAM_BANKS << " banks)" << std::endl;
        }
    }
    
    // Constructor with unified config
    MemoryHierarchy(sc_module_name name, const JsonConfig& config)
        : MemoryHierarchy(name, 
                         config.get_section("l1_cache"),
                         config.get_section("dram_controller"),
                         config.get_bool("memory_hierarchy.debug_enable", false))
    {
    }
    
    // Destructor
    ~MemoryHierarchy() {
        delete m_l1_cache;
        delete m_dram_controller;
        delete m_l1_to_dram_fifo;
        delete m_dram_to_l1_fifo;
    }
    
    // Print comprehensive statistics
    void print_hierarchy_stats() const {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Memory Hierarchy Performance Report" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // L1 Cache statistics
        m_l1_cache->print_stats();
        std::cout << std::endl;
        
        // DRAM Controller statistics
        m_dram_controller->print_stats();
        std::cout << std::endl;
        
        // Calculate overall metrics
        auto l1_stats = m_l1_cache->get_stats();
        auto dram_stats = m_dram_controller->get_stats();
        
        std::cout << "=== Overall Memory Hierarchy Metrics ===" << std::endl;
        std::cout << "Total CPU requests: " << l1_stats.total_accesses << std::endl;
        std::cout << "L1 hit rate: " << (l1_stats.get_hit_rate() * 100) << "%" << std::endl;
        std::cout << "DRAM row hit rate: " << (dram_stats.get_row_hit_rate() * 100) << "%" << std::endl;
        
        // Estimate average memory access time (AMAT)
        double l1_hit_rate = l1_stats.get_hit_rate();
        double l1_hit_time = 1.0; // 1ns L1 hit time (from config)
        double l1_miss_penalty = dram_stats.get_avg_read_latency().to_seconds() * 1e9; // Convert to ns
        double amat = l1_hit_time + (1.0 - l1_hit_rate) * l1_miss_penalty;
        
        std::cout << "Estimated AMAT (Average Memory Access Time): " 
                  << std::fixed << std::setprecision(2) << amat << " ns" << std::endl;
        std::cout << "========================================\n" << std::endl;
    }

private:
    void connect_components() {
        // Connect CPU interface to L1 Cache
        m_l1_cache->cpu_in(cpu_in);
        m_l1_cache->cpu_out(cpu_out);
        
        // Connect L1 Cache to DRAM Controller
        m_l1_cache->mem_out(*m_l1_to_dram_fifo);
        m_l1_cache->mem_in(*m_dram_to_l1_fifo);
        
        m_dram_controller->mem_in(*m_l1_to_dram_fifo);
        m_dram_controller->mem_out(*m_dram_to_l1_fifo);
    }
};

#endif