#include <systemc.h>
#include <iostream>
#include <memory>
#include <fstream>
#include <ctime>
#include "base/cache_l1.h"
#include "base/dram_controller.h"
#include "base/memory_hierarchy.h"
#include "base/traffic_generator.h"
#include "base/profiler_latency.h"
#include "common/json_config.h"

// Test bench for cache hierarchy
SC_MODULE(CacheTestBench) {
    SC_HAS_PROCESS(CacheTestBench);
    
    // Components
    TrafficGenerator* m_traffic_gen;
    MemoryHierarchy<32, 64, 4, 8, 1, 65536>* m_memory_hierarchy;
    
    // FIFOs for direct connection (simplified for testing)
    sc_fifo<std::shared_ptr<BasePacket>>* m_cpu_to_mem_fifo;
    sc_fifo<std::shared_ptr<BasePacket>>* m_mem_to_cpu_fifo;
    
    CacheTestBench(sc_module_name name, const std::string& config_dir)
        : sc_module(name)
    {
        // Load configurations
        JsonConfig traffic_config(config_dir + "/traffic_generator_config.json");
        JsonConfig cache_config(config_dir + "/cache_config.json");
        
        // Create FIFOs for direct connection
        m_cpu_to_mem_fifo = new sc_fifo<std::shared_ptr<BasePacket>>(32);
        m_mem_to_cpu_fifo = new sc_fifo<std::shared_ptr<BasePacket>>(32);
        
        // Create components
        m_traffic_gen = new TrafficGenerator("TrafficGen", traffic_config);
        m_memory_hierarchy = new MemoryHierarchy<32, 64, 4, 8, 1, 65536>("MemHierarchy", cache_config);
        
        // Direct connection: TrafficGen -> MemoryHierarchy -> TrafficGen (simplified for testing)
        m_traffic_gen->out(*m_cpu_to_mem_fifo);
        m_memory_hierarchy->cpu_in(*m_cpu_to_mem_fifo);
        m_memory_hierarchy->cpu_out(*m_mem_to_cpu_fifo);
        
        std::cout << "CacheTestBench: Initialized with cache hierarchy testing" << std::endl;
        
        SC_THREAD(monitor_process);
    }
    
    ~CacheTestBench() {
        delete m_traffic_gen;
        delete m_memory_hierarchy;
        delete m_cpu_to_mem_fifo;
        delete m_mem_to_cpu_fifo;
    }
    
    void print_final_stats() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Cache Hierarchy Test Results" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // Print memory hierarchy statistics
        m_memory_hierarchy->print_hierarchy_stats();
    }

private:
    void monitor_process() {
        // Wait for simulation to complete
        wait(sc_time(10, SC_SEC)); // 10 second timeout
        
        std::cout << sc_time_stamp() << " | CacheTestBench: Simulation timeout reached" << std::endl;
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    // Get configuration directory
    std::string config_dir = "config/base";
    if (argc > 1) {
        config_dir = argv[1];
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "SystemC Cache Hierarchy Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Config directory: " << config_dir << std::endl;
    std::cout << "Starting simulation..." << std::endl;
    std::cout << std::endl;
    
    // Create test bench
    CacheTestBench* tb = new CacheTestBench("CacheTestBench", config_dir);
    
    // Run simulation
    sc_start();
    
    std::cout << "Info: /OSCI/SystemC: Simulation stopped by user." << std::endl;
    std::cout << std::endl;
    
    // Print final statistics
    tb->print_final_stats();
    
    // Cleanup
    delete tb;
    
    std::cout << "Simulation finished." << std::endl;
    
    // Generate standardized metric files for sweep collection
    std::ofstream metrics_csv("metrics.csv");
    if (metrics_csv.is_open()) {
        metrics_csv << "metric,value,unit\n";
        metrics_csv << "simulation_target,cache_test,type\n";
        metrics_csv << "status,completed,state\n";
        metrics_csv.close();
    }
    
    std::ofstream performance_json("performance.json");
    if (performance_json.is_open()) {
        performance_json << "{\n";
        performance_json << "  \"simulation\": {\n";
        performance_json << "    \"target\": \"cache_test\",\n";
        performance_json << "    \"timestamp\": \"" << std::time(nullptr) << "\",\n";
        performance_json << "    \"status\": \"completed\"\n";
        performance_json << "  }\n";
        performance_json << "}\n";
        performance_json.close();
    }
    
    return 0;
}