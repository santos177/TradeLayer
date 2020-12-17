/**
 * @file tradelayer.cpp
 *
 * This file contains the core of Trade Layer.
 */

#include <tradelayer/tradelayer.h>

#include <tradelayer/activation.h>
#include <tradelayer/consensushash.h>
#include <tradelayer/convert.h>
#include <tradelayer/dbbase.h>
#include <tradelayer/dbfees.h>
#include <tradelayer/dbspinfo.h>
#include <tradelayer/dbstolist.h>
#include <tradelayer/dbtradelist.h>
#include <tradelayer/dbtransaction.h>
#include <tradelayer/dbtxlist.h>
#include <tradelayer/dex.h>
#include <tradelayer/log.h>
#include <tradelayer/mdex.h>
#include <tradelayer/notifications.h>
#include <tradelayer/parsing.h>
#include <tradelayer/pending.h>
#include <tradelayer/persistence.h>
#include <tradelayer/rules.h>
#include <tradelayer/script.h>
#include <tradelayer/seedblocks.h>
#include <tradelayer/sp.h>
#include <tradelayer/tally.h>
#include <tradelayer/tx.h>
#include <tradelayer/utilsbitcoin.h>
#include <tradelayer/utilsui.h>
#include <tradelayer/version.h>
#include <tradelayer/walletcache.h>
#include <tradelayer/walletutils.h>
#include <tradelayer/operators_algo_clearing.h>
#include <tradelayer/uint256_extensions.h>
#include <tradelayer/parse_string.h>

#include <base58.h>
#include <chainparams.h>
#include <coins.h>
#include <core_io.h>
#include <fs.h>
#include <key_io.h>
#include <init.h>
#include <validation.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <shutdown.h>
#include <sync.h>
#include <tinyformat.h>
#include <uint256.h>
#include <ui_interface.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <util/time.h>
#ifdef ENABLE_WALLET
#include <script/ismine.h>
#include <wallet/wallet.h>
#endif

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/exception/to_string.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/multiprecision/cpp_int.hpp>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <iostream>
#include <numeric>

#include "tradelayer_matrices.h"
#include "externfns.h"


extern int idx_expiration;
extern int expirationAchieve;
extern double globalPNLALL_DUSD;
extern int64_t globalVolumeALL_DUSD;
extern int lastBlockg;
extern int twapBlockg;
extern int vestingActivationBlock;
extern volatile int64_t globalVolumeALL_LTC;
extern std::vector<std::string> vestingAddresses;
extern int64_t globalNumPrice;
extern int64_t globalDenPrice;

extern MatrixTLS *pt_ndatabase;
extern int n_cols;
extern int n_rows;

/** TWAP containers **/
extern std::map<uint32_t, std::vector<uint64_t>> cdextwap_ele;
extern std::map<uint32_t, std::vector<uint64_t>> cdextwap_vec;
extern std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> mdextwap_ele;
extern std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> mdextwap_vec;

extern std::vector<std::map<std::string, std::string>> path_elef;

extern std::vector<std::map<std::string, std::string>> path_elef;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> market_priceMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> numVWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> denVWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMapSubVector;
extern std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> numVWAPVector;
extern std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> denVWAPVector;
extern std::map<uint32_t, std::vector<int64_t>> mapContractAmountTimesPrice;
extern std::map<uint32_t, std::vector<int64_t>> mapContractVolume;
extern std::map<uint32_t, int64_t> VWAPMapContracts;

/** Pending withdrawals **/
extern std::map<std::string,vector<withdrawalAccepted>> withdrawal_Map;

extern std::map<uint32_t, std::map<std::string, double>> addrs_upnlc;
extern std::map<std::string, int64_t> sum_upnls;
extern std::map<uint32_t, int64_t> cachefees;
extern int64_t factorE;

using namespace mastercore;

using mastercore::StrToInt64;

typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;

//! Global lock for state objects
CCriticalSection cs_tally;

static int nWaterlineBlock = 0;

/**
 * Used to indicate, whether to automatically commit created transactions.
 *
 * Can be set with configuration "-autocommit" or RPC "setautocommit_TL".
 */
bool autoCommit = true;

//! Path for file based persistence
fs::path pathStateFiles;

//! Flag to indicate whether Trade Layer was initialized
static int mastercoreInitialized = 0;

//! Flag to indicate whether there was a block reorganisatzion
static int reorgRecoveryMode = 0;
//! Block height to recover from after a block reorganization
static int reorgRecoveryMaxHeight = 0;

//! LevelDB based storage for currencies, smart properties and tokens
CMPSPInfo* mastercore::pDbSpInfo;
//! LevelDB based storage for transactions, with txid as key and validity bit, and other data as value
CMPTxList* mastercore::pDbTransactionList;

//! LevelDB based storage for the MetaDEx trade history
CMPTradeList* mastercore::pDbTradeList;

//! LevelDB based storage for STO recipients
CMPSTOList* mastercore::pDbStoList;
//! LevelDB based storage for storing Trade Layer transaction validation and position in block data
CTLTransactionDB* mastercore::pDbTransaction;
//! LevelDB based storage for the MetaDEx fee cache
CTLFeeCache* mastercore::pDbFeeCache;
//! LevelDB based storage for the MetaDEx fee distributions
CTLFeeHistory* mastercore::pDbFeeHistory;

//! In-memory collection of DEx offers
OfferMap mastercore::my_offers;
//! In-memory collection of DEx accepts
AcceptMap mastercore::my_accepts;

//! Set containing properties that have freezing enabled
std::set<std::pair<uint32_t,int> > setFreezingEnabledProperties;
//! Set containing addresses that have been frozen
std::set<std::pair<std::string,uint32_t> > setFrozenAddresses;

//! In-memory collection of all amounts for all addresses for all properties
std::unordered_map<std::string, CMPTally> mastercore::mp_tally_map;

// Only needed for GUI:

//! Available balances of wallet properties
std::map<uint32_t, int64_t> global_balance_money;
//! Reserved balances of wallet propertiess
std::map<uint32_t, int64_t> global_balance_reserved;
//! Vector containing a list of properties relative to the wallet
std::set<uint32_t> global_wallet_property_list;

using boost::algorithm::token_compress_on;

std::string mastercore::strMPProperty(uint32_t propertyId)
{
    std::string str = "*unknown*";

    // test user-token
    if (0x80000000 & propertyId) {
        str = strprintf("Test token: %d : 0x%08X", 0x7FFFFFFF & propertyId, propertyId);
    } else {
        switch (propertyId) {
            case TL_PROPERTY_BTC: str = "BTC";
                break;
            case TL_PROPERTY_MSC: str = "TOTAL";
                break;
            case TL_PROPERTY_TMSC: str = "TTOTAL";
                break;
            default:
                str = strprintf("SP token: %d", propertyId);
        }
    }

    return str;
}

std::string FormatDivisibleShortMP(int64_t n)
{
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    std::string str = strprintf("%d.%08d", quotient, remainder);
    // clean up trailing zeros - good for RPC not so much for UI
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    if (str.length() > 0) {
        std::string::iterator it = str.end() - 1;
        if (*it == '.') {
            str.erase(it);
        }
    } //get rid of trailing dot if non decimal
    return str;
}

std::string FormatDivisibleMP(int64_t n, bool fSign)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    std::string str = strprintf("%d.%08d", quotient, remainder);

    if (!fSign) return str;

    if (n < 0)
        str.insert((unsigned int) 0, 1, '-');
    else
        str.insert((unsigned int) 0, 1, '+');
    return str;
}

std::string FormatIndivisibleMP(int64_t n)
{
    return strprintf("%d", n);
}

std::string FormatShortMP(uint32_t property, int64_t n)
{
    if (isPropertyDivisible(property)) {
        return FormatDivisibleShortMP(n);
    } else {
        return FormatIndivisibleMP(n);
    }
}

std::string FormatMP(uint32_t property, int64_t n, bool fSign)
{
    if (isPropertyDivisible(property)) {
        return FormatDivisibleMP(n, fSign);
    } else {
        return FormatIndivisibleMP(n);
    }
}

std::string FormatByType(int64_t amount, uint16_t propertyType)
{
    if (propertyType & MSC_PROPERTY_TYPE_INDIVISIBLE) {
        return FormatIndivisibleMP(amount);
    } else {
        return FormatDivisibleMP(amount);
    }
}

double FormatContractShortMP(int64_t n)
{
  int64_t n_abs = (n > 0 ? n : -n);
  int64_t quotient = n_abs / COIN;
  int64_t remainder = n_abs % COIN;
  std::string str = strprintf("%d.%08d", quotient, remainder);
  // clean up trailing zeros - good for RPC not so much for UI
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    if (str.length() > 0) {
        std::string::iterator it = str.end() - 1;
        if (*it == '.') {
            str.erase(it);
        }
    } //get rid of trailing dot if non decimal
    double q = atof(str.c_str());
    return q;
}

long int FormatShortIntegerMP(int64_t n)
{
    int64_t n_abs = (n > 0 ? n : -n);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    std::string str = strprintf("%d.%08d", quotient, remainder);
    // clean up trailing zeros - good for RPC not so much for UI
    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
    if (str.length() > 0) {
      std::string::iterator it = str.end() - 1;
      if (*it == '.') {
	str.erase(it);
      }
    } //get rid of trailing dot if non decimal
    long int q = atol(str.c_str());
    return q;
}

CMPTally* mastercore::getTally(const std::string& address)
{
    std::unordered_map<std::string, CMPTally>::iterator it = mp_tally_map.find(address);

    if (it != mp_tally_map.end()) return &(it->second);

    return static_cast<CMPTally*>(nullptr);
}

// look at balance for an address
int64_t GetTokenBalance(const std::string& address, uint32_t propertyId, TallyType ttype)
{
    int64_t balance = 0;
    if (TALLY_TYPE_COUNT <= ttype) {
        return 0;
    }
    if (ttype == ACCEPT_RESERVE && propertyId > TL_PROPERTY_TMSC) {
        // ACCEPT_RESERVE is always empty, except for MSC and TMSC
        return 0;
    }

    LOCK(cs_tally);
    const std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.find(address);
    if (my_it != mp_tally_map.end()) {
        balance = (my_it->second).getMoney(propertyId, ttype);
    }

    return balance;
}

int64_t GetAvailableTokenBalance(const std::string& address, uint32_t propertyId)
{
    int64_t money = GetTokenBalance(address, propertyId, BALANCE);
    int64_t pending = GetTokenBalance(address, propertyId, PENDING);

    if (0 > pending) {
        return (money + pending); // show the decrease in available money
    }

    return money;
}

int64_t GetReservedTokenBalance(const std::string& address, uint32_t propertyId)
{
    int64_t nReserved = 0;
    nReserved += GetTokenBalance(address, propertyId, ACCEPT_RESERVE);
    nReserved += GetTokenBalance(address, propertyId, METADEX_RESERVE);
    nReserved += GetTokenBalance(address, propertyId, SELLOFFER_RESERVE);

    return nReserved;
}

int64_t GetFrozenTokenBalance(const std::string& address, uint32_t propertyId)
{
    int64_t frozen = 0;

    if (isAddressFrozen(address, propertyId)) {
        frozen = GetTokenBalance(address, propertyId, BALANCE);
    }

    return frozen;
}

bool mastercore::isTestEcosystemProperty(uint32_t propertyId)
{
    if ((TL_PROPERTY_TMSC == propertyId) || (TEST_ECO_PROPERTY_1 <= propertyId)) return true;

    return false;
}

bool mastercore::isMainEcosystemProperty(uint32_t propertyId)
{
    if ((TL_PROPERTY_BTC != propertyId) && !isTestEcosystemProperty(propertyId)) return true;

    return false;
}

void mastercore::ClearFreezeState()
{
    // Should only ever be called in the event of a reorg
    setFreezingEnabledProperties.clear();
    setFrozenAddresses.clear();
}

void mastercore::PrintFreezeState()
{
    PrintToLog("setFrozenAddresses state:\n");
    for (std::set<std::pair<std::string,uint32_t> >::iterator it = setFrozenAddresses.begin(); it != setFrozenAddresses.end(); it++) {
        PrintToLog("  %s:%d\n", (*it).first, (*it).second);
    }
    PrintToLog("setFreezingEnabledProperties state:\n");
    for (std::set<std::pair<uint32_t,int> >::iterator it = setFreezingEnabledProperties.begin(); it != setFreezingEnabledProperties.end(); it++) {
        PrintToLog("  %d:%d\n", (*it).first, (*it).second);
    }
}

