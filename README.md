# SystemC SoC Architecture Simulator

A high-performance, production-ready SystemC-based simulation framework for System-on-Chip (SoC) architecture modeling and analysis.

## 🚀 Features

### Core Architecture
- **Modular Pipeline Design**: TrafficGenerator → DelayLine → DelayLineDatabyte → Memory
- **Smart Memory Management**: Full `std::shared_ptr` integration for automatic memory safety
- **Type-Safe Design**: Modern C++ with `enum class` and const correctness
- **Performance Optimized**: Eliminated dynamic_cast, optimized random generation

### Components

#### TrafficGenerator
- Configurable READ/WRITE packet generation
- Support for RANDOM and SEQUENTIAL access patterns
- Deterministic C++11 random number generation
- Customizable transaction parameters

#### DelayLine
- Configurable fixed delay simulation
- Pipeline stage modeling
- Packet flow control

#### DelayLineDatabyte  
- Variable delay based on packet databyte attribute
- Configurable data path width simulation
- Clock-cycle accurate timing

#### Memory
- 256-entry memory array with bounds checking
- READ/WRITE operation support
- Safe array access with `std::array`

### Advanced Features
- **Centralized Error Handling**: Standardized error codes and reporting
- **Polymorphic Packet Design**: Clean virtual interface pattern
- **SystemC Best Practices**: Proper thread usage and timing
- **Production-Ready Code**: Memory safe, type safe, performance optimized

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

## 📊 Architecture Overview

```
┌─────────────────┐    ┌───────────┐    ┌──────────────────┐    ┌────────┐
│ TrafficGenerator│───▶│ DelayLine │───▶│ DelayLineDatabyte│───▶│ Memory │
└─────────────────┘    └───────────┘    └──────────────────┘    └────────┘
         │                     │                   │                │
    Generates packets     Fixed delay      Variable delay      256-entry
    READ/WRITE ops       simulation        based on data        storage
    RANDOM/SEQUENTIAL                      path width
```

## 🔧 Configuration

### Traffic Generator Parameters
- `interval`: Time between packet generations
- `locality`: RANDOM or SEQUENTIAL access pattern
- `do_reads/do_writes`: Enable READ/WRITE operations
- `databyte_value`: Custom databyte value for packets
- `num_transactions`: Total number of transactions

### Example Configuration
```cpp
TrafficGenerator traffic_generator(
    "traffic_generator", 
    sc_time(10, SC_NS),           // 10ns interval
    LocalityType::RANDOM,         // Random access
    true,                         // Enable reads
    true,                         // Enable writes
    64,                           // Databyte value
    10                           // 10 transactions
);
```

## 📈 Performance Features

- **Memory Safe**: Zero memory leaks with smart pointers
- **Type Safe**: Compile-time error prevention
- **Exception Safe**: Robust error handling
- **Cache Friendly**: Optimized data structures
- **Deterministic**: Reproducible simulation results

## 🧪 Simulation Output

Simulation logs are automatically timestamped and saved to `log/` directory with detailed packet tracing and component status information.

## 🎯 Use Cases

- SoC performance modeling and analysis
- Memory subsystem simulation
- Bus architecture evaluation
- Latency and throughput analysis
- Hardware/software co-design validation

## 🔬 Code Quality

This simulator follows modern C++ best practices:
- ✅ Smart pointer memory management
- ✅ Const correctness throughout
- ✅ Type-safe enum classes
- ✅ Centralized error handling
- ✅ SystemC best practices
- ✅ Performance optimizations
- ✅ Comprehensive bounds checking

## 📝 License

[Add your license here]

## 🤝 Contributing

[Add contribution guidelines here]

---
*Built with SystemC for high-performance SoC simulation*