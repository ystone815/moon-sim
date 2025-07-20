#!/bin/bash

# SystemC Regression Test Runner
# Usage: ./run_regression.sh [test_name] [batch_name]
# If no test_name provided, runs all regression tests
# If no batch_name provided, uses timestamp

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test scenarios
TESTS=("baseline" "performance" "stress" "validation")

# Generate batch name (timestamp or user-provided)
if [ $# -ge 2 ]; then
    BATCH_NAME="$2"
else
    BATCH_NAME="regression_$(date +%Y%m%d_%H%M%S)"
fi

# Create regression batch directory
REGRESSION_DIR="regression_runs/${BATCH_NAME}"
mkdir -p "$REGRESSION_DIR"

echo -e "${BLUE}Regression Batch: ${BATCH_NAME}${NC}"
echo -e "${BLUE}Output Directory: ${REGRESSION_DIR}${NC}"

# Function to run a single test
run_test() {
    local test_name=$1
    local config_dir="config/regression/${test_name}"
    local test_output_dir="${REGRESSION_DIR}/${test_name}"
    
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Running ${test_name} test...${NC}"
    echo -e "${BLUE}Config: ${config_dir}${NC}"
    echo -e "${BLUE}Output: ${test_output_dir}${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    # Check if config directory exists
    if [ ! -d "$config_dir" ]; then
        echo -e "${RED}Error: Config directory $config_dir not found${NC}"
        return 1
    fi
    
    # Create test-specific output directory
    mkdir -p "$test_output_dir"
    
    # Copy config files to output directory for reference
    cp -r "$config_dir"/* "$test_output_dir/"
    
    # Run the simulation
    start_time=$(date +%s)
    
    # Clear existing logs before test
    rm -f log/simulation_*.log
    
    if ./sim "$config_dir"; then
        end_time=$(date +%s)
        duration=$((end_time - start_time))
        
        # Move logs to test output directory
        if ls log/simulation_*.log 1> /dev/null 2>&1; then
            mv log/simulation_*.log "$test_output_dir/"
        fi
        
        echo -e "${GREEN}✓ ${test_name} test PASSED (${duration}s)${NC}"
        
        # Show performance summary from moved log
        latest_log=$(ls -t "${test_output_dir}"/simulation_*.log | head -1 2>/dev/null)
        if [ -f "$latest_log" ]; then
            echo -e "${YELLOW}Performance Summary:${NC}"
            tail -5 "$latest_log" | grep -E "(Total transactions|Simulation time|Throughput)" || true
        fi
        
        # Create test summary
        cat > "${test_output_dir}/test_summary.txt" << EOF
Test: ${test_name}
Batch: ${BATCH_NAME}
Status: PASSED
Duration: ${duration}s
Timestamp: $(date)
Config: ${config_dir}

Performance Summary:
$(tail -5 "$latest_log" | grep -E "(Total transactions|Simulation time|Throughput)" || echo "No performance data found")
EOF
        
        return 0
    else
        end_time=$(date +%s)
        duration=$((end_time - start_time))
        
        # Move logs to test output directory even on failure
        if ls log/simulation_*.log 1> /dev/null 2>&1; then
            mv log/simulation_*.log "$test_output_dir/"
        fi
        
        echo -e "${RED}✗ ${test_name} test FAILED${NC}"
        
        # Create failure summary
        cat > "${test_output_dir}/test_summary.txt" << EOF
Test: ${test_name}
Batch: ${BATCH_NAME}
Status: FAILED
Duration: ${duration}s
Timestamp: $(date)
Config: ${config_dir}
EOF
        
        return 1
    fi
}

# Main execution
echo -e "${BLUE}SystemC Simulator Regression Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"

# Build the simulator first
echo -e "${YELLOW}Building simulator...${NC}"
make clean > /dev/null 2>&1
if ! make > /dev/null 2>&1; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi
echo -e "${GREEN}Build successful${NC}"
echo ""

# Determine which tests to run
if [ $# -eq 1 ]; then
    # Run specific test
    test_name=$1
    if [[ " ${TESTS[@]} " =~ " ${test_name} " ]]; then
        run_test "$test_name"
        exit_code=$?
    else
        echo -e "${RED}Error: Unknown test '$test_name'${NC}"
        echo -e "${YELLOW}Available tests: ${TESTS[*]}${NC}"
        exit 1
    fi
else
    # Run all tests
    passed=0
    failed=0
    
    for test in "${TESTS[@]}"; do
        if run_test "$test"; then
            ((passed++))
        else
            ((failed++))
        fi
        echo ""
    done
    
    # Summary
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Regression Test Summary${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo -e "${GREEN}Passed: $passed${NC}"
    echo -e "${RED}Failed: $failed${NC}"
    echo -e "${BLUE}Total:  $((passed + failed))${NC}"
    echo -e "${BLUE}Batch:  ${BATCH_NAME}${NC}"
    echo -e "${BLUE}Output: ${REGRESSION_DIR}${NC}"
    
    # Create overall summary file
    cat > "${REGRESSION_DIR}/regression_summary.txt" << EOF
Regression Batch: ${BATCH_NAME}
Timestamp: $(date)
Total Tests: $((passed + failed))
Passed: $passed
Failed: $failed

Test Results:
EOF
    
    # Add individual test results to summary
    for test in "${TESTS[@]}"; do
        if [ -f "${REGRESSION_DIR}/${test}/test_summary.txt" ]; then
            echo "- ${test}: $(grep 'Status:' "${REGRESSION_DIR}/${test}/test_summary.txt" | cut -d' ' -f2)" >> "${REGRESSION_DIR}/regression_summary.txt"
        fi
    done
    
    if [ $failed -eq 0 ]; then
        echo -e "${GREEN}All tests passed! 🎉${NC}"
        echo "Overall Status: PASSED" >> "${REGRESSION_DIR}/regression_summary.txt"
        exit_code=0
    else
        echo -e "${RED}Some tests failed! 😞${NC}"
        echo "Overall Status: FAILED" >> "${REGRESSION_DIR}/regression_summary.txt"
        exit_code=1
    fi
    
    echo -e "${YELLOW}Detailed results saved to: ${REGRESSION_DIR}${NC}"
fi

exit $exit_code