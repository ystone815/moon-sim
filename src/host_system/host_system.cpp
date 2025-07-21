#include "host_system/host_system.h"
#include "common/common_utils.h"

// Constructor with default host system config file
HostSystem::HostSystem(sc_module_name name, const std::string& config_file_path)
    : sc_module(name) {
    JsonConfig config(config_file_path);
    configure_components(config, config_file_path);
}

void HostSystem::configure_components(const JsonConfig& config, const std::string& config_file_path) {
    // Create internal FIFO for TrafficGenerator -> IndexAllocator communication
    m_internal_fifo = std::unique_ptr<sc_fifo<std::shared_ptr<BasePacket>>>(
        new sc_fifo<std::shared_ptr<BasePacket>>(2));
    
    // Extract config directory from the host system config path
    std::string config_dir = "config/base/";
    size_t last_slash = config_file_path.find_last_of('/');
    if (last_slash != std::string::npos) {
        config_dir = config_file_path.substr(0, last_slash + 1);
    }
    
    // Create TrafficGenerator with config from same directory
    m_traffic_generator = std::unique_ptr<TrafficGenerator>(
        new TrafficGenerator("traffic_generator", config_dir + "traffic_generator_config.json"));
    
    // Extract IndexAllocator configuration from host system config (using leaf keys)
    unsigned int max_index = config.get_int("max_index", 1024);
    bool ia_debug = config.get_bool("debug_enable", false);
    
    // Create IndexAllocator with custom index setter
    m_index_allocator = std::unique_ptr<IndexAllocator<BasePacket>>(
        new IndexAllocator<BasePacket>(
            "index_allocator", 
            max_index, 
            create_index_setter(),
            ia_debug
        ));
    
    // Connect components:
    // TrafficGenerator -> internal_fifo -> IndexAllocator -> out
    // release_in -> IndexAllocator (for index deallocation)
    m_traffic_generator->out(*m_internal_fifo);
    m_index_allocator->in(*m_internal_fifo);
    m_index_allocator->out(out);
    m_index_allocator->release_in(release_in);
    
    std::cout << "HostSystem: Configured with TrafficGenerator and IndexAllocator" << std::endl;
    std::cout << "  IndexAllocator max_index: " << max_index << std::endl;
}

std::function<void(BasePacket&, unsigned int)> HostSystem::create_index_setter() {
    return [](BasePacket& packet, unsigned int index) {
        packet.set_attribute("index", static_cast<double>(index));
    };
}