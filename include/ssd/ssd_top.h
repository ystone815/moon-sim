#ifndef SSD_TOP_H
#define SSD_TOP_H

#include <systemc.h>
#include <memory>
#include <string>
#include <vector>
#include <iomanip>
#include "packet/base_packet.h"
#include "packet/pcie_packet.h"
#include "packet/flash_packet.h"
#include "common/json_config.h"
#include "common/error_handling.h"

// Include hardware modules
#include "ssd/ssd_controller.h"
#include "ssd/flash_controller.h"
#include "base/cache_l1.h"
#include "base/dram_controller.h"
#include "base/nand_flash.h"

// Hardware-oriented SSD Top Level Module
// Architecture: PCIe → SSD Controller → Cache → DRAM Controller → Flash Controller → NAND Flash
template<typename PacketType = BasePacket>
SC_MODULE(SSDTop) {
    SC_HAS_PROCESS(SSDTop);
    
    // PCIe interface ports (to Host)
    sc_fifo_in<std::shared_ptr<PacketType>> pcie_in;     // From Host via PCIe
    sc_fifo_out<std::shared_ptr<PacketType>> pcie_out;   // To Host via PCIe
    
    // Configuration
    const bool m_debug_enable;
    const std::string m_config_file;
    
    // Hardware Module Instances
    SSDController<PacketType>* m_ssd_controller;
    CacheL1<32, 64, 4>* m_cache_l1;                     // 32KB, 64B line, 4-way
    DramController<8, 1>* m_dram_controller;             // 8 banks, 1 rank
    FlashController<PacketType>* m_flash_controller;
    std::vector<NANDFlash<4, 1024, 128>*> m_nand_flash_devices;
    
    // Inter-module communication FIFOs
    sc_fifo<std::shared_ptr<PacketType>>* m_controller_to_cache;
    sc_fifo<std::shared_ptr<PacketType>>* m_cache_to_controller;
    sc_fifo<std::shared_ptr<BasePacket>>* m_cache_to_dram;
    sc_fifo<std::shared_ptr<BasePacket>>* m_dram_to_cache;
    sc_fifo<std::shared_ptr<PacketType>>* m_dram_to_flash;
    sc_fifo<std::shared_ptr<PacketType>>* m_flash_to_dram;
    
    // Flash channel FIFOs (multiple channels)
    std::vector<sc_fifo<std::shared_ptr<FlashPacket>>*> m_flash_channel_out;
    std::vector<sc_fifo<std::shared_ptr<FlashPacket>>*> m_flash_channel_in;
    
    // Configuration structures
    struct SSDTopConfig {
        uint32_t cache_size_kb;
        uint32_t dram_size_gb;
        uint32_t flash_channels;
        uint32_t fifo_depth;
        bool enable_debug_all_modules;
        
        SSDTopConfig() : cache_size_kb(32), dram_size_gb(4), flash_channels(8),
                        fifo_depth(32), enable_debug_all_modules(false) {}
    } m_config;
    
    // Statistics aggregation
    struct SSDStatistics {
        uint64_t total_requests;
        uint64_t completed_requests;
        uint64_t cache_hits;
        uint64_t cache_misses;
        uint64_t dram_accesses;
        uint64_t flash_reads;
        uint64_t flash_writes;
        double average_latency_ns;
        
        SSDStatistics() : total_requests(0), completed_requests(0), cache_hits(0),
                         cache_misses(0), dram_accesses(0), flash_reads(0),
                         flash_writes(0), average_latency_ns(0.0) {}
    } m_statistics;
    
    // Events for efficient bridge communication
    sc_event m_cache_data_ready;
    sc_event m_dram_data_ready;
    
    // Load configuration from JSON file
    void load_configuration() {
        try {
            JsonConfig config(m_config_file);
            
            // Load SSD Top level configuration
            m_config.cache_size_kb = config.get_int("ssd.cache.cache_size_mb", 256) / 1024; // Convert MB to KB for template
            m_config.dram_size_gb = config.get_int("ssd.dram.dram_size_gb", 4);
            m_config.flash_channels = 1; // Force to 1 channel for simplicity
            m_config.fifo_depth = config.get_int("ssd.top.fifo_depth", 32);
            m_config.enable_debug_all_modules = config.get_bool("ssd.top.enable_debug_all_modules", false);
            
            if (m_debug_enable) {
                std::cout << "SSD Top Configuration loaded:" << std::endl;
                std::cout << "  Cache Size: " << m_config.cache_size_kb << "KB" << std::endl;
                std::cout << "  DRAM Size: " << m_config.dram_size_gb << "GB" << std::endl;
                std::cout << "  Flash Channels: " << m_config.flash_channels << std::endl;
                std::cout << "  FIFO Depth: " << m_config.fifo_depth << std::endl;
            }
            
        } catch (const std::exception& e) {
            if (m_debug_enable) {
                std::cout << "Warning: Could not load SSD Top config, using defaults: " 
                          << e.what() << std::endl;
            }
        }
    }
    
    void create_hardware_modules() {
        bool module_debug = m_debug_enable || m_config.enable_debug_all_modules;
        
        // Create SSD Controller
        m_ssd_controller = new SSDController<PacketType>("ssd_controller", m_config_file, module_debug);
        
        // Create L1 Cache (template parameters: size, line_size, associativity)
        m_cache_l1 = new CacheL1<32, 64, 4>("cache_l1");
        
        // Create DRAM Controller
        m_dram_controller = new DramController<8, 1>("dram_controller", DramTiming(), 1024, 8, true, true, module_debug);
        
        // Create Flash Controller
        m_flash_controller = new FlashController<PacketType>("flash_controller", m_config_file, module_debug);
        
        // Create NAND Flash devices (one per channel)
        m_nand_flash_devices.resize(m_config.flash_channels);
        for (uint32_t ch = 0; ch < m_config.flash_channels; ch++) {
            std::string flash_name = std::string("nand_flash_ch") + std::to_string(ch);
            m_nand_flash_devices[ch] = new NANDFlash<4, 1024, 128>(flash_name.c_str(), FlashTimingParams(), 100000, module_debug);
        }
        
        if (m_debug_enable) {
            std::cout << "0 s | " << basename() << ": Created hardware modules:" << std::endl;
            std::cout << "  - SSD Controller" << std::endl;
            std::cout << "  - L1 Cache (32KB, 4-way)" << std::endl;
            std::cout << "  - DRAM Controller" << std::endl;
            std::cout << "  - Flash Controller" << std::endl;
            std::cout << "  - " << m_config.flash_channels << " NAND Flash devices" << std::endl;
        }
    }
    
    void create_interconnect_fifos() {
        uint32_t fifo_depth = m_config.fifo_depth;
        
        // Create inter-module FIFOs
        m_controller_to_cache = new sc_fifo<std::shared_ptr<PacketType>>("controller_to_cache", fifo_depth);
        m_cache_to_controller = new sc_fifo<std::shared_ptr<PacketType>>("cache_to_controller", fifo_depth);
        m_cache_to_dram = new sc_fifo<std::shared_ptr<BasePacket>>("cache_to_dram", fifo_depth);
        m_dram_to_cache = new sc_fifo<std::shared_ptr<BasePacket>>("dram_to_cache", fifo_depth);
        m_dram_to_flash = new sc_fifo<std::shared_ptr<PacketType>>("dram_to_flash", fifo_depth);
        m_flash_to_dram = new sc_fifo<std::shared_ptr<PacketType>>("flash_to_dram", fifo_depth);
        
        // Create Flash channel FIFOs
        m_flash_channel_out.resize(m_config.flash_channels);
        m_flash_channel_in.resize(m_config.flash_channels);
        
        for (uint32_t ch = 0; ch < m_config.flash_channels; ch++) {
            std::string out_name = std::string("flash_ch") + std::to_string(ch) + "_out";
            std::string in_name = std::string("flash_ch") + std::to_string(ch) + "_in";
            
            m_flash_channel_out[ch] = new sc_fifo<std::shared_ptr<FlashPacket>>(out_name.c_str(), fifo_depth);
            m_flash_channel_in[ch] = new sc_fifo<std::shared_ptr<FlashPacket>>(in_name.c_str(), fifo_depth);
        }
        
        if (m_debug_enable) {
            std::cout << "0 s | " << basename() << ": Created interconnect FIFOs (depth=" 
                      << fifo_depth << ")" << std::endl;
        }
    }
    
    void connect_hardware_modules() {
        // Minimal connection to avoid SystemC binding issues
        // Connect PCIe interface to SSD Controller
        m_ssd_controller->pcie_in(pcie_in);
        m_ssd_controller->pcie_out(pcie_out);
        
        // Connect SSD Controller to Cache
        m_ssd_controller->storage_out(*m_controller_to_cache);
        m_ssd_controller->storage_in(*m_cache_to_controller);
        
        // Connect Cache L1 ports
        m_cache_l1->cpu_in(*m_controller_to_cache);
        m_cache_l1->cpu_out(*m_cache_to_controller);
        m_cache_l1->mem_out(*m_cache_to_dram);
        m_cache_l1->mem_in(*m_dram_to_cache);
        
        // Connect DRAM Controller with basic ports
        m_dram_controller->mem_in(*m_cache_to_dram);
        m_dram_controller->mem_out(*m_dram_to_cache);
        
        // Connect Flash Controller
        m_flash_controller->dram_in(*m_dram_to_flash);
        m_flash_controller->dram_out(*m_flash_to_dram);
        
        // Connect only the first Flash device to avoid complexity
        if (m_config.flash_channels > 0) {
            m_flash_controller->flash_out[0]->bind(*m_flash_channel_out[0]);
            m_flash_controller->flash_in[0]->bind(*m_flash_channel_in[0]);
            
            m_nand_flash_devices[0]->in(*m_flash_channel_out[0]);
            m_nand_flash_devices[0]->release_out(*m_flash_channel_in[0]);
        }
        
        if (m_debug_enable) {
            std::cout << "0 s | " << basename() << ": Connected hardware modules (minimal)" << std::endl;
        }
    }
    
    // Bridge processes to handle interface mismatches between modules
    void cache_bridge_process() {
        // This process bridges between SSD Controller and Cache
        // Handles any packet type conversions or protocol differences
        while (true) {
            // Forward packets from controller to cache
            if (m_controller_to_cache->num_available() > 0) {
                auto packet = m_controller_to_cache->read();
                if (packet) {
                    // Convert to cache packet format if needed
                    // For now, assume direct compatibility
                    // m_cache_l1->process_request(packet);
                    
                    // Simulate cache processing and send response
                    wait(50, SC_NS); // Cache hit latency
                    m_cache_to_controller->write(packet);
                }
            }
            
            // Wait for cache data to be ready  
            if (m_controller_to_cache->num_available() == 0) {
                wait(m_controller_to_cache->data_written_event());
            }
        }
    }
    
    void dram_bridge_process() {
        // This process bridges between Cache and DRAM Controller
        while (true) {
            // Handle cache misses that need DRAM access
            // Wait for DRAM operations to be needed
            wait(m_dram_data_ready);
        }
    }
    
    // Constructor
    SSDTop(sc_module_name name, const std::string& config_file = "config/base/ssd_config.json", 
           bool debug_enable = false)
        : sc_module(name),
          m_debug_enable(debug_enable),
          m_config_file(config_file),
          m_ssd_controller(nullptr),
          m_cache_l1(nullptr),
          m_dram_controller(nullptr),
          m_flash_controller(nullptr) {
        
        std::cout << "DEBUG: SSDTop constructor started" << std::endl;
        
        // Load configuration
        std::cout << "DEBUG: Loading configuration..." << std::endl;
        load_configuration();
        std::cout << "DEBUG: Configuration loaded successfully" << std::endl;
        
        // Create hardware modules
        std::cout << "DEBUG: Creating hardware modules..." << std::endl;
        create_hardware_modules();
        std::cout << "DEBUG: Hardware modules created successfully" << std::endl;
        
        // Create interconnect FIFOs
        std::cout << "DEBUG: Creating interconnect FIFOs..." << std::endl;
        create_interconnect_fifos();
        std::cout << "DEBUG: Interconnect FIFOs created successfully" << std::endl;
        
        // Connect modules
        std::cout << "DEBUG: Connecting hardware modules..." << std::endl;
        connect_hardware_modules();
        std::cout << "DEBUG: Hardware modules connected successfully" << std::endl;
        
        if (m_debug_enable) {
            std::cout << "0 s | " << basename() << ": Hardware-oriented SSD Top initialized" << std::endl;
            std::cout << "  Architecture: PCIe → SSD Controller → Cache → DRAM → Flash Controller → NAND Flash" << std::endl;
        }
        
        // Bridge processes disabled for now
        // SC_THREAD(cache_bridge_process);
        // SC_THREAD(dram_bridge_process);
        std::cout << "DEBUG: SSDTop constructor completed successfully" << std::endl;
    }
    
    // Destructor
    ~SSDTop() {
        // Clean up dynamically allocated modules and FIFOs
        delete m_ssd_controller;
        delete m_cache_l1;
        delete m_dram_controller;
        delete m_flash_controller;
        
        for (auto* flash_device : m_nand_flash_devices) {
            delete flash_device;
        }
        
        delete m_controller_to_cache;
        delete m_cache_to_controller;
        delete m_cache_to_dram;
        delete m_dram_to_cache;
        delete m_dram_to_flash;
        delete m_flash_to_dram;
        
        for (auto* fifo : m_flash_channel_out) {
            delete fifo;
        }
        for (auto* fifo : m_flash_channel_in) {
            delete fifo;
        }
    }
    
public:
    // Statistics methods - aggregate from all hardware modules
    uint64_t get_total_requests() const { 
        return m_ssd_controller ? m_ssd_controller->get_total_commands() : 0; 
    }
    
    uint64_t get_cache_hits() const { 
        // This would come from cache module when properly connected
        return m_statistics.cache_hits; 
    }
    
    uint64_t get_cache_misses() const { 
        return m_statistics.cache_misses; 
    }
    
    double get_cache_hit_rate() const { 
        uint64_t total = get_cache_hits() + get_cache_misses();
        return (total > 0) ? (static_cast<double>(get_cache_hits()) / total) : 0.0; 
    }
    
    uint64_t get_dram_accesses() const { 
        return m_dram_controller ? m_dram_controller->get_stats().total_requests : 0; 
    }
    
    uint64_t get_flash_reads() const { 
        return m_flash_controller ? m_flash_controller->get_read_commands() : 0; 
    }
    
    uint64_t get_flash_writes() const { 
        return m_flash_controller ? m_flash_controller->get_write_commands() : 0; 
    }
    
    // Hardware module access for detailed statistics
    const SSDController<PacketType>* get_ssd_controller() const { return m_ssd_controller; }
    const CacheL1<32, 64, 4>* get_cache() const { return m_cache_l1; }
    const DramController<8, 1>* get_dram_controller() const { return m_dram_controller; }
    const FlashController<PacketType>* get_flash_controller() const { return m_flash_controller; }
    const NANDFlash<4, 1024, 128>* get_nand_flash(uint32_t channel) const { 
        return (channel < m_nand_flash_devices.size()) ? m_nand_flash_devices[channel] : nullptr; 
    }
    
    // Performance metrics - aggregate from all modules
    void print_statistics() const {
        std::cout << "\n========== Hardware-Oriented SSD Statistics ==========" << std::endl;
        std::cout << "Total Requests: " << get_total_requests() << std::endl;
        std::cout << "Cache Hits: " << get_cache_hits() << " (" 
                  << std::fixed << std::setprecision(1) << get_cache_hit_rate() * 100.0 << "%)" << std::endl;
        std::cout << "Cache Misses: " << get_cache_misses() << std::endl;
        std::cout << "DRAM Accesses: " << get_dram_accesses() << std::endl;
        std::cout << "Flash Reads: " << get_flash_reads() << std::endl;
        std::cout << "Flash Writes: " << get_flash_writes() << std::endl;
        std::cout << "=======================================================" << std::endl;
        
        // Print detailed statistics from each hardware module
        if (m_ssd_controller) {
            m_ssd_controller->print_statistics();
        }
        
        if (m_flash_controller) {
            m_flash_controller->print_statistics();
        }
        
        // Print per-channel flash statistics
        std::cout << "\n========== NAND Flash Channel Statistics ==========" << std::endl;
        for (uint32_t ch = 0; ch < m_nand_flash_devices.size(); ch++) {
            if (m_nand_flash_devices[ch]) {
                std::cout << "Channel " << ch << ": ";
                // NAND flash statistics would be printed here
                std::cout << "Active" << std::endl;
            }
        }
        std::cout << "===================================================" << std::endl;
    }
};

// Type aliases for common configurations
using BasePacketSSDTop = SSDTop<BasePacket>;
using PCIePacketSSDTop = SSDTop<PCIePacket>;

#endif // SSD_TOP_H