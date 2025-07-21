#include <systemc.h>
#include "host_system/host_system.h"
#include "base/memory.h"
#include "packet/base_packet.h" // Include BasePacket
#include "packet/generic_packet.h" // Include GenericPacket
#include "base/delay_line.h"
#include "base/delay_line_databyte.h"
#include "common/common_utils.h"
#include "common/json_config.h"
#include <memory> // For std::unique_ptr
#include <fstream> // For file operations
#include <sstream> // For std::stringstream
#include <chrono>  // For current time
#include <iomanip> // For put_time

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
    
    bool enable_logging = config.get_bool("enable_file_logging", true);
    
    // Generate a timestamp for the log file
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "log/simulation_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".log";
    std::string log_filename = ss.str();

    // Redirect cout to the log file if enabled
    std::ofstream log_file;
    std::streambuf* cout_sbuf = nullptr;
    if (enable_logging) {
        log_file.open(log_filename);
        cout_sbuf = std::cout.rdbuf(); // Save original streambuf
        std::cout.rdbuf(log_file.rdbuf()); // Redirect cout to log_file
    }

    // Create HostSystem (contains TrafficGenerator and IndexAllocator) - uses config from specified directory
    HostSystem host_system("host_system", config_dir + "host_system_config.json");
    DelayLine<BasePacket> downstream_delay("downstream_delay", sc_time(delay_ns, SC_NS), dl_debug); // HostSystem -> Memory
    DelayLine<BasePacket> upstream_delay("upstream_delay", sc_time(delay_ns, SC_NS), dl_debug);     // Memory -> HostSystem
    Memory<BasePacket, int, 65536> memory("memory", mem_debug);

    sc_fifo<std::shared_ptr<BasePacket>> fifo_host_downstream(2);  // HostSystem -> DownstreamDelay
    sc_fifo<std::shared_ptr<BasePacket>> fifo_downstream_mem(2);  // DownstreamDelay -> Memory
    sc_fifo<std::shared_ptr<BasePacket>> fifo_mem_upstream(2);    // Memory -> UpstreamDelay
    sc_fifo<std::shared_ptr<BasePacket>> fifo_upstream_host(2);   // UpstreamDelay -> HostSystem

    // PCIe-style bidirectional connections:
    // Downstream path: HostSystem -> DownstreamDelay -> Memory
    host_system.out(fifo_host_downstream);
    downstream_delay.in(fifo_host_downstream);
    downstream_delay.out(fifo_downstream_mem);
    memory.in(fifo_downstream_mem);
    
    // Upstream path: Memory -> UpstreamDelay -> HostSystem (for index release)
    memory.release_out(fifo_mem_upstream);
    upstream_delay.in(fifo_mem_upstream);
    upstream_delay.out(fifo_upstream_host);
    host_system.release_in(fifo_upstream_host);

    std::cout << "Starting simulation..." << std::endl;
    
    // Record start time for performance measurement
    auto start_time = std::chrono::high_resolution_clock::now();
    sc_start();
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
    if (enable_logging && cout_sbuf) {
        std::cout.rdbuf(cout_sbuf);
        log_file.close();
    }

    return 0;
}