# SystemC SoC Architecture Simulator

A high-performance, production-ready SystemC-based simulation framework for System-on-Chip (SoC) architecture modeling and analysis.

## 🚀 Features

### Core Architecture
- **PCIe-Style Bidirectional Design**: Upstream/downstream DelayLines for realistic interconnect modeling
- **HostSystem Encapsulation**: Integrated TrafficGenerator and IndexAllocator management
- **Template-Based Design**: Header-only template components for maximum flexibility
- **Modular JSON Configuration**: Separate config files per component for better organization
- **Smart Memory Management**: Full `std::shared_ptr` integration for automatic memory safety
- **High Performance**: 25,000,000+ transactions/second throughput
- **Index Resource Management**: Blocking allocation with automatic recycling

### Components

#### HostSystem
- **Encapsulated Architecture**: Contains TrafficGenerator and IndexAllocator as a unified module
- **Separate Configuration**: Dedicated `host_system_config.json` for modular settings
- **Index Resource Management**: Automatic allocation/deallocation with configurable pool size
- **PCIe-Style Interface**: Bidirectional communication with downstream/upstream paths

#### TrafficGenerator
- **Percentage-Based Controls**: 
  - **Locality**: 0-100% (0=full random, 100=full sequential)
  - **Write Ratio**: 0-100% (0=all reads, 100=all writes)
- **Configurable Address Ranges**: Start/end address and increment control
- **Mixed Access Patterns**: Fine-grained locality control for realistic workloads
- **Deterministic C++11 random number generation**
- **Debug Control**: Runtime enable/disable logging

#### DelayLine (Template)
- **PCIe-Style Bidirectional**: Separate downstream (HostSystem→Memory) and upstream (Memory→HostSystem) paths
- **Configurable delays**: Nanosecond-precision timing control
- **Template-based**: Works with any packet type
- **Debug logging**: Optional performance-optimized logging
- **Realistic Interconnect Modeling**: Models real PCIe-like latencies

#### Memory (Template)
- **Scalable size**: From 256 to 65536+ entries
- **Template data types**: Configurable data and packet types
- **Bounds checking**: Safe array access with error handling
- **READ/WRITE operations**: Full memory semantics
- **Release Path**: Automatic packet return for index deallocation

#### IndexAllocator (Template)
- **Resource-Limited Allocation**: Configurable pool size with blocking when exhausted
- **Multiple allocation policies**: Sequential, Round-Robin, Random, Pool-based
- **Automatic Recycling**: Index deallocation when Memory processing completes
- **Template-based**: Packet-type independent
- **Flexible index assignment**: Custom setter functions
- **Statistics tracking**: Allocation metrics and debugging

### Advanced Features
- **Modular JSON Configuration**: Separate config files for each major component
- **Index Resource Management**: Blocking allocation prevents resource exhaustion
- **PCIe-Style Bidirectional Communication**: Realistic interconnect latency modeling
- **Debug Control**: Per-component logging enable/disable
- **Performance Optimization**: I/O bottleneck elimination achieving 25M+ tps
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

### Modular JSON Configuration
The simulator uses separate JSON configuration files for better modularity:

#### `config/simulation_config.json` - Main simulation settings
```json
{
  "simulation": {
    "delay_lines": {
      "delay_ns": 10,
      "debug_enable": false
    },
    "memory": {
      "size": 256,
      "debug_enable": false
    }
  },
  "logging": {
    "enable_file_logging": true,
    "performance_metrics": true
  }
}
```

#### `config/traffic_generator_config.json` - TrafficGenerator settings
```json
{
  "traffic_generator": {
    "num_transactions": 25,
    "interval_ns": 10,
    "locality_percentage": 100,
    "write_percentage": 100,
    "databyte_value": 64,
    "debug_enable": false,
    "start_address": 0,
    "end_address": 65535,
    "address_increment": 64
  }
}
```

