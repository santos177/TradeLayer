#ifndef BITCOIN_TRADELAYER_TRADELAYER_H
#define BITCOIN_TRADELAYER_TRADELAYER_H

class CBlockIndex;
class CCoinsView;
class CCoinsViewCache;
class CTransaction;
class Coin;

#include <tradelayer/log.h>
#include <tradelayer/tally.h>
#include <tradelayer/dbtradelist.h>

#include <script/standard.h>
#include <sync.h>
#include <uint256.h>
#include <util/system.h>

#include <univalue.h>

#include <stdint.h>

#include <map>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>

#include <boost/lexical_cast.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <boost/filesystem/path.hpp>

#include "tradelayer_matrices.h"

typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;

int const MAX_STATE_HISTORY = 50;
int const STORE_EVERY_N_BLOCK = 10000;

#define TEST_ECO_PROPERTY_1 (0x80000003UL)

// increment this value to force a refresh of the state (similar to --startclean)
#define DB_VERSION 7

// could probably also use: int64_t maxInt64 = std::numeric_limits<int64_t>::max();
// maximum numeric values from the spec:
#define MAX_INT_8_BYTES (9223372036854775807UL)

// maximum size of string fields
#define SP_STRING_FIELD_LEN 256

// Trade Layer Transaction (Packet) Version
#define MP_TX_PKT_V0  0
#define MP_TX_PKT_V1  1


// Transaction types, from the spec
enum TransactionType {
  MSC_TYPE_SIMPLE_SEND                         =  0,
  MSC_TYPE_RESTRICTED_SEND                     =  2,
  MSC_TYPE_SEND_TO_OWNERS                      =  3,
  MSC_TYPE_SEND_ALL                            =  4,
  MSC_TYPE_SEND_VESTING                        =  5,
  MSC_TYPE_SAVINGS_MARK                        = 10,
  MSC_TYPE_SAVINGS_COMPROMISED                 = 11,
  MSC_TYPE_RATELIMITED_MARK                    = 12,
  MSC_TYPE_AUTOMATIC_DISPENSARY                = 15,
  MSC_TYPE_TRADE_OFFER                         = 20,
  MSC_TYPE_DEX_BUY_OFFER                       = 21,
  MSC_TYPE_ACCEPT_OFFER_BTC                    = 22,
  MSC_TYPE_METADEX_TRADE                       = 25,
  MSC_TYPE_METADEX_CANCEL_PRICE                = 26,
  MSC_TYPE_METADEX_CANCEL_PAIR                 = 27,
  MSC_TYPE_METADEX_CANCEL_ECOSYSTEM            = 28,
  MSC_TYPE_NOTIFICATION                        = 31,
  MSC_TYPE_CONTRACTDEX_TRADE                   = 29,
  MSC_TYPE_CONTRACTDEX_CANCEL_PRICE            = 30,
  MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM        = 32,
  MSC_TYPE_CONTRACTDEX_CLOSE_POSITION          = 33,
  MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK  = 34,
  MSC_TYPE_OFFER_ACCEPT_A_BET                  = 40,
  MSC_TYPE_CREATE_CONTRACT                     = 41,
  MSC_TYPE_CREATE_PROPERTY_FIXED               = 50,
  MSC_TYPE_CREATE_PROPERTY_VARIABLE            = 51,
  MSC_TYPE_PROMOTE_PROPERTY                    = 52,
  MSC_TYPE_CLOSE_CROWDSALE                     = 53,
  MSC_TYPE_CREATE_PROPERTY_MANUAL              = 54,
  MSC_TYPE_GRANT_PROPERTY_TOKENS               = 55,
  MSC_TYPE_REVOKE_PROPERTY_TOKENS              = 56,
  MSC_TYPE_CHANGE_ISSUER_ADDRESS               = 70,
  MSC_TYPE_ENABLE_FREEZING                     = 71,
  MSC_TYPE_DISABLE_FREEZING                    = 72,
  MSC_TYPE_PEGGED_CURRENCY                     = 100,
  MSC_TYPE_REDEMPTION_PEGGED                   = 101,
  MSC_TYPE_SEND_PEGGED_CURRENCY                = 102,
  MSC_TYPE_CREATE_ORACLE_CONTRACT              = 103,
  MSC_TYPE_CHANGE_ORACLE_REF                   = 104,
  MSC_TYPE_SET_ORACLE                          = 105,
  MSC_TYPE_ORACLE_BACKUP                       = 106,
  MSC_TYPE_CLOSE_ORACLE                        = 107,
  MSC_TYPE_COMMIT_CHANNEL                      = 108,
  MSC_TYPE_WITHDRAWAL_FROM_CHANNEL             = 109,
  MSC_TYPE_INSTANT_TRADE                       = 110,
  MSC_TYPE_PNL_UPDATE                          = 111,
  MSC_TYPE_TRANSFER                            = 112,
  MSC_TYPE_CREATE_CHANNEL                      = 113,
  MSC_TYPE_CONTRACT_INSTANT                    = 114,
  MSC_TYPE_NEW_ID_REGISTRATION                 = 115,
  MSC_TYPE_UPDATE_ID_REGISTRATION              = 116,
  MSC_TYPE_DEX_PAYMENT                         = 117,
  MSC_TYPE_FREEZE_PROPERTY_TOKENS              = 185,
  MSC_TYPE_UNFREEZE_PROPERTY_TOKENS            = 186,
  TRADELAYER_MESSAGE_TYPE_DEACTIVATION           = 65533,
  TRADELAYER_MESSAGE_TYPE_ACTIVATION             = 65534,
  TRADELAYER_MESSAGE_TYPE_ALERT                  = 65535
};

