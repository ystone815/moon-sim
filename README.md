# SystemC SoC Architecture Simulator

A high-performance, production-ready SystemC-based simulation framework for System-on-Chip (SoC) architecture modeling and analysis.

## 🚀 Features

### Core Architecture
- **PCIe-Style Bidirectional Design**: Upstream/downstream DelayLines for realistic interconnect modeling
- **HostSystem Encapsulation**: Integrated TrafficGenerator, IndexAllocator, and Function-based Profiler
- **Template-Based Design**: Header-only template components for maximum flexibility
- **Modular JSON Configuration**: Separate config files per component for better organization
- **Smart Memory Management**: Full `std::shared_ptr` integration for automatic memory safety
- **High Performance**: 640+ MB/sec throughput with real-time profiling
- **Index Resource Management**: Simplified minimum-allocation with automatic recycling
- **SC_MODULE Profiling**: Real-time throughput monitoring with SystemC-compliant architecture

### Components

#### HostSystem
- **Encapsulated Architecture**: Contains TrafficGenerator, IndexAllocator, and SC_MODULE Profiler as a unified module
- **Separate Configuration**: Dedicated `host_system_config.json` for modular settings
- **Index Resource Management**: Simplified minimum-allocation strategy with automatic recycling
- **PCIe-Style Interface**: Bidirectional communication with downstream/upstream paths
- **Integrated Profiling**: Built-in SC_MODULE throughput profiler with 10ms reporting and dedicated process infrastructure

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
- **Simplified Allocation**: Always allocates from minimum available index
- **Resource-Limited Pool**: Configurable pool size with blocking when exhausted
- **Automatic Recycling**: Index deallocation when Memory processing completes
- **Out-of-Order Handling**: Supports non-sequential index release patterns
- **Template-based**: Packet-type independent
- **Flexible index assignment**: Custom setter functions
- **Clean Output**: Optional statistics display for focused profiler reporting

#### FunctionProfiler (Template, SC_MODULE)
- **SC_MODULE Architecture**: Proper SystemC module with dedicated reporting process infrastructure
- **Function-based Design**: Direct function calls eliminate port/event overhead
- **Real-time Monitoring**: Automatic databyte extraction and accumulation
- **Configurable Reporting**: Periodic throughput reports (default 10ms intervals)
- **Comprehensive Metrics**: bytes/sec, MB/sec, packets/sec with cumulative totals
- **Template-based**: Works with any packet type supporting get_attribute()
- **Performance Optimized**: 610+ MB/sec throughput with minimal overhead
- **Dedicated Process Ready**: Infrastructure for independent reporting thread

### Advanced Features
- **Modular JSON Configuration**: Separate config files organized in `config/base/` structure
- **Simplified Index Management**: Minimum-allocation strategy with out-of-order release support
- **Function-based Profiling**: Real-time throughput monitoring integrated within HostSystem
- **PCIe-Style Bidirectional Communication**: Realistic interconnect latency modeling
- **Debug Control**: Per-component logging enable/disable for performance optimization
- **Automatic Directory Creation**: Smart log directory handling for deployment environments
- **Error Handling**: Standardized error codes and reporting
- **Polymorphic Design**: Clean virtual interface patterns

## 🛠 Building

### Prerequisites
- SystemC 2.3.3+ (Custom C++11 build recommended)
- GCC with C++11 support
- Python 3 (for parameter sweeps)
- Make

### Environment Setup

**Set SYSTEMC_HOME environment variable** (required):

```bash
# For the included C++11 build:
export SYSTEMC_HOME=/tmp/systemc-install/usr/local/systemc-cxx11

# For system-wide installation:
export SYSTEMC_HOME=/usr/local/systemc

# For custom installation:
export SYSTEMC_HOME=/path/to/your/systemc

# Add to ~/.bashrc for permanent setting:
echo 'export SYSTEMC_HOME=/tmp/systemc-install/usr/local/systemc-cxx11' >> ~/.bashrc
source ~/.bashrc
```

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

