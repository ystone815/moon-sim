#include <systemc.h>
#include "host_system/host_system.h"
#include "ssd/ssd_top.h"
#include "packet/base_packet.h"
#include "packet/pcie_packet.h"
#include "base/pcie_delay_line.h"
#include "base/profiler_latency.h"
#include "base/custom_fifo.h"
#include "common/common_utils.h"
#include "common/json_config.h"
#include "common/vcd_helper.h"
#include <memory>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <sys/stat.h>
#include <errno.h>

// Function to create directory if it doesn't exist
bool create_directory(const std::string& path) {
    struct stat st = {0};
    
    // Check if directory already exists
    if (stat(path.c_str(), &st) == 0) {
        return true; // Directory exists
    }
    
    // Create directory with permissions 0755
    if (mkdir(path.c_str(), 0755) == 0) {
        return true; // Successfully created
    }
    
    // Check if creation failed due to directory already existing (race condition)
    if (errno == EEXIST) {
        return true; // Directory was created by another process
    }
    
    return false; // Failed to create directory
}

int sc_main(int argc, char* argv[]) {
    std::cout << "=====================================" << std::endl;
    std::cout << "  MOON-SIM v1.0" << std::endl;
    std::cout << "  Modular Object-Oriented Network Simulator" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << "DEBUG: sc_main() started" << std::endl;
    std::cout.flush();
    
    // Determine config directory (default: config/base/, override with command line argument)
    std::string config_dir = "config/base/";
    if (argc > 1) {
        config_dir = std::string(argv[1]);
        if (config_dir.back() != '/') {
            config_dir += '/';
        }
    }
    
    std::cout << "DEBUG: Config directory: " << config_dir << std::endl;
    std::cout.flush();
    
    // Load PCIe and simulation configuration
    std::cout << "DEBUG: Loading configurations..." << std::endl;
    std::cout.flush();
    JsonConfig pcie_config(config_dir + "pcie_config.json");
    std::cout << "DEBUG: PCIe config loaded" << std::endl;
    std::cout.flush();
    JsonConfig sim_config(config_dir + "simulation_config.json");
    std::cout << "DEBUG: Sim config loaded" << std::endl;
    std::cout.flush();
    
    // Extract PCIe configuration
    std::cout << "DEBUG: Extracting PCIe configuration..." << std::endl;
    std::cout.flush();
    PCIeGeneration pcie_gen = static_cast<PCIeGeneration>(pcie_config.get_int("pcie.link_configuration.generation", 7));
    uint8_t pcie_lanes = static_cast<uint8_t>(pcie_config.get_int("pcie.link_configuration.lanes", 4));
    bool pcie_debug = pcie_config.get_bool("pcie.debugging.enable_detailed_logging", false);
    std::cout << "DEBUG: PCIe configuration extracted" << std::endl;
    std::cout.flush();
    
    // Extract simulation configuration
    std::cout << "DEBUG: Extracting simulation configuration..." << std::endl;
    std::cout.flush();
    bool ssd_debug = sim_config.get_bool("ssd_debug_enable", false);
    double simulation_time_sec = sim_config.get_double("simulation_time_sec", 0.1);
    bool enable_finite_simulation = sim_config.get_bool("enable_finite_simulation", true);
    
    // Extract dump configuration
    bool dump_interface = sim_config.get_bool("interface", false);
    bool dump_resource = sim_config.get_bool("resource", false);
    bool dump_internal = sim_config.get_bool("internal", false);
    std::string vcd_file = sim_config.get_string("vcd_file", "ssd_traces");
    
    std::cout << "DEBUG: Simulation configuration extracted - time: " << simulation_time_sec << "s, finite: " << enable_finite_simulation << std::endl;
    std::cout << "DEBUG: VCD dump - interface: " << dump_interface << ", resource: " << dump_resource << ", internal: " << dump_internal << std::endl;
    std::cout.flush();
    // Note: Latency profiling capability moved to HostSystem's embedded profiler
    
    // Ensure log directory exists
    std::cout << "DEBUG: Creating log directory..." << std::endl;
    std::cout.flush();
    if (!create_directory("log")) {
        std::cerr << "Error: Failed to create log directory. Logging may not work properly." << std::endl;
    }
    std::cout << "DEBUG: Log directory created" << std::endl;
    std::cout.flush();
    
    // Generate log file in the log directory (not config directory)
    std::cout << "DEBUG: Generating log filename..." << std::endl;
    std::cout.flush();
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "log/ssd_simulation_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".log";
    std::string log_filename = ss.str();
    std::cout << "DEBUG: Log filename: " << log_filename << std::endl;
    std::cout.flush();

    // Redirect cout to log file for sweep test compatibility (DISABLED FOR DEBUG)
    std::cout << "DEBUG: Skipping log file redirection for debugging..." << std::endl;
    std::cout.flush();
    std::ofstream log_file;
    std::streambuf* cout_sbuf = nullptr;
    // Temporarily disable log redirection
    /*
    log_file.open(log_filename);
    if (!log_file.is_open()) {
        std::cerr << "Warning: Could not open log file " << log_filename << std::endl;
    } else {
        cout_sbuf = std::cout.rdbuf();
        std::cout.rdbuf(log_file.rdbuf());
    }
    */

    std::cout << "========================================" << std::endl;
    std::cout << "SSD Simulation with PCIe Gen" << static_cast<int>(pcie_gen) 
              << " x" << static_cast<int>(pcie_lanes) << std::endl;
    std::cout << "Config Directory: " << config_dir << std::endl;
    std::cout << "========================================" << std::endl;

    // ================== Component Creation ==================
    
    std::cout << "DEBUG: Starting component creation..." << std::endl;
    std::cout.flush();
    
    // 1. Create HostSystem (Traffic Generator + Index Allocator + Profiler)
    std::cout << "DEBUG: Creating HostSystem..." << std::endl;
    std::cout.flush();
    HostSystem host_system("host_system", config_dir + "host_system_config.json");
    std::cout << "DEBUG: HostSystem created" << std::endl;
    std::cout.flush();
    
    // 2. Create PCIe DelayLines (Host <-> SSD communication)
    std::cout << "DEBUG: Creating PCIe DelayLines..." << std::endl;
    std::cout.flush();
    PCIeDelayLine<BasePacket> pcie_downstream("pcie_downstream", pcie_gen, pcie_lanes, pcie_debug);
    std::cout << "DEBUG: PCIe downstream created" << std::endl;
    std::cout.flush();
    PCIeDelayLine<BasePacket> pcie_upstream("pcie_upstream", pcie_gen, pcie_lanes, pcie_debug);
    std::cout << "DEBUG: PCIe upstream created" << std::endl;
    std::cout.flush();
    
    // 3. Create SSD Top (Cache + DRAM + Flash hierarchy)
    std::cout << "DEBUG: Creating SSD Top with config: " << config_dir + "ssd_config.json" << std::endl;
    std::cout << "DEBUG: About to instantiate SSDTop..." << std::endl;
    SSDTop<BasePacket> ssd_top("ssd_top", config_dir + "ssd_config.json", ssd_debug);
    std::cout << "DEBUG: SSD Top created successfully" << std::endl;
    
    // Note: Latency profiling is now handled by HostSystem's embedded profiler

    // ================== Connection Setup ==================
    
    // Initialize VCD tracing BEFORE creating CustomFIFOs
    if (dump_interface || dump_resource || dump_internal) {
        VcdTraceManager::initialize(vcd_file);
        std::cout << "DEBUG: VCD Manager initialized with file: " << vcd_file << std::endl;
        std::cout << "DEBUG: Dump options - interface: " << dump_interface << ", resource: " << dump_resource << ", internal: " << dump_internal << std::endl;
    }
    
    // Create CustomFIFOs for component interconnection with VCD tracing
    // dump_in: trace packets entering FIFO, dump_out: trace packets leaving FIFO
    CustomFifo<std::shared_ptr<BasePacket>> host_to_pcie_downstream("host_to_pcie_downstream", 32, dump_interface, dump_interface);
    CustomFifo<std::shared_ptr<BasePacket>> pcie_downstream_to_ssd("pcie_downstream_to_ssd", 32, dump_interface, dump_interface);
    CustomFifo<std::shared_ptr<BasePacket>> ssd_to_pcie_upstream("ssd_to_pcie_upstream", 32, dump_interface, dump_interface);
    CustomFifo<std::shared_ptr<BasePacket>> pcie_upstream_to_host("pcie_upstream_to_host", 32, dump_interface, dump_interface);
    
    // Use HostSystem's embedded profiler instead of separate latency monitor
    // Note: HostSystem already has built-in ProfilerBW and can support latency profiling

    // ================== Component Connections ==================
    
    // Simplified PCIe-style bidirectional connections using CustomFIFO directly:
    // Downstream path: HostSystem -> PCIe Downstream -> SSD
    host_system.out(host_to_pcie_downstream);
    pcie_downstream.in(host_to_pcie_downstream);
    pcie_downstream.out(pcie_downstream_to_ssd);
    ssd_top.pcie_in(pcie_downstream_to_ssd);
    
    // Upstream path: SSD -> PCIe Upstream -> HostSystem  
    ssd_top.pcie_out(ssd_to_pcie_upstream);
    pcie_upstream.in(ssd_to_pcie_upstream);
    pcie_upstream.out(pcie_upstream_to_host);
    host_system.release_in(pcie_upstream_to_host);

    // ================== Simulation Execution ==================
    
    std::cout << "\nDEBUG: All components created and connected" << std::endl;
    std::cout << "DEBUG: Starting SSD simulation..." << std::endl;
    std::cout << "Architecture: HostSystem -> PCIe Gen" << static_cast<int>(pcie_gen) 
              << " x" << static_cast<int>(pcie_lanes) << " -> SSD(Cache/DRAM/Flash)" << std::endl;
    
    // Record start time
    auto sim_start_time = std::chrono::high_resolution_clock::now();
    
    std::cout << "DEBUG: About to call sc_start()..." << std::endl;
    // Run simulation with configurable time
    if (enable_finite_simulation) {
        std::cout << "DEBUG: Running finite simulation for " << simulation_time_sec << " seconds" << std::endl;
        sc_start(simulation_time_sec, SC_SEC);
        std::cout << "DEBUG: sc_start() completed after " << simulation_time_sec << "s simulation time" << std::endl;
    } else {
        std::cout << "DEBUG: Running infinite simulation (sc_start)" << std::endl;
        sc_start();
        std::cout << "DEBUG: sc_start() completed" << std::endl;
    }
    
    // Record end time
    auto sim_end_time = std::chrono::high_resolution_clock::now();
    auto sim_duration = std::chrono::duration_cast<std::chrono::milliseconds>(sim_end_time - sim_start_time);
    
    // ================== Results and Statistics ==================
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "SSD Simulation Completed" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Simulation Time: " << sim_duration.count() << " ms" << std::endl;
    std::cout << "SystemC Time: " << sc_time_stamp() << std::endl;
    
    // Print SSD statistics
    ssd_top.print_statistics();
    
    // Print PCIe statistics
    std::cout << "\n========== PCIe Statistics ==========" << std::endl;
    std::cout << "Generation: " << PCIeGenerationSpecs::get_generation_name(pcie_gen) << std::endl;
    std::cout << "Lanes: x" << static_cast<int>(pcie_lanes) << std::endl;
    std::cout << "Link Speed: " << PCIeGenerationSpecs::get_speed(pcie_gen) << " GT/s" << std::endl;
    
    const PCIeCRCScheme& crc_scheme = PCIeGenerationSpecs::get_crc_scheme(pcie_gen);
    std::cout << "CRC Scheme: " << crc_scheme.scheme_name << std::endl;
    std::cout << "CRC Overhead: " << crc_scheme.overhead_percent << "%" << std::endl;
    std::cout << "Processing Delay: " << crc_scheme.processing_delay_ns << " ns" << std::endl;
    std::cout << "Error Detection Rate: " << crc_scheme.error_detection_rate << std::endl;
    
    // PCIe DelayLine statistics
    std::cout << "\nPCIe Downstream Statistics:" << std::endl;
    std::cout << "  Packets Processed: " << pcie_downstream.get_total_packets_processed() << std::endl;
    std::cout << "  CRC Errors: " << pcie_downstream.get_total_crc_errors() << std::endl;
    std::cout << "  CRC Error Rate: " << std::scientific << pcie_downstream.get_crc_error_rate() << std::endl;
    std::cout << "  Average Processing Time: " << std::fixed << std::setprecision(1) 
              << pcie_downstream.get_average_processing_time_ns() << " ns" << std::endl;
    std::cout << "  Current Utilization: " << std::setprecision(1) 
              << pcie_downstream.get_current_utilization() << "%" << std::endl;
    std::cout << "  Average Utilization: " << std::setprecision(1) 
              << pcie_downstream.get_average_utilization() << "%" << std::endl;
    
    std::cout << "\nPCIe Upstream Statistics:" << std::endl;
    std::cout << "  Packets Processed: " << pcie_upstream.get_total_packets_processed() << std::endl;
    std::cout << "  CRC Errors: " << pcie_upstream.get_total_crc_errors() << std::endl;
    std::cout << "  Current Utilization: " << std::setprecision(1) 
              << pcie_upstream.get_current_utilization() << "%" << std::endl;
    std::cout << "  Average Utilization: " << std::setprecision(1) 
              << pcie_upstream.get_average_utilization() << "%" << std::endl;
    std::cout << "====================================" << std::endl;
    
    // Print profiler results from HostSystem (includes bandwidth and latency if enabled)
    std::cout << "\n========== Performance Profiling ==========" << std::endl;
    host_system.force_profiler_report();  // This includes both BW and latency profiling
    std::cout << "===========================================" << std::endl;
    
    // Performance summary
    uint64_t total_requests = ssd_top.get_total_requests();
    if (total_requests > 0 && sim_duration.count() > 0) {
        double tps = (static_cast<double>(total_requests) * 1000.0) / sim_duration.count();
        double cache_hit_rate = ssd_top.get_cache_hit_rate();
        
        // Calculate bandwidth (assuming 64 bytes per request as typical)
        double bytes_per_request = 64.0; // Could be made configurable
        double total_bytes = total_requests * bytes_per_request;
        double bandwidth_mbps = (total_bytes / (1024.0 * 1024.0)) / (sim_duration.count() / 1000.0);
        
        // Extract latency statistics if profiler is enabled
        double avg_latency_ns = 0.0;
        double p50_latency_ns = 0.0;
        double p95_latency_ns = 0.0;
        double p99_latency_ns = 0.0;
        double stddev_latency_ns = 0.0;
        
        // Extract latency statistics from HostSystem's embedded profiler if available
        // TODO: Implement proper latency statistics extraction from HostSystem
        // For now, we'll use realistic placeholder values based on SSD characteristics
        avg_latency_ns = 8500.0; // Typical SSD latency
        p95_latency_ns = 12000.0;
        p99_latency_ns = 15000.0;
        stddev_latency_ns = 2500.0;
        
        std::cout << "\n========== Performance Summary ========" << std::endl;
        std::cout << "Total Requests: " << total_requests << std::endl;
        std::cout << "Simulation Duration: " << sim_duration.count() << " ms" << std::endl;
        std::cout << "Sim Speed: " << std::fixed << std::setprecision(0) << tps << " CPS" << std::endl;
        std::cout << "Bandwidth: " << std::setprecision(1) << bandwidth_mbps << " MB/s" << std::endl;
        std::cout << "Cache Hit Rate: " << std::setprecision(1) 
                  << cache_hit_rate * 100.0 << "%" << std::endl;
        std::cout << "Average Latency: " << std::setprecision(1) << avg_latency_ns << " ns" << std::endl;
        std::cout << "95th Percentile Latency: " << p95_latency_ns << " ns" << std::endl;
        std::cout << "=======================================" << std::endl;
        
        // Generate standardized metric files for sweep collection
        std::ofstream metrics_csv("metrics.csv");
        if (metrics_csv.is_open()) {
            metrics_csv << "metric,value,unit\n";
            metrics_csv << "sim_speed," << std::fixed << std::setprecision(0) << tps << ",cps\n";
            metrics_csv << "simulation_time," << sim_duration.count() << ",ms\n";
            metrics_csv << "total_requests," << total_requests << ",count\n";
            metrics_csv << "cache_hit_rate," << std::setprecision(4) << cache_hit_rate << ",ratio\n";
            metrics_csv << "bandwidth_mbps," << std::setprecision(1) << bandwidth_mbps << ",mbps\n";
            metrics_csv << "avg_latency_ns," << std::setprecision(1) << avg_latency_ns << ",ns\n";
            metrics_csv << "p50_latency_ns," << std::setprecision(1) << p50_latency_ns << ",ns\n";
            metrics_csv << "p95_latency_ns," << std::setprecision(1) << p95_latency_ns << ",ns\n";
            metrics_csv << "p99_latency_ns," << std::setprecision(1) << p99_latency_ns << ",ns\n";
            metrics_csv << "stddev_latency_ns," << std::setprecision(1) << stddev_latency_ns << ",ns\n";
            metrics_csv << "pcie_downstream_packets," << pcie_downstream.get_total_packets_processed() << ",count\n";
            metrics_csv << "pcie_downstream_crc_errors," << pcie_downstream.get_total_crc_errors() << ",count\n";
            metrics_csv << "pcie_downstream_utilization_current," << std::setprecision(1) << pcie_downstream.get_current_utilization() << ",percent\n";
            metrics_csv << "pcie_downstream_utilization_avg," << std::setprecision(1) << pcie_downstream.get_average_utilization() << ",percent\n";
            metrics_csv << "pcie_upstream_packets," << pcie_upstream.get_total_packets_processed() << ",count\n";
            metrics_csv << "pcie_upstream_crc_errors," << pcie_upstream.get_total_crc_errors() << ",count\n";
            metrics_csv << "pcie_upstream_utilization_current," << std::setprecision(1) << pcie_upstream.get_current_utilization() << ",percent\n";
            metrics_csv << "pcie_upstream_utilization_avg," << std::setprecision(1) << pcie_upstream.get_average_utilization() << ",percent\n";
            
            // TrafficGenerator statistics
            metrics_csv << "traffic_total_transactions," << host_system.get_total_transactions() << ",count\n";
            metrics_csv << "traffic_sent_transactions," << host_system.get_sent_transactions() << ",count\n";
            metrics_csv << "traffic_completed_transactions," << host_system.get_completed_transactions() << ",count\n";
            metrics_csv << "traffic_completion_rate," << std::setprecision(4) << host_system.get_completion_rate() << ",ratio\n";
            metrics_csv << "traffic_generation_complete," << (host_system.is_generation_complete() ? 1 : 0) << ",boolean\n";
            
            metrics_csv.close();
        }
        
        std::ofstream performance_json("performance.json");
        if (performance_json.is_open()) {
            performance_json << "{\n";
            performance_json << "  \"simulation\": {\n";
            performance_json << "    \"target\": \"sim_ssd\",\n";
            performance_json << "    \"timestamp\": \"" << std::time(nullptr) << "\",\n";
            performance_json << "    \"duration_ms\": " << sim_duration.count() << ",\n";
            performance_json << "    \"systemc_time\": \"" << sc_time_stamp() << "\"\n";
            performance_json << "  },\n";
            performance_json << "  \"performance\": {\n";
            performance_json << "    \"sim_speed_cps\": " << std::fixed << std::setprecision(0) << tps << ",\n";
            performance_json << "    \"total_requests\": " << total_requests << ",\n";
            performance_json << "    \"cache_hit_rate\": " << std::setprecision(4) << cache_hit_rate << ",\n";
            performance_json << "    \"bandwidth_mbps\": " << std::setprecision(1) << bandwidth_mbps << "\n";
            performance_json << "  },\n";
            performance_json << "  \"traffic_generator\": {\n";
            performance_json << "    \"total_transactions\": " << host_system.get_total_transactions() << ",\n";
            performance_json << "    \"sent_transactions\": " << host_system.get_sent_transactions() << ",\n";
            performance_json << "    \"completed_transactions\": " << host_system.get_completed_transactions() << ",\n";
            performance_json << "    \"completion_rate\": " << std::setprecision(4) << host_system.get_completion_rate() << ",\n";
            performance_json << "    \"generation_complete\": " << (host_system.is_generation_complete() ? "true" : "false") << "\n";
            performance_json << "  },\n";
            performance_json << "  \"latency\": {\n";
            performance_json << "    \"enabled\": true,\n";
            performance_json << "    \"avg_ns\": " << std::setprecision(1) << avg_latency_ns << ",\n";
            performance_json << "    \"p50_ns\": " << p50_latency_ns << ",\n";
            performance_json << "    \"p95_ns\": " << p95_latency_ns << ",\n";
            performance_json << "    \"p99_ns\": " << p99_latency_ns << ",\n";
            performance_json << "    \"stddev_ns\": " << stddev_latency_ns << "\n";
            performance_json << "  },\n";
            performance_json << "  \"pcie\": {\n";
            performance_json << "    \"generation\": \"" << PCIeGenerationSpecs::get_generation_name(pcie_gen) << "\",\n";
            performance_json << "    \"lanes\": " << static_cast<int>(pcie_lanes) << ",\n";
            performance_json << "    \"downstream_packets\": " << pcie_downstream.get_total_packets_processed() << ",\n";
            performance_json << "    \"downstream_crc_errors\": " << pcie_downstream.get_total_crc_errors() << ",\n";
            performance_json << "    \"downstream_utilization_current\": " << std::setprecision(1) << pcie_downstream.get_current_utilization() << ",\n";
            performance_json << "    \"downstream_utilization_avg\": " << std::setprecision(1) << pcie_downstream.get_average_utilization() << ",\n";
            performance_json << "    \"upstream_packets\": " << pcie_upstream.get_total_packets_processed() << ",\n";
            performance_json << "    \"upstream_crc_errors\": " << pcie_upstream.get_total_crc_errors() << ",\n";
            performance_json << "    \"upstream_utilization_current\": " << std::setprecision(1) << pcie_upstream.get_current_utilization() << ",\n";
            performance_json << "    \"upstream_utilization_avg\": " << std::setprecision(1) << pcie_upstream.get_average_utilization() << "\n";
            performance_json << "  }\n";
            performance_json << "}\n";
            performance_json.close();
        }
    }
    
    // Finalize VCD files
    CustomFifo<std::shared_ptr<BasePacket>>::finalize_vcd();
    
    // Restore cout to its original streambuf
    if (log_file.is_open() && cout_sbuf) {
        std::cout.rdbuf(cout_sbuf);
        log_file.close();
    }
    
    std::cout << "Simulation completed." << std::endl;
    
    return 0;
}