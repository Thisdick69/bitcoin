// Copyright (c) 2011-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/rpc/util.h>

#include <rpc/util.h>
#include <util/translation.h>
#include <util/url.h>
#include <wallet/context.h>
#include <wallet/wallet.h>

#include <univalue.h>

#include <boost/algorithm/string.hpp>  // boost::split

namespace wallet {
static const std::string WALLET_ENDPOINT_BASE = "/wallet/";
const std::string HELP_REQUIRING_PASSPHRASE{"\nRequires wallet passphrase to be set with walletpassphrase call if wallet is encrypted.\n"};

const RPCResult RESULT_LAST_PROCESSED_BLOCK
    {RPCResult::Type::OBJ, "lastprocessedblock", "hash and height of the block this information was generated on", {
        {RPCResult::Type::STR_HEX, "hash", "hash of the block this information was generated on"},
        {RPCResult::Type::NUM, "height", "height of the block this information was generated on"}
    }};

void AppendLastProcessedBlock(UniValue& entry, const CWallet& wallet) EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet)
{
    AssertLockHeld(wallet.cs_wallet);
    UniValue lastprocessedblock{UniValue::VOBJ};
    lastprocessedblock.pushKV("hash", wallet.GetLastBlockHash().GetHex());
    lastprocessedblock.pushKV("height", wallet.GetLastBlockHeight());
    entry.pushKV("lastprocessedblock", lastprocessedblock);
}

bool GetAvoidReuseFlag(const CWallet& wallet, const UniValue& param) {
    bool can_avoid_reuse = wallet.IsWalletFlagSet(WALLET_FLAG_AVOID_REUSE);
    bool avoid_reuse = param.isNull() ? can_avoid_reuse : param.get_bool();

    if (avoid_reuse && !can_avoid_reuse) {
        throw JSONRPCError(RPC_WALLET_ERROR, "wallet does not have the \"avoid reuse\" feature enabled");
    }

    return avoid_reuse;
}

/** Used by RPC commands that have an include_watchonly parameter.
 *  We default to true for watchonly wallets if include_watchonly isn't
 *  explicitly set.
 */
bool ParseIncludeWatchonly(const UniValue& include_watchonly, const CWallet& wallet)
{
    if (include_watchonly.isNull()) {
        // if include_watchonly isn't explicitly set, then check if we have a watchonly wallet
        return wallet.IsWalletFlagSet(WALLET_FLAG_DISABLE_PRIVATE_KEYS);
    }

    // otherwise return whatever include_watchonly was set to
    return include_watchonly.get_bool();
}

bool GetWalletNameFromJSONRPCRequest(const JSONRPCRequest& request, std::string& wallet_name)
{
    if (URL_DECODE && request.URI.substr(0, WALLET_ENDPOINT_BASE.size()) == WALLET_ENDPOINT_BASE) {
        // wallet endpoint was used
        wallet_name = URL_DECODE(request.URI.substr(WALLET_ENDPOINT_BASE.size()));
        return true;
    }
    return false;
}

