/**
 * @file example_regression.cpp
 * @brief Example for performance regression testing.
 * 
 * This example generates two trace files (baseline and current) with intentional
 * performance differences to demonstrate regression detection.
 * 
 * Usage:
 *   example_regression baseline    # Generates baseline.bin
 *   example_regression current     # Generates current.bin (with regressions)
 *   python tools/trc_analyze.py compare baseline.bin current.bin
 */

#include <trace-scope/trace_scope.hpp>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstring>

// Fast function - stays the same in both versions
void fast_function() {
    TRC_SCOPE();
    volatile int x = 0;
    for (int i = 0; i < 100; ++i) {
        x += i;
    }
}

// Slow function - gets slower in "current" version
void slow_function(bool is_regressed) {
    TRC_SCOPE();
    if (is_regressed) {
        // Regression: 2x slower
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    } else {
        // Baseline: fast
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Memory function - uses more memory in "current" version
void memory_function(bool is_regressed) {
    TRC_SCOPE();
    if (is_regressed) {
        // Regression: 2x more memory
        std::vector<int> big_vec(2 * 1024 * 1024);  // ~8MB
        std::fill(big_vec.begin(), big_vec.end(), 42);
    } else {
        // Baseline: less memory
        std::vector<int> small_vec(1024 * 1024);  // ~4MB
        std::fill(small_vec.begin(), small_vec.end(), 42);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

// Improved function - gets faster in "current" version
void improved_function(bool is_current) {
    TRC_SCOPE();
    if (is_current) {
        // Improvement: 50% faster
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    } else {
        // Baseline: slower
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Function only in current version
void new_function() {
    TRC_SCOPE();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

// Function only in baseline version
void removed_function() {
    TRC_SCOPE();
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::printf("Usage: %s <baseline|current>\n", argv[0]);
        std::printf("\n");
        std::printf("Generates trace files for regression testing:\n");
        std::printf("  baseline - Generates baseline.bin (faster version)\n");
        std::printf("  current  - Generates current.bin (with regressions)\n");
        std::printf("\n");
        std::printf("Then compare:\n");
        std::printf("  python tools/trc_analyze.py compare baseline.bin current.bin\n");
        return 1;
    }
    
    bool is_current = (std::strcmp(argv[1], "current") == 0);
    bool is_baseline = (std::strcmp(argv[1], "baseline") == 0);
    
    if (!is_current && !is_baseline) {
        std::printf("Error: Mode must be 'baseline' or 'current'\n");
        return 1;
    }
    
    std::printf("=======================================================================\n");
    std::printf(" Performance Regression Test - %s\n", argv[1]);
    std::printf("=======================================================================\n\n");
    
    // Configure tracer
    trace::config.mode = trace::TracingMode::Buffered;
    trace::config.out = nullptr;  // Don't print trace output
    trace::config.track_memory = true;  // Track memory for regression detection
    
    TRC_SCOPE();
    
    // Run workload
    for (int i = 0; i < 5; ++i) {
        fast_function();  // Always the same
        slow_function(is_current);  // Regressed in current
        improved_function(is_current);  // Improved in current
        memory_function(is_current);  // More memory in current
        
        // New/removed functions
        if (is_current) {
            new_function();  // Only in current
        } else {
            removed_function();  // Only in baseline
        }
    }
    
    // Generate output file with explicit prefix
    const char* prefix = is_current ? "current" : "baseline";
    std::string filename = trace::dump_binary(prefix);
    if (!filename.empty()) {
        std::printf("✓ Generated %s\n", filename.c_str());
        
        if (is_baseline) {
            std::printf("\nNext steps:\n");
            std::printf("  1. Run: example_regression current\n");
            std::printf("  2. Compare: python tools/trc_analyze.py compare baseline_*.bin current_*.bin\n");
            std::printf("     (use the most recent timestamped files)\n");
        } else {
            std::printf("\nCompare with baseline:\n");
            std::printf("  python tools/trc_analyze.py compare baseline_*.bin current_*.bin\n");
            std::printf("  (use the most recent timestamped files)\n");
            std::printf("\nExpected regressions:\n");
            std::printf("  - slow_function: ~2x slower (100%% increase)\n");
            std::printf("  - memory_function: ~2x more memory\n");
            std::printf("\nExpected improvements:\n");
            std::printf("  - improved_function: ~50%% faster\n");
            std::printf("\nExpected changes:\n");
            std::printf("  + new_function (added)\n");
            std::printf("  - removed_function (removed)\n");
        }
    }
    
    std::printf("\n");
    return 0;
}