void mastercore::enableFreezing(uint32_t propertyId, int liveBlock)
{
    setFreezingEnabledProperties.insert(std::make_pair(propertyId, liveBlock));
    assert(isFreezingEnabled(propertyId, liveBlock));
    PrintToLog("Freezing for property %d will be enabled at block %d.\n", propertyId, liveBlock);
}

void mastercore::disableFreezing(uint32_t propertyId)
{
    int liveBlock = 0;
    for (std::set<std::pair<uint32_t,int> >::iterator it = setFreezingEnabledProperties.begin(); it != setFreezingEnabledProperties.end(); it++) {
        if (propertyId == (*it).first) {
            liveBlock = (*it).second;
        }
    }
    assert(liveBlock > 0);

    setFreezingEnabledProperties.erase(std::make_pair(propertyId, liveBlock));
    PrintToLog("Freezing for property %d has been disabled.\n", propertyId);

    // When disabling freezing for a property, all frozen addresses for that property will be unfrozen!
    for (std::set<std::pair<std::string,uint32_t> >::iterator it = setFrozenAddresses.begin(); it != setFrozenAddresses.end(); ) {
        if ((*it).second == propertyId) {
            PrintToLog("Address %s has been unfrozen for property %d.\n", (*it).first, propertyId);
            it = setFrozenAddresses.erase(it);
            assert(!isAddressFrozen((*it).first, (*it).second));
        } else {
            it++;
        }
    }

    assert(!isFreezingEnabled(propertyId, liveBlock));
}

bool mastercore::isFreezingEnabled(uint32_t propertyId, int block)
{
    for (std::set<std::pair<uint32_t,int> >::iterator it = setFreezingEnabledProperties.begin(); it != setFreezingEnabledProperties.end(); it++) {
        uint32_t itemPropertyId = (*it).first;
        int itemBlock = (*it).second;
        if (propertyId == itemPropertyId && block >= itemBlock) {
            return true;
        }
    }
    return false;
}

void mastercore::freezeAddress(const std::string& address, uint32_t propertyId)
{
    setFrozenAddresses.insert(std::make_pair(address, propertyId));
    assert(isAddressFrozen(address, propertyId));
    PrintToLog("Address %s has been frozen for property %d.\n", address, propertyId);
}

void mastercore::unfreezeAddress(const std::string& address, uint32_t propertyId)
{
    setFrozenAddresses.erase(std::make_pair(address, propertyId));
    assert(!isAddressFrozen(address, propertyId));
    PrintToLog("Address %s has been unfrozen for property %d.\n", address, propertyId);
}

bool mastercore::isAddressFrozen(const std::string& address, uint32_t propertyId)
{
    if (setFrozenAddresses.find(std::make_pair(address, propertyId)) != setFrozenAddresses.end()) {
        return true;
    }
    return false;
}

std::string mastercore::getTokenLabel(uint32_t propertyId)
{
    std::string tokenStr;
    if (propertyId < 3) {
        if (propertyId == 1) {
            tokenStr = " TOTAL";
        } else {
            tokenStr = " TTOTAL";
        }
    } else {
        tokenStr = strprintf(" SPT#%d", propertyId);
    }
    return tokenStr;
}

// get total tokens for a property
// optionally counts the number of addresses who own that property: n_owners_total
int64_t mastercore::getTotalTokens(uint32_t propertyId, int64_t* n_owners_total)
{
    int64_t prev = 0;
    int64_t owners = 0;
    int64_t totalTokens = 0;

    LOCK(cs_tally);

    CMPSPInfo::Entry property;
    if (false == pDbSpInfo->getSP(propertyId, property)) {
        return 0; // property ID does not exist
    }

    if (!property.fixed || n_owners_total) {
        for (std::unordered_map<std::string, CMPTally>::const_iterator it = mp_tally_map.begin(); it != mp_tally_map.end(); ++it) {
            const CMPTally& tally = it->second;

            totalTokens += tally.getMoney(propertyId, BALANCE);
            totalTokens += tally.getMoney(propertyId, SELLOFFER_RESERVE);
            totalTokens += tally.getMoney(propertyId, ACCEPT_RESERVE);
            totalTokens += tally.getMoney(propertyId, METADEX_RESERVE);

            if (prev != totalTokens) {
                prev = totalTokens;
                owners++;
            }
        }
        int64_t cachedFee = pDbFeeCache->GetCachedAmount(propertyId);
        totalTokens += cachedFee;
    }

    if (property.fixed) {
        totalTokens = property.num_tokens; // only valid for TX50
    }

    if (n_owners_total) *n_owners_total = owners;

    return totalTokens;
}

// return true if everything is ok
bool mastercore::update_tally_map(const std::string& who, uint32_t propertyId, int64_t amount, TallyType ttype)
{
    if (0 == amount) {
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d) ERROR: amount to credit or debit is zero\n", __func__, who, propertyId, propertyId, amount, ttype);
        return false;
    }
    if (ttype >= TALLY_TYPE_COUNT) {
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d) ERROR: invalid tally type\n", __func__, who, propertyId, propertyId, amount, ttype);
        return false;
    }

    bool bRet = false;
    int64_t before = 0;
    int64_t after = 0;

    LOCK(cs_tally);

    if (ttype == BALANCE && amount < 0) {
        assert(!isAddressFrozen(who, propertyId)); // for safety, this should never fail if everything else is working properly.
    }

    before = GetTokenBalance(who, propertyId, ttype);

    std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.find(who);
    if (my_it == mp_tally_map.end()) {
        // insert an empty element
        my_it = (mp_tally_map.insert(std::make_pair(who, CMPTally()))).first;
    }

    CMPTally& tally = my_it->second;
    bRet = tally.updateMoney(propertyId, amount, ttype);

    after = GetTokenBalance(who, propertyId, ttype);
    if (!bRet) {
        assert(before == after);
        PrintToLog("%s(%s, %u=0x%X, %+d, ttype=%d) ERROR: insufficient balance (=%d)\n", __func__, who, propertyId, propertyId, amount, ttype, before);
    }

    return bRet;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// some old TODOs
//  6) verify large-number calculations (especially divisions & multiplications)
//  9) build in consesus checks with the masterchain.info & masterchest.info -- possibly run them automatically, daily (?)
// 10) need a locking mechanism between Core & Qt -- to retrieve the tally, for instance, this and similar to this: LOCK(wallet->cs_wallet);
//

uint32_t mastercore::GetNextPropertyId(bool maineco)
{
    if (!pDbSpInfo)
        return 0;

    if (maineco) {
        return pDbSpInfo->peekNextSPID(1);
    } else {
        return pDbSpInfo->peekNextSPID(2);
    }
}

// Perform any actions that need to be taken when the total number of tokens for a property ID changes
void NotifyTotalTokensChanged(uint32_t propertyId, int block)
{
    pDbFeeCache->UpdateDistributionThresholds(propertyId);
    pDbFeeCache->EvalCache(propertyId, block);
}

void CheckWalletUpdate(bool forceUpdate)
{
#ifdef ENABLE_WALLET
    if (!HasWallets()) {
        return;
    }
#endif

    // because the wallet balance cache is *only* used by the UI, it's not needed,
    // when the daemon is running without UI.
    if (!fQtMode) {
        return;
    }

    if (!WalletCacheUpdate()) {
        // no balance changes were detected that affect wallet addresses, signal a generic change to overall Trade Layer state
        if (!forceUpdate) {
            uiInterface.TLStateChanged();
            return;
        }
    }
#ifdef ENABLE_WALLET
    LOCK(cs_tally);

    // balance changes were found in the wallet, update the global totals and signal a Trade Layer balance change
    global_balance_money.clear();
    global_balance_reserved.clear();

    // populate global balance totals and wallet property list - note global balances do not include additional balances from watch-only addresses
    for (std::unordered_map<std::string, CMPTally>::iterator my_it = mp_tally_map.begin(); my_it != mp_tally_map.end(); ++my_it) {
        // check if the address is a wallet address (including watched addresses)
        std::string address = my_it->first;
        int addressIsMine = IsMyAddressAllWallets(address, false, ISMINE_SPENDABLE);
        if (!addressIsMine) continue;
        // iterate only those properties in the TokenMap for this address
        my_it->second.init();
        uint32_t propertyId;
        while (0 != (propertyId = (my_it->second).next())) {
            // add to the global wallet property list
            global_wallet_property_list.insert(propertyId);
            // check if the address is spendable (only spendable balances are included in totals)
            if (addressIsMine != ISMINE_SPENDABLE) continue;
            // work out the balances and add to globals
            global_balance_money[propertyId] += GetAvailableTokenBalance(address, propertyId);
            global_balance_reserved[propertyId] += GetTokenBalance(address, propertyId, SELLOFFER_RESERVE);
            global_balance_reserved[propertyId] += GetTokenBalance(address, propertyId, METADEX_RESERVE);
            global_balance_reserved[propertyId] += GetTokenBalance(address, propertyId, ACCEPT_RESERVE);
        }
    }
    // signal an Trade Layer balance change
    uiInterface.TLBalanceChanged();
#endif
}

//! Cache for potential Trade Layer transactions
static std::set<uint256> setMarkerCache;

//! Guards marker cache
static CCriticalSection cs_marker_cache;

/**
 * Checks, if transaction has any Trade Layer marker.
 *
 * Note: this may include invalid or malformed Trade Layer transactions!
 *
 * MUST NOT BE USED FOR CONSENSUS CRITICAL STUFF!
 */
static bool HasMarkerUnsafe(const CTransactionRef& tx)
{
    const std::string strClassC("6f6d6e69");
    const std::string strClassAB("76a914946cb2e08075bcbaf157e47bcb67eb2b2339d24288ac");
    const std::string strClassABTest("76a914643ce12b1590633077b8620316f43a9362ef18e588ac");
    const std::string strClassMoney("76a9145ab93563a289b74c355a9b9258b86f12bb84affb88ac");

    for (unsigned int n = 0; n < tx->vout.size(); ++n) {
        const CTxOut& out = tx->vout[n];
        std::string str = HexStr(out.scriptPubKey.begin(), out.scriptPubKey.end());

        if (str.find(strClassC) != std::string::npos) {
            return true;
        }

        if (MainNet()) {
            if (str == strClassAB) {
                return true;
            }
        } else {
            if (str == strClassABTest) {
                return true;
            }
            if (str == strClassMoney) {
                return true;
            }
        }
    }

    return false;
}

/** Scans for marker and if one is found, add transaction to marker cache. */
void TryToAddToMarkerCache(const CTransactionRef &tx)
{
    if (HasMarkerUnsafe(tx)) {
        LOCK(cs_marker_cache);
        setMarkerCache.insert(tx->GetHash());
    }
}

/** Removes transaction from marker cache. */
void RemoveFromMarkerCache(const uint256& txHash)
{
    LOCK(cs_marker_cache);
    setMarkerCache.erase(txHash);
}

/** Checks, if transaction is in marker cache. */
bool IsInMarkerCache(const uint256& txHash)
{
    LOCK(cs_marker_cache);
    return (setMarkerCache.find(txHash) != setMarkerCache.end());
}

void creatingVestingTokens()
{
  extern int64_t amountVesting;
  extern int64_t totalVesting;
  extern std::string admin_addrs;

  CMPSPInfo::Entry newSP;

  newSP.name = "Vesting Tokens";
  newSP.data = "Divisible Tokens";
  newSP.url  = "www.tradelayer.org";
  newSP.category = "N/A";
  newSP.subcategory = "N/A";
  newSP.prop_type = ALL_PROPERTY_TYPE_DIVISIBLE;
  newSP.num_tokens = amountVesting;
  newSP.attribute_type = ALL_PROPERTY_TYPE_VESTING;

  const uint32_t propertyIdVesting = pDbSpInfo->putSP(TL_PROPERTY_ALL, newSP);
  assert(propertyIdVesting > 0);

  assert(update_tally_map(admin_addrs, propertyIdVesting, totalVesting, BALANCE));
}

/**
 * Returns the encoding class, used to embed a payload.
 *
 *   0 None
 *   3 Class C (op-return)
 */
