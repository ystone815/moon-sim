#ifndef HOST_SYSTEM_H
#define HOST_SYSTEM_H

#include <systemc.h>
#include <memory>
#include "base/traffic_generator.h"
#include "base/index_allocator.h"
#include "base/function_profiler.h"
#include "packet/base_packet.h"
#include "common/json_config.h"

// Forward declaration
class JsonConfig;

SC_MODULE(HostSystem) {
    SC_HAS_PROCESS(HostSystem);
    
    // External interface - output to the rest of the system
    sc_fifo_out<std::shared_ptr<BasePacket>> out;
    
    // External interface - input from Memory for index release
    sc_fifo_in<std::shared_ptr<BasePacket>> release_in;

    // Constructor with config file path and optional traffic generator config directory
    HostSystem(sc_module_name name, const std::string& config_file_path = "config/base/host_system_config.json");

private:
    // Internal components
    std::unique_ptr<TrafficGenerator> m_traffic_generator;
    std::unique_ptr<IndexAllocator<BasePacket>> m_index_allocator;
    std::unique_ptr<FunctionProfiler<BasePacket>> m_profiler;
    
    // Internal FIFO connection between TrafficGenerator and IndexAllocator
    std::unique_ptr<sc_fifo<std::shared_ptr<BasePacket>>> m_internal_fifo;
    
    // Internal FIFO for profiling (IndexAllocator -> Profiler -> out)
    std::unique_ptr<sc_fifo<std::shared_ptr<BasePacket>>> m_profiling_fifo;
    
    // Configuration
    void configure_components(const JsonConfig& config, const std::string& config_file_path);
    
    // Process for profiling outgoing packets
    void profiling_process();
    
    // Helper method to create index setter function for IndexAllocator
    std::function<void(BasePacket&, unsigned int)> create_index_setter();
};

#endif