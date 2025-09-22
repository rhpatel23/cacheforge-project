Here's just the README content to copy and paste:

```markdown
# CacheForge: LLM-Guided Cache Replacement Policy Discovery

**CSC 491: Generative AI for Systems (Fall 2025)**  
**Author:** Rishabh Patel  
**Institution:** NC State University

## Overview

This project uses large language models to automatically discover high-performance cache replacement policies through evolutionary search. Our best policy, **Adaptive SHiP-D**, achieves 49.47% hit rate and 0.31170 mean IPC across five representative workloads.

## Key Results

- **Performance**: 49.47% hit rate (14.2% improvement over best baseline)
- **Competition Metric**: 0.31170 mean Instructions Per Cycle (IPC)
- **Storage**: 65.1 KiB overhead (only 1.7% over 64 KiB budget)
- **Efficiency**: 25 iterations achieve superior results compared to 100 iterations
- **Model Architecture**: Reasoning models vastly outperform standard completion models

## Repository Contents

| File | Description |
|------|-------------|
| `066_adaptive_ship_d.cc` | Best performing cache replacement policy |
| `run_loop.py` | Main evolutionary search script |
| `PromptGenerator.py` | LLM prompt generation system |
| `RAG.py` | Retrieval-augmented generation components |
| `DB_Connection.ipynb` | Database analysis and visualization |
| `environment.yml` | Conda environment specification |
| `reproduce.sh` | Script to reproduce experimental results |

## Quick Start

1. **Set up environment:**
   ```bash
   conda env create -f environment.yml
   conda activate cacheforge
   ```

2. **Set OpenAI API key:**
   ```bash
   export OPENAI_API_KEY="your-key-here"
   ```

3. **Reproduce results:**
   ```bash
   chmod +x reproduce.sh
   ./reproduce.sh
   ```

## Adaptive SHiP-D Policy Details

Our best policy combines multiple advanced techniques:

- **Signature-based Hit Prediction (SHiP)**: Uses program counter signatures to predict cache line reuse
- **Re-reference Interval Prediction (RRIP)**: Categorizes cache lines by predicted reuse distance
- **Dynamic Threshold Adaptation**: Automatically adjusts streaming detection sensitivity
- **Stream-aware Insertion**: Different insertion policies for streaming vs. non-streaming data
- **Epoch-based Phase Detection**: Resets predictor state when workload phases change

## Experimental Results

### Performance Comparison
| Policy | Hit Rate | IPC | Improvement |
|--------|----------|-----|-------------|
| Adaptive SHiP-D | 49.47% | 0.31170 | +14.2% |
| Less is More | 43.33% | - | Baseline |
| LRU | 41.69% | - | - |

### Ablation Studies
1. **Iteration Scaling**: 25 iterations vs 100 iterations
2. **Model Architecture**: Reasoning models vs temperature-controlled models

### Storage Analysis
- **Total overhead**: 65.1 KiB (1.7% over budget)
- **Pruning strategy**: Can be reduced to 57.1 KiB with minimal performance impact
- **Budget compliance**: Achieved through 32-bit address optimization

## Reproduction Instructions

The `reproduce.sh` script automates the complete experimental pipeline:

1. Dependency checking (Python3, g++)
2. Environment setup
3. CacheForge evolutionary search execution
4. Results validation

Expected runtime: ~2-4 hours depending on hardware and API response times.
