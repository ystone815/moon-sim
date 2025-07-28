#ifndef CUSTOM_FIFO_H
#define CUSTOM_FIFO_H

#include <systemc.h>
#include <memory>
#include <type_traits>
#include "packet/base_packet.h"
#include "common/json_config.h"

// Forward declaration for VCD manager
class VcdManager {
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
};

// Static member definitions
sc_trace_file* VcdManager::trace_file = nullptr;
bool VcdManager::initialized = false;
std::string VcdManager::vcd_filename = "";

// CustomFifo template class
template<typename T>
class CustomFifo : public sc_module {
    SC_HAS_PROCESS(CustomFifo);
    
private:
    sc_fifo<T> internal_fifo;
    bool dump_interface;
    bool dump_resource;
    bool dump_internal;
    std::string fifo_name;
    
    // Internal signals for VCD tracing
    sc_signal<bool> read_event;
    sc_signal<bool> write_event;
    sc_signal<int> fifo_size_signal;
    sc_signal<int> num_available_signal;
    sc_signal<int> num_free_signal;
    
    void update_resource_signals() {
        if (dump_resource) {
            // Use num_available + num_free to get size (since size() is not public)
            int current_size = internal_fifo.num_available() + internal_fifo.num_free();
            fifo_size_signal.write(current_size);
            num_available_signal.write(internal_fifo.num_available());
            num_free_signal.write(internal_fifo.num_free());
        }
    }
    
    void trace_packet_if_enabled(const T& packet, const std::string& operation) {
        if (dump_interface && VcdManager::is_initialized()) {
            // For BasePacket derivatives, use polymorphic tracing
            trace_packet_helper(packet, operation);
        }
    }
    
    // Template specialization helpers for different packet types (C++11 compatible)
    
    // 1. BasePacket shared_ptr types (PCIePacket, FlashPacket, GenericPacket)
    template<typename U = T>
    typename std::enable_if<std::is_convertible<U, std::shared_ptr<BasePacket>>::value, void>::type
    trace_packet_helper(const U& packet, const std::string& operation) {
        if (packet) {
            std::string trace_name = fifo_name + "_" + operation;
            packet->sc_trace_impl(VcdManager::get_trace_file(), trace_name);
        }
    }
    
    // 2. Raw BasePacket derivatives (direct objects, not pointers)
    template<typename U = T>  
    typename std::enable_if<std::is_base_of<BasePacket, U>::value && 
                           !std::is_convertible<U, std::shared_ptr<BasePacket>>::value, void>::type
    trace_packet_helper(const U& packet, const std::string& operation) {
        std::string trace_name = fifo_name + "_" + operation;
        packet.sc_trace_impl(VcdManager::get_trace_file(), trace_name);
    }
    
    // 3. Built-in types with basic tracing  
    template<typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, void>::type  
    trace_packet_helper(const U& packet, const std::string& operation) {
        // For arithmetic types, trace the value directly as a variable
        std::string var_name = fifo_name + "_" + operation + "_value";
        sc_trace(VcdManager::get_trace_file(), packet, var_name);
    }
    
    // 4. Fallback for other types (no tracing)
    template<typename U = T>
    typename std::enable_if<!std::is_convertible<U, std::shared_ptr<BasePacket>>::value &&
                           !std::is_base_of<BasePacket, U>::value &&
                           !std::is_arithmetic<U>::value, void>::type
    trace_packet_helper(const U& packet, const std::string& operation) {
        // No tracing for unsupported types
        // Could add custom tracing logic here for specific types
    }
    
public:
    // Constructor with JSON config
    CustomFifo(sc_module_name name, int size, const JsonConfig& config) 
        : sc_module(name), internal_fifo("internal_fifo", size), fifo_name(std::string(name)) {
        
        // Get dump configuration (use direct keys due to JsonConfig limitations)
        dump_interface = config.get_bool("dump.interface", false);
        dump_resource = config.get_bool("dump.resource", false);
        dump_internal = config.get_bool("dump.internal", false);
        
        // Remove debug output for production use
        // std::cout << "CustomFifo(" << fifo_name << "): dump_interface=" << dump_interface 
        //           << ", dump_resource=" << dump_resource << ", dump_internal=" << dump_internal << std::endl;
        
        // Initialize VCD file if any dump option is enabled
        if (dump_interface || dump_resource || dump_internal) {
            std::string vcd_file = config.get_string("dump.vcd_file", "traces.vcd");
            VcdManager::initialize(vcd_file);
            // std::cout << "CustomFifo(" << fifo_name << "): VCD dump enabled with file: " << vcd_file << std::endl;
            
            // Setup VCD tracing for resource monitoring
            if (dump_resource && VcdManager::is_initialized()) {
                sc_trace(VcdManager::get_trace_file(), fifo_size_signal, fifo_name + ".size");
                sc_trace(VcdManager::get_trace_file(), num_available_signal, fifo_name + ".num_available");
                sc_trace(VcdManager::get_trace_file(), num_free_signal, fifo_name + ".num_free");
            }
            
            // Setup VCD tracing for interface events
            if (dump_interface && VcdManager::is_initialized()) {
                sc_trace(VcdManager::get_trace_file(), read_event, fifo_name + ".read_event");
                sc_trace(VcdManager::get_trace_file(), write_event, fifo_name + ".write_event");
            }
        }
    }
    
