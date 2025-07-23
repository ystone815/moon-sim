#!/usr/bin/env python3
"""
SystemC Parameter Sweep Test Runner (Python Version)
Usage: python3 run_sweep.py <sweep_config_file> [batch_name]
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
    def __init__(self, sweep_config_file, batch_name=None):
        self.sweep_config_file = sweep_config_file
        self.sweep_config = self.load_sweep_config()
        
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
        print(f"{Colors.CYAN}========================================{Colors.NC}")
        
    def print_sweep_parameters(self):
        """Print sweep parameters"""
        print(f"{Colors.YELLOW}Sweep Parameters:{Colors.NC}")
        print(f"  Base Config: {self.sweep_config['base_config']}")
        print(f"  Parameter: {self.sweep_config['parameter']}")
        print(f"  Range: {self.sweep_config['start']} to {self.sweep_config['end']} (step {self.sweep_config['step']})")
        print(f"  Target File: {self.sweep_config['config_file']}")
        print("")
        
    def build_simulator(self):
        """Build the SystemC simulator"""
        print(f"{Colors.YELLOW}Building simulator...{Colors.NC}")
        
        # Clean build
        result = subprocess.run(['make', 'clean'], capture_output=True, text=True)
        
        # Build
        result = subprocess.run(['make'], capture_output=True, text=True)
        if result.returncode != 0:
            print(f"{Colors.RED}Build failed!{Colors.NC}")
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
            
            # Copy base config files (exclude sweeps directory to prevent infinite recursion)
            base_config_path = Path(self.sweep_config['base_config'])
            for item in base_config_path.iterdir():
                if item.is_file():
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
        
        # Clear existing logs
        log_files = list(Path("log").glob("simulation_*.log"))
        for log_file in log_files:
            log_file.unlink()
            
        # Run simulation with timeout
        start_time = time.time()
        try:
            result = subprocess.run(['./sim', str(tc_config_dir)], 
                                  timeout=30, 
                                  capture_output=True, 
                                  text=True)
            
            end_time = time.time()
            duration = int(end_time - start_time)
            
            if result.returncode == 0:
                # Success
                print(f"{Colors.GREEN}✓ {tc_name} PASSED ({duration}s){Colors.NC}")
                
                # Move logs to results directory
                log_files = list(Path("log").glob("simulation_*.log"))
                for log_file in log_files:
                    shutil.move(str(log_file), tc_config_dir)
                
                # Extract performance metrics
                throughput, sim_time, latency, latency_p50, latency_p95, latency_p99, latency_stddev, bw_mbps = self.extract_performance_metrics(tc_config_dir)
                if throughput:
                    print(f"{Colors.YELLOW}Performance: {throughput} tps, {sim_time} ms, {latency} ns avg, p95={latency_p95} ns, {bw_mbps} MB/s{Colors.NC}")
                
                # Store result
                self.results.append(f"{tc_name},{value},{throughput},{sim_time},{latency},{latency_p50},{latency_p95},{latency_p99},{latency_stddev},{bw_mbps},PASSED")
                
                # Create test case summary
                self.create_tc_result(tc_config_dir, tc_name, value, "PASSED", duration)
                
                return True
                
            else:
                # Failure
                print(f"{Colors.RED}✗ {tc_name} FAILED{Colors.NC}")
                
                # Move logs even on failure
                log_files = list(Path("log").glob("simulation_*.log"))
                for log_file in log_files:
                    shutil.move(str(log_file), tc_config_dir)
                
                self.results.append(f"{tc_name},{value},0,0,0,0,0,0,0,0,FAILED")
                self.create_tc_result(tc_config_dir, tc_name, value, "FAILED", duration)
                
                return False
                
        except subprocess.TimeoutExpired:
            print(f"{Colors.RED}✗ {tc_name} TIMEOUT{Colors.NC}")
            self.results.append(f"{tc_name},{value},0,0,0,0,0,0,0,0,TIMEOUT")
            self.create_tc_result(tc_config_dir, tc_name, value, "TIMEOUT", 30)
            return False
            
    def extract_performance_metrics(self, tc_results_dir):
        """Extract performance metrics from simulation log"""
        log_files = list(tc_results_dir.glob("simulation_*.log"))
        if not log_files:
            return "0", "0", "0", "0", "0", "0", "0", "0"
            
        latest_log = max(log_files, key=lambda x: x.stat().st_mtime)
        
        try:
            with open(latest_log, 'r') as f:
                content = f.read()
                
            throughput = "0"
            sim_time = "0"
            latency = "0"
            latency_p50 = "0"
            latency_p95 = "0"
            latency_p99 = "0"
            latency_stddev = "0"
            bw_mbps = "0"
            
            for line in content.split('\n'):
                # Extract existing metrics
                if "Throughput:" in line:
                    throughput = line.split("Throughput:")[1].strip().split()[0]
                if "Simulation time:" in line:
                    sim_time = line.split("Simulation time:")[1].strip().split()[0]
                    
                # Extract new metrics from profiler reports
                if "Period avg latency:" in line:
                    latency_str = line.split("Period avg latency:")[1].strip().split()[0]
                    latency = latency_str
                    
                if "Period median latency:" in line:
                    p50_str = line.split("Period median latency:")[1].strip().split()[0]
                    latency_p50 = p50_str
                    
                if "Period 95th percentile:" in line:
                    p95_str = line.split("Period 95th percentile:")[1].strip().split()[0]
                    latency_p95 = p95_str
                    
                if "Period 99th percentile:" in line:
                    p99_str = line.split("Period 99th percentile:")[1].strip().split()[0]
                    latency_p99 = p99_str
                    
                if "Period std deviation:" in line:
                    stddev_str = line.split("Period std deviation:")[1].strip().split()[0]
                    latency_stddev = stddev_str
                    
                if "Average throughput:" in line and "MB/sec" in line:
                    bw_str = line.split("Average throughput:")[1].strip().split()[0]
                    bw_mbps = bw_str
                    
            return throughput, sim_time, latency, latency_p50, latency_p95, latency_p99, latency_stddev, bw_mbps
            
        except Exception:
            return "0", "0", "0", "0", "0", "0", "0", "0"
            
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
            f.write(f"TestCase,{self.sweep_config['parameter']},Throughput_TPS,SimTime_MS,Latency_Avg_NS,Latency_P50_NS,Latency_P95_NS,Latency_P99_NS,Latency_StdDev_NS,BW_MBPS,Status\n")
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
                summary_content += f"  {tc_name} ({self.sweep_config['parameter']}={param}): {throughput} tps ({simtime} ms, {latency_avg} ns avg, p95={latency_p95} ns, {bw_mbps} MB/s)\n"
                
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
                print(f"  {tc_name} ({self.sweep_config['parameter']}={param}): {throughput} tps ({simtime} ms, {latency_avg} ns avg, p95={latency_p95} ns, {bw_mbps} MB/s)")
                
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
    parser.add_argument('batch_name', nargs='?', help='Optional custom batch name')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.sweep_config):
        print(f"{Colors.RED}Error: Sweep config file '{args.sweep_config}' not found{Colors.NC}")
        print(f"{Colors.YELLOW}Example: python3 run_sweep.py config/sweeps/write_ratio_sweep.json my_sweep{Colors.NC}")
        sys.exit(1)
        
    runner = SweepRunner(args.sweep_config, args.batch_name)
    exit_code = runner.run()
    sys.exit(exit_code)

if __name__ == "__main__":
    main()