int mastercore::GetEncodingClass(const CTransaction& tx, int nBlock)
{
    bool hasOpReturn = false;

    /* Fast Search
     * Perform a string comparison on hex for each scriptPubKey & look directly for Trade Layer marker bytes
     * This allows to drop non-Trade Layer transactions with less work
     */
    std::string strClassC = "6f6d6e69";
    bool examineClosely = false;
    for (unsigned int n = 0; n < tx.vout.size(); ++n) {
        const CTxOut& output = tx.vout[n];
        std::string strSPB = HexStr(output.scriptPubKey.begin(), output.scriptPubKey.end());
        if (nBlock < 395000) { // class C not enabled yet, no need to search for marker bytes
            continue;
        } else if (strSPB.find(strClassC) != std::string::npos) {
            examineClosely = true;
            break;
        }
    }

    // Examine everything when not on mainnet
    if (isNonMainNet()) {
        examineClosely = true;
    }

    if (!examineClosely) return NO_MARKER;

    for (unsigned int n = 0; n < tx.vout.size(); ++n) {
        const CTxOut& output = tx.vout[n];

        txnouttype outType;
        if (!GetOutputType(output.scriptPubKey, outType)) {
            continue;
        }
        if (!IsAllowedOutputType(outType, nBlock)) {
            continue;
        }

        if (outType == TX_NULL_DATA) {
            // Ensure there is a payload, and the first pushed element equals,
            // or starts with the "tl" marker
            std::vector<std::string> scriptPushes;
            if (!GetScriptPushes(output.scriptPubKey, scriptPushes)) {
                continue;
            }
            if (!scriptPushes.empty()) {
                std::vector<unsigned char> vchMarker = GetTLMarker();
                std::vector<unsigned char> vchPushed = ParseHex(scriptPushes[0]);
                if (vchPushed.size() < vchMarker.size()) {
                    continue;
                }
                if (std::equal(vchMarker.begin(), vchMarker.end(), vchPushed.begin())) {
                    hasOpReturn = true;
                }
            }
        }
    }

    if (hasOpReturn) {
        return TL_CLASS_C;
    }


    return NO_MARKER;
}

// TODO: move
CCoinsView mastercore::viewDummy;
CCoinsViewCache mastercore::view(&viewDummy);

//! Guards coins view cache
CCriticalSection mastercore::cs_tx_cache;

static unsigned int nCacheHits = 0;
static unsigned int nCacheMiss = 0;

/**
 * Fetches transaction inputs and adds them to the coins view cache.
 *
 * Note: cs_tx_cache should be locked, when adding and accessing inputs!
 *
 * @param tx[in]  The transaction to fetch inputs for
 * @return True, if all inputs were successfully added to the cache
 */
static bool FillTxInputCache(const CTransaction& tx, const std::shared_ptr<std::map<COutPoint, Coin>> removedCoins)
{
    static unsigned int nCacheSize = gArgs.GetArg("-tltxcache", 500000);

    if (view.GetCacheSize() > nCacheSize) {
        PrintToLog("%s(): clearing cache before insertion [size=%d, hit=%d, miss=%d]\n",
                __func__, view.GetCacheSize(), nCacheHits, nCacheMiss);
        view.Flush();
    }

    for (std::vector<CTxIn>::const_iterator it = tx.vin.begin(); it != tx.vin.end(); ++it) {
        const CTxIn& txIn = *it;
        unsigned int nOut = txIn.prevout.n;
        const Coin& coin = view.AccessCoin(txIn.prevout);

        if (!coin.IsSpent()) {
            ++nCacheHits;
            continue;
        } else {
            ++nCacheMiss;
        }

        CTransactionRef txPrev;
        uint256 hashBlock;
        Coin newcoin;
        if (GetTransaction(txIn.prevout.hash, txPrev, Params().GetConsensus(), hashBlock)) {
            newcoin.out.scriptPubKey = txPrev->vout[nOut].scriptPubKey;
            newcoin.out.nValue = txPrev->vout[nOut].nValue;
            BlockMap::iterator bit = mapBlockIndex.find(hashBlock);
            newcoin.nHeight = bit != mapBlockIndex.end() ? bit->second->nHeight : 1;
        } else if (removedCoins) {
            std::map<COutPoint, Coin>::const_iterator coinIt = removedCoins->find(txIn.prevout);
            if (coinIt != removedCoins->end()) {
                newcoin = coinIt->second;
            } else {
                return false;
            }
        } else {
            return false;
        }

        view.AddCoin(txIn.prevout, std::move(newcoin), true);
    }

    return true;
}

// idx is position within the block, 0-based
// int msc_tx_push(const CTransaction &wtx, int nBlock, unsigned int idx)
// INPUT: bRPConly -- set to true to avoid moving funds; to be called from various RPC calls like this
// RETURNS: 0 if parsed a MP TX
// RETURNS: < 0 if a non-MP-TX or invalid
// RETURNS: >0 if 1 or more payments have been made
static int parseTransaction(bool bRPConly, const CTransaction& wtx, int nBlock, unsigned int idx, CMPTransaction& mp_tx, unsigned int nTime, const std::shared_ptr<std::map<COutPoint, Coin>> removedCoins = nullptr)
{
    assert(bRPConly == mp_tx.isRpcOnly());
    mp_tx.Set(wtx.GetHash(), nBlock, idx, nTime);

    // ### CLASS IDENTIFICATION AND MARKER CHECK ###
    int tlClass = GetEncodingClass(wtx, nBlock);

    if (tlClass == NO_MARKER) {
        return -1; // Not a valid tradelayer transaction
    }

    if (!bRPConly || msc_debug_parser_readonly) {
        PrintToLog("____________________________________________________________________________________________________________________________________\n");
        PrintToLog("%s(block=%d, %s idx= %d); txid: %s\n", __FUNCTION__, nBlock, FormatISO8601DateTime(nTime), idx, wtx.GetHash().GetHex());
    }

    // ### SENDER IDENTIFICATION ###
    std::string strSender;
    int64_t inAll = 0;

    { // needed to ensure the cache isn't cleared in the meantime when doing parallel queries
        LOCK2(cs_main, cs_tx_cache); // cs_main should be locked first to avoid deadlocks with cs_tx_cache at FillTxInputCache(...)->GetTransaction(...)->LOCK(cs_main)

        // Add previous transaction inputs to the cache
        if (!FillTxInputCache(wtx, removedCoins)) {
            PrintToLog("%s() ERROR: failed to get inputs for %s\n", __func__, wtx.GetHash().GetHex());
            return -101;
        }

        assert(view.HaveInputs(wtx));

        // NEW LOGIC - the sender is chosen based on the first vin

        // determine the sender, but invalidate transaction, if the input is not accepted
        unsigned int vin_n = 0; // the first input
        if (msc_debug_vin) PrintToLog("vin=%d:%s\n", vin_n, ScriptToAsmStr(wtx.vin[vin_n].scriptSig));

        const CTxIn& txIn = wtx.vin[vin_n];
        const Coin& coin = view.AccessCoin(txIn.prevout);
        const CTxOut& txOut = coin.out;

        assert(!txOut.IsNull());

        txnouttype whichType;
        if (!GetOutputType(txOut.scriptPubKey, whichType)) {
            return -108;
        }
        if (!IsAllowedInputType(whichType, nBlock)) {
            return -109;
        }
        CTxDestination source;
        if (ExtractDestination(txOut.scriptPubKey, source)) {
            strSender = EncodeDestination(source);
        }
        else return -110;

        inAll = view.GetValueIn(wtx);

    } // end of LOCK(cs_tx_cache)

    int64_t outAll = wtx.GetValueOut();
    int64_t txFee = inAll - outAll; // miner fee

    if (!strSender.empty()) {
        if (msc_debug_verbose) PrintToLog("The Sender: %s : fee= %s\n", strSender, FormatDivisibleMP(txFee));
    } else {
        PrintToLog("The sender is still EMPTY !!! txid: %s\n", wtx.GetHash().GetHex());
        return -5;
    }

    // ### DATA POPULATION ### - save output addresses, values and scripts
    std::string strReference;
    unsigned char single_pkt[MAX_PACKETS * PACKET_SIZE];
    unsigned int packet_size = 0;
    std::vector<std::string> script_data;
    std::vector<std::string> address_data;
    std::vector<int64_t> value_data;

    for (unsigned int n = 0; n < wtx.vout.size(); ++n) {
        txnouttype whichType;
        if (!GetOutputType(wtx.vout[n].scriptPubKey, whichType)) {
            continue;
        }
        if (!IsAllowedOutputType(whichType, nBlock)) {
            continue;
        }
        CTxDestination dest;
        if (ExtractDestination(wtx.vout[n].scriptPubKey, dest)) {
                // saving for Class A processing or reference
                GetScriptPushes(wtx.vout[n].scriptPubKey, script_data);
                address_data.push_back(EncodeDestination(dest));
                value_data.push_back(wtx.vout[n].nValue);
                if (msc_debug_parser_data) PrintToLog("saving address_data #%d: %s:%s\n", n, EncodeDestination(dest), ScriptToAsmStr(wtx.vout[n].scriptPubKey));
        }
    }
    if (msc_debug_parser_data) PrintToLog(" address_data.size=%lu\n script_data.size=%lu\n value_data.size=%lu\n", address_data.size(), script_data.size(), value_data.size());

    // ### CLASS B / CLASS C PARSING ###
    if (tlClass == TL_CLASS_C) {
        if (msc_debug_parser_data) PrintToLog("Beginning reference identification\n");
        bool referenceFound = false; // bool to hold whether we've found the reference yet
        bool changeRemoved = false; // bool to hold whether we've ignored the first output to sender as change
        unsigned int potentialReferenceOutputs = 0; // int to hold number of potential reference outputs
        for (unsigned k = 0; k < address_data.size(); ++k) { // how many potential reference outputs do we have, if just one select it right here
            const std::string& addr = address_data[k];
            if (msc_debug_parser_data) PrintToLog("ref? data[%d]:%s: %s (%s)\n", k, script_data[k], addr, FormatIndivisibleMP(value_data[k]));
                ++potentialReferenceOutputs;
                if (1 == potentialReferenceOutputs) {
                    strReference = addr;
                    referenceFound = true;
                    if (msc_debug_parser_data) PrintToLog("Single reference potentially id'd as follows: %s \n", strReference);
                } else { //as soon as potentialReferenceOutputs > 1 we need to go fishing
                    strReference.clear(); // avoid leaving strReference populated for sanity
                    referenceFound = false;
                    if (msc_debug_parser_data) PrintToLog("More than one potential reference candidate, blanking strReference, need to go fishing\n");
                }
        }

        if (!referenceFound) { // do we have a reference now? or do we need to dig deeper
            if (msc_debug_parser_data) PrintToLog("Reference has not been found yet, going fishing\n");
            for (unsigned k = 0; k < address_data.size(); ++k)
            {
                const std::string& addr = address_data[k];
                if (addr == strSender && !changeRemoved)
                {
                    changeRemoved = true; // per spec ignore first output to sender as change if multiple possible ref addresses
                    if (msc_debug_parser_data) PrintToLog("Removed change\n");
                } else {
                        strReference = addr; // this may be set several times, but last time will be highest vout
                        if (msc_debug_parser_data) PrintToLog("Resetting strReference as follows: %s \n ", strReference);
                    }
            }
        }

        if (msc_debug_parser_data) PrintToLog("Ending reference identification\nFinal decision on reference identification is: %s\n", strReference);

        std::vector<std::string> op_return_script_data;

        // ### POPULATE OP RETURN SCRIPT DATA ###
        for (unsigned int n = 0; n < wtx.vout.size(); ++n) {
            txnouttype whichType;
            if (!GetOutputType(wtx.vout[n].scriptPubKey, whichType)) {
                continue;
            }
            if (!IsAllowedOutputType(whichType, nBlock)) {
                continue;
            }
            if (whichType == TX_NULL_DATA) {
                // only consider outputs, which are explicitly tagged
                std::vector<std::string> vstrPushes;
                if (!GetScriptPushes(wtx.vout[n].scriptPubKey, vstrPushes)) {
                    continue;
                }
                // TODO: maybe encapsulate the following sort of messy code
                if (!vstrPushes.empty()) {
                    std::vector<unsigned char> vchMarker = GetTLMarker();
                    std::vector<unsigned char> vchPushed = ParseHex(vstrPushes[0]);
                    if (vchPushed.size() < vchMarker.size()) {
                        continue;
                    }
                    if (std::equal(vchMarker.begin(), vchMarker.end(), vchPushed.begin())) {
                        size_t sizeHex = vchMarker.size() * 2;
                        // strip out the marker at the very beginning
                        vstrPushes[0] = vstrPushes[0].substr(sizeHex);
                        // add the data to the rest
                        op_return_script_data.insert(op_return_script_data.end(), vstrPushes.begin(), vstrPushes.end());

                        if (msc_debug_parser_data) {
                            PrintToLog("Class C transaction detected: %s parsed to %s at vout %d\n", wtx.GetHash().GetHex(), vstrPushes[0], n);
                        }
                    }
                }
            }
        }
        // ### EXTRACT PAYLOAD FOR CLASS C ###
        for (unsigned int n = 0; n < op_return_script_data.size(); ++n) {
            if (!op_return_script_data[n].empty()) {
                assert(IsHex(op_return_script_data[n])); // via GetScriptPushes()
                std::vector<unsigned char> vch = ParseHex(op_return_script_data[n]);
                unsigned int payload_size = vch.size();
                if (packet_size + payload_size > MAX_PACKETS * PACKET_SIZE) {
                    payload_size = MAX_PACKETS * PACKET_SIZE - packet_size;
                    PrintToLog("limiting payload size to %d byte\n", packet_size + payload_size);
                }
                if (payload_size > 0) {
                    memcpy(single_pkt+packet_size, &vch[0], payload_size);
                    packet_size += payload_size;
                }
                if (MAX_PACKETS * PACKET_SIZE == packet_size) {
                    break;
                }
            }
        }
    }

    // ### SET MP TX INFO ###
    if (msc_debug_verbose) PrintToLog("single_pkt: %s\n", HexStr(single_pkt, packet_size + single_pkt));
    mp_tx.Set(strSender, strReference, 0, wtx.GetHash(), nBlock, idx, (unsigned char *)&single_pkt, packet_size, tlClass, (inAll-outAll));

    return 0;
}

