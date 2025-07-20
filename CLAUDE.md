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

This is a SystemC-based packet simulation system with a modular architecture following a pipeline pattern:

**Traffic Generator → Delay Line → Delay Line Databyte → Memory**

### Core Components

1. **TrafficGenerator** (`src/host_system/traffic_generator.cpp`)
   - Generates READ/WRITE packets with configurable patterns (RANDOM/SEQUENTIAL)
   - Configurable parameters: interval, locality type, read/write operations, databyte values, transaction count
   - Outputs packets to FIFO for processing

2. **DelayLine** (`src/base/delay_line.cpp`)
   - Introduces configurable delay to packet processing
   - Processes BasePacket objects with specified time delay
   - Acts as a pipeline stage between traffic generator and databyte delay line

3. **DelayLineDatabyte** (`src/base/delay_line_databyte.cpp`)
   - Specialized delay line that processes packets based on their databyte attribute
   - Configurable width and uses packet attribute for delay calculation
   - Second stage in the delay pipeline

4. **Memory** (`src/base/memory.cpp`)
   - Terminal component that stores packet data
   - 256-entry array storing both data and databyte values
   - Processes READ/WRITE commands from incoming packets

### Packet System

- **BasePacket** (`include/packet/base_packet.h`): Abstract base class with virtual methods for attribute access, printing, and tracing
- **DataPacket** (`include/packet/packet_base.h`): Concrete implementation containing command, address, data, and databyte fields
- Packets use polymorphic design with virtual methods for extensibility

### Communication

- Components communicate via `sc_fifo<std::unique_ptr<BasePacket>>` 
- Three FIFO channels connect the four main components in sequence
- Smart pointers ensure proper memory management of packet objects

### Utilities

- **common_utils.h**: Provides packet logging and dynamic attribute access functions
- Consistent timestamped logging across all modules
- Dynamic attribute getter supporting "databyte", "data", and "address"

### SystemC Dependencies

- Requires SystemC installation at `/usr/local/systemc`
- Uses SystemC processes, modules, and timing mechanisms
- Simulation logs are automatically timestamped and saved to `log/` directory

### File Organization

```
include/
├── base/           # Core simulation components (delay lines, memory)
├── host_system/    # Traffic generation
├── packet/         # Packet definitions and base classes
└── common/         # Shared utilities

src/                # Implementation files mirroring include structure
└── main.cpp        # Simulation entry point and component instantiation
```