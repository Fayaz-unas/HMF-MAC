# HMF-MAC: Real-Time Adaptive Cache Architecture

HMF-MAC is a real-time adaptive cache architecture implemented within the gem5 simulator. It dynamically detects memory access patterns and adjusts its replacement and victim selection policies to optimize performance for different workloads.

## Features

- **Dynamic Pattern Detection**: Identifies four primary memory access patterns in real-time:
  - `STREAMING`: Sequential access with small strides.
  - `STRIDED`: Regular access with larger strides.
  - `CONFLICT_HEAVY`: High frequency of conflict misses.
  - `RANDOM`: Unpredictable access patterns.
- **Adaptive Victim Selection**: Switches between specialized selection logic based on the detected pattern (e.g., using reference counts for conflict-heavy workloads).
- **Integrated with gem5**: Built directly into the gem5 "classic" memory system.

## Project Structure

- `src/mem/cache/adaptive_policy.hh`: Core logic for pattern detection and adaptive policy state.
- `src/mem/cache/tags/base_set_assoc.hh`: Integration of the adaptive policy into the set-associative tag storage.
- `src/mem/cache/programs/`: Test workloads generating various memory patterns:
  - `random.c`: Random memory access.
  - `sequential.c`: Linear memory access.
  - `strided16.c` / `strided1024.c`: Access with fixed strides.
- `show_stats.sh`: Utility script to parse gem5 `stats.txt` files and display L2 cache performance metrics in a table.

## Usage

### Running Simulations
After compiling gem5 with these changes, run your simulation as usual. The adaptive policy is enabled by default in `BaseSetAssoc`.

### Analyzing Results
Use the provided `show_stats.sh` script to view L2 cache statistics:

```bash
./show_stats.sh m5out/stats.txt "My Simulation Run"
```

The script will output a table similar to:
```text
==========================================================
  L2 CACHE STATISTICS
==========================================================
| Metric                              | Value          |
|-------------------------------------|----------------|
| L2 Data Demand Hits                 | 12345          |
| L2 Data Demand Misses               | 678            |
| L2 Data Miss Rate                   | 0.052          |
| L2 Total Replacements (Evictions)   | 432            |
| L2 Total Writebacks to Main Mem     | 210            |
==========================================================
```

## Implementation Details

The `AdaptivePolicy` class tracks the "streak" of strides and the frequency of conflict misses. 
- A `stride_streak > 4` triggers `STREAMING` or `STRIDED` modes.
- High `conflict_misses` (tracked via `is_conflict` flag on misses) triggers `CONFLICT_HEAVY` mode.
- In `CONFLICT_HEAVY` mode, the cache prefers evicting blocks with the lowest reference count.
