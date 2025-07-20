#ifndef BASE_PACKET_H
#define BASE_PACKET_H

#include <systemc.h>
#include <string>
#include <iostream>

enum class Command {
    READ,
    WRITE
};

// Abstract Base Packet Class
class BasePacket {
public:
    virtual ~BasePacket() = default; // Virtual destructor for proper cleanup

    // Pure virtual function for dynamic attribute access
    virtual double get_attribute(const std::string& attribute_name) const = 0;

    // Virtual operator<< for polymorphic printing
    virtual void print(std::ostream& os) const = 0;

    // Pure virtual functions for memory operations
    virtual Command get_command() const = 0;
    virtual int get_address() const = 0;
    virtual int get_data() const = 0;
    virtual unsigned char get_databyte() const = 0;
    virtual void set_data(int data) = 0;
    virtual void set_databyte(unsigned char databyte) = 0;

    // Friend function to allow operator<< to call virtual print
    friend std::ostream& operator<<(std::ostream& os, const BasePacket& p) {
        p.print(os);
        return os;
    }

    // Virtual sc_trace for polymorphic tracing
    virtual void sc_trace_impl(sc_trace_file* tf, const std::string& name) const = 0;
};

// Global sc_trace function for BasePacket pointers
inline void sc_trace(sc_trace_file* tf, const BasePacket& p, const std::string& name) {
    p.sc_trace_impl(tf, name);
}

#endif