# Run parameter sweeps
python3 run_sweep.py config/sweeps/small_test.json
```

## ⚙️ Configuration

### Modular JSON Configuration
The simulator uses separate JSON configuration files organized in `config/base/` for better modularity:

#### `config/base/simulation_config.json` - Main simulation settings
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
    "log_directory": "log",
    "performance_metrics": true
  }
}
```

#### `config/base/traffic_generator_config.json` - TrafficGenerator settings
```json
{
  "traffic_generator": {
    "num_transactions": 1000000,
    "interval_ns": 100,
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

#### `config/base/host_system_config.json` - HostSystem settings
```json
{
  "host_system": {
    "index_allocator": {
      "max_index": 100,
      "debug_enable": false
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

### PCIe-Style Bidirectional Design with Integrated Profiler
```
┌─────────────────────────────────────────────┐
│                HostSystem                   │
│ ┌─────────────────┐ ┌─────────┐ ┌────────┐  │
│ │ TrafficGenerator│ │IndexAlloc│ │SC_MODULE│  │
│ │                 │ │         │ │Profiler │  │
│ └─────────────────┘ └─────────┘ └────────┘  │
└─────────────────────────────────────────────┘
           │                ▲
     downstream        upstream
        path             path
    (with profiling)        │
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
- **HostSystem**: Encapsulated TrafficGenerator + IndexAllocator + SC_MODULE FunctionProfiler
- **PCIe-Style**: Bidirectional DelayLines for realistic latency
- **Index Management**: Simplified minimum-allocation with out-of-order recycling
- **SC_MODULE Profiling**: SystemC-compliant profiler with dedicated process infrastructure
- **Function-based Interface**: Real-time throughput monitoring integrated within datapath
- **Modular Config**: Separate JSON files per component in `config/base/`
- **Smart Directory Handling**: Automatic log directory creation for deployment

## 📈 Performance Benchmarks

### Throughput Results
| Configuration | Throughput | Notes |
|---------------|------------|-------|
| **SC_MODULE FunctionProfiler** | **640+ MB/sec** | Real-time profiling with 610.35 MB/sec sustained |
| **Simulation Performance** | **48,000-50,000 tps** | 1M transactions in ~2 seconds |
| **Profiler Overhead** | **Minimal** | SystemC-compliant with dedicated process infrastructure |
| **Index Pool (100 entries)** | **No blocking** | Minimum allocation strategy |
| **Debug Disabled** | **Optimal** | Clean profiler-only output |
| **Parameter Sweeps** | **48,473-50,000 tps** | Consistent across write ratio variations (latest test) |

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

- **Timestamped logs**: Saved to `log/simulation_YYYYMMDD_HHMMSS.log` (auto-created directory)
- **Real-time profiling**: 10ms periodic throughput reports with bytes/sec, MB/sec, packets/sec
- **Performance metrics**: Comprehensive timing and throughput analysis
- **Clean output**: IndexAllocator statistics disabled for focused profiler reporting
- **Parameter sweep results**: CSV data and summary reports in `regression_runs/`
- **Error reporting**: Centralized error handling with deployment-friendly directory creation

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

- **SC_MODULE Profiling**: 640+ MB/sec throughput monitoring with SystemC-compliant architecture
- **PCIe-Style Architecture**: Realistic bidirectional interconnect modeling
- **Simplified Index Management**: Minimum-allocation strategy with out-of-order recycling
- **Deployment Ready**: Automatic log directory creation for production environments
- **Dedicated Process Infrastructure**: Ready for independent reporting thread implementation
- **Low Overhead**: Direct function calls with minimal SystemC process overhead
- **Scalable Design**: Template-based components with C++11 compatibility
- **Memory Efficient**: Smart pointer management with automatic recycling
- **Real-time Monitoring**: 10ms periodic reporting with comprehensive metrics

## 📝 License

[Add your license here]

## 🤝 Contributing

[Add contribution guidelines here]

---
*Built with SystemC for high-performance SoC simulation*