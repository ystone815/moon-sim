#ifndef MEMORY_H
#define MEMORY_H

#include <systemc.h>
#include <memory>
#include <array>
#include <functional>
#include <random>
#include <algorithm>
#include "common/error_handling.h"

// Memory entry structure - can be customized per use case
template<typename DataType>
struct MemoryEntry {
    DataType data;
    unsigned char databyte;
    bool valid;
    
    MemoryEntry() : data{}, databyte(0), valid(false) {}
};

// Template-based Memory that works with any packet type
template<typename PacketType, typename DataType = int, size_t MemorySize = 256>
SC_MODULE(Memory) {
    SC_HAS_PROCESS(Memory);
    
    // Template-based SystemC ports
    sc_fifo_in<std::shared_ptr<PacketType>> in;
    sc_fifo_out<std::shared_ptr<PacketType>> release_out; // For sending processed packets back to IndexAllocator

    // Memory configuration
    static constexpr size_t MEMORY_SIZE = MemorySize;
    const bool m_debug_enable;
    
    // Delay parameters
    const double m_min_delay_ns;
    const double m_max_delay_ns;
    const double m_mean_delay_ns;
    const double m_stddev_delay_ns;
    
    // Accessor functions for packet attributes
    std::function<int(const PacketType&)> m_get_command;
    std::function<int(const PacketType&)> m_get_address;
    std::function<DataType(const PacketType&)> m_get_data;
    std::function<unsigned char(const PacketType&)> m_get_databyte;
    std::function<void(PacketType&, DataType)> m_set_data;
    std::function<void(PacketType&, unsigned char)> m_set_databyte;
    
    // Random number generation for normal distribution
    mutable std::mt19937 m_rng;
    mutable std::normal_distribution<double> m_normal_dist;
    
    // Memory operation types (customizable)
    enum class MemoryCommand {
        READ = 0,
        WRITE = 1
    };

    // Process method
    void run() {
        while (true) {
            auto packet = in.read();
            
            if (!packet) {
                SOC_SIM_ERROR("Memory", soc_sim::error::codes::INVALID_PACKET_TYPE, 
                             "Received null packet");
                continue;
            }
            
            // Get packet attributes using accessor functions
            int command = m_get_command(*packet);
            int address = m_get_address(*packet);
            
            // Bounds checking
            if (address < 0 || static_cast<size_t>(address) >= MEMORY_SIZE) {
                SOC_SIM_ERROR("Memory", soc_sim::error::codes::ADDRESS_OUT_OF_BOUNDS,
                             "Address out of bounds: " + std::to_string(address) + 
                             " (valid range: 0-" + std::to_string(MEMORY_SIZE-1) + ")");
                continue;
            }
            
            // Apply memory access delay
            double delay_ns = generate_delay();
            if (delay_ns > 0.0) {
                wait(delay_ns, SC_NS);
            }
            
            // Process memory operation
            if (command == static_cast<int>(MemoryCommand::WRITE)) {
                DataType data = m_get_data(*packet);
                unsigned char databyte = m_get_databyte(*packet);
                
                mem[address].data = data;
                mem[address].databyte = databyte;
                mem[address].valid = true;
                
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | Memory: Received WRITE, " 
                              << *packet << " (delay=" << std::fixed << std::setprecision(1) << delay_ns << "ns)" << std::endl;
                }
                          
            } else if (command == static_cast<int>(MemoryCommand::READ)) {
                if (mem[address].valid) {
                    m_set_data(*packet, mem[address].data);
                    m_set_databyte(*packet, mem[address].databyte);
                } else {
                    // Return default values for uninitialized memory
                    m_set_data(*packet, DataType{});
                    m_set_databyte(*packet, 0);
                }
                
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | Memory: Received READ, " 
                              << *packet << " (delay=" << std::fixed << std::setprecision(1) << delay_ns << "ns)" << std::endl;
                }
            } else {
                SOC_SIM_ERROR("Memory", soc_sim::error::codes::INVALID_PACKET_TYPE,
                             "Unknown command: " + std::to_string(command));
            }
            
            // Send processed packet to release path for index deallocation
            release_out.write(packet);
        }
    }

    // Constructor with custom accessor functions
    Memory(sc_module_name name,
           std::function<int(const PacketType&)> get_command,
           std::function<int(const PacketType&)> get_address,
           std::function<DataType(const PacketType&)> get_data,
           std::function<unsigned char(const PacketType&)> get_databyte,
           std::function<void(PacketType&, DataType)> set_data,
           std::function<void(PacketType&, unsigned char)> set_databyte,
           bool debug_enable = false,
           double min_delay_ns = 0.0,
           double max_delay_ns = 0.0)
        : sc_module(name),
          m_debug_enable(debug_enable),
          m_min_delay_ns(min_delay_ns),
          m_max_delay_ns(max_delay_ns),
          m_mean_delay_ns((min_delay_ns + max_delay_ns) / 2.0),
          m_stddev_delay_ns((max_delay_ns > min_delay_ns) ? (max_delay_ns - min_delay_ns) / 6.0 : 0.0),
          m_get_command(get_command),
          m_get_address(get_address),
          m_get_data(get_data),
          m_get_databyte(get_databyte),
          m_set_data(set_data),
          m_set_databyte(set_databyte),
          m_rng(std::random_device{}()),
          m_normal_dist(0.0, 1.0) {
        
        // Initialize memory
        for (size_t i = 0; i < MEMORY_SIZE; ++i) {
            mem[i] = MemoryEntry<DataType>();
        }
        
        if (m_debug_enable && m_max_delay_ns > 0.0) {
            std::cout << "0 s | " << basename() << ": Memory delay range: " 
                      << m_min_delay_ns << " - " << m_max_delay_ns << " ns (normal distribution)" << std::endl;
        }
        
        SC_THREAD(run);
    }
    
    // Constructor for types with standard accessor methods
    Memory(sc_module_name name, bool debug_enable = false, double min_delay_ns = 0.0, double max_delay_ns = 0.0) 
        : sc_module(name), 
          m_debug_enable(debug_enable),
          m_min_delay_ns(min_delay_ns),
          m_max_delay_ns(max_delay_ns),
          m_mean_delay_ns((min_delay_ns + max_delay_ns) / 2.0),
          m_stddev_delay_ns((max_delay_ns > min_delay_ns) ? (max_delay_ns - min_delay_ns) / 6.0 : 0.0),
          m_rng(std::random_device{}()),
          m_normal_dist(0.0, 1.0) {
        // Set up default accessors for types with standard methods
        m_get_command = [](const PacketType& p) { return static_cast<int>(p.get_command()); };
        m_get_address = [](const PacketType& p) { return p.get_address(); };
        m_get_data = [](const PacketType& p) { return static_cast<DataType>(p.get_data()); };
        m_get_databyte = [](const PacketType& p) { return p.get_databyte(); };
        m_set_data = [](PacketType& p, DataType data) { p.set_data(static_cast<int>(data)); };
        m_set_databyte = [](PacketType& p, unsigned char databyte) { p.set_databyte(databyte); };
        
        // Initialize memory
        for (size_t i = 0; i < MEMORY_SIZE; ++i) {
            mem[i] = MemoryEntry<DataType>();
        }
        
        if (m_debug_enable && m_max_delay_ns > 0.0) {
            std::cout << "0 s | " << basename() << ": Memory delay range: " 
                      << m_min_delay_ns << " - " << m_max_delay_ns << " ns (normal distribution)" << std::endl;
        }
        
        SC_THREAD(run);
    }

    // Memory access methods for testing/debugging
    const MemoryEntry<DataType>& get_memory_entry(size_t address) const {
        return mem[address];
    }
    
    void set_memory_entry(size_t address, const MemoryEntry<DataType>& entry) {
        if (address < MEMORY_SIZE) {
            mem[address] = entry;
        }
    }
    
    // Statistics
    size_t get_memory_size() const { return MEMORY_SIZE; }
    
    size_t count_valid_entries() const {
        size_t count = 0;
        for (const auto& entry : mem) {
            if (entry.valid) count++;
        }
        return count;
    }

