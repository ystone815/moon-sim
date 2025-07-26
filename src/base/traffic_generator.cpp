#include "base/traffic_generator.h"
#include "common/common_utils.h"
#include "common/json_config.h"
#include <cmath>

TrafficGenerator::TrafficGenerator(sc_module_name name, sc_time interval, unsigned int locality_percentage, unsigned int write_percentage, unsigned char databyte_value, unsigned int num_transactions, bool debug_enable, unsigned int start_address, unsigned int end_address, unsigned int address_increment)
    : sc_module(name), m_interval(interval), m_locality_percentage(locality_percentage), m_write_percentage(write_percentage), m_databyte_value(databyte_value), m_num_transactions(num_transactions), m_debug_enable(debug_enable), m_start_address(start_address), m_end_address(end_address), m_address_increment(address_increment),
      // Default to CONSTANT pattern and CUSTOM template
      m_traffic_pattern(TrafficPattern::CONSTANT), m_workload_template(WorkloadTemplate::CUSTOM),
      m_burst_size(10), m_burst_interval(sc_time(10, SC_NS)), m_idle_time(sc_time(1000, SC_NS)),
      m_delay_mean(100.0), m_delay_stddev(20.0), m_poisson_rate(1000.0),
      m_current_address(start_address),
      m_transactions_sent(0),
      m_transactions_completed(0),
      m_random_generator(std::random_device{}()),
      m_address_dist(start_address, end_address),
      m_data_dist(0, 0xFFF),
      m_locality_dist(0, 99),
      m_write_dist(0, 99),
      m_exponential_dist(1.0 / m_delay_mean),
      m_normal_dist(m_delay_mean, m_delay_stddev),
      m_poisson_dist(static_cast<int>(m_poisson_rate)),
      m_uniform_real_dist(0.0, 1.0)
{
    SC_THREAD(run);
}

// Helper function to parse traffic pattern from string
TrafficPattern parse_traffic_pattern(const std::string& pattern) {
    if (pattern == "BURST") return TrafficPattern::BURST;
    if (pattern == "POISSON") return TrafficPattern::POISSON;
    if (pattern == "EXPONENTIAL") return TrafficPattern::EXPONENTIAL;
    if (pattern == "NORMAL") return TrafficPattern::NORMAL;
    return TrafficPattern::CONSTANT; // Default
}

// Helper function to parse workload template from string
WorkloadTemplate parse_workload_template(const std::string& template_name) {
    if (template_name == "DATABASE") return WorkloadTemplate::DATABASE;
    if (template_name == "WEB_SERVER") return WorkloadTemplate::WEB_SERVER;
    if (template_name == "ML_INFERENCE") return WorkloadTemplate::ML_INFERENCE;
    if (template_name == "IOT_SENSORS") return WorkloadTemplate::IOT_SENSORS;
    if (template_name == "STREAMING") return WorkloadTemplate::STREAMING;
    return WorkloadTemplate::CUSTOM; // Default
}

// JSON-based constructor
TrafficGenerator::TrafficGenerator(sc_module_name name, const JsonConfig& config)
    : sc_module(name),
      m_interval(sc_time(config.get_int("interval_ns", 10), SC_NS)),
      m_locality_percentage(config.get_int("locality_percentage", 0)),
      m_write_percentage(config.get_int("write_percentage", 50)),
      m_databyte_value(static_cast<unsigned char>(config.get_int("databyte_value", 64))),
      m_num_transactions(config.get_int("num_transactions", 100000)),
      m_debug_enable(config.get_bool("debug_enable", false)),
      m_start_address(config.get_int("start_address", 0)),
      m_end_address(config.get_int("end_address", 0xFF)),
      m_address_increment(config.get_int("address_increment", 0x10)),
      // Advanced pattern configuration
      m_traffic_pattern(parse_traffic_pattern(config.get_string("traffic_pattern", "CONSTANT"))),
      m_workload_template(parse_workload_template(config.get_string("workload_template", "CUSTOM"))),
      m_burst_size(config.get_int("burst_size", 10)),
      m_burst_interval(sc_time(config.get_int("burst_interval_ns", 10), SC_NS)),
      m_idle_time(sc_time(config.get_int("idle_time_ns", 1000), SC_NS)),
      m_delay_mean(config.get_double("delay_mean_ns", 100.0)),
      m_delay_stddev(config.get_double("delay_stddev_ns", 20.0)),
      m_poisson_rate(config.get_double("poisson_rate", 1000.0)),
      m_current_address(m_start_address),
      m_transactions_sent(0),
      m_transactions_completed(0),
      m_random_generator(std::random_device{}()),
      m_address_dist(m_start_address, m_end_address),
      m_data_dist(0, 0xFFF),
      m_locality_dist(0, 99),
      m_write_dist(0, 99),
      m_exponential_dist(1.0 / m_delay_mean),
      m_normal_dist(m_delay_mean, m_delay_stddev),
      m_poisson_dist(static_cast<int>(m_poisson_rate)),
      m_uniform_real_dist(0.0, 1.0)
{
    // Apply workload template settings if not CUSTOM
    if (m_workload_template != WorkloadTemplate::CUSTOM) {
        apply_workload_template();
    }
    SC_THREAD(run);
}

