// SystemC Dashboard JavaScript
class SystemCDashboard {
    constructor() {
        this.socket = io();
        this.throughputData = [];
        this.bandwidthData = [];
        this.maxDataPoints = 50;
        this.simulationStatus = 'stopped';
        
        this.initializeCharts();
        this.setupSocketHandlers();
        this.startHeartbeat();
        this.loadAvailableConfigs();
    }
    
    initializeCharts() {
        // Throughput Chart
        const throughputCtx = document.getElementById('throughputChart').getContext('2d');
        this.throughputChart = new Chart(throughputCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Packets/sec',
                    data: [],
                    borderColor: '#667eea',
                    backgroundColor: 'rgba(102, 126, 234, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Packets/sec'
                        },
                        beginAtZero: true
                    }
                },
                plugins: {
                    legend: {
                        display: false
                    }
                },
                elements: {
                    point: {
                        radius: 2
                    }
                }
            }
        });
        
        // Bandwidth Chart
        const bandwidthCtx = document.getElementById('bandwidthChart').getContext('2d');
        this.bandwidthChart = new Chart(bandwidthCtx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Mbps',
                    data: [],
                    borderColor: '#764ba2',
                    backgroundColor: 'rgba(118, 75, 162, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Mbps'
                        },
                        beginAtZero: true
                    }
                },
                plugins: {
                    legend: {
                        display: false
                    }
                },
                elements: {
                    point: {
                        radius: 2
                    }
                }
            }
        });
    }
    
    setupSocketHandlers() {
        this.socket.on('connect', () => {
            console.log('Connected to SystemC monitor');
            this.updateConnectionStatus(true);
        });
        
        this.socket.on('disconnect', () => {
            console.log('Disconnected from SystemC monitor');
            this.updateConnectionStatus(false);
        });
        
        this.socket.on('metrics_update', (data) => {
            this.updateDashboard(data);
        });
        
        this.socket.on('history_data', (data) => {
            this.loadHistoricalData(data);
        });
        
        // Simulation control events
        this.socket.on('simulation_started', (data) => {
            this.updateSimulationStatus('running', data.pid);
            this.addLogEntry(`Simulation started with PID ${data.pid}`);
        });
        
        this.socket.on('simulation_stopped', (data) => {
            this.updateSimulationStatus('stopped');
            this.addLogEntry('Simulation stopped');
        });
        
        this.socket.on('sweep_started', (data) => {
            this.updateSimulationStatus('running', data.pid);
            this.addLogEntry(`Parameter sweep started with PID ${data.pid}`);
        });
    }
    
    updateConnectionStatus(connected) {
        const statusIndicator = document.getElementById('connectionStatus');
        const statusText = document.getElementById('statusText');
        
        if (connected) {
            statusIndicator.className = 'status-indicator status-connected';
            statusText.textContent = 'Connected';
        } else {
            statusIndicator.className = 'status-indicator status-disconnected';
            statusText.textContent = 'Disconnected';
        }
    }
    
    updateDashboard(data) {
        if (!data || !data.metrics) {
            console.log('Invalid data received:', data);
            return;
        }
        
        const metrics = data.metrics;
        
        // Update simulation time
        const simTimeNs = data.simulation_time_ns || 0;
        document.getElementById('simTime').textContent = (simTimeNs / 1e9).toFixed(3);
        
        // Update metric cards
        if (metrics.performance) {
            const perf = metrics.performance;
            document.getElementById('throughput').textContent = Math.round(perf.packet_rate_pps || 0);
            document.getElementById('bandwidth').textContent = (perf.bandwidth_mbps || 0).toFixed(2);
        }
        
        // Update cache metrics
        this.updateCacheMetrics(metrics.caches);
        
        // Update DRAM metrics
        this.updateDramMetrics(metrics.dram);
        
        // Update charts
        this.updateCharts(data);
    }
    
    updateCacheMetrics(caches) {
        let totalHitRate = 0;
        let cacheCount = 0;
        let totalAccesses = 0;
        let totalHits = 0;
        let totalMisses = 0;
        let totalEvictions = 0;
        
        for (const [name, cache] of Object.entries(caches || {})) {
            if (cache.hit_rate !== undefined) {
                totalHitRate += cache.hit_rate;
                cacheCount++;
            }
            totalAccesses += cache.hits + cache.misses || 0;
            totalHits += cache.hits || 0;
            totalMisses += cache.misses || 0;
            totalEvictions += cache.evictions || 0;
        }
        
        const avgHitRate = cacheCount > 0 ? totalHitRate / cacheCount : 0;
        document.getElementById('cacheHitRate').textContent = avgHitRate.toFixed(1);
        document.getElementById('cacheAccesses').textContent = totalAccesses;
        document.getElementById('cacheHits').textContent = totalHits;
        document.getElementById('cacheMisses').textContent = totalMisses;
        document.getElementById('cacheEvictions').textContent = totalEvictions;
    }
    
    updateDramMetrics(dramControllers) {
        let totalRequests = 0;
        let totalRowHits = 0;
        let totalConflicts = 0;
        let avgLatency = 0;
        let dramCount = 0;
        
        for (const [name, dram] of Object.entries(dramControllers || {})) {
            totalRequests += dram.total_requests || 0;
            totalRowHits += dram.row_hits || 0;
            totalConflicts += dram.bank_conflicts || 0;
            
            if (dram.avg_read_latency_ns) {
                avgLatency += dram.avg_read_latency_ns;
                dramCount++;
            }
        }
        
        const utilization = totalRequests > 0 ? (totalRowHits / totalRequests * 100) : 0;
        document.getElementById('dramUtil').textContent = utilization.toFixed(1);
        document.getElementById('dramRequests').textContent = totalRequests;
        document.getElementById('dramRowHits').textContent = totalRowHits;
        document.getElementById('dramConflicts').textContent = totalConflicts;
        
        const finalAvgLatency = dramCount > 0 ? avgLatency / dramCount : 0;
        document.getElementById('dramLatency').textContent = finalAvgLatency.toFixed(1) + ' ns';
    }
    
    updateCharts(data) {
        if (!data.metrics || !data.metrics.performance) return;
        
        const performance = data.metrics.performance;
        const timestamp = new Date().toLocaleTimeString();
        
        // Add new data points
        this.throughputData.push({
            x: timestamp,
            y: performance.packet_rate_pps || 0
        });
        
        this.bandwidthData.push({
            x: timestamp,
            y: performance.bandwidth_mbps || 0
        });
        
        // Keep only recent data points
        if (this.throughputData.length > this.maxDataPoints) {
            this.throughputData.shift();
        }
        if (this.bandwidthData.length > this.maxDataPoints) {
            this.bandwidthData.shift();
        }
        
        // Update throughput chart
        this.throughputChart.data.labels = this.throughputData.map(d => d.x);
        this.throughputChart.data.datasets[0].data = this.throughputData.map(d => d.y);
        this.throughputChart.update('none');
        
        // Update bandwidth chart
        this.bandwidthChart.data.labels = this.bandwidthData.map(d => d.x);
        this.bandwidthChart.data.datasets[0].data = this.bandwidthData.map(d => d.y);
        this.bandwidthChart.update('none');
    }
    
    loadHistoricalData(history) {
        console.log('Loading historical data:', history.length, 'points');
        
        // Clear existing data
        this.throughputData = [];
        this.bandwidthData = [];
        
        // Load historical data (sample recent data to avoid overload)
        const sampleRate = Math.max(1, Math.floor(history.length / this.maxDataPoints));
        
        for (let i = 0; i < history.length; i += sampleRate) {
            const data = history[i];
            if (data && data.metrics && data.metrics.performance) {
                const timestamp = new Date(data.timestamp).toLocaleTimeString();
                
                this.throughputData.push({
                    x: timestamp,
                    y: data.metrics.performance.packet_rate_pps || 0
                });
                
                this.bandwidthData.push({
                    x: timestamp,
                    y: data.metrics.performance.bandwidth_mbps || 0
                });
            }
        }
        
        // Update charts
        this.throughputChart.data.labels = this.throughputData.map(d => d.x);
        this.throughputChart.data.datasets[0].data = this.throughputData.map(d => d.y);
        this.throughputChart.update();
        
        this.bandwidthChart.data.labels = this.bandwidthData.map(d => d.x);
        this.bandwidthChart.data.datasets[0].data = this.bandwidthData.map(d => d.y);
        this.bandwidthChart.update();
    }
    
    startHeartbeat() {
        // Request historical data on load
        this.socket.emit('request_history', { limit: 100 });
        
        // Periodic status updates
        setInterval(() => {
            fetch('/api/status')
                .then(response => response.json())
                .then(data => {
                    console.log('Status:', data);
                })
                .catch(error => {
                    console.error('Status check failed:', error);
                });
        }, 30000); // Every 30 seconds
    }
    
    // ========================================
    // Simulation Control Methods
    // ========================================
    
    loadAvailableConfigs() {
        fetch('/api/simulation/configs')
            .then(response => response.json())
            .then(data => {
                console.log('Available configs:', data);
                this.populateConfigTemplates(data.workloads || []);
            })
            .catch(error => {
                console.error('Error loading configs:', error);
            });
    }
    
    populateConfigTemplates(workloads) {
        const select = document.getElementById('configTemplate');
        
        // Add workload options
        workloads.forEach(workload => {
            const option = document.createElement('option');
            option.value = workload;
            option.textContent = workload.replace('traffic_', '').replace('_', ' ').toUpperCase();
            select.appendChild(option);
        });
    }
    
    updateSimulationStatus(status, pid = null) {
        this.simulationStatus = status;
        const statusElement = document.getElementById('simStatus');
        const pidElement = document.getElementById('simPid');
        const startBtn = document.getElementById('startBtn');
        const stopBtn = document.getElementById('stopBtn');
        const buildBtn = document.getElementById('buildBtn');
        
        statusElement.className = `sim-status ${status}`;
        statusElement.textContent = status;
        
        pidElement.textContent = pid || '-';
        
        // Update button states
        if (status === 'running') {
            startBtn.disabled = true;
            stopBtn.disabled = false;
            buildBtn.disabled = true;
        } else {
            startBtn.disabled = false;
            stopBtn.disabled = true;
            buildBtn.disabled = false;
        }
    }
    
    addLogEntry(message) {
        const logContainer = document.getElementById('simulationLog');
        const timestamp = new Date().toLocaleTimeString();
        const logLine = document.createElement('div');
        logLine.className = 'log-line';
        logLine.textContent = `[${timestamp}] ${message}`;
        
        logContainer.appendChild(logLine);
        logContainer.scrollTop = logContainer.scrollHeight;
        
        // Keep log size manageable
        const logLines = logContainer.querySelectorAll('.log-line');
        if (logLines.length > 100) {
            logLines[0].remove();
        }
    }
    
    async startSimulation() {
        if (this.simulationStatus === 'running') {
            alert('Simulation is already running');
            return;
        }
        
        const simType = document.getElementById('simType').value;
        const configTemplate = document.getElementById('configTemplate').value;
        const numTransactions = document.getElementById('numTransactions').value;
        const writePercentage = document.getElementById('writePercentage').value;
        
        this.addLogEntry(`Starting ${simType} simulation...`);
        
        try {
            // First update configuration if needed
            if (configTemplate !== 'base') {
                await this.updateConfiguration(configTemplate, {
                    num_transactions: parseInt(numTransactions),
                    write_percentage: parseInt(writePercentage)
                });
            }
            
            const response = await fetch('/api/simulation/start', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    type: simType,
                    config_dir: configTemplate === 'base' ? 'config/base' : 'config/runtime',
                    duration: 60
                })
            });
            
            const result = await response.json();
            
            if (result.success) {
                this.updateSimulationStatus('running', result.pid);
                this.addLogEntry(result.message);
                this.startLogPolling();
            } else {
                this.addLogEntry(`Failed to start: ${result.message}`);
                alert(`Failed to start simulation: ${result.message}`);
            }
        } catch (error) {
            console.error('Error starting simulation:', error);
            this.addLogEntry(`Error: ${error.message}`);
            alert(`Error starting simulation: ${error.message}`);
        }
    }
    
    async stopSimulation() {
        if (this.simulationStatus !== 'running') {
            alert('No simulation is running');
            return;
        }
        
        this.addLogEntry('Stopping simulation...');
        
        try {
            const response = await fetch('/api/simulation/stop', {
                method: 'POST'
            });
            
            const result = await response.json();
            
            if (result.success) {
                this.updateSimulationStatus('stopped');
                this.addLogEntry(result.message);
                this.stopLogPolling();
            } else {
                this.addLogEntry(`Failed to stop: ${result.message}`);
                alert(`Failed to stop simulation: ${result.message}`);
            }
        } catch (error) {
            console.error('Error stopping simulation:', error);
            this.addLogEntry(`Error: ${error.message}`);
            alert(`Error stopping simulation: ${error.message}`);
        }
    }
    
    async buildSimulation() {
        const simType = document.getElementById('simType').value;
        this.addLogEntry(`Building ${simType}...`);
        
        try {
            const response = await fetch('/api/simulation/build', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    target: simType
                })
            });
            
            const result = await response.json();
            
            if (result.success) {
                this.addLogEntry('Build completed successfully');
            } else {
                this.addLogEntry(`Build failed: ${result.message}`);
                if (result.stderr) {
                    this.addLogEntry(`Error: ${result.stderr}`);
                }
                alert(`Build failed: ${result.message}`);
            }
        } catch (error) {
            console.error('Error building simulation:', error);
            this.addLogEntry(`Build error: ${error.message}`);
            alert(`Error building simulation: ${error.message}`);
        }
    }
    
    async updateConfiguration(templateName, overrides) {
        try {
            // Get base configuration
            const configResponse = await fetch(`/api/simulation/config/${templateName}`);
            const config = await configResponse.json();
            
            // Apply overrides
            if (config.traffic_generator && overrides) {
                Object.assign(config.traffic_generator, overrides);
            }
            
            // Save updated configuration
            const saveResponse = await fetch(`/api/simulation/config/runtime_config`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(config)
            });
            
            const result = await saveResponse.json();
            if (result.success) {
                this.addLogEntry('Configuration updated');
            } else {
                this.addLogEntry('Failed to update configuration');
            }
        } catch (error) {
            console.error('Error updating configuration:', error);
            this.addLogEntry(`Configuration error: ${error.message}`);
        }
    }
    
    startLogPolling() {
        this.logPollInterval = setInterval(() => {
            this.pollSimulationLog();
        }, 2000); // Poll every 2 seconds
    }
    
    stopLogPolling() {
        if (this.logPollInterval) {
            clearInterval(this.logPollInterval);
            this.logPollInterval = null;
        }
    }
    
    async pollSimulationLog() {
        try {
            const response = await fetch('/api/simulation/log?lines=20');
            const result = await response.json();
            
            if (result.log_lines && result.log_lines.length > 0) {
                const logContainer = document.getElementById('simulationLog');
                
                // Clear existing log
                logContainer.innerHTML = '';
                
                // Add new log lines
                result.log_lines.forEach(line => {
                    const logLine = document.createElement('div');
                    logLine.className = 'log-line';
                    logLine.textContent = line;
                    logContainer.appendChild(logLine);
                });
                
                logContainer.scrollTop = logContainer.scrollHeight;
            }
        } catch (error) {
            console.error('Error polling log:', error);
        }
    }
}

// Global functions for button onclick events
function startSimulation() {
    window.dashboard.startSimulation();
}

function stopSimulation() {
    window.dashboard.stopSimulation();
}

function buildSimulation() {
    window.dashboard.buildSimulation();
}

// Initialize dashboard when page loads
document.addEventListener('DOMContentLoaded', () => {
    window.dashboard = new SystemCDashboard();
});

// Handle page visibility changes to optimize performance
document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
        console.log('Page hidden - reducing update frequency');
    } else {
        console.log('Page visible - resuming normal updates');
    }
});