private:
    std::array<MemoryEntry<DataType>, MEMORY_SIZE> mem;
    
    // Generate delay using normal distribution
    double generate_delay() const {
        if (m_max_delay_ns <= 0.0 || m_max_delay_ns <= m_min_delay_ns) {
            return 0.0;  // No delay
        }
        
        // Generate normal distribution sample and clamp to [min, max]
        double sample = m_normal_dist(m_rng) * m_stddev_delay_ns + m_mean_delay_ns;
        return std::max(m_min_delay_ns, std::min(m_max_delay_ns, sample));
    }
};

// Type aliases for common configurations
using BasePacketMemory = Memory<BasePacket, int, 256>;
using IntMemory256 = Memory<BasePacket, int, 256>;
using FloatMemory1K = Memory<BasePacket, float, 1024>;

// Helper function to create Memory with custom accessors
template<typename PacketType, typename DataType = int, size_t MemorySize = 256>
Memory<PacketType, DataType, MemorySize>* create_memory(
    sc_module_name name,
    std::function<int(const PacketType&)> get_command,
    std::function<int(const PacketType&)> get_address,
    std::function<DataType(const PacketType&)> get_data,
    std::function<unsigned char(const PacketType&)> get_databyte,
    std::function<void(PacketType&, DataType)> set_data,
    std::function<void(PacketType&, unsigned char)> set_databyte) {
    
    return new Memory<PacketType, DataType, MemorySize>(
        name, get_command, get_address, get_data, get_databyte, set_data, set_databyte);
}

#endif