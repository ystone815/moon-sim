#ifndef PCIE_DELAY_LINE_H
#define PCIE_DELAY_LINE_H

#include <systemc.h>
#include <memory>
#include <queue>
#include <random>
#include <iomanip>
#include <functional>
#include "packet/pcie_packet.h"
#include "common/error_handling.h"

// PCIe Link utilization tracking
struct PCIeLinkUtilization {
    double current_utilization;     // Current link utilization (0.0 - 1.0)
    double average_utilization;     // Moving average utilization
    uint64_t total_bytes_transmitted;
    uint64_t total_transmission_time_ns;
    sc_time last_update_time;
    
    PCIeLinkUtilization() : current_utilization(0.0), average_utilization(0.0),
                           total_bytes_transmitted(0), total_transmission_time_ns(0),
                           last_update_time(SC_ZERO_TIME) {}
    
    void update_utilization(uint32_t packet_size, double transmission_time_ns) {
        total_bytes_transmitted += packet_size;
        total_transmission_time_ns += static_cast<uint64_t>(transmission_time_ns);
        
        sc_time current_time = sc_time_stamp();
        if (last_update_time != SC_ZERO_TIME) {
            double time_window_ns = (current_time - last_update_time).to_seconds() * 1e9;
            if (time_window_ns > 0) {
                current_utilization = (transmission_time_ns / time_window_ns) * 100.0;
                
                // Update moving average (exponential smoothing)
                const double alpha = 0.1;  // Smoothing factor
                average_utilization = alpha * current_utilization + (1.0 - alpha) * average_utilization;
            }
        }
        last_update_time = current_time;
    }
};

// PCIe Congestion Model
struct PCIeCongestionModel {
    double congestion_threshold;    // Utilization threshold for congestion (0.8 = 80%)
    double max_congestion_delay_ns; // Maximum additional delay due to congestion
    bool adaptive_flow_control;     // Enable adaptive flow control
    
    PCIeCongestionModel() : congestion_threshold(0.8), max_congestion_delay_ns(1000.0),
                           adaptive_flow_control(true) {}
    
    double calculate_congestion_delay(double utilization) const {
        if (utilization < congestion_threshold) {
            return 0.0;  // No congestion
        }
        
        // Exponential increase in delay as utilization approaches 100%
        double congestion_factor = (utilization - congestion_threshold) / (1.0 - congestion_threshold);
        double congestion_delay = max_congestion_delay_ns * std::pow(congestion_factor, 2.0);
        
        return std::min(congestion_delay, max_congestion_delay_ns);
    }
};

