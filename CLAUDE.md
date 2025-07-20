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

This is a high-performance SystemC-based SoC architecture simulation system with a modular template-based design:

**TrafficGenerator → DelayLine × 5 → Memory**

### Core Components

1. **TrafficGenerator** (`src/base/traffic_generator.cpp`)
   - Generates READ/WRITE packets with percentage-based locality control (0-100%)
   - Configurable address ranges (start_address, end_address, address_increment)
   - Mixed access patterns: 0=full random, 100=full sequential, 50=mixed
   - Parameters: interval, locality_percentage, read/write operations, databyte values, transaction count
   - Runtime debug control via JSON configuration

2. **DelayLine** (`include/base/delay_line.h` - Template)
   - Header-only template component for any packet type
   - Configurable fixed delay simulation (nanosecond precision)
   - Runtime debug logging control
   - Multiple instances create pipeline chains

3. **Memory** (`include/base/memory.h` - Template)
   - Template-based scalable memory (256 to 65536+ entries)
   - Configurable data types and memory sizes via template parameters
   - Bounds checking with comprehensive error handling
   - Runtime debug control for performance optimization

4. **IndexAllocator** (`include/base/index_allocator.h` - Template)
   - Multiple allocation policies: Sequential, Round-Robin, Random, Pool-based
   - Template-based packet-type independence
   - Custom index setter functions for flexibility
   - Statistics tracking and debugging support

### Packet System

- **BasePacket** (`include/packet/base_packet.h`): Abstract base class with virtual methods and set_attribute support
- **GenericPacket** (`include/packet/generic_packet.h`): Concrete implementation with index field and dynamic attribute setting
- Polymorphic design with virtual methods for extensibility and type safety

### Communication

- Components communicate via `sc_fifo<std::shared_ptr<BasePacket>>` 
- Smart pointers ensure automatic memory management and performance
- FIFO channels connect components in configurable pipeline sequences

### Configuration System

- **JSON Runtime Configuration** (`config/simulation_config.json`): No recompilation needed
- **Per-component debug control**: Enable/disable logging for performance
- **Scalable parameters**: Address ranges, memory sizes, locality patterns
- **Performance optimization**: I/O bottleneck elimination

### Performance Features

- **High Throughput**: 600,000+ transactions/second (debug disabled)
- **Template Design**: Header-only templates for optimal performance
- **Smart Logging**: Runtime enable/disable to eliminate I/O bottlenecks
- **Memory Efficiency**: Shared pointer management with minimal overhead

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
└── host_system/    # (Empty - moved to base for generality)

src/
├── base/           # Non-template implementations (traffic_generator.cpp only)
└── main.cpp        # Simulation entry point with JSON configuration

config/
└── simulation_config.json  # Runtime configuration file

log/                # Timestamped simulation output logs
```

### JSON Configuration Example

```json
{
  "simulation": {
    "num_transactions": 100000,
    "traffic_generator": {
      "interval_ns": 10,
      "locality_percentage": 75,
      "start_address": 0,
      "end_address": 65535,
      "address_increment": 64,
      "debug_enable": false
    },
    "delay_lines": {
      "count": 5,
      "delay_ns": 10,
      "debug_enable": false
    },
    "memory": {
      "debug_enable": false
    }
  }
}
```

### Performance Benchmarks

- **Debug Disabled**: 600,000+ transactions/second
- **Debug Enabled**: ~2,000-3,000 transactions/second  
- **0% Locality (Random)**: 561,797 tps
- **100% Locality (Sequential)**: 606,060 tps
- **75% Locality (Mixed)**: 578,034 tps