#define ALL_PROPERTY_TYPE_INDIVISIBLE                 1
#define ALL_PROPERTY_TYPE_DIVISIBLE                   2
#define ALL_PROPERTY_TYPE_CONTRACT                    3
#define ALL_PROPERTY_TYPE_VESTING                     4
#define ALL_PROPERTY_TYPE_PEGGEDS                     5
#define ALL_PROPERTY_TYPE_ORACLE_CONTRACT             6
#define ALL_PROPERTY_TYPE_PERPETUAL_ORACLE            7
#define ALL_PROPERTY_TYPE_PERPETUAL_CONTRACTS         8


#define MSC_PROPERTY_TYPE_INDIVISIBLE             1
#define MSC_PROPERTY_TYPE_DIVISIBLE               2
#define MSC_PROPERTY_TYPE_INDIVISIBLE_REPLACING   65
#define MSC_PROPERTY_TYPE_DIVISIBLE_REPLACING     66
#define MSC_PROPERTY_TYPE_INDIVISIBLE_APPENDING   129
#define MSC_PROPERTY_TYPE_DIVISIBLE_APPENDING     130

#define PKT_RETURNED_OBJECT    (1000)

#define PKT_ERROR             ( -9000)
#define DEX_ERROR_SELLOFFER   (-10000)
#define DEX_ERROR_ACCEPT      (-20000)
#define DEX_ERROR_PAYMENT     (-30000)
// Smart Properties
#define PKT_ERROR_SP          (-40000)
#define PKT_ERROR_CROWD       (-45000)
// Send To Owners
#define PKT_ERROR_STO         (-50000)
#define PKT_ERROR_SEND        (-60000)
#define PKT_ERROR_TRADEOFFER  (-70000)
#define PKT_ERROR_METADEX     (-80000)
#define METADEX_ERROR         (-81000)
#define PKT_ERROR_TOKENS      (-82000)
#define PKT_ERROR_SEND_ALL    (-83000)

#define PKT_ERROR_TRADEOFFER  (-70000)

#define PKT_ERROR_KYC            (-90000)
#define PKT_ERROR_CONTRACTDEX    (-100000)
#define PKT_ERROR_ORACLE         (-110000)
#define PKT_ERROR_CHANNELS       (-120000)

#define TL_PROPERTY_MSC   1
#define TL_PROPERTY_TMSC  2

#define TL_PROPERTY_BTC             0
#define TL_PROPERTY_ALL             1
#define TL_PROPERTY_TALL            2
#define TL_PROPERTY_VESTING         3
#define TL_PROPERTY_ALL_ISSUANCE    6
#define TOTAL_AMOUNT_VESTING_TOKENS   1500000*COIN

#define BUY            1
#define SELL           2
#define ACTIONINVALID  3

