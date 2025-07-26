#ifndef SSD_CONTROLLER_H
#define SSD_CONTROLLER_H

#include <systemc.h>
#include <memory>
#include <queue>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include "packet/base_packet.h"
#include "packet/pcie_packet.h"
#include "common/json_config.h"
#include "common/error_handling.h"

// SSD Controller configuration
struct SSDControllerConfig {
    uint32_t command_queue_depth;     // Maximum outstanding commands
    uint32_t completion_queue_depth;  // Completion queue depth
    uint32_t max_data_transfer_size;  // Maximum data transfer size in bytes
    double command_processing_time_ns; // Time to process a command
    bool enable_ncq;                  // Native Command Queuing support
    uint32_t max_lba_range;           // Maximum LBA address range
    std::string controller_type;       // AHCI, NVMe, etc.
    
    SSDControllerConfig() : command_queue_depth(32), completion_queue_depth(32),
                          max_data_transfer_size(65536), command_processing_time_ns(100.0),
                          enable_ncq(true), max_lba_range(0xFFFFFFFF), controller_type("NVMe") {}
};

// Command state tracking
enum class CommandState {
    SUBMITTED,      // Command submitted to controller
    QUEUED,         // Command queued for processing
    PROCESSING,     // Command being processed by storage hierarchy
    COMPLETED,      // Command completed
    ERROR          // Command encountered error
};

// Internal command tracking structure
struct SSDCommand {
    std::shared_ptr<BasePacket> packet;
    CommandState state;
    sc_time submit_time;
    sc_time start_time;
    sc_time completion_time;
    uint32_t command_id;
    uint64_t lba;
    uint32_t transfer_size;
    
    SSDCommand(std::shared_ptr<BasePacket> pkt, uint32_t id) 
        : packet(pkt), state(CommandState::SUBMITTED), submit_time(sc_time_stamp()),
          start_time(SC_ZERO_TIME), completion_time(SC_ZERO_TIME), command_id(id),
          lba(pkt->get_address()), transfer_size(pkt->get_databyte()) {}
};

