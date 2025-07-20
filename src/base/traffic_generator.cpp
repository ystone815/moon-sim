#include "base/traffic_generator.h"
#include "common/common_utils.h"

TrafficGenerator::TrafficGenerator(sc_module_name name, sc_time interval, LocalityType locality, bool do_reads, bool do_writes, unsigned char databyte_value, unsigned int num_transactions, bool debug_enable)
    : sc_module(name), m_interval(interval), m_locality_type(locality), m_do_reads(do_reads), m_do_writes(do_writes), m_databyte_value(databyte_value), m_num_transactions(num_transactions), m_debug_enable(debug_enable), m_current_address(0),
      m_random_generator(std::random_device{}()), // Seed with random device
      m_address_dist(0, 0xFF), // Address range 0-255
      m_data_dist(0, 0xFFF), // Data range 0-4095
      m_command_dist(0.5) // 50% probability for each command
{
    SC_THREAD(run);
}

void TrafficGenerator::run() {
    // Generate a fixed number of transactions for demonstration
    for (int i = 0; i < m_num_transactions; ++i) {
        auto p = std::make_shared<GenericPacket>();
        p->index = 0; // Will be assigned by IndexAllocator

        // Determine address based on locality
        if (m_locality_type == LocalityType::RANDOM) {
            p->address = rand() % 0x100; // Random address up to 0xFF
        } else { // SEQUENTIAL
            p->address = m_current_address;
            m_current_address += 0x10; // Increment address for next sequential access
            if (m_current_address > 0xFF) { // Wrap around if address exceeds limit
                m_current_address = 0;
            }
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
