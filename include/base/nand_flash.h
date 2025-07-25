#ifndef NAND_FLASH_H
#define NAND_FLASH_H

#include <systemc.h>
#include <memory>
#include <array>
#include <vector>
#include <random>
#include <algorithm>
#include <iomanip>
#include "packet/flash_packet.h"
#include "common/error_handling.h"

// NAND Flash timing parameters (in nanoseconds)
struct FlashTimingParams {
    double tR_ns;           // Read latency (25,000 ns typical)
    double tProg_ns;        // Program latency (200,000 ns typical)
    double tErase_ns;       // Erase latency (2,000,000 ns typical)
    double io_speed_mhz;    // I/O interface speed (MHz)
    uint8_t io_width_bits;  // I/O width (8 or 16 bits)
    
    FlashTimingParams() : tR_ns(25000.0), tProg_ns(200000.0), tErase_ns(2000000.0),
                         io_speed_mhz(100.0), io_width_bits(8) {}
    
    // Calculate I/O transfer time for given data size
    double calculate_io_time_ns(uint32_t data_size_bytes) const {
        if (io_speed_mhz <= 0.0) return 0.0;
        
        // Convert data size to bits
        uint64_t data_bits = static_cast<uint64_t>(data_size_bytes) * 8;
        
        // Calculate transfer cycles
        uint64_t transfer_cycles = (data_bits + io_width_bits - 1) / io_width_bits;
        
        // Convert to nanoseconds (1 MHz = 1000 ns period)
        return static_cast<double>(transfer_cycles) * 1000.0 / io_speed_mhz;
    }
};

// Page state enumeration
enum class PageState {
    CLEAN,      // Erased, ready for program
    PROGRAMMED, // Contains valid data
    INVALID     // Contains invalid/old data
};

// Block state structure
struct FlashBlock {
    std::vector<PageState> pages;
    uint32_t erase_count;
    bool is_bad_block;
    
    FlashBlock(size_t pages_per_block) 
        : pages(pages_per_block, PageState::CLEAN), erase_count(0), is_bad_block(false) {}
};

