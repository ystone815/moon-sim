#!/usr/bin/env python3
"""
SystemC Terminal Performance Monitor
Real-time terminal-based monitoring for SystemC simulations
"""

import json
import os
import time
import threading
from datetime import datetime
import curses
import signal
import sys

class TerminalMonitor:
    def __init__(self, metrics_file="metrics.json", update_interval=1.0):
        self.metrics_file = metrics_file
        self.update_interval = update_interval
        self.last_modified = 0
        self.latest_metrics = {}
        self.history = []
        self.max_history = 50
        self.running = True
        
    def load_metrics(self):
        """Load metrics from JSON file"""
        try:
            if os.path.exists(self.metrics_file):
                current_modified = os.path.getmtime(self.metrics_file)
                if current_modified > self.last_modified:
                    self.last_modified = current_modified
                    with open(self.metrics_file, 'r') as f:
                        content = f.read().replace('\\n', '\n')
                        self.latest_metrics = json.loads(content)
                        
                        # Add to history
                        self.history.append({
                            'timestamp': datetime.now(),
                            'metrics': self.latest_metrics.get('metrics', {})
                        })
                        if len(self.history) > self.max_history:
                            self.history.pop(0)
                        return True
        except Exception as e:
            pass
        return False
    
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
    
    def simple_monitor(self):
        """Simple text-based monitoring without curses"""
        print("=" * 80)
        print("SystemC Terminal Performance Monitor")
        print("=" * 80)
        print(f"Monitoring: {self.metrics_file}")
        print(f"Update interval: {self.update_interval}s")
        print("Press Ctrl+C to exit")
        print("=" * 80)
        
        try:
            while self.running:
                if self.load_metrics():
                    self.display_simple_metrics()
                else:
                    print(f"[{datetime.now().strftime('%H:%M:%S')}] Waiting for metrics file...")
                
                time.sleep(self.update_interval)
                
        except KeyboardInterrupt:
            print("\nMonitoring stopped.")
    
    def display_simple_metrics(self):
        """Display metrics in simple text format"""
        os.system('clear' if os.name == 'posix' else 'cls')
        
        metrics = self.latest_metrics.get('metrics', {})
        sim_time_ns = self.latest_metrics.get('simulation_time_ns', 0)
        
        print("=" * 80)
        print("SystemC Performance Monitor")
        print("=" * 80)
        print(f"Last Update: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"Simulation Time: {sim_time_ns / 1e9:.3f} seconds")
        print("=" * 80)
        
        # Performance metrics
        perf = metrics.get('performance', {})
        print("\n📊 PERFORMANCE METRICS")
        print("-" * 40)
        print(f"Throughput:    {perf.get('packet_rate_pps', 0):>10.1f} packets/sec")
        print(f"Bandwidth:     {perf.get('bandwidth_mbps', 0):>10.2f} Mbps")
        print(f"Total Packets: {perf.get('total_packets', 0):>10}")
        
        # Cache metrics
        caches = metrics.get('caches', {})
        if caches:
            print("\n🗄️  CACHE STATISTICS")
            print("-" * 40)
            total_hits = sum(cache.get('hits', 0) for cache in caches.values())
            total_misses = sum(cache.get('misses', 0) for cache in caches.values())
            total_accesses = total_hits + total_misses
            hit_rate = (total_hits / total_accesses * 100) if total_accesses > 0 else 0
            
            print(f"Hit Rate:      {hit_rate:>10.1f}%")
            print(f"Total Hits:    {total_hits:>10}")
            print(f"Total Misses:  {total_misses:>10}")
            print(f"Total Access:  {total_accesses:>10}")
        
        # DRAM metrics
        dram = metrics.get('dram', {})
        if dram:
            print("\n💾 DRAM STATISTICS")
            print("-" * 40)
            total_requests = sum(d.get('total_requests', 0) for d in dram.values())
            total_row_hits = sum(d.get('row_hits', 0) for d in dram.values())
            total_conflicts = sum(d.get('bank_conflicts', 0) for d in dram.values())
            
            row_hit_rate = (total_row_hits / total_requests * 100) if total_requests > 0 else 0
            
            print(f"Row Hit Rate:  {row_hit_rate:>10.1f}%")
            print(f"Total Requests:{total_requests:>10}")
            print(f"Row Hits:      {total_row_hits:>10}")
            print(f"Bank Conflicts:{total_conflicts:>10}")
        
        # Components
        components = metrics.get('components', {})
        if components:
            print("\n🔧 COMPONENT ACTIVITY")
            print("-" * 40)
            for name, count in components.items():
                print(f"{name:<20} {count:>10}")
        
        # History graph (simple ASCII)
        if len(self.history) > 1:
            print("\n📈 THROUGHPUT HISTORY (last 20 points)")
            print("-" * 40)
            recent_history = self.history[-20:]
            max_val = max(h['metrics'].get('performance', {}).get('packet_rate_pps', 0) 
                         for h in recent_history)
            
            if max_val > 0:
                for i, h in enumerate(recent_history):
                    pps = h['metrics'].get('performance', {}).get('packet_rate_pps', 0)
                    bar_length = int((pps / max_val) * 30)
                    bar = "█" * bar_length
                    print(f"{i+1:>2}: {bar:<30} {pps:>6.1f} pps")
        
        print("\n" + "=" * 80)
        print("Press Ctrl+C to exit")
    
    def curses_monitor(self, stdscr):
        """Advanced curses-based monitoring with real-time updates"""
        curses.curs_set(0)  # Hide cursor
        stdscr.nodelay(1)   # Non-blocking input
        stdscr.timeout(100) # 100ms timeout
        
        # Color pairs
        curses.start_color()
        curses.init_pair(1, curses.COLOR_GREEN, curses.COLOR_BLACK)
        curses.init_pair(2, curses.COLOR_YELLOW, curses.COLOR_BLACK)
        curses.init_pair(3, curses.COLOR_RED, curses.COLOR_BLACK)
        curses.init_pair(4, curses.COLOR_CYAN, curses.COLOR_BLACK)
        
        while self.running:
            try:
                stdscr.clear()
                self.display_curses_metrics(stdscr)
                stdscr.refresh()
                
                # Check for 'q' key to quit
                key = stdscr.getch()
                if key == ord('q') or key == ord('Q'):
                    break
                
                if self.load_metrics():
                    pass  # Metrics updated
                
                time.sleep(self.update_interval)
                
            except KeyboardInterrupt:
                break
        
        self.running = False
    
    def display_curses_metrics(self, stdscr):
        """Display metrics using curses for better formatting"""
        height, width = stdscr.getmaxyx()
        
        metrics = self.latest_metrics.get('metrics', {})
        sim_time_ns = self.latest_metrics.get('simulation_time_ns', 0)
        
        row = 0
        
        # Header
        title = "SystemC Performance Monitor"
        stdscr.addstr(row, (width - len(title)) // 2, title, curses.color_pair(1) | curses.A_BOLD)
        row += 1
        stdscr.addstr(row, 0, "=" * width, curses.color_pair(1))
        row += 2
        
        # Time info
        time_str = f"Last Update: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
        sim_str = f"Simulation Time: {sim_time_ns / 1e9:.3f}s"
        stdscr.addstr(row, 0, time_str, curses.color_pair(4))
        stdscr.addstr(row, width - len(sim_str), sim_str, curses.color_pair(4))
        row += 2
        
        # Performance metrics
        perf = metrics.get('performance', {})
        stdscr.addstr(row, 0, "📊 PERFORMANCE", curses.color_pair(2) | curses.A_BOLD)
        row += 1
        stdscr.addstr(row, 0, f"Throughput:    {perf.get('packet_rate_pps', 0):8.1f} pps")
        row += 1
        stdscr.addstr(row, 0, f"Bandwidth:     {perf.get('bandwidth_mbps', 0):8.2f} Mbps")
        row += 1
        stdscr.addstr(row, 0, f"Total Packets: {perf.get('total_packets', 0):8}")
        row += 2
        
        # Cache metrics
        caches = metrics.get('caches', {})
        if caches and row < height - 10:
            stdscr.addstr(row, 0, "🗄️  CACHE", curses.color_pair(2) | curses.A_BOLD)
            row += 1
            total_hits = sum(cache.get('hits', 0) for cache in caches.values())
            total_misses = sum(cache.get('misses', 0) for cache in caches.values())
            total_accesses = total_hits + total_misses
            hit_rate = (total_hits / total_accesses * 100) if total_accesses > 0 else 0
            
            color = curses.color_pair(1) if hit_rate > 80 else curses.color_pair(3) if hit_rate < 50 else 0
            stdscr.addstr(row, 0, f"Hit Rate:      {hit_rate:8.1f}%", color)
            row += 1
            stdscr.addstr(row, 0, f"Total Access:  {total_accesses:8}")
            row += 2
        
        # ASCII graph
        if len(self.history) > 1 and row < height - 15:
            stdscr.addstr(row, 0, "📈 THROUGHPUT GRAPH", curses.color_pair(2) | curses.A_BOLD)
            row += 1
            
            recent_history = self.history[-min(10, len(self.history)):]
            max_val = max(h['metrics'].get('performance', {}).get('packet_rate_pps', 0) 
                         for h in recent_history)
            
            if max_val > 0:
                for i, h in enumerate(recent_history):
                    if row >= height - 3:
                        break
                    pps = h['metrics'].get('performance', {}).get('packet_rate_pps', 0)
                    bar_length = int((pps / max_val) * 30)
                    bar = "█" * bar_length
                    stdscr.addstr(row, 0, f"{i+1:2}: {bar:<30} {pps:6.1f}")
                    row += 1
        
        # Footer
        if row < height - 2:
            footer = "Press 'q' to quit | Ctrl+C to exit"
            stdscr.addstr(height - 2, (width - len(footer)) // 2, footer, curses.color_pair(4))
    
    def run(self, use_curses=False):
        """Run the monitor"""
        def signal_handler(sig, frame):
            self.running = False
            sys.exit(0)
        
        signal.signal(signal.SIGINT, signal_handler)
        
        if use_curses:
            try:
                curses.wrapper(self.curses_monitor)
            except:
                # Fallback to simple monitor if curses fails
                self.simple_monitor()
        else:
            self.simple_monitor()

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='SystemC Terminal Performance Monitor')
    parser.add_argument('--file', '-f', default='metrics.json', 
                       help='Metrics JSON file to monitor (default: metrics.json)')
    parser.add_argument('--interval', '-i', type=float, default=1.0,
                       help='Update interval in seconds (default: 1.0)')
    parser.add_argument('--curses', '-c', action='store_true',
                       help='Use curses interface (advanced)')
    
    args = parser.parse_args()
    
    print(f"Starting SystemC Terminal Monitor...")
    print(f"Monitoring file: {args.file}")
    print(f"Update interval: {args.interval}s")
    
    monitor = TerminalMonitor(args.file, args.interval)
    monitor.run(use_curses=args.curses)

if __name__ == '__main__':
    main()