// Template-based PCIe DelayLine with generation-specific CRC and timing
template<typename PacketType>
SC_MODULE(PCIeDelayLine) {
    SC_HAS_PROCESS(PCIeDelayLine);
    
    // SystemC ports
    sc_fifo_in<std::shared_ptr<PacketType>> in;
    sc_fifo_out<std::shared_ptr<PacketType>> out;
    
    // PCIe configuration
    const PCIeGeneration m_generation;
    const uint8_t m_lanes;
    const bool m_debug_enable;
    const bool m_enable_crc_simulation;
    const bool m_enable_congestion_model;
    
    // Link characteristics
    PCIeLinkUtilization m_link_utilization;
    PCIeCongestionModel m_congestion_model;
    
    // CRC error simulation
    mutable std::mt19937 m_rng;
    mutable std::uniform_real_distribution<double> m_error_dist;
    
    // Statistics
    uint64_t m_total_packets_processed;
    uint64_t m_total_crc_errors;
    uint64_t m_total_retries;
    double m_total_processing_time_ns;
    
    // Packet conversion functions
    std::function<std::shared_ptr<PCIePacket>(std::shared_ptr<PacketType>)> m_to_pcie_converter;
    std::function<std::shared_ptr<PacketType>(std::shared_ptr<PCIePacket>)> m_from_pcie_converter;
    
    // Main processing method
    void process_packets() {
        while (true) {
            auto packet = in.read();
            
            if (!packet) {
                SOC_SIM_ERROR("PCIeDelayLine", soc_sim::error::codes::INVALID_PACKET_TYPE,
                             "Received null packet");
                continue;
            }
            
            // Convert to PCIePacket if needed
            std::shared_ptr<PCIePacket> pcie_packet = convert_to_pcie_packet(packet);
            if (!pcie_packet) {
                SOC_SIM_ERROR("PCIeDelayLine", soc_sim::error::codes::INVALID_PACKET_TYPE,
                             "Failed to convert packet to PCIePacket");
                continue;
            }
            
            // Process PCIe packet with CRC and timing simulation
            bool success = process_pcie_packet(pcie_packet);
            
            // Handle CRC errors and retries
            while (!success && pcie_packet->retry_count < 3) {  // Max 3 retries
                pcie_packet->retry_count++;
                m_total_retries++;
                
                if (m_debug_enable) {
                    std::cout << sc_time_stamp() << " | " << basename() 
                              << ": CRC error, retry #" << pcie_packet->retry_count 
                              << " for tag " << pcie_packet->tlp_header.tag << std::endl;
                }
                
                // Retry with additional delay
                wait(100, SC_NS);  // Retry penalty
                success = process_pcie_packet(pcie_packet);
            }
            
            if (!success) {
                SOC_SIM_ERROR("PCIeDelayLine", soc_sim::error::codes::DEVICE_ERROR,
                             "Packet failed after maximum retries");
                continue;
            }
            
            // Convert back to original packet type if needed
            auto output_packet = convert_from_pcie_packet(pcie_packet);
            if (output_packet) {
                out.write(output_packet);
            }
            
            m_total_packets_processed++;
        }
    }
    
    // Process individual PCIe packet with timing and CRC simulation
    bool process_pcie_packet(std::shared_ptr<PCIePacket> pcie_packet) {
        const PCIeCRCScheme& crc_scheme = pcie_packet->get_crc_scheme();
        
        // Calculate transmission time based on generation and lanes
        double transmission_time_ns = pcie_packet->calculate_transmission_time_ns();
        
        // Add CRC processing delay
        double crc_processing_delay = crc_scheme.processing_delay_ns;
        
        // Apply Gen7 AI-based optimizations
        if (m_generation == PCIeGeneration::GEN7) {
            crc_processing_delay = apply_gen7_optimizations(pcie_packet, crc_processing_delay);
        }
        
        // Calculate congestion delay if enabled
        double congestion_delay = 0.0;
        if (m_enable_congestion_model) {
            congestion_delay = m_congestion_model.calculate_congestion_delay(
                m_link_utilization.current_utilization);
        }
        
        // Total delay
        double total_delay_ns = transmission_time_ns + crc_processing_delay + congestion_delay;
        
        // Apply delay
        if (total_delay_ns > 0.0) {
            wait(total_delay_ns, SC_NS);
        }
        
        // Update link utilization
        m_link_utilization.update_utilization(pcie_packet->total_packet_size, transmission_time_ns);
        
        // CRC error simulation
        bool crc_success = true;
        if (m_enable_crc_simulation) {
            crc_success = !pcie_packet->should_crc_error_occur();
            if (!crc_success) {
                m_total_crc_errors++;
                pcie_packet->crc_error_injected = true;
            }
        }
        
        // Debug logging
        if (m_debug_enable) {
            std::cout << sc_time_stamp() << " | " << basename() << ": "
                      << PCIeGenerationSpecs::get_generation_name(m_generation)
                      << " x" << static_cast<int>(m_lanes)
                      << ", Tag: " << pcie_packet->tlp_header.tag
                      << ", Size: " << pcie_packet->total_packet_size << "B"
                      << ", Delay: " << std::fixed << std::setprecision(1) << total_delay_ns << "ns"
                      << ", Util: " << std::setprecision(1) << m_link_utilization.current_utilization << "%"
                      << ", CRC: " << (crc_success ? "OK" : "ERROR")
                      << std::endl;
        }
        
        m_total_processing_time_ns += total_delay_ns;
        return crc_success;
    }
    
    // Apply Gen7 AI-based optimizations
    double apply_gen7_optimizations(std::shared_ptr<PCIePacket> pcie_packet, double base_delay) const {
        if (m_generation != PCIeGeneration::GEN7) {
            return base_delay;
        }
        
        const PCIeCRCScheme& crc_scheme = pcie_packet->get_crc_scheme();
        double optimized_delay = base_delay;
        
        // AI-based pattern recognition (simulated)
        if (crc_scheme.ml_prediction) {
            // Reduce delay for common patterns (simulated ML optimization)
            if (pcie_packet->tlp_header.tlp_type == TLPType::MEMORY_READ) {
                optimized_delay *= 0.8;  // 20% reduction for reads
            } else if (pcie_packet->data_payload_size <= 64) {
                optimized_delay *= 0.9;  // 10% reduction for small packets
            }
        }
        
        // Adaptive CRC strength based on link quality
        if (crc_scheme.adaptive_crc) {
            double link_quality = 1.0 - (m_total_crc_errors / std::max(1UL, m_total_packets_processed));
            if (link_quality > 0.99) {
                // High link quality - reduce CRC overhead
                optimized_delay *= 0.7;
            } else if (link_quality < 0.95) {
                // Poor link quality - increase CRC strength
                optimized_delay *= 1.3;
            }
        }
        
        return std::max(1.0, optimized_delay);  // Minimum 1ns delay
    }
    
    // Packet conversion methods
    std::shared_ptr<PCIePacket> convert_to_pcie_packet(std::shared_ptr<PacketType> packet) {
        // Try direct cast first
        auto pcie_packet = std::dynamic_pointer_cast<PCIePacket>(packet);
        if (pcie_packet) {
            return pcie_packet;
        }
        
        // Use custom converter if available
        if (m_to_pcie_converter) {
            return m_to_pcie_converter(packet);
        }
        
        // Default conversion for BasePacket-derived types
        auto base_packet = std::dynamic_pointer_cast<BasePacket>(packet);
        if (base_packet) {
            return std::make_shared<PCIePacket>(base_packet, m_generation, m_lanes);
        }
        
        return nullptr;
    }
    
    std::shared_ptr<PacketType> convert_from_pcie_packet(std::shared_ptr<PCIePacket> pcie_packet) {
        // Try direct cast first
        auto packet = std::dynamic_pointer_cast<PacketType>(pcie_packet);
        if (packet) {
            return packet;
        }
        
        // Use custom converter if available
        if (m_from_pcie_converter) {
            return m_from_pcie_converter(pcie_packet);
        }
        
        // Return original packet if available
        if (pcie_packet->original_packet) {
            auto original = std::dynamic_pointer_cast<PacketType>(pcie_packet->original_packet);
            if (original) {
                // Update original packet with any changes
                original->set_databyte(static_cast<unsigned char>(pcie_packet->data_payload_size));
                return original;
            }
        }
        
        // Last resort: return the PCIePacket as-is (if types are compatible)
        return std::dynamic_pointer_cast<PacketType>(pcie_packet);
    }
    
    // Constructor
    PCIeDelayLine(sc_module_name name,
                  PCIeGeneration generation = PCIeGeneration::GEN3,
                  uint8_t lanes = 8,
                  bool debug_enable = false,
                  bool enable_crc_simulation = true,
                  bool enable_congestion_model = true)
        : sc_module(name),
          m_generation(generation),
          m_lanes(lanes),
          m_debug_enable(debug_enable),
          m_enable_crc_simulation(enable_crc_simulation),
          m_enable_congestion_model(enable_congestion_model),
          m_rng(std::random_device{}()),
          m_error_dist(0.0, 1.0),
          m_total_packets_processed(0),
          m_total_crc_errors(0),
          m_total_retries(0),
          m_total_processing_time_ns(0.0) {
        
        if (m_debug_enable) {
            const PCIeCRCScheme& crc_scheme = PCIeGenerationSpecs::get_crc_scheme(m_generation);
            std::cout << "0 s | " << basename() << ": PCIe DelayLine initialized "
                      << "(" << PCIeGenerationSpecs::get_generation_name(m_generation)
                      << " x" << static_cast<int>(m_lanes)
                      << ", CRC: " << crc_scheme.scheme_name
                      << ", Speed: " << PCIeGenerationSpecs::get_speed(m_generation) << " GT/s"
                      << ", Overhead: " << std::setprecision(1) << crc_scheme.overhead_percent << "%"
                      << ")" << std::endl;
        }
        
        SC_THREAD(process_packets);
    }
    
    // Constructor with custom converters
    PCIeDelayLine(sc_module_name name,
                  std::function<std::shared_ptr<PCIePacket>(std::shared_ptr<PacketType>)> to_pcie,
                  std::function<std::shared_ptr<PacketType>(std::shared_ptr<PCIePacket>)> from_pcie,
                  PCIeGeneration generation = PCIeGeneration::GEN3,
                  uint8_t lanes = 8,
                  bool debug_enable = false)
        : sc_module(name),
          m_generation(generation),
          m_lanes(lanes),
          m_debug_enable(debug_enable),
          m_enable_crc_simulation(true),
          m_enable_congestion_model(true),
          m_rng(std::random_device{}()),
          m_error_dist(0.0, 1.0),
          m_total_packets_processed(0),
          m_total_crc_errors(0),
          m_total_retries(0),
          m_total_processing_time_ns(0.0),
          m_to_pcie_converter(to_pcie),
          m_from_pcie_converter(from_pcie) {
        
        if (m_debug_enable) {
            const PCIeCRCScheme& crc_scheme = PCIeGenerationSpecs::get_crc_scheme(m_generation);
            std::cout << "0 s | " << basename() << ": PCIe DelayLine initialized "
                      << "(" << PCIeGenerationSpecs::get_generation_name(m_generation)
                      << " x" << static_cast<int>(m_lanes)
                      << ", CRC: " << crc_scheme.scheme_name
                      << ", Speed: " << PCIeGenerationSpecs::get_speed(m_generation) << " GT/s"
                      << ", Overhead: " << std::setprecision(1) << crc_scheme.overhead_percent << "%"
                      << ")" << std::endl;
        }
        
        SC_THREAD(process_packets);
    }
    
    // Statistics and monitoring methods
    uint64_t get_total_packets_processed() const { return m_total_packets_processed; }
    uint64_t get_total_crc_errors() const { return m_total_crc_errors; }
    uint64_t get_total_retries() const { return m_total_retries; }
    double get_average_processing_time_ns() const {
        return (m_total_packets_processed > 0) ? 
               (m_total_processing_time_ns / m_total_packets_processed) : 0.0;
    }
    
    double get_current_utilization() const { return m_link_utilization.current_utilization; }
    double get_average_utilization() const { return m_link_utilization.average_utilization; }
    
    double get_crc_error_rate() const {
        return (m_total_packets_processed > 0) ? 
               (static_cast<double>(m_total_crc_errors) / m_total_packets_processed) : 0.0;
    }
    
    // Configuration access
    PCIeGeneration get_generation() const { return m_generation; }
    uint8_t get_lanes() const { return m_lanes; }
    const PCIeCRCScheme& get_crc_scheme() const { 
        return PCIeGenerationSpecs::get_crc_scheme(m_generation); 
    }
    
    // Congestion model configuration
    void set_congestion_threshold(double threshold) {
        m_congestion_model.congestion_threshold = std::max(0.0, std::min(1.0, threshold));
    }
    
    void set_max_congestion_delay(double max_delay_ns) {
        m_congestion_model.max_congestion_delay_ns = std::max(0.0, max_delay_ns);
    }
};

// Type aliases for common configurations
using PCIeBasePacketDelayLine = PCIeDelayLine<BasePacket>;
using PCIeGen3DelayLine = PCIeDelayLine<BasePacket>;
using PCIeGen7DelayLine = PCIeDelayLine<BasePacket>;

// Helper functions to create PCIe DelayLines with specific configurations
template<typename PacketType>
PCIeDelayLine<PacketType>* create_pcie_gen7_delay_line(
    sc_module_name name,
    uint8_t lanes = 16,
    bool debug_enable = false) {
    
    return new PCIeDelayLine<PacketType>(name, PCIeGeneration::GEN7, lanes, debug_enable);
}

template<typename PacketType>
PCIeDelayLine<PacketType>* create_pcie_delay_line(
    sc_module_name name,
    PCIeGeneration generation,
    uint8_t lanes = 8,
    bool debug_enable = false) {
    
    return new PCIeDelayLine<PacketType>(name, generation, lanes, debug_enable);
}

#endif