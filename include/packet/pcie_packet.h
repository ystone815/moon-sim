#ifndef PCIE_PACKET_H
#define PCIE_PACKET_H

#include <systemc.h>
#include <string>
#include <iostream>
#include <memory>
#include <map>
#include <random>
#include "packet/base_packet.h"

// PCIe Generation enumeration with speed characteristics
enum class PCIeGeneration {
    GEN1 = 1,    // 2.5 GT/s
    GEN2 = 2,    // 5.0 GT/s  
    GEN3 = 3,    // 8.0 GT/s
    GEN4 = 4,    // 16.0 GT/s
    GEN5 = 5,    // 32.0 GT/s
    GEN6 = 6,    // 64.0 GT/s
    GEN7 = 7     // 128.0 GT/s (Future/Next-gen)
};

// PCIe TLP (Transaction Layer Packet) types
enum class TLPType {
    MEMORY_READ = 0x00,
    MEMORY_WRITE = 0x01,
    IO_READ = 0x02,
    IO_WRITE = 0x03,
    CONFIGURATION_READ = 0x04,
    CONFIGURATION_WRITE = 0x05,
    COMPLETION = 0x0A,
    COMPLETION_DATA = 0x4A
};

// PCIe CRC Scheme characteristics for each generation
struct PCIeCRCScheme {
    std::string scheme_name;        // CRC scheme identifier
    double overhead_percent;        // CRC overhead as percentage of payload
    double processing_delay_ns;     // CRC calculation/verification delay
    double error_detection_rate;    // Error detection capability (e.g., 1e-12)
    double retry_probability;       // Probability of retry due to CRC failure
    uint32_t crc_bits;             // Number of CRC bits
    bool has_fec;                  // Forward Error Correction capability
    bool adaptive_crc;             // Dynamic CRC adjustment
    bool ml_prediction;            // Machine Learning based error prediction
    
    PCIeCRCScheme() : scheme_name("Unknown"), overhead_percent(2.0), processing_delay_ns(50.0),
                     error_detection_rate(1e-12), retry_probability(0.001), crc_bits(32),
                     has_fec(false), adaptive_crc(false), ml_prediction(false) {}
    
    PCIeCRCScheme(const std::string& name, double overhead, double delay, double error_rate,
                 double retry_prob, uint32_t bits, bool fec = false, bool adaptive = false, bool ml = false)
        : scheme_name(name), overhead_percent(overhead), processing_delay_ns(delay),
          error_detection_rate(error_rate), retry_probability(retry_prob), crc_bits(bits),
          has_fec(fec), adaptive_crc(adaptive), ml_prediction(ml) {}
};

// PCIe Generation specifications with CRC schemes
class PCIeGenerationSpecs {
public:
    static const std::map<PCIeGeneration, double> SPEED_GT_PER_SEC;
    static const std::map<PCIeGeneration, PCIeCRCScheme> CRC_SCHEMES;
    
    static double get_speed(PCIeGeneration gen) {
        auto it = SPEED_GT_PER_SEC.find(gen);
        return (it != SPEED_GT_PER_SEC.end()) ? it->second : 2.5; // Default to Gen1
    }
    
    static const PCIeCRCScheme& get_crc_scheme(PCIeGeneration gen) {
        auto it = CRC_SCHEMES.find(gen);
        return (it != CRC_SCHEMES.end()) ? it->second : CRC_SCHEMES.at(PCIeGeneration::GEN1);
    }
    
    static std::string get_generation_name(PCIeGeneration gen) {
        switch (gen) {
            case PCIeGeneration::GEN1: return "PCIe Gen1";
            case PCIeGeneration::GEN2: return "PCIe Gen2";
            case PCIeGeneration::GEN3: return "PCIe Gen3";
            case PCIeGeneration::GEN4: return "PCIe Gen4";
            case PCIeGeneration::GEN5: return "PCIe Gen5";
            case PCIeGeneration::GEN6: return "PCIe Gen6";
            case PCIeGeneration::GEN7: return "PCIe Gen7 (Next-gen)";
            default: return "Unknown Generation";
        }
    }
};

