#ifndef VCD_HELPER_H
#define VCD_HELPER_H

#include <systemc.h>
#include <string>
#include <memory>
#include <type_traits>
#include "packet/base_packet.h"

// Enhanced VCD Trace Manager for centralized VCD file management
class VcdTraceManager {
private:
    static sc_trace_file* trace_file;
    static bool initialized;
    static std::string vcd_filename;
    
public:
    static void initialize(const std::string& filename = "traces") {
        if (!initialized) {
            // Remove .vcd extension if present, as SystemC adds it automatically
            std::string clean_filename = filename;
            if (clean_filename.size() >= 4 && clean_filename.substr(clean_filename.size() - 4) == ".vcd") {
                clean_filename = clean_filename.substr(0, clean_filename.size() - 4);
            }
            
            trace_file = sc_create_vcd_trace_file(clean_filename.c_str());
            vcd_filename = clean_filename;
            initialized = true;
        }
    }
    
    static void finalize() {
        if (initialized && trace_file) {
            sc_close_vcd_trace_file(trace_file);
            trace_file = nullptr;
            initialized = false;
        }
    }
    
    static sc_trace_file* get_trace_file() {
        return trace_file;
    }
    
    static bool is_initialized() {
        return initialized;
    }
    
    static const std::string& get_filename() {
        return vcd_filename;
    }
};

// Static member definitions
sc_trace_file* VcdTraceManager::trace_file = nullptr;
bool VcdTraceManager::initialized = false;
std::string VcdTraceManager::vcd_filename = "";

// Packet field signal container for organized VCD tracing
struct PacketTraceSignals {
    sc_signal<int> command_signal;
    sc_signal<int> address_signal;
    sc_signal<int> data_signal;
    sc_signal<unsigned char> databyte_signal;
    sc_signal<int> index_signal;
    
    PacketTraceSignals(const std::string& name_prefix) 
        : command_signal((name_prefix + "_cmd").c_str()),
          address_signal((name_prefix + "_addr").c_str()),
          data_signal((name_prefix + "_data").c_str()),
          databyte_signal((name_prefix + "_databyte").c_str()),
          index_signal((name_prefix + "_idx").c_str()) {}
    
    // Simplified constructor for cleaner signal names (optional)
    PacketTraceSignals() 
        : command_signal("cmd"),
          address_signal("addr"),
          data_signal("data"),
          databyte_signal("databyte"),
          index_signal("idx") {}
};

// Template function to register packet field traces with signal prefix support
template<typename T>
void register_packet_vcd_traces(const std::string& trace_prefix, 
                                const std::string& signal_prefix,
                                bool enable_traces,
                                PacketTraceSignals& signals) {
    if (enable_traces && VcdTraceManager::is_initialized()) {
        sc_trace(VcdTraceManager::get_trace_file(), signals.command_signal, trace_prefix + "." + signal_prefix + "command");
        sc_trace(VcdTraceManager::get_trace_file(), signals.address_signal, trace_prefix + "." + signal_prefix + "address");
        sc_trace(VcdTraceManager::get_trace_file(), signals.data_signal, trace_prefix + "." + signal_prefix + "data");
        sc_trace(VcdTraceManager::get_trace_file(), signals.databyte_signal, trace_prefix + "." + signal_prefix + "databyte");
        sc_trace(VcdTraceManager::get_trace_file(), signals.index_signal, trace_prefix + "." + signal_prefix + "index");
    }
}

// Backward compatibility: Template function to register packet field traces (original version)
template<typename T>
void register_packet_vcd_traces(const std::string& trace_prefix, 
                                bool enable_traces,
                                PacketTraceSignals& signals) {
    register_packet_vcd_traces<T>(trace_prefix, "", enable_traces, signals);
}

// Template function to update packet trace signal values during simulation
// Specialization for BasePacket shared_ptr types
template<typename T>
typename std::enable_if<std::is_convertible<T, std::shared_ptr<BasePacket>>::value, void>::type
update_packet_vcd_traces(const T& packet, PacketTraceSignals& signals) {
    if (packet) {
        signals.command_signal.write(static_cast<int>(packet->get_command()));
        signals.address_signal.write(packet->get_address());
        signals.data_signal.write(packet->get_data());
        signals.databyte_signal.write(packet->get_databyte());
        signals.index_signal.write(static_cast<int>(packet->get_attribute("index")));
    }
}

// Specialization for raw BasePacket derivatives
template<typename T>
typename std::enable_if<std::is_base_of<BasePacket, T>::value && 
                       !std::is_convertible<T, std::shared_ptr<BasePacket>>::value, void>::type
update_packet_vcd_traces(const T& packet, PacketTraceSignals& signals) {
    signals.command_signal.write(static_cast<int>(packet.get_command()));
    signals.address_signal.write(packet.get_address());
    signals.data_signal.write(packet.get_data());
    signals.databyte_signal.write(packet.get_databyte());
    signals.index_signal.write(static_cast<int>(packet.get_attribute("index")));
}

// Fallback for unsupported types (no-op)
template<typename T>
typename std::enable_if<!std::is_convertible<T, std::shared_ptr<BasePacket>>::value &&
                       !std::is_base_of<BasePacket, T>::value, void>::type
update_packet_vcd_traces(const T& packet, PacketTraceSignals& signals) {
    // No updates for unsupported packet types
}

// Convenience function for dual in/out trace registration with unified hierarchy
template<typename T>
void register_dual_packet_traces(const std::string& component_name,
                                 bool enable_in_traces,
                                 bool enable_out_traces,
                                 PacketTraceSignals& in_signals,
                                 PacketTraceSignals& out_signals) {
    if (enable_in_traces) {
        register_packet_vcd_traces<T>(component_name, "in_", true, in_signals);
    }
    
    if (enable_out_traces) {
        register_packet_vcd_traces<T>(component_name, "out_", true, out_signals);
    }
}

#endif // VCD_HELPER_H