/**
 * Provides access to parseTransaction in read-only mode.
 */
int ParseTransaction(const CTransaction& tx, int nBlock, unsigned int idx, CMPTransaction& mptx, unsigned int nTime)
{
    return parseTransaction(true, tx, nBlock, idx, mptx, nTime);
}

/**
 * Handles potential DEx payments.
 *
 * Note: must *not* be called outside of the transaction handler, and it does not
 * check, if a transaction marker exists.
 *
 * @return True, if valid
 */
static bool HandleDExPayments(const CTransaction& tx, int nBlock, const std::string& strSender)
{
    int count = 0;

    for (unsigned int n = 0; n < tx.vout.size(); ++n) {
        CTxDestination dest;
        if (ExtractDestination(tx.vout[n].scriptPubKey, dest)) {
            std::string strAddress = EncodeDestination(dest);
            if (strAddress == strSender)
                continue;

            if (msc_debug_parser_dex) PrintToLog("payment #%d %s %s\n", count, strAddress, FormatIndivisibleMP(tx.vout[n].nValue));


            // check everything and pay BTC for the property we are buying here...
            if (0 == DEx_payment(tx.GetHash(), n, strAddress, strSender, tx.vout[n].nValue, nBlock)) ++count;
        }
    }

    return (count > 0);
}


/**
 * Reports the progress of the initial transaction scanning.
 *
 * The progress is printed to the console, written to the debug log file, and
 * the RPC status, as well as the splash screen progress label, are updated.
 *
 * @see msc_initial_scan()
 */
class ProgressReporter
{
private:
    const CBlockIndex* m_pblockFirst;
    const CBlockIndex* m_pblockLast;
    const int64_t m_timeStart;

    /** Returns the estimated remaining time in milliseconds. */
    int64_t estimateRemainingTime(double progress) const
    {
        int64_t timeSinceStart = GetTimeMillis() - m_timeStart;

        double timeRemaining = 3600000.0; // 1 hour
        if (progress > 0.0 && timeSinceStart > 0) {
            timeRemaining = (100.0 - progress) / progress * timeSinceStart;
        }

        return static_cast<int64_t>(timeRemaining);
    }

    /** Converts a time span to a human readable string. */
    std::string remainingTimeAsString(int64_t remainingTime) const
    {
        int64_t secondsTotal = 0.001 * remainingTime;
        int64_t hours = secondsTotal / 3600;
        int64_t minutes = (secondsTotal / 60) % 60;
        int64_t seconds = secondsTotal % 60;

        if (hours > 0) {
            return strprintf("%d:%02d:%02d hours", hours, minutes, seconds);
        } else if (minutes > 0) {
            return strprintf("%d:%02d minutes", minutes, seconds);
        } else {
            return strprintf("%d seconds", seconds);
        }
    }

public:
    ProgressReporter(const CBlockIndex* pblockFirst, const CBlockIndex* pblockLast)
    : m_pblockFirst(pblockFirst), m_pblockLast(pblockLast), m_timeStart(GetTimeMillis())
    {
    }

    /** Prints the current progress to the console and notifies the UI. */
    void update(const CBlockIndex* pblockNow) const
    {
        int nLastBlock = m_pblockLast->nHeight;
        int nCurrentBlock = pblockNow->nHeight;
        unsigned int nFirst = m_pblockFirst->nChainTx;
        unsigned int nCurrent = pblockNow->nChainTx;
        unsigned int nLast = m_pblockLast->nChainTx;

        double dProgress = 100.0 * (nCurrent - nFirst) / (nLast - nFirst);
        int64_t nRemainingTime = estimateRemainingTime(dProgress);

        std::string strProgress = strprintf(
                "Still scanning.. at block %d of %d. Progress: %.2f %%, about %s remaining..\n",
                nCurrentBlock, nLastBlock, dProgress, remainingTimeAsString(nRemainingTime));
        std::string strProgressUI = strprintf(
                "Still scanning.. at block %d of %d.\nProgress: %.2f %% (about %s remaining)",
                nCurrentBlock, nLastBlock, dProgress, remainingTimeAsString(nRemainingTime));

        PrintToConsole(strProgress);
        uiInterface.InitMessage(strProgressUI);
    }
};

/**
 * Scans the blockchain for meta transactions.
 *
 * It scans the blockchain, starting at the given block index, to the current
 * tip, much like as if new block were arriving and being processed on the fly.
 *
 * Every 30 seconds the progress of the scan is reported.
 *
 * In case the current block being processed is not part of the active chain, or
 * if a block could not be retrieved from the disk, then the scan stops early.
 * Likewise, global shutdown requests are honored, and stop the scan progress.
 *
 * @see mastercore_handler_block_begin()
 * @see mastercore_handler_tx()
 * @see mastercore_handler_block_end()
 *
 * @param nFirstBlock[in]  The index of the first block to scan
 * @return An exit code, indicating success or failure
 */
static int msc_initial_scan(int nFirstBlock)
{
    int nTimeBetweenProgressReports = gArgs.GetArg("-tlprogressfrequency", 30);  // seconds
    int64_t nNow = GetTime();
    unsigned int nTxsTotal = 0;
    unsigned int nTxsFoundTotal = 0;
    int nBlock = 999999;
    const int nLastBlock = GetHeight();

    // this function is useless if there are not enough blocks in the blockchain yet!
    if (nFirstBlock < 0 || nLastBlock < nFirstBlock) return -1;
    PrintToConsole("Scanning for transactions in block %d to block %d..\n", nFirstBlock, nLastBlock);

    // used to print the progress to the console and notifies the UI
    ProgressReporter progressReporter(chainActive[nFirstBlock], chainActive[nLastBlock]);

    // check if using seed block filter should be disabled
    bool seedBlockFilterEnabled = gArgs.GetBoolArg("-tlseedblockfilter", true);

    for (nBlock = nFirstBlock; nBlock <= nLastBlock; ++nBlock)
    {
        if (ShutdownRequested()) {
            PrintToLog("Shutdown requested, stop scan at block %d of %d\n", nBlock, nLastBlock);
            break;
        }

        CBlockIndex* pblockindex = chainActive[nBlock];
        if (nullptr == pblockindex) break;
        std::string strBlockHash = pblockindex->GetBlockHash().GetHex();

        if (msc_debug_exo) PrintToLog("%s(%d; max=%d):%s, line %d, file: %s\n",
            __FUNCTION__, nBlock, nLastBlock, strBlockHash, __LINE__, __FILE__);

        if (GetTime() >= nNow + nTimeBetweenProgressReports) {
            progressReporter.update(pblockindex);
            nNow = GetTime();
        }

        unsigned int nTxNum = 0;
        unsigned int nTxsFoundInBlock = 0;
        mastercore_handler_block_begin(nBlock, pblockindex);

        if (!seedBlockFilterEnabled || !SkipBlock(nBlock)) {
            CBlock block;
            if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus())) break;

            for(const auto tx : block.vtx) {
                if (mastercore_handler_tx(*tx, nBlock, nTxNum, pblockindex, nullptr)) ++nTxsFoundInBlock;
                ++nTxNum;
            }
        }

        nTxsFoundTotal += nTxsFoundInBlock;
        nTxsTotal += nTxNum;
        mastercore_handler_block_end(nBlock, pblockindex, nTxsFoundInBlock);
    }

    if (nBlock < nLastBlock) {
        PrintToConsole("Scan stopped early at block %d of block %d\n", nBlock, nLastBlock);
    }

    PrintToConsole("%d new transactions processed, %d meta transactions found\n", nTxsTotal, nTxsFoundTotal);

    return 0;
}

/**
 * Clears the state of the system.
 */
void clear_all_state()
{
    PrintToLog("Clearing all state..\n");
    LOCK2(cs_tally, cs_pending);

    // Memory based storage
    mp_tally_map.clear();
    my_offers.clear();
    my_accepts.clear();
    metadex.clear();
    my_pending.clear();
    ResetConsensusParams();
    ClearActivations();
    ClearAlerts();
    ClearFreezeState();

    // LevelDB based storage
    pDbSpInfo->Clear();
    pDbTransactionList->Clear();
    pDbStoList->Clear();
    pDbTradeList->Clear();
    pDbTransaction->Clear();
    pDbFeeCache->Clear();
    pDbFeeHistory->Clear();
    assert(pDbTransactionList->setDBVersion() == DB_VERSION); // new set of databases, set DB version
}

void RewindDBsAndState(int nHeight, int nBlockPrev = 0, bool fInitialParse = false)
{
    // Check if any freeze related transactions would be rolled back - if so wipe the state and startclean
    bool reorgContainsFreeze = pDbTransactionList->CheckForFreezeTxs(nHeight);

    // NOTE: The blockNum parameter is inclusive, so deleteAboveBlock(1000) will delete records in block 1000 and above.
    pDbTransactionList->isMPinBlockRange(nHeight, reorgRecoveryMaxHeight, true);
    pDbTradeList->deleteAboveBlock(nHeight);
    pDbStoList->deleteAboveBlock(nHeight);
    pDbFeeCache->RollBackCache(nHeight);
    pDbFeeHistory->RollBackHistory(nHeight);
    reorgRecoveryMaxHeight = 0;

    nWaterlineBlock = ConsensusParams().GENESIS_BLOCK - 1;

    if (reorgContainsFreeze && !fInitialParse) {
       PrintToConsole("Reorganization containing freeze related transactions detected, forcing a reparse...\n");
       clear_all_state(); // unable to reorg freezes safely, clear state and reparse
    } else {
        int best_state_block = LoadMostRelevantInMemoryState();
        if (best_state_block < 0) {
            // unable to recover easily, remove stale stale state bits and reparse from the beginning.
            clear_all_state();
        } else {
            nWaterlineBlock = best_state_block;
        }
    }

    // clear the global wallet property list, perform a forced wallet update and tell the UI that state is no longer valid, and UI views need to be reinit
    global_wallet_property_list.clear();
    CheckWalletUpdate(true);
    uiInterface.TLStateInvalidated();

    if (nWaterlineBlock < nBlockPrev) {
        // scan from the block after the best active block to catch up to the active chain
        msc_initial_scan(nWaterlineBlock + 1);
    }
}

/**
 * Global handler to initialize Trade Layer.
 *
 * @return An exit code, indicating success or failure
 */
