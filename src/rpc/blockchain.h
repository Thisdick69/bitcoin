// Copyright (c) 2017-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_BLOCKCHAIN_H
#define BITCOIN_RPC_BLOCKCHAIN_H

#include <amount.h>
#include <optional.h>
#include <sync.h>

#include <stdint.h>
#include <vector>

extern RecursiveMutex cs_main;

class CBlock;
class CBlockIndex;
class CTxMemPool;
class ChainstateManager;
class UniValue;
struct NodeContext;
namespace util {
class Ref;
} // namespace util

static constexpr int NUM_GETBLOCKSTATS_PERCENTILES = 5;

/**
 * Get the difficulty of the net wrt to the given block index.
 *
 * @return A floating point number that is a multiple of the main net minimum
 * difficulty (4295032833 hashes).
 */
double GetDifficulty(const CBlockIndex* blockindex);

/** Callback for when block tip changed. */
void RPCNotifyBlockChange(const CBlockIndex*);

/** Block description to JSON */
UniValue blockToJSON(const CBlock& block, const CBlockIndex* tip, const CBlockIndex* blockindex, bool txDetails = false) LOCKS_EXCLUDED(cs_main);

typedef std::vector<CAmount> MempoolHistogramFeeRates;

/* TODO: define log scale formular for dynamically creating the
 * feelimits but with the property of not constantly changing
 * (and thus screw up client implementations) */
static const MempoolHistogramFeeRates MempoolInfoToJSON_const_limits{
    1, 2, 3, 4, 5, 6, 7, 8, 10,
    12, 14, 17, 20, 25, 30, 40, 50, 60, 70, 80, 100,
    120, 140, 170, 200, 250, 300, 400, 500, 600, 700, 800, 1000,
    1200, 1400, 1700, 2000, 2500, 3000, 4000, 5000, 6000, 7000, 8000, 10000};

/** Mempool information to JSON */
UniValue MempoolInfoToJSON(const CTxMemPool& pool, const Optional<MempoolHistogramFeeRates> feeLimits);

/** Mempool to JSON */
UniValue MempoolToJSON(const CTxMemPool& pool, bool verbose = false, bool include_mempool_sequence = false);

/** Block header to JSON */
UniValue blockheaderToJSON(const CBlockIndex* tip, const CBlockIndex* blockindex) LOCKS_EXCLUDED(cs_main);

/** Used by getblockstats to get feerates at different percentiles by weight  */
void CalculatePercentilesByWeight(CAmount result[NUM_GETBLOCKSTATS_PERCENTILES], std::vector<std::pair<CAmount, int64_t>>& scores, int64_t total_weight);

NodeContext& EnsureNodeContext(const util::Ref& context);
CTxMemPool& EnsureMemPool(const util::Ref& context);
ChainstateManager& EnsureChainman(const util::Ref& context);

#endif