// Template-based NAND Flash controller
template<size_t NumPlanes = 4, size_t BlocksPerPlane = 1024, size_t PagesPerBlock = 128>
SC_MODULE(NANDFlash) {
    SC_HAS_PROCESS(NANDFlash);
    
    // SystemC ports
    sc_fifo_in<std::shared_ptr<FlashPacket>> in;
    sc_fifo_out<std::shared_ptr<FlashPacket>> release_out;
    
    // Configuration
    const bool m_debug_enable;
    const FlashTimingParams m_timing;
    const uint32_t m_max_pe_cycles;  // Maximum P/E cycles before failure
    
    // Flash memory structure: [plane][block]
    std::array<std::array<FlashBlock, BlocksPerPlane>, NumPlanes> m_flash_memory;
    
    // Random number generation for timing variation
    mutable std::mt19937 m_rng;
    mutable std::normal_distribution<double> m_timing_variation;
    
    // Statistics
    uint64_t m_total_reads;
    uint64_t m_total_programs;
    uint64_t m_total_erases;
    uint64_t m_bad_block_count;
    
    // Main processing method
    void flash_process() {
        while (true) {
            auto packet = in.read();
            
            if (!packet) {
                SOC_SIM_ERROR("NANDFlash", soc_sim::error::codes::INVALID_PACKET_TYPE,
                             "Received null FlashPacket");
                continue;
            }
            
            const FlashAddress& addr = packet->get_flash_address();
            FlashCommand cmd = packet->get_flash_command();
            
            // Validate address bounds
            if (!is_valid_address(addr)) {
                SOC_SIM_ERROR("NANDFlash", soc_sim::error::codes::ADDRESS_OUT_OF_BOUNDS,
                             "Invalid flash address: " + addr.to_string());
                continue;
            }
            
            // Check for bad block
            if (m_flash_memory[addr.plane][addr.block].is_bad_block) {
                SOC_SIM_ERROR("NANDFlash", soc_sim::error::codes::DEVICE_ERROR,
                             "Access to bad block: " + addr.to_string());
                continue;
            }
            
            // Process the flash operation
            bool operation_success = false;
            double operation_delay_ns = 0.0;
            
            switch (cmd) {
                case FlashCommand::READ:
                    operation_success = process_read(packet, operation_delay_ns);
                    m_total_reads++;
                    break;
                    
                case FlashCommand::PROGRAM:
                    operation_success = process_program(packet, operation_delay_ns);
                    m_total_programs++;
                    break;
                    
                case FlashCommand::ERASE:
                    operation_success = process_erase(packet, operation_delay_ns);
                    m_total_erases++;
                    break;
                    
                default:
                    SOC_SIM_ERROR("NANDFlash", soc_sim::error::codes::INVALID_PACKET_TYPE,
                                 "Unknown flash command");
                    continue;
            }
            
            // Apply operation delay with variation
            if (operation_delay_ns > 0.0) {
                double actual_delay = add_timing_variation(operation_delay_ns);
                wait(actual_delay, SC_NS);
            }
            
            if (m_debug_enable) {
                const char* cmd_str = (cmd == FlashCommand::READ) ? "READ" :
                                      (cmd == FlashCommand::PROGRAM) ? "PROGRAM" : "ERASE";
                std::cout << sc_time_stamp() << " | NANDFlash: " << cmd_str 
                          << " " << addr.to_string() 
                          << " (delay=" << std::fixed << std::setprecision(1) 
                          << operation_delay_ns << "ns, success=" << operation_success << ")"
                          << std::endl;
            }
            
            // Send processed packet back
            release_out.write(packet);
        }
    }
    
    // Process read operation
    bool process_read(std::shared_ptr<FlashPacket> packet, double& delay_ns) {
        const FlashAddress& addr = packet->get_flash_address();
        
        // Calculate read delay: tR + I/O transfer time
        delay_ns = m_timing.tR_ns + m_timing.calculate_io_time_ns(packet->get_data_size());
        
        // Check if page has valid data
        PageState page_state = get_page_state(addr);
        if (page_state == PageState::CLEAN) {
            // Reading from clean page returns all 0xFF (or default data)
            if (packet->original_packet) {
                packet->original_packet->set_data(0xFF);
            }
        }
        
        return true; // Read operations rarely fail in simulation
    }
    
    // Process program operation
    bool process_program(std::shared_ptr<FlashPacket> packet, double& delay_ns) {
        const FlashAddress& addr = packet->get_flash_address();
        
        // Calculate program delay: tProg + I/O transfer time
        delay_ns = m_timing.tProg_ns + m_timing.calculate_io_time_ns(packet->get_data_size());
        
        // Check if page is in clean state (erase-before-write rule)
        PageState page_state = get_page_state(addr);
        if (page_state != PageState::CLEAN) {
            SOC_SIM_ERROR("NANDFlash", soc_sim::error::codes::DEVICE_ERROR,
                         "Program operation to non-clean page: " + addr.to_string());
            return false;
        }
        
        // Update page state to programmed
        set_page_state(addr, PageState::PROGRAMMED);
        
        // Check for program failure (rare in simulation)
        if (should_fail_operation(0.001)) { // 0.1% failure rate
            mark_bad_block(addr.plane, addr.block);
            return false;
        }
        
        return true;
    }
    
    // Process erase operation
    bool process_erase(std::shared_ptr<FlashPacket> packet, double& delay_ns) {
        const FlashAddress& addr = packet->get_flash_address();
        
        // Erase operates on entire block
        delay_ns = m_timing.tErase_ns;
        
        // Erase entire block (all pages become clean)
        FlashBlock& block = m_flash_memory[addr.plane][addr.block];
        for (auto& page : block.pages) {
            page = PageState::CLEAN;
        }
        
        // Increment erase count
        block.erase_count++;
        
        // Check for wear-out
        if (block.erase_count >= m_max_pe_cycles && should_fail_operation(0.1)) {
            mark_bad_block(addr.plane, addr.block);
            return false;
        }
        
        // Check for erase failure (rare)
        if (should_fail_operation(0.01)) { // 1% failure rate
            mark_bad_block(addr.plane, addr.block);
            return false;
        }
        
        return true;
    }
    
    // Helper methods
    bool is_valid_address(const FlashAddress& addr) const {
        return (addr.plane < NumPlanes) && 
               (addr.block < BlocksPerPlane) &&
               (addr.wl < PagesPerBlock) &&
               (addr.ssl < 4) &&  // Typical SSL count
               (addr.page < 32);  // Typical pages per SSL
    }
    
    PageState get_page_state(const FlashAddress& addr) const {
        size_t page_index = addr.wl * 4 * 32 + addr.ssl * 32 + addr.page;
        if (page_index < m_flash_memory[addr.plane][addr.block].pages.size()) {
            return m_flash_memory[addr.plane][addr.block].pages[page_index];
        }
        return PageState::CLEAN;
    }
    
    void set_page_state(const FlashAddress& addr, PageState state) {
        size_t page_index = addr.wl * 4 * 32 + addr.ssl * 32 + addr.page;
        if (page_index < m_flash_memory[addr.plane][addr.block].pages.size()) {
            m_flash_memory[addr.plane][addr.block].pages[page_index] = state;
        }
    }
    
    void mark_bad_block(uint8_t plane, uint16_t block) {
        if (plane < NumPlanes && block < BlocksPerPlane) {
            m_flash_memory[plane][block].is_bad_block = true;
            m_bad_block_count++;
        }
    }
    
    bool should_fail_operation(double failure_rate) const {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(m_rng) < failure_rate;
    }
    
    double add_timing_variation(double base_delay_ns) const {
        // Add ±5% timing variation
        double variation = m_timing_variation(m_rng) * base_delay_ns * 0.05;
        return std::max(0.0, base_delay_ns + variation);
    }
    
    // Constructor
    NANDFlash(sc_module_name name,
              const FlashTimingParams& timing = FlashTimingParams(),
              uint32_t max_pe_cycles = 100000,
              bool debug_enable = false)
        : sc_module(name),
          m_debug_enable(debug_enable),
          m_timing(timing),
          m_max_pe_cycles(max_pe_cycles),
          m_rng(std::random_device{}()),
          m_timing_variation(0.0, 1.0),
          m_total_reads(0),
          m_total_programs(0),
          m_total_erases(0),
          m_bad_block_count(0) {
        
        // Initialize flash memory structure
        for (auto& plane : m_flash_memory) {
            for (auto& block : plane) {
                block = FlashBlock(PagesPerBlock * 4 * 32); // wl * ssl * pages_per_ssl
            }
        }
        
        if (m_debug_enable) {
            std::cout << "0 s | " << basename() << ": NAND Flash initialized "
                      << "(" << NumPlanes << " planes, " << BlocksPerPlane << " blocks/plane, "
                      << PagesPerBlock << " pages/block, tR=" << m_timing.tR_ns/1000.0 << "μs, "
                      << "tProg=" << m_timing.tProg_ns/1000.0 << "μs, "
                      << "tErase=" << m_timing.tErase_ns/1000000.0 << "ms)" << std::endl;
        }
        
        SC_THREAD(flash_process);
    }
    
    // Statistics and monitoring methods
    uint64_t get_total_reads() const { return m_total_reads; }
    uint64_t get_total_programs() const { return m_total_programs; }
    uint64_t get_total_erases() const { return m_total_erases; }
    uint64_t get_bad_block_count() const { return m_bad_block_count; }
    
    uint32_t get_block_erase_count(uint8_t plane, uint16_t block) const {
        if (plane < NumPlanes && block < BlocksPerPlane) {
            return m_flash_memory[plane][block].erase_count;
        }
        return 0;
    }
    
    bool is_bad_block(uint8_t plane, uint16_t block) const {
        if (plane < NumPlanes && block < BlocksPerPlane) {
            return m_flash_memory[plane][block].is_bad_block;
        }
        return false;
    }
    
    // Get flash geometry information
    static constexpr size_t get_num_planes() { return NumPlanes; }
    static constexpr size_t get_blocks_per_plane() { return BlocksPerPlane; }
    static constexpr size_t get_pages_per_block() { return PagesPerBlock; }
    
    const FlashTimingParams& get_timing_params() const { return m_timing; }
};

// Type aliases for common configurations
using StandardNANDFlash = NANDFlash<4, 1024, 128>;   // 4 planes, 1024 blocks/plane, 128 pages/block
using LargeNANDFlash = NANDFlash<4, 2048, 256>;      // Larger capacity
using SmallNANDFlash = NANDFlash<2, 512, 64>;        // Smaller for testing

// Helper function to create NAND Flash with custom parameters
template<size_t NumPlanes, size_t BlocksPerPlane, size_t PagesPerBlock>
NANDFlash<NumPlanes, BlocksPerPlane, PagesPerBlock>* create_nand_flash(
    sc_module_name name,
    const FlashTimingParams& timing = FlashTimingParams(),
    uint32_t max_pe_cycles = 100000,
    bool debug_enable = false) {
    
    return new NANDFlash<NumPlanes, BlocksPerPlane, PagesPerBlock>(
        name, timing, max_pe_cycles, debug_enable);
}

#endif