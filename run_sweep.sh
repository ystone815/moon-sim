#!/bin/bash

# SystemC Parameter Sweep Test Runner
# Usage: ./run_sweep.sh <sweep_config_file> [batch_name]
# sweep_config_file: JSON file defining the parameter sweep
# batch_name: Optional custom batch name (default: sweep_timestamp)

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Check arguments
if [ $# -lt 1 ]; then
    echo -e "${RED}Usage: $0 <sweep_config_file> [batch_name]${NC}"
    echo -e "${YELLOW}Example: $0 config/sweeps/write_ratio_sweep.json my_sweep${NC}"
    exit 1
fi

SWEEP_CONFIG="$1"
if [ ! -f "$SWEEP_CONFIG" ]; then
    echo -e "${RED}Error: Sweep config file '$SWEEP_CONFIG' not found${NC}"
    exit 1
fi

# Generate batch name
if [ $# -ge 2 ]; then
    BATCH_NAME="$2"
else
    SWEEP_NAME=$(basename "$SWEEP_CONFIG" .json)
    BATCH_NAME="${SWEEP_NAME}"
fi

# Add timestamp to batch name
BATCH_WITH_TIME="${BATCH_NAME}_$(date +%Y%m%d_%H%M%S)"

# Create sweep config directory and results directory
SWEEP_CONFIG_DIR="config/sweeps/${BATCH_WITH_TIME}"
SWEEP_RESULTS_DIR="regression_runs/${BATCH_WITH_TIME}"
mkdir -p "$SWEEP_CONFIG_DIR"
mkdir -p "$SWEEP_RESULTS_DIR"

echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}SystemC Parameter Sweep Test${NC}"
echo -e "${CYAN}========================================${NC}"
echo -e "${BLUE}Sweep Config: ${SWEEP_CONFIG}${NC}"
echo -e "${BLUE}Batch Name: ${BATCH_WITH_TIME}${NC}"
echo -e "${BLUE}Config Directory: ${SWEEP_CONFIG_DIR}${NC}"
echo -e "${BLUE}Results Directory: ${SWEEP_RESULTS_DIR}${NC}"
echo -e "${CYAN}========================================${NC}"

# Parse sweep configuration (simple JSON parsing for our specific format)
get_json_value() {
    grep "\"$1\"" "$SWEEP_CONFIG" | sed 's/.*"'"$1"'"[[:space:]]*:[[:space:]]*"\?\([^",]*\)"\?.*/\1/' | tr -d ' '
}

# Extract sweep parameters
BASE_CONFIG=$(get_json_value "base_config")
PARAMETER=$(get_json_value "parameter")
START_VALUE=$(get_json_value "start")
END_VALUE=$(get_json_value "end")
STEP_VALUE=$(get_json_value "step")
CONFIG_FILE=$(get_json_value "config_file")

echo -e "${YELLOW}Sweep Parameters:${NC}"
echo -e "  Base Config: ${BASE_CONFIG}"
echo -e "  Parameter: ${PARAMETER}"
echo -e "  Range: ${START_VALUE} to ${END_VALUE} (step ${STEP_VALUE})"
echo -e "  Target File: ${CONFIG_FILE}"
echo ""

# Build the simulator first
echo -e "${YELLOW}Building simulator...${NC}"
make clean > /dev/null 2>&1
if ! make > /dev/null 2>&1; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}Build successful${NC}"
echo ""

# Copy original sweep config to both directories
cp "$SWEEP_CONFIG" "$SWEEP_CONFIG_DIR/"
cp "$SWEEP_CONFIG" "$SWEEP_RESULTS_DIR/"

# Generate all test case configs first
echo -e "${YELLOW}Generating test case configurations...${NC}"
tc_count=0
current_value=$START_VALUE
while [ $current_value -le $END_VALUE ]; do
    tc_count=$((tc_count + 1))
    tc_name=$(printf "TC%03d" $tc_count)
    tc_config_dir="${SWEEP_CONFIG_DIR}/${tc_name}"
    
    echo -e "  ${tc_name}: ${PARAMETER}=${current_value}"
    
    # Create TC config directory
    mkdir -p "$tc_config_dir"
    
    # Copy base config files
    cp -r "${BASE_CONFIG}"/* "$tc_config_dir/"
    
    # Modify the target parameter in the specified config file
    target_file="${tc_config_dir}/${CONFIG_FILE}"
    if [ -f "$target_file" ]; then
        # Replace the parameter value using sed
        sed -i "s/\"${PARAMETER}\"[[:space:]]*:[[:space:]]*[0-9]*/\"${PARAMETER}\": ${current_value}/" "$target_file"
        
        # Create TC description file
        cat > "${tc_config_dir}/TC_INFO.txt" << EOF
Test Case: ${tc_name}
Parameter: ${PARAMETER}
Value: ${current_value}
Batch: ${BATCH_WITH_TIME}
Generated: $(date)
Base Config: ${BASE_CONFIG}
Modified File: ${CONFIG_FILE}
EOF
    else
        echo -e "${RED}Warning: Target config file '${CONFIG_FILE}' not found${NC}"
    fi
    
    current_value=$((current_value + STEP_VALUE))