std::shared_ptr<CWallet> GetWalletForJSONRPCRequest(const JSONRPCRequest& request)
{
    CHECK_NONFATAL(request.mode == JSONRPCRequest::EXECUTE);
    WalletContext& context = EnsureWalletContext(request.context);

    bool have_wallet_restriction;
    std::string authorized_wallet_name;
    have_wallet_restriction = GetWalletRestrictionFromJSONRPCRequest(request, authorized_wallet_name);

    bool have_requested_wallet;
    std::string requested_wallet_name;
    have_requested_wallet = GetWalletNameFromJSONRPCRequest(request, requested_wallet_name);

    std::shared_ptr<CWallet> pwallet;

    if (!have_wallet_restriction) {
        // Any wallet is permitted; select by endpoint, or use the sole wallet
        if (have_requested_wallet) {
            pwallet = GetWallet(context, requested_wallet_name);
        } else {
            std::vector<std::shared_ptr<CWallet>> wallets = GetWallets(context);
            if (wallets.size() == 1) {
                pwallet = wallets[0];
            }
        }
    } else if (authorized_wallet_name == "-") {
        // Block wallet access always
    } else if ((!have_requested_wallet) || requested_wallet_name == authorized_wallet_name) {
        // Select specifically the authorized wallet
        pwallet = GetWallet(context, authorized_wallet_name);
    }

    if (pwallet) {
        return pwallet;
    }

    if (have_requested_wallet) {
        throw JSONRPCError(RPC_WALLET_NOT_FOUND, "Requested wallet does not exist or is not loaded");
    }
    if (have_wallet_restriction
    ? (authorized_wallet_name == "-" || !GetWallet(context, authorized_wallet_name))
    : GetWallets(context).empty()
     ) {
        throw JSONRPCError(
            RPC_WALLET_NOT_FOUND, "No wallet is loaded. Load a wallet using loadwallet or create a new one with createwallet. (Note: A default wallet is no longer automatically created)");
    }
    throw JSONRPCError(RPC_WALLET_NOT_SPECIFIED,
        "Wallet file not specified (must request wallet RPC through /wallet/<filename> uri-path).");
}

void EnsureWalletIsUnlocked(const CWallet& wallet)
{
    if (wallet.IsLocked()) {
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    }
}

WalletContext& EnsureWalletContext(const std::any& context)
{
    auto wallet_context = util::AnyPtr<WalletContext>(context);
    if (!wallet_context) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet context not found");
    }
    return *wallet_context;
}

// also_create should only be set to true only when the RPC is expected to add things to a blank wallet and make it no longer blank
LegacyScriptPubKeyMan& EnsureLegacyScriptPubKeyMan(CWallet& wallet, bool also_create)
{
    LegacyScriptPubKeyMan* spk_man = wallet.GetLegacyScriptPubKeyMan();
    if (!spk_man && also_create) {
        spk_man = wallet.GetOrCreateLegacyScriptPubKeyMan();
    }
    if (!spk_man) {
        throw JSONRPCError(RPC_WALLET_ERROR, "This type of wallet does not support this command");
    }
    return *spk_man;
}

const LegacyScriptPubKeyMan& EnsureConstLegacyScriptPubKeyMan(const CWallet& wallet)
{
    const LegacyScriptPubKeyMan* spk_man = wallet.GetLegacyScriptPubKeyMan();
    if (!spk_man) {
        throw JSONRPCError(RPC_WALLET_ERROR, "This type of wallet does not support this command");
    }
    return *spk_man;
}

std::string LabelFromValue(const UniValue& value)
{
    std::string label = value.get_str();
    if (label == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_LABEL_NAME, "Invalid label name");
    return label;
}

void HandleWalletError(const std::shared_ptr<CWallet> wallet, DatabaseStatus& status, bilingual_str& error)
{
    if (!wallet) {
        // Map bad format to not found, since bad format is returned when the
        // wallet directory exists, but doesn't contain a data file.
        RPCErrorCode code = RPC_WALLET_ERROR;
        switch (status) {
            case DatabaseStatus::FAILED_NOT_FOUND:
            case DatabaseStatus::FAILED_BAD_FORMAT:
                code = RPC_WALLET_NOT_FOUND;
                break;
            case DatabaseStatus::FAILED_ALREADY_LOADED:
                code = RPC_WALLET_ALREADY_LOADED;
                break;
            case DatabaseStatus::FAILED_ALREADY_EXISTS:
                code = RPC_WALLET_ALREADY_EXISTS;
                break;
            case DatabaseStatus::FAILED_INVALID_BACKUP_FILE:
                code = RPC_INVALID_PARAMETER;
                break;
            default: // RPC_WALLET_ERROR is returned for all other cases.
                break;
        }
        throw JSONRPCError(code, error.original);
    }
}
} // namespace wallet
