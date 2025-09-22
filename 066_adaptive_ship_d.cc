#include <vector>
#include <cstdint>
#include <iostream>
#include <cmath>
#include "../inc/champsim_crc2.h"

#define NUM_CORE            1
#define LLC_SETS            (NUM_CORE * 2048)
#define LLC_WAYS            16

// RRPV: 2‐bit (0..3)
#define MAX_RRPV            3

// Signature table parameters
#define SIG_TABLE_SIZE      2048

// SHCT: 2‐bit saturating counters (0..3)
#define SHCT_INIT           2     // weakly hot
#define SHCT_MAX            3
#define SHCT_THRESHOLD      2     // predicts hot

// STCT: 2‐bit stream counters (0..3)
#define STCT_INIT           1     // weakly neutral
#define STCT_MAX            3

// Epoch and adaptation parameters
#define EPOCH_LENGTH        100000       // in accesses
#define PHASE_CHANGE_DELTA  0.05         // 5% change triggers phase reset
#define STREAM_LOW_RATIO    0.10         // <10% hit⇒make stricter
#define STREAM_HIGH_RATIO   0.70         // >70% hit⇒make more lenient

static const uint64_t INVALID_BLK = (uint64_t)-1;

// Replacement state per line
static uint8_t   RRPV          [LLC_SETS][LLC_WAYS];
static bool      valid_entries [LLC_SETS][LLC_WAYS];
static uint32_t  sig_mem       [LLC_SETS][LLC_WAYS];
static bool      has_hit       [LLC_SETS][LLC_WAYS];
static bool      is_streaming  [LLC_SETS][LLC_WAYS];

// Signature tables
static uint8_t   SHCT[SIG_TABLE_SIZE];
static uint8_t   STCT[SIG_TABLE_SIZE];
static uint64_t  last_addr[SIG_TABLE_SIZE];

// Dynamic thresholds & epoch stats
static uint8_t   g_STCT_THRESHOLD;
static uint64_t  stat_hits, stat_misses;
static uint64_t  epoch_accesses, epoch_hits, epoch_misses;
static uint64_t  stream_inserts, stream_hits, stream_misses;
static double    prev_miss_rate;

// Hash PC to signature
static inline uint32_t GetSignature(uint64_t PC) {
    return (uint32_t)((PC ^ (PC >> 12) ^ (PC >> 20)) & (SIG_TABLE_SIZE - 1));
}

// Victim selection via RRIP
static uint32_t FindVictimWay(uint32_t set) {
    for (uint32_t w = 0; w < LLC_WAYS; w++)
        if (!valid_entries[set][w]) return w;
    while (true) {
        for (uint32_t w = 0; w < LLC_WAYS; w++)
            if (RRPV[set][w] == MAX_RRPV) return w;
        for (uint32_t w = 0; w < LLC_WAYS; w++)
            if (RRPV[set][w] < MAX_RRPV)
                RRPV[set][w]++;
    }
}

// Called at end of each epoch to adapt thresholds & detect phase
static void MonitorEpoch() {
    double cur_mr = epoch_accesses ? (double)epoch_misses / epoch_accesses : 0.0;
    // Phase change?
    if (std::fabs(cur_mr - prev_miss_rate) >= PHASE_CHANGE_DELTA) {
        for (uint32_t i = 0; i < SIG_TABLE_SIZE; i++) {
            STCT[i] = STCT_INIT;
            last_addr[i] = INVALID_BLK;
        }
    }
    // Adjust streaming threshold
    double stream_ratio = stream_inserts ? (double)stream_hits / stream_inserts : 0.0;
    if (stream_ratio < STREAM_LOW_RATIO && g_STCT_THRESHOLD < STCT_MAX) {
        g_STCT_THRESHOLD++;
    } else if (stream_ratio > STREAM_HIGH_RATIO && g_STCT_THRESHOLD > 1) {
        g_STCT_THRESHOLD--;
    }
    // Reset epoch stats
    prev_miss_rate  = cur_mr;
    epoch_accesses = epoch_hits = epoch_misses = 0;
    stream_inserts = stream_hits = stream_misses = 0;
}

