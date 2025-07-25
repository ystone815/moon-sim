#!/bin/bash

# SystemC Parameter Sweep Runner
# Usage: ./sweep.sh [sweep_config_file] [batch_name] [target]
# Available targets: sim (default), sim_ssd, cache_test, web_test

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

# Available simulation targets
declare -A TARGETS
TARGETS["sim"]="Basic SystemC simulation (HostSystem + Memory)"
TARGETS["sim_ssd"]="SSD simulation with PCIe interface"
TARGETS["cache_test"]="Cache performance testing"
TARGETS["web_test"]="Web monitoring simulation"

# Interactive mode if no arguments provided
if [ $# -eq 0 ]; then
    echo -e "${CYAN}SystemC Parameter Sweep Runner${NC}"
    echo ""
    
    # Check if config/sweeps directory exists
    if [ ! -d "config/sweeps" ]; then
        print_error "config/sweeps directory not found"
        exit 1
    fi
    
    # Get list of available sweep configurations (recursively search subdirectories)
    configs=()
    config_paths=()
    while IFS= read -r -d '' file; do
        # Store relative path from config/sweeps/
        rel_path=$(echo "$file" | sed 's|config/sweeps/||')
        configs+=("$rel_path")
        config_paths+=("$file")
    done < <(find config/sweeps -name "*.json" -type f -print0 2>/dev/null | sort -z)
    
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
            SWEEP_CONFIG="${config_paths[$((choice-1))]}"
            break
        else
            print_error "Invalid selection. Please enter a number between 1 and ${#configs[@]}, or 'q' to quit."
        fi
    done
    
    # Check if config file already specifies a target
    if command -v jq &> /dev/null && [ -f "$SWEEP_CONFIG" ]; then
        config_target=$(jq -r '.target // empty' "$SWEEP_CONFIG" 2>/dev/null)
        if [ -n "$config_target" ] && [[ "${!TARGETS[@]}" =~ $config_target ]]; then
            SIMULATION_TARGET="$config_target"
            print_info "Auto-detected target from config: $SIMULATION_TARGET - ${TARGETS[$SIMULATION_TARGET]}"
        fi
    elif [ -f "$SWEEP_CONFIG" ]; then
        # Fallback: simple grep-based detection if jq is not available
        config_target=$(grep -o '"target"[[:space:]]*:[[:space:]]*"[^"]*"' "$SWEEP_CONFIG" 2>/dev/null | sed 's/.*"target"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/')
        if [ -n "$config_target" ] && [[ "${!TARGETS[@]}" =~ $config_target ]]; then
            SIMULATION_TARGET="$config_target"
            print_info "Auto-detected target from config: $SIMULATION_TARGET - ${TARGETS[$SIMULATION_TARGET]}"
        fi
    fi
    
    # Ask for simulation target only if not auto-detected
    if [ -z "$SIMULATION_TARGET" ]; then
        echo ""
        echo "Available simulation targets:"
        echo ""
        target_list=()
        for target in "${!TARGETS[@]}"; do
            target_list+=("$target")
        done
        
        # Sort targets for consistent display
        IFS=$'\n' sorted_targets=($(sort <<<"${target_list[*]}"))
        unset IFS
        
        for i in "${!sorted_targets[@]}"; do
            target="${sorted_targets[$i]}"
            printf "%2d) %-12s - %s\n" $((i+1)) "$target" "${TARGETS[$target]}"
        done
        echo ""
        
        # Get target selection
        while true; do
            echo -n "Select simulation target (1-${#sorted_targets[@]}) or 's' to skip (default: sim): "
            read -r target_choice
            
            if [ -z "$target_choice" ] || [ "$target_choice" = "s" ] || [ "$target_choice" = "S" ]; then
                SIMULATION_TARGET="sim"
                break
            fi
            
            if [[ "$target_choice" =~ ^[0-9]+$ ]] && [ "$target_choice" -ge 1 ] && [ "$target_choice" -le ${#sorted_targets[@]} ]; then
                SIMULATION_TARGET="${sorted_targets[$((target_choice-1))]}"
                break
            else
                print_error "Invalid selection. Please enter a number between 1 and ${#sorted_targets[@]}, or 's' to skip."
            fi
        done
    fi
    
    # Ask for optional batch name
    echo ""
    echo -n "Enter optional batch name (or press Enter to use default): "
    read -r BATCH_NAME
    
    echo ""
    print_info "Selected: $selected_config"
    print_info "Target: $SIMULATION_TARGET - ${TARGETS[$SIMULATION_TARGET]}"
    if [ -n "$BATCH_NAME" ]; then
        print_info "Batch name: $BATCH_NAME"
    fi
    echo ""
    
else
    # Command-line mode (backward compatibility)
    SWEEP_CONFIG="$1"
    BATCH_NAME="${2:-}"
    SIMULATION_TARGET="${3:-sim}"  # Default to sim if not specified
    
    # Validate target
    if [[ ! "${!TARGETS[@]}" =~ $SIMULATION_TARGET ]]; then
        print_error "Invalid target: $SIMULATION_TARGET"
        print_info "Available targets: ${!TARGETS[*]}"
        exit 1
    fi
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
        # Search recursively in subdirectories
        found_file=$(find config/sweeps -name "${SWEEP_CONFIG}*.json" -type f | head -1)
        if [ -n "$found_file" ]; then
            SWEEP_CONFIG="$found_file"
        else
            print_error "Sweep config file not found: $1"
            print_info "Available configs in config/sweeps/:"
            if [ -d "config/sweeps" ]; then
                find config/sweeps -name "*.json" -type f | sed 's|config/sweeps/||' | sort || echo "  (none found)"
            fi
            exit 1
        fi
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

# Run the Python sweep script with target parameter
if [ -n "$BATCH_NAME" ]; then
    python3 run_sweep.py "$SWEEP_CONFIG" "$BATCH_NAME" "$SIMULATION_TARGET"
else
    python3 run_sweep.py "$SWEEP_CONFIG" "" "$SIMULATION_TARGET"
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