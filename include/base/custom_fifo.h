#ifndef CUSTOM_FIFO_H
#define CUSTOM_FIFO_H

#include <systemc.h>
#include <memory>
#include <type_traits>
#include "packet/base_packet.h"
#include "common/json_config.h"
#include "common/vcd_helper.h"

// CustomFifo template class with sc_fifo interface compatibility
template<typename T>
class CustomFifo : public sc_module, public sc_fifo_in_if<T>, public sc_fifo_out_if<T> {
    SC_HAS_PROCESS(CustomFifo);
    
private:
    sc_fifo<T> internal_fifo;
    bool dump_in;
    bool dump_out;
    std::string fifo_name;
    
    // VCD trace signal containers using helper structure
    std::unique_ptr<PacketTraceSignals> in_trace_signals;
    std::unique_ptr<PacketTraceSignals> out_trace_signals;
    
    void trace_packet_if_enabled(const T& packet, const std::string& operation) {
        // Update pre-registered signals instead of creating new traces
        if (operation == "write" || operation == "nb_write") {
            if (dump_in) {
                update_packet_signals(packet, true);  // true = input signals
            }
        } else if (operation == "read" || operation == "nb_read") {
            if (dump_out) {
                update_packet_signals(packet, false); // false = output signals
            }
        }
    }
    
    // Helper function to update packet field signals using VCD helper functions
    void update_packet_signals(const T& packet, bool is_input) {
        if (is_input && in_trace_signals) {
            update_packet_vcd_traces(packet, *in_trace_signals);
        } else if (!is_input && out_trace_signals) {
            update_packet_vcd_traces(packet, *out_trace_signals);
        }
    }
    
    
public:
    // Constructor with JSON config
    CustomFifo(sc_module_name name, int size, const JsonConfig& config) 
        : sc_module(name), internal_fifo("internal_fifo", size), fifo_name(std::string(name)) {
        
        // Get dump configuration (use direct keys due to JsonConfig limitations)
        bool dump_interface = config.get_bool("dump.interface", false);
        dump_in = dump_interface;   // Use interface setting for both in/out
        dump_out = dump_interface;
        
        // Initialize VCD file if any dump option is enabled
        if (dump_in || dump_out) {
            std::string vcd_file = config.get_string("dump.vcd_file", "traces.vcd");
            VcdTraceManager::initialize(vcd_file);
            
            // Create signal containers and register traces using helper functions with unified hierarchy
            if (dump_in) {
                in_trace_signals = std::unique_ptr<PacketTraceSignals>(new PacketTraceSignals(fifo_name + "_in"));
                register_packet_vcd_traces<T>(fifo_name, "in_", true, *in_trace_signals);
            }
            
            if (dump_out) {
                out_trace_signals = std::unique_ptr<PacketTraceSignals>(new PacketTraceSignals(fifo_name + "_out"));
                register_packet_vcd_traces<T>(fifo_name, "out_", true, *out_trace_signals);
            }
        }
    }
    
    // Constructor with explicit dump options (dump_in: trace packets coming IN, dump_out: trace packets going OUT)
    CustomFifo(sc_module_name name, int size, bool dump_in_packets = false, bool dump_out_packets = false)
        : sc_module(name), internal_fifo("internal_fifo", size), fifo_name(std::string(name)),
          dump_in(dump_in_packets), dump_out(dump_out_packets) {
        
        // Initialize signal containers if tracing is enabled, then register traces with unified hierarchy
        if (dump_in || dump_out) {
            if (dump_in) {
                in_trace_signals = std::unique_ptr<PacketTraceSignals>(new PacketTraceSignals(fifo_name + "_in"));
                register_packet_vcd_traces<T>(fifo_name, "in_", true, *in_trace_signals);
            }
            
            if (dump_out) {
                out_trace_signals = std::unique_ptr<PacketTraceSignals>(new PacketTraceSignals(fifo_name + "_out"));
                register_packet_vcd_traces<T>(fifo_name, "out_", true, *out_trace_signals);
            }
        }
    }
    
    // sc_fifo_out_if implementation (writing interface)
    void write(const T& data) override {
        internal_fifo.write(data);
        trace_packet_if_enabled(data, "write");
    }
    
    bool nb_write(const T& data) override {
        bool success = internal_fifo.nb_write(data);
        
        if (success) {
            trace_packet_if_enabled(data, "nb_write");
        }
        
        return success;
    }
    
    int num_free() const override { 
        return internal_fifo.num_free(); 
    }
    
    // sc_fifo_in_if implementation (reading interface)  
    T read() override {
        T data = internal_fifo.read();
        trace_packet_if_enabled(data, "read");
        return data;
    }
    
    void read(T& data) override {
        data = read();  // Use the existing read() method
    }
    
    bool nb_read(T& data) override {
        bool success = internal_fifo.nb_read(data);
        
        if (success) {
            trace_packet_if_enabled(data, "nb_read");
        }
        
        return success;
    }
    
    int num_available() const override { 
        return internal_fifo.num_available(); 
    }
    
    // Additional sc_fifo compatibility methods
    int size() const { return internal_fifo.size(); }
    
    // sc_prim_channel interface methods (required for SystemC ports)
    const sc_event& data_written_event() const { 
        return internal_fifo.data_written_event(); 
    }
    
    const sc_event& data_read_event() const { 
        return internal_fifo.data_read_event(); 
    }
    
    // Port binding support - CustomFIFO can be used directly with ports
    // Keep get_fifo() for backward compatibility with existing SSD code
    sc_fifo<T>& get_fifo() { return internal_fifo; }
    
    // Static method to finalize all VCD files
    static void finalize_vcd() {
        VcdTraceManager::finalize();
    }
};

#endif