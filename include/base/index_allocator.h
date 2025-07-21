#ifndef INDEX_ALLOCATOR_H
#define INDEX_ALLOCATOR_H

#include <systemc.h>
#include <memory>
#include <set>
#include <functional>
#include "common/error_handling.h"

// Template-based IndexAllocator that works with any packet type
// Always allocates from minimum available index, handles out-of-order releases
template<typename PacketType>
SC_MODULE(IndexAllocator) {
    SC_HAS_PROCESS(IndexAllocator);
    
    // Template-based SystemC ports
    sc_fifo_in<std::shared_ptr<PacketType>> in;
    sc_fifo_out<std::shared_ptr<PacketType>> out;
    sc_fifo_in<std::shared_ptr<PacketType>> release_in; // For processed packets returning from Memory

    // Configuration parameters
    const unsigned int m_max_index;
    
    // Index setter function - allows flexible index assignment
    std::function<void(PacketType&, unsigned int)> m_index_setter;
    
    // Process methods
    void allocate_indices() {
        while (true) {
            auto packet = in.read();
            
            if (!packet) {
                SOC_SIM_ERROR("IndexAllocator", soc_sim::error::codes::INVALID_PACKET_TYPE, 
                             "Received null packet");
                continue;
            }
            
            // Wait for available index if pool is exhausted
            unsigned int allocated_index = allocate_index_blocking();
            
            // Set index using the setter function
            m_index_setter(*packet, allocated_index);
            
            // Update statistics
            m_total_allocated++;
            
            // Log allocation
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | IndexAllocator: Allocated index=" 
                          << allocated_index << ", in_use=" << m_allocated_indices.size() 
                          << ", " << *packet << std::endl;
            }
            
            // Forward packet
            out.write(packet);
            
            // Print statistics periodically (disabled for cleaner output)
            // if (m_total_allocated % 1000 == 0) {
            //     print_statistics();
            // }
        }
    }
    
    void release_indices() {
        while (true) {
            auto packet = release_in.read();
            
            if (!packet) {
                SOC_SIM_ERROR("IndexAllocator", soc_sim::error::codes::INVALID_PACKET_TYPE, 
                             "Received null packet in release path");
                continue;
            }
            
            // Get index from packet
            unsigned int released_index = static_cast<unsigned int>(packet->get_attribute("index"));
            
            // Release the index (remove from allocated set)
            m_allocated_indices.erase(released_index);
            
            // Update statistics
            m_total_deallocated++;
            
            // Log release
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | IndexAllocator: Released index=" 
                          << released_index << ", in_use=" << m_allocated_indices.size() 
                          << ", " << *packet << std::endl;
            }
            
            // Signal that an index is now available
            m_index_available.notify();
        }
    }

    // Constructor with custom index setter
    IndexAllocator(sc_module_name name, 
                  unsigned int max_index,
                  std::function<void(PacketType&, unsigned int)> index_setter,
                  bool debug_enable = false)
        : sc_module(name), 
          m_max_index(max_index),
          m_index_setter(index_setter),
          m_debug_enable(debug_enable),
          m_total_allocated(0),
          m_total_deallocated(0) {
        
        SC_THREAD(allocate_indices);
        SC_THREAD(release_indices);
        
        SOC_SIM_INFO("IndexAllocator", "INIT", 
                     "Initialized simple index allocator with max_index=" + std::to_string(max_index));
    }
    
    // Constructor for types with set_attribute method
    IndexAllocator(sc_module_name name, 
                  unsigned int max_index = 1024,
                  bool debug_enable = false)
        : IndexAllocator(name, max_index,
                        [](PacketType& packet, unsigned int index) {
                            packet.set_attribute("index", static_cast<double>(index));
                        }, debug_enable) {}

private:
    // Configuration
    const bool m_debug_enable;
    
    // Internal state - tracks allocated indices for minimum allocation
    std::set<unsigned int> m_allocated_indices;
    
    // Statistics
    unsigned int m_total_allocated;
    unsigned int m_total_deallocated;
    
    // Synchronization for blocking allocation
    sc_event m_index_available;
    
    // Blocking allocation - waits if no indices available
    unsigned int allocate_index_blocking() {
        while (true) {
            if (m_allocated_indices.size() < m_max_index) {
                return allocate_minimum_index();
            }
            // Wait for an index to become available
            wait(m_index_available);
        }
    }
    
    // Find and allocate the minimum available index
    unsigned int allocate_minimum_index() {
        // Find the smallest index not in the allocated set
        for (unsigned int i = 0; i < m_max_index; ++i) {
            if (m_allocated_indices.find(i) == m_allocated_indices.end()) {
                m_allocated_indices.insert(i);
                return i;
            }
        }
        
        // Should never reach here if can_allocate() check is correct
        handle_allocation_error("No available indices found");
        return 0;
    }
    
    void print_statistics() const {
        std::cout << sc_time_stamp() << " | IndexAllocator Statistics:" << std::endl;
        std::cout << "  Total allocated: " << m_total_allocated << std::endl;
        std::cout << "  Total deallocated: " << m_total_deallocated << std::endl;
        std::cout << "  Current in use: " << m_allocated_indices.size() << std::endl;
        std::cout << "  Available slots: " << (m_max_index - m_allocated_indices.size()) << std::endl;
    }
    
    void handle_allocation_error(const std::string& reason) {
        SOC_SIM_ERROR("IndexAllocator", soc_sim::error::codes::RESOURCE_EXHAUSTED, 
                      "Index allocation failed: " + reason);
    }
};

// Type alias for backward compatibility
using BasePacketIndexAllocator = IndexAllocator<BasePacket>;

// Helper function to create IndexAllocator with custom index setter
template<typename PacketType>
IndexAllocator<PacketType>* create_index_allocator(
    sc_module_name name,
    unsigned int max_index,
    std::function<void(PacketType&, unsigned int)> index_setter) {
    
    return new IndexAllocator<PacketType>(name, max_index, index_setter);
}

#endif