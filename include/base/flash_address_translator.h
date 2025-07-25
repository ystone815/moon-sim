#ifndef FLASH_ADDRESS_TRANSLATOR_H
#define FLASH_ADDRESS_TRANSLATOR_H

#include <systemc.h>
#include <memory>
#include <functional>
#include <vector>
#include <unordered_map>
#include "packet/base_packet.h"
#include "packet/flash_packet.h"
#include "common/error_handling.h"

// Address translation policies
enum class AddressTranslationPolicy {
    LINEAR,          // Simple linear mapping
    INTERLEAVED,     // Plane interleaved mapping
    WEAR_LEVELING    // Wear leveling aware mapping
};

// Flash geometry configuration
struct FlashGeometry {
    uint8_t planes;              // Number of planes (typically 4)
    uint16_t blocks_per_plane;   // Blocks per plane
    uint8_t wordlines_per_block; // WordLines per block (typically 128)
    uint8_t ssl_per_wordline;    // SSL per WordLine (typically 4)
    uint16_t pages_per_ssl;      // Pages per SSL
    uint32_t page_size_bytes;    // Page size in bytes (16KiB typical)
    
    FlashGeometry() : planes(4), blocks_per_plane(1024), wordlines_per_block(128),
                      ssl_per_wordline(4), pages_per_ssl(32), page_size_bytes(16384) {}
};

