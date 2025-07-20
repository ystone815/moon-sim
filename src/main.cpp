#include <systemc.h>
#include "base/traffic_generator.h"
#include "base/memory.h"
#include "packet/base_packet.h" // Include BasePacket
#include "packet/generic_packet.h" // Include GenericPacket
#include "base/delay_line.h"
#include "base/delay_line_databyte.h"
#include "common/common_utils.h"
#include <memory> // For std::unique_ptr
#include <fstream> // For file operations
#include <chrono>  // For current time
#include <iomanip> // For put_time

int sc_main(int argc, char* argv[]) {
    // Generate a timestamp for the log file
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "log/simulation_" << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S") << ".log";
    std::string log_filename = ss.str();

    // Redirect cout to the log file
    std::ofstream log_file(log_filename);
    std::streambuf* cout_sbuf = std::cout.rdbuf(); // Save original streambuf
    std::cout.rdbuf(log_file.rdbuf()); // Redirect cout to log_file

    TrafficGenerator traffic_generator("traffic_generator", sc_time(10, SC_NS), LocalityType::RANDOM, true, true, 64, 10);
    Memory memory("memory");
    DelayLine delay_line("delay_line", sc_time(50, SC_NS)); // Instantiate DelayLine with 50ns delay
    DelayLineDatabyte delay_line_databyte("delay_line_databyte", 1, sc_time(1, SC_NS), "databyte"); // Instantiate DelayLineDatabyte with 1 byte width, using "databyte" for delay

    sc_fifo<std::shared_ptr<BasePacket>> fifo_tg_dl(2); // FIFO from TrafficGenerator to DelayLine
    sc_fifo<std::shared_ptr<BasePacket>> fifo_dl_dl_databyte(2); // FIFO from DelayLine to DelayLineDatabyte
    sc_fifo<std::shared_ptr<BasePacket>> fifo_dl_databyte_mem(2); // FIFO from DelayLineDatabyte to Memory

    traffic_generator.out(fifo_tg_dl);
    delay_line.in(fifo_tg_dl);
    delay_line.out(fifo_dl_dl_databyte);
    delay_line_databyte.in(fifo_dl_dl_databyte);
    delay_line_databyte.out(fifo_dl_databyte_mem);
    memory.in(fifo_dl_databyte_mem);

    std::cout << "Starting simulation..." << std::endl;
    sc_start();
    std::cout << "Simulation finished." << std::endl;

    // Restore cout to its original streambuf
    std::cout.rdbuf(cout_sbuf);
    log_file.close();

    return 0;
}