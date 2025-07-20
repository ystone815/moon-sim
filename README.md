# SystemC SoC Architecture Simulator

A high-performance, production-ready SystemC-based simulation framework for System-on-Chip (SoC) architecture modeling and analysis.

## 🚀 Features

### Core Architecture
- **Template-Based Design**: Header-only template components for maximum flexibility
- **JSON Configuration**: Runtime configuration without recompilation
- **Smart Memory Management**: Full `std::shared_ptr` integration for automatic memory safety
- **High Performance**: 600,000+ transactions/second throughput
- **Modular Pipeline**: Configurable component chains

### Components

#### TrafficGenerator
- **Percentage-Based Locality**: 0-100% control (0=full random, 100=full sequential)
- **Configurable Address Ranges**: Start/end address and increment control
- **Mixed Access Patterns**: Fine-grained locality control for realistic workloads
- **Deterministic C++11 random number generation**
- **Debug Control**: Runtime enable/disable logging

#### DelayLine (Template)
- **Configurable delays**: Nanosecond-precision timing control
- **Template-based**: Works with any packet type
- **Debug logging**: Optional performance-optimized logging
- **Pipeline modeling**: Multi-stage delay chains

#### Memory (Template)
- **Scalable size**: From 256 to 65536+ entries
- **Template data types**: Configurable data and packet types
- **Bounds checking**: Safe array access with error handling
- **READ/WRITE operations**: Full memory semantics

#### IndexAllocator (Template)
- **Multiple allocation policies**: Sequential, Round-Robin, Random, Pool-based
- **Template-based**: Packet-type independent
- **Flexible index assignment**: Custom setter functions
- **Statistics tracking**: Allocation metrics and debugging

### Advanced Features
- **JSON Runtime Configuration**: No recompilation needed for parameter changes
- **Debug Control**: Per-component logging enable/disable
- **Performance Optimization**: I/O bottleneck elimination
- **Error Handling**: Standardized error codes and reporting
- **Polymorphic Design**: Clean virtual interface patterns

## 🛠 Building

### Prerequisites
- SystemC 2.3.4+ installed at `/usr/local/systemc`
- GCC with C++11 support
- Make

### Build Commands
```bash
# Build the simulator
make

# Clean build artifacts
make clean

# Build from scratch
make clean && make

# Run simulation
./sim
```

## ⚙️ Configuration

### JSON Configuration File
The simulator uses `config/simulation_config.json` for runtime configuration:

```json
{
  "simulation": {
    "num_transactions": 100000,
    "traffic_generator": {
      "interval_ns": 10,
      "locality_percentage": 75,
      "do_reads": true,
      "do_writes": true,
      "databyte_value": 64,
      "debug_enable": false,
      "start_address": 0,
      "end_address": 65535,
      "address_increment": 64
    },
    "delay_lines": {
      "count": 5,
      "delay_ns": 10,
      "debug_enable": false
    },
    "memory": {
      "debug_enable": false
    }
  },
  "logging": {
    "enable_file_logging": true,
    "performance_metrics": true
  }
}
```

### Key Parameters
- **locality_percentage**: 0-100 (0=random, 100=sequential)
- **address_range**: Configurable start/end addresses
- **debug_enable**: Per-component logging control
- **num_transactions**: Simulation workload size

## 📊 Architecture Overview

```
┌─────────────────┐    ┌─────────────┐ × 5    ┌────────────────┐
│ TrafficGenerator│───▶│  DelayLine  │────────▶│ Memory (65536) │
└─────────────────┘    └─────────────┘        └────────────────┘
         │                     │                       │
   JSON configured       Template-based         Template scalable
   Locality 0-100%      Configurable delay      Any size/type
   Address ranges       Debug control           Bounds checking
```

## 📈 Performance Benchmarks

### Throughput Results
| Configuration | Throughput (tps) | Notes |
|---------------|------------------|-------|
| Debug Enabled | ~2,000-3,000 | With I/O logging |
| Debug Disabled | **600,000+** | Pure simulation |
| 0% Locality (Random) | 561,797 | Full random access |
| 75% Locality (Mixed) | 578,034 | Mixed pattern |
| 100% Locality (Sequential) | 606,060 | Full sequential |

### Memory Configurations
- **Small (256 entries)**: Basic testing
- **Large (65536 entries)**: Realistic SoC simulation
- **Custom sizes**: Template parameter control

## 🔧 Advanced Usage

### Template Component Creation
```cpp
// Custom memory configuration
Memory<CustomPacket, float, 4096> custom_memory("mem", debug_enabled);

// Multiple delay lines
DelayLine<BasePacket> delay1("dl1", sc_time(10, SC_NS), false);
DelayLine<BasePacket> delay2("dl2", sc_time(20, SC_NS), true);
```

### Runtime Configuration Changes
```bash
# Change locality without recompilation
vim config/simulation_config.json  # Edit locality_percentage
./sim                               # Run with new settings
```

## 🧪 Simulation Output

- **Timestamped logs**: Saved to `log/simulation_YYYYMMDD_HHMMSS.log`
- **Performance metrics**: Throughput and timing analysis
- **Debug traces**: Detailed packet flow (when enabled)
- **Error reporting**: Centralized error handling

## 🎯 Use Cases

- **SoC Performance Modeling**: Multi-core and memory subsystem analysis
- **Memory Hierarchy Simulation**: Cache, DRAM, storage interactions
- **Bus Architecture Evaluation**: Interconnect performance analysis
- **Workload Characterization**: Mixed locality pattern studies
- **Hardware/Software Co-Design**: System-level validation

## 🔬 Code Quality

This simulator follows modern C++ and SystemC best practices:
- ✅ Template-based generic programming
- ✅ JSON runtime configuration
- ✅ Smart pointer memory management
- ✅ Header-only template design
- ✅ Performance-optimized logging
- ✅ Comprehensive error handling
- ✅ Type safety throughout
- ✅ Scalable architecture

## 🚀 Performance Features

- **High Throughput**: 600K+ transactions/second
- **Low Overhead**: Minimal logging bottlenecks
- **Scalable Design**: Template-based components
- **Memory Efficient**: Smart pointer management
- **Cache Friendly**: Optimized data access patterns

## 📝 License

[Add your license here]

## 🤝 Contributing

[Add contribution guidelines here]

---
*Built with SystemC for high-performance SoC simulation*