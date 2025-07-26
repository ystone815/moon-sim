#ifndef DRAM_CONTROLLER_H
#define DRAM_CONTROLLER_H

#include <systemc.h>
#include <memory>
#include <queue>
#include <vector>
#include <random>
#include "packet/base_packet.h"
#include "common/json_config.h"

/*
 * MOON-SIM: Modular Object-Oriented Network Simulator
 * DRAM Controller - Multi-standard memory controller (DDR4/DDR5/LPDDR5)
 */

// Memory technology types
enum class MemoryType {
    DDR4,       // Double Data Rate 4
    DDR5,       // Double Data Rate 5  
    LPDDR5      // Low Power DDR5
};

// Refresh schemes for different memory types
enum class RefreshScheme {
    ALL_BANK_REFRESH,       // Traditional refresh - all banks refreshed together (DDR4)
    SAME_BANK_REFRESH,      // Same bank group refresh (DDR5/LPDDR5 optimization)
    PER_BANK_REFRESH,       // Individual bank refresh (DDR5/LPDDR5 fine-grained)
    DISTRIBUTED_REFRESH,    // Distributed refresh across time (LPDDR5 power optimization)
    REFRESH_MANAGEMENT_UNIT // RMU-based refresh (DDR5 advanced)
};

// Speed grades for different memory types
enum class SpeedGrade {
    // DDR4 speed grades
    DDR4_2400,  // DDR4-2400 (1200 MHz)
    DDR4_2666,  // DDR4-2666 (1333 MHz)
    DDR4_3200,  // DDR4-3200 (1600 MHz)
    DDR4_4266,  // DDR4-4266 (2133 MHz)
    
    // DDR5 speed grades  
    DDR5_4800,  // DDR5-4800 (2400 MHz)
    DDR5_5600,  // DDR5-5600 (2800 MHz)
    DDR5_6400,  // DDR5-6400 (3200 MHz)
    DDR5_8400,  // DDR5-8400 (4200 MHz)
    
    // LPDDR5 speed grades
    LPDDR5_5500,  // LPDDR5-5500 (2750 MHz)
    LPDDR5_6400,  // LPDDR5-6400 (3200 MHz)
    LPDDR5_7500,  // LPDDR5-7500 (3750 MHz)
    LPDDR5_8533   // LPDDR5-8533 (4266 MHz)
};

// DRAM timing parameters
struct DramTiming {
    sc_time tCL;        // CAS Latency (Column Address Strobe)
    sc_time tRCD;       // RAS to CAS Delay
    sc_time tRP;        // Row Precharge time
    sc_time tRAS;       // Row Active time
    sc_time tWR;        // Write Recovery time
    sc_time tRFC;       // Refresh Cycle time
    sc_time tREFI;      // Refresh Interval
    sc_time tBurst;     // Burst duration
    
    // Bank Group specific timings (DDR5/LPDDR5)
    sc_time tCCDL;      // CAS to CAS Delay (Long) - different bank group
    sc_time tCCDS;      // CAS to CAS Delay (Short) - same bank group
    sc_time tRRDL;      // Row to Row Delay (Long) - different bank group
    sc_time tRRDS;      // Row to Row Delay (Short) - same bank group
    
    // Refresh scheme specific timings
    RefreshScheme refresh_scheme;   // Refresh strategy
    sc_time tRFCab;     // All Bank Refresh Cycle time
    sc_time tRFCsb;     // Same Bank Refresh Cycle time  
    sc_time tRFCpb;     // Per Bank Refresh Cycle time
    sc_time tREFIpb;    // Per Bank Refresh Interval
    uint32_t refresh_granularity;   // Number of rows refreshed per operation
    
    // Default DDR4-3200 timings (approximate)
    DramTiming() 
        : tCL(sc_time(14, SC_NS)),      // 14 ns CAS latency
          tRCD(sc_time(14, SC_NS)),     // 14 ns RAS-to-CAS
          tRP(sc_time(14, SC_NS)),      // 14 ns Row precharge
          tRAS(sc_time(32, SC_NS)),     // 32 ns Row active
          tWR(sc_time(15, SC_NS)),      // 15 ns Write recovery
          tRFC(sc_time(350, SC_NS)),    // 350 ns Refresh cycle
          tREFI(sc_time(7800, SC_NS)),  // 7.8 μs Refresh interval
          tBurst(sc_time(4, SC_NS)),    // 4 ns burst (8 beats at 3200 MT/s)
          // DDR4 doesn't use bank groups, set to same values
          tCCDL(sc_time(4, SC_NS)),     // Same as tBurst for DDR4
          tCCDS(sc_time(4, SC_NS)),     // Same as tBurst for DDR4
          tRRDL(sc_time(6, SC_NS)),     // Row-to-row delay
          tRRDS(sc_time(4, SC_NS)),     // Row-to-row delay (same bank)
          // DDR4 uses traditional all-bank refresh
          refresh_scheme(RefreshScheme::ALL_BANK_REFRESH),
          tRFCab(sc_time(350, SC_NS)),  // All bank refresh cycle
          tRFCsb(sc_time(350, SC_NS)),  // Same as all bank for DDR4
          tRFCpb(sc_time(90, SC_NS)),   // Per bank refresh (if supported)
          tREFIpb(sc_time(1950, SC_NS)), // Per bank refresh interval (tREFI/4)
          refresh_granularity(8192)     // 8K rows per refresh
    {
    }
    
    // Constructor with memory type and speed grade
    DramTiming(MemoryType type, SpeedGrade grade);
};

// DRAM Timing Factory for different memory types
class DramTimingFactory {
public:
    // Create timing configuration for specific memory type and speed
    static DramTiming create(MemoryType type, SpeedGrade grade);
    
    // Create timing with default speed for each type
    static DramTiming createDDR4(SpeedGrade grade = SpeedGrade::DDR4_3200);
    static DramTiming createDDR5(SpeedGrade grade = SpeedGrade::DDR5_4800);
    static DramTiming createLPDDR5(SpeedGrade grade = SpeedGrade::LPDDR5_6400);
    
    // Get default speed grade for memory type
    static SpeedGrade getDefaultSpeedGrade(MemoryType type);
    
    // Validate speed grade compatibility with memory type
    static bool isSpeedGradeValid(MemoryType type, SpeedGrade grade);
    
