#ifndef TRAFFIC_GENERATOR_H
#define TRAFFIC_GENERATOR_H

#include <systemc.h>
#include "packet/base_packet.h" // Include BasePacket
#include "packet/generic_packet.h" // Include GenericPacket
#include <memory> // For smart pointers
#include <random> // For C++11 random library

enum class LocalityType {
    RANDOM,
    SEQUENTIAL
};

SC_MODULE(TrafficGenerator) {
    SC_HAS_PROCESS(TrafficGenerator);
    sc_fifo_out<std::shared_ptr<BasePacket>> out;

    // New member variables for options
    const sc_time m_interval;
    const LocalityType m_locality_type;
    const bool m_do_reads;
    const bool m_do_writes;
    const unsigned char m_databyte_value;
    const unsigned int m_num_transactions;
    const bool m_debug_enable;

    void run();

    // Updated constructor with new parameter
    TrafficGenerator(sc_module_name name, sc_time interval, LocalityType locality, bool do_reads, bool do_writes, unsigned char databyte_value, unsigned int num_transactions, bool debug_enable = false);

private:
    unsigned int m_current_address; // For sequential access
    std::mt19937 m_random_generator; // Random number generator
    std::uniform_int_distribution<int> m_address_dist; // Address distribution
    std::uniform_int_distribution<int> m_data_dist; // Data distribution
    std::bernoulli_distribution m_command_dist; // Command distribution
};

#endif