# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Development Commands

### Build the simulation
```bash
make
```

### Clean build artifacts
```bash
make clean
```

### Run the simulation
```bash
./sim
```

### Build from scratch
```bash
make clean && make
```

## Project Architecture

This is a high-performance SystemC-based SoC architecture simulation system with a PCIe-style bidirectional design:

**HostSystem (TrafficGenerator + IndexAllocator) ⟷ Memory**
- **Downstream**: HostSystem → DownstreamDelay → Memory  
- **Upstream**: Memory → UpstreamDelay → HostSystem (for index release)

### Core Components

1. **HostSystem** (`src/host_system/host_system.cpp`)
   - Encapsulates TrafficGenerator and IndexAllocator as a unified module
   - Separate JSON configuration file (`config/host_system_config.json`)
   - Internal FIFO communication between TrafficGenerator and IndexAllocator
   - PCIe-style bidirectional interface (downstream out, upstream release_in)

2. **TrafficGenerator** (`src/base/traffic_generator.cpp`)
   - Generates READ/WRITE packets with percentage-based controls:
     - **Locality**: 0-100% (0=full random, 100=full sequential)
     - **Write Ratio**: 0-100% (0=all reads, 100=all writes)
   - Configurable address ranges (start_address, end_address, address_increment)
   - Separate JSON configuration file (`config/traffic_generator_config.json`)
   - Runtime debug control via JSON configuration

3. **IndexAllocator** (`include/base/index_allocator.h` - Template)
   - **Resource-limited allocation**: Configurable max_index pool with blocking when exhausted
   - **Automatic recycling**: Index deallocation when Memory processing completes
   - **Bidirectional operation**: Allocation on request path, deallocation on release path
   - Multiple allocation policies: Sequential, Round-Robin, Random, Pool-based
   - Template-based packet-type independence with custom index setter functions

4. **DelayLine** (`include/base/delay_line.h` - Template)
   - **PCIe-style bidirectional**: Separate downstream and upstream DelayLines
   - Header-only template component for any packet type
   - Configurable fixed delay simulation (nanosecond precision)
   - Runtime debug logging control

5. **Memory** (`include/base/memory.h` - Template)
   - Template-based scalable memory (256 to 65536+ entries)
   - **Release path**: Automatic packet return for index deallocation
   - Configurable data types and memory sizes via template parameters
   - Bounds checking with comprehensive error handling

### Packet System

- **BasePacket** (`include/packet/base_packet.h`): Abstract base class with virtual methods and set_attribute support
- **GenericPacket** (`include/packet/generic_packet.h`): Concrete implementation with index field and dynamic attribute setting
- Polymorphic design with virtual methods for extensibility and type safety

### Communication

- **PCIe-style bidirectional**: Separate downstream and upstream paths
- Components communicate via `sc_fifo<std::shared_ptr<BasePacket>>` 
- Smart pointers ensure automatic memory management and performance
- **Index lifecycle management**: Allocation → Processing → Release → Recycling

### Configuration System

- **Modular JSON Configuration**: Separate config files for better organization
  - `config/simulation_config.json`: Main simulation and DelayLine settings
  - `config/traffic_generator_config.json`: TrafficGenerator-specific settings
  - `config/host_system_config.json`: HostSystem and IndexAllocator settings
- **Per-component debug control**: Enable/disable logging for performance
- **Resource management**: Configurable index pool sizes and allocation policies
- **Performance optimization**: I/O bottleneck elimination achieving 25M+ tps

### Performance Features

- **Ultra-High Throughput**: 25,000,000+ transactions/second (PCIe-style architecture)
- **Index Resource Management**: Blocking allocation prevents resource exhaustion
- **Template Design**: Header-only templates for optimal performance
- **Smart Logging**: Runtime enable/disable to eliminate I/O bottlenecks
- **Memory Efficiency**: Shared pointer management with automatic index recycling

### Utilities

- **common_utils.h**: Packet logging and dynamic attribute access functions
- **json_config.h**: Simple JSON parser for runtime configuration
- **error_handling.h**: Centralized error codes and reporting
- Consistent timestamped logging with performance control

### SystemC Dependencies

- Requires SystemC 2.3.4+ installation at `/usr/local/systemc`
- Uses SystemC processes, modules, and timing mechanisms
- Simulation logs automatically timestamped and saved to `log/` directory
- Template-based design follows SystemC best practices

### File Organization

```
include/
├── base/           # Template components (delay lines, memory, traffic gen, index allocator)
├── common/         # Utilities (JSON config, error handling, common utils)
├── packet/         # Packet definitions and base classes
└── host_system/    # HostSystem encapsulation module

src/
├── base/           # Non-template implementations (traffic_generator.cpp only)
├── host_system/    # HostSystem implementation (host_system.cpp)
└── main.cpp        # Simulation entry point with PCIe-style architecture

config/
├── simulation_config.json         # Main simulation settings
├── traffic_generator_config.json  # TrafficGenerator-specific settings
└── host_system_config.json       # HostSystem and IndexAllocator settings

log/                # Timestamped simulation output logs
```

### JSON Configuration Examples

#### `config/traffic_generator_config.json`
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

#### `config/host_system_config.json`
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

#### `config/simulation_config.json`
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

### Performance Benchmarks

- **PCIe-Style Architecture (Debug Off)**: 25,000,000+ transactions/second
- **Index Resource Management (10 pool)**: 10,000,000+ transactions/second
- **Debug Enabled**: ~2,000-3,000 transactions/second  
- **100% Write Ratio**: 25,000,000+ transactions/second
- **Mixed Read/Write (50%)**: 15,000,000+ transactions/second
- **Legacy 5-Stage Pipeline**: 600,000+ transactions/second