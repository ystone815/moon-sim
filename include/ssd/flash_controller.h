#ifndef FLASH_CONTROLLER_H
#define FLASH_CONTROLLER_H

#include <systemc.h>
#include <memory>
#include <queue>
#include <vector>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <iomanip>
#include "packet/base_packet.h"
#include "packet/flash_packet.h"
#include "base/nand_flash.h"
#include "common/json_config.h"
#include "common/error_handling.h"

// Flash Controller configuration
struct FlashControllerConfig {
    uint32_t num_channels;           // Number of Flash channels
    uint32_t dies_per_channel;       // Dies per channel
    uint32_t command_queue_depth;    // Command queue depth per channel
    uint32_t page_size_kb;           // Page size in KB
    uint32_t pages_per_block;        // Pages per block
    uint32_t blocks_per_die;         // Blocks per die
    double channel_bandwidth_mbps;   // Channel bandwidth in MB/s
    bool enable_interleaving;        // Enable channel interleaving
    bool enable_wear_leveling;       // Enable wear leveling
    std::string ecc_type;            // ECC type (BCH, LDPC, etc.)
    
    FlashControllerConfig() : num_channels(8), dies_per_channel(4), command_queue_depth(16),
                            page_size_kb(16), pages_per_block(128), blocks_per_die(1024),
                            channel_bandwidth_mbps(400.0), enable_interleaving(true),
                            enable_wear_leveling(true), ecc_type("LDPC") {}
};

// Flash operation types
enum class FlashOperation {
    READ_PAGE,      // Read page operation
    PROGRAM_PAGE,   // Program page operation
    ERASE_BLOCK,    // Erase block operation
    READ_STATUS,    // Read status operation
    RESET          // Reset operation
};

// Flash command structure
struct FlashControllerCommand {
    std::shared_ptr<BasePacket> original_packet;
    FlashOperation operation;
    uint32_t channel;
    uint32_t die;
    uint32_t plane;
    uint32_t block;
    uint32_t page;
    uint64_t physical_address;
    sc_time submit_time;
    sc_time start_time;
    sc_time completion_time;
    bool completed;
    
    FlashControllerCommand(std::shared_ptr<BasePacket> pkt, FlashOperation op)
        : original_packet(pkt), operation(op), channel(0), die(0), plane(0),
          block(0), page(0), physical_address(0), submit_time(sc_time_stamp()),
          start_time(SC_ZERO_TIME), completion_time(SC_ZERO_TIME), completed(false) {}
};

// Channel state tracking
struct ChannelState {
    bool busy;
    std::queue<std::shared_ptr<FlashControllerCommand>> command_queue;
    sc_time last_operation_time;
    uint64_t total_operations;
    uint64_t read_operations;
    uint64_t write_operations;
    uint64_t erase_operations;
    
    ChannelState() : busy(false), last_operation_time(SC_ZERO_TIME),
                    total_operations(0), read_operations(0), write_operations(0),
                    erase_operations(0) {}
};