int mastercore_init()
{
    {
        LOCK(cs_tally);

        if (mastercoreInitialized) {
            // nothing to do
            return 0;
        }

        PrintToConsole("Initializing Trade Layer v%s [%s]\n", TradeLayerVersion(), Params().NetworkIDString());

        PrintToLog("\nInitializing Trade Layer v%s [%s]\n", TradeLayerVersion(), Params().NetworkIDString());
        PrintToLog("Startup time: %s\n", FormatISO8601DateTime(GetTime()));

        InitDebugLogLevels();
        ShrinkDebugLog();


        // check for --autocommit option and set transaction commit flag accordingly
        if (!gArgs.GetBoolArg("-autocommit", true)) {
            PrintToLog("Process was started with --autocommit set to false. "
                    "Created Trade Layer transactions will not be committed to wallet or broadcast.\n");
            autoCommit = false;
        }

        // check for --startclean option and delete MP_ folders if present
        bool startClean = false;
        if (gArgs.GetBoolArg("-startclean", false)) {
            PrintToLog("Process was started with --startclean option, attempting to clear persistence files..\n");
            try {
                fs::path persistPath = GetDataDir() / "MP_persist";
                fs::path txlistPath = GetDataDir() / "MP_txlist";
                fs::path tradePath = GetDataDir() / "MP_tradelist";
                fs::path spPath = GetDataDir() / "MP_spinfo";
                fs::path stoPath = GetDataDir() / "MP_stolist";
                fs::path tlTXDBPath = GetDataDir() / "TL_TXDB";
                // fs::path feesPath = GetDataDir() / "TL_feecache";
                // fs::path feeHistoryPath = GetDataDir() / "TL_feehistory";
                if (fs::exists(persistPath)) fs::remove_all(persistPath);
                if (fs::exists(txlistPath)) fs::remove_all(txlistPath);
                if (fs::exists(tradePath)) fs::remove_all(tradePath);
                if (fs::exists(spPath)) fs::remove_all(spPath);
                if (fs::exists(stoPath)) fs::remove_all(stoPath);
                if (fs::exists(tlTXDBPath)) fs::remove_all(tlTXDBPath);
                // if (fs::exists(feesPath)) fs::remove_all(feesPath);
                // if (fs::exists(feeHistoryPath)) fs::remove_all(feeHistoryPath);
                PrintToLog("Success clearing persistence files in datadir %s\n", GetDataDir().string());
                startClean = true;
            } catch (const fs::filesystem_error& e) {
                PrintToLog("Failed to delete persistence folders: %s\n", e.what());
                PrintToConsole("Failed to delete persistence folders: %s\n", e.what());
            }
        }

        pDbTradeList = new CMPTradeList(GetDataDir() / "MP_tradelist", fReindex);
        pDbStoList = new CMPSTOList(GetDataDir() / "MP_stolist", fReindex);
        pDbTransactionList = new CMPTxList(GetDataDir() / "MP_txlist", fReindex);
        pDbSpInfo = new CMPSPInfo(GetDataDir() / "MP_spinfo", fReindex);
        pDbTransaction = new CTLTransactionDB(GetDataDir() / "TL_TXDB", fReindex);
        // pDbFeeCache = new CTLFeeCache(GetDataDir() / "TL_feecache", fReindex);
        // pDbFeeHistory = new CTLFeeHistory(GetDataDir() / "TL_feehistory", fReindex);

        pathStateFiles = GetDataDir() / "MP_persist";
        TryCreateDirectories(pathStateFiles);

        bool wrongDBVersion = (pDbTransactionList->getDBVersion() != DB_VERSION);

        ++mastercoreInitialized;

        nWaterlineBlock = LoadMostRelevantInMemoryState();

        if (!startClean && nWaterlineBlock > 0 && nWaterlineBlock < GetHeight()) {
            RewindDBsAndState(nWaterlineBlock + 1, 0, true);
        }

        bool noPreviousState = (nWaterlineBlock <= 0);

        if (startClean) {
            assert(pDbTransactionList->setDBVersion() == DB_VERSION); // new set of databases, set DB version
        } else if (wrongDBVersion) {
            nWaterlineBlock = -1; // force a clear_all_state and parse from start
        }

        // consistency check
        bool inconsistentDb = !VerifyTransactionExistence(nWaterlineBlock);
        if (inconsistentDb) {
            nWaterlineBlock = -1; // force a clear_all_state and parse from start
        }

        if (nWaterlineBlock > 0) {
            PrintToConsole("Loading persistent state: OK [block %d]\n", nWaterlineBlock);
        } else {
            std::string strReason = "unknown";
            if (wrongDBVersion) strReason = "client version changed";
            if (noPreviousState) strReason = "no usable previous state found";
            if (startClean) strReason = "-startclean parameter used";
            if (inconsistentDb) strReason = "INCONSISTENT DB DETECTED!\n"
                    "\n!!! WARNING !!!\n\n"
                    "IF YOU ARE USING AN OVERLAY DB, YOU MAY NEED TO REPROCESS\n"
                    "ALL TRADE LAYER TRANSACTIONS TO AVOID INCONSISTENCIES!\n"
                    "\n!!! WARNING !!!";
            PrintToConsole("Loading persistent state: NONE (%s)\n", strReason);
        }

        if (nWaterlineBlock < 0) {
            // persistence says we reparse!, nuke some stuff in case the partial loads left stale bits
            clear_all_state();
        }

        if (inconsistentDb) {
            std::string strAlert("INCONSISTENT DB DETECTED! IF YOU ARE USING AN OVERLAY DB, YOU MAY NEED TO REPROCESS"
                    "ALL TRADE LAYER TRANSACTIONS TO AVOID INCONSISTENCIES!");
            AddAlert("tradelayer", ALERT_CLIENT_VERSION_EXPIRY, std::numeric_limits<uint32_t>::max(), strAlert);
            DoWarning(strAlert);
        }

        // legacy code, setting to pre-genesis-block
        int snapshotHeight = ConsensusParams().GENESIS_BLOCK - 1;

        if (nWaterlineBlock < snapshotHeight) {
            nWaterlineBlock = snapshotHeight;
        }

        // advance the waterline so that we start on the next unaccounted for block
        nWaterlineBlock += 1;


        // load feature activation messages from txlistdb and process them accordingly
        pDbTransactionList->LoadActivations(nWaterlineBlock);
    }

    {
        LOCK2(cs_main, cs_tally);
        // load all alerts from levelDB (and immediately expire old ones)
        pDbTransactionList->LoadAlerts(nWaterlineBlock);
    }

    {
        LOCK(cs_tally);
        // load the state of any freeable properties and frozen addresses from levelDB
        if (!pDbTransactionList->LoadFreezeState(nWaterlineBlock)) {
            std::string strShutdownReason = "Failed to load freeze state from levelDB.  It is unsafe to continue.\n";
            PrintToLog(strShutdownReason);
            if (!gArgs.GetBoolArg("-overrideforcedshutdown", false)) {
                DoAbortNode(strShutdownReason, strShutdownReason);
            }
        }

        // initial scan
        msc_initial_scan(nWaterlineBlock);

    }

    PrintToConsole("Trade Layer initialization completed\n");

    return 0;
}

/**
 * Global handler to shut down Trade Layer.
 *
 * In particular, the LevelDB databases of the global state objects are closed
 * properly.
 *
 * @return An exit code, indicating success or failure
 */
int mastercore_shutdown()
{
    LOCK(cs_tally);

    if (pDbTransactionList) {
        delete pDbTransactionList;
        pDbTransactionList = nullptr;
    }
    if (pDbTradeList) {
        delete pDbTradeList;
        pDbTradeList = nullptr;
    }
    if (pDbStoList) {
        delete pDbStoList;
        pDbStoList = nullptr;
    }
    if (pDbSpInfo) {
        delete pDbSpInfo;
        pDbSpInfo = nullptr;
    }
    if (pDbTransaction) {
        delete pDbTransaction;
        pDbTransaction = nullptr;
    }

    mastercoreInitialized = 0;

    PrintToLog("\nTrade Layer shutdown completed\n");
    PrintToLog("Shutdown time: %s\n", FormatISO8601DateTime(GetTime()));

    PrintToConsole("Trade Layer shutdown completed\n");

    return 0;
}

/**
 * This handler is called for every new transaction that comes in (actually in block parsing loop).
 *
 * @return True, if the transaction was an Exodus purchase, DEx payment or a valid Trade Layer transaction
 */
bool mastercore_handler_tx(const CTransaction& tx, int nBlock, unsigned int idx, const CBlockIndex* pBlockIndex, const std::shared_ptr<std::map<COutPoint, Coin> > removedCoins)
{
    extern volatile int id_contract;
    extern std::vector<std::string> vestingAddresses;

    ui128 numLog128;
    ui128 numQuad128;

    LOCK(cs_tally);

    if (!mastercoreInitialized) {
        mastercore_init();
    }

    // clear pending, if any
    // NOTE1: Every incoming TX is checked, not just MP-ones because:
    // if for some reason the incoming TX doesn't pass our parser validation steps successfully, I'd still want to clear pending amounts for that TX.
    // NOTE2: Plus I wanna clear the amount before that TX is parsed by our protocol, in case we ever consider pending amounts in internal calculations.
    PendingDelete(tx.GetHash());

    // we do not care about parsing blocks prior to our waterline (empty blockchain defense)
    if (nBlock < nWaterlineBlock) return false;
    int64_t nBlockTime = pBlockIndex->GetBlockTime();


    // int nBlockNow = GetHeight();

    /***********************************************************************/
    /** Calling The Settlement Algorithm Here **/
    /************************************************************************/

    CMPTransaction mp_obj;
    mp_obj.unlockLogic();

    int expirationBlock = 0, tradeBlock = 0, checkExpiration = 0;
    CMPSPInfo::Entry sp;
    if ( id_contract != 0 )
      {
        if (pDbSpInfo->getSP(id_contract, sp) && sp.prop_type == ALL_PROPERTY_TYPE_CONTRACT)
    {
      expirationBlock = static_cast<int>(sp.blocks_until_expiration);
      tradeBlock = static_cast<int>(pBlockIndex->nHeight);
    }
      }

    lastBlockg = static_cast<int>(pBlockIndex->nHeight);
    const CConsensusParams &params = ConsensusParams();
    vestingActivationBlock = params.MSC_VESTING_BLOCK;

    if (static_cast<int>(pBlockIndex->nHeight) == params.MSC_VESTING_BLOCK) creatingVestingTokens();

    int deadline = sp.init_block + expirationBlock;
    if ( tradeBlock != 0 && deadline != 0 ) checkExpiration = tradeBlock == deadline ? 1 : 0;

    if (checkExpiration) {
      idx_expiration += 1;
      if ( idx_expiration == 2 ) {
        expirationAchieve = 1;
      } else expirationAchieve = 0;
    } else expirationAchieve = 0;


    bool fFoundTx = false;
    int pop_ret = parseTransaction(false, tx, nBlock, idx, mp_obj, nBlockTime, removedCoins);

    if (0 == pop_ret) {
        int interp_ret = mp_obj.interpretPacket();
        if (interp_ret) PrintToLog("!!! interpretPacket() returned %d !!!\n", interp_ret);

        // Only structurally valid transactions get recorded in levelDB
        // PKT_ERROR - 2 = interpret_Transaction failed, structurally invalid payload
        if (interp_ret != PKT_ERROR - 2) {
            bool bValid = (0 <= interp_ret);
            pDbTransactionList->recordTX(tx.GetHash(), bValid, nBlock, mp_obj.getType(), mp_obj.getNewAmount());
            pDbTransaction->RecordTransaction(tx.GetHash(), idx, interp_ret);
        }

        // if interpretPacket returns 1, that means we have an instant trade between LTCs and tokens.
        if (interp_ret == 1){
            // HandleLtcInstantTrade(tx, nBlock, mp_obj.getSender(), mp_obj.getReceiver(), mp_obj.getProperty(), mp_obj.getAmountForSale(), mp_obj.getDesiredProperty(), mp_obj.getDesiredValue(), mp_obj.getIndexInBlock());

        //NOTE: we need to return this number 2 from mp_obj.interpretPacket() (tx.cpp)
        } else if (interp_ret == 2) {
            PrintToLog("%s(): interp_ret == 2 \n",__func__);
            HandleDExPayments(tx, nBlock, mp_obj.getSender());
        }

        fFoundTx |= (interp_ret == 0);
    }

    if (fFoundTx && msc_debug_consensus_hash_every_transaction) {
        uint256 consensusHash = GetConsensusHash();
        PrintToLog("Consensus hash for transaction %s: %s\n", tx.GetHash().GetHex(), consensusHash.GetHex());
    }

    return fFoundTx;
}

/**
 * Determines, whether it is valid to use a Class C transaction for a given payload size.
 *
 * @param nDataSize The length of the payload
 * @return True, if Class C is enabled and the payload is small enough
 */
bool mastercore::UseEncodingClassC(size_t nDataSize)
{
    size_t nTotalSize = nDataSize + GetTLMarker().size(); // Marker "tl"
    bool fDataEnabled = gArgs.GetBoolArg("-datacarrier", true);
    int nBlockNow = GetHeight();
    if (!IsAllowedOutputType(TX_NULL_DATA, nBlockNow)) {
        fDataEnabled = false;
    }
    return nTotalSize <= nMaxDatacarrierBytes && fDataEnabled;
}