    // Alternative constructor with explicit dump options (for backward compatibility)
    CustomFifo(sc_module_name name, int size, bool dump_if = false, bool dump_rsrc = false, bool dump_int = false)
        : sc_module(name), internal_fifo("internal_fifo", size), fifo_name(std::string(name)),
          dump_interface(dump_if), dump_resource(dump_rsrc), dump_internal(dump_int) {
        
        // Initialize VCD file if any dump option is enabled
        if (dump_interface || dump_resource || dump_internal) {
            VcdManager::initialize();
            
            // Setup VCD tracing for resource monitoring
            if (dump_resource && VcdManager::is_initialized()) {
                sc_trace(VcdManager::get_trace_file(), fifo_size_signal, fifo_name + ".size");
                sc_trace(VcdManager::get_trace_file(), num_available_signal, fifo_name + ".num_available");
                sc_trace(VcdManager::get_trace_file(), num_free_signal, fifo_name + ".num_free");
            }
            
            // Setup VCD tracing for interface events
            if (dump_interface && VcdManager::is_initialized()) {
                sc_trace(VcdManager::get_trace_file(), read_event, fifo_name + ".read_event");
                sc_trace(VcdManager::get_trace_file(), write_event, fifo_name + ".write_event");
            }
        }
    }
    
    // sc_fifo compatible interface
    void write(const T& data) {
        internal_fifo.write(data);
        
        if (dump_interface) {
            write_event.write(true);
            trace_packet_if_enabled(data, "write");
            write_event.write(false);
        }
        
        update_resource_signals();
    }
    
    T read() {
        T data = internal_fifo.read();
        
        if (dump_interface) {
            read_event.write(true);
            trace_packet_if_enabled(data, "read");
            read_event.write(false);
        }
        
        update_resource_signals();
        return data;
    }
    
    bool nb_write(const T& data) {
        bool success = internal_fifo.nb_write(data);
        
        if (success && dump_interface) {
            write_event.write(true);
            trace_packet_if_enabled(data, "nb_write");
            write_event.write(false);
        }
        
        update_resource_signals();
        return success;
    }
    
    bool nb_read(T& data) {
        bool success = internal_fifo.nb_read(data);
        
        if (success && dump_interface) {
            read_event.write(true);
            trace_packet_if_enabled(data, "nb_read");
            read_event.write(false);
        }
        
        update_resource_signals();
        return success;
    }
    
    // sc_fifo compatibility methods
    int num_available() const { return internal_fifo.num_available(); }
    int num_free() const { return internal_fifo.num_free(); }
    int size() const { return internal_fifo.size(); }
    
    // Port access for connecting to other modules
    sc_fifo<T>& get_fifo() { return internal_fifo; }
    
    // Operator overloads for seamless replacement
    CustomFifo& operator()(sc_fifo_in<T>& port) {
        port(internal_fifo);
        return *this;
    }
    
    CustomFifo& operator()(sc_fifo_out<T>& port) {
        port(internal_fifo);
        return *this;
    }
    
    // Static method to finalize all VCD files
    static void finalize_vcd() {
        VcdManager::finalize();
    }
};

#endif