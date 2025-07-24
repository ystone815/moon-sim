#!/usr/bin/env python3
"""
SystemC Web Monitoring Dashboard
Real-time performance monitoring for SystemC simulations
"""

from flask import Flask, render_template, jsonify, request
from flask_socketio import SocketIO, emit
import json
import os
import time
import threading
from datetime import datetime
from pathlib import Path
from simulation_controller import SimulationController

app = Flask(__name__)
app.config['SECRET_KEY'] = 'systemc_monitor_secret'
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='threading')

class MetricsMonitor:
    def __init__(self, metrics_file="metrics.json"):
        self.metrics_file = metrics_file
        self.last_modified = 0
        self.latest_metrics = {}
        self.history = []
        self.max_history = 1000  # Keep last 1000 data points
        
    def check_for_updates(self):
        """Check if metrics file has been updated"""
        try:
            if os.path.exists(self.metrics_file):
                current_modified = os.path.getmtime(self.metrics_file)
                if current_modified > self.last_modified:
                    self.last_modified = current_modified
                    self.load_metrics()
                    return True
        except Exception as e:
            print(f"Error checking file: {e}")
        return False
    
    def load_metrics(self):
        """Load metrics from JSON file"""
        try:
            with open(self.metrics_file, 'r') as f:
                content = f.read().replace('\\n', '\n')  # Fix escaped newlines
                self.latest_metrics = json.loads(content)
                
                # Add to history
                self.history.append(self.latest_metrics.copy())
                if len(self.history) > self.max_history:
                    self.history.pop(0)
                
                print(f"Loaded metrics at {datetime.now()}: {self.latest_metrics.get('simulation_time_ns', 0):.0f}ns")
                
        except Exception as e:
            print(f"Error loading metrics: {e}")
            self.latest_metrics = self.get_default_metrics()
    
    def get_default_metrics(self):
        """Return default metrics structure"""
        return {
            "timestamp": int(time.time() * 1000),
            "simulation_time_ns": 0,
            "metrics": {
                "performance": {
                    "packet_rate_pps": 0,
                    "bandwidth_mbps": 0,
                    "total_packets": 0
                },
                "caches": {},
                "dram": {},
                "components": {}
            }
        }
    
    def get_latest_metrics(self):
        """Get the latest metrics"""
        return self.latest_metrics or self.get_default_metrics()
    
    def get_history(self, limit=100):
        """Get metrics history"""
        return self.history[-limit:] if self.history else [self.get_default_metrics()]

# Global metrics monitor and simulation controller
monitor = MetricsMonitor()
sim_controller = SimulationController()

def background_monitor():
    """Background thread to monitor file changes"""
    while True:
        if monitor.check_for_updates():
            # Emit update to all connected clients
            socketio.emit('metrics_update', monitor.get_latest_metrics())
        time.sleep(1)  # Check every second

# Start background monitoring thread
monitor_thread = threading.Thread(target=background_monitor, daemon=True)
monitor_thread.start()

@app.route('/')
def dashboard():
    """Main dashboard page"""
    return render_template('dashboard.html')

@app.route('/api/metrics')
def get_metrics():
    """REST API endpoint for latest metrics"""
    return jsonify(monitor.get_latest_metrics())

@app.route('/api/history')
def get_history():
    """REST API endpoint for metrics history"""
    limit = request.args.get('limit', 100, type=int)
    return jsonify(monitor.get_history(limit))

@app.route('/api/status')
def get_status():
    """API endpoint for monitoring status"""
    return jsonify({
        "status": "running",
        "metrics_file": monitor.metrics_file,
        "file_exists": os.path.exists(monitor.metrics_file),
        "last_update": monitor.last_modified,
        "history_count": len(monitor.history)
    })

@socketio.on('connect')
def handle_connect():
    """Handle client connection"""
    print(f"Client connected: {request.sid}")
    emit('metrics_update', monitor.get_latest_metrics())

@socketio.on('disconnect')
def handle_disconnect():
    """Handle client disconnection"""
    print(f"Client disconnected: {request.sid}")

