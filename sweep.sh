#!/bin/bash

# SystemC Parameter Sweep Runner
# Usage: ./sweep.sh [sweep_config_file] [batch_name]

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${CYAN}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    print_error "Python 3 is not installed or not in PATH"
    exit 1
fi

# Check if run_sweep.py exists
if [ ! -f "run_sweep.py" ]; then
    print_error "run_sweep.py not found in current directory"
    exit 1
fi

# Interactive mode if no arguments provided
if [ $# -eq 0 ]; then
    echo -e "${CYAN}SystemC Parameter Sweep Runner${NC}"
    echo ""
    
    # Check if config/sweeps directory exists
    if [ ! -d "config/sweeps" ]; then
        print_error "config/sweeps directory not found"
        exit 1
    fi
    
    # Get list of available sweep configurations (only in root of config/sweeps)
    configs=()
    while IFS= read -r -d '' file; do
        configs+=("$(basename "$file")")
    done < <(find config/sweeps -maxdepth 1 -name "*.json" -type f -print0 2>/dev/null | sort -z)
    
    if [ ${#configs[@]} -eq 0 ]; then
        print_error "No sweep configuration files found in config/sweeps/"
        exit 1
    fi
    
    # Show available configurations
    echo "Available sweep configurations:"
    echo ""
    for i in "${!configs[@]}"; do
        printf "%2d) %s\n" $((i+1)) "${configs[$i]}"
    done
    echo ""
    
    # Get user selection
    while true; do
        echo -n "Select a configuration (1-${#configs[@]}) or 'q' to quit: "
        read -r choice
        
        if [ "$choice" = "q" ] || [ "$choice" = "Q" ]; then
            print_info "Exiting..."
            exit 0
        fi
        
        # Check if choice is a valid number
        if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le ${#configs[@]} ]; then
            selected_config="${configs[$((choice-1))]}"
            SWEEP_CONFIG="config/sweeps/$selected_config"
            break
        else
            print_error "Invalid selection. Please enter a number between 1 and ${#configs[@]}, or 'q' to quit."
        fi
    done
    
    # Ask for optional batch name
    echo ""
    echo -n "Enter optional batch name (or press Enter to use default): "
    read -r BATCH_NAME
    
    echo ""
    print_info "Selected: $selected_config"
    if [ -n "$BATCH_NAME" ]; then
        print_info "Batch name: $BATCH_NAME"
    fi
    echo ""
    
else
    # Command-line mode (backward compatibility)
    SWEEP_CONFIG="$1"
    BATCH_NAME="${2:-}"
fi

# Auto-complete sweep config file name
if [ ! -f "$SWEEP_CONFIG" ]; then
    # Try with config/sweeps/ prefix
    if [ -f "config/sweeps/$SWEEP_CONFIG" ]; then
        SWEEP_CONFIG="config/sweeps/$SWEEP_CONFIG"
    # Try adding .json extension
    elif [ -f "config/sweeps/${SWEEP_CONFIG}.json" ]; then
        SWEEP_CONFIG="config/sweeps/${SWEEP_CONFIG}.json"
    # Try adding _sweep.json
    elif [ -f "config/sweeps/${SWEEP_CONFIG}_sweep.json" ]; then
        SWEEP_CONFIG="config/sweeps/${SWEEP_CONFIG}_sweep.json"
    else
        print_error "Sweep config file not found: $1"
        print_info "Available configs in config/sweeps/:"
        if [ -d "config/sweeps" ]; then
            ls config/sweeps/*.json 2>/dev/null || echo "  (none found)"
        fi
        exit 1
    fi
fi

# Verify config file exists
if [ ! -f "$SWEEP_CONFIG" ]; then
    print_error "Sweep config file not found: $SWEEP_CONFIG"
    exit 1
fi

print_info "Starting parameter sweep..."
print_info "Config file: $SWEEP_CONFIG"
if [ -n "$BATCH_NAME" ]; then
    print_info "Batch name: $BATCH_NAME"
fi

# Check SYSTEMC_HOME
if [ -z "$SYSTEMC_HOME" ]; then
    print_warning "SYSTEMC_HOME not set. Make sure it's in your environment."
fi

# Run the Python sweep script
if [ -n "$BATCH_NAME" ]; then
    python3 run_sweep.py "$SWEEP_CONFIG" "$BATCH_NAME"
else
    python3 run_sweep.py "$SWEEP_CONFIG"
fi

# Check if sweep completed successfully
if [ $? -eq 0 ]; then
    print_success "Parameter sweep completed successfully!"
    
    # Show results directory
    TIMESTAMP=$(date +%Y%m%d_%H%M%S)
    CONFIG_NAME=$(basename "$SWEEP_CONFIG" .json)
    if [ -n "$BATCH_NAME" ]; then
        RESULTS_DIR="regression_runs/${TIMESTAMP}_${BATCH_NAME}"
    else
        RESULTS_DIR="regression_runs/${TIMESTAMP}_${CONFIG_NAME}"
    fi
    
    # Find the actual results directory (timestamp might be slightly different)
    ACTUAL_RESULTS=$(find regression_runs -name "*${CONFIG_NAME}*" -type d | tail -1)
    if [ -n "$ACTUAL_RESULTS" ] && [ -d "$ACTUAL_RESULTS" ]; then
        print_info "Results available in: $ACTUAL_RESULTS"
        if [ -f "$ACTUAL_RESULTS/sweep_results.csv" ]; then
            print_info "CSV data: $ACTUAL_RESULTS/sweep_results.csv"
        fi
        if [ -f "$ACTUAL_RESULTS/sweep_summary.txt" ]; then
            print_info "Summary: $ACTUAL_RESULTS/sweep_summary.txt"
        fi
    fi
else
    print_error "Parameter sweep failed!"
    exit 1
fi