done

echo -e "${GREEN}Generated ${tc_count} test case configurations in ${SWEEP_CONFIG_DIR}${NC}"
echo ""

# Initialize results tracking
passed=0
failed=0
declare -a results

# Function to run a single test case
run_test_case() {
    local tc_number=$1
    local value=$2
    local tc_name=$(printf "TC%03d" $tc_number)
    local tc_config_dir="${SWEEP_CONFIG_DIR}/${tc_name}"
    local tc_results_dir="${SWEEP_RESULTS_DIR}/${tc_name}"
    
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Running ${tc_name}: ${PARAMETER}=${value}${NC}"
    echo -e "${BLUE}Config: ${tc_config_dir}${NC}"
    echo -e "${BLUE}Results: ${tc_results_dir}${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    # Check if config directory exists
    if [ ! -d "$tc_config_dir" ]; then
        echo -e "${RED}Error: Test case config directory '${tc_config_dir}' not found${NC}"
        return 1
    fi
    
    # Create results directory
    mkdir -p "$tc_results_dir"
    
    # Copy config to results directory for reference
    cp -r "$tc_config_dir"/* "$tc_results_dir/"
    
    # Run the simulation
    start_time=$(date +%s)
    
    # Clear existing logs before test
    rm -f log/simulation_*.log
    
    if timeout 30 ./sim "$tc_config_dir"; then
        end_time=$(date +%s)
        duration=$((end_time - start_time))
        
        # Move logs to results directory
        if ls log/simulation_*.log 1> /dev/null 2>&1; then
            mv log/simulation_*.log "$tc_results_dir/"
        fi
        
        echo -e "${GREEN}✓ ${tc_name} PASSED (${duration}s)${NC}"
        
        # Extract performance metrics
        latest_log=$(ls -t "${tc_results_dir}"/simulation_*.log | head -1 2>/dev/null)
        if [ -f "$latest_log" ]; then
            throughput=$(grep "Throughput:" "$latest_log" | sed 's/.*Throughput: \([0-9]*\).*/\1/')
            sim_time=$(grep "Simulation time:" "$latest_log" | sed 's/.*Simulation time: \([0-9]*\).*/\1/')
            
            echo -e "${YELLOW}Performance: ${throughput} tps, ${sim_time} ms${NC}"
            
            # Store result for analysis
            results+=("${tc_name},${value},${throughput},${sim_time},PASSED")
        else
            results+=("${tc_name},${value},0,0,PASSED")
        fi
        
        # Create test case summary
        cat > "${tc_results_dir}/TC_RESULT.txt" << EOF
Test Case: ${tc_name}
Parameter: ${PARAMETER} = ${value}
Batch: ${BATCH_WITH_TIME}
Status: PASSED
Duration: ${duration}s
Timestamp: $(date)
Config: ${tc_config_dir}
Results: ${tc_results_dir}

Performance Summary:
$(tail -5 "$latest_log" | grep -E "(Total transactions|Simulation time|Throughput)" || echo "No performance data found")
EOF
        
        echo -e "${BLUE}DEBUG: TC${tc_number} completed successfully, returning 0${NC}"
        return 0
    else
        end_time=$(date +%s)
        duration=$((end_time - start_time))
        
        # Move logs to results directory even on failure
        if ls log/simulation_*.log 1> /dev/null 2>&1; then
            mv log/simulation_*.log "$tc_results_dir/"
        fi
        
        echo -e "${RED}✗ ${tc_name} FAILED${NC}"
        
        results+=("${tc_name},${value},0,0,FAILED")
        
        # Create failure summary
        cat > "${tc_results_dir}/TC_RESULT.txt" << EOF
Test Case: ${tc_name}
Parameter: ${PARAMETER} = ${value}
Batch: ${BATCH_WITH_TIME}
Status: FAILED
Duration: ${duration}s
Timestamp: $(date)
Config: ${tc_config_dir}
Results: ${tc_results_dir}
EOF
        
        return 1
    fi
}

# Run sweep
echo -e "${YELLOW}Running test cases...${NC}"
tc_number=1
current_value=$START_VALUE
while [ $current_value -le $END_VALUE ]; do
    echo -e "${CYAN}DEBUG: About to run TC${tc_number} with value ${current_value}${NC}"
    if run_test_case $tc_number $current_value; then
        passed=$((passed + 1))
        echo -e "${CYAN}DEBUG: TC${tc_number} passed, incrementing counters${NC}"
    else
        failed=$((failed + 1))
        echo -e "${CYAN}DEBUG: TC${tc_number} failed, incrementing counters${NC}"
    fi
    echo ""
    
    echo -e "${CYAN}DEBUG: Before increment - tc_number=$tc_number, current_value=$current_value${NC}"
    tc_number=$((tc_number + 1))
    current_value=$((current_value + STEP_VALUE))
    echo -e "${CYAN}DEBUG: After increment - tc_number=$tc_number, current_value=$current_value${NC}"
    echo -e "${CYAN}DEBUG: Loop condition check: $current_value -le $END_VALUE = $([ $current_value -le $END_VALUE ] && echo true || echo false)${NC}"
