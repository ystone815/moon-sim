#ifndef DRAM_CONTROLLER_H
#define DRAM_CONTROLLER_H

#include <systemc.h>
#include <memory>
#include <queue>
#include <vector>
#include <random>
#include "packet/base_packet.h"
#include "common/json_config.h"

// DRAM timing parameters (based on DDR4-3200)
struct DramTiming {
    sc_time tCL;        // CAS Latency (Column Address Strobe)
    sc_time tRCD;       // RAS to CAS Delay
    sc_time tRP;        // Row Precharge time
    sc_time tRAS;       // Row Active time
    sc_time tWR;        // Write Recovery time
    sc_time tRFC;       // Refresh Cycle time
    sc_time tREFI;      // Refresh Interval
    sc_time tBurst;     // Burst duration
    
    // Default DDR4-3200 timings (approximate)
    DramTiming() 
        : tCL(sc_time(14, SC_NS)),      // 14 ns CAS latency
          tRCD(sc_time(14, SC_NS)),     // 14 ns RAS-to-CAS
          tRP(sc_time(14, SC_NS)),      // 14 ns Row precharge
          tRAS(sc_time(32, SC_NS)),     // 32 ns Row active
          tWR(sc_time(15, SC_NS)),      // 15 ns Write recovery
          tRFC(sc_time(350, SC_NS)),    // 350 ns Refresh cycle
          tREFI(sc_time(7800, SC_NS)),  // 7.8 μs Refresh interval
          tBurst(sc_time(4, SC_NS))     // 4 ns burst (8 beats at 3200 MT/s)
    {
    }
};

// DRAM command types
enum class DramCommand {
    ACTIVATE,       // Activate row (RAS)
    READ,          // Read from column (CAS)
    WRITE,         // Write to column (CAS) 
    PRECHARGE,     // Precharge row
    REFRESH,       // Refresh operation
    NOP            // No operation
};

// DRAM bank state
enum class BankState {
    IDLE,          // Bank is idle (precharged)
    ACTIVATING,    // Bank is being activated
    ACTIVE,        // Bank is active (row open)
    READING,       // Bank is processing read
    WRITING,       // Bank is processing write
    PRECHARGING,   // Bank is being precharged
    REFRESHING     // Bank is being refreshed
};

// DRAM bank structure
struct DramBank {
    BankState state;
    uint32_t active_row;           // Currently active row
    sc_time last_activate_time;    // For tRAS timing
    sc_time last_precharge_time;   // For tRP timing
    sc_time last_read_time;        // For tCL timing
    sc_time last_write_time;       // For tWR timing
    std::queue<std::shared_ptr<BasePacket>> pending_requests;
    
    DramBank() : state(BankState::IDLE), active_row(0xFFFFFFFF),
                 last_activate_time(SC_ZERO_TIME), last_precharge_time(SC_ZERO_TIME),
                 last_read_time(SC_ZERO_TIME), last_write_time(SC_ZERO_TIME) {}
};

// DRAM Controller statistics
struct DramStats {
    uint64_t total_requests;
    uint64_t read_requests; 
    uint64_t write_requests;
    uint64_t row_hits;
    uint64_t row_misses;
    uint64_t page_empty_hits;
    uint64_t refresh_cycles;
    uint64_t bank_conflicts;
    sc_time total_read_latency;
    sc_time total_write_latency;
    
    DramStats() : total_requests(0), read_requests(0), write_requests(0),
                  row_hits(0), row_misses(0), page_empty_hits(0), 
                  refresh_cycles(0), bank_conflicts(0),
                  total_read_latency(SC_ZERO_TIME), total_write_latency(SC_ZERO_TIME) {}
    
    double get_row_hit_rate() const {
        return total_requests > 0 ? (double)row_hits / total_requests : 0.0;
    }
    
    sc_time get_avg_read_latency() const {
        return read_requests > 0 ? sc_time(total_read_latency.to_seconds() / read_requests, SC_SEC) : SC_ZERO_TIME;
    }
    
    sc_time get_avg_write_latency() const {
        return write_requests > 0 ? sc_time(total_write_latency.to_seconds() / write_requests, SC_SEC) : SC_ZERO_TIME;
    }
};