@socketio.on('request_history')
def handle_history_request(data):
    """Handle request for historical data"""
    limit = data.get('limit', 100)
    emit('history_data', monitor.get_history(limit))

# ========================================
# Simulation Control API Endpoints
# ========================================

@app.route('/api/simulation/configs')
def get_available_configs():
    """Get available configuration templates"""
    return jsonify(sim_controller.get_available_configs())

@app.route('/api/simulation/config/<config_name>')
def get_config(config_name):
    """Get specific configuration"""
    config = sim_controller.get_config(config_name)
    if config:
        return jsonify(config)
    else:
        return jsonify({"error": "Configuration not found"}), 404

@app.route('/api/simulation/config/<config_name>', methods=['POST'])
def save_config(config_name):
    """Save configuration"""
    config_data = request.get_json()
    if sim_controller.save_config(config_name, config_data):
        return jsonify({"success": True, "message": "Configuration saved"})
    else:
        return jsonify({"success": False, "message": "Failed to save configuration"}), 500

@app.route('/api/simulation/start', methods=['POST'])
def start_simulation():
    """Start simulation"""
    data = request.get_json() or {}
    sim_type = data.get('type', 'web_test')
    config_dir = data.get('config_dir', 'config/base')
    duration = data.get('duration', 30)
    
    result = sim_controller.start_simulation(sim_type, config_dir, duration)
    
    if result['success']:
        # Notify all clients
        socketio.emit('simulation_started', result)
    
    return jsonify(result)

@app.route('/api/simulation/stop', methods=['POST'])
def stop_simulation():
    """Stop simulation"""
    result = sim_controller.stop_simulation()
    
    if result['success']:
        # Notify all clients
        socketio.emit('simulation_stopped', result)
    
    return jsonify(result)

@app.route('/api/simulation/status')
def get_simulation_status():
    """Get simulation status"""
    return jsonify(sim_controller.get_simulation_status())

@app.route('/api/simulation/log')
def get_simulation_log():
    """Get simulation log"""
    lines = request.args.get('lines', 100, type=int)
    return jsonify(sim_controller.get_simulation_log(lines))

@app.route('/api/simulation/build', methods=['POST'])
def build_simulation():
    """Build simulation"""
    data = request.get_json() or {}
    target = data.get('target', 'web_test')
    
    result = sim_controller.build_simulation(target)
    return jsonify(result)

@app.route('/api/simulation/sweep', methods=['POST'])
def run_parameter_sweep():
    """Run parameter sweep"""
    sweep_config = request.get_json()
    result = sim_controller.run_parameter_sweep(sweep_config)
    
    if result['success']:
        # Notify all clients
        socketio.emit('sweep_started', result)
    
    return jsonify(result)

@app.route('/api/simulation/results')
def get_recent_results():
    """Get recent simulation results"""
    return jsonify(sim_controller.get_recent_results())

# WebSocket events for simulation control
@socketio.on('start_simulation')
def handle_start_simulation(data):
    """Handle simulation start request via WebSocket"""
    result = sim_controller.start_simulation(
        data.get('type', 'web_test'),
        data.get('config_dir', 'config/base'),
        data.get('duration', 30)
    )
    emit('simulation_result', result)

@socketio.on('stop_simulation')
def handle_stop_simulation():
    """Handle simulation stop request via WebSocket"""
    result = sim_controller.stop_simulation()
    emit('simulation_result', result)

if __name__ == '__main__':
    print("Starting SystemC Web Monitor Dashboard...")
    print(f"Monitoring file: {monitor.metrics_file}")
    print("Dashboard available at: http://localhost:5000")
    
    # Create templates directory if it doesn't exist
    os.makedirs('templates', exist_ok=True)
    os.makedirs('static', exist_ok=True)
    
    # Use threading instead of eventlet to avoid SSL issues
    # host='0.0.0.0' allows access from Windows browser when running in WSL
    socketio.run(app, host='0.0.0.0', port=5000, debug=False, allow_unsafe_werkzeug=True)