// Template-based Flash Address Translator
template<typename PacketType>
SC_MODULE(FlashAddressTranslator) {
    SC_HAS_PROCESS(FlashAddressTranslator);
    
    // SystemC ports
    sc_fifo_in<std::shared_ptr<PacketType>> in;
    sc_fifo_out<std::shared_ptr<FlashPacket>> out;
    
    // Configuration
    const bool m_debug_enable;
    const AddressTranslationPolicy m_policy;
    const FlashGeometry m_geometry;
    
    // Bad block management
    std::unordered_map<uint32_t, uint32_t> m_bad_block_remap;
    std::vector<uint32_t> m_bad_blocks;
    
    // Wear leveling data
    std::vector<uint32_t> m_erase_counts;
    uint32_t m_total_erases;
    
    // Address accessor functions
    std::function<int(const PacketType&)> m_get_address;
    std::function<Command(const PacketType&)> m_get_command;
    
    // Main translation process
    void translate_process() {
        while (true) {
            auto packet = in.read();
            
            if (!packet) {
                SOC_SIM_ERROR("FlashAddressTranslator", soc_sim::error::codes::INVALID_PACKET_TYPE,
                             "Received null packet");
                continue;
            }
            
            // Create FlashPacket from BasePacket
            auto flash_packet = std::make_shared<FlashPacket>(packet);
            
            // Get system address from original packet
            int system_address = m_get_address(*packet);
            Command original_command = m_get_command(*packet);
            
            // Convert Command to FlashCommand
            FlashCommand flash_cmd = (original_command == Command::READ) ? 
                                   FlashCommand::READ : FlashCommand::PROGRAM;
            flash_packet->set_flash_command(flash_cmd);
            
            // Translate system address to flash address
            FlashAddress flash_addr = translate_address(static_cast<uint32_t>(system_address));
            flash_packet->set_flash_address(flash_addr);
            
            // Set data size based on original packet
            uint32_t data_size = static_cast<uint32_t>(packet->get_databyte());
            if (data_size == 0) data_size = m_geometry.page_size_bytes; // Default page size
            flash_packet->set_data_size(data_size);
            
            // Copy index for tracking
            flash_packet->set_attribute("index", packet->get_attribute("index"));
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | FlashAddressTranslator: "
                          << "SysAddr: 0x" << std::hex << system_address << std::dec
                          << " → " << flash_addr.to_string()
                          << " (" << (flash_cmd == FlashCommand::READ ? "READ" : "PROGRAM") << ")"
                          << std::endl;
            }
            
            // Send translated packet
            out.write(flash_packet);
        }
    }
    
    // Address translation methods
    FlashAddress translate_address(uint32_t system_address) {
        switch (m_policy) {
            case AddressTranslationPolicy::LINEAR:
                return linear_translate(system_address);
            case AddressTranslationPolicy::INTERLEAVED:
                return interleaved_translate(system_address);
            case AddressTranslationPolicy::WEAR_LEVELING:
                return wear_leveling_translate(system_address);
            default:
                return linear_translate(system_address);
        }
    }
    
    FlashAddress linear_translate(uint32_t system_address) {
        FlashAddress addr;
        
        // Simple linear mapping with bounds checking
        uint32_t total_pages = m_geometry.planes * m_geometry.blocks_per_plane * 
                              m_geometry.wordlines_per_block * m_geometry.ssl_per_wordline * 
                              m_geometry.pages_per_ssl;
        
        uint32_t page_index = system_address % total_pages;
        
        // Extract flash address components
        addr.page = page_index % m_geometry.pages_per_ssl;
        page_index /= m_geometry.pages_per_ssl;
        
        addr.ssl = page_index % m_geometry.ssl_per_wordline;
        page_index /= m_geometry.ssl_per_wordline;
        
        addr.wl = page_index % m_geometry.wordlines_per_block;
        page_index /= m_geometry.wordlines_per_block;
        
        addr.block = page_index % m_geometry.blocks_per_plane;
        page_index /= m_geometry.blocks_per_plane;
        
        addr.plane = page_index % m_geometry.planes;
        
        return check_bad_block_remap(addr);
    }
    
    FlashAddress interleaved_translate(uint32_t system_address) {
        FlashAddress addr;
        
        // Plane interleaved mapping for better parallelism
        addr.plane = system_address % m_geometry.planes;
        uint32_t remaining = system_address / m_geometry.planes;
        
        addr.page = remaining % m_geometry.pages_per_ssl;
        remaining /= m_geometry.pages_per_ssl;
        
        addr.ssl = remaining % m_geometry.ssl_per_wordline;
        remaining /= m_geometry.ssl_per_wordline;
        
        addr.wl = remaining % m_geometry.wordlines_per_block;
        remaining /= m_geometry.wordlines_per_block;
        
        addr.block = remaining % m_geometry.blocks_per_plane;
        
        return check_bad_block_remap(addr);
    }
    
    FlashAddress wear_leveling_translate(uint32_t system_address) {
        // Start with interleaved mapping
        FlashAddress addr = interleaved_translate(system_address);
        
        // Apply wear leveling if needed
        uint32_t block_id = addr.plane * m_geometry.blocks_per_plane + addr.block;
        if (block_id < m_erase_counts.size()) {
            // Simple wear leveling: remap heavily used blocks
            uint32_t avg_erases = m_total_erases / m_erase_counts.size();
            if (m_erase_counts[block_id] > avg_erases * 2) {
                // Find a less used block in the same plane
                for (uint32_t i = 0; i < m_geometry.blocks_per_plane; ++i) {
                    uint32_t candidate_id = addr.plane * m_geometry.blocks_per_plane + i;
                    if (candidate_id < m_erase_counts.size() && 
                        m_erase_counts[candidate_id] < avg_erases) {
                        addr.block = i;
                        break;
                    }
                }
            }
        }
        
        return check_bad_block_remap(addr);
    }
    
    FlashAddress check_bad_block_remap(const FlashAddress& addr) {
        uint32_t block_id = addr.plane * m_geometry.blocks_per_plane + addr.block;
        
        auto remap_it = m_bad_block_remap.find(block_id);
        if (remap_it != m_bad_block_remap.end()) {
            FlashAddress remapped_addr = addr;
            uint32_t new_block_id = remap_it->second;
            remapped_addr.plane = new_block_id / m_geometry.blocks_per_plane;
            remapped_addr.block = new_block_id % m_geometry.blocks_per_plane;
            return remapped_addr;
        }
        
        return addr;
    }
    
    // Constructor
    FlashAddressTranslator(sc_module_name name,
                          const FlashGeometry& geometry = FlashGeometry(),
                          AddressTranslationPolicy policy = AddressTranslationPolicy::LINEAR,
                          bool debug_enable = false)
        : sc_module(name),
          m_debug_enable(debug_enable),
          m_policy(policy),
          m_geometry(geometry),
          m_total_erases(0) {
        
        // Set up default accessors for types with standard methods
        m_get_address = [](const PacketType& p) { return p.get_address(); };
        m_get_command = [](const PacketType& p) { return p.get_command(); };
        
        // Initialize wear leveling data
        uint32_t total_blocks = m_geometry.planes * m_geometry.blocks_per_plane;
        m_erase_counts.resize(total_blocks, 0);
        
        if (m_debug_enable) {
            std::cout << "0 s | " << basename() << ": Flash Address Translator initialized"
                      << " (Policy: " << static_cast<int>(m_policy) 
                      << ", Planes: " << static_cast<int>(m_geometry.planes)
                      << ", Blocks/Plane: " << m_geometry.blocks_per_plane << ")" << std::endl;
        }
        
        SC_THREAD(translate_process);
    }
    
    // Bad block management methods
    void add_bad_block(uint32_t block_id, uint32_t replacement_block_id) {
        m_bad_block_remap[block_id] = replacement_block_id;
        m_bad_blocks.push_back(block_id);
    }
    
    void record_erase(uint32_t block_id) {
        if (block_id < m_erase_counts.size()) {
            m_erase_counts[block_id]++;
            m_total_erases++;
        }
    }
    
    // Statistics
    uint32_t get_erase_count(uint32_t block_id) const {
        return (block_id < m_erase_counts.size()) ? m_erase_counts[block_id] : 0;
    }
    
    size_t get_bad_block_count() const { return m_bad_blocks.size(); }
    const FlashGeometry& get_geometry() const { return m_geometry; }
};

// Type aliases for common configurations
using BasePacketFlashTranslator = FlashAddressTranslator<BasePacket>;

// Helper function to create translator with custom accessors
template<typename PacketType>
FlashAddressTranslator<PacketType>* create_flash_translator(
    sc_module_name name,
    const FlashGeometry& geometry,
    AddressTranslationPolicy policy = AddressTranslationPolicy::LINEAR,
    bool debug_enable = false) {
    
    return new FlashAddressTranslator<PacketType>(name, geometry, policy, debug_enable);
}

#endif