// DRAM Controller template
template<int NUM_BANKS = 8, int NUM_RANKS = 1>
SC_MODULE(DramController) {
    SC_HAS_PROCESS(DramController);
    
    // SystemC ports
    sc_fifo_in<std::shared_ptr<BasePacket>> mem_in;    // From cache/CPU
    sc_fifo_out<std::shared_ptr<BasePacket>> mem_out;  // To cache/CPU
    
    // Configuration
    const DramTiming m_timing;
    const int m_page_size;          // DRAM page size in bytes
    const int m_burst_length;       // Burst length (typically 8)
    const bool m_auto_precharge;    // Auto precharge after read/write
    const bool m_refresh_enable;    // Enable periodic refresh
    const bool m_debug_enable;
    
    // Constructor
    DramController(sc_module_name name,
                   DramTiming timing = DramTiming(),
                   int page_size = 1024,              // 1KB page size
                   int burst_length = 8,
                   bool auto_precharge = true,
                   bool refresh_enable = true,
                   bool debug_enable = false)
        : sc_module(name),
          m_timing(timing),
          m_page_size(page_size),
          m_burst_length(burst_length),
          m_auto_precharge(auto_precharge),
          m_refresh_enable(refresh_enable),
          m_debug_enable(debug_enable),
          m_random_generator(std::random_device{}())
    {
        // Initialize banks
        m_banks.resize(NUM_BANKS * NUM_RANKS);
        for (auto& bank : m_banks) {
            bank = DramBank();
        }
        
        // Initialize statistics
        m_stats = DramStats();
        
        if (m_debug_enable) {
            std::cout << "DramController: Initialized with " << NUM_BANKS 
                      << " banks, " << NUM_RANKS << " ranks" << std::endl;
            std::cout << "  Page size: " << m_page_size << " bytes" << std::endl;
            std::cout << "  tCL: " << m_timing.tCL << ", tRCD: " << m_timing.tRCD << std::endl;
        }
        
        SC_THREAD(memory_controller_process);
        if (m_refresh_enable) {
            SC_THREAD(refresh_process);
        }
    }
    
    // JSON-based constructor
    DramController(sc_module_name name, const JsonConfig& config)
        : DramController(name,
                        create_timing_from_config(config),
                        config.get_int("page_size", 1024),
                        config.get_int("burst_length", 8),
                        config.get_bool("auto_precharge", true),
                        config.get_bool("refresh_enable", true),
                        config.get_bool("debug_enable", false))
    {
    }
    
    // Get DRAM statistics
    DramStats get_stats() const { return m_stats; }
    
    // Print DRAM statistics
    void print_stats() const {
        std::cout << "=== DRAM Controller Statistics ===" << std::endl;
        std::cout << "Total requests: " << m_stats.total_requests << std::endl;
        std::cout << "Read requests: " << m_stats.read_requests << std::endl;
        std::cout << "Write requests: " << m_stats.write_requests << std::endl;
        std::cout << "Row hits: " << m_stats.row_hits << " (" 
                  << (m_stats.get_row_hit_rate() * 100) << "%)" << std::endl;
        std::cout << "Row misses: " << m_stats.row_misses << std::endl;
        std::cout << "Page empty hits: " << m_stats.page_empty_hits << std::endl;
        std::cout << "Bank conflicts: " << m_stats.bank_conflicts << std::endl;
        std::cout << "Refresh cycles: " << m_stats.refresh_cycles << std::endl;
        std::cout << "Avg read latency: " << m_stats.get_avg_read_latency() << std::endl;
        std::cout << "Avg write latency: " << m_stats.get_avg_write_latency() << std::endl;
    }

private:
    // DRAM banks storage
    std::vector<DramBank> m_banks;
    
    // Statistics
    DramStats m_stats;
    
    // Random number generator
    std::mt19937 m_random_generator;
    
    // Helper function to create timing from config
    static DramTiming create_timing_from_config(const JsonConfig& config) {
        DramTiming timing;
        timing.tCL = sc_time(config.get_int("tCL_ns", 14), SC_NS);
        timing.tRCD = sc_time(config.get_int("tRCD_ns", 14), SC_NS);
        timing.tRP = sc_time(config.get_int("tRP_ns", 14), SC_NS);
        timing.tRAS = sc_time(config.get_int("tRAS_ns", 32), SC_NS);
        timing.tWR = sc_time(config.get_int("tWR_ns", 15), SC_NS);
        timing.tRFC = sc_time(config.get_int("tRFC_ns", 350), SC_NS);
        timing.tREFI = sc_time(config.get_int("tREFI_ns", 7800), SC_NS);
        timing.tBurst = sc_time(config.get_int("tBurst_ns", 4), SC_NS);
        return timing;
    }
    
    // Main memory controller process
    void memory_controller_process() {
        while (true) {
            // Wait for incoming memory request
            auto packet = mem_in.read();
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | DramController: Processing "
                          << (packet->get_command() == Command::READ ? "READ" : "WRITE")
                          << " address=0x" << std::hex << packet->get_address() << std::dec << std::endl;
            }
            
            // Track request start time for latency calculation
            sc_time request_start_time = sc_time_stamp();
            
            // Process DRAM request
            process_dram_request(packet);
            
            // Calculate and record latency
            sc_time latency = sc_time_stamp() - request_start_time;
            
            m_stats.total_requests++;
            if (packet->get_command() == Command::READ) {
                m_stats.read_requests++;
                m_stats.total_read_latency += latency;
            } else {
                m_stats.write_requests++;
                m_stats.total_write_latency += latency;
            }
            
            // Send response back
            mem_out.write(packet);
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | DramController: Completed request with latency " 
                          << latency << std::endl;
            }
        }
    }
    
    // Process DRAM request
    void process_dram_request(std::shared_ptr<BasePacket> packet) {
        uint32_t address = packet->get_address();
        int bank_id = get_bank_id(address);
        uint32_t row = get_row(address);
        uint32_t col = get_column(address);
        
        auto& bank = m_banks[bank_id];
        
        // Check for bank conflict
        if (bank.state != BankState::IDLE && bank.state != BankState::ACTIVE) {
            m_stats.bank_conflicts++;
            // Wait for bank to become available
            while (bank.state != BankState::IDLE && bank.state != BankState::ACTIVE) {
                wait(m_timing.tBurst);
            }
        }
        
        // Handle different scenarios
        if (bank.state == BankState::IDLE) {
            // Bank is idle, need to activate row
            activate_row(bank, row);
            m_stats.page_empty_hits++;
        } else if (bank.state == BankState::ACTIVE) {
            if (bank.active_row == row) {
                // Row hit - can proceed directly
                m_stats.row_hits++;
            } else {
                // Row miss - need to precharge and activate new row
                precharge_bank(bank);
                activate_row(bank, row);
                m_stats.row_misses++;
            }
        }
        
        // Perform read or write operation
        if (packet->get_command() == Command::READ) {
            perform_read(bank, col);
        } else {
            perform_write(bank, col);
        }
        
        // Auto precharge if enabled
        if (m_auto_precharge) {
            precharge_bank(bank);
        }
    }
    
    // Activate row in bank
    void activate_row(DramBank& bank, uint32_t row) {
        // Wait for tRP if needed
        sc_time time_since_precharge = sc_time_stamp() - bank.last_precharge_time;
        if (time_since_precharge < m_timing.tRP) {
            wait(m_timing.tRP - time_since_precharge);
        }
        
        bank.state = BankState::ACTIVATING;
        bank.active_row = row;
        bank.last_activate_time = sc_time_stamp();
        
        // Wait for tRCD (RAS to CAS delay)
        wait(m_timing.tRCD);
        
        bank.state = BankState::ACTIVE;
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | DramController: Activated row " 
                      << row << " in bank " << (&bank - &m_banks[0]) << std::endl;
        }
    }
    
    // Precharge bank
    void precharge_bank(DramBank& bank) {
        // Ensure minimum tRAS time has passed
        sc_time time_since_activate = sc_time_stamp() - bank.last_activate_time;
        if (time_since_activate < m_timing.tRAS) {
            wait(m_timing.tRAS - time_since_activate);
        }
        
        bank.state = BankState::PRECHARGING;
        
        // Wait for tRP (precharge time)
        wait(m_timing.tRP);
        
        bank.state = BankState::IDLE;
        bank.last_precharge_time = sc_time_stamp();
        bank.active_row = 0xFFFFFFFF;
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | DramController: Precharged bank " 
                      << (&bank - &m_banks[0]) << std::endl;
        }
    }
    
    // Perform read operation
    void perform_read(DramBank& bank, uint32_t column) {
        bank.state = BankState::READING;
        bank.last_read_time = sc_time_stamp();
        
        // Wait for CAS latency + burst time
        wait(m_timing.tCL + m_timing.tBurst);
        
        bank.state = BankState::ACTIVE;
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | DramController: Read from column " 
                      << column << std::endl;
        }
    }
    
    // Perform write operation  
    void perform_write(DramBank& bank, uint32_t column) {
        bank.state = BankState::WRITING;
        bank.last_write_time = sc_time_stamp();
        
        // Wait for write latency + burst time
        wait(m_timing.tBurst);
        
        bank.state = BankState::ACTIVE;
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | DramController: Write to column " 
                      << column << std::endl;
        }
    }
    
    // Refresh process (periodic background refresh)
    void refresh_process() {
        while (true) {
            wait(m_timing.tREFI);
            
            // Perform refresh on all banks
            for (auto& bank : m_banks) {
                if (bank.state == BankState::ACTIVE) {
                    precharge_bank(bank);
                }
                
                bank.state = BankState::REFRESHING;
                wait(m_timing.tRFC);
                bank.state = BankState::IDLE;
            }
            
            m_stats.refresh_cycles++;
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | DramController: Refresh cycle completed" << std::endl;
            }
        }
    }
    
    // Address mapping functions
    int get_bank_id(uint32_t address) const {
        // Simple bank interleaving: use middle bits
        return (address >> 6) & (NUM_BANKS - 1);  // Assumes NUM_BANKS is power of 2
    }
    
    uint32_t get_row(uint32_t address) const {
        // Row address from upper bits
        return address >> (6 + __builtin_ctz(NUM_BANKS) + __builtin_ctz(m_page_size/64));
    }
    
    uint32_t get_column(uint32_t address) const {
        // Column address from lower bits (excluding offset)
        int bank_bits = __builtin_ctz(NUM_BANKS);
        int offset_bits = 6; // 64-byte cache line
        return (address >> offset_bits) & ((m_page_size/64) - 1);
    }
};

#endif