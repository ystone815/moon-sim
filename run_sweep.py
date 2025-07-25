#!/usr/bin/env python3
"""
SystemC Parameter Sweep Test Runner (Python Version)
Usage: python3 run_sweep.py <sweep_config_file> [batch_name] [target]
Targets: sim (default), sim_ssd, cache_test, web_test
"""

import json
import os
import sys
import subprocess
import shutil
import time
from datetime import datetime
from pathlib import Path
import argparse

class Colors:
    RED = '\033[0;31m'
    GREEN = '\033[0;32m'
    YELLOW = '\033[1;33m'
    BLUE = '\033[0;34m'
    CYAN = '\033[0;36m'
    NC = '\033[0m'  # No Color

class SweepRunner:
    def __init__(self, sweep_config_file, batch_name=None, target="sim"):
        self.sweep_config_file = sweep_config_file
        
        # Enable auto-detection if target is default and config file might have target field
        self.auto_detect_target = (target == "sim")
        self.target = target
        
        self.sweep_config = self.load_sweep_config()
        
        # Simulation target configuration (target may have been updated by load_sweep_config)
        self.target_configs = {
            "sim": {
                "executable": "./sim",
                "make_target": "sim",
                "description": "Basic SystemC simulation (HostSystem + Memory)"
            },
            "sim_ssd": {
                "executable": "./sim_ssd", 
                "make_target": "sim_ssd",
                "description": "SSD simulation with PCIe interface"
            },
            "cache_test": {
                "executable": "./cache_test",
                "make_target": "cache_test", 
                "description": "Cache performance testing"
            },
            "web_test": {
                "executable": "./web_test",
                "make_target": "web_test",
                "description": "Web monitoring simulation"
            }
        }
        
        # Validate target
        if target not in self.target_configs:
            print(f"{Colors.RED}Error: Invalid target '{target}'. Available: {list(self.target_configs.keys())}{Colors.NC}")
            sys.exit(1)
            
        # Generate batch name with timestamp
        if batch_name:
            self.batch_name = batch_name
        else:
            config_name = Path(sweep_config_file).stem
            self.batch_name = config_name
            
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        self.batch_with_time = f"{timestamp}_{self.batch_name}"
        
        # Setup directories
        self.sweep_results_dir = Path(f"regression_runs/{self.batch_with_time}")
        
        # Results tracking
        self.passed = 0
        self.failed = 0
        self.results = []
        
    def load_sweep_config(self):
        """Load and validate sweep configuration"""
        try:
            with open(self.sweep_config_file, 'r') as f:
                config = json.load(f)
            
            # Auto-detect target from config if not specified by user
            if hasattr(self, 'auto_detect_target') and config.get('target'):
                self.target = config['target']
                print(f"{Colors.BLUE}Auto-detected target from config: {self.target}{Colors.NC}")
                
            return config
        except FileNotFoundError:
            print(f"{Colors.RED}Error: Sweep config file '{self.sweep_config_file}' not found{Colors.NC}")
            sys.exit(1)
        except json.JSONDecodeError as e:
            print(f"{Colors.RED}Error: Invalid JSON in config file: {e}{Colors.NC}")
            sys.exit(1)
            
    def print_header(self):
        """Print sweep information header"""
        print(f"{Colors.CYAN}========================================{Colors.NC}")
        print(f"{Colors.CYAN}SystemC Parameter Sweep Test (Python){Colors.NC}")
        print(f"{Colors.CYAN}========================================{Colors.NC}")
        print(f"{Colors.BLUE}Sweep Config: {self.sweep_config_file}{Colors.NC}")
        print(f"{Colors.BLUE}Batch Name: {self.batch_with_time}{Colors.NC}")
        print(f"{Colors.BLUE}Results Directory: {self.sweep_results_dir}{Colors.NC}")
        print(f"{Colors.BLUE}Target: {self.target} - {self.target_configs[self.target]['description']}{Colors.NC}")
        print(f"{Colors.CYAN}========================================{Colors.NC}")
        
    def print_sweep_parameters(self):
        """Print sweep parameters"""
        print(f"{Colors.YELLOW}Sweep Parameters:{Colors.NC}")
        print(f"  Base Config: {self.sweep_config['base_config']}")
        print(f"  Parameter: {self.sweep_config['parameter']}")
        print(f"  Range: {self.sweep_config['start']} to {self.sweep_config['end']} (step {self.sweep_config['step']})")
        print(f"  Target File: {self.sweep_config['config_file']}")
        print(f"  Simulation Target: {self.target} ({self.target_configs[self.target]['executable']})")
        print("")
        
    def build_simulator(self):
        """Build the SystemC simulator"""
        print(f"{Colors.YELLOW}Building simulator for target '{self.target}'...{Colors.NC}")
        
        # Clean build
        result = subprocess.run(['make', 'clean'], capture_output=True, text=True)
        
        # Build specific target
        make_target = self.target_configs[self.target]['make_target']
        if make_target == "sim":  # Default make target
            result = subprocess.run(['make'], capture_output=True, text=True)
        else:
            result = subprocess.run(['make', make_target], capture_output=True, text=True)
            
        if result.returncode != 0:
            print(f"{Colors.RED}Build failed for target '{self.target}'!{Colors.NC}")
            print(result.stderr)
            sys.exit(1)
            
        print(f"{Colors.GREEN}Build successful{Colors.NC}")
        print("")
        
    def generate_test_configs(self):
        """Generate all test case configurations"""
        print(f"{Colors.YELLOW}Generating test case configurations...{Colors.NC}")
        
        # Create main results directory
        self.sweep_results_dir.mkdir(parents=True, exist_ok=True)
        
        # Copy original sweep config to results directory
        shutil.copy2(self.sweep_config_file, self.sweep_results_dir)
        
        # Generate test cases
        tc_count = 0
        current_value = self.sweep_config['start']
        end_value = self.sweep_config['end']
        step_value = self.sweep_config['step']
        
        while current_value <= end_value:
            tc_count += 1
            tc_name = f"TC{tc_count:03d}"
            tc_config_dir = self.sweep_results_dir / tc_name
            
            print(f"  {tc_name}: {self.sweep_config['parameter']}={current_value}")
            
            # Create TC config directory
            tc_config_dir.mkdir(exist_ok=True)
            
            # Copy base config files (exclude sweeps directory and log files)
            base_config_path = Path(self.sweep_config['base_config'])
            for item in base_config_path.iterdir():
                if item.is_file() and not item.suffix == '.log':
                    shutil.copy2(item, tc_config_dir)
                elif item.is_dir() and item.name != "sweeps":
                    shutil.copytree(item, tc_config_dir / item.name, dirs_exist_ok=True)
            
            # Modify the target parameter
            self.modify_config_parameter(tc_config_dir, current_value)
            
            # Create TC info file
            self.create_tc_info(tc_config_dir, tc_name, current_value)
            
            current_value += step_value
            
        print(f"{Colors.GREEN}Generated {tc_count} test case configurations in {self.sweep_results_dir}{Colors.NC}")
        print("")
        
        return tc_count
        
    def modify_config_parameter(self, tc_config_dir, value):
        """Modify the target parameter in the config file"""
        target_file = tc_config_dir / self.sweep_config['config_file']
        
        if not target_file.exists():
            print(f"{Colors.RED}Warning: Target config file '{self.sweep_config['config_file']}' not found{Colors.NC}")
            return
            
        # Read, modify, and write back the JSON config
        try:
            with open(target_file, 'r') as f:
                config_data = json.load(f)
            
            # Find and update the parameter (simple search in nested structure)
            self.update_parameter_in_dict(config_data, self.sweep_config['parameter'], value)
            
            with open(target_file, 'w') as f:
                json.dump(config_data, f, indent=2)
                
        except Exception as e:
            print(f"{Colors.RED}Error modifying config file {target_file}: {e}{Colors.NC}")
            
    def update_parameter_in_dict(self, data, param_name, value):
        """Recursively search and update parameter in nested dictionary"""
        if isinstance(data, dict):
            for key, val in data.items():
                if key == param_name:
                    data[key] = value
                    return True
                elif isinstance(val, dict):
                    if self.update_parameter_in_dict(val, param_name, value):
                        return True
        return False
        
    def create_tc_info(self, tc_config_dir, tc_name, value):
        """Create TC information file"""
        info_content = f"""Test Case: {tc_name}
Parameter: {self.sweep_config['parameter']}
Value: {value}
Batch: {self.batch_with_time}
Generated: {datetime.now()}
Base Config: {self.sweep_config['base_config']}
Modified File: {self.sweep_config['config_file']}
"""
        with open(tc_config_dir / "TC_INFO.txt", 'w') as f:
            f.write(info_content)
            
    def run_test_case(self, tc_number, value):
        """Run a single test case"""
        tc_name = f"TC{tc_number:03d}"
        tc_config_dir = self.sweep_results_dir / tc_name
        tc_results_dir = self.sweep_results_dir / tc_name
        
        print(f"{Colors.BLUE}========================================{Colors.NC}")
        print(f"{Colors.BLUE}Running {tc_name}: {self.sweep_config['parameter']}={value}{Colors.NC}")
        print(f"{Colors.BLUE}Results: {tc_results_dir}{Colors.NC}")
        print(f"{Colors.BLUE}========================================{Colors.NC}")
        
        # Check if config directory exists
        if not tc_config_dir.exists():
            print(f"{Colors.RED}Error: Test case config directory '{tc_config_dir}' not found{Colors.NC}")
            return False
        
        # Note: Log files are now generated directly in TC directories
            
        # Run simulation with timeout
        start_time = time.time()
        try:
            executable = self.target_configs[self.target]['executable']
            # Use longer timeout for complex simulations like SSD
            timeout = 60 if self.target == 'sim_ssd' else 30
            result = subprocess.run([executable, str(tc_config_dir)], 
                                  timeout=timeout, 
                                  capture_output=True, 
                                  text=True)
            
            end_time = time.time()
            duration = int(end_time - start_time)
            
            if result.returncode == 0:
                # Success
                print(f"{Colors.GREEN}✓ {tc_name} PASSED ({duration}s){Colors.NC}")
                
                # Note: Log files are now generated directly in TC directories
                
                # Move metric files to results directory if they exist
                for metric_file in ["metrics.csv", "performance.json"]:
                    if Path(metric_file).exists():
                        shutil.move(metric_file, tc_config_dir)
                        print(f"{Colors.BLUE}Moved {metric_file} to {tc_config_dir}{Colors.NC}")
                
                # Extract performance metrics from files or console output
                throughput, sim_time, latency, latency_p50, latency_p95, latency_p99, latency_stddev, bw_mbps = self.extract_performance_metrics(tc_config_dir, result.stdout)
                if throughput and throughput != "0":
                    print(f"{Colors.YELLOW}Performance: {throughput} cps, {sim_time} ms, {latency} ns avg, p95={latency_p95} ns, {bw_mbps} MB/s{Colors.NC}")
                
                # Store result
                self.results.append(f"{tc_name},{value},{throughput},{sim_time},{latency},{latency_p50},{latency_p95},{latency_p99},{latency_stddev},{bw_mbps},PASSED")
                
                # Create test case summary
                self.create_tc_result(tc_config_dir, tc_name, value, "PASSED", duration)
                
                return True
                
            else:
                # Failure
                print(f"{Colors.RED}✗ {tc_name} FAILED{Colors.NC}")
                
                # Note: Log files are now generated directly in TC directories
                
                self.results.append(f"{tc_name},{value},0,0,0,0,0,0,0,0,FAILED")
                self.create_tc_result(tc_config_dir, tc_name, value, "FAILED", duration)
                
                return False
                
        except subprocess.TimeoutExpired:
            print(f"{Colors.RED}✗ {tc_name} TIMEOUT{Colors.NC}")
            self.results.append(f"{tc_name},{value},0,0,0,0,0,0,0,0,TIMEOUT")
            self.create_tc_result(tc_config_dir, tc_name, value, "TIMEOUT", 30)
            return False
            
    def extract_performance_metrics(self, tc_results_dir, stdout_content=""):
        """Extract performance metrics from generated metric files or fallback to console parsing"""
        
        # Try to read metrics from generated files first (preferred method)
        metrics_csv_path = tc_results_dir / "metrics.csv"
        performance_json_path = tc_results_dir / "performance.json"
        
        throughput = "0"
        sim_time = "0"  
        latency = "0"
        latency_p50 = "0"
        latency_p95 = "0"
        latency_p99 = "0"
        latency_stddev = "0"
        bw_mbps = "0"
        
        # Read metrics from CSV file
        if metrics_csv_path.exists():
            try:
                with open(metrics_csv_path, 'r') as f:
                    lines = f.readlines()
                    for line in lines[1:]:  # Skip header
                        parts = line.strip().split(',')
                        if len(parts) >= 3:
                            metric, value, unit = parts[0], parts[1], parts[2]
                            if metric == "sim_speed":
                                throughput = value
                            elif metric == "simulation_time":
                                sim_time = value
                            elif metric == "avg_latency_ns":
                                latency = value
                            elif metric == "p50_latency_ns":
                                latency_p50 = value
                            elif metric == "p95_latency_ns":
                                latency_p95 = value
                            elif metric == "p99_latency_ns":
                                latency_p99 = value
                            elif metric == "stddev_latency_ns":
                                latency_stddev = value
                            elif metric == "bandwidth_mbps":
                                bw_mbps = value
                print(f"{Colors.GREEN}Successfully read metrics from {metrics_csv_path}{Colors.NC}")
            except Exception as e:
                print(f"{Colors.YELLOW}Warning: Failed to read metrics.csv: {e}{Colors.NC}")
        
        # Read additional performance data from JSON file
        if performance_json_path.exists():
            try:
                with open(performance_json_path, 'r') as f:
                    perf_data = json.load(f)
                    
                # Extract sim speed if not already set
                if throughput == "0" and "performance" in perf_data and "sim_speed_cps" in perf_data["performance"]:
                    throughput = str(int(float(perf_data["performance"]["sim_speed_cps"])))
                    
                # Extract simulation time if not already set  
                if sim_time == "0" and "simulation" in perf_data and "duration_ms" in perf_data["simulation"]:
                    sim_time = str(perf_data["simulation"]["duration_ms"])
                
                # Extract bandwidth if available
                if bw_mbps == "0" and "performance" in perf_data and "bandwidth_mbps" in perf_data["performance"]:
                    bw_mbps = str(perf_data["performance"]["bandwidth_mbps"])
                
                # Extract latency metrics if available
                if "latency" in perf_data:
                    latency_data = perf_data["latency"]
                    if latency == "0" and "avg_ns" in latency_data:
                        latency = str(latency_data["avg_ns"])
                    if latency_p50 == "0" and "p50_ns" in latency_data:
                        latency_p50 = str(latency_data["p50_ns"])
                    if latency_p95 == "0" and "p95_ns" in latency_data:
                        latency_p95 = str(latency_data["p95_ns"])
                    if latency_p99 == "0" and "p99_ns" in latency_data:
                        latency_p99 = str(latency_data["p99_ns"])
                    if latency_stddev == "0" and "stddev_ns" in latency_data:
                        latency_stddev = str(latency_data["stddev_ns"])
                    
                print(f"{Colors.GREEN}Successfully read performance data from {performance_json_path}{Colors.NC}")
            except Exception as e:
                print(f"{Colors.YELLOW}Warning: Failed to read performance.json: {e}{Colors.NC}")
        
        # Fallback to console output parsing if no metric files found
        if throughput == "0" and sim_time == "0":
            print(f"{Colors.YELLOW}Falling back to console output parsing{Colors.NC}")
            content = stdout_content
            
            # Fallback to log files if stdout is empty
            if not content:
                log_files = list(tc_results_dir.glob("simulation_*.log"))
                if log_files:
                    latest_log = max(log_files, key=lambda x: x.stat().st_mtime)
                    try:
                        with open(latest_log, 'r') as f:
                            content = f.read()
                    except Exception:
                        pass
            
            if content:
                try:
                    for line in content.split('\n'):
                        # Extract basic metrics
                        if "Sim Speed:" in line:
                            throughput = line.split("Sim Speed:")[1].strip().split()[0]
                        elif "CPS" in line:
                            # For basic sim target
                            parts = line.split("Sim Speed:")
                            if len(parts) > 1:
                                throughput = parts[1].strip().split()[0]
                        if "Simulation time:" in line:
                            sim_time = line.split("Simulation time:")[1].strip().split()[0]
                        elif "ms (" in line and "seconds)" in line:
                            # For basic sim target format
                            parts = line.split("time:")
                            if len(parts) > 1:
                                sim_time = parts[1].strip().split()[0]
                                
                        # Extract latency metrics if available  
                        if "Period avg latency:" in line:
                            latency = line.split("Period avg latency:")[1].strip().split()[0]
                        if "Period median latency:" in line:
                            latency_p50 = line.split("Period median latency:")[1].strip().split()[0]
                        if "Period 95th percentile:" in line:
                            latency_p95 = line.split("Period 95th percentile:")[1].strip().split()[0]
                        if "Period 99th percentile:" in line:
                            latency_p99 = line.split("Period 99th percentile:")[1].strip().split()[0]
                        if "Period std deviation:" in line:
                            latency_stddev = line.split("Period std deviation:")[1].strip().split()[0]
                        if "Average throughput:" in line and "MB/sec" in line:
                            bw_mbps = line.split("Average throughput:")[1].strip().split()[0]
                            
                except Exception as e:
                    print(f"{Colors.YELLOW}Warning: Console parsing failed: {e}{Colors.NC}")
                    
        return throughput, sim_time, latency, latency_p50, latency_p95, latency_p99, latency_stddev, bw_mbps
            
    def create_tc_result(self, tc_results_dir, tc_name, value, status, duration):
        """Create test case result file"""
        result_content = f"""Test Case: {tc_name}
Parameter: {self.sweep_config['parameter']} = {value}
Batch: {self.batch_with_time}
Status: {status}
Duration: {duration}s
Timestamp: {datetime.now()}
Results: {tc_results_dir}
"""
        with open(tc_results_dir / "TC_RESULT.txt", 'w') as f:
            f.write(result_content)
            
    def run_sweep(self):
        """Run the complete parameter sweep"""
        print(f"{Colors.YELLOW}Running test cases...{Colors.NC}")
        
        tc_number = 1
        current_value = self.sweep_config['start']
        end_value = self.sweep_config['end']
        step_value = self.sweep_config['step']
        
        while current_value <= end_value:
            print(f"{Colors.CYAN}DEBUG: About to run TC{tc_number:03d} with value {current_value}{Colors.NC}")
            
            if self.run_test_case(tc_number, current_value):
                self.passed += 1
                print(f"{Colors.CYAN}DEBUG: TC{tc_number:03d} passed{Colors.NC}")
            else:
                self.failed += 1
                print(f"{Colors.CYAN}DEBUG: TC{tc_number:03d} failed{Colors.NC}")
                
            print("")
            
            tc_number += 1
            current_value += step_value
            print(f"{Colors.CYAN}DEBUG: Next - tc_number={tc_number}, current_value={current_value}{Colors.NC}")
            
    def generate_analysis(self):
        """Generate sweep analysis and reports"""
        print(f"{Colors.CYAN}========================================{Colors.NC}")
        print(f"{Colors.CYAN}Sweep Analysis{Colors.NC}")
        print(f"{Colors.CYAN}========================================{Colors.NC}")
        
        # Create CSV results
        csv_file = self.sweep_results_dir / "sweep_results.csv"
        with open(csv_file, 'w') as f:
            f.write(f"TestCase,{self.sweep_config['parameter']},SimSpeed_CPS,SimTime_MS,Latency_Avg_NS,Latency_P50_NS,Latency_P95_NS,Latency_P99_NS,Latency_StdDev_NS,BW_MBPS,Status\n")
            for result in self.results:
                f.write(result + "\n")
                
        # Create summary
        self.create_sweep_summary()
        
        # Create reproduction script
        self.create_reproduction_script()
        
        # Display results
        self.display_results()
        
    def create_sweep_summary(self):
        """Create comprehensive sweep summary"""
        summary_file = self.sweep_results_dir / "sweep_summary.txt"
        
        summary_content = f"""Parameter Sweep Summary
=======================
Sweep Name: {self.sweep_config['parameter']}
Batch: {self.batch_with_time}
Timestamp: {datetime.now()}
Base Config: {self.sweep_config['base_config']}
Parameter Range: {self.sweep_config['start']} to {self.sweep_config['end']} (step {self.sweep_config['step']})
Total Test Cases: {self.passed + self.failed}
Passed: {self.passed}
Failed: {self.failed}

Results Directory: {self.sweep_results_dir}

Test Case Results:
Parameter vs Performance:
"""

        # Add performance data
        for result in self.results:
            parts = result.split(',')
            if len(parts) >= 11 and parts[10] == "PASSED":
                tc_name, param, throughput, simtime, latency_avg, latency_p50, latency_p95, latency_p99, latency_stddev, bw_mbps, status = parts
                summary_content += f"  {tc_name} ({self.sweep_config['parameter']}={param}): {throughput} cps ({simtime} ms, {latency_avg} ns avg, p95={latency_p95} ns, {bw_mbps} MB/s)\n"
                
        # Overall status
        if self.failed == 0:
            summary_content += "\nOverall Status: PASSED\n"
        else:
            summary_content += "\nOverall Status: FAILED\n"
            
        with open(summary_file, 'w') as f:
            f.write(summary_content)
            
    def create_reproduction_script(self):
        """Create script to reproduce the sweep"""
        repro_script = self.sweep_results_dir / "reproduce_sweep.sh"
        
        script_content = f"""#!/bin/bash
# Reproduction script for sweep batch: {self.batch_with_time}
# Generated: {datetime.now()}

echo "Reproducing sweep: {self.batch_with_time}"
echo "Using regression results directory: {self.sweep_results_dir}"

# Run individual test cases using configs from regression results
"""

        tc_number = 1
        current_value = self.sweep_config['start']
        while current_value <= self.sweep_config['end']:
            tc_name = f"TC{tc_number:03d}"
            script_content += f"./sim {self.sweep_results_dir}/{tc_name}  # {self.sweep_config['parameter']}={current_value}\n"
            tc_number += 1
            current_value += self.sweep_config['step']
            
        with open(repro_script, 'w') as f:
            f.write(script_content)
            
        # Make executable
        os.chmod(repro_script, 0o755)
        
    def display_results(self):
        """Display final results"""
        print(f"{Colors.BLUE}Parameter: {self.sweep_config['parameter']}{Colors.NC}")
        print(f"{Colors.BLUE}Range: {self.sweep_config['start']} to {self.sweep_config['end']} (step {self.sweep_config['step']}){Colors.NC}")
        print(f"{Colors.GREEN}Passed: {self.passed}{Colors.NC}")
        print(f"{Colors.RED}Failed: {self.failed}{Colors.NC}")
        print(f"{Colors.BLUE}Total Test Cases: {self.passed + self.failed}{Colors.NC}")
        
        print(f"{Colors.YELLOW}Performance Summary:{Colors.NC}")
        for result in self.results:
            parts = result.split(',')
            if len(parts) >= 11 and parts[10] == "PASSED":
                tc_name, param, throughput, simtime, latency_avg, latency_p50, latency_p95, latency_p99, latency_stddev, bw_mbps, status = parts
                print(f"  {tc_name} ({self.sweep_config['parameter']}={param}): {throughput} cps ({simtime} ms, {latency_avg} ns avg, p95={latency_p95} ns, {bw_mbps} MB/s)")
                
        if self.failed == 0:
            print(f"{Colors.GREEN}All test cases passed! 🎉{Colors.NC}")
        else:
            print(f"{Colors.RED}Some test cases failed! 😞{Colors.NC}")
            
        print("")
        print(f"{Colors.CYAN}========================================{Colors.NC}")
        print(f"{Colors.CYAN}Sweep Completed{Colors.NC}")
        print(f"{Colors.CYAN}========================================{Colors.NC}")
        print(f"{Colors.YELLOW}Results Directory: {self.sweep_results_dir}{Colors.NC}")
        print(f"{Colors.YELLOW}CSV Data: {self.sweep_results_dir}/sweep_results.csv{Colors.NC}")
        print(f"{Colors.YELLOW}Summary: {self.sweep_results_dir}/sweep_summary.txt{Colors.NC}")
        print(f"{Colors.YELLOW}Reproduction Script: {self.sweep_results_dir}/reproduce_sweep.sh{Colors.NC}")
        
    def run(self):
        """Run the complete sweep process"""
        self.print_header()
        self.print_sweep_parameters()
        self.build_simulator()
        self.generate_test_configs()
        self.run_sweep()
        self.generate_analysis()
        
        return 0 if self.failed == 0 else 1

def main():
    parser = argparse.ArgumentParser(description='SystemC Parameter Sweep Test Runner')
    parser.add_argument('sweep_config', help='JSON file defining the parameter sweep')
    parser.add_argument('batch_name', nargs='?', default='', help='Optional custom batch name')
    parser.add_argument('target', nargs='?', default='sim', help='Simulation target (sim, sim_ssd, cache_test, web_test)')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.sweep_config):
        print(f"{Colors.RED}Error: Sweep config file '{args.sweep_config}' not found{Colors.NC}")
        print(f"{Colors.YELLOW}Example: python3 run_sweep.py config/sweeps/write_ratio_sweep.json my_sweep sim_ssd{Colors.NC}")
        sys.exit(1)
    
    # Handle empty batch name properly    
    batch_name = args.batch_name if args.batch_name else None
        
    runner = SweepRunner(args.sweep_config, batch_name, args.target)
    exit_code = runner.run()
    sys.exit(exit_code)

if __name__ == "__main__":
    main()