int mastercore_handler_block_begin(int nBlockPrev, CBlockIndex const * pBlockIndex)
{
    LOCK(cs_tally);

    if (reorgRecoveryMode > 0) {
        reorgRecoveryMode = 0; // clear reorgRecovery here as this is likely re-entrant
        RewindDBsAndState(pBlockIndex->nHeight, nBlockPrev);
    }

    // handle any features that go live with this block
    CheckLiveActivations(pBlockIndex->nHeight);

    // handle any features that go live with this block
    makeWithdrawals(pBlockIndex->nHeight);
    CheckLiveActivations(pBlockIndex->nHeight);
    update_sum_upnls();
    // marginMain(pBlockIndex->nHeight);
    // addInterestPegged(nBlockPrev,pBlockIndex);
    // _my_sps->rollingContractsBlock(pBlockIndex); // NOTE: we are checking every contract expiration

    return 0;
}

// called once per block, after the block has been processed
// TODO: consolidate into *handler_block_begin() << need to adjust Accept expiry check.............
// it performs cleanup and other functions
int mastercore_handler_block_end(int nBlockNow, CBlockIndex const * pBlockIndex,
        unsigned int countMP)
{
    LOCK(cs_tally);

    if (!mastercoreInitialized) {
        mastercore_init();
    }

    // for every new received block must do:
    // 1) remove expired entries from the accept list (per spec accept entries are
    //    valid until their blocklimit expiration; because the customer can keep
    //    paying BTC for the offer in several installments)

    unsigned int how_many_erased = eraseExpiredAccepts(nBlockNow);

    if (how_many_erased) {
        PrintToLog("%s(%d); erased %u accepts this block, line %d, file: %s\n",
            __FUNCTION__, how_many_erased, nBlockNow, __LINE__, __FILE__);
    }

    // if (msc_debug_exo) {
    //     int64_t balance = GetTokenBalance(exodus_address, TL_PROPERTY_MSC, BALANCE);
    //     PrintToLog("devmsc for block %d: %d, Exodus balance: %d\n", nBlockNow, devmsc, FormatDivisibleMP(balance));
    // }

    // check the alert status, do we need to do anything else here?
    CheckExpiredAlerts(nBlockNow, pBlockIndex->GetBlockTime());

    // check that pending transactions are still in the mempool
    PendingCheck();

    // transactions were found in the block, signal the UI accordingly
    if (countMP > 0) CheckWalletUpdate(true);

    // calculate and print a consensus hash if required
    if (ShouldConsensusHashBlock(nBlockNow)) {
        uint256 consensusHash = GetConsensusHash();
        PrintToLog("Consensus hash for block %d: %s\n", nBlockNow, consensusHash.GetHex());
    }

    // request checkpoint verification
    bool checkpointValid = VerifyCheckpoint(nBlockNow, pBlockIndex->GetBlockHash());
    if (!checkpointValid) {
        // failed checkpoint, can't be trusted to provide valid data - shutdown client
        const std::string& msg = strprintf(
                "Shutting down due to failed checkpoint for block %d (hash %s). "
                "Please restart with -startclean flag and if this doesn't work, please reach out to the support.\n",
                nBlockNow, pBlockIndex->GetBlockHash().GetHex());
        PrintToLog(msg);
        if (!gArgs.GetBoolArg("-overrideforcedshutdown", false)) {
            fs::path persistPath = GetDataDir() / "MP_persist";
            if (fs::exists(persistPath)) fs::remove_all(persistPath); // prevent the node being restarted without a reparse after forced shutdown
            DoAbortNode(msg, msg);
        }
    } else {
        // save out the state after this block
        if (IsPersistenceEnabled(nBlockNow) && nBlockNow >= ConsensusParams().GENESIS_BLOCK) {
            PersistInMemoryState(pBlockIndex);
        }
    }

    return 0;
}

void mastercore_handler_disc_begin(const int nHeight)
{
    LOCK(cs_tally);

    reorgRecoveryMode = 1;
    reorgRecoveryMaxHeight = (nHeight > reorgRecoveryMaxHeight) ? nHeight: reorgRecoveryMaxHeight;
}

rational_t mastercore::notionalChange(uint32_t contractId)
{
    rational_t inversePrice;
    if (globalDenPrice != 0) {
        inversePrice = rational_t(globalNumPrice,globalDenPrice);
    } else {
        inversePrice = rational_t(1,1);
    }

    return inversePrice;
}


void Filling_Twap_Vec(std::map<uint32_t, std::vector<uint64_t>> &twap_ele, std::map<uint32_t, std::vector<uint64_t>> &twap_vec,
		      uint32_t property_traded, uint32_t property_desired, uint64_t effective_price)
{
  int nBlockNow = GetHeight();
  std::vector<uint64_t> twap_minmax;
  if (msc_debug_tradedb) PrintToLog("\nCheck here CDEx:\t nBlockNow = %d\t twapBlockg = %d\n", nBlockNow, twapBlockg);

  if (nBlockNow == twapBlockg)
    twap_ele[property_traded].push_back(effective_price);
  else
    {
      if (twap_ele[property_traded].size() != 0)
	{
	  twap_minmax = min_max(twap_ele[property_traded]);
	  uint64_t numerator = twap_ele[property_traded].front()+twap_minmax[0]+twap_minmax[1]+twap_ele[property_traded].back();
	  rational_t twapRat(numerator/COIN, 4);
	  int64_t twap_elej = mastercore::RationalToInt64(twapRat);
	  if (msc_debug_tradedb) PrintToLog("\ntwap_elej CDEx = %s\n", FormatDivisibleMP(twap_elej));
	  cdextwap_vec[property_traded].push_back(twap_elej);
	}
      twap_ele[property_traded].clear();
      twap_ele[property_traded].push_back(effective_price);
    }
  twapBlockg = nBlockNow;
}

bool callingPerpetualSettlement(double globalPNLALL_DUSD, int64_t globalVolumeALL_DUSD, int64_t volumeToCompare)
{
  bool perpetualBool = false;

  if ( globalPNLALL_DUSD == 0 )
    {
      // PrintToLog("\nLiquidate Forward Positions\n");
      perpetualBool = true;
    }
  else if ( globalVolumeALL_DUSD > volumeToCompare )
    {
      // PrintToLog("\nTake decisions for globalVolumeALL_DUSD > volumeToCompare\n");
    }

  return perpetualBool;
}

void fillingMatrix(MatrixTLS &M_file, MatrixTLS &ndatabase, std::vector<std::map<std::string, std::string>> &path_ele)
{
  for (unsigned int i = 0; i < path_ele.size(); ++i)
    {
      M_file[i][0] = ndatabase[i][0] = path_ele[i]["addrs_src"];
      M_file[i][1] = ndatabase[i][1] = path_ele[i]["status_src"];
      M_file[i][2] = ndatabase[i][2] = path_ele[i]["lives_src"];
      M_file[i][3] = ndatabase[i][3] = path_ele[i]["addrs_trk"];
      M_file[i][4] = ndatabase[i][4] = path_ele[i]["status_trk"];
      M_file[i][5] = ndatabase[i][5] = path_ele[i]["lives_trk"];
      M_file[i][6] = ndatabase[i][6] = path_ele[i]["amount_trd"];
      M_file[i][7] = ndatabase[i][7] = path_ele[i]["matched_price"];
      M_file[i][8] = ndatabase[i][8] = path_ele[i]["amount_trd"];
      M_file[i][9] = ndatabase[i][9] = path_ele[i]["amount_trd"];
    }
}

double PNL_function(double entry_price, double exit_price, int64_t amount_trd, std::string netted_status)
{
  double PNL = 0;

  if ( finding_string("Long", netted_status) )
    PNL = (double)amount_trd*(1/(double)entry_price-1/(double)exit_price);
  else if ( finding_string("Short", netted_status) )
    PNL = (double)amount_trd*(1/(double)exit_price-1/(double)entry_price);

  return PNL;
}

void loopForUPNL(std::vector<std::map<std::string, std::string>> path_ele, std::vector<std::map<std::string, std::string>> path_eleh, unsigned int path_length, std::string address1, std::string address2, std::string status1, std::string status2, double &UPNL1, double &UPNL2, uint64_t exit_price, int64_t nCouldBuy0)
{
  std::vector<std::map<std::string, std::string>>::iterator it_path_ele;

  double entry_pricesrc = 0, entry_pricetrk = 0;
  double exit_priceh = (double)exit_price/COIN;

  int idx_price_src = 0, idx_price_trk = 0;
  uint64_t entry_pricesrc_num = 0, entry_pricetrk_num = 0;
  std::string addrs_upnl = address1;
  unsigned int limSup = path_ele.size()-path_length;
  uint64_t amount_src = 0, amount_trk = 0;
  std::string status_src = "", status_trk = "";

  loopforEntryPrice(path_ele, path_eleh, address1, status1, entry_pricesrc, idx_price_src, entry_pricesrc_num, limSup, exit_priceh, amount_src, status_src);
  // PrintToLog("\nentry_pricesrc = %d, address1 = %s, exit_price = %d, amount_src = %d\n", entry_pricesrc, address1, exit_priceh, amount_src);
  UPNL1 = PNL_function(entry_pricesrc, exit_priceh, amount_src, status_src);

  loopforEntryPrice(path_ele, path_eleh, address2, status2, entry_pricetrk, idx_price_trk, entry_pricetrk_num, limSup, exit_priceh, amount_trk, status_trk);
  // PrintToLog("\nentry_pricetrk = %d, address2 = %s, exit_price = %d, amount_trk = %d\n", entry_pricetrk, address2, exit_priceh, amount_src);
  UPNL2 = PNL_function(entry_pricetrk, exit_priceh, amount_trk, status_trk);
}

void loopforEntryPrice(std::vector<std::map<std::string, std::string>> path_ele, std::vector<std::map<std::string, std::string>> path_eleh, std::string addrs_upnl, std::string status_match, double &entry_price, int &idx_price, uint64_t entry_price_num, unsigned int limSup, double exit_priceh, uint64_t &amount, std::string &status)
{
  std::vector<std::map<std::string, std::string>>::reverse_iterator reit_path_ele;
  std::vector<std::map<std::string, std::string>>::iterator it_path_eleh;
  double price_num_w = 0;
  extern VectorTLS *pt_changepos_status; VectorTLS &changepos_status  = *pt_changepos_status;

  if (finding(status_match, changepos_status))
    {
      for (it_path_eleh = path_eleh.begin(); it_path_eleh != path_eleh.end(); ++it_path_eleh)
      	{
      	  if (addrs_upnl == (*it_path_eleh)["addrs_src"] && !finding_string("Netted", (*it_path_eleh)["status_src"]))
      	    {
      	      price_num_w += stod((*it_path_eleh)["matched_price"])*stod((*it_path_eleh)["amount_trd"]);
      	      amount += static_cast<uint64_t>(stol((*it_path_eleh)["amount_trd"]));
      	    }
	  if (addrs_upnl == (*it_path_eleh)["addrs_trk"] && !finding_string("Netted", (*it_path_eleh)["status_trk"]))
      	    {
      	      price_num_w += stod((*it_path_eleh)["matched_price"])*stod((*it_path_eleh)["amount_trd"]);
      	      amount += static_cast<uint64_t>(stol((*it_path_eleh)["amount_trd"]));
      	    }
      	}
      entry_price = price_num_w/(double)amount;

      for (reit_path_ele = path_eleh.rbegin(); reit_path_ele != path_eleh.rend(); ++reit_path_ele)
	{
	  if (addrs_upnl == (*reit_path_ele)["addrs_src"] && finding_string("Open", (*reit_path_ele)["status_src"]))
	    {
	      status = (*reit_path_ele)["status_src"];
	    }
	  else if (addrs_upnl == (*reit_path_ele)["addrs_trk"] && finding_string("Open", (*reit_path_ele)["status_trk"]))
	    {
	      status = (*reit_path_ele)["status_trk"];
	    }
	}
    }
  else
    {
      // PrintToLog("\nLoop in the Path Element\n");
      for (it_path_eleh = path_eleh.begin(); it_path_eleh != path_eleh.end(); ++it_path_eleh)
      	{
      	  if (addrs_upnl == (*it_path_eleh)["addrs_src"] || addrs_upnl == (*it_path_eleh)["addrs_trk"])
      	    {
      	      printing_edges_database(*it_path_eleh);
      	      price_num_w += stod((*it_path_eleh)["matched_price"])*stod((*it_path_eleh)["amount_trd"]);
      	      amount += static_cast<uint64_t>(stol((*it_path_eleh)["amount_trd"]));
      	    }
      	}

      // PrintToLog("\nInside LoopForEntryPrice:\n");
      for (reit_path_ele = path_ele.rbegin()+limSup; reit_path_ele != path_ele.rend(); ++reit_path_ele)
	{
	  if (addrs_upnl == (*reit_path_ele)["addrs_src"])
	    {
	      if (finding_string("Open", (*reit_path_ele)["status_src"]) || finding_string("Increased", (*reit_path_ele)["status_src"]))
		{
		  // PrintToLog("\nRow Reverse Loop for addrs_upnl = %s\n", addrs_upnl);
		  printing_edges_database(*reit_path_ele);
		  idx_price += 1;
		  entry_price_num += static_cast<uint64_t>(stol((*reit_path_ele)["matched_price"]));
		  amount += static_cast<uint64_t>(stol((*reit_path_ele)["amount_trd"]));

		  price_num_w += stod((*reit_path_ele)["matched_price"])*stod((*reit_path_ele)["amount_trd"]);

		  if (finding_string("Open", (*reit_path_ele)["status_src"]))
		    {
		      // PrintToLog("\naddrs = %s, price_num_w trk = %d, amount = %d\n", addrs_upnl, price_num_w, amount);
		      entry_price = price_num_w/(double)amount;
		      status = (*reit_path_ele)["status_src"];
		      break;
		    }
		}
	    }
	  else if ( addrs_upnl == (*reit_path_ele)["addrs_trk"])
	    {
	      if (finding_string("Open", (*reit_path_ele)["status_trk"]) || finding_string("Increased", (*reit_path_ele)["status_trk"]))
		{
		  // PrintToLog("\nRow Reverse Loop for addrs_upnl = %s\n", addrs_upnl);
		  printing_edges_database(*reit_path_ele);
		  idx_price += 1;
		  entry_price_num += static_cast<uint64_t>(stol((*reit_path_ele)["matched_price"]));
		  amount += static_cast<uint64_t>(stol((*reit_path_ele)["amount_trd"]));

		  price_num_w += stod((*reit_path_ele)["matched_price"])*stod((*reit_path_ele)["amount_trd"]);

		  if (finding_string("Open", (*reit_path_ele)["status_trk"]))
		    {
		      // PrintToLog("\naddrs = %s, price_num_w trk = %d, amount = %d\n", addrs_upnl, price_num_w, amount);
		      entry_price = price_num_w/(double)amount;
		      status = (*reit_path_ele)["status_trk"];
		      break;
		    }
		}
	    }
	  else
	    continue;
	}
      if (idx_price == 0)
	{
	  entry_price = exit_priceh;
	  status = status_match;
	}
    }
}


