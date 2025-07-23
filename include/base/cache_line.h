#ifndef CACHE_LINE_H
#define CACHE_LINE_H

#include <systemc.h>
#include <cstdint>

// Cache line states for MESI coherence protocol
enum class CacheLineState {
    INVALID,    // Cache line contains invalid data
    SHARED,     // Cache line is shared (read-only) across multiple caches
    EXCLUSIVE,  // Cache line is exclusive to this cache (clean)
    MODIFIED    // Cache line is modified (dirty) in this cache
};

// Cache line structure
template<int LINE_SIZE_BYTES = 64>
struct CacheLine {
    static const int SIZE = LINE_SIZE_BYTES;
    
    // Cache line data
    uint8_t data[LINE_SIZE_BYTES];
    
    // Cache line metadata
    uint32_t tag;                    // Address tag
    CacheLineState state;            // MESI state
    bool valid;                      // Valid bit
    bool dirty;                      // Dirty bit (for write-back)
    sc_time last_access_time;        // LRU tracking
    uint32_t access_count;           // Access frequency tracking
    
    // Constructor
    CacheLine() : tag(0), state(CacheLineState::INVALID), valid(false), 
                  dirty(false), last_access_time(SC_ZERO_TIME), access_count(0) {
        // Initialize data to zero
        for (int i = 0; i < LINE_SIZE_BYTES; ++i) {
            data[i] = 0;
        }
    }
    
    // Reset cache line
    void reset() {
        tag = 0;
        state = CacheLineState::INVALID;
        valid = false;
        dirty = false;
        last_access_time = SC_ZERO_TIME;
        access_count = 0;
        for (int i = 0; i < LINE_SIZE_BYTES; ++i) {
            data[i] = 0;
        }
    }
    
    // Check if cache line matches address
    bool matches(uint32_t address, int offset_bits, int index_bits) const {
        uint32_t address_tag = address >> (offset_bits + index_bits);
        return valid && (tag == address_tag);
    }
    
    // Update access time for LRU
    void update_access_time() {
        last_access_time = sc_time_stamp();
        access_count++;
    }
    
    // Get address from tag and index
    uint32_t get_address(int index, int offset_bits, int index_bits) const {
        return (tag << (offset_bits + index_bits)) | (index << offset_bits);
    }
};

// Cache replacement policies
enum class ReplacementPolicy {
    LRU,        // Least Recently Used
    FIFO,       // First In, First Out
    RANDOM,     // Random replacement
    LFU         // Least Frequently Used
};

// Cache write policies
enum class WritePolicy {
    WRITE_THROUGH,      // Write to cache and memory simultaneously
    WRITE_BACK,         // Write to cache, write to memory on eviction
    WRITE_AROUND        // Write directly to memory, bypass cache
};

// Cache allocation policies
enum class AllocationPolicy {
    WRITE_ALLOCATE,     // Allocate on write miss
    NO_WRITE_ALLOCATE   // Don't allocate on write miss
};

#endif