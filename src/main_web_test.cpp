#include <systemc.h>
#include <iostream>
#include <memory>
#include "base/cache_l1.h"
#include "base/dram_controller.h"
#include "base/memory_hierarchy.h"
#include "base/traffic_generator.h"
#include "base/web_profiler.h"
#include "common/json_config.h"

// Test bench with web monitoring
SC_MODULE(WebMonitorTestBench) {
    SC_HAS_PROCESS(WebMonitorTestBench);
    
    // Components
    TrafficGenerator* m_traffic_gen;
    MemoryHierarchy<32, 64, 4, 8, 1, 65536>* m_memory_hierarchy;
    WebProfiler<BasePacket>* m_web_profiler;
    
    // FIFOs for connections
    sc_fifo<std::shared_ptr<BasePacket>>* m_cpu_to_mem_fifo;
    sc_fifo<std::shared_ptr<BasePacket>>* m_mem_to_cpu_fifo;
    
    WebMonitorTestBench(sc_module_name name, const std::string& config_dir)
        : sc_module(name)
    {
        // Load configurations
        JsonConfig traffic_config(config_dir + "/traffic_generator_config.json");
        JsonConfig cache_config(config_dir + "/cache_config.json");
        
        // Create FIFOs
        m_cpu_to_mem_fifo = new sc_fifo<std::shared_ptr<BasePacket>>(32);
        m_mem_to_cpu_fifo = new sc_fifo<std::shared_ptr<BasePacket>>(32);
        
        // Create components
        m_traffic_gen = new TrafficGenerator("TrafficGen", traffic_config);
        m_memory_hierarchy = new MemoryHierarchy<32, 64, 4, 8, 1, 65536>("MemHierarchy", cache_config);
        m_web_profiler = new WebProfiler<BasePacket>("WebProfiler", 
                                                     "web_monitor/metrics.json",
                                                     sc_time(2, SC_SEC), // Update every 2 seconds
                                                     true); // Enable debug
        
        // Connect components
        m_traffic_gen->out(*m_cpu_to_mem_fifo);
        m_memory_hierarchy->cpu_in(*m_cpu_to_mem_fifo);
        m_memory_hierarchy->cpu_out(*m_mem_to_cpu_fifo);
        
        std::cout << "WebMonitorTestBench: Initialized with web monitoring" << std::endl;
        std::cout << "Web dashboard available at: http://localhost:5000" << std::endl;
        std::cout << "Metrics file: web_monitor/metrics.json" << std::endl;
        
        SC_THREAD(monitor_process);
        SC_THREAD(packet_monitor_process);
    }
    
    ~WebMonitorTestBench() {
        delete m_traffic_gen;
        delete m_memory_hierarchy;
        delete m_web_profiler;
        delete m_cpu_to_mem_fifo;
        delete m_mem_to_cpu_fifo;
    }
    
    void print_final_stats() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "Web Monitor Test Results" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // Print memory hierarchy statistics
        m_memory_hierarchy->print_hierarchy_stats();
        
        // Force final metrics update
        m_web_profiler->force_update();
        
        std::cout << "\nWeb monitoring completed. Check web_monitor/metrics.json for final data." << std::endl;
    }

private:
    void monitor_process() {
        // Run for 30 seconds or until simulation completes
        wait(sc_time(30, SC_SEC));
        
        std::cout << sc_time_stamp() << " | WebMonitorTestBench: Simulation timeout reached" << std::endl;
        sc_stop();
    }
    
    void packet_monitor_process() {
        // Monitor packets passing through and profile them
        while (true) {
            // Wait for packets to be available
            wait(sc_time(500, SC_MS));
            
            // Simulate packet profiling
            // In a real implementation, this would be triggered by actual packet flows
            if (sc_time_stamp() > sc_time(1, SC_SEC)) {
                // Create a dummy packet for profiling demonstration
                auto dummy_packet = std::make_shared<GenericPacket>();
                m_web_profiler->profile_packet(*dummy_packet, "TrafficGenerator");
            }
        }
    }
};

int sc_main(int argc, char* argv[]) {
    // Get configuration directory
    std::string config_dir = "config/base";
    if (argc > 1) {
        config_dir = argv[1];
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "SystemC Web Monitoring Test" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Config directory: " << config_dir << std::endl;
    std::cout << "Starting simulation with real-time web monitoring..." << std::endl;
    std::cout << std::endl;
    
    // Instructions for user
    std::cout << "📊 REAL-TIME MONITORING:" << std::endl;
    std::cout << "1. Start the web server: cd web_monitor && python3 app.py" << std::endl;
    std::cout << "2. Open browser: http://localhost:5000" << std::endl;
    std::cout << "3. Watch real-time performance metrics!" << std::endl;
    std::cout << std::endl;
    
    // Create test bench
    WebMonitorTestBench* tb = new WebMonitorTestBench("WebMonitorTestBench", config_dir);
    
    // Run simulation
    sc_start();
    
    std::cout << "Info: /OSCI/SystemC: Simulation stopped by user." << std::endl;
    std::cout << std::endl;
    
    // Print final statistics
    tb->print_final_stats();
    
    // Cleanup
    delete tb;
    
    std::cout << "Simulation finished." << std::endl;
    
    return 0;
}