// Flash Controller SystemC Module
template<typename PacketType = BasePacket>
SC_MODULE(FlashController) {
    SC_HAS_PROCESS(FlashController);
    
    // Interface to upper level (DRAM Controller)
    sc_fifo_in<std::shared_ptr<PacketType>> dram_in;      // Commands from DRAM Controller
    sc_fifo_out<std::shared_ptr<PacketType>> dram_out;    // Completions to DRAM Controller
    
    // Interface to NAND Flash arrays (multiple channels)
    std::vector<sc_fifo_out<std::shared_ptr<FlashPacket>>*> flash_out;  // Commands to Flash
    std::vector<sc_fifo_in<std::shared_ptr<FlashPacket>>*> flash_in;    // Completions from Flash
    
    // Configuration and debug
    const bool m_debug_enable;
    const std::string m_config_file;
    FlashControllerConfig m_config;
    
    // Channel management
    std::vector<ChannelState> m_channels;
    
    // Address translation and mapping
    std::unordered_map<uint64_t, uint64_t> m_logical_to_physical;
    std::vector<uint32_t> m_erase_counts;  // Per-block erase count for wear leveling
    
    // Statistics
    uint64_t m_total_flash_commands;
    uint64_t m_completed_flash_commands;
    uint64_t m_read_commands;
    uint64_t m_write_commands;
    uint64_t m_erase_commands;
    double m_total_flash_latency_ns;
    uint64_t m_channel_conflicts;
    
    // Random number generator for wear leveling
    mutable std::mt19937 m_rng;
    mutable std::uniform_int_distribution<uint32_t> m_block_dist;
    
    // Main Flash Controller processes
    void command_reception_process() {
        while (true) {
            // Wait for command from DRAM Controller
            auto packet = dram_in.read();
            
            if (!packet) {
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | FlashController: Received null packet" << std::endl;
                }
                continue;
            }
            
            m_total_flash_commands++;
            
            // Translate logical address to physical address
            uint64_t logical_addr = packet->get_address();
            uint64_t physical_addr = translate_address(logical_addr);
            
            // Determine Flash operation type
            FlashOperation operation = (packet->get_command() == Command::READ) ? 
                                     FlashOperation::READ_PAGE : FlashOperation::PROGRAM_PAGE;
            
            // Create Flash command
            auto flash_cmd = std::make_shared<FlashControllerCommand>(packet, operation);
            decode_physical_address(physical_addr, flash_cmd);
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | FlashController: Received " 
                          << (operation == FlashOperation::READ_PAGE ? "READ" : "WRITE")
                          << " command, Logical 0x" << std::hex << logical_addr 
                          << " → Physical 0x" << physical_addr << std::dec
                          << " (Ch" << flash_cmd->channel << "/Die" << flash_cmd->die 
                          << "/Block" << flash_cmd->block << "/Page" << flash_cmd->page << ")" << std::endl;
            }
            
            // Route command to appropriate channel
            route_command_to_channel(flash_cmd);
        }
    }
    
    void channel_arbitration_process() {
        while (true) {
            // Process commands for each channel
            for (uint32_t ch = 0; ch < m_config.num_channels; ch++) {
                ChannelState& channel = m_channels[ch];
                
                // Skip if channel is busy or no commands queued
                if (channel.busy || channel.command_queue.empty()) {
                    continue;
                }
                
                // Get next command from channel queue
                auto flash_cmd = channel.command_queue.front();
                channel.command_queue.pop();
                
                // Mark channel as busy
                channel.busy = true;
                flash_cmd->start_time = sc_time_stamp();
                
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | FlashController: Starting " 
                              << get_operation_name(flash_cmd->operation) 
                              << " on Channel " << ch << std::endl;
                }
                
                // Send command to appropriate Flash device
                send_command_to_flash(flash_cmd);
                
                // Update channel statistics
                channel.total_operations++;
                channel.last_operation_time = sc_time_stamp();
                
                switch (flash_cmd->operation) {
                    case FlashOperation::READ_PAGE:
                        channel.read_operations++;
                        m_read_commands++;
                        break;
                    case FlashOperation::PROGRAM_PAGE:
                        channel.write_operations++;
                        m_write_commands++;
                        break;
                    case FlashOperation::ERASE_BLOCK:
                        channel.erase_operations++;
                        m_erase_commands++;
                        break;
                    default:
                        break;
                }
            }
            
            // Small delay to prevent busy waiting
            wait(1, SC_NS);
        }
    }
    
    void completion_handling_process() {
        while (true) {
            // Check for completions from all Flash channels
            for (uint32_t ch = 0; ch < m_config.num_channels; ch++) {
                if (flash_in[ch]->num_available() > 0) {
                    auto flash_packet = flash_in[ch]->read();
                    
                    if (flash_packet) {
                        handle_flash_completion(flash_packet, ch);
                    }
                }
            }
            
            // Small delay to prevent busy waiting
            wait(1, SC_NS);
        }
    }
    
    void wear_leveling_process() {
        if (!m_config.enable_wear_leveling) {
            return; // Wear leveling disabled
        }
        
        while (true) {
            // Periodic wear leveling check (every 100ms simulation time)
            wait(100000, SC_NS);
            
            // Find blocks with high erase counts
            uint32_t max_erase_count = *std::max_element(m_erase_counts.begin(), m_erase_counts.end());
            uint32_t min_erase_count = *std::min_element(m_erase_counts.begin(), m_erase_counts.end());
            
            // Trigger wear leveling if difference is too high
            if (max_erase_count - min_erase_count > 100) {
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | FlashController: Wear leveling triggered "
                              << "(Max: " << max_erase_count << ", Min: " << min_erase_count << ")" << std::endl;
                }
                
                // Perform wear leveling (simplified)
                perform_wear_leveling();
            }
        }
    }
    
    // Address translation and management
    uint64_t translate_address(uint64_t logical_addr) {
        // Simple direct mapping for now
        // In real SSD, this would involve complex FTL (Flash Translation Layer)
        auto it = m_logical_to_physical.find(logical_addr);
        if (it != m_logical_to_physical.end()) {
            return it->second;
        }
        
        // Allocate new physical address
        uint64_t physical_addr = allocate_physical_address();
        m_logical_to_physical[logical_addr] = physical_addr;
        
        return physical_addr;
    }
    
    uint64_t allocate_physical_address() {
        // Simple round-robin allocation across channels
        static uint64_t next_addr = 0;
        return next_addr++;
    }
    
    void decode_physical_address(uint64_t physical_addr, std::shared_ptr<FlashControllerCommand> cmd) {
        // Extract channel, die, block, page from physical address
        // Address format: [Channel][Die][Block][Page]
        uint32_t pages_per_die = m_config.blocks_per_die * m_config.pages_per_block;
        uint32_t pages_per_channel = pages_per_die * m_config.dies_per_channel;
        
        cmd->channel = (physical_addr / pages_per_channel) % m_config.num_channels;
        uint64_t addr_in_channel = physical_addr % pages_per_channel;
        
        cmd->die = addr_in_channel / pages_per_die;
        uint64_t addr_in_die = addr_in_channel % pages_per_die;
        
        cmd->block = addr_in_die / m_config.pages_per_block;
        cmd->page = addr_in_die % m_config.pages_per_block;
        
        cmd->physical_address = physical_addr;
    }
    
    void route_command_to_channel(std::shared_ptr<FlashControllerCommand> cmd) {
        ChannelState& channel = m_channels[cmd->channel];
        
        // Check if channel queue is full
        if (channel.command_queue.size() >= m_config.command_queue_depth) {
            m_channel_conflicts++;
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | FlashController: Channel " 
                          << cmd->channel << " queue full, waiting..." << std::endl;
            }
            
            // Wait for space in channel queue
            while (channel.command_queue.size() >= m_config.command_queue_depth) {
                wait(10, SC_NS);
            }
        }
        
        // Add command to channel queue
        channel.command_queue.push(cmd);
    }
    
    void send_command_to_flash(std::shared_ptr<FlashControllerCommand> cmd) {
        // Create Flash packet
        auto flash_packet = std::make_shared<FlashPacket>();
        flash_packet->set_databyte(cmd->original_packet->get_databyte());
        
        // Set Flash-specific address fields
        flash_packet->flash_address.plane = static_cast<uint8_t>(cmd->plane);
        flash_packet->flash_address.block = static_cast<uint16_t>(cmd->block);
        flash_packet->flash_address.page = static_cast<uint16_t>(cmd->page);
        flash_packet->flash_address.wl = 0;  // Default wordline
        flash_packet->flash_address.ssl = 0; // Default string select line
        
        // Determine Flash command type
        switch (cmd->operation) {
            case FlashOperation::READ_PAGE:
                flash_packet->flash_command = FlashCommand::READ;
                break;
            case FlashOperation::PROGRAM_PAGE:
                flash_packet->flash_command = FlashCommand::PROGRAM;
                break;
            case FlashOperation::ERASE_BLOCK:
                flash_packet->flash_command = FlashCommand::ERASE;
                break;
            default:
                flash_packet->flash_command = FlashCommand::READ;
                break;
        }
        
        // Send to appropriate Flash device
        flash_out[cmd->channel]->write(flash_packet);
    }
    
    void handle_flash_completion(std::shared_ptr<FlashPacket> flash_packet, uint32_t channel) {
        // Mark channel as not busy
        m_channels[channel].busy = false;
        
        // Calculate latency
        sc_time completion_time = sc_time_stamp();
        // Note: We'd need to track the original command to calculate accurate latency
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | FlashController: Flash operation completed on Channel " 
                      << channel << std::endl;
        }
        
        // Convert back to original packet format and send to DRAM Controller
        auto response_packet = std::static_pointer_cast<PacketType>(flash_packet);
        dram_out.write(response_packet);
        
        m_completed_flash_commands++;
    }
    
    void perform_wear_leveling() {
        // Simplified wear leveling: swap data between high and low wear blocks
        // Real implementation would be much more complex
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | FlashController: Performing wear leveling..." << std::endl;
        }
        
        // Find high and low wear blocks
        auto max_it = std::max_element(m_erase_counts.begin(), m_erase_counts.end());
        auto min_it = std::min_element(m_erase_counts.begin(), m_erase_counts.end());
        
        // Swap erase counts (simplified)
        if (max_it != m_erase_counts.end() && min_it != m_erase_counts.end()) {
            std::swap(*max_it, *min_it);
        }
    }
    
    std::string get_operation_name(FlashOperation op) const {
        switch (op) {
            case FlashOperation::READ_PAGE: return "READ";
            case FlashOperation::PROGRAM_PAGE: return "PROGRAM";
            case FlashOperation::ERASE_BLOCK: return "ERASE";
            case FlashOperation::READ_STATUS: return "STATUS";
            case FlashOperation::RESET: return "RESET";
            default: return "UNKNOWN";
        }
    }
    
    // Load configuration from JSON file
    void load_configuration() {
        try {
            JsonConfig config(m_config_file);
            
            // Load Flash Controller configuration
            m_config.num_channels = 1; // Force to 1 channel for now
            m_config.dies_per_channel = config.get_int("ssd.flash.dies_per_channel", 4);
            m_config.command_queue_depth = config.get_int("ssd.flash.command_queue_depth", 16);
            m_config.page_size_kb = config.get_int("ssd.flash.page_size_kb", 16);
            m_config.pages_per_block = config.get_int("ssd.flash.pages_per_block", 128);
            m_config.blocks_per_die = config.get_int("ssd.flash.blocks_per_die", 1024);
            m_config.channel_bandwidth_mbps = config.get_double("ssd.flash.channel_bandwidth_mbps", 400.0);
            m_config.enable_interleaving = config.get_bool("ssd.flash.enable_interleaving", true);
            m_config.enable_wear_leveling = config.get_bool("ssd.flash.enable_wear_leveling", true);
            m_config.ecc_type = config.get_string("ssd.flash.ecc_type", "LDPC");
            
            if (m_debug_enable) {
                std::cout << "Flash Controller Configuration loaded:" << std::endl;
                std::cout << "  Channels: " << m_config.num_channels << std::endl;
                std::cout << "  Dies per Channel: " << m_config.dies_per_channel << std::endl;
                std::cout << "  ECC Type: " << m_config.ecc_type << std::endl;
                std::cout << "  Wear Leveling: " << (m_config.enable_wear_leveling ? "Enabled" : "Disabled") << std::endl;
            }
            
        } catch (const std::exception& e) {
            if (m_debug_enable) {
                std::cout << "Warning: Could not load Flash Controller config, using defaults: " 
                          << e.what() << std::endl;
            }
        }
    }
    
    // Constructor
    FlashController(sc_module_name name, const std::string& config_file = "config/base/ssd_config.json", 
                   bool debug_enable = false)
        : sc_module(name),
          m_debug_enable(debug_enable),
          m_config_file(config_file),
          m_total_flash_commands(0),
          m_completed_flash_commands(0),
          m_read_commands(0),
          m_write_commands(0),
          m_erase_commands(0),
          m_total_flash_latency_ns(0.0),
          m_channel_conflicts(0),
          m_rng(std::random_device{}()) {
        
        // Load configuration
        load_configuration();
        
        // Initialize channels
        m_channels.resize(m_config.num_channels);
        flash_out.resize(m_config.num_channels);
        flash_in.resize(m_config.num_channels);
        
        // Initialize erase counts
        uint32_t total_blocks = m_config.num_channels * m_config.dies_per_channel * m_config.blocks_per_die;
        m_erase_counts.resize(total_blocks, 0);
        m_block_dist = std::uniform_int_distribution<uint32_t>(0, total_blocks - 1);
        
        // Create FIFO connections for each channel
        for (uint32_t ch = 0; ch < m_config.num_channels; ch++) {
            flash_out[ch] = new sc_fifo_out<std::shared_ptr<FlashPacket>>();
            flash_in[ch] = new sc_fifo_in<std::shared_ptr<FlashPacket>>();
        }
        
        if (m_debug_enable) {
            std::cout << "0 s | " << basename() << ": Flash Controller initialized"
                      << " (" << m_config.num_channels << " channels"
                      << ", " << m_config.dies_per_channel << " dies/channel"
                      << ", ECC: " << m_config.ecc_type << ")" << std::endl;
        }
        
        // Start controller processes
        SC_THREAD(command_reception_process);
        SC_THREAD(channel_arbitration_process);
        SC_THREAD(completion_handling_process);
        SC_THREAD(wear_leveling_process);
    }
    
    // Destructor
    ~FlashController() {
        // Clean up dynamically allocated FIFOs
        for (auto* fifo : flash_out) {
            delete fifo;
        }
        for (auto* fifo : flash_in) {
            delete fifo;
        }
    }
    