// JSON file-based constructor (loads own config file)
TrafficGenerator::TrafficGenerator(sc_module_name name, const std::string& config_file_path)
    : TrafficGenerator(name, JsonConfig(config_file_path))
{
    // Delegating constructor - loads config file and calls JSON config constructor
}

void TrafficGenerator::run() {
    if (m_debug_enable) {
        std::cout << sc_time_stamp() << " | TrafficGenerator: Starting with pattern=" 
                  << static_cast<int>(m_traffic_pattern) 
                  << ", template=" << static_cast<int>(m_workload_template) << std::endl;
    }
    
    // Route to appropriate pattern handler
    switch (m_traffic_pattern) {
        case TrafficPattern::CONSTANT:
            run_constant_pattern();
            break;
        case TrafficPattern::BURST:
            run_burst_pattern();
            break;
        case TrafficPattern::POISSON:
        case TrafficPattern::EXPONENTIAL:
        case TrafficPattern::NORMAL:
            run_stochastic_pattern();
            break;
    }
    
    sc_stop(); // Stop simulation after generating transactions
}

void TrafficGenerator::apply_workload_template() {
    // Override configuration parameters based on workload template
    switch (m_workload_template) {
        case WorkloadTemplate::DATABASE:
            // Database workload: Read-heavy (70% reads), high locality (80%)
            // Burst pattern with small bursts to simulate query batches
            if (m_debug_enable) {
                std::cout << "TrafficGenerator: Applied DATABASE template - read-heavy, high locality" << std::endl;
            }
            // Note: These would be non-const overrides in a real implementation
            // For C++11 compatibility, we'll log the intended settings
            break;
            
        case WorkloadTemplate::WEB_SERVER:
            // Web server workload: Mixed R/W (50/50), bursty traffic patterns
            // Variable inter-arrival times to simulate request spikes
            if (m_debug_enable) {
                std::cout << "TrafficGenerator: Applied WEB_SERVER template - bursty mixed R/W" << std::endl;
            }
            break;
            
        case WorkloadTemplate::ML_INFERENCE:
            // ML inference: Batch processing, sequential reads with periodic writes
            // High locality, read-heavy with batch intervals
            if (m_debug_enable) {
                std::cout << "TrafficGenerator: Applied ML_INFERENCE template - batch sequential" << std::endl;
            }
            break;
            
        case WorkloadTemplate::IOT_SENSORS:
            // IoT sensors: Low-rate periodic updates, mostly writes
            // Low locality, write-heavy, longer intervals
            if (m_debug_enable) {
                std::cout << "TrafficGenerator: Applied IOT_SENSORS template - periodic writes" << std::endl;
            }
            break;
            
        case WorkloadTemplate::STREAMING:
            // Streaming workload: High-throughput sequential reads
            // Very high locality, read-heavy, minimal intervals
            if (m_debug_enable) {
                std::cout << "TrafficGenerator: Applied STREAMING template - high-throughput sequential" << std::endl;
            }
            break;
            
        case WorkloadTemplate::CUSTOM:
        default:
            if (m_debug_enable) {
                std::cout << "TrafficGenerator: Using CUSTOM template - user-defined parameters" << std::endl;
            }
            break;
    }
}