#### `config/host_system_config.json` - HostSystem settings
```json
{
  "host_system": {
    "index_allocator": {
      "policy": "SEQUENTIAL",
      "max_index": 10,
      "enable_reuse": true,
      "debug_enable": true
    }
  }
}
```

### Key Parameters
- **locality_percentage**: 0-100 (0=random, 100=sequential access patterns)
- **write_percentage**: 0-100 (0=all reads, 100=all writes)
- **max_index**: IndexAllocator pool size (blocks when exhausted)
- **address_range**: Configurable start/end addresses
- **debug_enable**: Per-component logging control
- **num_transactions**: Simulation workload size

## 📊 Architecture Overview

### PCIe-Style Bidirectional Design
```
┌─────────────────────────────────┐
│           HostSystem            │
│ ┌─────────────────┐ ┌─────────┐ │
│ │ TrafficGenerator│ │IndexAlloc│ │
│ └─────────────────┘ └─────────┘ │
└─────────────────────────────────┘
           │                ▲
     downstream        upstream
        path             path
           │                │
           ▼                │
┌─────────────┐      ┌─────────────┐
│DownstreamDelay│      │UpstreamDelay│
└─────────────┘      └─────────────┘
           │                ▲
           ▼                │
      ┌────────────────────────────┐
      │    Memory (65536)          │
      │  ┌──────┐ ┌──────────────┐ │
      │  │ READ │ │ WRITE + Index│ │
      │  │      │ │ Release      │ │
      │  └──────┘ └──────────────┘ │
      └────────────────────────────┘
```

### Key Features
- **HostSystem**: Encapsulated TrafficGenerator + IndexAllocator
- **PCIe-Style**: Bidirectional DelayLines for realistic latency
- **Index Management**: Resource-limited allocation with recycling
- **Modular Config**: Separate JSON files per component

## 📈 Performance Benchmarks

### Throughput Results
| Configuration | Throughput (tps) | Notes |
|---------------|------------------|-------|
| PCIe-Style (Debug Off) | **25,000,000+** | New bidirectional architecture |
| Index Limited (10 pool) | 10,000,000+ | With resource management |
| Debug Enabled | ~2,000-3,000 | With I/O logging |
| Legacy Pipeline | 600,000+ | Previous 5-stage design |
| 100% Write Ratio | 25,000,000+ | All WRITE operations |
| Mixed Read/Write | 15,000,000+ | 50% write percentage |

### Memory Configurations
- **Small (256 entries)**: Basic testing
- **Large (65536 entries)**: Realistic SoC simulation
- **Custom sizes**: Template parameter control

## 🔧 Advanced Usage

### Template Component Creation
```cpp
// HostSystem with custom config
HostSystem host_system("host", "config/custom_host_config.json");

// Custom memory configuration
Memory<CustomPacket, float, 4096> custom_memory("mem", debug_enabled);

// PCIe-style bidirectional DelayLines
DelayLine<BasePacket> downstream("downstream", sc_time(10, SC_NS), false);
DelayLine<BasePacket> upstream("upstream", sc_time(20, SC_NS), true);
```

### Runtime Configuration Changes
```bash
# Change TrafficGenerator settings without recompilation
vim config/traffic_generator_config.json  # Edit write_percentage, locality_percentage
vim config/host_system_config.json        # Edit max_index, allocation policy
./sim                                      # Run with new settings
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

- **Ultra-High Throughput**: 25M+ transactions/second
- **PCIe-Style Architecture**: Realistic bidirectional interconnect modeling
- **Index Resource Management**: Blocking allocation prevents resource exhaustion
- **Low Overhead**: Minimal logging bottlenecks with optional debug
- **Scalable Design**: Template-based components
- **Memory Efficient**: Smart pointer management with automatic recycling
- **Cache Friendly**: Optimized data access patterns

## 📝 License

[Add your license here]

## 🤝 Contributing

[Add contribution guidelines here]

---
*Built with SystemC for high-performance SoC simulation*