// Static member definitions
const std::map<PCIeGeneration, double> PCIeGenerationSpecs::SPEED_GT_PER_SEC = {
    {PCIeGeneration::GEN1, 2.5},
    {PCIeGeneration::GEN2, 5.0},
    {PCIeGeneration::GEN3, 8.0},
    {PCIeGeneration::GEN4, 16.0},
    {PCIeGeneration::GEN5, 32.0},
    {PCIeGeneration::GEN6, 64.0},
    {PCIeGeneration::GEN7, 128.0}
};

const std::map<PCIeGeneration, PCIeCRCScheme> PCIeGenerationSpecs::CRC_SCHEMES = {
    // Gen1-2: Basic 32-bit CRC
    {PCIeGeneration::GEN1, PCIeCRCScheme("LCRC32", 2.0, 50.0, 1e-12, 0.001, 32, false, false, false)},
    {PCIeGeneration::GEN2, PCIeCRCScheme("LCRC32+", 1.8, 45.0, 1e-13, 0.0008, 32, false, false, false)},
    
    // Gen3-4: 128b/130b encoding with improved CRC
    {PCIeGeneration::GEN3, PCIeCRCScheme("128b130b", 1.5, 30.0, 1e-15, 0.0001, 32, false, false, false)},
    {PCIeGeneration::GEN4, PCIeCRCScheme("Enhanced CRC", 1.3, 25.0, 1e-16, 0.00005, 32, false, false, false)},
    
    // Gen5-6: FEC with advanced CRC
    {PCIeGeneration::GEN5, PCIeCRCScheme("FEC+CRC", 4.0, 20.0, 1e-17, 0.00001, 64, true, false, false)},
    {PCIeGeneration::GEN6, PCIeCRCScheme("Advanced FEC", 3.2, 15.0, 1e-18, 0.000005, 64, true, true, false)},
    
    // Gen7: Next-generation AI-based FEC with ultra-low latency
    {PCIeGeneration::GEN7, PCIeCRCScheme("AI-FEC", 2.5, 5.0, 1e-19, 0.000001, 128, true, true, true)}
};

// PCIe TLP Header structure
struct PCIeTLPHeader {
    TLPType tlp_type;              // Transaction type
    uint8_t format;                // TLP format (3DW or 4DW)
    uint8_t traffic_class;         // Traffic Class (0-7)
    bool id_based_ordering;        // ID-based ordering
    bool relaxed_ordering;         // Relaxed ordering
    bool no_snoop;                 // No snoop attribute
    uint16_t length;               // Data payload length in DW
    uint16_t requester_id;         // Requester ID (Bus:Device:Function)
    uint16_t tag;                  // Transaction tag (0-255 for Gen1-2, 0-1023 for Gen3+)
    uint16_t completer_id;         // Completer ID
    uint32_t address;              // Memory/IO address (32 or 64-bit)
    uint32_t byte_enables;         // First/Last DW byte enables
    
    PCIeTLPHeader() : tlp_type(TLPType::MEMORY_READ), format(0), traffic_class(0),
                     id_based_ordering(false), relaxed_ordering(false), no_snoop(false),
                     length(1), requester_id(0), tag(0), completer_id(0), 
                     address(0), byte_enables(0xF) {}
    
    // Calculate TLP header size in bytes
    uint32_t get_header_size() const {
        return (format == 0) ? 12 : 16;  // 3DW (12 bytes) or 4DW (16 bytes)
    }
    
    // Get maximum tag value based on generation
    uint16_t get_max_tag(PCIeGeneration gen) const {
        return (static_cast<int>(gen) >= 3) ? 1023 : 255;  // Gen3+ supports 10-bit tags
    }
};

// PCIe Packet extending BasePacket
struct PCIePacket : public BasePacket {
    PCIeTLPHeader tlp_header;
    PCIeGeneration generation;
    uint8_t lanes;                     // Number of PCIe lanes (x1, x4, x8, x16)
    uint32_t data_payload_size;        // Actual data size in bytes
    uint32_t total_packet_size;        // Including headers and CRC overhead
    
    // CRC and error simulation
    bool crc_error_injected;           // For testing error recovery
    uint32_t retry_count;              // Number of retries due to CRC errors
    
    // Preserve original BasePacket information
    std::shared_ptr<BasePacket> original_packet;
    