public:
    // Statistics methods
    uint64_t get_total_flash_commands() const { return m_total_flash_commands; }
    uint64_t get_completed_flash_commands() const { return m_completed_flash_commands; }
    uint64_t get_read_commands() const { return m_read_commands; }
    uint64_t get_write_commands() const { return m_write_commands; }
    uint64_t get_erase_commands() const { return m_erase_commands; }
    uint64_t get_channel_conflicts() const { return m_channel_conflicts; }
    
    double get_average_flash_latency_ns() const {
        return (m_completed_flash_commands > 0) ? (m_total_flash_latency_ns / m_completed_flash_commands) : 0.0;
    }
    
    double get_channel_utilization(uint32_t channel) const {
        if (channel >= m_config.num_channels) return 0.0;
        const ChannelState& ch = m_channels[channel];
        return ch.busy ? 100.0 : 0.0; // Simplified utilization
    }
    
    // Configuration access
    const FlashControllerConfig& get_config() const { return m_config; }
    
    // Channel statistics
    const ChannelState& get_channel_state(uint32_t channel) const {
        static ChannelState empty_state;
        return (channel < m_config.num_channels) ? m_channels[channel] : empty_state;
    }
    
    // Performance metrics
    void print_statistics() const {
        std::cout << "\n======== Flash Controller Statistics ========" << std::endl;
        std::cout << "Total Flash Commands: " << m_total_flash_commands << std::endl;
        std::cout << "Completed Flash Commands: " << m_completed_flash_commands << std::endl;
        std::cout << "Read Commands: " << m_read_commands << std::endl;
        std::cout << "Write Commands: " << m_write_commands << std::endl;
        std::cout << "Erase Commands: " << m_erase_commands << std::endl;
        std::cout << "Channel Conflicts: " << m_channel_conflicts << std::endl;
        std::cout << "Average Flash Latency: " << std::fixed << std::setprecision(1) 
                  << get_average_flash_latency_ns() << " ns" << std::endl;
        
        std::cout << "\nChannel Statistics:" << std::endl;
        for (uint32_t ch = 0; ch < m_config.num_channels; ch++) {
            const ChannelState& channel = m_channels[ch];
            std::cout << "  Channel " << ch << ": " 
                      << channel.total_operations << " ops "
                      << "(R:" << channel.read_operations 
                      << " W:" << channel.write_operations 
                      << " E:" << channel.erase_operations << ")" << std::endl;
        }
        std::cout << "=============================================" << std::endl;
    }
};

// Type aliases for common configurations
using BasePacketFlashController = FlashController<BasePacket>;
using FlashPacketFlashController = FlashController<FlashPacket>;

#endif // FLASH_CONTROLLER_H