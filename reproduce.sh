#!/bin/bash
# Reproduce Adaptive SHiP-D cache replacement policy results
# CSC 491 - Generative AI for Systems
# Author: Rishabh Patel

echo "=== CacheForge Reproduction Script ==="
echo "Reproducing Adaptive SHiP-D policy generation and evaluation"

# Check dependencies
if ! command -v python3 &> /dev/null; then
    echo "Error: Python3 not found"
    exit 1
fi

if ! command -v g++ &> /dev/null; then
    echo "Error: g++ compiler not found"
    exit 1
fi

# Set up environment
echo "Setting up environment..."
export WARMUP_INST="1000000"
export SIM_INST="10000000" 
export MODEL="o4-mini"
export ITERATIONS="25"

# Check for API key
if [ -z "$OPENAI_API_KEY" ]; then
    echo "Warning: OPENAI_API_KEY not set. Cannot run full reproduction."
    echo "To reproduce results, set your OpenAI API key:"
    echo "export OPENAI_API_KEY='your-key-here'"
    echo ""
    echo "However, you can test the best policy directly:"
    echo "The Adaptive SHiP-D policy (066_adaptive_ship_d.cc) is included."
    exit 1
fi

# Run the main experiment
echo "Running CacheForge evolutionary search..."
python3 run_loop.py

echo "=== Reproduction Complete ==="
echo "Best policy: Adaptive SHiP-D"
echo "Expected results:"
echo "- Hit rate: 49.47%"
echo "- Mean IPC: 0.31170"
echo "- Storage overhead: 65.1 KiB"