    // Timing information
    sc_time creation_time;             // When packet was created
    sc_time crc_processing_time;       // Time spent on CRC processing
    
    // Default constructor
    PCIePacket(PCIeGeneration gen = PCIeGeneration::GEN3, uint8_t lane_count = 8) 
        : generation(gen), lanes(lane_count), data_payload_size(0), total_packet_size(0),
          crc_error_injected(false), retry_count(0), creation_time(sc_time_stamp()) {
        calculate_packet_size();
    }
    
    // Constructor with original packet
    PCIePacket(std::shared_ptr<BasePacket> orig_packet, PCIeGeneration gen = PCIeGeneration::GEN3, uint8_t lane_count = 8)
        : generation(gen), lanes(lane_count), data_payload_size(0), total_packet_size(0),
          crc_error_injected(false), retry_count(0), original_packet(orig_packet),
          creation_time(sc_time_stamp()) {
        if (orig_packet) {
            setup_from_base_packet(*orig_packet);
        }
        calculate_packet_size();
    }
    
    // Setup PCIe packet from BasePacket
    void setup_from_base_packet(const BasePacket& base_packet) {
        // Convert Command to TLPType
        Command cmd = base_packet.get_command();
        tlp_header.tlp_type = (cmd == Command::READ) ? TLPType::MEMORY_READ : TLPType::MEMORY_WRITE;
        
        // Set addressing information
        tlp_header.address = static_cast<uint32_t>(base_packet.get_address());
        tlp_header.requester_id = 0x0100;  // Default host ID
        tlp_header.completer_id = 0x0200;  // Default device ID
        
        // Set data size
        data_payload_size = static_cast<uint32_t>(base_packet.get_databyte());
        if (data_payload_size == 0) data_payload_size = 64;  // Default size
        
        // Calculate length in DW (32-bit words)
        tlp_header.length = (data_payload_size + 3) / 4;
        
        // Set tag from packet index
        tlp_header.tag = static_cast<uint16_t>(base_packet.get_attribute("index")) % 
                        tlp_header.get_max_tag(generation);
    }
    
    // Calculate total packet size including CRC overhead
    void calculate_packet_size() {
        const PCIeCRCScheme& crc_scheme = PCIeGenerationSpecs::get_crc_scheme(generation);
        
        uint32_t header_size = tlp_header.get_header_size();
        uint32_t payload_size = data_payload_size;
        uint32_t base_size = header_size + payload_size;
        
        // Add CRC overhead
        uint32_t crc_overhead = static_cast<uint32_t>(base_size * crc_scheme.overhead_percent / 100.0);
        total_packet_size = base_size + crc_overhead;
        
        // Update CRC processing time based on packet size and generation
        double base_processing_time = crc_scheme.processing_delay_ns;
        if (crc_scheme.adaptive_crc) {
            // Adaptive CRC adjusts processing time based on packet size
            base_processing_time *= (1.0 + (total_packet_size / 1024.0) * 0.1);
        }
        if (crc_scheme.ml_prediction) {
            // ML prediction reduces processing time for common patterns
            base_processing_time *= 0.7;  // 30% reduction
        }
        
        crc_processing_time = sc_time(base_processing_time, SC_NS);
    }
    
    // Check if CRC error should occur (probabilistic)
    bool should_crc_error_occur() const {
        const PCIeCRCScheme& crc_scheme = PCIeGenerationSpecs::get_crc_scheme(generation);
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(0.0, 1.0);
        
        return dis(gen) < crc_scheme.retry_probability;
    }
    
    // Implementation of virtual methods from BasePacket
    double get_attribute(const std::string& attribute_name) const override {
        if (attribute_name == "generation") {
            return static_cast<double>(static_cast<int>(generation));
        } else if (attribute_name == "lanes") {
            return static_cast<double>(lanes);
        } else if (attribute_name == "tag") {
            return static_cast<double>(tlp_header.tag);
        } else if (attribute_name == "total_size") {
            return static_cast<double>(total_packet_size);
        } else if (attribute_name == "crc_overhead") {
            const PCIeCRCScheme& crc_scheme = PCIeGenerationSpecs::get_crc_scheme(generation);
            return crc_scheme.overhead_percent;
        } else if (attribute_name == "retry_count") {
            return static_cast<double>(retry_count);
        } else if (attribute_name == "databyte") {
            return static_cast<double>(data_payload_size);
        } else if (original_packet) {
            return original_packet->get_attribute(attribute_name);
        } else {
            return 0.0;
        }
    }
    