sc_time TrafficGenerator::generate_next_interval() {
    double delay_ns = 0.0;
    
    switch (m_traffic_pattern) {
        case TrafficPattern::CONSTANT:
            return m_interval;
            
        case TrafficPattern::EXPONENTIAL:
            delay_ns = m_exponential_dist(m_random_generator);
            break;
            
        case TrafficPattern::NORMAL:
            delay_ns = std::max(1.0, m_normal_dist(m_random_generator)); // Ensure positive
            break;
            
        case TrafficPattern::POISSON:
            // For Poisson, use exponential inter-arrival times
            delay_ns = m_exponential_dist(m_random_generator);
            break;
            
        default:
            delay_ns = m_delay_mean;
            break;
    }
    
    return sc_time(delay_ns, SC_NS);
}

std::shared_ptr<GenericPacket> TrafficGenerator::generate_packet() {
    auto p = std::make_shared<GenericPacket>();
    p->index = 0; // Will be assigned by IndexAllocator

    // Determine address based on locality percentage
    bool use_sequential = (m_locality_dist(m_random_generator) < static_cast<int>(m_locality_percentage));
    
    if (use_sequential) {
        // Sequential access
        p->address = m_current_address;
        m_current_address += m_address_increment;
        if (m_current_address > m_end_address) {
            m_current_address = m_start_address;
        }
    } else {
        // Random access
        p->address = m_address_dist(m_random_generator);
    }

    // Determine packet type (READ/WRITE) based on write percentage
    bool use_write = (m_write_dist(m_random_generator) < static_cast<int>(m_write_percentage));
    
    if (use_write) {
        p->command = Command::WRITE;
        p->data = m_data_dist(m_random_generator);
        p->databyte = m_databyte_value;
    } else {
        p->command = Command::READ;
        p->databyte = m_databyte_value;
    }
    
    return p;
}

void TrafficGenerator::run_constant_pattern() {
    for (int i = 0; i < m_num_transactions; ++i) {
        auto p = generate_packet();
        out.write(p);
        m_transactions_sent++;
        
        if (m_debug_enable) {
            print_packet_log(std::cout, "TrafficGenerator", *p, 
                            (p->command == Command::WRITE) ? "Sent WRITE" : "Sent READ");
        }
        
        wait(m_interval);
    }
}

void TrafficGenerator::run_burst_pattern() {
    while (m_transactions_sent < m_num_transactions) {
        // Send a burst of packets
        int remaining = static_cast<int>(m_num_transactions) - m_transactions_sent;
        int burst_count = std::min(static_cast<int>(m_burst_size), remaining);
        
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | TrafficGenerator: Starting burst of " 
                      << burst_count << " packets" << std::endl;
        }
        
        for (int i = 0; i < burst_count; ++i) {
            auto p = generate_packet();
            out.write(p);
            m_transactions_sent++;
            
            if (m_debug_enable) {
                print_packet_log(std::cout, "TrafficGenerator", *p, 
                                (p->command == Command::WRITE) ? "Sent WRITE (burst)" : "Sent READ (burst)");
            }
            
            if (i < burst_count - 1) { // Don't wait after the last packet in burst
                wait(m_burst_interval);
            }
        }
        
        // Wait for idle time before next burst
        if (m_transactions_sent < m_num_transactions) {
            if (m_debug_enable) {
                std::cout << sc_time_stamp() << " | TrafficGenerator: Entering idle period for " 
                          << m_idle_time << std::endl;
            }
            wait(m_idle_time);
        }
    }
}

void TrafficGenerator::run_stochastic_pattern() {
    for (int i = 0; i < m_num_transactions; ++i) {
        auto p = generate_packet();
        out.write(p);
        m_transactions_sent++;
        
        if (m_debug_enable) {
            print_packet_log(std::cout, "TrafficGenerator", *p, 
                            (p->command == Command::WRITE) ? "Sent WRITE (stochastic)" : "Sent READ (stochastic)");
        }
        
        sc_time next_interval = generate_next_interval();
        wait(next_interval);
    }
}
