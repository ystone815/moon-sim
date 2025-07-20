#ifndef MEMORY_H
#define MEMORY_H

#include <systemc.h>
#include "packet/base_packet.h" // Include BasePacket
#include "packet/generic_packet.h" // Include GenericPacket
#include <memory> // For smart pointers
#include <array> // For std::array

// New struct to hold data and databyte
struct MemoryEntry {
    int data;
    unsigned char databyte;
};

SC_MODULE(Memory) {
    SC_HAS_PROCESS(Memory);
    sc_fifo_in<std::shared_ptr<BasePacket>> in;

    void run();

    // Constructor declaration
    Memory(sc_module_name name);

private:
    static constexpr size_t MEMORY_SIZE = 256;
    std::array<MemoryEntry, MEMORY_SIZE> mem; // 256 MemoryEntry array
};

#endif