/*24 horus to blocks*/
const int dayblocks = 144;

// channels definitions
#define TYPE_COMMIT                     "commit"
#define TYPE_WITHDRAWAL                 "withdrawal"
#define TYPE_INSTANT_TRADE              "instant_trade"
#define TYPE_TRANSFER                   "transfer"
#define TYPE_CONTRACT_INSTANT_TRADE     "contract_instat_trade"
#define TYPE_CREATE_CHANNEL             "create channel"
#define TYPE_NEW_ID_REGISTER            "new id register"


// limits for margin dynamic
const rational_t factor = rational_t(80,100);  // critical limit
const rational_t factor2 = rational_t(20,100); // normal limits


// forward declarations
double FormatContractShortMP(int64_t n);
long int FormatShortIntegerMP(int64_t n);
// uint64_t int64ToUint64(int64_t value);
// std::string FormatDivisibleZeroClean(int64_t n);

void Filling_Twap_Vec(std::map<uint32_t, std::vector<uint64_t>> &twap_ele, std::map<uint32_t, std::vector<uint64_t>> &twap_vec,
		      uint32_t property_traded, uint32_t property_desired, uint64_t effective_price);
void Filling_Twap_Vec(std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> &twap_ele,
		      std::map<uint32_t, std::map<uint32_t, std::vector<uint64_t>>> &twap_vec,
		      uint32_t property_traded, uint32_t property_desired, uint64_t effective_price);

/** Number formatting related functions. */
std::string FormatDivisibleMP(int64_t amount, bool fSign = false);
std::string FormatDivisibleShortMP(int64_t amount);
std::string FormatIndivisibleMP(int64_t amount);
std::string FormatByType(int64_t amount, uint16_t propertyType);
// Note: require initialized state to get divisibility.
std::string FormatMP(uint32_t propertyId, int64_t amount, bool fSign = false);
std::string FormatShortMP(uint32_t propertyId, int64_t amount);

/** Returns the Exodus address. */
const CTxDestination ExodusAddress();

/** Returns the Exodus crowdsale address. */
const CTxDestination ExodusCrowdsaleAddress(int nBlock = 0);

/** Returns the marker for class C transactions. */
const std::vector<unsigned char> GetTLMarker();

//! Used to indicate, whether to automatically commit created transactions
extern bool autoCommit;

//! Global lock for state objects
extern CCriticalSection cs_tally;

//! Available balances of wallet properties
extern std::map<uint32_t, int64_t> global_balance_money;
//! Reserved balances of wallet propertiess
extern std::map<uint32_t, int64_t> global_balance_reserved;
//! Vector containing a list of properties relative to the wallet
extern std::set<uint32_t> global_wallet_property_list;

int64_t GetTokenBalance(const std::string& address, uint32_t propertyId, TallyType ttype);
int64_t GetAvailableTokenBalance(const std::string& address, uint32_t propertyId);
int64_t GetReservedTokenBalance(const std::string& address, uint32_t propertyId);
int64_t GetFrozenTokenBalance(const std::string& address, uint32_t propertyId);

/** Global handler to initialize Trade Layer. */
int mastercore_init();

/** Global handler to shut down Trade Layer. */
int mastercore_shutdown();

/** Block and transaction handlers. */
void mastercore_handler_disc_begin(const int nHeight);
int mastercore_handler_block_begin(int nBlockNow, CBlockIndex const * pBlockIndex);
int mastercore_handler_block_end(int nBlockNow, CBlockIndex const * pBlockIndex, unsigned int);
bool mastercore_handler_tx(const CTransaction& tx, int nBlock, unsigned int idx, const CBlockIndex* pBlockIndex, const std::shared_ptr<std::map<COutPoint, Coin>> removedCoins);
void printing_edges_database(std::map<std::string, std::string> &path_ele);
void loopforEntryPrice(std::vector<std::map<std::string, std::string>> path_ele, std::vector<std::map<std::string, std::string>> path_eleh, std::string addrs_upnl, std::string status_match, double &entry_price, int &idx_price, uint64_t entry_price_num, unsigned int limSup, double exit_priceh, uint64_t &amount, std::string &status);
bool callingPerpetualSettlement(double globalPNLALL_DUSD, int64_t globalVolumeALL_DUSD, int64_t volumeToCompare);
double PNL_function(double entry_price, double exit_price, int64_t amount_trd, std::string netted_status);
void fillingMatrix(MatrixTLS &M_file, MatrixTLS &ndatabase, std::vector<std::map<std::string, std::string>> &path_ele);
inline int64_t clamp_function(int64_t diff, int64_t nclamp);
bool TxValidNodeReward(std::string ConsensusHash, std::string Tx);