void InitReplacementState() {
    stat_hits   = stat_misses = 0;
    epoch_accesses = epoch_hits = epoch_misses = 0;
    stream_inserts = stream_hits = stream_misses = 0;
    prev_miss_rate = 0.0;
    g_STCT_THRESHOLD = (uint8_t)STCT_INIT + 1; // initialize to 2
    for (uint32_t s = 0; s < LLC_SETS; s++) {
        for (uint32_t w = 0; w < LLC_WAYS; w++) {
            valid_entries[s][w] = false;
            RRPV[s][w]          = MAX_RRPV;
            sig_mem[s][w]       = 0;
            has_hit[s][w]       = false;
            is_streaming[s][w]  = false;
        }
    }
    for (uint32_t i = 0; i < SIG_TABLE_SIZE; i++) {
        SHCT[i]      = SHCT_INIT;
        STCT[i]      = STCT_INIT;
        last_addr[i] = INVALID_BLK;
    }
}

uint32_t GetVictimInSet(
    uint32_t        cpu,
    uint32_t        set,
    const BLOCK    *current_set,
    uint64_t        PC,
    uint64_t        paddr,
    uint32_t        type
) {
    // Stream detection update (on miss allocation)
    uint32_t sig      = GetSignature(PC);
    uint64_t blk_addr = paddr >> 6;
    if (last_addr[sig] != INVALID_BLK) {
        int64_t delta = (int64_t)blk_addr - (int64_t)last_addr[sig];
        if (delta == 1 || delta == -1) {
            if (STCT[sig] < STCT_MAX) STCT[sig]++;
        } else {
            if (STCT[sig] > 0) STCT[sig]--;
        }
    }
    last_addr[sig] = blk_addr;

    uint32_t way = FindVictimWay(set);

    // Eviction penalties & streaming miss count
    if (valid_entries[set][way]) {
        uint32_t old_sig = sig_mem[set][way];
        if (!has_hit[set][way] && SHCT[old_sig] > 0) {
            SHCT[old_sig]--;
        }
        if (is_streaming[set][way] && !has_hit[set][way]) {
            stream_misses++;
        }
    }

    // Install new line
    sig_mem[set][way]       = sig;
    valid_entries[set][way] = true;
    has_hit[set][way]       = false;
    bool stream = (STCT[sig] >= g_STCT_THRESHOLD);
    is_streaming[set][way]  = stream;
    if (stream) stream_inserts++;
    // Three‐tier insertion
    if (SHCT[sig] >= SHCT_THRESHOLD) {
        RRPV[set][way] = 0;
    } else if (stream) {
        RRPV[set][way] = 1;
    } else {
        RRPV[set][way] = MAX_RRPV;
    }
    return way;
}

void UpdateReplacementState(
    uint32_t cpu,
    uint32_t set,
    uint32_t way,
    uint64_t paddr,
    uint64_t PC,
    uint64_t victim_addr,
    uint32_t type,
    uint8_t  hit
) {
    // Global stats
    epoch_accesses++;
    if (hit) {
        stat_hits++;
        epoch_hits++;
        has_hit[set][way] = true;
        RRPV[set][way]    = 0;
        uint32_t sig = sig_mem[set][way];
        if (SHCT[sig] < SHCT_MAX) SHCT[sig]++;
        if (is_streaming[set][way]) stream_hits++;
    } else {
        stat_misses++;
        epoch_misses++;
    }
    // Epoch boundary
    if (epoch_accesses >= EPOCH_LENGTH) {
        MonitorEpoch();
    }
}

void PrintStats() {
    uint64_t total = stat_hits + stat_misses;
    double   hr    = total ? (100.0 * stat_hits / total) : 0.0;
    std::cout << "==== Adaptive SHiP-D Policy Stats ====\n"
              << "Hits:         " << stat_hits   << "\n"
              << "Misses:       " << stat_misses << "\n"
              << "HitRate:      " << hr << "%\n"
              << "STCT_Thresh:  " << (int)g_STCT_THRESHOLD << "\n";
}

void PrintStats_Heartbeat() {
    // no periodic output
}