done

# Generate sweep analysis
echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}Sweep Analysis${NC}"
echo -e "${CYAN}========================================${NC}"

# Create detailed results file
cat > "${SWEEP_RESULTS_DIR}/sweep_results.csv" << EOF
TestCase,${PARAMETER},Throughput_TPS,SimTime_MS,Status
EOF

for result in "${results[@]}"; do
    echo "$result" >> "${SWEEP_RESULTS_DIR}/sweep_results.csv"
done

# Create comprehensive summary
cat > "${SWEEP_RESULTS_DIR}/sweep_summary.txt" << EOF
Parameter Sweep Summary
=======================
Sweep Name: ${PARAMETER}
Batch: ${BATCH_WITH_TIME}
Timestamp: $(date)
Base Config: ${BASE_CONFIG}
Parameter Range: ${START_VALUE} to ${END_VALUE} (step ${STEP_VALUE})
Total Test Cases: $((passed + failed))
Passed: $passed
Failed: $failed

Config Directory: ${SWEEP_CONFIG_DIR}
Results Directory: ${SWEEP_RESULTS_DIR}

Test Case Results:
EOF

# Add performance analysis
echo "Parameter vs Performance:" >> "${SWEEP_RESULTS_DIR}/sweep_summary.txt"
while IFS=',' read -r tc_name param throughput simtime status; do
    if [ "$tc_name" != "TestCase" ] && [ "$status" = "PASSED" ]; then
        printf "  %s (%s=%s): %s tps (%s ms)\n" "$tc_name" "$PARAMETER" "$param" "$throughput" "$simtime" >> "${SWEEP_RESULTS_DIR}/sweep_summary.txt"
    fi
done < "${SWEEP_RESULTS_DIR}/sweep_results.csv"

# Create reproduction script
cat > "${SWEEP_RESULTS_DIR}/reproduce_sweep.sh" << EOF
#!/bin/bash
# Reproduction script for sweep batch: ${BATCH_WITH_TIME}
# Generated: $(date)

echo "Reproducing sweep: ${BATCH_WITH_TIME}"
echo "Config directory: ${SWEEP_CONFIG_DIR}"

# Run individual test cases
EOF

tc_number=1
current_value=$START_VALUE
while [ $current_value -le $END_VALUE ]; do
    tc_name=$(printf "TC%03d" $tc_number)
    echo "./sim ${SWEEP_CONFIG_DIR}/${tc_name}  # ${PARAMETER}=${current_value}" >> "${SWEEP_RESULTS_DIR}/reproduce_sweep.sh"
    tc_number=$((tc_number + 1))
    current_value=$((current_value + STEP_VALUE))
done

chmod +x "${SWEEP_RESULTS_DIR}/reproduce_sweep.sh"

# Display results
echo -e "${BLUE}Parameter: ${PARAMETER}${NC}"
echo -e "${BLUE}Range: ${START_VALUE} to ${END_VALUE} (step ${STEP_VALUE})${NC}"
echo -e "${GREEN}Passed: $passed${NC}"
echo -e "${RED}Failed: $failed${NC}"
echo -e "${BLUE}Total Test Cases: $((passed + failed))${NC}"

echo -e "${YELLOW}Performance Summary:${NC}"
while IFS=',' read -r tc_name param throughput simtime status; do
    if [ "$tc_name" != "TestCase" ] && [ "$status" = "PASSED" ]; then
        printf "  %s (%s=%s): %s tps (%s ms)\n" "$tc_name" "$PARAMETER" "$param" "$throughput" "$simtime"
    fi
done < "${SWEEP_RESULTS_DIR}/sweep_results.csv"

if [ $failed -eq 0 ]; then
    echo -e "${GREEN}All test cases passed! 🎉${NC}"
    echo "Overall Status: PASSED" >> "${SWEEP_RESULTS_DIR}/sweep_summary.txt"
    exit_code=0
else
    echo -e "${RED}Some test cases failed! 😞${NC}"
    echo "Overall Status: FAILED" >> "${SWEEP_RESULTS_DIR}/sweep_summary.txt"
    exit_code=1
fi

echo ""
echo -e "${CYAN}========================================${NC}"
echo -e "${CYAN}Sweep Completed${NC}"
echo -e "${CYAN}========================================${NC}"
echo -e "${YELLOW}Config Directory: ${SWEEP_CONFIG_DIR}${NC}"
echo -e "${YELLOW}Results Directory: ${SWEEP_RESULTS_DIR}${NC}"
echo -e "${YELLOW}CSV Data: ${SWEEP_RESULTS_DIR}/sweep_results.csv${NC}"
echo -e "${YELLOW}Summary: ${SWEEP_RESULTS_DIR}/sweep_summary.txt${NC}"
echo -e "${YELLOW}Reproduction Script: ${SWEEP_RESULTS_DIR}/reproduce_sweep.sh${NC}"

exit $exit_code