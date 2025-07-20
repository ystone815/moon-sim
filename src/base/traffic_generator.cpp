#include "base/traffic_generator.h"
#include "common/common_utils.h"
#include "common/json_config.h"

TrafficGenerator::TrafficGenerator(sc_module_name name, sc_time interval, unsigned int locality_percentage, bool do_reads, bool do_writes, unsigned char databyte_value, unsigned int num_transactions, bool debug_enable, unsigned int start_address, unsigned int end_address, unsigned int address_increment)
    : sc_module(name), m_interval(interval), m_locality_percentage(locality_percentage), m_do_reads(do_reads), m_do_writes(do_writes), m_databyte_value(databyte_value), m_num_transactions(num_transactions), m_debug_enable(debug_enable), m_start_address(start_address), m_end_address(end_address), m_address_increment(address_increment), m_current_address(start_address),
      m_random_generator(std::random_device{}()), // Seed with random device
      m_address_dist(start_address, end_address), // Configurable address range
      m_data_dist(0, 0xFFF), // Data range 0-4095
      m_command_dist(0.5), // 50% probability for each command
      m_locality_dist(0, 99) // For locality percentage check (0-99)
{
    SC_THREAD(run);
}

// JSON-based constructor
TrafficGenerator::TrafficGenerator(sc_module_name name, const JsonConfig& config)
    : sc_module(name),
      m_interval(sc_time(config.get_int("interval_ns", 10), SC_NS)),
      m_locality_percentage(config.get_int("locality_percentage", 0)),
      m_do_reads(config.get_bool("do_reads", true)),
      m_do_writes(config.get_bool("do_writes", true)),
      m_databyte_value(static_cast<unsigned char>(config.get_int("databyte_value", 64))),
      m_num_transactions(config.get_int("num_transactions", 100000)),
      m_debug_enable(config.get_bool("debug_enable", false)),
      m_start_address(config.get_int("start_address", 0)),
      m_end_address(config.get_int("end_address", 0xFF)),
      m_address_increment(config.get_int("address_increment", 0x10)),
      m_current_address(m_start_address),
      m_random_generator(std::random_device{}()), // Seed with random device
      m_address_dist(m_start_address, m_end_address), // Configurable address range
      m_data_dist(0, 0xFFF), // Data range 0-4095
      m_command_dist(0.5), // 50% probability for each command
      m_locality_dist(0, 99) // For locality percentage check (0-99)
{
    SC_THREAD(run);
}

// JSON file-based constructor (loads own config file)
TrafficGenerator::TrafficGenerator(sc_module_name name, const std::string& config_file_path)
    : TrafficGenerator(name, JsonConfig(config_file_path))
{
    // Delegating constructor - loads config file and calls JSON config constructor
}

void TrafficGenerator::run() {
    // Generate a fixed number of transactions for demonstration
    for (int i = 0; i < m_num_transactions; ++i) {
        auto p = std::make_shared<GenericPacket>();
        p->index = 0; // Will be assigned by IndexAllocator

        // Determine address based on locality percentage
        // Generate random number 0-99, if < locality_percentage, use sequential
        bool use_sequential = (m_locality_dist(m_random_generator) < static_cast<int>(m_locality_percentage));
        
        if (use_sequential) {
            // Sequential access
            p->address = m_current_address;
            m_current_address += m_address_increment; // Configurable increment
            if (m_current_address > m_end_address) { // Wrap around if address exceeds limit
                m_current_address = m_start_address;
            }
        } else {
            // Random access
            p->address = m_address_dist(m_random_generator); // Random address in configured range
        }

        // Determine packet type (READ/WRITE) based on options
        if (m_do_writes && (!m_do_reads || m_command_dist(m_random_generator))) { // 50% probability if both enabled
            p->command = Command::WRITE;
            p->data = m_data_dist(m_random_generator); // Random data 0-4095
            p->databyte = m_databyte_value; // Set databyte from option
            out.write(p);
            if (m_debug_enable) {
                print_packet_log(std::cout, "TrafficGenerator", *p, "Sent WRITE");
            }
        } else if (m_do_reads) {
            p->command = Command::READ;
            p->databyte = 0; // databyte is not relevant for READ, set to 0 or default
            out.write(p);
            if (m_debug_enable) {
                print_packet_log(std::cout, "TrafficGenerator", *p, "Sent READ");
            }
        } else {
            // No reads or writes enabled, or no action for this iteration
            if (m_debug_enable) {
                print_packet_log(std::cout, "TrafficGenerator", *p, "No operation performed");
            }
        }

        wait(m_interval); // Wait for the specified interval
    }

    sc_stop(); // Stop simulation after generating transactions
}