    // Convert speed grade to string for debugging
    static std::string speedGradeToString(SpeedGrade grade);
    static std::string memoryTypeToString(MemoryType type);

private:
    // Internal timing calculation helpers
    static DramTiming createDDR4Timing(SpeedGrade grade);
    static DramTiming createDDR5Timing(SpeedGrade grade);
    static DramTiming createLPDDR5Timing(SpeedGrade grade);
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

// DRAM bank structure with Bank Group support
struct DramBank {
    BankState state;
    uint32_t active_row;           // Currently active row
    uint32_t bank_group_id;        // Bank Group ID (DDR5/LPDDR5)
    uint32_t bank_id;              // Bank ID within group
    sc_time last_activate_time;    // For tRAS timing
    sc_time last_precharge_time;   // For tRP timing
    sc_time last_read_time;        // For tCL timing
    sc_time last_write_time;       // For tWR timing
    std::queue<std::shared_ptr<BasePacket>> pending_requests;
    
    DramBank() : state(BankState::IDLE), active_row(0xFFFFFFFF),
                 bank_group_id(0), bank_id(0),
                 last_activate_time(SC_ZERO_TIME), last_precharge_time(SC_ZERO_TIME),
                 last_read_time(SC_ZERO_TIME), last_write_time(SC_ZERO_TIME) {}
                 
    DramBank(uint32_t bg_id, uint32_t b_id) : state(BankState::IDLE), active_row(0xFFFFFFFF),
                 bank_group_id(bg_id), bank_id(b_id),
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
    
    // Refresh scheme specific statistics
    uint64_t all_bank_refreshes;
    uint64_t same_bank_refreshes;
    uint64_t per_bank_refreshes;
    uint64_t distributed_refreshes;
    sc_time total_refresh_latency;
    uint64_t refresh_conflicts;     // Commands delayed due to refresh
    
