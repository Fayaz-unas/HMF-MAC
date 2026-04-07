#!/bin/bash

# Function to extract a specific stat value safely using awk
extract_stat() {
    local file=$1
    local stat_name=$2
    # Grep the exact stat name, then print the second column (the number)
    local value=$(grep -w "$stat_name" "$file" | awk '{print $2}')
    
    # If the value is empty (stat not found), return 0
    if [ -z "$value" ]; then
        echo "0"
    else
        echo "$value"
    fi
}

print_stats() {
    local stats_file=$1
    local title=${2:-"L2 CACHE STATISTICS"}

    # Check if the file exists
    if [ ! -f "$stats_file" ]; then
        echo "Error: Stats file '$stats_file' not found!"
        return 1
    fi

    # Extract the exact L2 metrics we care about
    l2_hits=$(extract_stat "$stats_file" "system.l2.demandHits::cpu.data")
    l2_misses=$(extract_stat "$stats_file" "system.l2.demandMisses::cpu.data")
    l2_miss_rate=$(extract_stat "$stats_file" "system.l2.demandMissRate::cpu.data")
    l2_replacements=$(extract_stat "$stats_file" "system.l2.replacements")
    l2_writebacks=$(extract_stat "$stats_file" "system.l2.writebacks::total")

    # Print the formatted ASCII table
    echo "=========================================================="
    printf "  %-54s\n" "$title"
    echo "=========================================================="
    printf "| %-35s | %-14s |\n" "Metric" "Value"
    echo "|-------------------------------------|----------------|"
    printf "| %-35s | %-14s |\n" "L2 Data Demand Hits" "$l2_hits"
    printf "| %-35s | %-14s |\n" "L2 Data Demand Misses" "$l2_misses"
    printf "| %-35s | %-14s |\n" "L2 Data Miss Rate" "$l2_miss_rate"
    printf "| %-35s | %-14s |\n" "L2 Total Replacements (Evictions)" "$l2_replacements"
    printf "| %-35s | %-14s |\n" "L2 Total Writebacks to Main Mem" "$l2_writebacks"
    echo "=========================================================="
    echo ""
}

# Check if the user provided a file argument
if [ -z "$1" ]; then
    echo "Usage: ./show_stats.sh <path_to_stats.txt> [Custom Title]"
    exit 1
fi

# Run the function with the provided arguments
print_stats "$1" "$2"
