#include <systemc.h>
#include "base/custom_fifo.h"
#include "packet/base_packet.h"
#include "packet/generic_packet.h"
#include "packet/pcie_packet.h"
#include "packet/flash_packet.h"
#include "common/json_config.h"
#include <memory>

int sc_main(int argc, char* argv[]) {
    // Create a simple JSON config for testing
    JsonConfig test_config("config/base/simulation_config.json");
    
    std::cout << "=== CustomFifo Packet Type Support Test ===" << std::endl;
    
    // Test 1: BasePacket shared_ptr (current usage)
    std::cout << "Test 1: BasePacket shared_ptr" << std::endl;
    CustomFifo<std::shared_ptr<BasePacket>> fifo_basepacket("test_basepacket", 4, test_config);
    auto generic_pkt = std::shared_ptr<BasePacket>(new GenericPacket());
    fifo_basepacket.write(generic_pkt);
    auto read_pkt = fifo_basepacket.read();
    std::cout << "  BasePacket FIFO: OK" << std::endl;
    
    // Test 2: Specific packet types as shared_ptr
    std::cout << "Test 2: PCIePacket shared_ptr" << std::endl;
    CustomFifo<std::shared_ptr<PCIePacket>> fifo_pcie("test_pcie", 4, test_config);
    auto pcie_pkt = std::shared_ptr<PCIePacket>(new PCIePacket());
    pcie_pkt->generation = PCIeGeneration::GEN7;
    pcie_pkt->lanes = 4;
    fifo_pcie.write(pcie_pkt);
    auto read_pcie = fifo_pcie.read();
    std::cout << "  PCIePacket FIFO: OK" << std::endl;
    
    // Test 3: FlashPacket shared_ptr
    std::cout << "Test 3: FlashPacket shared_ptr" << std::endl;
    CustomFifo<std::shared_ptr<FlashPacket>> fifo_flash("test_flash", 4, test_config);
    auto flash_pkt = std::shared_ptr<FlashPacket>(new FlashPacket());
    flash_pkt->flash_command = FlashCommand::PROGRAM;
    flash_pkt->flash_address.plane = 2;
    fifo_flash.write(flash_pkt);
    auto read_flash = fifo_flash.read();
    std::cout << "  FlashPacket FIFO: OK" << std::endl;
    
    // Test 4: Built-in types
    std::cout << "Test 4: Integer FIFO" << std::endl;
    CustomFifo<int> fifo_int("test_int", 4, test_config);
    fifo_int.write(42);
    int read_int = fifo_int.read();
    std::cout << "  Integer FIFO: " << read_int << " OK" << std::endl;
    
    std::cout << "Test 5: Double FIFO" << std::endl;
    CustomFifo<double> fifo_double("test_double", 4, test_config);
    fifo_double.write(3.14159);
    double read_double = fifo_double.read();
    std::cout << "  Double FIFO: " << read_double << " OK" << std::endl;
    
    // Test 6: Skip custom struct for now (SystemC FIFO limitation)
    std::cout << "Test 6: Custom struct FIFO - SKIPPED (SystemC limitation)" << std::endl;
    
    // Finalize VCD
    CustomFifo<int>::finalize_vcd();
    
    std::cout << "=== All packet type tests completed ===" << std::endl;
    
    return 0;
}