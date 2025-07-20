#include "base/delay_line.h"
#include "common/common_utils.h"

DelayLine::DelayLine(sc_module_name name, sc_time delay) : sc_module(name), m_delay(delay) {
    SC_THREAD(process_packets);
}

void DelayLine::process_packets() {
    while (true) {
        auto p_ptr = in.read();
        wait(m_delay);
        print_packet_log(std::cout, "DelayLine", *p_ptr, "Packet processed after " + m_delay.to_string());
        out.write(p_ptr);
    }
}