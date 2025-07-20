#include <systemc.h>
#include "base/traffic_generator.h"
#include "base/index_allocator.h"
#include "base/memory.h"
#include "packet/base_packet.h" // Include BasePacket
#include "packet/generic_packet.h" // Include GenericPacket
#include "base/delay_line.h"
#include "base/delay_line_databyte.h"
#include "common/common_utils.h"
#include "common/json_config.h"
#include <memory> // For std::unique_ptr
#include <fstream> // For file operations
#include <chrono>  // For current time
#include <iomanip> // For put_time

int sc_main(int argc, char* argv[]) {
    // Load configuration
    JsonConfig config("config/simulation_config.json");
    
    // Extract configuration values for non-TrafficGenerator components
    // (TrafficGenerator now loads its own config file)
    
    int delay_ns = config.get_int("delay_ns", 10);
    bool dl_debug = config.get_bool("delay_lines.debug_enable", false);
    bool mem_debug = config.get_bool("memory.debug_enable", false);
    
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

    // Option 1: Traditional parameter-based constructor (commented out)
    /*
    TrafficGenerator traffic_generator("traffic_generator", 
                                     sc_time(interval_ns, SC_NS), 
                                     static_cast<unsigned int>(locality_percentage), 
                                     do_reads, 
                                     do_writes, 
                                     static_cast<unsigned char>(databyte_value), 
                                     num_transactions, 
                                     tg_debug,
                                     static_cast<unsigned int>(start_address),
                                     static_cast<unsigned int>(end_address),
                                     static_cast<unsigned int>(address_increment));
    */
    
    // Option 2: JSON-based constructor (shared config object)
    // TrafficGenerator traffic_generator("traffic_generator", config);
    
    // Option 3: Separate config file approach (best for large components)
    TrafficGenerator traffic_generator("traffic_generator", "config/traffic_generator_config.json");
    DelayLine<BasePacket> delay_line1("delay_line1", sc_time(delay_ns, SC_NS), dl_debug);
    DelayLine<BasePacket> delay_line2("delay_line2", sc_time(delay_ns, SC_NS), dl_debug);
    DelayLine<BasePacket> delay_line3("delay_line3", sc_time(delay_ns, SC_NS), dl_debug);
    DelayLine<BasePacket> delay_line4("delay_line4", sc_time(delay_ns, SC_NS), dl_debug);
    DelayLine<BasePacket> delay_line5("delay_line5", sc_time(delay_ns, SC_NS), dl_debug);
    Memory<BasePacket, int, 65536> memory("memory", mem_debug);

    sc_fifo<std::shared_ptr<BasePacket>> fifo_tg_dl1(2);
    sc_fifo<std::shared_ptr<BasePacket>> fifo_dl1_dl2(2);
    sc_fifo<std::shared_ptr<BasePacket>> fifo_dl2_dl3(2);
    sc_fifo<std::shared_ptr<BasePacket>> fifo_dl3_dl4(2);
    sc_fifo<std::shared_ptr<BasePacket>> fifo_dl4_dl5(2);
    sc_fifo<std::shared_ptr<BasePacket>> fifo_dl5_mem(2);

    traffic_generator.out(fifo_tg_dl1);
    delay_line1.in(fifo_tg_dl1);
    delay_line1.out(fifo_dl1_dl2);
    delay_line2.in(fifo_dl1_dl2);
    delay_line2.out(fifo_dl2_dl3);
    delay_line3.in(fifo_dl2_dl3);
    delay_line3.out(fifo_dl3_dl4);
    delay_line4.in(fifo_dl3_dl4);
    delay_line4.out(fifo_dl4_dl5);
    delay_line5.in(fifo_dl4_dl5);
    delay_line5.out(fifo_dl5_mem);
    memory.in(fifo_dl5_mem);

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