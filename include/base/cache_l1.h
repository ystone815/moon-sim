#ifndef CACHE_L1_H
#define CACHE_L1_H

#include <systemc.h>
#include <memory>
#include <vector>
#include <random>
#include <algorithm>
#include <iomanip>
#include "packet/base_packet.h"
#include "packet/generic_packet.h"
#include "cache_line.h"
#include "common/json_config.h"

// L1 Cache statistics
struct CacheStats {
    uint64_t total_accesses;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    uint64_t writebacks;
    
    CacheStats() : total_accesses(0), hits(0), misses(0), evictions(0), writebacks(0) {}
    
    double get_hit_rate() const {
        return total_accesses > 0 ? (double)hits / total_accesses : 0.0;
    }
    
    double get_miss_rate() const {
        return total_accesses > 0 ? (double)misses / total_accesses : 0.0;
    }
};

// L1 Cache template class
template<int CACHE_SIZE_KB = 32, int LINE_SIZE_BYTES = 64, int ASSOCIATIVITY = 4>
SC_MODULE(CacheL1) {
    SC_HAS_PROCESS(CacheL1);
    
    // Cache parameters
    static const int CACHE_SIZE = CACHE_SIZE_KB * 1024;
    static const int LINE_SIZE = LINE_SIZE_BYTES;
    static const int NUM_LINES = CACHE_SIZE / LINE_SIZE;
    static const int NUM_SETS = NUM_LINES / ASSOCIATIVITY;
    static const int WAYS = ASSOCIATIVITY;
    
    // Address bit calculations
    static const int OFFSET_BITS = __builtin_ctz(LINE_SIZE);  // log2(LINE_SIZE)
    static const int INDEX_BITS = __builtin_ctz(NUM_SETS);   // log2(NUM_SETS)
    static const int TAG_BITS = 32 - OFFSET_BITS - INDEX_BITS;
    
    // SystemC ports
    sc_fifo_in<std::shared_ptr<BasePacket>> cpu_in;     // From CPU/HostSystem
    sc_fifo_out<std::shared_ptr<BasePacket>> cpu_out;   // To CPU/HostSystem
    sc_fifo_out<std::shared_ptr<BasePacket>> mem_out;   // To L2/Memory
    sc_fifo_in<std::shared_ptr<BasePacket>> mem_in;     // From L2/Memory
    
    // Configuration
    const ReplacementPolicy m_replacement_policy;
    const WritePolicy m_write_policy;
    const AllocationPolicy m_allocation_policy;
    const sc_time m_hit_latency;
    const sc_time m_miss_latency;
    const bool m_debug_enable;
    
    // Constructor
    CacheL1(sc_module_name name, 
            ReplacementPolicy replacement_policy = ReplacementPolicy::LRU,
            WritePolicy write_policy = WritePolicy::WRITE_BACK,
            AllocationPolicy allocation_policy = AllocationPolicy::WRITE_ALLOCATE,
            sc_time hit_latency = sc_time(1, SC_NS),
            sc_time miss_latency = sc_time(10, SC_NS),
            bool debug_enable = false)
        : sc_module(name),
          m_replacement_policy(replacement_policy),
          m_write_policy(write_policy),
          m_allocation_policy(allocation_policy),
          m_hit_latency(hit_latency),
          m_miss_latency(miss_latency),
          m_debug_enable(debug_enable),
          m_random_generator(std::random_device{}())
    {
        // Initialize cache sets
        m_cache_sets.resize(NUM_SETS);
        for (int i = 0; i < NUM_SETS; ++i) {
            m_cache_sets[i].resize(WAYS);
        }
        
        // Initialize statistics
        m_stats = CacheStats();
        
        if (m_debug_enable) {
            std::cout << "CacheL1: Initialized " << CACHE_SIZE_KB << "KB L1 cache" << std::endl;
            std::cout << "  Sets: " << NUM_SETS << ", Ways: " << WAYS << ", Line Size: " << LINE_SIZE << std::endl;
            std::cout << "  Index bits: " << INDEX_BITS << ", Offset bits: " << OFFSET_BITS << std::endl;
        }
        
        SC_THREAD(cache_process);
    }
    
    // JSON-based constructor
    CacheL1(sc_module_name name, const JsonConfig& config)
        : CacheL1(name,
                  parse_replacement_policy(config.get_string("replacement_policy", "LRU")),
                  parse_write_policy(config.get_string("write_policy", "WRITE_BACK")),
                  parse_allocation_policy(config.get_string("allocation_policy", "WRITE_ALLOCATE")),
                  sc_time(config.get_int("hit_latency_ns", 1), SC_NS),
                  sc_time(config.get_int("miss_latency_ns", 10), SC_NS),
                  config.get_bool("debug_enable", false))
    {
    }
    
    // Get cache statistics
    CacheStats get_stats() const { return m_stats; }
    
    // Print cache statistics
    void print_stats() const {
        std::cout << "=== L1 Cache Statistics ===" << std::endl;
        std::cout << "Total accesses: " << m_stats.total_accesses << std::endl;
        std::cout << "Hits: " << m_stats.hits << " (" << (m_stats.get_hit_rate() * 100) << "%)" << std::endl;
        std::cout << "Misses: " << m_stats.misses << " (" << (m_stats.get_miss_rate() * 100) << "%)" << std::endl;
        std::cout << "Evictions: " << m_stats.evictions << std::endl;
        std::cout << "Writebacks: " << m_stats.writebacks << std::endl;
    }

private:
    // Cache storage: sets x ways
    std::vector<std::vector<CacheLine<LINE_SIZE_BYTES>>> m_cache_sets;
    
    // Statistics
    CacheStats m_stats;
    
    // Random number generator for random replacement
    std::mt19937 m_random_generator;
    
    // Helper functions for policy parsing
    ReplacementPolicy parse_replacement_policy(const std::string& policy) {
        if (policy == "FIFO") return ReplacementPolicy::FIFO;
        if (policy == "RANDOM") return ReplacementPolicy::RANDOM;
        if (policy == "LFU") return ReplacementPolicy::LFU;
        return ReplacementPolicy::LRU; // Default
    }
    
    WritePolicy parse_write_policy(const std::string& policy) {
        if (policy == "WRITE_THROUGH") return WritePolicy::WRITE_THROUGH;
        if (policy == "WRITE_AROUND") return WritePolicy::WRITE_AROUND;
        return WritePolicy::WRITE_BACK; // Default
    }
    
    AllocationPolicy parse_allocation_policy(const std::string& policy) {
        if (policy == "NO_WRITE_ALLOCATE") return AllocationPolicy::NO_WRITE_ALLOCATE;
        return AllocationPolicy::WRITE_ALLOCATE; // Default
    }
    
    // Main cache process
    void cache_process() {
        while (true) {
            // Wait for incoming CPU request
            auto packet = cpu_in.read();
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | CacheL1: Processing " 
                          << (packet->get_command() == Command::READ ? "READ" : "WRITE")
                          << " address=0x" << std::hex << packet->get_address() << std::dec << std::endl;
            }
            
            // Process cache access
            bool hit = process_cache_access(packet);
            
            m_stats.total_accesses++;
            
            if (hit) {
                m_stats.hits++;
                wait(m_hit_latency);
                
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | CacheL1: HIT - returning to CPU" << std::endl;
                }
                
                // Send response back to CPU
                cpu_out.write(packet);
            } else {
                m_stats.misses++;
                wait(m_miss_latency);
                
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | CacheL1: MISS - forwarding to memory" << std::endl;
                }
                
                // Forward to next level (L2/Memory)
                mem_out.write(packet);
                
                // Wait for response from memory
                auto response = mem_in.read();
                
                // Handle cache line fill
                handle_cache_fill(response);
                
                // Send response back to CPU
                cpu_out.write(response);
            }
        }
    }
    
    // Process cache access (hit/miss detection)
    bool process_cache_access(std::shared_ptr<BasePacket> packet) {
        uint32_t address = packet->get_address();
        int set_index = get_set_index(address);
        int way = find_way(address, set_index);
        
        if (way != -1) {
            // Cache hit
            auto& line = m_cache_sets[set_index][way];
            line.update_access_time();
            
            if (packet->get_command() == Command::WRITE) {
                // Handle write hit
                if (m_write_policy == WritePolicy::WRITE_BACK) {
                    line.dirty = true;
                    line.state = CacheLineState::MODIFIED;
                } else if (m_write_policy == WritePolicy::WRITE_THROUGH) {
                    // Write through to next level
                    auto generic_packet = std::static_pointer_cast<GenericPacket>(packet);
                    auto write_packet = std::make_shared<GenericPacket>(*generic_packet);
                    mem_out.write(write_packet);
                }
            }
            
            return true; // Cache hit
        } else {
            // Cache miss
            return false;
        }
    }
    
    // Handle cache line fill on miss
    void handle_cache_fill(std::shared_ptr<BasePacket> packet) {
        uint32_t address = packet->get_address();
        int set_index = get_set_index(address);
        int victim_way = select_victim_way(set_index);
        
        auto& victim_line = m_cache_sets[set_index][victim_way];
        
        // Handle eviction if line is dirty
        if (victim_line.valid && victim_line.dirty && 
            m_write_policy == WritePolicy::WRITE_BACK) {
            m_stats.writebacks++;
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | CacheL1: Writing back dirty line" << std::endl;
            }
            
            // Create writeback packet (in real implementation)
            // For now, just count the writeback
        }
        
        if (victim_line.valid) {
            m_stats.evictions++;
        }
        
        // Fill cache line
        victim_line.tag = get_tag(address);
        victim_line.valid = true;
        victim_line.dirty = (packet->get_command() == Command::WRITE && 
                           m_write_policy == WritePolicy::WRITE_BACK);
        victim_line.state = (packet->get_command() == Command::WRITE) ? 
                           CacheLineState::MODIFIED : CacheLineState::EXCLUSIVE;
        victim_line.update_access_time();
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | CacheL1: Filled cache line in set " 
                      << set_index << ", way " << victim_way << std::endl;
        }
    }
    
    // Address parsing functions
    int get_set_index(uint32_t address) const {
        return (address >> OFFSET_BITS) & ((1 << INDEX_BITS) - 1);
    }
    
    uint32_t get_tag(uint32_t address) const {
        return address >> (OFFSET_BITS + INDEX_BITS);
    }
    
    int get_offset(uint32_t address) const {
        return address & ((1 << OFFSET_BITS) - 1);
    }
    
    // Find way in set that matches address
    int find_way(uint32_t address, int set_index) const {
        uint32_t tag = get_tag(address);
        for (int way = 0; way < WAYS; ++way) {
            const auto& line = m_cache_sets[set_index][way];
            if (line.valid && line.tag == tag) {
                return way;
            }
        }
        return -1; // Not found
    }
    
    // Select victim way for eviction
    int select_victim_way(int set_index) {
        auto& set = m_cache_sets[set_index];
        
        // First, try to find an invalid line
        for (int way = 0; way < WAYS; ++way) {
            if (!set[way].valid) {
                return way;
            }
        }
        
        // All lines valid, apply replacement policy
        switch (m_replacement_policy) {
            case ReplacementPolicy::LRU: {
                int lru_way = 0;
                sc_time oldest_time = set[0].last_access_time;
                for (int way = 1; way < WAYS; ++way) {
                    if (set[way].last_access_time < oldest_time) {
                        oldest_time = set[way].last_access_time;
                        lru_way = way;
                    }
                }
                return lru_way;
            }
            
            case ReplacementPolicy::LFU: {
                int lfu_way = 0;
                uint32_t min_count = set[0].access_count;
                for (int way = 1; way < WAYS; ++way) {
                    if (set[way].access_count < min_count) {
                        min_count = set[way].access_count;
                        lfu_way = way;
                    }
                }
                return lfu_way;
            }
            
            case ReplacementPolicy::RANDOM: {
                std::uniform_int_distribution<int> dist(0, WAYS - 1);
                return dist(m_random_generator);
            }
            
            case ReplacementPolicy::FIFO:
            default: {
                // Simple FIFO approximation using access time
                int fifo_way = 0;
                sc_time oldest_time = set[0].last_access_time;
                for (int way = 1; way < WAYS; ++way) {
                    if (set[way].last_access_time < oldest_time) {
                        oldest_time = set[way].last_access_time;
                        fifo_way = way;
                    }
                }
                return fifo_way;
            }
        }
    }
};

#endif