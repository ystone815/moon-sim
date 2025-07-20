#ifndef INDEX_ALLOCATOR_H
#define INDEX_ALLOCATOR_H

#include <systemc.h>
#include <memory>
#include <queue>
#include <set>
#include <random>
#include <functional>
#include "common/error_handling.h"

// Index allocation policies
enum class AllocationPolicy {
    SEQUENTIAL,     // Sequential allocation (0, 1, 2, ...)
    ROUND_ROBIN,    // Round-robin with reuse
    RANDOM,         // Random index allocation
    POOL_BASED      // Pool-based allocation with free list
};

// Template-based IndexAllocator that works with any packet type
template<typename PacketType>
SC_MODULE(IndexAllocator) {
    SC_HAS_PROCESS(IndexAllocator);
    
    // Template-based SystemC ports
    sc_fifo_in<std::shared_ptr<PacketType>> in;
    sc_fifo_out<std::shared_ptr<PacketType>> out;

    // Configuration parameters
    const AllocationPolicy m_policy;
    const unsigned int m_max_index;
    const bool m_enable_reuse;
    
    // Index setter function - allows flexible index assignment
    std::function<void(PacketType&, unsigned int)> m_index_setter;
    
    // Process method
    void allocate_indices() {
        while (true) {
            auto packet = in.read();
            
            if (!packet) {
                SOC_SIM_ERROR("IndexAllocator", soc_sim::error::codes::INVALID_PACKET_TYPE, 
                             "Received null packet");
                continue;
            }
            
            // Allocate index
            unsigned int allocated_index = allocate_index();
            
            // Set index using the setter function
            m_index_setter(*packet, allocated_index);
            
            // Update statistics
            m_total_allocated++;
            m_current_in_use++;
            
            // Log allocation
            std::cout << sc_time_stamp() << " | IndexAllocator: Allocated index=" 
                      << allocated_index << ", " << *packet << std::endl;
            
            // Forward packet
            out.write(packet);
            
            // Print statistics periodically
            if (m_total_allocated % 100 == 0) {
                print_statistics();
            }
        }
    }

    // Constructor with custom index setter
    IndexAllocator(sc_module_name name, 
                  AllocationPolicy policy,
                  unsigned int max_index,
                  bool enable_reuse,
                  std::function<void(PacketType&, unsigned int)> index_setter)
        : sc_module(name), 
          m_policy(policy), 
          m_max_index(max_index),
          m_enable_reuse(enable_reuse),
          m_index_setter(index_setter),
          m_next_sequential_index(0),
          m_round_robin_index(0),
          m_random_generator(std::random_device{}()),
          m_index_dist(0, max_index - 1),
          m_total_allocated(0),
          m_total_deallocated(0),
          m_current_in_use(0) {
        
        SC_THREAD(allocate_indices);
        
        if (m_policy == AllocationPolicy::POOL_BASED) {
            initialize_free_pool();
        }
        
        SOC_SIM_INFO("IndexAllocator", "INIT", 
                     "Initialized template version with policy=" + std::to_string(static_cast<int>(m_policy)));
    }
    
    // Constructor for types with set_attribute method
    IndexAllocator(sc_module_name name, 
                  AllocationPolicy policy = AllocationPolicy::SEQUENTIAL,
                  unsigned int max_index = 1024,
                  bool enable_reuse = true)
        : IndexAllocator(name, policy, max_index, enable_reuse,
                        [](PacketType& packet, unsigned int index) {
                            packet.set_attribute("index", static_cast<double>(index));
                        }) {}

private:
    // Internal state for different allocation policies
    unsigned int m_next_sequential_index;
    unsigned int m_round_robin_index;
    std::queue<unsigned int> m_free_pool;
    std::set<unsigned int> m_allocated_indices;
    
    // Random number generation for RANDOM policy
    std::mt19937 m_random_generator;
    std::uniform_int_distribution<unsigned int> m_index_dist;
    
    // Statistics
    unsigned int m_total_allocated;
    unsigned int m_total_deallocated;
    unsigned int m_current_in_use;
    
    // Core allocation logic (same as before)
    unsigned int allocate_index() {
        unsigned int index = 0;
        
        switch (m_policy) {
            case AllocationPolicy::SEQUENTIAL:
                index = m_next_sequential_index;
                m_next_sequential_index = (m_next_sequential_index + 1) % m_max_index;
                break;
                
            case AllocationPolicy::ROUND_ROBIN:
                index = m_round_robin_index;
                m_round_robin_index = (m_round_robin_index + 1) % m_max_index;
                break;
                
            case AllocationPolicy::RANDOM:
                index = m_index_dist(m_random_generator);
                break;
                
            case AllocationPolicy::POOL_BASED:
                if (!m_free_pool.empty()) {
                    index = m_free_pool.front();
                    m_free_pool.pop();
                } else {
                    handle_allocation_error("Free pool exhausted");
                    index = m_next_sequential_index;
                    m_next_sequential_index = (m_next_sequential_index + 1) % m_max_index;
                }
                break;
                
            default:
                handle_allocation_error("Unknown allocation policy");
                index = 0;
                break;
        }
        
        if (m_enable_reuse && (m_policy == AllocationPolicy::ROUND_ROBIN || 
                              m_policy == AllocationPolicy::POOL_BASED)) {
            m_allocated_indices.insert(index);
        }
        
        return index;
    }
    
    void initialize_free_pool() {
        for (unsigned int i = 0; i < m_max_index; ++i) {
            m_free_pool.push(i);
        }
    }
    
    void print_statistics() const {
        std::cout << sc_time_stamp() << " | IndexAllocator Statistics:" << std::endl;
        std::cout << "  Total allocated: " << m_total_allocated << std::endl;
        std::cout << "  Current in use: " << m_current_in_use << std::endl;
        std::cout << "  Free pool size: " << m_free_pool.size() << std::endl;
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
    AllocationPolicy policy,
    unsigned int max_index,
    bool enable_reuse,
    std::function<void(PacketType&, unsigned int)> index_setter) {
    
    return new IndexAllocator<PacketType>(name, policy, max_index, enable_reuse, index_setter);
}

#endif