// SSD Controller SystemC Module
template<typename PacketType = BasePacket>
SC_MODULE(SSDController) {
    SC_HAS_PROCESS(SSDController);
    
    // PCIe interface ports (from Host)
    sc_fifo_in<std::shared_ptr<PacketType>> pcie_in;     // Commands from Host via PCIe
    sc_fifo_out<std::shared_ptr<PacketType>> pcie_out;   // Completions to Host via PCIe
    
    // Storage hierarchy interface ports
    sc_fifo_out<std::shared_ptr<PacketType>> storage_out; // Commands to Cache/DRAM/Flash hierarchy
    sc_fifo_in<std::shared_ptr<PacketType>> storage_in;   // Completions from storage hierarchy
    
    // Configuration and debug
    const bool m_debug_enable;
    const std::string m_config_file;
    SSDControllerConfig m_config;
    
    // Command tracking
    std::queue<std::shared_ptr<SSDCommand>> m_command_queue;
    std::unordered_map<uint32_t, std::shared_ptr<SSDCommand>> m_active_commands;
    uint32_t m_next_command_id;
    
    // Event for command queue notifications
    sc_event m_command_queued;
    
    // Statistics
    uint64_t m_total_commands;
    uint64_t m_completed_commands;
    uint64_t m_error_commands;
    uint64_t m_total_bytes_transferred;
    double m_total_latency_ns;
    uint64_t m_queue_full_count;
    
    // Main controller processes
    void command_submission_process() {
        while (true) {
            // Wait for command from PCIe interface
            auto packet = pcie_in.read();
            
            if (!packet) {
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | SSDController: Received null packet" << std::endl;
                }
                continue;
            }
            
            m_total_commands++;
            
            // Create internal command structure
            auto command = std::make_shared<SSDCommand>(packet, m_next_command_id++);
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | SSDController: Received command ID " 
                          << command->command_id << ", LBA 0x" << std::hex << command->lba 
                          << std::dec << ", Size " << command->transfer_size << " bytes" << std::endl;
            }
            
            // Check if command queue has space
            if (m_command_queue.size() >= m_config.command_queue_depth) {
                m_queue_full_count++;
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | SSDController: Command queue full, waiting..." << std::endl;
                }
                // In real hardware, this would cause backpressure to host
                // For simulation, we'll wait until space is available
                while (m_command_queue.size() >= m_config.command_queue_depth) {
                    wait(10, SC_NS); // Small delay to prevent busy waiting
                }
            }
            
            // Validate LBA range
            if (command->lba > m_config.max_lba_range) {
                command->state = CommandState::ERROR;
                m_error_commands++;
                
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | SSDController: LBA out of range error for command " 
                              << command->command_id << std::endl;
                }
                
                // Send error completion back to host
                send_completion(command, false);
                continue;
            }
            
            // Add to command queue
            command->state = CommandState::QUEUED;
            m_command_queue.push(command);
            
            // Notify dispatch process that command is available
            m_command_queued.notify();
            
            // Simulate command processing overhead
            wait(m_config.command_processing_time_ns, SC_NS);
        }
    }
    
    void command_dispatch_process() {
        while (true) {
            // Wait for command available event
            if (m_command_queue.empty()) {
                wait(m_command_queued);
            }
            
            // Get next command from queue
            auto command = m_command_queue.front();
            m_command_queue.pop();
            
            command->state = CommandState::PROCESSING;
            command->start_time = sc_time_stamp();
            
            // Add to active commands tracking
            m_active_commands[command->command_id] = command;
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | SSDController: Dispatching command ID " 
                          << command->command_id << " to storage hierarchy" << std::endl;
            }
            
            // Forward packet to storage hierarchy
            storage_out.write(command->packet);
            
            // Update statistics
            m_total_bytes_transferred += command->transfer_size;
        }
    }
    
    void completion_handling_process() {
        while (true) {
            // Wait for completion from storage hierarchy
            auto completed_packet = storage_in.read();
            
            if (!completed_packet) {
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | SSDController: Received null completion" << std::endl;
                }
                continue;
            }
            
            // Find corresponding active command
            // Note: In real implementation, we'd use packet tagging/correlation
            // For simplicity, we'll match based on address
            std::shared_ptr<SSDCommand> command = nullptr;
            for (auto it = m_active_commands.begin(); it != m_active_commands.end(); ++it) {
                if (it->second->packet->get_address() == completed_packet->get_address()) {
                    command = it->second;
                    break;
                }
            }
            
            if (!command) {
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | SSDController: No matching command found for completion" << std::endl;
                }
                continue;
            }
            
            // Update command state and timing
            command->state = CommandState::COMPLETED;
            command->completion_time = sc_time_stamp();
            
            // Calculate latency
            double latency_ns = (command->completion_time - command->submit_time).to_seconds() * 1e9;
            m_total_latency_ns += latency_ns;
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | SSDController: Command ID " 
                          << command->command_id << " completed, latency " 
                          << std::fixed << std::setprecision(1) << latency_ns << " ns" << std::endl;
            }
            
            // Send completion to host
            send_completion(command, true);
            
            // Remove from active commands
            m_active_commands.erase(command->command_id);
            m_completed_commands++;
        }
    }
    
    void send_completion(std::shared_ptr<SSDCommand> command, bool success) {
        // Update packet status if needed
        if (!success) {
            // Mark packet as error (implementation specific)
        }
        
        // Send completion back to host
        pcie_out.write(command->packet);
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | SSDController: Sent " 
                      << (success ? "successful" : "error") << " completion for command ID " 
                      << command->command_id << std::endl;
        }
    }
    
    // Load configuration from JSON file
    void load_configuration() {
        try {
            JsonConfig config(m_config_file);
            
            // Load SSD Controller configuration
            m_config.command_queue_depth = config.get_int("ssd.controller.command_queue_depth", 32);
            m_config.completion_queue_depth = config.get_int("ssd.controller.completion_queue_depth", 32);
            m_config.max_data_transfer_size = config.get_int("ssd.controller.max_data_transfer_size", 65536);
            m_config.command_processing_time_ns = config.get_double("ssd.controller.command_processing_time_ns", 100.0);
            m_config.enable_ncq = config.get_bool("ssd.controller.enable_ncq", true);
            m_config.controller_type = config.get_string("ssd.controller.controller_type", "NVMe");
            
            if (m_debug_enable) {
                std::cout << "SSD Controller Configuration loaded:" << std::endl;
                std::cout << "  Command Queue Depth: " << m_config.command_queue_depth << std::endl;
                std::cout << "  Controller Type: " << m_config.controller_type << std::endl;
                std::cout << "  NCQ Enabled: " << (m_config.enable_ncq ? "Yes" : "No") << std::endl;
            }
            
        } catch (const std::exception& e) {
            if (m_debug_enable) {
                std::cout << "Warning: Could not load SSD Controller config, using defaults: " 
                          << e.what() << std::endl;
            }
        }
    }
    
    // Constructor
    SSDController(sc_module_name name, const std::string& config_file = "config/base/ssd_config.json", 
                  bool debug_enable = false)
        : sc_module(name),
          m_debug_enable(debug_enable),
          m_config_file(config_file),
          m_next_command_id(1),
          m_total_commands(0),
          m_completed_commands(0),
          m_error_commands(0),
          m_total_bytes_transferred(0),
          m_total_latency_ns(0.0),
          m_queue_full_count(0) {
        
        // Load configuration
        load_configuration();
        
        if (m_debug_enable) {
            std::cout << "0 s | " << basename() << ": SSD Controller initialized"
                      << " (Type: " << m_config.controller_type
                      << ", Queue Depth: " << m_config.command_queue_depth << ")" << std::endl;
        }
        
        // Start controller processes
        SC_THREAD(command_submission_process);
        SC_THREAD(command_dispatch_process);
        SC_THREAD(completion_handling_process);
    }
    
