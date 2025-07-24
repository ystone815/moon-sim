# SystemC Web-based Performance Monitor & Simulation Controller

A comprehensive web-based dashboard for real-time SystemC simulation monitoring and control. This system provides both performance visualization and complete simulation lifecycle management through a modern web interface.

## 🚀 Features

### Real-time Performance Monitoring
- **Live Metrics Dashboard**: Real-time throughput, bandwidth, cache hit rates, and DRAM statistics
- **Interactive Charts**: Dynamic Chart.js visualizations updated every 2 seconds
- **WebSocket Communication**: Low-latency real-time data streaming
- **Historical Data**: Maintains 1000 data points of simulation history

### Web-based Simulation Control
- **Simulation Management**: Start, stop, and build simulations directly from the browser
- **Configuration Templates**: Pre-configured workload templates (database, web server, ML inference, etc.)
- **Real-time Logging**: Live simulation output displayed in the web interface
- **Process Monitoring**: PID tracking and resource usage monitoring

### REST API
- Complete RESTful API for programmatic control
- Configuration management endpoints
- Simulation lifecycle management
- Status monitoring and logging

## 🛠️ Installation

### 1. Install Dependencies
```bash
cd web_monitor
pip install -r requirements.txt
```

### 2. Build SystemC Web Test Executable
```bash
# Set SystemC environment (add to ~/.bashrc for permanent setting)
export SYSTEMC_HOME=/tmp/systemc-install/usr/local/systemc-cxx11

# Build the web monitoring test executable
make -f Makefile_web clean && make -f Makefile_web
```

## 🎯 Quick Start

### Method 1: Manual Setup (Recommended for Learning)

1. **Start the Web Server**
   ```bash
   cd web_monitor
   python3 app.py
   ```

2. **Run SystemC Simulation**
   ```bash
   # In another terminal
   export SYSTEMC_HOME=/tmp/systemc-install/usr/local/systemc-cxx11
   ./web_test config/base
   ```

3. **Open Web Dashboard**
   - Navigate to `http://localhost:5000`
   - Watch real-time performance metrics

### Method 2: Web-based Control (Full Featured)

1. **Start Web Server**
   ```bash
   cd web_monitor
   python3 app.py
   ```

2. **Open Dashboard and Control**
   - Navigate to `http://localhost:5000`
   - Click **Build** to compile simulation
   - Select configuration template
   - Click **Start** to run simulation
   - Monitor real-time metrics and logs

## 📊 Dashboard Overview

### Control Panel
- **Simulation Status**: Shows current state (stopped/running/completed/error)
- **Build Button**: Compiles the selected simulation type
- **Start/Stop Buttons**: Controls simulation execution
- **Configuration Selection**: Choose from pre-configured workload templates
- **Parameter Tuning**: Adjust transaction count and write percentage
- **Real-time Log**: Live simulation output with auto-scrolling

### Performance Metrics Cards
- **Throughput**: Packets per second (real-time)
- **Bandwidth**: Mbps utilization
- **Cache Hit Rate**: L1 cache effectiveness percentage
- **DRAM Utilization**: Memory controller efficiency

### Interactive Charts
- **Throughput Over Time**: Line chart showing packet rate trends
- **Bandwidth Over Time**: Network utilization visualization
- Both charts maintain 50 data points with smooth animations

### Detailed Statistics
- **Cache Statistics**: Total accesses, hits, misses, evictions
- **DRAM Statistics**: Total requests, row hits, bank conflicts, average latency

## 🏗️ Architecture

```
SystemC Simulation → JSON Metrics → Python Flask → WebSocket → Browser Dashboard
```

### Components
1. **WebProfiler (C++)**: Collects SystemC performance data
2. **Flask Server (Python)**: REST API and WebSocket server  
3. **Dashboard (HTML/JS)**: Real-time visualization with Chart.js

### Data Flow
1. SystemC components generate performance metrics
2. WebProfiler writes JSON data to `metrics.json`
3. Python server monitors file changes
4. WebSocket pushes updates to browser
5. JavaScript updates charts and metrics in real-time

## 📁 File Structure

```
web_monitor/
├── app.py                     # Flask web server with API endpoints
├── simulation_controller.py   # SystemC simulation process management
├── requirements.txt           # Python dependencies
├── templates/
│   └── dashboard.html        # Main dashboard HTML template
├── static/
│   └── dashboard.js          # Frontend JavaScript logic
└── README.md                 # This documentation

../
├── Makefile_web              # Build system for web monitoring
├── src/main_web_test.cpp     # SystemC test bench with WebProfiler
├── include/base/web_profiler.h # Real-time metrics collection
└── config/                   # Configuration templates and settings
```

## 🛠️ Configuration

### SystemC Side
- Metrics update interval: 1-2 seconds
- JSON output file: `web_monitor/metrics.json`
- Debug logging: Configurable

### Web Server
- Port: 5000 (configurable in app.py)
- WebSocket: Real-time updates
- CORS: Enabled for development

### Dashboard
- Chart data points: 50 recent values
- Update frequency: 1 second
- Responsive design for mobile/desktop

## 🔧 Configuration Templates

The system includes several pre-configured workload templates:

### Available Templates
- **Base Configuration**: Default SystemC simulation settings
- **Database Workload**: High read/write ratio with locality patterns
- **Web Server Workload**: Mixed request patterns with caching
- **ML Inference**: Batch processing with sequential access patterns
- **IoT Sensors**: Low-rate, scattered access patterns
- **Streaming Workload**: High-throughput sequential access

## 🌐 API Reference

### Monitoring Endpoints
- `GET /api/status` - Monitor system status
- `GET /api/metrics` - Get latest performance metrics
- `GET /api/history?limit=N` - Get historical data

### Simulation Control
- `POST /api/simulation/start` - Start simulation
- `POST /api/simulation/stop` - Stop running simulation
- `GET /api/simulation/status` - Get simulation status
- `POST /api/simulation/build` - Build simulation executable
- `GET /api/simulation/configs` - List available configurations

## 🔍 Troubleshooting

### Common Issues

1. **"SYSTEMC_HOME is not set" Error**
   ```bash
   export SYSTEMC_HOME=/tmp/systemc-install/usr/local/systemc-cxx11
   echo 'export SYSTEMC_HOME=/tmp/systemc-install/usr/local/systemc-cxx11' >> ~/.bashrc
   ```

2. **Web Server Won't Start**
   ```bash
   # Check if port 5000 is already in use
   lsof -i :5000
   pkill -f "python3 app.py"
   ```

3. **WSL Network Access Issues**
   ```bash
   # Find WSL IP address
   hostname -I
   # Access via http://WSL_IP:5000 instead of localhost:5000
   # Example: http://172.30.151.67:5000
   ```

4. **No Metrics Displayed**
   - Ensure SystemC simulation is running
   - Check `web_monitor/metrics.json` file is being created
   - Verify WebSocket connection in browser developer tools

5. **Build Failures**
   - Verify SystemC installation and SYSTEMC_HOME
   - Check compiler version (g++ with C++11 support required)

## 📈 Performance Tips

- Keep maximum data points reasonable (50-100)
- Use appropriate update intervals (1-2 seconds)
- Monitor browser memory usage for long simulations
- Consider data compression for high-frequency updates

## 🎯 Use Cases

- **Development**: Real-time debugging of SystemC performance
- **Optimization**: Identify bottlenecks during simulation
- **Validation**: Monitor cache hit rates and memory efficiency  
- **Demos**: Live visualization of system behavior
- **CI/CD**: Automated performance monitoring