/** Scans for marker and if one is found, add transaction to marker cache. */
void TryToAddToMarkerCache(const CTransactionRef& tx);
/** Removes transaction from marker cache. */
void RemoveFromMarkerCache(const uint256& txHash);
/** Checks, if transaction is in marker cache. */
bool IsInMarkerCache(const uint256& txHash);

/** Global handler to total wallet balances. */
void CheckWalletUpdate(bool forceUpdate = false);

/** Used to notify that the number of tokens for a property has changed. */
void NotifyTotalTokensChanged(uint32_t propertyId, int block);


namespace mastercore
{
//! In-memory collection of all amounts for all addresses for all properties
extern std::unordered_map<std::string, CMPTally> mp_tally_map;

// TODO: move, rename
extern CCoinsView viewDummy;
extern CCoinsViewCache view;
//! Guards coins view cache
extern CCriticalSection cs_tx_cache;

/** Returns the encoding class, used to embed a payload. */
int GetEncodingClass(const CTransaction& tx, int nBlock);

/** Determines, whether it is valid to use a Class C transaction for a given payload size. */
bool UseEncodingClassC(size_t nDataSize);

bool isTestEcosystemProperty(uint32_t propertyId);
bool isMainEcosystemProperty(uint32_t propertyId);
uint32_t GetNextPropertyId(bool maineco); // maybe move into sp

CMPTally* getTally(const std::string& address);
bool update_tally_map(const std::string& who, uint32_t propertyId, int64_t amount, TallyType ttype);
int64_t getTotalTokens(uint32_t propertyId, int64_t* n_owners_total = nullptr);

std::string strMPProperty(uint32_t propertyId);
std::string strTransactionType(uint16_t txType);
std::string getTokenLabel(uint32_t propertyId);

/**
    NOTE: The following functions are only permitted for properties
          managed by a central issuer that have enabled freezing.
 **/
/** Adds an address and property to the frozenMap **/
void freezeAddress(const std::string& address, uint32_t propertyId);
/** Removes an address and property from the frozenMap **/
void unfreezeAddress(const std::string& address, uint32_t propertyId);
/** Checks whether an address and property are frozen **/
bool isAddressFrozen(const std::string& address, uint32_t propertyId);
/** Adds a property to the freezingEnabledMap **/
void enableFreezing(uint32_t propertyId, int liveBlock);
/** Removes a property from the freezingEnabledMap **/
void disableFreezing(uint32_t propertyId);
/** Checks whether a property has freezing enabled **/
bool isFreezingEnabled(uint32_t propertyId, int block);
/** Clears the freeze state in the event of a reorg **/
void ClearFreezeState();
/** Prints the freeze state **/
void PrintFreezeState();

rational_t notionalChange(uint32_t contractId);

bool marginMain(int Block);

void update_sum_upnls(); // update the sum of all upnls for all addresses.

int64_t sum_check_upnl(std::string address); //  sum of all upnls for a given address.

int64_t pos_margin(uint32_t contractId, std::string address, uint16_t prop_type, uint32_t margin_requirement); // return mainteinance margin for a given contrand and address

bool makeWithdrawals(int Block); // make the withdrawals for multisig channels

bool closeChannels(int Block);

// x_Trade function for contracts on instant trade
bool Instant_x_Trade(const uint256& txid, uint8_t tradingAction, std::string& channelAddr, std::string& firstAddr, std::string& secondAddr, uint32_t property, int64_t amount_forsale, uint64_t price, int block, int tx_idx);

//Fees for contract instant trades
bool ContInst_Fees(const std::string& firstAddr,const std::string& secondAddr,const std::string& channelAddr, int64_t amountToReserve,uint16_t type, uint32_t colateral);

}

#endif // BITCOIN_TRADELAYE_TRADELAYER_H