public:
    // Statistics methods
    uint64_t get_total_commands() const { return m_total_commands; }
    uint64_t get_completed_commands() const { return m_completed_commands; }
    uint64_t get_error_commands() const { return m_error_commands; }
    uint64_t get_active_commands() const { return m_active_commands.size(); }
    uint64_t get_queued_commands() const { return m_command_queue.size(); }
    uint64_t get_total_bytes_transferred() const { return m_total_bytes_transferred; }
    uint64_t get_queue_full_count() const { return m_queue_full_count; }
    
    double get_average_latency_ns() const {
        return (m_completed_commands > 0) ? (m_total_latency_ns / m_completed_commands) : 0.0;
    }
    
    double get_command_completion_rate() const {
        return (m_total_commands > 0) ? (static_cast<double>(m_completed_commands) / m_total_commands) : 0.0;
    }
    
    // Configuration access
    const SSDControllerConfig& get_config() const { return m_config; }
    
    // Performance metrics
    void print_statistics() const {
        std::cout << "\n======== SSD Controller Statistics ========" << std::endl;
        std::cout << "Total Commands: " << m_total_commands << std::endl;
        std::cout << "Completed Commands: " << m_completed_commands << std::endl;
        std::cout << "Error Commands: " << m_error_commands << std::endl;
        std::cout << "Active Commands: " << m_active_commands.size() << std::endl;
        std::cout << "Queued Commands: " << m_command_queue.size() << std::endl;
        std::cout << "Total Bytes Transferred: " << m_total_bytes_transferred << std::endl;
        std::cout << "Queue Full Events: " << m_queue_full_count << std::endl;
        std::cout << "Average Latency: " << std::fixed << std::setprecision(1) 
                  << get_average_latency_ns() << " ns" << std::endl;
        std::cout << "Completion Rate: " << std::setprecision(1) 
                  << get_command_completion_rate() * 100.0 << "%" << std::endl;
        std::cout << "===========================================" << std::endl;
    }
};

// Type aliases for common configurations
using BasePacketSSDController = SSDController<BasePacket>;
using PCIePacketSSDController = SSDController<PCIePacket>;

#endif // SSD_CONTROLLER_H