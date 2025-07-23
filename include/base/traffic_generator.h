#ifndef TRAFFIC_GENERATOR_H
#define TRAFFIC_GENERATOR_H

#include <systemc.h>
#include "packet/base_packet.h" // Include BasePacket
#include "packet/generic_packet.h" // Include GenericPacket
#include <memory> // For smart pointers
#include <random> // For C++11 random library
#include <string>
#include <vector>

// Forward declaration
class JsonConfig;

// Locality is now controlled by percentage (0-100)
// 0 = full random, 100 = full sequential

// Traffic pattern types
enum class TrafficPattern {
    CONSTANT,      // Fixed interval (existing behavior)
    BURST,         // Burst of packets followed by idle
    POISSON,       // Poisson arrival process
    EXPONENTIAL,   // Exponential inter-arrival times
    NORMAL         // Normally distributed intervals
};

// Workload templates
enum class WorkloadTemplate {
    CUSTOM,        // User-defined parameters
    DATABASE,      // Read-heavy with high locality
    WEB_SERVER,    // Bursty mixed read/write
    ML_INFERENCE,  // Predictable batch processing
    IOT_SENSORS,   // Low-rate periodic updates
    STREAMING      // High-throughput sequential
};

SC_MODULE(TrafficGenerator) {
    SC_HAS_PROCESS(TrafficGenerator);
    sc_fifo_out<std::shared_ptr<BasePacket>> out;

    // Basic configuration
    const sc_time m_interval;
    const unsigned int m_locality_percentage; // 0-100: 0=random, 100=sequential
    const unsigned int m_write_percentage; // 0-100: 0=all reads, 100=all writes
    const unsigned char m_databyte_value;
    const unsigned int m_num_transactions;
    const bool m_debug_enable;
    const unsigned int m_start_address;
    const unsigned int m_end_address;
    const unsigned int m_address_increment;
    
    // Advanced traffic pattern configuration
    const TrafficPattern m_traffic_pattern;
    const WorkloadTemplate m_workload_template;
    
    // Burst pattern parameters
    const unsigned int m_burst_size;        // Packets per burst
    const sc_time m_burst_interval;         // Interval between packets in burst
    const sc_time m_idle_time;              // Idle time between bursts
    
    // Distribution parameters for variable delays
    const double m_delay_mean;              // Mean for exponential/normal
    const double m_delay_stddev;            // Standard deviation for normal
    const double m_poisson_rate;            // Rate parameter for Poisson

    void run();

    // Updated constructor with new parameter
    TrafficGenerator(sc_module_name name, sc_time interval, unsigned int locality_percentage, unsigned int write_percentage, unsigned char databyte_value, unsigned int num_transactions, bool debug_enable = false, unsigned int start_address = 0, unsigned int end_address = 0xFF, unsigned int address_increment = 0x10);
    
    // JSON-based constructor (from existing config object)
    TrafficGenerator(sc_module_name name, const class JsonConfig& config);
    
    // JSON file-based constructor (loads own config file)
    TrafficGenerator(sc_module_name name, const std::string& config_file_path = "config/base/traffic_generator_config.json");

private:
    unsigned int m_current_address; // For sequential access
    std::mt19937 m_random_generator; // Random number generator
    std::uniform_int_distribution<int> m_address_dist; // Address distribution
    std::uniform_int_distribution<int> m_data_dist; // Data distribution
    std::uniform_int_distribution<int> m_locality_dist; // For locality percentage (0-99)
    std::uniform_int_distribution<int> m_write_dist; // For write percentage (0-99)
    
    // Additional distributions for advanced patterns
    std::exponential_distribution<double> m_exponential_dist;
    std::normal_distribution<double> m_normal_dist;
    std::poisson_distribution<int> m_poisson_dist;
    std::uniform_real_distribution<double> m_uniform_real_dist;
    
    // Helper methods
    void apply_workload_template();
    sc_time generate_next_interval();
    std::shared_ptr<GenericPacket> generate_packet();
    void run_constant_pattern();
    void run_burst_pattern();
    void run_stochastic_pattern();
};

#endif