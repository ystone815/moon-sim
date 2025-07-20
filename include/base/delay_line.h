#ifndef DELAY_LINE_H
#define DELAY_LINE_H

#include <systemc.h>
#include "packet/base_packet.h" // Include BasePacket
#include <memory> // For std::unique_ptr

SC_MODULE(DelayLine) {
    SC_HAS_PROCESS(DelayLine);
    sc_fifo_in<std::shared_ptr<BasePacket>> in;
    sc_fifo_out<std::shared_ptr<BasePacket>> out;

    const sc_time m_delay;

    void process_packets();

    DelayLine(sc_module_name name, sc_time delay);
};

#endif