    DramStats() : total_requests(0), read_requests(0), write_requests(0),
                  row_hits(0), row_misses(0), page_empty_hits(0), 
                  refresh_cycles(0), bank_conflicts(0),
                  total_read_latency(SC_ZERO_TIME), total_write_latency(SC_ZERO_TIME),
                  all_bank_refreshes(0), same_bank_refreshes(0), per_bank_refreshes(0),
                  distributed_refreshes(0), total_refresh_latency(SC_ZERO_TIME), refresh_conflicts(0) {}
    
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

// DRAM Controller template with Bank Group support
template<int NUM_BANKS = 8, int NUM_BANK_GROUPS = 1, int NUM_RANKS = 1>
SC_MODULE(DramController) {
    SC_HAS_PROCESS(DramController);
    
    // SystemC ports
    sc_fifo_in<std::shared_ptr<BasePacket>> mem_in;    // From cache/CPU
    sc_fifo_out<std::shared_ptr<BasePacket>> mem_out;  // To cache/CPU
    
    // Configuration
    const DramTiming m_timing;
    const MemoryType m_memory_type;  // Memory type for bank group logic
    const int m_page_size;          // DRAM page size in bytes
    const int m_burst_length;       // Burst length (typically 8)
    const bool m_auto_precharge;    // Auto precharge after read/write
    const bool m_refresh_enable;    // Enable periodic refresh
    const bool m_debug_enable;
    
    // Constructor
    DramController(sc_module_name name,
                   DramTiming timing = DramTiming(),
                   MemoryType memory_type = MemoryType::DDR4,
                   int page_size = 1024,              // 1KB page size
                   int burst_length = 8,
                   bool auto_precharge = true,
                   bool refresh_enable = true,
                   bool debug_enable = false)
        : sc_module(name),
          m_timing(timing),
          m_memory_type(memory_type),
          m_page_size(page_size),
          m_burst_length(burst_length),
          m_auto_precharge(auto_precharge),
          m_refresh_enable(refresh_enable),
          m_debug_enable(debug_enable),
          m_random_generator(std::random_device{}())
    {
        // Initialize banks with Bank Group support
        init_banks_with_groups();
        
        // Initialize statistics
        m_stats = DramStats();
        
        if (m_debug_enable) {
            std::cout << "MOON-SIM DramController: Initialized " << DramTimingFactory::memoryTypeToString(m_memory_type) << std::endl;
            std::cout << "  Banks: " << NUM_BANKS << ", Bank Groups: " << NUM_BANK_GROUPS << ", Ranks: " << NUM_RANKS << std::endl;
            std::cout << "  Page size: " << m_page_size << " bytes" << std::endl;
            std::cout << "  tCL: " << m_timing.tCL << ", tRCD: " << m_timing.tRCD << std::endl;
            if (has_bank_groups()) {
                std::cout << "  Bank Group timings - tCCDL: " << m_timing.tCCDL << ", tCCDS: " << m_timing.tCCDS << std::endl;
            }
        }
        
        SC_THREAD(memory_controller_process);
        if (m_refresh_enable) {
            SC_THREAD(refresh_process);
        }
    }
    
    // JSON-based constructor with automatic memory type detection
    DramController(sc_module_name name, const JsonConfig& config)
        : DramController(name,
                        create_timing_from_config(config),
                        parseMemoryType(config.get_string("dram.memory_type", "DDR4")),
                        config.get_int("dram.page_size", 1024),
                        config.get_int("dram.burst_length", 8),
                        config.get_bool("dram.auto_precharge", true),
                        config.get_bool("dram.refresh_enable", true),
                        config.get_bool("dram.debug_enable", false))
    {
        // Print memory configuration info
        if (config.get_bool("dram.debug_enable", false)) {
            auto memory_type = parseMemoryType(config.get_string("dram.memory_type", "DDR4"));
            auto speed_grade = parseSpeedGrade(config.get_string("dram.speed_grade", "DDR4_3200"));
            
            std::cout << "MOON-SIM DRAM Controller initialized:" << std::endl;
            std::cout << "  Memory Type: " << DramTimingFactory::memoryTypeToString(memory_type) << std::endl;
            std::cout << "  Speed Grade: " << DramTimingFactory::speedGradeToString(speed_grade) << std::endl;
            std::cout << "  Banks: " << NUM_BANKS << ", Ranks: " << NUM_RANKS << std::endl;
        }
    }
    
    // Memory type-based constructor
    DramController(sc_module_name name, 
                   MemoryType memory_type = MemoryType::DDR4,
                   SpeedGrade speed_grade = SpeedGrade::DDR4_3200,
                   int page_size = 1024,
                   int burst_length = 8,
                   bool auto_precharge = true,
                   bool refresh_enable = true,
                   bool debug_enable = false)
        : DramController(name,
                        DramTimingFactory::create(memory_type, speed_grade),
                        page_size,
                        burst_length,
                        auto_precharge,
                        refresh_enable,
                        debug_enable)
    {
        if (debug_enable) {
            std::cout << "MOON-SIM DRAM Controller initialized:" << std::endl;
            std::cout << "  Memory Type: " << DramTimingFactory::memoryTypeToString(memory_type) << std::endl;
            std::cout << "  Speed Grade: " << DramTimingFactory::speedGradeToString(speed_grade) << std::endl;
            std::cout << "  Banks: " << NUM_BANKS << ", Ranks: " << NUM_RANKS << std::endl;
        }
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
    
    // Bank Group helper functions
    void init_banks_with_groups() {
        // Total banks = NUM_BANKS * NUM_BANK_GROUPS * NUM_RANKS
        int total_banks = NUM_BANKS * NUM_BANK_GROUPS * NUM_RANKS;
        m_banks.resize(total_banks);
        
        // Initialize banks with Bank Group and Bank IDs
        for (int rank = 0; rank < NUM_RANKS; rank++) {
            for (int bg = 0; bg < NUM_BANK_GROUPS; bg++) {
                for (int bank = 0; bank < NUM_BANKS; bank++) {
                    int bank_index = rank * (NUM_BANK_GROUPS * NUM_BANKS) + bg * NUM_BANKS + bank;
                    m_banks[bank_index] = DramBank(bg, bank);
                }
            }
        }
    }
    
    bool has_bank_groups() const {
        return (m_memory_type == MemoryType::DDR5 || m_memory_type == MemoryType::LPDDR5) 
               && NUM_BANK_GROUPS > 1;
    }
    
    bool same_bank_group(const DramBank& bank1, const DramBank& bank2) const {
        return bank1.bank_group_id == bank2.bank_group_id;
    }
    
    sc_time get_cas_to_cas_delay(const DramBank& bank1, const DramBank& bank2) const {
        if (!has_bank_groups()) {
            return m_timing.tBurst; // DDR4 behavior
        }
        return same_bank_group(bank1, bank2) ? m_timing.tCCDS : m_timing.tCCDL;
    }
    
    sc_time get_row_to_row_delay(const DramBank& bank1, const DramBank& bank2) const {
        if (!has_bank_groups()) {
            return m_timing.tRRDL; // DDR4 behavior
        }
        return same_bank_group(bank1, bank2) ? m_timing.tRRDS : m_timing.tRRDL;
    }
    
    // Helper function to create timing from config
    static DramTiming create_timing_from_config(const JsonConfig& config) {
        // Check if custom timings are enabled
        if (config.get_bool("dram.custom_timings.enable_custom", false)) {
            // Use custom timing values
            return create_timing_from_custom_config(config);
        } 
        
        // Check if using predefined configuration from memory_configs
        std::string selected_config = config.get_string("dram.selected_config", "");
        if (!selected_config.empty()) {
            return create_timing_from_predefined_config(config, selected_config);
        }
        
        // Fallback: Use automatic timing based on memory type and speed grade
        auto memory_type = parseMemoryType(config.get_string("dram.memory_type", "DDR4"));
        auto speed_grade = parseSpeedGrade(config.get_string("dram.speed_grade", "DDR4_3200"));
        return DramTimingFactory::create(memory_type, speed_grade);
    }
    
    static DramTiming create_timing_from_custom_config(const JsonConfig& config) {
        DramTiming timing;
        timing.tCL = sc_time(config.get_double("dram.custom_timings.tCL_ns", 14), SC_NS);
        timing.tRCD = sc_time(config.get_double("dram.custom_timings.tRCD_ns", 14), SC_NS);
        timing.tRP = sc_time(config.get_double("dram.custom_timings.tRP_ns", 14), SC_NS);
        timing.tRAS = sc_time(config.get_double("dram.custom_timings.tRAS_ns", 32), SC_NS);
        timing.tWR = sc_time(config.get_double("dram.custom_timings.tWR_ns", 15), SC_NS);
        timing.tRFC = sc_time(config.get_double("dram.custom_timings.tRFC_ns", 350), SC_NS);
        timing.tREFI = sc_time(config.get_double("dram.custom_timings.tREFI_ns", 7800), SC_NS);
        timing.tBurst = sc_time(config.get_double("dram.custom_timings.tBurst_ns", 4), SC_NS);
        timing.tCCDL = sc_time(config.get_double("dram.custom_timings.tCCDL_ns", 4), SC_NS);
        timing.tCCDS = sc_time(config.get_double("dram.custom_timings.tCCDS_ns", 4), SC_NS);
        timing.tRRDL = sc_time(config.get_double("dram.custom_timings.tRRDL_ns", 6), SC_NS);
        timing.tRRDS = sc_time(config.get_double("dram.custom_timings.tRRDS_ns", 4), SC_NS);
        return timing;
    }
    
    static DramTiming create_timing_from_predefined_config(const JsonConfig& config, const std::string& config_name) {
        std::string prefix = "dram.memory_configs." + config_name + ".ac_parameters.";
        
        DramTiming timing;
        timing.tCL = sc_time(config.get_double(prefix + "tCL_ns", 14), SC_NS);
        timing.tRCD = sc_time(config.get_double(prefix + "tRCD_ns", 14), SC_NS);
        timing.tRP = sc_time(config.get_double(prefix + "tRP_ns", 14), SC_NS);
        timing.tRAS = sc_time(config.get_double(prefix + "tRAS_ns", 32), SC_NS);
        timing.tWR = sc_time(config.get_double(prefix + "tWR_ns", 15), SC_NS);
        timing.tRFC = sc_time(config.get_double(prefix + "tRFC_ns", 350), SC_NS);
        timing.tREFI = sc_time(config.get_double(prefix + "tREFI_ns", 7800), SC_NS);
        timing.tBurst = sc_time(config.get_double(prefix + "tBurst_ns", 4), SC_NS);
        timing.tCCDL = sc_time(config.get_double(prefix + "tCCDL_ns", 4), SC_NS);
        timing.tCCDS = sc_time(config.get_double(prefix + "tCCDS_ns", 4), SC_NS);
        timing.tRRDL = sc_time(config.get_double(prefix + "tRRDL_ns", 6), SC_NS);
        timing.tRRDS = sc_time(config.get_double(prefix + "tRRDS_ns", 4), SC_NS);
        return timing;
    }
    
    // Helper functions to parse memory type and speed grade from strings
    static MemoryType parseMemoryType(const std::string& type_str) {
        if (type_str == "DDR4") return MemoryType::DDR4;
        if (type_str == "DDR5") return MemoryType::DDR5;
        if (type_str == "LPDDR5") return MemoryType::LPDDR5;
        
        std::cerr << "Warning: Unknown memory type '" << type_str << "', defaulting to DDR4" << std::endl;
        return MemoryType::DDR4;
    }
    
    static SpeedGrade parseSpeedGrade(const std::string& grade_str) {
        if (grade_str == "DDR4_2400") return SpeedGrade::DDR4_2400;
        if (grade_str == "DDR4_2666") return SpeedGrade::DDR4_2666;
        if (grade_str == "DDR4_3200") return SpeedGrade::DDR4_3200;
        if (grade_str == "DDR4_4266") return SpeedGrade::DDR4_4266;
        if (grade_str == "DDR5_4800") return SpeedGrade::DDR5_4800;
        if (grade_str == "DDR5_5600") return SpeedGrade::DDR5_5600;
        if (grade_str == "DDR5_6400") return SpeedGrade::DDR5_6400;
        if (grade_str == "DDR5_8400") return SpeedGrade::DDR5_8400;
        if (grade_str == "LPDDR5_5500") return SpeedGrade::LPDDR5_5500;
        if (grade_str == "LPDDR5_6400") return SpeedGrade::LPDDR5_6400;
        if (grade_str == "LPDDR5_7500") return SpeedGrade::LPDDR5_7500;
        if (grade_str == "LPDDR5_8533") return SpeedGrade::LPDDR5_8533;
        
        std::cerr << "Warning: Unknown speed grade '" << grade_str << "', defaulting to DDR4_3200" << std::endl;
        return SpeedGrade::DDR4_3200;
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
    
    // Refresh process with multiple refresh schemes
    void refresh_process() {
        uint32_t refresh_counter = 0;
        uint32_t distributed_bank_index = 0;
        
        while (true) {
            sc_time refresh_interval = get_refresh_interval();
            wait(refresh_interval);
            
            switch (m_timing.refresh_scheme) {
                case RefreshScheme::ALL_BANK_REFRESH:
                    perform_all_bank_refresh();
                    break;
                    
                case RefreshScheme::SAME_BANK_REFRESH:
                    perform_same_bank_refresh(refresh_counter % NUM_BANK_GROUPS);
                    break;
                    
                case RefreshScheme::PER_BANK_REFRESH:
                    perform_per_bank_refresh(refresh_counter % get_total_banks());
                    break;
                    
                case RefreshScheme::DISTRIBUTED_REFRESH:
                    perform_distributed_refresh(distributed_bank_index);
                    distributed_bank_index = (distributed_bank_index + 1) % get_total_banks();
                    break;
                    
                case RefreshScheme::REFRESH_MANAGEMENT_UNIT:
                    perform_rmu_refresh(refresh_counter);
                    break;
                    
                default:
                    perform_all_bank_refresh(); // Fallback
                    break;
            }
            
            refresh_counter++;
            m_stats.refresh_cycles++;
            
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | DramController: " 
                          << refresh_scheme_to_string(m_timing.refresh_scheme) 
                          << " refresh cycle " << refresh_counter << " completed" << std::endl;
            }
        }
    }
    
    // All Bank Refresh - traditional DDR4 approach
    void perform_all_bank_refresh() {
        // Check for refresh conflicts (commands in progress)
        bool conflict_detected = false;
        for (const auto& bank : m_banks) {
            if (bank.state != BankState::IDLE && bank.state != BankState::REFRESHING) {
                conflict_detected = true;
                break;
            }
        }
        
        if (conflict_detected) {
            m_stats.refresh_conflicts++;
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | DramController: Refresh conflict detected, waiting..." << std::endl;
            }
            // Wait for ongoing operations to complete
            wait(m_timing.tBurst * 2);
        }
        
        sc_time refresh_start_time = sc_time_stamp();
        
        // Precharge all active banks
        for (auto& bank : m_banks) {
            if (bank.state == BankState::ACTIVE) {
                precharge_bank(bank);
            }
            bank.state = BankState::REFRESHING;
        }
        
        // Wait for all-bank refresh cycle time
        wait(m_timing.tRFCab);
        
        // Return all banks to idle state
        for (auto& bank : m_banks) {
            bank.state = BankState::IDLE;
        }
        
        m_stats.all_bank_refreshes++;
        m_stats.total_refresh_latency += (sc_time_stamp() - refresh_start_time);
    }
    
    // Same Bank Refresh - DDR5/LPDDR5 optimization
    void perform_same_bank_refresh(uint32_t bank_group_id) {
        if (!has_bank_groups() || bank_group_id >= NUM_BANK_GROUPS) {
            perform_all_bank_refresh(); // Fallback for DDR4
            return;
        }
        
        sc_time refresh_start_time = sc_time_stamp();
        
        // Refresh all banks in the specified bank group
        for (uint32_t bank_id = 0; bank_id < NUM_BANKS; bank_id++) {
            uint32_t global_bank_id = bank_group_id * NUM_BANKS + bank_id;
            if (global_bank_id < m_banks.size()) {
                auto& bank = m_banks[global_bank_id];
                
                if (bank.state == BankState::ACTIVE) {
                    precharge_bank(bank);
                }
                bank.state = BankState::REFRESHING;
            }
        }
        
        // Wait for same-bank refresh cycle time
        wait(m_timing.tRFCsb);
        
        // Return banks to idle state
        for (uint32_t bank_id = 0; bank_id < NUM_BANKS; bank_id++) {
            uint32_t global_bank_id = bank_group_id * NUM_BANKS + bank_id;
            if (global_bank_id < m_banks.size()) {
                m_banks[global_bank_id].state = BankState::IDLE;
            }
        }
        
        m_stats.same_bank_refreshes++;
        m_stats.total_refresh_latency += (sc_time_stamp() - refresh_start_time);
    }
    
    // Per Bank Refresh - fine-grained DDR5/LPDDR5
    void perform_per_bank_refresh(uint32_t bank_index) {
        if (bank_index >= m_banks.size()) {
            return;
        }
        
        sc_time refresh_start_time = sc_time_stamp();
        auto& bank = m_banks[bank_index];
        
        // Check if bank is busy
        if (bank.state != BankState::IDLE) {
            if (bank.state == BankState::ACTIVE) {
                precharge_bank(bank);
            } else {
                m_stats.refresh_conflicts++;
                wait(m_timing.tBurst); // Wait for ongoing operation
            }
        }
        
        // Perform per-bank refresh
        bank.state = BankState::REFRESHING;
        wait(m_timing.tRFCpb);
        bank.state = BankState::IDLE;
        
        m_stats.per_bank_refreshes++;
        m_stats.total_refresh_latency += (sc_time_stamp() - refresh_start_time);
    }
    
    // Distributed Refresh - LPDDR5 power optimization
    void perform_distributed_refresh(uint32_t bank_index) {
        // Similar to per-bank but with distributed timing
        perform_per_bank_refresh(bank_index);
        m_stats.distributed_refreshes++;
        
        // Additional power optimization delay
        if (m_timing.refresh_granularity > 8192) {
            wait(m_timing.tREFIpb / 4); // Spread refresh load
        }
    }
    
    // Refresh Management Unit (RMU) - advanced DDR5
    void perform_rmu_refresh(uint32_t refresh_counter) {
        // Intelligent refresh based on access patterns
        // For simulation, use a hybrid approach
        
        if (refresh_counter % 4 == 0) {
            // Every 4th cycle, do all-bank refresh for consistency
            perform_all_bank_refresh();
        } else {
            // Use per-bank refresh for efficiency
            uint32_t target_bank = refresh_counter % get_total_banks();
            perform_per_bank_refresh(target_bank);
        }
    }
    
    // Helper functions for refresh schemes
    sc_time get_refresh_interval() const {
        switch (m_timing.refresh_scheme) {
            case RefreshScheme::ALL_BANK_REFRESH:
                return m_timing.tREFI;
            case RefreshScheme::SAME_BANK_REFRESH:
                return m_timing.tREFI / NUM_BANK_GROUPS;
            case RefreshScheme::PER_BANK_REFRESH:
            case RefreshScheme::DISTRIBUTED_REFRESH:
                return m_timing.tREFIpb;
            case RefreshScheme::REFRESH_MANAGEMENT_UNIT:
                return m_timing.tREFI / 2; // Adaptive interval
            default:
                return m_timing.tREFI;
        }
    }
    
    uint32_t get_total_banks() const {
        return NUM_BANKS * NUM_BANK_GROUPS;
    }
    
    std::string refresh_scheme_to_string(RefreshScheme scheme) const {
        switch (scheme) {
            case RefreshScheme::ALL_BANK_REFRESH: return "ALL_BANK";
            case RefreshScheme::SAME_BANK_REFRESH: return "SAME_BANK";
            case RefreshScheme::PER_BANK_REFRESH: return "PER_BANK";
            case RefreshScheme::DISTRIBUTED_REFRESH: return "DISTRIBUTED";
            case RefreshScheme::REFRESH_MANAGEMENT_UNIT: return "RMU";
            default: return "UNKNOWN";
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

// DramTiming constructor implementation
inline DramTiming::DramTiming(MemoryType type, SpeedGrade grade) {
    *this = DramTimingFactory::create(type, grade);
}

// DramTimingFactory implementation
inline DramTiming DramTimingFactory::create(MemoryType type, SpeedGrade grade) {
    if (!isSpeedGradeValid(type, grade)) {
        std::cerr << "Warning: Invalid speed grade " << speedGradeToString(grade) 
                  << " for memory type " << memoryTypeToString(type) 
                  << ". Using default." << std::endl;
        grade = getDefaultSpeedGrade(type);
    }
    
    switch (type) {
        case MemoryType::DDR4:   return createDDR4Timing(grade);
        case MemoryType::DDR5:   return createDDR5Timing(grade);
        case MemoryType::LPDDR5: return createLPDDR5Timing(grade);
        default: return createDDR4Timing(SpeedGrade::DDR4_3200);
    }
}

inline DramTiming DramTimingFactory::createDDR4(SpeedGrade grade) {
    return createDDR4Timing(grade);
}

inline DramTiming DramTimingFactory::createDDR5(SpeedGrade grade) {
    return createDDR5Timing(grade);
}

inline DramTiming DramTimingFactory::createLPDDR5(SpeedGrade grade) {
    return createLPDDR5Timing(grade);
}

inline SpeedGrade DramTimingFactory::getDefaultSpeedGrade(MemoryType type) {
    switch (type) {
        case MemoryType::DDR4:   return SpeedGrade::DDR4_3200;
        case MemoryType::DDR5:   return SpeedGrade::DDR5_4800;
        case MemoryType::LPDDR5: return SpeedGrade::LPDDR5_6400;
        default: return SpeedGrade::DDR4_3200;
    }
}

inline bool DramTimingFactory::isSpeedGradeValid(MemoryType type, SpeedGrade grade) {
    switch (type) {
        case MemoryType::DDR4:
            return (grade >= SpeedGrade::DDR4_2400 && grade <= SpeedGrade::DDR4_4266);
        case MemoryType::DDR5:
            return (grade >= SpeedGrade::DDR5_4800 && grade <= SpeedGrade::DDR5_8400);
        case MemoryType::LPDDR5:
            return (grade >= SpeedGrade::LPDDR5_5500 && grade <= SpeedGrade::LPDDR5_8533);
        default: return false;
    }
}

inline std::string DramTimingFactory::speedGradeToString(SpeedGrade grade) {
    switch (grade) {
        case SpeedGrade::DDR4_2400:  return "DDR4-2400";
        case SpeedGrade::DDR4_2666:  return "DDR4-2666";
        case SpeedGrade::DDR4_3200:  return "DDR4-3200";
        case SpeedGrade::DDR4_4266:  return "DDR4-4266";
        case SpeedGrade::DDR5_4800:  return "DDR5-4800";
        case SpeedGrade::DDR5_5600:  return "DDR5-5600";
        case SpeedGrade::DDR5_6400:  return "DDR5-6400";
        case SpeedGrade::DDR5_8400:  return "DDR5-8400";
        case SpeedGrade::LPDDR5_5500: return "LPDDR5-5500";
        case SpeedGrade::LPDDR5_6400: return "LPDDR5-6400";
        case SpeedGrade::LPDDR5_7500: return "LPDDR5-7500";
        case SpeedGrade::LPDDR5_8533: return "LPDDR5-8533";
        default: return "Unknown";
    }
}

inline std::string DramTimingFactory::memoryTypeToString(MemoryType type) {
    switch (type) {
        case MemoryType::DDR4:   return "DDR4";
        case MemoryType::DDR5:   return "DDR5";
        case MemoryType::LPDDR5: return "LPDDR5";
        default: return "Unknown";
    }
}

inline DramTiming DramTimingFactory::createDDR4Timing(SpeedGrade grade) {
    DramTiming timing;
    
    switch (grade) {
        case SpeedGrade::DDR4_2400:  // DDR4-2400 (1200 MHz, 0.833 ns cycle)
            timing.tCL = sc_time(15, SC_NS);     // CL=18, 18 * 0.833ns = 15ns
            timing.tRCD = sc_time(15, SC_NS);    // tRCD=18
            timing.tRP = sc_time(15, SC_NS);     // tRP=18
            timing.tRAS = sc_time(35, SC_NS);    // tRAS=42
            timing.tBurst = sc_time(3.33, SC_NS); // 4 beats * 0.833ns
            break;
            
        case SpeedGrade::DDR4_2666:  // DDR4-2666 (1333 MHz, 0.75 ns cycle)
            timing.tCL = sc_time(13.5, SC_NS);   // CL=18, 18 * 0.75ns = 13.5ns
            timing.tRCD = sc_time(13.5, SC_NS);  // tRCD=18
            timing.tRP = sc_time(13.5, SC_NS);   // tRP=18
            timing.tRAS = sc_time(31.5, SC_NS);  // tRAS=42
            timing.tBurst = sc_time(3, SC_NS);   // 4 beats * 0.75ns
            break;
            
        case SpeedGrade::DDR4_3200:  // DDR4-3200 (1600 MHz, 0.625 ns cycle)
        default:
            timing.tCL = sc_time(14, SC_NS);     // CL=22, 22 * 0.625ns = 13.75ns ≈ 14ns
            timing.tRCD = sc_time(14, SC_NS);    // tRCD=22
            timing.tRP = sc_time(14, SC_NS);     // tRP=22
            timing.tRAS = sc_time(32, SC_NS);    // tRAS=52
            timing.tBurst = sc_time(2.5, SC_NS); // 4 beats * 0.625ns
            break;
            
        case SpeedGrade::DDR4_4266:  // DDR4-4266 (2133 MHz, 0.468 ns cycle)
            timing.tCL = sc_time(13, SC_NS);     // CL=28, 28 * 0.468ns = 13.1ns
            timing.tRCD = sc_time(13, SC_NS);    // tRCD=28
            timing.tRP = sc_time(13, SC_NS);     // tRP=28
            timing.tRAS = sc_time(30, SC_NS);    // tRAS=64
            timing.tBurst = sc_time(1.87, SC_NS); // 4 beats * 0.468ns
            break;
    }
    
    // Common DDR4 parameters
    timing.tWR = sc_time(15, SC_NS);
    timing.tRFC = sc_time(350, SC_NS);     // 350ns for 8Gb chips
    timing.tREFI = sc_time(7800, SC_NS);   // 7.8μs refresh interval
    
    // DDR4 Bank Group specific timings (same values since no bank groups)
    timing.tCCDL = sc_time(4, SC_NS);      // Same as tBurst for DDR4
    timing.tCCDS = sc_time(4, SC_NS);      // Same as tBurst for DDR4
    timing.tRRDL = sc_time(6, SC_NS);      // Row-to-row delay
    timing.tRRDS = sc_time(4, SC_NS);      // Row-to-row delay (same bank)
    
    // DDR4 refresh scheme parameters (traditional all-bank refresh)
    timing.refresh_scheme = RefreshScheme::ALL_BANK_REFRESH;
    timing.tRFCab = sc_time(350, SC_NS);   // All bank refresh cycle time
    timing.tRFCsb = sc_time(350, SC_NS);   // Same as all bank for DDR4
    timing.tRFCpb = sc_time(60, SC_NS);    // Per bank refresh (if supported)
    timing.tREFIpb = sc_time(488, SC_NS);  // Per bank refresh interval (tREFI/16)
    timing.refresh_granularity = 8192;    // 8K rows per refresh
    
    return timing;
}

inline DramTiming DramTimingFactory::createDDR5Timing(SpeedGrade grade) {
    DramTiming timing;
    
    switch (grade) {
        case SpeedGrade::DDR5_4800:  // DDR5-4800 (2400 MHz, 0.417 ns cycle)
        default:
            timing.tCL = sc_time(10, SC_NS);     // CL=24, 24 * 0.417ns = 10ns
            timing.tRCD = sc_time(10, SC_NS);    // tRCD=24
            timing.tRP = sc_time(10, SC_NS);     // tRP=24
            timing.tRAS = sc_time(25, SC_NS);    // tRAS=60
            timing.tBurst = sc_time(1.67, SC_NS); // 4 beats * 0.417ns
            break;
            
        case SpeedGrade::DDR5_5600:  // DDR5-5600 (2800 MHz, 0.357 ns cycle)
            timing.tCL = sc_time(9, SC_NS);      // CL=25, 25 * 0.357ns = 8.93ns
            timing.tRCD = sc_time(9, SC_NS);     // tRCD=25
            timing.tRP = sc_time(9, SC_NS);      // tRP=25
            timing.tRAS = sc_time(23, SC_NS);    // tRAS=65
            timing.tBurst = sc_time(1.43, SC_NS); // 4 beats * 0.357ns
            break;
            
        case SpeedGrade::DDR5_6400:  // DDR5-6400 (3200 MHz, 0.3125 ns cycle)
            timing.tCL = sc_time(8, SC_NS);      // CL=26, 26 * 0.3125ns = 8.125ns
            timing.tRCD = sc_time(8, SC_NS);     // tRCD=26
            timing.tRP = sc_time(8, SC_NS);      // tRP=26
            timing.tRAS = sc_time(21, SC_NS);    // tRAS=68
            timing.tBurst = sc_time(1.25, SC_NS); // 4 beats * 0.3125ns
            break;
            
        case SpeedGrade::DDR5_8400:  // DDR5-8400 (4200 MHz, 0.238 ns cycle)
            timing.tCL = sc_time(7, SC_NS);      // CL=30, 30 * 0.238ns = 7.14ns
            timing.tRCD = sc_time(7, SC_NS);     // tRCD=30
            timing.tRP = sc_time(7, SC_NS);      // tRP=30
            timing.tRAS = sc_time(18, SC_NS);    // tRAS=76
            timing.tBurst = sc_time(0.95, SC_NS); // 4 beats * 0.238ns
            break;
    }
    
    // DDR5 specific parameters
    timing.tWR = sc_time(12, SC_NS);       // Faster write recovery
    timing.tRFC = sc_time(295, SC_NS);     // 295ns for 16Gb chips
    timing.tREFI = sc_time(3900, SC_NS);   // 3.9μs refresh interval (2x faster)
    
    // DDR5 Bank Group specific timings
    timing.tCCDL = sc_time(6, SC_NS);      // CAS to CAS Long (different bank group)
    timing.tCCDS = sc_time(4, SC_NS);      // CAS to CAS Short (same bank group)
    timing.tRRDL = sc_time(6, SC_NS);      // Row to Row Long (different bank group)
    timing.tRRDS = sc_time(4, SC_NS);      // Row to Row Short (same bank group)
    
    // DDR5 refresh scheme parameters (default to same bank refresh for DDR5)
    timing.refresh_scheme = RefreshScheme::SAME_BANK_REFRESH;
    timing.tRFCab = sc_time(295, SC_NS);   // All bank refresh cycle time
    timing.tRFCsb = sc_time(100, SC_NS);   // Same bank refresh cycle time
    timing.tRFCpb = sc_time(50, SC_NS);    // Per bank refresh cycle time
    timing.tREFIpb = sc_time(244, SC_NS);  // Per bank refresh interval (tREFI/16)
    timing.refresh_granularity = 16384;   // 16K rows per refresh
    
    return timing;
}

inline DramTiming DramTimingFactory::createLPDDR5Timing(SpeedGrade grade) {
    DramTiming timing;
    
    switch (grade) {
        case SpeedGrade::LPDDR5_5500: // LPDDR5-5500 (2750 MHz, 0.364 ns cycle)
            timing.tCL = sc_time(8, SC_NS);     // CL=22, 22 * 0.364ns = 8ns
            timing.tRCD = sc_time(8, SC_NS);    // tRCD=22
            timing.tRP = sc_time(8, SC_NS);     // tRP=22
            timing.tRAS = sc_time(18, SC_NS);   // tRAS=50
            timing.tBurst = sc_time(1.45, SC_NS); // 4 beats * 0.364ns
            break;
            
        case SpeedGrade::LPDDR5_6400: // LPDDR5-6400 (3200 MHz, 0.3125 ns cycle)
        default:
            timing.tCL = sc_time(7, SC_NS);     // CL=22, 22 * 0.3125ns = 6.875ns
            timing.tRCD = sc_time(7, SC_NS);    // tRCD=22
            timing.tRP = sc_time(7, SC_NS);     // tRP=22
            timing.tRAS = sc_time(16, SC_NS);   // tRAS=52
            timing.tBurst = sc_time(1.25, SC_NS); // 4 beats * 0.3125ns
            break;
            
        case SpeedGrade::LPDDR5_7500: // LPDDR5-7500 (3750 MHz, 0.267 ns cycle)
            timing.tCL = sc_time(6, SC_NS);     // CL=22, 22 * 0.267ns = 5.87ns
            timing.tRCD = sc_time(6, SC_NS);    // tRCD=22
            timing.tRP = sc_time(6, SC_NS);     // tRP=22
            timing.tRAS = sc_time(14, SC_NS);   // tRAS=54
            timing.tBurst = sc_time(1.07, SC_NS); // 4 beats * 0.267ns
            break;
            
        case SpeedGrade::LPDDR5_8533: // LPDDR5-8533 (4266 MHz, 0.234 ns cycle)
            timing.tCL = sc_time(5, SC_NS);     // CL=22, 22 * 0.234ns = 5.15ns
            timing.tRCD = sc_time(5, SC_NS);    // tRCD=22
            timing.tRP = sc_time(5, SC_NS);     // tRP=22
            timing.tRAS = sc_time(13, SC_NS);   // tRAS=56
            timing.tBurst = sc_time(0.94, SC_NS); // 4 beats * 0.234ns
            break;
    }
    
    // LPDDR5 specific parameters (optimized for mobile)
    timing.tWR = sc_time(10, SC_NS);       // Fast write recovery
    timing.tRFC = sc_time(180, SC_NS);     // 180ns refresh (mobile optimized)
    timing.tREFI = sc_time(3900, SC_NS);   // 3.9μs refresh interval
    
    // LPDDR5 Bank Group specific timings (optimized for mobile)
    timing.tCCDL = sc_time(5, SC_NS);      // CAS to CAS Long (different bank group)
    timing.tCCDS = sc_time(3, SC_NS);      // CAS to CAS Short (same bank group)  
    timing.tRRDL = sc_time(5, SC_NS);      // Row to Row Long (different bank group)
    timing.tRRDS = sc_time(3, SC_NS);      // Row to Row Short (same bank group)
    
    // LPDDR5 refresh scheme parameters (default to distributed refresh for power optimization)
    timing.refresh_scheme = RefreshScheme::DISTRIBUTED_REFRESH;
    timing.tRFCab = sc_time(180, SC_NS);   // All bank refresh cycle time
    timing.tRFCsb = sc_time(90, SC_NS);    // Same bank refresh cycle time  
    timing.tRFCpb = sc_time(30, SC_NS);    // Per bank refresh cycle time (mobile optimized)
    timing.tREFIpb = sc_time(244, SC_NS);  // Per bank refresh interval (tREFI/16)
    timing.refresh_granularity = 16384;   // 16K rows per refresh
    
    return timing;
}

// JSON Configuration parsing helper functions
inline MemoryType parseMemoryType(const std::string& type_str) {
    if (type_str == "DDR4") return MemoryType::DDR4;
    else if (type_str == "DDR5") return MemoryType::DDR5;
    else if (type_str == "LPDDR5") return MemoryType::LPDDR5;
    else {
        std::cerr << "Warning: Unknown memory type '" << type_str << "', defaulting to DDR4" << std::endl;
        return MemoryType::DDR4;
    }
}

inline SpeedGrade parseSpeedGrade(const std::string& grade_str) {
    if (grade_str == "DDR4_2400") return SpeedGrade::DDR4_2400;
    else if (grade_str == "DDR4_2666") return SpeedGrade::DDR4_2666;
    else if (grade_str == "DDR4_3200") return SpeedGrade::DDR4_3200;
    else if (grade_str == "DDR4_4266") return SpeedGrade::DDR4_4266;
    else if (grade_str == "DDR5_4800") return SpeedGrade::DDR5_4800;
    else if (grade_str == "DDR5_5600") return SpeedGrade::DDR5_5600;
    else if (grade_str == "DDR5_6400") return SpeedGrade::DDR5_6400;
    else if (grade_str == "DDR5_8400") return SpeedGrade::DDR5_8400;
    else if (grade_str == "LPDDR5_5500") return SpeedGrade::LPDDR5_5500;
    else if (grade_str == "LPDDR5_6400") return SpeedGrade::LPDDR5_6400;
    else if (grade_str == "LPDDR5_7500") return SpeedGrade::LPDDR5_7500;
    else if (grade_str == "LPDDR5_8533") return SpeedGrade::LPDDR5_8533;
    else {
        std::cerr << "Warning: Unknown speed grade '" << grade_str << "', defaulting to DDR4_3200" << std::endl;
        return SpeedGrade::DDR4_3200;
    }
}

inline RefreshScheme parseRefreshScheme(const std::string& scheme_str) {
    if (scheme_str == "ALL_BANK_REFRESH") return RefreshScheme::ALL_BANK_REFRESH;
    else if (scheme_str == "SAME_BANK_REFRESH") return RefreshScheme::SAME_BANK_REFRESH;
    else if (scheme_str == "PER_BANK_REFRESH") return RefreshScheme::PER_BANK_REFRESH;
    else if (scheme_str == "DISTRIBUTED_REFRESH") return RefreshScheme::DISTRIBUTED_REFRESH;
    else if (scheme_str == "REFRESH_MANAGEMENT_UNIT") return RefreshScheme::REFRESH_MANAGEMENT_UNIT;
    else {
        std::cerr << "Warning: Unknown refresh scheme '" << scheme_str << "', defaulting to ALL_BANK_REFRESH" << std::endl;
        return RefreshScheme::ALL_BANK_REFRESH;
    }
}

inline DramTiming create_timing_from_config(const JsonConfig& config) {
    try {
        // Parse basic memory configuration
        auto memory_type = parseMemoryType(config.get_string("dram.memory_type", "DDR4"));
        auto speed_grade = parseSpeedGrade(config.get_string("dram.speed_grade", "DDR4_3200"));
        
        // Create base timing from predefined configuration or selected config
        DramTiming timing;
        std::string selected_config = config.get_string("dram.selected_config", "");
        
        if (!selected_config.empty()) {
            // Load from predefined memory configuration
            std::string config_prefix = "dram.memory_configs." + selected_config + ".ac_parameters.";
            
            timing.tCL = sc_time(config.get_double(config_prefix + "tCL_ns", 14.0), SC_NS);
            timing.tRCD = sc_time(config.get_double(config_prefix + "tRCD_ns", 14.0), SC_NS);
            timing.tRP = sc_time(config.get_double(config_prefix + "tRP_ns", 14.0), SC_NS);
            timing.tRAS = sc_time(config.get_double(config_prefix + "tRAS_ns", 32.0), SC_NS);
            timing.tWR = sc_time(config.get_double(config_prefix + "tWR_ns", 15.0), SC_NS);
            timing.tRFC = sc_time(config.get_double(config_prefix + "tRFC_ns", 350.0), SC_NS);
            timing.tREFI = sc_time(config.get_double(config_prefix + "tREFI_ns", 7800.0), SC_NS);
            timing.tBurst = sc_time(config.get_double(config_prefix + "tBurst_ns", 2.5), SC_NS);
            timing.tCCDL = sc_time(config.get_double(config_prefix + "tCCDL_ns", 4.0), SC_NS);
            timing.tCCDS = sc_time(config.get_double(config_prefix + "tCCDS_ns", 4.0), SC_NS);
            timing.tRRDL = sc_time(config.get_double(config_prefix + "tRRDL_ns", 6.0), SC_NS);
            timing.tRRDS = sc_time(config.get_double(config_prefix + "tRRDS_ns", 4.0), SC_NS);
            
            // Load refresh parameters if available
            timing.tRFCab = sc_time(config.get_double(config_prefix + "tRFCab_ns", 350.0), SC_NS);
            timing.tRFCsb = sc_time(config.get_double(config_prefix + "tRFCsb_ns", 120.0), SC_NS);
            timing.tRFCpb = sc_time(config.get_double(config_prefix + "tRFCpb_ns", 60.0), SC_NS);
            timing.tREFIpb = sc_time(config.get_double(config_prefix + "tREFIpb_ns", 488.0), SC_NS);
            timing.refresh_granularity = config.get_int(config_prefix + "refresh_granularity", 8192);
            
        } else {
            // Use factory defaults for the memory type and speed grade
            timing = DramTimingFactory::create(memory_type, speed_grade);
        }
        
        // Override with custom timings if enabled
        if (config.get_bool("dram.custom_timings.enable_custom", false)) {
            timing.tCL = sc_time(config.get_double("dram.custom_timings.tCL_ns", timing.tCL.to_seconds() * 1e9), SC_NS);
            timing.tRCD = sc_time(config.get_double("dram.custom_timings.tRCD_ns", timing.tRCD.to_seconds() * 1e9), SC_NS);
            timing.tRP = sc_time(config.get_double("dram.custom_timings.tRP_ns", timing.tRP.to_seconds() * 1e9), SC_NS);
            timing.tRAS = sc_time(config.get_double("dram.custom_timings.tRAS_ns", timing.tRAS.to_seconds() * 1e9), SC_NS);
            timing.tWR = sc_time(config.get_double("dram.custom_timings.tWR_ns", timing.tWR.to_seconds() * 1e9), SC_NS);
            timing.tRFC = sc_time(config.get_double("dram.custom_timings.tRFC_ns", timing.tRFC.to_seconds() * 1e9), SC_NS);
            timing.tREFI = sc_time(config.get_double("dram.custom_timings.tREFI_ns", timing.tREFI.to_seconds() * 1e9), SC_NS);
            timing.tBurst = sc_time(config.get_double("dram.custom_timings.tBurst_ns", timing.tBurst.to_seconds() * 1e9), SC_NS);
        }
        
        // Set refresh scheme (can be overridden from JSON)
        auto refresh_scheme_str = config.get_string("dram.refresh_scheme", "ALL_BANK_REFRESH");
        timing.refresh_scheme = parseRefreshScheme(refresh_scheme_str);
        
        return timing;
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing DRAM configuration: " << e.what() << ", using DDR4 defaults" << std::endl;
        return DramTimingFactory::create(MemoryType::DDR4, SpeedGrade::DDR4_3200);
    }
}

#endif