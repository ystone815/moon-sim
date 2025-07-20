#include "base/delay_line_databyte.h"
#include "common/common_utils.h"

DelayLineDatabyte::DelayLineDatabyte(sc_module_name name, unsigned int width_byte, sc_time clk_period, const std::string& attribute_name)
    : sc_module(name), m_width_byte(width_byte), m_clk_period(clk_period), m_attribute_name(attribute_name) {
    SC_THREAD(process_packets);
}

void DelayLineDatabyte::process_packets() {
    while (true) {
        auto p_ptr = in.read();

        double attribute_value = p_ptr->get_attribute(m_attribute_name);

        // Calculate delay based on selected attribute, width_byte, and clock frequency
        sc_time calculated_delay = (attribute_value / m_width_byte) * m_clk_period;

        wait(calculated_delay);
        print_packet_log(std::cout, "DelayLineDatabyte", *p_ptr, "Packet processed after " + calculated_delay.to_string());
        out.write(p_ptr);
    }
}