inline int64_t clamp_function(int64_t diff, int64_t nclamp)
{
  int64_t interest = diff > nclamp ? diff-nclamp+1 : (diff < -nclamp ? diff+nclamp+1 : 0.01);
  return interest;
}

void printing_edges_database(std::map<std::string, std::string> &path_ele)
{
  PrintToLog("{ addrs_src : %s , status_src : %s, lives_src : %d, addrs_trk : %s , status_trk : %s, lives_trk : %d, amount_trd : %d, matched_price : %d, edge_row : %d, ghost_edge : %d }\n", path_ele["addrs_src"], path_ele["status_src"], path_ele["lives_src"], path_ele["addrs_trk"], path_ele["status_trk"], path_ele["lives_trk"], path_ele["amount_trd"], path_ele["matched_price"], path_ele["edge_row"], path_ele["ghost_edge"]);
}

bool mastercore::marginMain(int Block)
{
  //checking in map for address and the UPNL.
    if(msc_debug_margin_main) PrintToLog("%s: Block in marginMain: %d\n", __func__, Block);
    LOCK(cs_tally);
    uint32_t nextSPID = pDbSpInfo->peekNextSPID(1);
    for (uint32_t contractId = 1; contractId < nextSPID; contractId++)
    {
        CMPSPInfo::Entry sp;
        if (pDbSpInfo->getSP(contractId, sp))
        {
            if(msc_debug_margin_main) PrintToLog("%s: Property Id: %d\n", __func__, contractId);
            if (sp.prop_type != ALL_PROPERTY_TYPE_CONTRACT)
            {
                if(msc_debug_margin_main) PrintToLog("%s: Property is not future contract\n",__func__);
                continue;
            }
        }

        uint32_t collateralCurrency = sp.collateral_currency;
        //int64_t notionalSize = static_cast<int64_t>(sp.notional_size);

        // checking the upnl map
        std::map<uint32_t, std::map<std::string, double>>::iterator it = addrs_upnlc.find(contractId);
        std::map<std::string, double> upnls = it->second;

        //  if upnls is < 0, we need to cancel orders or liquidate contracts.
        for(std::map<std::string, double>::iterator it2 = upnls.begin(); it2 != upnls.end(); ++it2)
	  {
            const std::string address = it2->first;

            int64_t upnl = static_cast<int64_t>(it2->second * factorE);
            if(msc_debug_margin_main) PrintToLog("%s: upnl: %d", __func__, upnl);
            // if upnl is positive, keep searching
            if (upnl >= 0)
                continue;

            if(msc_debug_margin_main)
            {
                PrintToLog("%s: upnl: %d", __func__, upnl);
                PrintToLog("%s: sum_check_upnl: %d",__func__, sum_check_upnl(address));
            }
            // if sum of upnl is bigger than this upnl, skip address.
            if (sum_check_upnl(address) > upnl)
                continue;

            // checking position margin
            int64_t posMargin = pos_margin(contractId, address, sp.prop_type, sp.margin_requirement);

            // if there's no position, something is wrong!
            if (posMargin < 0)
                continue;

            // checking the initMargin (init_margin = position_margin + active_orders_margin)
            std::string channelAddr;
            int64_t initMargin;

            initMargin = GetTokenBalance(address,collateralCurrency,CONTRACTDEX_MARGIN);

            rational_t percent = rational_t(-upnl,initMargin);

            int64_t ordersMargin = initMargin - posMargin;

            if(msc_debug_margin_main)
            {
                PrintToLog("\n--------------------------------------------------\n");
                PrintToLog("\n%s: initMargin= %d\n", __func__, initMargin);
                PrintToLog("\n%s: positionMargin= %d\n", __func__, posMargin);
                PrintToLog("\n%s: ordersMargin= %d\n", __func__, ordersMargin);
                PrintToLog("%s: upnl= %d\n", __func__, upnl);
                PrintToLog("%s: factor= %d\n", __func__, factor);
                PrintToLog("%s: proportion upnl/initMargin= %d\n", __func__, xToString(percent));
                PrintToLog("\n--------------------------------------------------\n");
            }
            // if the upnl loss is more than 80% of the initial Margin
            if (factor <= percent)
            {
                const uint256 txid;
                unsigned char ecosystem = '\0';
                if(msc_debug_margin_main)
                {
                    PrintToLog("%s: factor <= percent : %d <= %d\n",__func__, xToString(factor), xToString(percent));
                    PrintToLog("%s: margin call!\n", __func__);
                }

                ContractDex_CLOSE_POSITION(txid, Block, address, ecosystem, contractId, collateralCurrency);
                continue;

            // if the upnl loss is more than 20% and minus 80% of the Margin
            } else if (factor2 <= percent) {
                if(msc_debug_margin_main)
                {
                    PrintToLog("%s: CALLING CANCEL IN ORDER\n", __func__);
                    PrintToLog("%s: factor2 <= percent : %s <= %s\n", __func__, xToString(factor2),xToString(percent));
                }

                int64_t fbalance, diff;
                int64_t margin = GetTokenBalance(address,collateralCurrency,CONTRACTDEX_MARGIN);
                int64_t ibalance = GetTokenBalance(address,collateralCurrency, BALANCE);
                int64_t left = - 0.2 * margin - upnl;

                bool orders = false;

                do
                {
                      if(msc_debug_margin_main) PrintToLog("%s: margin before cancel: %s\n", __func__, margin);
                      if(ContractDex_CANCEL_IN_ORDER(address, contractId) == 1)
                          orders = true;
                      fbalance = GetTokenBalance(address,collateralCurrency, BALANCE);
                      diff = fbalance - ibalance;

                      if(msc_debug_margin_main)
                      {
                          PrintToLog("%s: ibalance: %s\n", __func__, ibalance);
                          PrintToLog("%s: fbalance: %s\n", __func__, fbalance);
                          PrintToLog("%s: diff: %d\n", __func__, diff);
                          PrintToLog("%s: left: %d\n", __func__, left);
                      }

                      if ( left <= diff && msc_debug_margin_main) {
                          PrintToLog("%s: left <= diff !\n", __func__);
                      }

                      if (orders) {
                          PrintToLog("%s: orders=true !\n", __func__);
                      } else
                         PrintToLog("%s: orders=false\n", __func__);

                } while(diff < left && !orders);

                // if left is negative, the margin is above the first limit (more than 80% maintMargin)
                if (0 < left)
                {
                    if(msc_debug_margin_main)
                    {
                        PrintToLog("%s: orders can't cover, we have to check the balance to refill margin\n", __func__);
                        PrintToLog("%s: left: %d\n", __func__, left);
                    }
                    //we have to see if we can cover this with the balance
                    int64_t balance = GetTokenBalance(address,collateralCurrency,BALANCE);

                    if(balance >= left) // recover to 80% of maintMargin
                    {
                        if(msc_debug_margin_main) PrintToLog("\n%s: balance >= left\n", __func__);
                        update_tally_map(address, collateralCurrency, -left, BALANCE);
                        update_tally_map(address, collateralCurrency, left, CONTRACTDEX_MARGIN);
                        continue;

                    } else { // not enough money in balance to recover margin, so we use position

                         if(msc_debug_margin_main) PrintToLog("%s: not enough money in balance to recover margin, so we use position\n", __func__);
                         if (balance > 0)
                         {
                             update_tally_map(address, collateralCurrency, -balance, BALANCE);
                             update_tally_map(address, collateralCurrency, balance, CONTRACTDEX_MARGIN);
                         }

                         const uint256 txid;
                         unsigned int idx = 0;
                         uint8_t option;
                         int64_t fcontracts;

                         int64_t longs = GetTokenBalance(address,contractId,POSSITIVE_BALANCE);
                         int64_t shorts = GetTokenBalance(address,contractId,NEGATIVE_BALANCE);

                         if(msc_debug_margin_main) PrintToLog("%s: longs: %d,shorts: %d \n", __func__, longs,shorts);

                         (longs > 0 && shorts == 0) ? option = SELL, fcontracts = longs : option = BUY, fcontracts = shorts;

                         if(msc_debug_margin_main) PrintToLog("%s: option: %d, upnl: %d, posMargin: %d\n", __func__, option,upnl,posMargin);

                         arith_uint256 contracts = DivideAndRoundUp(ConvertTo256(posMargin) + ConvertTo256(-upnl), ConvertTo256(static_cast<int64_t>(sp.margin_requirement)));
                         int64_t icontracts = ConvertTo64(contracts);

                         if(msc_debug_margin_main)
                         {
                             PrintToLog("%s: icontracts: %d\n", __func__, icontracts);
                             PrintToLog("%s: fcontracts before: %d\n", __func__, fcontracts);
                         }

                         if (icontracts > fcontracts)
                             icontracts = fcontracts;

                         if(msc_debug_margin_main) PrintToLog("%s: fcontracts after: %d\n", __func__, fcontracts);

                         ContractDex_ADD_MARKET_PRICE(address, contractId, icontracts, Block, txid, idx, option, 0);


                    }

                }

            } else {
                if(msc_debug_margin_main) PrintToLog("%s: the upnl loss is LESS than 20% of the margin, nothing happen\n", __func__);

            }
        }
    }

    return true;

}


int64_t mastercore::sum_check_upnl(std::string address)
{
    std::map<std::string, int64_t>::iterator it = sum_upnls.find(address);
    int64_t upnl = it->second;
    return upnl;
}


void mastercore::update_sum_upnls()
{
    //cleaning the sum_upnls map
    if(!sum_upnls.empty())
        sum_upnls.clear();

    LOCK(cs_tally);
    uint32_t nextSPID = pDbSpInfo->peekNextSPID(1);

    for (uint32_t contractId = 1; contractId < nextSPID; contractId++)
    {
        CMPSPInfo::Entry sp;
        if (pDbSpInfo->getSP(contractId, sp))
        {
            if (sp.prop_type != ALL_PROPERTY_TYPE_CONTRACT)
                continue;

            std::map<uint32_t, std::map<std::string, double>>::iterator it = addrs_upnlc.find(contractId);
            std::map<std::string, double> upnls = it->second;

            for(std::map<std::string, double>::iterator it1 = upnls.begin(); it1 != upnls.end(); ++it1)
            {
                const std::string address = it1->first;
                int64_t upnl = static_cast<int64_t>(it1->second * factorE);

                //add this in the sumupnl vector
                sum_upnls[address] += upnl;
            }

        }
    }
}