    void set_attribute(const std::string& attribute_name, double value) override {
        if (attribute_name == "generation") {
            generation = static_cast<PCIeGeneration>(static_cast<int>(value));
            calculate_packet_size();
        } else if (attribute_name == "lanes") {
            lanes = static_cast<uint8_t>(value);
        } else if (attribute_name == "tag") {
            tlp_header.tag = static_cast<uint16_t>(value);
        } else if (attribute_name == "data_payload_size") {
            data_payload_size = static_cast<uint32_t>(value);
            calculate_packet_size();
        } else if (attribute_name == "retry_count") {
            retry_count = static_cast<uint32_t>(value);
        } else if (original_packet) {
            original_packet->set_attribute(attribute_name, value);
        }
    }
    
    void print(std::ostream& os) const override {
        const char* tlp_str = (tlp_header.tlp_type == TLPType::MEMORY_READ) ? "MRd" :
                              (tlp_header.tlp_type == TLPType::MEMORY_WRITE) ? "MWr" : "Other";
        
        os << "PCIe " << PCIeGenerationSpecs::get_generation_name(generation)
           << " x" << static_cast<int>(lanes)
           << ", TLP: " << tlp_str
           << ", Tag: " << tlp_header.tag
           << ", Addr: 0x" << std::hex << tlp_header.address << std::dec
           << ", Size: " << total_packet_size << "B"
           << ", Retries: " << retry_count;
    }
    
    void sc_trace_impl(sc_trace_file* tf, const std::string& name) const override {
        int gen_value = static_cast<int>(generation);
        sc_trace(tf, gen_value, name + ".generation");
        sc_trace(tf, lanes, name + ".lanes");
        sc_trace(tf, tlp_header.tag, name + ".tag");
        sc_trace(tf, tlp_header.address, name + ".address");
        sc_trace(tf, total_packet_size, name + ".total_size");
        sc_trace(tf, retry_count, name + ".retry_count");
    }
    
    // Implementation of memory operation methods (for compatibility)
    Command get_command() const override {
        return (tlp_header.tlp_type == TLPType::MEMORY_READ) ? Command::READ : Command::WRITE;
    }
    
    int get_address() const override {
        return static_cast<int>(tlp_header.address);
    }
    
    int get_data() const override {
        return original_packet ? original_packet->get_data() : 0;
    }
    
    unsigned char get_databyte() const override {
        return static_cast<unsigned char>(data_payload_size);
    }
    
    void set_data(int data) override {
        if (original_packet) original_packet->set_data(data);
    }
    
    void set_databyte(unsigned char databyte) override {
        if (original_packet) original_packet->set_databyte(databyte);
        data_payload_size = static_cast<uint32_t>(databyte);
        calculate_packet_size();
    }
    
    // PCIe-specific methods
    const PCIeCRCScheme& get_crc_scheme() const {
        return PCIeGenerationSpecs::get_crc_scheme(generation);
    }
    
    double get_link_speed_gbps() const {
        double gt_per_sec = PCIeGenerationSpecs::get_speed(generation);
        // Convert GT/s to Gbps (accounting for encoding overhead)
        if (static_cast<int>(generation) >= 3) {
            return gt_per_sec * 128.0 / 130.0;  // 128b/130b encoding (Gen3+)
        } else {
            return gt_per_sec * 8.0 / 10.0;     // 8b/10b encoding (Gen1-2)
        }
    }
    
    double calculate_transmission_time_ns() const {
        double link_speed_gbps = get_link_speed_gbps();
        double effective_bandwidth_gbps = link_speed_gbps * lanes;
        
        // Convert to bytes per nanosecond
        double bytes_per_ns = effective_bandwidth_gbps / 8.0;
        
        // Calculate transmission time
        return static_cast<double>(total_packet_size) / bytes_per_ns;
    }
};

#endif