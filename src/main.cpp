#include <systemc.h>
#include "host_system/host_system.h"
#include "base/memory.h"
#include "packet/base_packet.h" // Include BasePacket
#include "packet/generic_packet.h" // Include GenericPacket
#include "base/delay_line.h"
#include "base/delay_line_databyte.h"
#include "base/profiler_latency.h"
#include "common/common_utils.h"
#include "common/json_config.h"
#include <memory> // For std::unique_ptr
#include <fstream> // For file operations
#include <sstream> // For std::stringstream
#include <chrono>  // For current time
#include <iomanip> // For put_time
#include <sys/stat.h> // For mkdir and stat
#include <errno.h> // For errno

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
    // Determine config directory (default: config/base/, override with command line argument)
    std::string config_dir = "config/base/";
    if (argc > 1) {
        config_dir = std::string(argv[1]);
        if (config_dir.back() != '/') {
            config_dir += '/';
        }
    }
    
    // Load configuration from specified directory
    JsonConfig config(config_dir + "simulation_config.json");
    
    // Extract configuration values for non-TrafficGenerator components
    // (TrafficGenerator now loads its own config file)
    
    int delay_ns = config.get_int("delay_ns", 10);
    bool dl_debug = config.get_bool("debug_enable", false);
    bool mem_debug = config.get_bool("debug_enable", false);  // Note: same key as dl_debug
    
    // Ensure log directory exists
    if (!create_directory("log")) {
        std::cerr << "Error: Failed to create log directory. Logging may not work properly." << std::endl;
        // Continue execution - program should still work without logging
    }
    
    // Generate a timestamp for the log file
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "log/simulation_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".log";
    std::string log_filename = ss.str();

    // Always redirect cout to the log file
    std::ofstream log_file;
    std::streambuf* cout_sbuf = nullptr; // Initialize to null
    log_file.open(log_filename);
    if (!log_file.is_open()) {
        std::cerr << "Warning: Could not open log file " << log_filename << std::endl;
        // Continue without redirection if file can't be opened
    } else {
        cout_sbuf = std::cout.rdbuf(); // Save original streambuf
        std::cout.rdbuf(log_file.rdbuf()); // Redirect cout to log_file
    }

    // Create HostSystem (contains TrafficGenerator, IndexAllocator, and embedded Profiler) - uses config from specified directory
    HostSystem host_system("host_system", config_dir + "host_system_config.json");
    DelayLine<BasePacket> downstream_delay("downstream_delay", sc_time(delay_ns, SC_NS), dl_debug); // HostSystem -> Memory
    DelayLine<BasePacket> upstream_delay("upstream_delay", sc_time(delay_ns, SC_NS), dl_debug);     // Memory -> HostSystem
    Memory<BasePacket, int, 65536> memory("memory", mem_debug);
    
    // Create latency profiler for measuring request-to-response latency (optimized for performance)
    bool enable_latency_profiler = true; // Set to false for maximum performance (40,000+ tps)
    ProfilerLatency<BasePacket> latency_profiler("latency_profiler", "SystemLatency", sc_time(500, SC_MS), false); // Longer period for less overhead

    // Define latency monitoring module
    SC_MODULE(LatencyMonitor) {
        SC_HAS_PROCESS(LatencyMonitor);
        
        sc_fifo_in<std::shared_ptr<BasePacket>> downstream_monitor;
        sc_fifo_out<std::shared_ptr<BasePacket>> downstream_out;
        sc_fifo_in<std::shared_ptr<BasePacket>> upstream_monitor; 
        sc_fifo_out<std::shared_ptr<BasePacket>> upstream_out;
        ProfilerLatency<BasePacket>* profiler;
        
        LatencyMonitor(sc_module_name name, ProfilerLatency<BasePacket>* prof) 
            : sc_module(name), profiler(prof) {
            SC_THREAD(monitor_downstream);
            SC_THREAD(monitor_upstream);
        }
        
        void monitor_downstream() {
            while (true) {
                auto packet = downstream_monitor.read();
                if (packet) {
                    profiler->profile_request(packet);
                    downstream_out.write(packet);
                }
            }
        }
        
        void monitor_upstream() {
            while (true) {
                auto packet = upstream_monitor.read();
                if (packet) {
                    profiler->profile_response(packet);
                    upstream_out.write(packet);
                }
            }
        }
    };
    
    LatencyMonitor latency_monitor("latency_monitor", &latency_profiler);

    sc_fifo<std::shared_ptr<BasePacket>> fifo_host_downstream(2);  // HostSystem -> DownstreamDelay
    sc_fifo<std::shared_ptr<BasePacket>> fifo_downstream_mem(2);  // DownstreamDelay -> Memory
    sc_fifo<std::shared_ptr<BasePacket>> fifo_mem_upstream(2);    // Memory -> UpstreamDelay
    sc_fifo<std::shared_ptr<BasePacket>> fifo_upstream_host(2);   // UpstreamDelay -> HostSystem
    
    // Additional FIFOs for latency monitoring
    sc_fifo<std::shared_ptr<BasePacket>> fifo_downstream_monitor(2);
    sc_fifo<std::shared_ptr<BasePacket>> fifo_upstream_monitor(2);

    if (enable_latency_profiler) {
        // PCIe-style bidirectional connections with latency monitoring:
        // Downstream path: HostSystem -> LatencyMonitor(request) -> DownstreamDelay -> Memory
        host_system.out(fifo_host_downstream);
        latency_monitor.downstream_monitor(fifo_host_downstream);
        latency_monitor.downstream_out(fifo_downstream_monitor);
        downstream_delay.in(fifo_downstream_monitor);
        downstream_delay.out(fifo_downstream_mem);
        memory.in(fifo_downstream_mem);
        
        // Upstream path: Memory -> UpstreamDelay -> LatencyMonitor(response) -> HostSystem
        memory.release_out(fifo_mem_upstream);
        upstream_delay.in(fifo_mem_upstream);
        upstream_delay.out(fifo_upstream_monitor);
        latency_monitor.upstream_monitor(fifo_upstream_monitor);
        latency_monitor.upstream_out(fifo_upstream_host);
        host_system.release_in(fifo_upstream_host);
    } else {
        // PCIe-style bidirectional connections with embedded profiler (high performance):
        // Downstream path: HostSystem(with profiler) -> DownstreamDelay -> Memory
        host_system.out(fifo_host_downstream);
        downstream_delay.in(fifo_host_downstream);
        downstream_delay.out(fifo_downstream_mem);
        memory.in(fifo_downstream_mem);
        
        // Upstream path: Memory -> UpstreamDelay -> HostSystem (for index release)
        memory.release_out(fifo_mem_upstream);
        upstream_delay.in(fifo_mem_upstream);
        upstream_delay.out(fifo_upstream_host);
        host_system.release_in(fifo_upstream_host);
    }

    std::cout << "Starting simulation..." << std::endl;
    
    // Record start time for performance measurement
    auto start_time = std::chrono::high_resolution_clock::now();
    sc_start();
    
    // Force final reports from profilers
    if (enable_latency_profiler) {
        latency_profiler.force_report();      // ProfilerLatency report  
    }
    host_system.force_profiler_report();  // ProfilerBW report (always enabled)
    std::cout.flush(); // Ensure profiler reports are written to file
    
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Calculate performance metrics
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    double duration_seconds = duration.count() / 1000.0;
    // Note: Transaction count is now configured in TrafficGenerator's config file
    double transactions_per_second = 100000.0 / duration_seconds; // Default assumption
    
    std::cout << "Simulation finished." << std::endl;
    std::cout << "Performance Metrics:" << std::endl;
    std::cout << "Total transactions: (configured in TrafficGenerator)" << std::endl;
    std::cout << "Simulation time: " << duration.count() << " ms (" << duration_seconds << " seconds)" << std::endl;
    std::cout << "Throughput: " << static_cast<int>(transactions_per_second) << " transactions/second" << std::endl;

    // Restore cout to its original streambuf
    if (log_file.is_open() && cout_sbuf) {
        std::cout.rdbuf(cout_sbuf);
        log_file.close();
    }

    return 0;
}