/* margin needed for a given position */
int64_t mastercore::pos_margin(uint32_t contractId, std::string address, uint16_t prop_type, uint32_t margin_requirement)
{
        arith_uint256 maintMargin;

        if (prop_type != ALL_PROPERTY_TYPE_CONTRACT)
        {
            if(msc_debug_pos_margin) PrintToLog("%s: this is not a future contract\n", __func__);
            return -1;
        }

        int64_t longs = GetTokenBalance(address,contractId,POSSITIVE_BALANCE);
        int64_t shorts = GetTokenBalance(address,contractId,NEGATIVE_BALANCE);

        if(msc_debug_pos_margin)
        {
            PrintToLog("%s: longs: %d, shorts: %d\n", __func__, longs,shorts);
            PrintToLog("%s: margin requirement: %d\n", __func__, margin_requirement);
        }

        if (longs > 0 && shorts == 0)
        {
            maintMargin = (ConvertTo256(longs) * ConvertTo256(static_cast<int64_t>(margin_requirement))) / ConvertTo256(factorE);
        } else if (shorts > 0 && longs == 0){
            maintMargin = (ConvertTo256(shorts) * ConvertTo256(static_cast<int64_t>(margin_requirement))) / ConvertTo256(factorE);
        } else {
            if(msc_debug_pos_margin) PrintToLog("%s: there's no position avalaible\n", __func__);
            return -2;
        }

        int64_t maint_margin = ConvertTo64(maintMargin);
        if(msc_debug_pos_margin) PrintToLog("%s: maint margin: %d\n", __func__, maint_margin);
        return maint_margin;
}


int64_t setPosition(int64_t positive, int64_t negative)
{
    if (positive > 0 && negative == 0)
        return positive;
    else if (positive == 0 && negative > 0)
        return -negative;
    else
        return 0;
}

std::string updateStatus(int64_t oldPos, int64_t newPos)
{

    PrintToLog("%s: old position: %d, new position: %d \n", __func__, oldPos, newPos);

    if(oldPos == 0 && newPos > 0)
        return "OpenLongPosition";

    else if (oldPos == 0 && newPos < 0)
        return "OpenShortPosition";

    else if (oldPos > newPos && oldPos > 0 && newPos > 0)
        return "LongPosNettedPartly";

    else if (oldPos < newPos && oldPos < 0 && newPos < 0)
        return "ShortPosNettedPartly";

    else if (oldPos < newPos && oldPos > 0 && newPos > 0)
        return "LongPosIncreased";

    else if (oldPos > newPos && oldPos < 0 && newPos < 0)
        return "ShortPosIncreased";

    else if (newPos == 0 && oldPos > 0)
        return "LongPosNetted";

    else if (newPos == 0 && oldPos < 0)
        return "ShortPosNetted";

    else if (newPos > 0 && oldPos < 0)
        return "OpenLongPosByShortPosNetted";

    else if (newPos < 0 && oldPos > 0)
        return "OpenShortPosByLongPosNetted";
    else
        return "None";
}

bool mastercore::ContInst_Fees(const std::string& firstAddr,const std::string& secondAddr,const std::string& channelAddr, int64_t amountToReserve,uint16_t type, uint32_t colateral)
{
    arith_uint256 fee;

    if(msc_debug_contract_inst_fee)
    {
        PrintToLog("%s: firstAddr: %d\n", __func__, firstAddr);
        PrintToLog("%s: secondAddr: %d\n", __func__, secondAddr);
        PrintToLog("%s: amountToReserve: %d\n", __func__, amountToReserve);
        PrintToLog("%s: colateral: %d\n", __func__,colateral);
    }

    if (type == ALL_PROPERTY_TYPE_CONTRACT)
    {
        // 0.5% minus for firstAddr, 0.5% minus for secondAddr
        fee = (ConvertTo256(amountToReserve) * ConvertTo256(5)) / ConvertTo256(1000);

    } else if (type == ALL_PROPERTY_TYPE_ORACLE_CONTRACT){
        // 1.25% minus each
        fee = (ConvertTo256(amountToReserve) * ConvertTo256(5)) / (ConvertTo256(4000) * ConvertTo256(COIN));
    }

    int64_t uFee = ConvertTo64(fee);


    // checking if each address can pay the totalAmount + uFee:

    int64_t totalAmount = uFee + amountToReserve;
    int64_t firstRem = static_cast<int64_t>(pDbTradeList->getRemaining(channelAddr, firstAddr, colateral));

    if (firstRem < totalAmount)
    {

            if(msc_debug_contract_inst_fee) PrintToLog("%s:address %s doesn't have enough money %d\n", __func__, firstAddr);

            return false;
    }

    int64_t secondRem = static_cast<int64_t>(pDbTradeList->getRemaining(channelAddr, secondAddr, colateral));

    if (secondRem < totalAmount)
    {
            if(msc_debug_contract_inst_fee) PrintToLog("%s:address %s doesn't have enough money %d\n", __func__, secondAddr);
            return false;
    }


    if(msc_debug_contract_inst_fee) PrintToLog("%s: uFee: %d\n",__func__,uFee);

    update_tally_map(channelAddr, colateral, -2*uFee, CHANNEL_RESERVE);

    // % to feecache
    cachefees[colateral] += 2*uFee;

    return true;
}



bool mastercore::Instant_x_Trade(const uint256& txid, uint8_t tradingAction, std::string& channelAddr, std::string& firstAddr, std::string& secondAddr, uint32_t property, int64_t amount_forsale, uint64_t price, int block, int tx_idx)
{

    int64_t firstPoss = GetTokenBalance(firstAddr, property, POSSITIVE_BALANCE);
    int64_t firstNeg = GetTokenBalance(firstAddr, property, NEGATIVE_BALANCE);
    int64_t secondPoss = GetTokenBalance(secondAddr, property, POSSITIVE_BALANCE);
    int64_t secondNeg = GetTokenBalance(secondAddr, property, NEGATIVE_BALANCE);

    if(tradingAction == SELL)
        amount_forsale = -amount_forsale;

    int64_t first_p = firstPoss - firstNeg + amount_forsale;
    int64_t second_p = secondPoss - secondNeg - amount_forsale;

    if(first_p > 0)
    {
        assert(update_tally_map(firstAddr, property, first_p - firstPoss, POSSITIVE_BALANCE));
        if(firstNeg != 0)
            assert(update_tally_map(firstAddr, property, -firstNeg, NEGATIVE_BALANCE));

    } else if (first_p < 0){
        assert(update_tally_map(firstAddr, property, -first_p - firstNeg, NEGATIVE_BALANCE));
        if(firstPoss != 0)
            assert(update_tally_map(firstAddr, property, -firstPoss, POSSITIVE_BALANCE));

    } else {  //cleaning the tally

        if(firstPoss != 0)
            assert(update_tally_map(firstAddr, property, -firstPoss, POSSITIVE_BALANCE));
        else if (firstNeg != 0)
            assert(update_tally_map(firstAddr, property, -firstNeg, NEGATIVE_BALANCE));

    }

    if(second_p > 0){
        assert(update_tally_map(secondAddr, property, second_p - secondPoss, POSSITIVE_BALANCE));
        if (secondNeg != 0)
            assert(update_tally_map(secondAddr, property, -secondNeg, NEGATIVE_BALANCE));

    } else if (second_p < 0){
        assert(update_tally_map(secondAddr, property, -second_p - secondNeg, NEGATIVE_BALANCE));
        if (secondPoss != 0)
            assert(update_tally_map(secondAddr, property, -secondPoss, POSSITIVE_BALANCE));

    } else {

        if (secondPoss != 0)
            assert(update_tally_map(secondAddr, property, -secondPoss, POSSITIVE_BALANCE));
        else if (secondNeg != 0)
            assert(update_tally_map(secondAddr, property, -secondNeg, NEGATIVE_BALANCE));
    }

  // fees here?

  std::string Status_s0 = "EmptyStr", Status_s1 = "EmptyStr", Status_s2 = "EmptyStr", Status_s3 = "EmptyStr";
  std::string Status_b0 = "EmptyStr", Status_b1 = "EmptyStr", Status_b2 = "EmptyStr", Status_b3 = "EmptyStr";

  // old positions
  int64_t oldFrs = setPosition(firstPoss,firstNeg);
  int64_t oldSec = setPosition(secondPoss,secondNeg);

  std::string Status_maker0 = updateStatus(oldFrs,first_p);
  std::string Status_taker0 = updateStatus(oldSec,second_p);

  if(msc_debug_instant_x_trade)
  {
      PrintToLog("%s: old first position: %d, old second position: %d \n", __func__, oldFrs, oldSec);
      PrintToLog("%s: new first position: %d, new second position: %d \n", __func__, first_p, second_p);
      PrintToLog("%s: Status_marker0: %s, Status_taker0: %s \n",__func__,Status_maker0, Status_taker0);
      PrintToLog("%s: amount_forsale: %d\n", __func__, amount_forsale);
  }

  uint64_t amountTraded ,newPosMaker ,newPosTaker;

  (amount_forsale < 0) ? amountTraded = static_cast<uint64_t>(-amount_forsale) : amountTraded = static_cast<uint64_t>(amount_forsale);

  (first_p < 0) ? newPosMaker = static_cast<uint64_t>(-first_p) : newPosMaker = static_cast<uint64_t>(first_p);

  (second_p < 0) ? newPosTaker = static_cast<uint64_t>(-second_p) : newPosTaker = static_cast<uint64_t>(second_p);

  if(msc_debug_instant_x_trade)
  {
      PrintToLog("%s: newPosMaker: %d\n", __func__, newPosMaker);
      PrintToLog("%s: newPosTaker: %d\n", __func__, newPosTaker);
      PrintToLog("%s: amountTraded: %d\n", __func__, amountTraded);
  }

  // (const uint256 txid1, const uint256 txid2, string address1, string address2, uint64_t effective_price, uint64_t amount_maker, uint64_t amount_taker, int blockNum1, int blockNum2, uint32_t property_traded, string tradeStatus, int64_t lives_s0, int64_t lives_s1, int64_t lives_s2, int64_t lives_s3, int64_t lives_b0, int64_t lives_b1, int64_t lives_b2, int64_t lives_b3, string s_maker0, string s_taker0, string s_maker1, string s_taker1, string s_maker2, string s_taker2, string s_maker3, string //s_taker3, int64_t nCouldBuy0, int64_t nCouldBuy1, int64_t nCouldBuy2, int64_t nCouldBuy3,uint64_t amountpnew, uint64_t amountpold)


    pDbTradeList->recordMatchedTrade(txid,
         txid,
         firstAddr,
         secondAddr,
         price,
         amountTraded,
         amountTraded,
         block,
         block,
         property,
         "Matched",
         newPosMaker,
         0,
         0,
         0,
         newPosTaker,
         0,
         0,
         0,
         Status_maker0,
         Status_taker0,
         "EmptyStr",
         "EmptyStr",
         "EmptyStr",
         "EmptyStr",
         "EmptyStr",
         "EmptyStr",
         amountTraded,
         0,
         0,
         0,
         amountTraded,
         amountTraded);

  return true;
}

bool mastercore::makeWithdrawals(int Block)
{
    for(std::map<std::string,vector<withdrawalAccepted>>::iterator it = withdrawal_Map.begin(); it != withdrawal_Map.end(); ++it)
    {
        std::string channelAddress = it->first;

        vector<withdrawalAccepted> &accepted = it->second;

        for (std::vector<withdrawalAccepted>::iterator itt = accepted.begin() ; itt != accepted.end();)
        {
            withdrawalAccepted wthd = *itt;

            const int deadline = wthd.deadline_block;

            if (Block != deadline)
            {
                ++itt;
                continue;
            }

            const std::string address = wthd.address;
            const uint32_t propertyId = wthd.propertyId;
            const int64_t amount = static_cast<int64_t>(wthd.amount);


            PrintToLog("%s: withdrawal: block: %d, deadline: %d, address: %s, propertyId: %d, amount: %d \n", __func__, Block, deadline, address, propertyId, amount);

            // updating tally map
            assert(update_tally_map(address, propertyId, amount, BALANCE));
            assert(update_tally_map(channelAddress, propertyId, -amount, CHANNEL_RESERVE));

            // deleting element from vector
            accepted.erase(itt);

        }

    }

    return true;

}

/**
 * @return The marker for class C transactions.
 */
const std::vector<unsigned char> GetTLMarker()
{
    static unsigned char pch[] = {0x6f, 0x6d, 0x6e, 0x69}; // Hex-encoded: "tdly"

    return std::vector<unsigned char>(pch, pch + sizeof(pch) / sizeof(pch[0]));
}
