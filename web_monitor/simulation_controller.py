#!/usr/bin/env python3
"""
SystemC Simulation Controller
Manages simulation execution, configuration, and process control
"""

import json
import os
import subprocess
import threading
import time
import signal
import psutil
from pathlib import Path
from datetime import datetime

class SimulationController:
    def __init__(self, base_dir="../"):
        self.base_dir = Path(base_dir).resolve()
        self.current_process = None
        self.simulation_status = "stopped"
        self.simulation_log = []
        self.config_templates = {}
        self.load_config_templates()
        
    def load_config_templates(self):
        """Load available configuration templates"""
        templates_dir = self.base_dir / "config" / "templates"
        if templates_dir.exists():
            for config_file in templates_dir.glob("*.json"):
                try:
                    with open(config_file, 'r') as f:
                        self.config_templates[config_file.stem] = json.load(f)
                except Exception as e:
                    print(f"Error loading template {config_file}: {e}")
        
        # Load base configs
        base_dir = self.base_dir / "config" / "base"
        if base_dir.exists():
            for config_file in base_dir.glob("*.json"):
                try:
                    with open(config_file, 'r') as f:
                        self.config_templates[f"base_{config_file.stem}"] = json.load(f)
                except Exception as e:
                    print(f"Error loading base config {config_file}: {e}")
    
    def get_available_configs(self):
        """Get list of available configuration templates"""
        return {
            "templates": list(self.config_templates.keys()),
            "workloads": [
                "traffic_database",
                "traffic_webserver", 
                "traffic_ml_inference",
                "traffic_iot_sensors",
                "traffic_streaming"
            ],
            "test_types": [
                "cache_test",
                "web_test", 
                "sweep_test"
            ]
        }
    
    def get_config(self, config_name):
        """Get specific configuration"""
        return self.config_templates.get(config_name, {})
    
    def save_config(self, config_name, config_data):
        """Save configuration to file"""
        try:
            config_path = self.base_dir / "config" / "runtime" / f"{config_name}.json"
            config_path.parent.mkdir(exist_ok=True)
            
            with open(config_path, 'w') as f:
                json.dump(config_data, f, indent=2)
            
            # Update templates cache
            self.config_templates[f"runtime_{config_name}"] = config_data
            return True
        except Exception as e:
            print(f"Error saving config {config_name}: {e}")
            return False
    
    def start_simulation(self, sim_type="web_test", config_dir="config/base", duration=30):
        """Start SystemC simulation"""
        if self.simulation_status == "running":
            return {"success": False, "message": "Simulation already running"}
        
        try:
            # Determine executable and arguments
            if sim_type == "cache_test":
                executable = "./cache_test"
            elif sim_type == "web_test":
                executable = "./web_test"
            else:
                executable = "./sim"
            
            # Prepare command
            cmd = [executable, config_dir]
            
            # Clear previous metrics
            metrics_file = Path("metrics.json")
            if metrics_file.exists():
                metrics_file.unlink()
            
            # Start process
            self.current_process = subprocess.Popen(
                cmd,
                cwd=self.base_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1
            )
            
            self.simulation_status = "running"
            self.simulation_log = []
            
            # Start log monitoring thread
            log_thread = threading.Thread(target=self._monitor_simulation_output, daemon=True)
            log_thread.start()
            
            # Start process monitoring thread
            monitor_thread = threading.Thread(target=self._monitor_simulation_process, daemon=True)
            monitor_thread.start()
            
            return {
                "success": True, 
                "message": f"Simulation started with PID {self.current_process.pid}",
                "pid": self.current_process.pid,
                "command": " ".join(cmd)
            }
            
        except Exception as e:
            self.simulation_status = "error"
            return {"success": False, "message": f"Failed to start simulation: {str(e)}"}
    
    def stop_simulation(self):
        """Stop running simulation"""
        if self.current_process and self.simulation_status == "running":
            try:
                # Try graceful termination first
                self.current_process.terminate()
                
                # Wait up to 5 seconds for graceful shutdown
                try:
                    self.current_process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    # Force kill if graceful termination fails
                    self.current_process.kill()
                    self.current_process.wait()
                
                self.simulation_status = "stopped"
                return {"success": True, "message": "Simulation stopped"}
                
            except Exception as e:
                return {"success": False, "message": f"Error stopping simulation: {str(e)}"}
        else:
            return {"success": False, "message": "No simulation running"}
    
    def get_simulation_status(self):
        """Get current simulation status"""
        status_info = {
            "status": self.simulation_status,
            "pid": self.current_process.pid if self.current_process else None,
            "log_lines": len(self.simulation_log),
            "recent_log": self.simulation_log[-10:] if self.simulation_log else []
        }
        
        # Add system resource info
        if self.current_process and self.simulation_status == "running":
            try:
                process = psutil.Process(self.current_process.pid)
                status_info.update({
                    "cpu_percent": process.cpu_percent(),
                    "memory_mb": process.memory_info().rss / 1024 / 1024,
                    "runtime_seconds": time.time() - process.create_time()
                })
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass
        
        return status_info
    
    def get_simulation_log(self, lines=100):
        """Get simulation log output"""
        return {
            "total_lines": len(self.simulation_log),
            "log_lines": self.simulation_log[-lines:] if self.simulation_log else []
        }
    
    def run_parameter_sweep(self, sweep_config):
        """Run parameter sweep"""
        if self.simulation_status == "running":
            return {"success": False, "message": "Simulation already running"}
        
        try:
            # Save sweep configuration
            sweep_file = self.base_dir / "config" / "runtime" / "web_sweep.json"
            sweep_file.parent.mkdir(exist_ok=True)
            
            with open(sweep_file, 'w') as f:
                json.dump(sweep_config, f, indent=2)
            
            # Start sweep
            cmd = ["python3", "run_sweep.py", str(sweep_file)]
            
            self.current_process = subprocess.Popen(
                cmd,
                cwd=self.base_dir,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1
            )
            
            self.simulation_status = "running"
            self.simulation_log = []
            
            # Start monitoring threads
            log_thread = threading.Thread(target=self._monitor_simulation_output, daemon=True)
            log_thread.start()
            
            monitor_thread = threading.Thread(target=self._monitor_simulation_process, daemon=True)
            monitor_thread.start()
            
            return {
                "success": True,
                "message": f"Parameter sweep started with PID {self.current_process.pid}",
                "pid": self.current_process.pid
            }
            
        except Exception as e:
            self.simulation_status = "error"
            return {"success": False, "message": f"Failed to start sweep: {str(e)}"}
    
    def _monitor_simulation_output(self):
        """Monitor simulation output in separate thread"""
        if not self.current_process:
            return
        
        try:
            for line in iter(self.current_process.stdout.readline, ''):
                if line:
                    timestamp = datetime.now().strftime("%H:%M:%S")
                    log_entry = f"[{timestamp}] {line.strip()}"
                    self.simulation_log.append(log_entry)
                    
                    # Keep log size manageable
                    if len(self.simulation_log) > 1000:
                        self.simulation_log = self.simulation_log[-800:]
                        
        except Exception as e:
            print(f"Error monitoring simulation output: {e}")
    
    def _monitor_simulation_process(self):
        """Monitor simulation process status"""
        if not self.current_process:
            return
        
        try:
            # Wait for process to complete
            return_code = self.current_process.wait()
            
            if return_code == 0:
                self.simulation_status = "completed"
            else:
                self.simulation_status = "error"
                
        except Exception as e:
            print(f"Error monitoring simulation process: {e}")
            self.simulation_status = "error"
    
    def build_simulation(self, target="web_test"):
        """Build simulation executable"""
        try:
            if target == "cache_test":
                cmd = ["make", "-f", "Makefile_cache", "clean"]
                subprocess.run(cmd, cwd=self.base_dir, check=True, capture_output=True)
                cmd = ["make", "-f", "Makefile_cache"]
            elif target == "web_test":
                cmd = ["make", "-f", "Makefile_web", "clean"]
                subprocess.run(cmd, cwd=self.base_dir, check=True, capture_output=True)
                cmd = ["make", "-f", "Makefile_web"]
            else:
                cmd = ["make", "clean"]
                subprocess.run(cmd, cwd=self.base_dir, check=True, capture_output=True)
                cmd = ["make"]
            
            result = subprocess.run(cmd, cwd=self.base_dir, capture_output=True, text=True)
            
            if result.returncode == 0:
                return {"success": True, "message": f"Build successful for {target}"}
            else:
                return {
                    "success": False, 
                    "message": f"Build failed: {result.stderr}",
                    "stdout": result.stdout,
                    "stderr": result.stderr
                }
                
        except Exception as e:
            return {"success": False, "message": f"Build error: {str(e)}"}
    
    def get_recent_results(self):
        """Get recent simulation results"""
        results_dir = self.base_dir / "regression_runs"
        results = []
        
        if results_dir.exists():
            for result_dir in sorted(results_dir.iterdir(), reverse=True)[:5]:
                if result_dir.is_dir():
                    try:
                        # Look for sweep results
                        csv_file = result_dir / "sweep_results.csv"
                        if csv_file.exists():
                            with open(csv_file, 'r') as f:
                                lines = f.readlines()
                                results.append({
                                    "name": result_dir.name,
                                    "type": "sweep",
                                    "test_cases": len(lines) - 1,  # Exclude header
                                    "path": str(result_dir)
                                })
                    except Exception as e:
                        print(f"Error reading result {result_dir}: {e}")
        
        return results[:5]  # Return 5 most recent