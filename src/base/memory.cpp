#include "base/memory.h"
#include "common/common_utils.h"
#include "packet/generic_packet.h" // For GenericPacket
#include "common/error_handling.h" // For error handling

Memory::Memory(sc_module_name name) : sc_module(name) {    
    // Initialize memory to a known state (e.g., zero)
    for (size_t i = 0; i < MEMORY_SIZE; ++i) {
        mem[i] = {0, 0};
    }
    SC_THREAD(run);
}

void Memory::run() {
    while (true) {
        
        auto p_ptr = in.read();
        // No dynamic cast needed - use virtual methods from BasePacket

        // Bounds checking for memory address
        int address = p_ptr->get_address();
        if (address < 0 || static_cast<size_t>(address) >= MEMORY_SIZE) {
            SOC_SIM_ERROR("Memory", soc_sim::error::codes::ADDRESS_OUT_OF_BOUNDS,
                         "Address out of bounds: " + std::to_string(address) + " (valid range: 0-" + std::to_string(MEMORY_SIZE-1) + ")");
            continue;
        }

        if (p_ptr->get_command() == Command::WRITE) {
            mem[address].data = p_ptr->get_data();
            mem[address].databyte = p_ptr->get_databyte();
            print_packet_log(std::cout, "Memory", *p_ptr, "Received WRITE");
        } else if (p_ptr->get_command() == Command::READ) {
            p_ptr->set_data(mem[address].data);
            p_ptr->set_databyte(mem[address].databyte);
            print_packet_log(std::cout, "Memory", *p_ptr, "Received READ");
            // In a real system, we would send the read data back.
            // For now, we just print it.
        }
    }
}
