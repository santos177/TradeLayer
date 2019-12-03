// Master Protocol transaction code

#include <omnicore/tx.h>

#include <omnicore/activation.h>
#include <omnicore/dbfees.h>
#include <omnicore/dbspinfo.h>
#include <omnicore/dbstolist.h>
#include <omnicore/dbtradelist.h>
#include <omnicore/dbtxlist.h>
#include <omnicore/dex.h>
#include <omnicore/log.h>
#include <omnicore/mdex.h>
#include <omnicore/notifications.h>
#include <omnicore/parsing.h>
#include <omnicore/rules.h>
#include <omnicore/sp.h>
#include <omnicore/sto.h>
#include <omnicore/uint256_extensions.h>
#include <omnicore/utilsbitcoin.h>
#include <omnicore/version.h>


#include <amount.h>
#include <base58.h>
#include <key_io.h>
#include <validation.h>
#include <sync.h>
#include <util/time.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <utility>
#include <vector>

using boost::algorithm::token_compress_on;

using namespace mastercore;

typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;
typedef boost::multiprecision::cpp_dec_float_100 dec_float;
typedef boost::multiprecision::checked_int128_t int128_t;
extern std::map<std::string,uint32_t> peggedIssuers;
extern std::map<uint32_t,oracledata> oraclePrices;
extern std::map<std::string,vector<withdrawalAccepted>> withdrawal_Map;
extern std::map<std::string,channel> channels_Map;
extern int64_t factorE;
extern int64_t priceIndex;
extern int64_t allPrice;
extern double denMargin;
extern uint64_t marketP[NPTYPES];
extern volatile int id_contract;
extern volatile int64_t factorALLtoLTC;
extern volatile int64_t globalVolumeALL_LTC;
extern volatile int64_t LTCPriceOffer;
extern std::vector<std::string> vestingAddresses;
extern mutex mReward;

// using mastercore::StrToInt64;

/** Returns a label for the given transaction type. */
std::string mastercore::strTransactionType(uint16_t txType)
{
    switch (txType) {
        case MSC_TYPE_SIMPLE_SEND: return "Simple Send";
        case MSC_TYPE_RESTRICTED_SEND: return "Restricted Send";
        case MSC_TYPE_SEND_TO_OWNERS: return "Send To Owners";
        case MSC_TYPE_SEND_ALL: return "Send All";
        case MSC_TYPE_SAVINGS_MARK: return "Savings";
        case MSC_TYPE_SAVINGS_COMPROMISED: return "Savings COMPROMISED";
        case MSC_TYPE_RATELIMITED_MARK: return "Rate-Limiting";
        case MSC_TYPE_AUTOMATIC_DISPENSARY: return "Automatic Dispensary";
        case MSC_TYPE_TRADE_OFFER: return "DEx Sell Offer";
        case MSC_TYPE_METADEX_TRADE: return "MetaDEx trade";
        case MSC_TYPE_METADEX_CANCEL_PRICE: return "MetaDEx cancel-price";
        case MSC_TYPE_METADEX_CANCEL_PAIR: return "MetaDEx cancel-pair";
        case MSC_TYPE_METADEX_CANCEL_ECOSYSTEM: return "MetaDEx cancel-ecosystem";
        case MSC_TYPE_ACCEPT_OFFER_BTC: return "DEx Accept Offer";
        case MSC_TYPE_CREATE_PROPERTY_FIXED: return "Create Property - Fixed";
        case MSC_TYPE_CREATE_PROPERTY_VARIABLE: return "Create Property - Variable";
        case MSC_TYPE_PROMOTE_PROPERTY: return "Promote Property";
        case MSC_TYPE_CLOSE_CROWDSALE: return "Close Crowdsale";
        case MSC_TYPE_CREATE_PROPERTY_MANUAL: return "Create Property - Manual";
        case MSC_TYPE_GRANT_PROPERTY_TOKENS: return "Grant Property Tokens";
        case MSC_TYPE_REVOKE_PROPERTY_TOKENS: return "Revoke Property Tokens";
        case MSC_TYPE_CHANGE_ISSUER_ADDRESS: return "Change Issuer Address";
        case MSC_TYPE_ENABLE_FREEZING: return "Enable Freezing";
        case MSC_TYPE_DISABLE_FREEZING: return "Disable Freezing";
        case MSC_TYPE_FREEZE_PROPERTY_TOKENS: return "Freeze Property Tokens";
        case MSC_TYPE_UNFREEZE_PROPERTY_TOKENS: return "Unfreeze Property Tokens";
        case MSC_TYPE_NOTIFICATION: return "Notification";
        case OMNICORE_MESSAGE_TYPE_ALERT: return "ALERT";
        case OMNICORE_MESSAGE_TYPE_DEACTIVATION: return "Feature Deactivation";
        case OMNICORE_MESSAGE_TYPE_ACTIVATION: return "Feature Activation";
        case MSC_TYPE_DEX_PAYMENT: return "DEx payment";
        case MSC_TYPE_CREATE_CONTRACT: return "Create Contract";

        default: return "* unknown type *";
    }
}

/** Helper to convert class number to string. */
static std::string intToClass(int encodingClass)
{
    switch (encodingClass) {
        case OMNI_CLASS_A:
            return "A";
        case OMNI_CLASS_B:
            return "B";
        case OMNI_CLASS_C:
            return "C";
    }

    return "-";
}

/** Checks whether a pointer to the payload is past it's last position. */
bool CMPTransaction::isOverrun(const char* p)
{
    ptrdiff_t pos = (char*) p - (char*) &pkt;
    return (pos > pkt_size);
}

// -------------------- PACKET PARSING -----------------------

/** Parses the packet or payload. */
bool CMPTransaction::interpret_Transaction()
{
    if (!interpret_TransactionType()) {
        PrintToLog("Failed to interpret type and version\n");
        return false;
    }

    switch (type) {
        case MSC_TYPE_SIMPLE_SEND:
            return interpret_SimpleSend();

        case MSC_TYPE_SEND_TO_OWNERS:
            return interpret_SendToOwners();

        case MSC_TYPE_SEND_ALL:
            return interpret_SendAll();

        case MSC_TYPE_TRADE_OFFER:
            return interpret_TradeOffer();

        case MSC_TYPE_ACCEPT_OFFER_BTC:
            return interpret_AcceptOfferBTC();

        case MSC_TYPE_METADEX_TRADE:
            return interpret_MetaDExTrade();

        case MSC_TYPE_METADEX_CANCEL_PRICE:
            return interpret_MetaDExCancelPrice();

        case MSC_TYPE_METADEX_CANCEL_PAIR:
            return interpret_MetaDExCancelPair();

        case MSC_TYPE_METADEX_CANCEL_ECOSYSTEM:
            return interpret_MetaDExCancelEcosystem();

        case MSC_TYPE_CREATE_PROPERTY_FIXED:
            return interpret_CreatePropertyFixed();

        case MSC_TYPE_CREATE_PROPERTY_VARIABLE:
            return interpret_CreatePropertyVariable();

        case MSC_TYPE_CLOSE_CROWDSALE:
            return interpret_CloseCrowdsale();

        case MSC_TYPE_CREATE_PROPERTY_MANUAL:
            return interpret_CreatePropertyManaged();

        case MSC_TYPE_GRANT_PROPERTY_TOKENS:
            return interpret_GrantTokens();

        case MSC_TYPE_REVOKE_PROPERTY_TOKENS:
            return interpret_RevokeTokens();

        case MSC_TYPE_CHANGE_ISSUER_ADDRESS:
            return interpret_ChangeIssuer();

        case MSC_TYPE_ENABLE_FREEZING:
            return interpret_EnableFreezing();

        case MSC_TYPE_DISABLE_FREEZING:
            return interpret_DisableFreezing();

        case MSC_TYPE_FREEZE_PROPERTY_TOKENS:
            return interpret_FreezeTokens();

        case MSC_TYPE_UNFREEZE_PROPERTY_TOKENS:
            return interpret_UnfreezeTokens();

        case OMNICORE_MESSAGE_TYPE_DEACTIVATION:
            return interpret_Deactivation();

        case OMNICORE_MESSAGE_TYPE_ACTIVATION:
            return interpret_Activation();

        case OMNICORE_MESSAGE_TYPE_ALERT:
            return interpret_Alert();

        case MSC_TYPE_DEX_PAYMENT:
            return interpret_DEx_Payment();

        case MSC_TYPE_SEND_VESTING:
            return interpret_SendVestingTokens();

        case MSC_TYPE_CREATE_CONTRACT:
            return interpret_CreateContractDex();

        case MSC_TYPE_CREATE_ORACLE_CONTRACT:
            return interpret_CreateOracleContract();

        case MSC_TYPE_CONTRACTDEX_TRADE:
            return interpret_ContractDexTrade();

        case MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM:
            return interpret_ContractDexCancelEcosystem();

        // case MSC_TYPE_PEGGED_CURRENCY:
        //     return interpret_CreatePeggedCurrency();
        //
        // case MSC_TYPE_SEND_PEGGED_CURRENCY:
        //     return interpret_SendPeggedCurrency();
        //
        // case MSC_TYPE_REDEMPTION_PEGGED:
        //     return interpret_RedemptionPegged();
        //
        case MSC_TYPE_CONTRACTDEX_CLOSE_POSITION:
            return interpret_ContractDexClosePosition();

        case MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK:
            return interpret_ContractDex_Cancel_Orders_By_Block();

        // case MSC_TYPE_DEX_BUY_OFFER:
        //     return interpret_DExBuy();
        //
        case MSC_TYPE_CHANGE_ORACLE_REF:
            return interpret_Change_OracleRef();

        case MSC_TYPE_SET_ORACLE:
            return interpret_Set_Oracle();

        case MSC_TYPE_ORACLE_BACKUP:
            return interpret_OracleBackup();

        case MSC_TYPE_CLOSE_ORACLE:
            return interpret_CloseOracle();

        case MSC_TYPE_COMMIT_CHANNEL:
            return interpret_CommitChannel();

        case MSC_TYPE_WITHDRAWAL_FROM_CHANNEL:
            return interpret_Withdrawal_FromChannel();

        case MSC_TYPE_INSTANT_TRADE:
            return interpret_Instant_Trade();

        case MSC_TYPE_TRANSFER:
            return interpret_Transfer();

        case MSC_TYPE_CREATE_CHANNEL:
            return interpret_Create_Channel();

        case MSC_TYPE_CONTRACT_INSTANT:
            return interpret_Contract_Instant();

        case MSC_TYPE_NEW_ID_REGISTRATION:
            return interpret_New_Id_Registration();

        case MSC_TYPE_UPDATE_ID_REGISTRATION:
            return interpret_Update_Id_Registration();

    }

    return false;
}

/** Version and type */
bool CMPTransaction::interpret_TransactionType()
{
    if (pkt_size < 4) {
        return false;
    }
    uint16_t txVersion = 0;
    uint16_t txType = 0;
    memcpy(&txVersion, &pkt[0], 2);
    SwapByteOrder16(txVersion);
    memcpy(&txType, &pkt[2], 2);
    SwapByteOrder16(txType);
    version = txVersion;
    type = txType;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t------------------------------\n");
        PrintToLog("\t         version: %d, class %s\n", txVersion, intToClass(encodingClass));
        PrintToLog("\t            type: %d (%s)\n", txType, strTransactionType(txType));
    }

    return true;
}

/** Tx 1 */
bool CMPTransaction::interpret_SimpleSend()
{
    if (pkt_size < 16) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&nValue, &pkt[8], 8);
    SwapByteOrder64(nValue);
    nNewValue = nValue;

    // Special case: if can't find the receiver -- assume send to self!
    if (receiver.empty()) {
        receiver = sender;
    }

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
    }

    return true;
}

/** Tx 3 */
bool CMPTransaction::interpret_SendToOwners()
{
    int expectedSize = (version == MP_TX_PKT_V0) ? 16 : 20;
    if (pkt_size < expectedSize) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&nValue, &pkt[8], 8);
    SwapByteOrder64(nValue);
    nNewValue = nValue;
    if (version > MP_TX_PKT_V0) {
        memcpy(&distribution_property, &pkt[16], 4);
        SwapByteOrder32(distribution_property);
    }

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t             property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t                value: %s\n", FormatMP(property, nValue));
        if (version > MP_TX_PKT_V1) {
            PrintToLog("\t distributionproperty: %d (%s)\n", distribution_property, strMPProperty(distribution_property));
        }
    }

    return true;
}

/** Tx 4 */
bool CMPTransaction::interpret_SendAll()
{
    if (pkt_size < 5) {
        return false;
    }
    memcpy(&ecosystem, &pkt[4], 1);

    property = ecosystem; // provide a hint for the UI, TODO: better handling!

    // Special case: if can't find the receiver -- assume send to self!
    if (receiver.empty()) {
        receiver = sender;
    }

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t       ecosystem: %d\n", (int)ecosystem);
    }

    return true;
}

/** Tx 5 */
bool CMPTransaction::interpret_SendVestingTokens()
{
  PrintToLog("%s(): inside interpret!\n",__func__);

  // if (pkt_size < 5) { /** TODO: check minimum size here */
  //     return false;
  // }

  memcpy(&property, &pkt[4], 4);
  SwapByteOrder32(property);
  memcpy(&nValue, &pkt[8], 8);
  SwapByteOrder64(nValue);
  nNewValue = nValue;

  // if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
    PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
    PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
  // }

  return true;
}

/** Tx 20 */
bool CMPTransaction::interpret_TradeOffer()
{
    int expectedSize = (version == MP_TX_PKT_V0) ? 33 : 34;
    if (pkt_size < expectedSize) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&nValue, &pkt[8], 8);
    SwapByteOrder64(nValue);
    nNewValue = nValue;
    memcpy(&amount_desired, &pkt[16], 8);
    memcpy(&blocktimelimit, &pkt[24], 1);
    memcpy(&min_fee, &pkt[25], 8);
    if (version > MP_TX_PKT_V0) {
        memcpy(&subaction, &pkt[33], 1);
    }
    SwapByteOrder64(amount_desired);
    SwapByteOrder64(min_fee);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
        PrintToLog("\t  amount desired: %s\n", FormatDivisibleMP(amount_desired));
        PrintToLog("\tblock time limit: %d\n", blocktimelimit);
        PrintToLog("\t         min fee: %s\n", FormatDivisibleMP(min_fee));
        if (version > MP_TX_PKT_V0) {
            PrintToLog("\t      sub-action: %d\n", subaction);
        }
    }

    return true;
}

/** Tx 22 */
bool CMPTransaction::interpret_AcceptOfferBTC()
{
    if (pkt_size < 16) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&nValue, &pkt[8], 8);
    SwapByteOrder64(nValue);
    nNewValue = nValue;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
    }

    return true;
}

/** Tx 25 */
bool CMPTransaction::interpret_MetaDExTrade()
{
    if (pkt_size < 28) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&nValue, &pkt[8], 8);
    SwapByteOrder64(nValue);
    nNewValue = nValue;
    memcpy(&desired_property, &pkt[16], 4);
    SwapByteOrder32(desired_property);
    memcpy(&desired_value, &pkt[20], 8);
    SwapByteOrder64(desired_value);

    action = CMPTransaction::ADD; // depreciated

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
        PrintToLog("\tdesired property: %d (%s)\n", desired_property, strMPProperty(desired_property));
        PrintToLog("\t   desired value: %s\n", FormatMP(desired_property, desired_value));
    }

    return true;
}

/** Tx 26 */
bool CMPTransaction::interpret_MetaDExCancelPrice()
{
    if (pkt_size < 28) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&nValue, &pkt[8], 8);
    SwapByteOrder64(nValue);
    nNewValue = nValue;
    memcpy(&desired_property, &pkt[16], 4);
    SwapByteOrder32(desired_property);
    memcpy(&desired_value, &pkt[20], 8);
    SwapByteOrder64(desired_value);

    action = CMPTransaction::CANCEL_AT_PRICE; // depreciated

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
        PrintToLog("\tdesired property: %d (%s)\n", desired_property, strMPProperty(desired_property));
        PrintToLog("\t   desired value: %s\n", FormatMP(desired_property, desired_value));
    }

    return true;
}

/** Tx 27 */
bool CMPTransaction::interpret_MetaDExCancelPair()
{
    if (pkt_size < 12) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&desired_property, &pkt[8], 4);
    SwapByteOrder32(desired_property);

    nValue = 0; // depreciated
    nNewValue = nValue; // depreciated
    desired_value = 0; // depreciated
    action = CMPTransaction::CANCEL_ALL_FOR_PAIR; // depreciated

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\tdesired property: %d (%s)\n", desired_property, strMPProperty(desired_property));
    }

    return true;
}

/** Tx 28 */
bool CMPTransaction::interpret_MetaDExCancelEcosystem()
{
    if (pkt_size < 5) {
        return false;
    }
    memcpy(&ecosystem, &pkt[4], 1);

    property = ecosystem; // depreciated
    desired_property = ecosystem; // depreciated
    nValue = 0; // depreciated
    nNewValue = nValue; // depreciated
    desired_value = 0; // depreciated
    action = CMPTransaction::CANCEL_EVERYTHING; // depreciated

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t       ecosystem: %d\n", (int)ecosystem);
    }

    return true;
}

/** Tx 50 */
bool CMPTransaction::interpret_CreatePropertyFixed()
{
    if (pkt_size < 25) {
        return false;
    }
    const char* p = 11 + (char*) &pkt;
    std::vector<std::string> spstr;
    memcpy(&ecosystem, &pkt[4], 1);
    memcpy(&prop_type, &pkt[5], 2);
    SwapByteOrder16(prop_type);
    memcpy(&prev_prop_id, &pkt[7], 4);
    SwapByteOrder32(prev_prop_id);
    for (int i = 0; i < 5; i++) {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }
    int i = 0;
    memcpy(category, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(category)-1)); i++;
    memcpy(subcategory, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(subcategory)-1)); i++;
    memcpy(name, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(name)-1)); i++;
    memcpy(url, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(url)-1)); i++;
    memcpy(data, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(data)-1)); i++;
    memcpy(&nValue, p, 8);
    SwapByteOrder64(nValue);
    p += 8;
    nNewValue = nValue;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t       ecosystem: %d\n", ecosystem);
        PrintToLog("\t   property type: %d (%s)\n", prop_type, strPropertyType(prop_type));
        PrintToLog("\tprev property id: %d\n", prev_prop_id);
        PrintToLog("\t        category: %s\n", category);
        PrintToLog("\t     subcategory: %s\n", subcategory);
        PrintToLog("\t            name: %s\n", name);
        PrintToLog("\t             url: %s\n", url);
        PrintToLog("\t            data: %s\n", data);
        PrintToLog("\t           value: %s\n", FormatByType(nValue, prop_type));
    }

    if (isOverrun(p)) {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    return true;
}

/** Tx 51 */
bool CMPTransaction::interpret_CreatePropertyVariable()
{
    if (pkt_size < 39) {
        return false;
    }
    const char* p = 11 + (char*) &pkt;
    std::vector<std::string> spstr;
    memcpy(&ecosystem, &pkt[4], 1);
    memcpy(&prop_type, &pkt[5], 2);
    SwapByteOrder16(prop_type);
    memcpy(&prev_prop_id, &pkt[7], 4);
    SwapByteOrder32(prev_prop_id);
    for (int i = 0; i < 5; i++) {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }
    int i = 0;
    memcpy(category, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(category)-1)); i++;
    memcpy(subcategory, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(subcategory)-1)); i++;
    memcpy(name, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(name)-1)); i++;
    memcpy(url, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(url)-1)); i++;
    memcpy(data, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(data)-1)); i++;
    memcpy(&property, p, 4);
    SwapByteOrder32(property);
    p += 4;
    memcpy(&nValue, p, 8);
    SwapByteOrder64(nValue);
    p += 8;
    nNewValue = nValue;
    memcpy(&deadline, p, 8);
    SwapByteOrder64(deadline);
    p += 8;
    memcpy(&early_bird, p++, 1);
    memcpy(&percentage, p++, 1);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t       ecosystem: %d\n", ecosystem);
        PrintToLog("\t   property type: %d (%s)\n", prop_type, strPropertyType(prop_type));
        PrintToLog("\tprev property id: %d\n", prev_prop_id);
        PrintToLog("\t        category: %s\n", category);
        PrintToLog("\t     subcategory: %s\n", subcategory);
        PrintToLog("\t            name: %s\n", name);
        PrintToLog("\t             url: %s\n", url);
        PrintToLog("\t            data: %s\n", data);
        PrintToLog("\tproperty desired: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t tokens per unit: %s\n", FormatByType(nValue, prop_type));
        PrintToLog("\t        deadline: %d\n", deadline);
        PrintToLog("\tearly bird bonus: %d\n", early_bird);
        PrintToLog("\t    issuer bonus: %d\n", percentage);
    }

    if (isOverrun(p)) {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    return true;
}

/** Tx 53 */
bool CMPTransaction::interpret_CloseCrowdsale()
{
    if (pkt_size < 8) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
    }

    return true;
}

/** Tx 54 */
bool CMPTransaction::interpret_CreatePropertyManaged()
{
    if (pkt_size < 17) {
        return false;
    }
    const char* p = 11 + (char*) &pkt;
    std::vector<std::string> spstr;
    memcpy(&ecosystem, &pkt[4], 1);
    memcpy(&prop_type, &pkt[5], 2);
    SwapByteOrder16(prop_type);
    memcpy(&prev_prop_id, &pkt[7], 4);
    SwapByteOrder32(prev_prop_id);
    for (int i = 0; i < 5; i++) {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }
    int i = 0;
    memcpy(category, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(category)-1)); i++;
    memcpy(subcategory, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(subcategory)-1)); i++;
    memcpy(name, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(name)-1)); i++;
    memcpy(url, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(url)-1)); i++;
    memcpy(data, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(data)-1)); i++;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t       ecosystem: %d\n", ecosystem);
        PrintToLog("\t   property type: %d (%s)\n", prop_type, strPropertyType(prop_type));
        PrintToLog("\tprev property id: %d\n", prev_prop_id);
        PrintToLog("\t        category: %s\n", category);
        PrintToLog("\t     subcategory: %s\n", subcategory);
        PrintToLog("\t            name: %s\n", name);
        PrintToLog("\t             url: %s\n", url);
        PrintToLog("\t            data: %s\n", data);
    }

    if (isOverrun(p)) {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    return true;
}

/** Tx 55 */
bool CMPTransaction::interpret_GrantTokens()
{
    if (pkt_size < 16) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&nValue, &pkt[8], 8);
    SwapByteOrder64(nValue);
    nNewValue = nValue;

    // Special case: if can't find the receiver -- assume grant to self!
    if (receiver.empty()) {
        receiver = sender;
    }

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
    }

    return true;
}

/** Tx 56 */
bool CMPTransaction::interpret_RevokeTokens()
{
    if (pkt_size < 16) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&nValue, &pkt[8], 8);
    SwapByteOrder64(nValue);
    nNewValue = nValue;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t           value: %s\n", FormatMP(property, nValue));
    }

    return true;
}

/** Tx 70 */
bool CMPTransaction::interpret_ChangeIssuer()
{
    if (pkt_size < 8) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
    }

    return true;
}

/** Tx 71 */
bool CMPTransaction::interpret_EnableFreezing()
{
    if (pkt_size < 8) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
    }

    return true;
}

/** Tx 72 */
bool CMPTransaction::interpret_DisableFreezing()
{
    if (pkt_size < 8) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
    }

    return true;
}

/** Tx 185 */
bool CMPTransaction::interpret_FreezeTokens()
{
    if (pkt_size < 37) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&nValue, &pkt[8], 8);
    SwapByteOrder64(nValue);
    nNewValue = nValue;

    /**
        Note, TX185 is a virtual reference transaction type.
              With virtual reference transactions a hash160 in the payload sets the receiver.
              Reference outputs are ignored.
    **/
    unsigned char address_version;
    uint160 address_hash160;
    memcpy(&address_version, &pkt[16], 1);
    memcpy(&address_hash160, &pkt[17], 20);
    receiver = HashToAddress(address_version, address_hash160);
    if (receiver.empty()) {
        return false;
    }
    CTxDestination recAddress = DecodeDestination(receiver);
    if (!IsValidDestination(recAddress)) {
        return false;
    }

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t  value (unused): %s\n", FormatMP(property, nValue));
        PrintToLog("\t         address: %s\n", receiver);
    }

    return true;
}

/** Tx 186 */
bool CMPTransaction::interpret_UnfreezeTokens()
{
    if (pkt_size < 37) {
        return false;
    }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(property);
    memcpy(&nValue, &pkt[8], 8);
    SwapByteOrder64(nValue);
    nNewValue = nValue;

    /**
        Note, TX186 virtual reference transaction type.
              With virtual reference transactions a hash160 in the payload sets the receiver.
              Reference outputs are ignored.
    **/
    unsigned char address_version;
    uint160 address_hash160;
    memcpy(&address_version, &pkt[16], 1);
    memcpy(&address_hash160, &pkt[17], 20);
    receiver = HashToAddress(address_version, address_hash160);
    if (receiver.empty()) {
        return false;
    }
    CTxDestination recAddress = DecodeDestination(receiver);
    if (!IsValidDestination(recAddress)) {
        return false;
    }

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t        property: %d (%s)\n", property, strMPProperty(property));
        PrintToLog("\t  value (unused): %s\n", FormatMP(property, nValue));
        PrintToLog("\t         address: %s\n", receiver);
    }

    return true;
}

/** Tx 65533 */
bool CMPTransaction::interpret_Deactivation()
{
    if (pkt_size < 6) {
        return false;
    }
    memcpy(&feature_id, &pkt[4], 2);
    SwapByteOrder16(feature_id);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t      feature id: %d\n", feature_id);
    }

    return true;
}

/** Tx 65534 */
bool CMPTransaction::interpret_Activation()
{
    if (pkt_size < 14) {
        return false;
    }
    memcpy(&feature_id, &pkt[4], 2);
    SwapByteOrder16(feature_id);
    memcpy(&activation_block, &pkt[6], 4);
    SwapByteOrder32(activation_block);
    memcpy(&min_client_version, &pkt[10], 4);
    SwapByteOrder32(min_client_version);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t      feature id: %d\n", feature_id);
        PrintToLog("\tactivation block: %d\n", activation_block);
        PrintToLog("\t minimum version: %d\n", min_client_version);
    }

    return true;
}

/** Tx 65535 */
bool CMPTransaction::interpret_Alert()
{
    if (pkt_size < 11) {
        return false;
    }

    memcpy(&alert_type, &pkt[4], 2);
    SwapByteOrder16(alert_type);
    memcpy(&alert_expiry, &pkt[6], 4);
    SwapByteOrder32(alert_expiry);

    const char* p = 10 + (char*) &pkt;
    std::string spstr(p);
    memcpy(alert_text, spstr.c_str(), std::min(spstr.length(), sizeof(alert_text)-1));

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly) {
        PrintToLog("\t      alert type: %d\n", alert_type);
        PrintToLog("\t    expiry value: %d\n", alert_expiry);
        PrintToLog("\t   alert message: %s\n", alert_text);
    }

    if (isOverrun(p)) {
        PrintToLog("%s(): rejected: malformed string value(s)\n", __func__);
        return false;
    }

    return true;
}

/** Tx  117*/
bool CMPTransaction::interpret_DEx_Payment()
{

  // memcpy(&property, &pkt[4], 4);
  // SwapByteOrder32(property);
  // memcpy(&nValue, &pkt[8], 8);
  // SwapByteOrder64(nValue);

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("%s(): inside the function\n",__func__);
      PrintToLog("\t sender: %s\n", sender);
      PrintToLog("\t receiver: %s\n", receiver);
  }

  return true;
}

/** Tx  40*/
bool CMPTransaction::interpret_CreateContractDex()
{

  if (pkt_size < 17) {
      return false;
  }

  memcpy(&ecosystem, &pkt[4], 1);
  memcpy(&blocks_until_expiration, &pkt[5], 4);
  SwapByteOrder32(blocks_until_expiration);
  memcpy(&notional_size, &pkt[9], 4);
  SwapByteOrder32(notional_size);
  memcpy(&collateral_currency, &pkt[13], 4);
  SwapByteOrder32(collateral_currency);
  memcpy(&margin_requirement, &pkt[17], 4);
  SwapByteOrder32(margin_requirement);

  const char* p = 4 + (char*) &pkt[17];
  std::vector<std::string> spstr;

  for (int i = 0; i < 1; i++) {
      spstr.push_back(std::string(p));
      p += spstr.back().size() + 1;
  }

  int i = 0;
  memcpy(name, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(name)-1)); i++;

  prop_type = ALL_PROPERTY_TYPE_CONTRACT;

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      // PrintToLog("\t denomination: %d\n", denomination);
      PrintToLog("\t blocks until expiration : %d\n", blocks_until_expiration);
      PrintToLog("\t notional size : %d\n", notional_size);
      PrintToLog("\t collateral currency: %d\n", collateral_currency);
      PrintToLog("\t margin requirement: %d\n", margin_requirement);
      PrintToLog("\t ecosystem: %d\n", ecosystem);
      PrintToLog("\t name: %s\n", name);
      PrintToLog("\t prop_type: %d\n", prop_type);
  }

  return true;
}

/** Tx  103*/
bool CMPTransaction::interpret_CreateOracleContract()
{

    if (pkt_size < 17) {
        return false;
    }

    memcpy(&ecosystem, &pkt[4], 1);
    memcpy(&blocks_until_expiration, &pkt[5], 4);
    SwapByteOrder32(blocks_until_expiration);
    memcpy(&notional_size, &pkt[9], 4);
    SwapByteOrder32(notional_size);
    memcpy(&collateral_currency, &pkt[13], 4);
    SwapByteOrder32(collateral_currency);
    memcpy(&margin_requirement, &pkt[17], 4);
    SwapByteOrder32(margin_requirement);

    const char* p = 4 + (char*) &pkt[17];
    std::vector<std::string> spstr;

    for (int i = 0; i < 1; i++) {
        spstr.push_back(std::string(p));
        p += spstr.back().size() + 1;
    }

    int i = 0;
    memcpy(name, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(name)-1)); i++;


    prop_type = ALL_PROPERTY_TYPE_ORACLE_CONTRACT;

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t denomination: %d\n", denomination);
        PrintToLog("\t blocks until expiration : %d\n", blocks_until_expiration);
        PrintToLog("\t notional size : %d\n", notional_size);
        PrintToLog("\t collateral currency: %d\n", collateral_currency);
        PrintToLog("\t margin requirement: %d\n", margin_requirement);
        PrintToLog("\t ecosystem: %d\n", ecosystem);
        PrintToLog("\t name: %s\n", name);
        PrintToLog("\t oracleAddress: %s\n", sender);
        PrintToLog("\t backupAddress: %s\n", receiver);
        PrintToLog("\t prop_type: %d\n", prop_type);
  }

  return true;
}

/**Tx 29 */
bool CMPTransaction::interpret_ContractDexTrade()
{

  if (pkt_size < 17) {
      return false;
  }

  memcpy(&amount, &pkt[4], 8);
  SwapByteOrder64(amount);
  memcpy(&effective_price, &pkt[12], 8);
  SwapByteOrder64(effective_price);
  memcpy(&leverage, &pkt[20], 8);
  SwapByteOrder64(leverage);
  memcpy(&trading_action,&pkt[28], 1);

  const char* p = 1 + (char*) &pkt[28]; // next char
  std::vector<std::string> spstr;

  for (int i = 0; i < 1; i++) {
      spstr.push_back(std::string(p));
      p += spstr.back().size() + 1;
  }

  memcpy(name, spstr[0].c_str(), std::min(spstr[0].length(), sizeof(name)-1));


  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t leverage: %d\n", leverage);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t contractName: %s\n", name_traded);
      PrintToLog("\t amount of contracts : %d\n", amount);
      PrintToLog("\t effective price : %d\n", effective_price);
      PrintToLog("\t trading action : %d\n", trading_action);
  }

  return true;
}

/** Tx 32 */
bool CMPTransaction::interpret_ContractDexCancelEcosystem()
{

  // if (pkt_size < 17) {
  //     return false;
  // }

  memcpy(&ecosystem, &pkt[4], 1);

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
     PrintToLog("\t version: %d\n", version);
     PrintToLog("\t messageType: %d\n",type);
     PrintToLog("\t ecosystem: %d\n", ecosystem);
  }

  return true;
}


/** Tx 33 */
bool CMPTransaction::interpret_ContractDexClosePosition()
{

  // if (pkt_size < 17) {
  //     return false;
  // }

  memcpy(&ecosystem, &pkt[4], 1);
  memcpy(&contractId, &pkt[5], 4);
  SwapByteOrder32(contractId);

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t ecosystem: %d\n", ecosystem);
      PrintToLog("\t contractId: %d\n", contractId);
  }

  return true;
}

/** Tx 34 */
bool CMPTransaction::interpret_ContractDex_Cancel_Orders_By_Block()
{
    // if (pkt_size < 17) {
    //     return false;
    // }

    memcpy(&block, &pkt[4], 1);
    memcpy(&tx_idx, &pkt[5], 1);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t block: %d\n", block);
      PrintToLog("\t tx_idx: %d\n", tx_idx);
    }

    return true;
}

/** Tx 105 */
bool CMPTransaction::interpret_Set_Oracle()
{
  // if (pkt_size < 17) {
  //     return false;
  // }

    memcpy(&oracle_high, &pkt[4], 8);
    SwapByteOrder64(oracle_high);
    memcpy(&oracle_low, &pkt[12], 8);
    SwapByteOrder64(oracle_low);
    memcpy(&propertyId, &pkt[20], 4);
    SwapByteOrder32(propertyId);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t oracle high price: %d\n",oracle_high);
        PrintToLog("\t oracle low price: %d\n",oracle_low);
        PrintToLog("\t propertyId: %d\n", propertyId);
    }

    return true;
}

/** Tx 106 */
bool CMPTransaction::interpret_OracleBackup()
{
    // if (pkt_size < 17) {
    //     return false;
    // }
    memcpy(&contractId, &pkt[4], 4);
    SwapByteOrder32(contractId);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t contractId: %d\n", contractId);
    }

    return true;
}

/** Tx 107 */
bool CMPTransaction::interpret_CloseOracle()
{
    // if (pkt_size < 17) {
    //     return false;
    // }
    memcpy(&contractId, &pkt[4], 4);
    SwapByteOrder32(contractId);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t contractId: %d\n", contractId);
    }

    return true;
}

/** Tx 104 */
bool CMPTransaction::interpret_Change_OracleRef()
{
    // if (pkt_size < 17) {
    //     return false;
    // }
    memcpy(&contractId, &pkt[4], 4);
    SwapByteOrder32(contractId);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t propertyId: %d\n", propertyId);
    }

    return true;
}

/** Tx 113 */
bool CMPTransaction::interpret_Create_Channel()
{
  // if (pkt_size < 17) {
  //     return false;
  // }

    memcpy(&block_forexpiry, &pkt[4], 1);
    const char* p = 1 + (char*) &pkt[4]; // next char
    std::vector<std::string> spstr;

    spstr.push_back(std::string(p));
    p += spstr.back().size() + 1;

    memcpy(channel_address, spstr[0].c_str(), std::min(spstr[0].length(), sizeof(channel_address)-1));


  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t channelAddress : %d\n",channel_address);
      PrintToLog("\t first address : %d\n", sender);
      PrintToLog("\t second address : %d\n", receiver);
      PrintToLog("\t blocks : %d\n", block_forexpiry);
  }

  return true;
}

/** Tx 108 */
bool CMPTransaction::interpret_CommitChannel()
{
    // if (pkt_size < 17) {
    //     return false;
    // }
    memcpy(&propertyId, &pkt[4], 4);
    SwapByteOrder32(propertyId);
    memcpy(&amount_commited, &pkt[8], 8);
    SwapByteOrder64(amount_commited);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t channelAddress: %s\n", receiver);
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t propertyId: %d\n", propertyId);
        PrintToLog("\t amount commited: %d\n", amount_commited);
    }

    return true;
}

/** Tx 109 */
bool CMPTransaction::interpret_Withdrawal_FromChannel()
{
    // if (pkt_size < 17) {
    //     return false;
    // }
    memcpy(&propertyId, &pkt[4], 4);
    SwapByteOrder32(propertyId);
    memcpy(&amount_commited, &pkt[8], 8);
    SwapByteOrder64(amount_to_withdraw);

    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t channelAddress: %s\n", receiver);
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t propertyId: %d\n", propertyId);
        PrintToLog("\t amount to withdrawal: %d\n", amount_to_withdraw);
    }

    return true;
}

/** Tx 110 */
bool CMPTransaction::interpret_Instant_Trade()
{
    // if (pkt_size < 17) {
    //     return false;
    // }
    memcpy(&property, &pkt[4], 4);
    SwapByteOrder32(propertyId);
    memcpy(&amount_forsale, &pkt[8], 8);
    SwapByteOrder64(amount_to_withdraw);
    memcpy(&block_forexpiry, &pkt[16], 1);
    memcpy(&desired_property, &pkt[17], 4);
    SwapByteOrder32(desired_property);
    memcpy(&desired_value, &pkt[21], 8);


    if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
    {
        PrintToLog("\t version: %d\n", version);
        PrintToLog("\t messageType: %d\n",type);
        PrintToLog("\t property: %d\n", property);
        PrintToLog("\t amount : %d\n", amount_forsale);
        PrintToLog("\t blockheight_expiry : %d\n", block_forexpiry);
        PrintToLog("\t property desired : %d\n", desired_property);
        PrintToLog("\t amount desired : %d\n", desired_value);
    }

    return true;
}

/** Tx 114 */
bool CMPTransaction::interpret_Contract_Instant()
{
  // if (pkt_size < 17) {
  //     return false;
  // }

  memcpy(&property, &pkt[4], 4);
  SwapByteOrder32(propertyId);
  memcpy(&amount_forsale, &pkt[8], 8);
  SwapByteOrder64(amount_to_withdraw);
  memcpy(&block_forexpiry, &pkt[16], 1);
  memcpy(&price, &pkt[17], 8);
  SwapByteOrder64(price);
  memcpy(&itrading_action, &pkt[25], 8);
  SwapByteOrder64(itrading_action);
  memcpy(&ileverage, &pkt[33], 8);
  SwapByteOrder64(ileverage);

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t property: %d\n", property);
      PrintToLog("\t amount : %d\n", amount_forsale);
      PrintToLog("\t blockfor_expiry : %d\n", block_forexpiry);
      PrintToLog("\t price : %d\n", price);
      PrintToLog("\t trading action : %d\n", itrading_action);
      PrintToLog("\t leverage : %d\n", ileverage);
  }

  return true;
}

/** Tx  115*/
bool CMPTransaction::interpret_New_Id_Registration()
{
  // if (pkt_size < 17) {
  //     return false;
  // }

  memcpy(&tokens, &pkt[4], 1);
  memcpy(&ltc, &pkt[5], 1);
  memcpy(&natives, &pkt[6], 1);
  memcpy(&oracles, &pkt[7], 1);

  const char* p = 1 + (char*) &pkt[7]; // next char
  std::vector<std::string> spstr;

  for (int i = 0; i < 2; i++) {
      spstr.push_back(std::string(p));
      p += spstr.back().size() + 1;
  }

  int i = 0;

  memcpy(website, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(website)-1));i++;
  memcpy(company_name, spstr[i].c_str(), std::min(spstr[i].length(), sizeof(company_name)-1));i++;

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t address: %s\n", sender);
      PrintToLog("\t website: %s\n", website);
      PrintToLog("\t company name: %s\n", company_name);
      PrintToLog("\t tokens: %d\n", tokens);
      PrintToLog("\t ltc: %d\n", ltc);
      PrintToLog("\t natives: %d\n", natives);
      PrintToLog("\t oracles: %d\n", oracles);
  }

  return true;
}

/** Tx  116*/
bool CMPTransaction::interpret_Update_Id_Registration()
{
  // if (pkt_size < 17) {
  //     return false;
  // }

  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t sender: %s\n", sender);
      PrintToLog("\t receiver: %s\n", receiver);
  }

  return true;
}

/** Tx 112 */
bool CMPTransaction::interpret_Transfer()
{
  // if (pkt_size < 17) {
  //     return false;
  // }

  memcpy(&property, &pkt[4], 4);
  SwapByteOrder32(propertyId);
  memcpy(&amount, &pkt[8], 8);
  SwapByteOrder64(amount);


  if ((!rpcOnly && msc_debug_packets) || msc_debug_packets_readonly)
  {
      PrintToLog("\t version: %d\n", version);
      PrintToLog("\t messageType: %d\n",type);
      PrintToLog("\t property: %d\n", property);
      PrintToLog("\t amount : %d\n", amount);
  }

  return true;
}


// ---------------------- CORE LOGIC -------------------------

/**
 * Interprets the payload and executes the logic.
 *
 * @return  0  if the transaction is fully valid
 *         <0  if the transaction is invalid
 */
int CMPTransaction::interpretPacket()
{
    if (rpcOnly) {
        PrintToLog("%s(): ERROR: attempt to execute logic in RPC mode\n", __func__);
        return (PKT_ERROR -1);
    }

    if (!interpret_Transaction()) {
        return (PKT_ERROR -2);
    }

    LOCK(cs_tally);

    if (isAddressFrozen(sender, property)) {
        PrintToLog("%s(): REJECTED: address %s is frozen for property %d\n", __func__, sender, property);
        return (PKT_ERROR -3);
    }

    switch (type) {
        case MSC_TYPE_SIMPLE_SEND:
            return logicMath_SimpleSend();

        case MSC_TYPE_SEND_TO_OWNERS:
            return logicMath_SendToOwners();

        case MSC_TYPE_SEND_ALL:
            return logicMath_SendAll();

        case MSC_TYPE_TRADE_OFFER:
            return logicMath_TradeOffer();

        case MSC_TYPE_ACCEPT_OFFER_BTC:
            return logicMath_AcceptOffer_BTC();

        case MSC_TYPE_METADEX_TRADE:
            return logicMath_MetaDExTrade();

        case MSC_TYPE_METADEX_CANCEL_PRICE:
            return logicMath_MetaDExCancelPrice();

        case MSC_TYPE_METADEX_CANCEL_PAIR:
            return logicMath_MetaDExCancelPair();

        case MSC_TYPE_METADEX_CANCEL_ECOSYSTEM:
            return logicMath_MetaDExCancelEcosystem();

        case MSC_TYPE_CREATE_PROPERTY_FIXED:
            return logicMath_CreatePropertyFixed();

        case MSC_TYPE_CREATE_PROPERTY_VARIABLE:
            return logicMath_CreatePropertyVariable();

        case MSC_TYPE_CLOSE_CROWDSALE:
            return logicMath_CloseCrowdsale();

        case MSC_TYPE_CREATE_PROPERTY_MANUAL:
            return logicMath_CreatePropertyManaged();

        case MSC_TYPE_GRANT_PROPERTY_TOKENS:
            return logicMath_GrantTokens();

        case MSC_TYPE_REVOKE_PROPERTY_TOKENS:
            return logicMath_RevokeTokens();

        case MSC_TYPE_CHANGE_ISSUER_ADDRESS:
            return logicMath_ChangeIssuer();

        case MSC_TYPE_ENABLE_FREEZING:
            return logicMath_EnableFreezing();

        case MSC_TYPE_DISABLE_FREEZING:
            return logicMath_DisableFreezing();

        case MSC_TYPE_FREEZE_PROPERTY_TOKENS:
            return logicMath_FreezeTokens();

        case MSC_TYPE_UNFREEZE_PROPERTY_TOKENS:
            return logicMath_UnfreezeTokens();

        case OMNICORE_MESSAGE_TYPE_DEACTIVATION:
            return logicMath_Deactivation();

        case OMNICORE_MESSAGE_TYPE_ACTIVATION:
            return logicMath_Activation();

        case OMNICORE_MESSAGE_TYPE_ALERT:
            return logicMath_Alert();

        case MSC_TYPE_DEX_PAYMENT:
            return logicMath_DEx_Payment();

        case MSC_TYPE_SEND_VESTING:
            return logicMath_SendVestingTokens();

        case MSC_TYPE_CREATE_CONTRACT:
                return logicMath_CreateContractDex();

        case MSC_TYPE_CREATE_ORACLE_CONTRACT:
                return logicMath_CreateOracleContract();

        case MSC_TYPE_CONTRACTDEX_TRADE:
                return logicMath_ContractDexTrade();

        case MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM:
                return logicMath_ContractDexCancelEcosystem();

        case MSC_TYPE_CONTRACTDEX_CLOSE_POSITION:
                return logicMath_ContractDexClosePosition();

        case MSC_TYPE_CONTRACTDEX_CANCEL_ORDERS_BY_BLOCK:
                return logicMath_ContractDex_Cancel_Orders_By_Block();

        case MSC_TYPE_SET_ORACLE:
                return logicMath_Set_Oracle();

        case MSC_TYPE_ORACLE_BACKUP:
                return logicMath_OracleBackup();

        case MSC_TYPE_CLOSE_ORACLE:
                return logicMath_CloseOracle();

        case MSC_TYPE_CHANGE_ORACLE_REF:
                return logicMath_Change_OracleRef();

/**********************************************************/
        case MSC_TYPE_CREATE_CHANNEL:
                return logicMath_Create_Channel();

        case MSC_TYPE_COMMIT_CHANNEL:
                return logicMath_CommitChannel();

        case MSC_TYPE_WITHDRAWAL_FROM_CHANNEL:
                return logicMath_Withdrawal_FromChannel();

        case MSC_TYPE_INSTANT_TRADE:
                return logicMath_Instant_Trade();

        case MSC_TYPE_NEW_ID_REGISTRATION:
                return logicMath_New_Id_Registration();

        case MSC_TYPE_TRANSFER:
                return logicMath_Transfer();
    }

    return (PKT_ERROR -100);
}

/** Tx 108 */
int CMPTransaction::logicMath_CommitChannel()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!pDbTradeList->checkChannelAddress(receiver)) {
        PrintToLog("%s(): rejected: address %s doesn't belong to multisig channel\n", __func__, receiver);
        return (PKT_ERROR_CHANNELS -10);
    }

    if (!IsPropertyIdValid(propertyId)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }


    // ------------------------------------------

    // logic for the commit Here

    if(msc_debug_commit_channel) PrintToLog("%s():sender: %s, channelAddress: %s\n",__func__, sender, receiver);

    //putting money into channel reserve
    assert(update_tally_map(sender, propertyId, -amount_commited, BALANCE));
    assert(update_tally_map(receiver, propertyId, amount_commited, CHANNEL_RESERVE));

    pDbTradeList->recordNewCommit(txid, receiver, sender, propertyId, amount_commited, block, tx_idx);

    int64_t amountCheck = GetTokenBalance(receiver, propertyId,CHANNEL_RESERVE);

    if(msc_debug_commit_channel) PrintToLog("amount inside channel multisig: %s\n",amountCheck);

    return 0;
}

/** Passive effect of crowdsale participation. */
int CMPTransaction::logicHelper_CrowdsaleParticipation()
{
    CMPCrowd* pcrowdsale = getCrowd(receiver);

    // No active crowdsale
    if (pcrowdsale == nullptr) {
        return (PKT_ERROR_CROWD -1);
    }
    // Active crowdsale, but not for this property
    if (pcrowdsale->getCurrDes() != property) {
        return (PKT_ERROR_CROWD -2);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(pcrowdsale->getPropertyId(), sp));
    PrintToLog("INVESTMENT SEND to Crowdsale Issuer: %s\n", receiver);

    // Holds the tokens to be credited to the sender and issuer
    std::pair<int64_t, int64_t> tokens;

    // Passed by reference to determine, if max_tokens has been reached
    bool close_crowdsale = false;

    // Units going into the calculateFundraiser function must match the unit of
    // the fundraiser's property_type. By default this means satoshis in and
    // satoshis out. In the condition that the fundraiser is divisible, but
    // indivisible tokens are accepted, it must account for .0 Div != 1 Indiv,
    // but actually 1.0 Div == 100000000 Indiv. The unit must be shifted or the
    // values will be incorrect, which is what is checked below.
    bool inflateAmount = isPropertyDivisible(property) ? false : true;

    // Calculate the amounts to credit for this fundraiser
    calculateFundraiser(inflateAmount, nValue, sp.early_bird, sp.deadline, blockTime,
            sp.num_tokens, sp.percentage, getTotalTokens(pcrowdsale->getPropertyId()),
            tokens, close_crowdsale);

    if (msc_debug_sp) {
        PrintToLog("%s(): granting via crowdsale to user: %s %d (%s)\n",
                __func__, FormatMP(property, tokens.first), property, strMPProperty(property));
        PrintToLog("%s(): granting via crowdsale to issuer: %s %d (%s)\n",
                __func__, FormatMP(property, tokens.second), property, strMPProperty(property));
    }

    // Update the crowdsale object
    pcrowdsale->incTokensUserCreated(tokens.first);
    pcrowdsale->incTokensIssuerCreated(tokens.second);

    // Data to pass to txFundraiserData
    int64_t txdata[] = {(int64_t) nValue, blockTime, tokens.first, tokens.second};
    std::vector<int64_t> txDataVec(txdata, txdata + sizeof(txdata) / sizeof(txdata[0]));

    // Insert data about crowdsale participation
    pcrowdsale->insertDatabase(txid, txDataVec);

    // Credit tokens for this fundraiser
    if (tokens.first > 0) {
        assert(update_tally_map(sender, pcrowdsale->getPropertyId(), tokens.first, BALANCE));
    }
    if (tokens.second > 0) {
        assert(update_tally_map(receiver, pcrowdsale->getPropertyId(), tokens.second, BALANCE));
    }

    // Number of tokens has changed, update fee distribution thresholds
    NotifyTotalTokensChanged(pcrowdsale->getPropertyId(), block);

    // Close crowdsale, if we hit MAX_TOKENS
    if (close_crowdsale) {
        eraseMaxedCrowdsale(receiver, blockTime, block);
    }

    // Indicate, if no tokens were transferred
    if (!tokens.first && !tokens.second) {
        return (PKT_ERROR_CROWD -3);
    }

    return 0;
}

/** Tx 0 */
int CMPTransaction::logicMath_SimpleSend()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SEND -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d", __func__, nValue);
        return (PKT_ERROR_SEND -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SEND -24);
    }

    int64_t nBalance = GetTokenBalance(sender, property, BALANCE);
    if (nBalance < (int64_t) nValue) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
                __func__,
                sender,
                property,
                FormatMP(property, nBalance),
                FormatMP(property, nValue));
        return (PKT_ERROR_SEND -25);
    }

    // ------------------------------------------

    // Move the tokens
    assert(update_tally_map(sender, property, -nValue, BALANCE));
    assert(update_tally_map(receiver, property, nValue, BALANCE));

    // Is there an active crowdsale running from this recipient?
    logicHelper_CrowdsaleParticipation();

    return 0;
}

/** Tx 3 */
int CMPTransaction::logicMath_SendToOwners()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_STO -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_STO -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_STO -24);
    }

    if (version > MP_TX_PKT_V0) {
        if (!IsPropertyIdValid(distribution_property)) {
            PrintToLog("%s(): rejected: distribution property %d does not exist\n", __func__, distribution_property);
            return (PKT_ERROR_STO -24);
        }
    }

    int64_t nBalance = GetTokenBalance(sender, property, BALANCE);
    if (nBalance < (int64_t) nValue) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
                __func__,
                sender,
                FormatMP(property, nBalance),
                FormatMP(property, nValue),
                property);
        return (PKT_ERROR_STO -25);
    }

    // ------------------------------------------

    uint32_t distributeTo = (version == MP_TX_PKT_V0) ? property : distribution_property;
    OwnerAddrType receiversSet = STO_GetReceivers(sender, distributeTo, nValue);
    uint64_t numberOfReceivers = receiversSet.size();

    // make sure we found some owners
    if (numberOfReceivers <= 0) {
        PrintToLog("%s(): rejected: no other owners of property %d [owners=%d <= 0]\n", __func__, distributeTo, numberOfReceivers);
        return (PKT_ERROR_STO -26);
    }

    // determine which property the fee will be paid in
    uint32_t feeProperty = isTestEcosystemProperty(property) ? OMNI_PROPERTY_TMSC : OMNI_PROPERTY_MSC;
    int64_t feePerOwner = (version == MP_TX_PKT_V0) ? TRANSFER_FEE_PER_OWNER : TRANSFER_FEE_PER_OWNER_V1;
    int64_t transferFee = feePerOwner * numberOfReceivers;
    PrintToLog("\t    Transfer fee: %s %s\n", FormatDivisibleMP(transferFee), strMPProperty(feeProperty));

    // enough coins to pay the fee?
    if (feeProperty != property) {
        int64_t nBalanceFee = GetTokenBalance(sender, feeProperty, BALANCE);
        if (nBalanceFee < transferFee) {
            PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d to pay for fee [%s < %s]\n",
                    __func__,
                    sender,
                    feeProperty,
                    FormatMP(property, nBalanceFee),
                    FormatMP(property, transferFee));
            return (PKT_ERROR_STO -27);
        }
    } else {
        // special case check, only if distributing MSC or TMSC -- the property the fee will be paid in
        int64_t nBalanceFee = GetTokenBalance(sender, feeProperty, BALANCE);
        if (nBalanceFee < ((int64_t) nValue + transferFee)) {
            PrintToLog("%s(): rejected: sender %s has insufficient balance of %d to pay for amount + fee [%s < %s + %s]\n",
                    __func__,
                    sender,
                    feeProperty,
                    FormatMP(property, nBalanceFee),
                    FormatMP(property, nValue),
                    FormatMP(property, transferFee));
            return (PKT_ERROR_STO -28);
        }
    }

    // ------------------------------------------

    assert(update_tally_map(sender, feeProperty, -transferFee, BALANCE));
    if (version == MP_TX_PKT_V0) {
        // v0 - do not credit the subtracted fee to any tally (ie burn the tokens)
    } else {
        // v1 - credit the subtracted fee to the fee cache
        pDbFeeCache->AddFee(feeProperty, block, transferFee);
    }

    // split up what was taken and distribute between all holders
    int64_t sent_so_far = 0;
    for (OwnerAddrType::reverse_iterator it = receiversSet.rbegin(); it != receiversSet.rend(); ++it) {
        const std::string& address = it->second;

        int64_t will_really_receive = it->first;
        sent_so_far += will_really_receive;

        // real execution of the loop
        assert(update_tally_map(sender, property, -will_really_receive, BALANCE));
        assert(update_tally_map(address, property, will_really_receive, BALANCE));

        // add to stodb
        pDbStoList->recordSTOReceive(address, txid, block, property, will_really_receive);

        if (sent_so_far != (int64_t)nValue) {
            PrintToLog("sent_so_far= %14d, nValue= %14d, n_owners= %d\n", sent_so_far, nValue, numberOfReceivers);
        } else {
            PrintToLog("SendToOwners: DONE HERE\n");
        }
    }

    // sent_so_far must equal nValue here
    assert(sent_so_far == (int64_t)nValue);

    // Number of tokens has changed, update fee distribution thresholds
    if (version == MP_TX_PKT_V0) NotifyTotalTokensChanged(OMNI_PROPERTY_MSC, block); // fee was burned

    return 0;
}

/** Tx 4 */
int CMPTransaction::logicMath_SendAll()
{
    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                ecosystem,
                block);
        return (PKT_ERROR_SEND_ALL -22);
    }

    // ------------------------------------------

    CMPTally* ptally = getTally(sender);
    if (ptally == nullptr) {
        PrintToLog("%s(): rejected: sender %s has no tokens to send\n", __func__, sender);
        return (PKT_ERROR_SEND_ALL -54);
    }

    uint32_t propertyId = ptally->init();
    int numberOfPropertiesSent = 0;

    while (0 != (propertyId = ptally->next())) {
        // only transfer tokens in the specified ecosystem
        if (ecosystem == OMNI_PROPERTY_MSC && isTestEcosystemProperty(propertyId)) {
            continue;
        }
        if (ecosystem == OMNI_PROPERTY_TMSC && isMainEcosystemProperty(propertyId)) {
            continue;
        }

        // do not transfer tokens from a frozen property
        if (isAddressFrozen(sender, propertyId)) {
            PrintToLog("%s(): sender %s is frozen for property %d - the property will not be included in processing.\n", __func__, sender, propertyId);
            continue;
        }

        int64_t moneyAvailable = ptally->getMoney(propertyId, BALANCE);
        if (moneyAvailable > 0) {
            ++numberOfPropertiesSent;
            assert(update_tally_map(sender, propertyId, -moneyAvailable, BALANCE));
            assert(update_tally_map(receiver, propertyId, moneyAvailable, BALANCE));
            pDbTransactionList->recordSendAllSubRecord(txid, numberOfPropertiesSent, propertyId, moneyAvailable);
        }
    }

    if (!numberOfPropertiesSent) {
        PrintToLog("%s(): rejected: sender %s has no tokens to send\n", __func__, sender);
        return (PKT_ERROR_SEND_ALL -55);
    }

    nNewValue = numberOfPropertiesSent;

    return 0;
}

/** Tx 5 */
int CMPTransaction::logicMath_SendVestingTokens()
{
  assert(update_tally_map(sender, property, -nValue, BALANCE));
  assert(update_tally_map(receiver, property, nValue, BALANCE));
  assert(update_tally_map(receiver, OMNI_PROPERTY_ALL, nValue, UNVESTED));

  vestingAddresses.push_back(receiver);

  return 0;
}

/** Tx 20 */
int CMPTransaction::logicMath_TradeOffer()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TRADEOFFER -22);
    }

    if (MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_TRADEOFFER -23);
    }

    // Ensure only OMNI and TOMNI are allowed, when the DEx is not yet free
    if (!IsFeatureActivated(FEATURE_FREEDEX, block)) {
        if (OMNI_PROPERTY_TMSC != property && OMNI_PROPERTY_MSC != property) {
            PrintToLog("%s(): rejected: property for sale %d must be OMN or TOMN\n", __func__, property);
            return (PKT_ERROR_TRADEOFFER -47);
        }
    }

    // ------------------------------------------

    int rc = PKT_ERROR_TRADEOFFER;

    // figure out which Action this is based on amount for sale, version & etc.
    switch (version)
    {
        case MP_TX_PKT_V0:
        {
            if (0 != nValue) {
                if (!DEx_offerExists(sender, property)) {
                    rc = DEx_offerCreate(sender, property, nValue, block, amount_desired, min_fee, blocktimelimit, txid, &nNewValue);
                } else {
                    rc = DEx_offerUpdate(sender, property, nValue, block, amount_desired, min_fee, blocktimelimit, txid, &nNewValue);
                }
            } else {
                // what happens if nValue is 0 for V0 ?  ANSWER: check if exists and it does -- cancel, otherwise invalid
                if (DEx_offerExists(sender, property)) {
                    rc = DEx_offerDestroy(sender, property);
                } else {
                    PrintToLog("%s(): rejected: sender %s has no active sell offer for property: %d\n", __func__, sender, property);
                    rc = (PKT_ERROR_TRADEOFFER -49);
                }
            }

            break;
        }

        case MP_TX_PKT_V1:
        {
            if (DEx_offerExists(sender, property)) {
                if (CANCEL != subaction && UPDATE != subaction) {
                    PrintToLog("%s(): rejected: sender %s has an active sell offer for property: %d\n", __func__, sender, property);
                    rc = (PKT_ERROR_TRADEOFFER -48);
                    break;
                }
            } else {
                // Offer does not exist
                if (NEW != subaction) {
                    PrintToLog("%s(): rejected: sender %s has no active sell offer for property: %d\n", __func__, sender, property);
                    rc = (PKT_ERROR_TRADEOFFER -49);
                    break;
                }
            }

            switch (subaction) {
                case NEW:
                    rc = DEx_offerCreate(sender, property, nValue, block, amount_desired, min_fee, blocktimelimit, txid, &nNewValue);
                    break;

                case UPDATE:
                    rc = DEx_offerUpdate(sender, property, nValue, block, amount_desired, min_fee, blocktimelimit, txid, &nNewValue);
                    break;

                case CANCEL:
                    rc = DEx_offerDestroy(sender, property);
                    break;

                default:
                    rc = (PKT_ERROR -999);
                    break;
            }
            break;
        }

        default:
            rc = (PKT_ERROR -500); // neither V0 nor V1
            break;
    };

    return rc;
}

/** Tx 22 */
int CMPTransaction::logicMath_AcceptOffer_BTC()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (DEX_ERROR_ACCEPT -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (DEX_ERROR_ACCEPT -23);
    }

    // ------------------------------------------

    // the min fee spec requirement is checked in the following function
    int rc = DEx_acceptCreate(sender, receiver, property, nValue, block, tx_fee_paid, &nNewValue);

    return rc;
}

/** Tx 25 */
int CMPTransaction::logicMath_MetaDExTrade()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_METADEX -22);
    }

    if (property == desired_property) {
        PrintToLog("%s(): rejected: property for sale %d and desired property %d must not be equal\n",
                __func__,
                property,
                desired_property);
        return (PKT_ERROR_METADEX -29);
    }

    if (isTestEcosystemProperty(property) != isTestEcosystemProperty(desired_property)) {
        PrintToLog("%s(): rejected: property for sale %d and desired property %d not in same ecosystem\n",
                __func__,
                property,
                desired_property);
        return (PKT_ERROR_METADEX -30);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
        return (PKT_ERROR_METADEX -31);
    }

    if (!IsPropertyIdValid(desired_property)) {
        PrintToLog("%s(): rejected: desired property %d does not exist\n", __func__, desired_property);
        return (PKT_ERROR_METADEX -32);
    }

    if (nNewValue <= 0 || MAX_INT_8_BYTES < nNewValue) {
        PrintToLog("%s(): rejected: amount for sale out of range or zero: %d\n", __func__, nNewValue);
        return (PKT_ERROR_METADEX -33);
    }

    if (desired_value <= 0 || MAX_INT_8_BYTES < desired_value) {
        PrintToLog("%s(): rejected: desired amount out of range or zero: %d\n", __func__, desired_value);
        return (PKT_ERROR_METADEX -34);
    }

    if (!IsFeatureActivated(FEATURE_TRADEALLPAIRS, block)) {
        // Trading non-Omni pairs is not allowed before trading all pairs is activated
        if ((property != OMNI_PROPERTY_MSC) && (desired_property != OMNI_PROPERTY_MSC) &&
            (property != OMNI_PROPERTY_TMSC) && (desired_property != OMNI_PROPERTY_TMSC)) {
            PrintToLog("%s(): rejected: one side of a trade [%d, %d] must be OMN or TOMN\n", __func__, property, desired_property);
            return (PKT_ERROR_METADEX -35);
        }
    }

    int64_t nBalance = GetTokenBalance(sender, property, BALANCE);
    if (nBalance < (int64_t) nNewValue) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
                __func__,
                sender,
                property,
                FormatMP(property, nBalance),
                FormatMP(property, nNewValue));
        return (PKT_ERROR_METADEX -25);
    }

    // ------------------------------------------

    pDbTradeList->recordNewTrade(txid, sender, property, desired_property, block, tx_idx);
    int rc = MetaDEx_ADD(sender, property, nNewValue, block, desired_property, desired_value, txid, tx_idx);
    return rc;
}

/** Tx 26 */
int CMPTransaction::logicMath_MetaDExCancelPrice()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_METADEX -22);
    }

    if (property == desired_property) {
        PrintToLog("%s(): rejected: property for sale %d and desired property %d must not be equal\n",
                __func__,
                property,
                desired_property);
        return (PKT_ERROR_METADEX -29);
    }

    if (isTestEcosystemProperty(property) != isTestEcosystemProperty(desired_property)) {
        PrintToLog("%s(): rejected: property for sale %d and desired property %d not in same ecosystem\n",
                __func__,
                property,
                desired_property);
        return (PKT_ERROR_METADEX -30);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
        return (PKT_ERROR_METADEX -31);
    }

    if (!IsPropertyIdValid(desired_property)) {
        PrintToLog("%s(): rejected: desired property %d does not exist\n", __func__, desired_property);
        return (PKT_ERROR_METADEX -32);
    }

    if (nNewValue <= 0 || MAX_INT_8_BYTES < nNewValue) {
        PrintToLog("%s(): rejected: amount for sale out of range or zero: %d\n", __func__, nNewValue);
        return (PKT_ERROR_METADEX -33);
    }

    if (desired_value <= 0 || MAX_INT_8_BYTES < desired_value) {
        PrintToLog("%s(): rejected: desired amount out of range or zero: %d\n", __func__, desired_value);
        return (PKT_ERROR_METADEX -34);
    }

    // ------------------------------------------

    int rc = MetaDEx_CANCEL_AT_PRICE(txid, block, sender, property, nNewValue, desired_property, desired_value);

    return rc;
}

/** Tx 27 */
int CMPTransaction::logicMath_MetaDExCancelPair()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_METADEX -22);
    }

    if (property == desired_property) {
        PrintToLog("%s(): rejected: property for sale %d and desired property %d must not be equal\n",
                __func__,
                property,
                desired_property);
        return (PKT_ERROR_METADEX -29);
    }

    if (isTestEcosystemProperty(property) != isTestEcosystemProperty(desired_property)) {
        PrintToLog("%s(): rejected: property for sale %d and desired property %d not in same ecosystem\n",
                __func__,
                property,
                desired_property);
        return (PKT_ERROR_METADEX -30);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
        return (PKT_ERROR_METADEX -31);
    }

    if (!IsPropertyIdValid(desired_property)) {
        PrintToLog("%s(): rejected: desired property %d does not exist\n", __func__, desired_property);
        return (PKT_ERROR_METADEX -32);
    }

    // ------------------------------------------

    int rc = MetaDEx_CANCEL_ALL_FOR_PAIR(txid, block, sender, property, desired_property);

    return rc;
}

/** Tx 28 */
int CMPTransaction::logicMath_MetaDExCancelEcosystem()
{
    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_METADEX -22);
    }

    if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, ecosystem);
        return (PKT_ERROR_METADEX -21);
    }

    int rc = MetaDEx_CANCEL_EVERYTHING(txid, block, sender, ecosystem);

    return rc;
}

/** Tx 50 */
int CMPTransaction::logicMath_CreatePropertyFixed()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, (uint32_t) ecosystem);
        return (PKT_ERROR_SP -21);
    }

    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_SP -23);
    }

    if (MSC_PROPERTY_TYPE_INDIVISIBLE != prop_type && MSC_PROPERTY_TYPE_DIVISIBLE != prop_type) {
        PrintToLog("%s(): rejected: invalid property type: %d\n", __func__, prop_type);
        return (PKT_ERROR_SP -36);
    }

    if ('\0' == name[0]) {
        PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
        return (PKT_ERROR_SP -37);
    }

    // ------------------------------------------

    CMPSPInfo::Entry newSP;
    newSP.issuer = sender;
    newSP.updateIssuer(block, tx_idx, sender);
    newSP.txid = txid;
    newSP.prop_type = prop_type;
    newSP.num_tokens = nValue;
    newSP.category.assign(category);
    newSP.subcategory.assign(subcategory);
    newSP.name.assign(name);
    newSP.url.assign(url);
    newSP.data.assign(data);
    newSP.fixed = true;
    newSP.creation_block = blockHash;
    newSP.update_block = newSP.creation_block;

    const uint32_t propertyId = pDbSpInfo->putSP(ecosystem, newSP);
    assert(propertyId > 0);
    assert(update_tally_map(sender, propertyId, nValue, BALANCE));

    NotifyTotalTokensChanged(propertyId, block);

    return 0;
}

/** Tx 51 */
int CMPTransaction::logicMath_CreatePropertyVariable()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, (uint32_t) ecosystem);
        return (PKT_ERROR_SP -21);
    }

    if (IsFeatureActivated(FEATURE_SPCROWDCROSSOVER, block)) {
    /**
     * Ecosystem crossovers shall not be allowed after the feature was enabled.
     */
    if (isTestEcosystemProperty(ecosystem) != isTestEcosystemProperty(property)) {
        PrintToLog("%s(): rejected: ecosystem %d of tokens to issue and desired property %d not in same ecosystem\n",
                __func__,
                ecosystem,
                property);
        return (PKT_ERROR_SP -50);
    }
    }

    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_SP -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SP -24);
    }

    if (MSC_PROPERTY_TYPE_INDIVISIBLE != prop_type && MSC_PROPERTY_TYPE_DIVISIBLE != prop_type) {
        PrintToLog("%s(): rejected: invalid property type: %d\n", __func__, prop_type);
        return (PKT_ERROR_SP -36);
    }

    if ('\0' == name[0]) {
        PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
        return (PKT_ERROR_SP -37);
    }

    if (!deadline || (int64_t) deadline < blockTime) {
        PrintToLog("%s(): rejected: deadline must not be in the past [%d < %d]\n", __func__, deadline, blockTime);
        return (PKT_ERROR_SP -38);
    }

    if (nullptr != getCrowd(sender)) {
        PrintToLog("%s(): rejected: sender %s has an active crowdsale\n", __func__, sender);
        return (PKT_ERROR_SP -39);
    }

    // ------------------------------------------

    CMPSPInfo::Entry newSP;
    newSP.issuer = sender;
    newSP.updateIssuer(block, tx_idx, sender);
    newSP.txid = txid;
    newSP.prop_type = prop_type;
    newSP.num_tokens = nValue;
    newSP.category.assign(category);
    newSP.subcategory.assign(subcategory);
    newSP.name.assign(name);
    newSP.url.assign(url);
    newSP.data.assign(data);
    newSP.fixed = false;
    newSP.property_desired = property;
    newSP.deadline = deadline;
    newSP.early_bird = early_bird;
    newSP.percentage = percentage;
    newSP.creation_block = blockHash;
    newSP.update_block = newSP.creation_block;

    const uint32_t propertyId = pDbSpInfo->putSP(ecosystem, newSP);
    assert(propertyId > 0);
    my_crowds.insert(std::make_pair(sender, CMPCrowd(propertyId, nValue, property, deadline, early_bird, percentage, 0, 0)));

    PrintToLog("CREATED CROWDSALE id: %d value: %d property: %d\n", propertyId, nValue, property);

    return 0;
}

/** Tx 53 */
int CMPTransaction::logicMath_CloseCrowdsale()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_SP -24);
    }

    CrowdMap::iterator it = my_crowds.find(sender);
    if (it == my_crowds.end()) {
        PrintToLog("%s(): rejected: sender %s has no active crowdsale\n", __func__, sender);
        return (PKT_ERROR_SP -40);
    }

    const CMPCrowd& crowd = it->second;
    if (property != crowd.getPropertyId()) {
        PrintToLog("%s(): rejected: property identifier mismatch [%d != %d]\n", __func__, property, crowd.getPropertyId());
        return (PKT_ERROR_SP -41);
    }

    // ------------------------------------------

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(property, sp));

    int64_t missedTokens = GetMissedIssuerBonus(sp, crowd);

    sp.historicalData = crowd.getDatabase();
    sp.update_block = blockHash;
    sp.close_early = true;
    sp.timeclosed = blockTime;
    sp.txid_close = txid;
    sp.missedTokens = missedTokens;

    assert(pDbSpInfo->updateSP(property, sp));
    if (missedTokens > 0) {
        assert(update_tally_map(sp.issuer, property, missedTokens, BALANCE));
    }
    my_crowds.erase(it);

    if (msc_debug_sp) PrintToLog("CLOSED CROWDSALE id: %d=%X\n", property, property);

    return 0;
}

/** Tx 54 */
int CMPTransaction::logicMath_CreatePropertyManaged()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (OMNI_PROPERTY_MSC != ecosystem && OMNI_PROPERTY_TMSC != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, (uint32_t) ecosystem);
        return (PKT_ERROR_SP -21);
    }

    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_SP -22);
    }

    if (MSC_PROPERTY_TYPE_INDIVISIBLE != prop_type && MSC_PROPERTY_TYPE_DIVISIBLE != prop_type) {
        PrintToLog("%s(): rejected: invalid property type: %d\n", __func__, prop_type);
        return (PKT_ERROR_SP -36);
    }

    if ('\0' == name[0]) {
        PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
        return (PKT_ERROR_SP -37);
    }

    // ------------------------------------------

    CMPSPInfo::Entry newSP;
    newSP.issuer = sender;
    newSP.updateIssuer(block, tx_idx, sender);
    newSP.txid = txid;
    newSP.prop_type = prop_type;
    newSP.category.assign(category);
    newSP.subcategory.assign(subcategory);
    newSP.name.assign(name);
    newSP.url.assign(url);
    newSP.data.assign(data);
    newSP.fixed = false;
    newSP.manual = true;
    newSP.creation_block = blockHash;
    newSP.update_block = newSP.creation_block;

    uint32_t propertyId = pDbSpInfo->putSP(ecosystem, newSP);
    assert(propertyId > 0);

    PrintToLog("CREATED MANUAL PROPERTY id: %d admin: %s\n", propertyId, sender);

    return 0;
}

/** Tx 55 */
int CMPTransaction::logicMath_GrantTokens()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_SP -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_TOKENS -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(property, sp));

    if (!sp.manual) {
        PrintToLog("%s(): rejected: property %d is not managed\n", __func__, property);
        return (PKT_ERROR_TOKENS -42);
    }

    if (sender != sp.getIssuer(block)) {
        PrintToLog("%s(): rejected: sender %s is not issuer of property %d [issuer=%s]\n", __func__, sender, property, sp.issuer);
        return (PKT_ERROR_TOKENS -43);
    }

    int64_t nTotalTokens = getTotalTokens(property);
    if (nValue > (MAX_INT_8_BYTES - nTotalTokens)) {
        PrintToLog("%s(): rejected: no more than %s tokens can ever exist [%s + %s > %s]\n",
                __func__,
                FormatMP(property, MAX_INT_8_BYTES),
                FormatMP(property, nTotalTokens),
                FormatMP(property, nValue),
                FormatMP(property, MAX_INT_8_BYTES));
        return (PKT_ERROR_TOKENS -44);
    }

    // ------------------------------------------

    std::vector<int64_t> dataPt;
    dataPt.push_back(nValue);
    dataPt.push_back(0);
    sp.historicalData.insert(std::make_pair(txid, dataPt));
    sp.update_block = blockHash;

    // Persist the number of granted tokens
    assert(pDbSpInfo->updateSP(property, sp));

    // Move the tokens
    assert(update_tally_map(receiver, property, nValue, BALANCE));

    /**
     * As long as the feature to disable the side effects of "granting tokens"
     * is not activated, "granting tokens" can trigger crowdsale participations.
     */
    if (!IsFeatureActivated(FEATURE_GRANTEFFECTS, block)) {
        // Is there an active crowdsale running from this recipient?
        logicHelper_CrowdsaleParticipation();
    }

    NotifyTotalTokensChanged(property, block);

    return 0;
}

/** Tx 56 */
int CMPTransaction::logicMath_RevokeTokens()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (nValue <= 0 || MAX_INT_8_BYTES < nValue) {
        PrintToLog("%s(): rejected: value out of range or zero: %d\n", __func__, nValue);
        return (PKT_ERROR_TOKENS -23);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(property, sp));

    if (!sp.manual) {
        PrintToLog("%s(): rejected: property %d is not managed\n", __func__, property);
        return (PKT_ERROR_TOKENS -42);
    }

    int64_t nBalance = GetTokenBalance(sender, property, BALANCE);
    if (nBalance < (int64_t) nValue) {
        PrintToLog("%s(): rejected: sender %s has insufficient balance of property %d [%s < %s]\n",
                __func__,
                sender,
                property,
                FormatMP(property, nBalance),
                FormatMP(property, nValue));
        return (PKT_ERROR_TOKENS -25);
    }

    // ------------------------------------------

    std::vector<int64_t> dataPt;
    dataPt.push_back(0);
    dataPt.push_back(nValue);
    sp.historicalData.insert(std::make_pair(txid, dataPt));
    sp.update_block = blockHash;

    assert(update_tally_map(sender, property, -nValue, BALANCE));
    assert(pDbSpInfo->updateSP(property, sp));

    NotifyTotalTokensChanged(property, block);

    return 0;
}

/** Tx 70 */
int CMPTransaction::logicMath_ChangeIssuer()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(property, sp));

    if (sender != sp.getIssuer(block)) {
        PrintToLog("%s(): rejected: sender %s is not issuer of property %d [issuer=%s]\n", __func__, sender, property, sp.issuer);
        return (PKT_ERROR_TOKENS -43);
    }

    if (nullptr != getCrowd(sender)) {
        PrintToLog("%s(): rejected: sender %s has an active crowdsale\n", __func__, sender);
        return (PKT_ERROR_TOKENS -39);
    }

    if (receiver.empty()) {
        PrintToLog("%s(): rejected: receiver is empty\n", __func__);
        return (PKT_ERROR_TOKENS -45);
    }

    if (nullptr != getCrowd(receiver)) {
        PrintToLog("%s(): rejected: receiver %s has an active crowdsale\n", __func__, receiver);
        return (PKT_ERROR_TOKENS -46);
    }

    // ------------------------------------------

    sp.updateIssuer(block, tx_idx, receiver);

    sp.issuer = receiver;
    sp.update_block = blockHash;

    assert(pDbSpInfo->updateSP(property, sp));

    return 0;
}

/** Tx 71 */
int CMPTransaction::logicMath_EnableFreezing()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(property, sp));

    if (!sp.manual) {
        PrintToLog("%s(): rejected: property %d is not managed\n", __func__, property);
        return (PKT_ERROR_TOKENS -42);
    }

    if (sender != sp.getIssuer(block)) {
        PrintToLog("%s(): rejected: sender %s is not issuer of property %d [issuer=%s]\n", __func__, sender, property, sp.issuer);
        return (PKT_ERROR_TOKENS -43);
    }

    if (isFreezingEnabled(property, block)) {
        PrintToLog("%s(): rejected: freezing is already enabled for property %d\n", __func__, property);
        return (PKT_ERROR_TOKENS -49);
    }

    int liveBlock = 0;
    if (!IsFeatureActivated(FEATURE_FREEZENOTICE, block)) {
        liveBlock = block;
    } else {
        const CConsensusParams& params = ConsensusParams();
        liveBlock = params.OMNI_FREEZE_WAIT_PERIOD + block;
    }

    enableFreezing(property, liveBlock);

    return 0;
}

/** Tx 72 */
int CMPTransaction::logicMath_DisableFreezing()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(property, sp));

    if (!sp.manual) {
        PrintToLog("%s(): rejected: property %d is not managed\n", __func__, property);
        return (PKT_ERROR_TOKENS -42);
    }

    if (sender != sp.getIssuer(block)) {
        PrintToLog("%s(): rejected: sender %s is not issuer of property %d [issuer=%s]\n", __func__, sender, property, sp.issuer);
        return (PKT_ERROR_TOKENS -43);
    }

    if (!isFreezingEnabled(property, block)) {
        PrintToLog("%s(): rejected: freezing is not enabled for property %d\n", __func__, property);
        return (PKT_ERROR_TOKENS -47);
    }

    disableFreezing(property);

    return 0;
}

/** Tx 185 */
int CMPTransaction::logicMath_FreezeTokens()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(property, sp));

    if (!sp.manual) {
        PrintToLog("%s(): rejected: property %d is not managed\n", __func__, property);
        return (PKT_ERROR_TOKENS -42);
    }

    if (sender != sp.getIssuer(block)) {
        PrintToLog("%s(): rejected: sender %s is not issuer of property %d [issuer=%s]\n", __func__, sender, property, sp.issuer);
        return (PKT_ERROR_TOKENS -43);
    }

    if (!isFreezingEnabled(property, block)) {
        PrintToLog("%s(): rejected: freezing is not enabled for property %d\n", __func__, property);
        return (PKT_ERROR_TOKENS -47);
    }

    if (isAddressFrozen(receiver, property)) {
        PrintToLog("%s(): rejected: address %s is already frozen for property %d\n", __func__, receiver, property);
        return (PKT_ERROR_TOKENS -50);
    }

    freezeAddress(receiver, property);

    return 0;
}

/** Tx 186 */
int CMPTransaction::logicMath_UnfreezeTokens()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == nullptr) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(property)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(property, sp));

    if (!sp.manual) {
        PrintToLog("%s(): rejected: property %d is not managed\n", __func__, property);
        return (PKT_ERROR_TOKENS -42);
    }

    if (sender != sp.getIssuer(block)) {
        PrintToLog("%s(): rejected: sender %s is not issuer of property %d [issuer=%s]\n", __func__, sender, property, sp.issuer);
        return (PKT_ERROR_TOKENS -43);
    }

    if (!isFreezingEnabled(property, block)) {
        PrintToLog("%s(): rejected: freezing is not enabled for property %d\n", __func__, property);
        return (PKT_ERROR_TOKENS -47);
    }

    if (!isAddressFrozen(receiver, property)) {
        PrintToLog("%s(): rejected: address %s is not frozen for property %d\n", __func__, receiver, property);
        return (PKT_ERROR_TOKENS -48);
    }

    unfreezeAddress(receiver, property);

    return 0;
}

/** Tx 65533 */
int CMPTransaction::logicMath_Deactivation()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR -22);
    }

    // is sender authorized
    bool authorized = CheckDeactivationAuthorization(sender);

    PrintToLog("\t          sender: %s\n", sender);
    PrintToLog("\t      authorized: %s\n", authorized);

    if (!authorized) {
        PrintToLog("%s(): rejected: sender %s is not authorized to deactivate features\n", __func__, sender);
        return (PKT_ERROR -51);
    }

    // authorized, request feature deactivation
    bool DeactivationSuccess = DeactivateFeature(feature_id, block);

    if (!DeactivationSuccess) {
        PrintToLog("%s(): DeactivateFeature failed\n", __func__);
        return (PKT_ERROR -54);
    }

    // successful deactivation - did we deactivate the MetaDEx?  If so close out all trades
    if (feature_id == FEATURE_METADEX) {
        MetaDEx_SHUTDOWN();
    }
    if (feature_id == FEATURE_TRADEALLPAIRS) {
        MetaDEx_SHUTDOWN_ALLPAIR();
    }

    return 0;
}

/** Tx 65534 */
int CMPTransaction::logicMath_Activation()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR -22);
    }

    // is sender authorized - temporarily use alert auths but ## TO BE MOVED TO FOUNDATION P2SH KEY ##
    bool authorized = CheckActivationAuthorization(sender);

    PrintToLog("\t          sender: %s\n", sender);
    PrintToLog("\t      authorized: %s\n", authorized);

    if (!authorized) {
        PrintToLog("%s(): rejected: sender %s is not authorized for feature activations\n", __func__, sender);
        return (PKT_ERROR -51);
    }

    // authorized, request feature activation
    bool activationSuccess = ActivateFeature(feature_id, activation_block, min_client_version, block);

    if (!activationSuccess) {
        PrintToLog("%s(): ActivateFeature failed to activate this feature\n", __func__);
        return (PKT_ERROR -54);
    }

    return 0;
}

/** Tx 65535 */
int CMPTransaction::logicMath_Alert()
{
    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR -22);
    }

    // is sender authorized?
    bool authorized = CheckAlertAuthorization(sender);

    PrintToLog("\t          sender: %s\n", sender);
    PrintToLog("\t      authorized: %s\n", authorized);

    if (!authorized) {
        PrintToLog("%s(): rejected: sender %s is not authorized for alerts\n", __func__, sender);
        return (PKT_ERROR -51);
    }

    if (alert_type == ALERT_CLIENT_VERSION_EXPIRY && OMNICORE_VERSION < alert_expiry) {
        // regular alert keys CANNOT be used to force a client upgrade on mainnet - at least 3 signatures from board/devs are required
        if (sender == "34kwkVRSvFVEoUwcQSgpQ4ZUasuZ54DJLD" || isNonMainNet()) {
            std::string msgText = "Client upgrade is required!  Shutting down due to unsupported consensus state!";
            PrintToLog(msgText);
            PrintToConsole(msgText);
            if (!gArgs.GetBoolArg("-overrideforcedshutdown", false)) {
                fs::path persistPath = GetDataDir() / "MP_persist";
                if (fs::exists(persistPath)) fs::remove_all(persistPath); // prevent the node being restarted without a reparse after forced shutdown
                DoAbortNode(msgText, msgText);
            }
        }
    }

    if (alert_type == 65535) { // set alert type to FFFF to clear previously sent alerts
        DeleteAlerts(sender);
    } else {
        AddAlert(sender, alert_type, alert_expiry, alert_text);
    }

    // we have a new alert, fire a notify event if needed
    DoWarning(alert_text);

    return 0;
}

/** Tx 117 */
int CMPTransaction::logicMath_DEx_Payment()
{
  int rc = 2;

  if (!IsTransactionTypeAllowed(block, property, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_METADEX -22);
  }
  PrintToLog("%s(): inside the function\n",__func__);

  return rc;
}

/** Tx 41 */
int CMPTransaction::logicMath_CreateContractDex()
{
  uint256 blockHash;
  {
    LOCK(cs_main);

    CBlockIndex* pindex = chainActive[block];

      if (pindex == NULL) {
	PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
	return (PKT_ERROR_SP -20);
      }
      blockHash = pindex->GetBlockHash();
  }

  if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
          __func__,
          type,
          version,
          propertyId,
          block);
      return (PKT_ERROR_SP -22);
  }

  if ('\0' == name[0]) {
    PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
    return (PKT_ERROR_SP -37);
  }

  // PrintToLog("type of denomination: %d\n",denomination);


  // -----------------------------------------------

  CMPSPInfo::Entry newSP;
  newSP.txid = txid;
  newSP.issuer = sender;
  newSP.prop_type = prop_type;
  newSP.subcategory.assign(subcategory);
  newSP.name.assign(name);
  newSP.fixed = false;
  newSP.manual = true;
  newSP.creation_block = blockHash;
  newSP.update_block = blockHash;
  newSP.blocks_until_expiration = blocks_until_expiration;
  newSP.notional_size = notional_size;
  newSP.collateral_currency = collateral_currency;
  newSP.margin_requirement = margin_requirement;
  newSP.init_block = block;
  newSP.denomination = denomination;
  newSP.ecosystemSP = ecosystem;
  newSP.attribute_type = attribute_type;

  const uint32_t propertyId = pDbSpInfo->putSP(ecosystem, newSP);
  assert(propertyId > 0);

  return 0;
}

/** Tx 103 */
int CMPTransaction::logicMath_CreateOracleContract()
{
  uint256 blockHash;
  {
    LOCK(cs_main);

    CBlockIndex* pindex = chainActive[block];

      if (pindex == NULL)
      {
	        PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
	        return (PKT_ERROR_SP -20);
      }

      blockHash = pindex->GetBlockHash();
  }

  if (sender == receiver)
  {
      PrintToLog("%s(): ERROR: oracle and backup addresses can't be the same!\n", __func__, block);
      return (PKT_ERROR_ORACLE -10);
  }

  if (OMNI_PROPERTY_ALL != ecosystem && OMNI_PROPERTY_TALL != ecosystem) {
      PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, (uint32_t) ecosystem);
      return (PKT_ERROR_SP -21);
  }

  if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
          __func__,
          type,
          version,
          propertyId,
          block);
      return (PKT_ERROR_SP -22);
  }

  if ('\0' == name[0])
  {
      PrintToLog("%s(): rejected: property name must not be empty\n", __func__);
      return (PKT_ERROR_SP -37);
  }

  // PrintToLog("type of denomination: %d\n",denomination);


  // -----------------------------------------------

  CMPSPInfo::Entry newSP;
  newSP.txid = txid;
  newSP.issuer = sender;
  newSP.prop_type = prop_type;
  newSP.subcategory.assign(subcategory);
  newSP.name.assign(name);
  newSP.fixed = false;
  newSP.manual = true;
  newSP.creation_block = blockHash;
  newSP.update_block = blockHash;
  newSP.blocks_until_expiration = blocks_until_expiration;
  newSP.notional_size = notional_size;
  newSP.collateral_currency = collateral_currency;
  newSP.margin_requirement = margin_requirement;
  newSP.init_block = block;
  newSP.denomination = denomination;
  newSP.ecosystemSP = ecosystem;
  newSP.attribute_type = attribute_type;
  newSP.backup_address = receiver;

  const uint32_t propertyId = pDbSpInfo->putSP(ecosystem, newSP);
  assert(propertyId > 0);

  return 0;
}

int CMPTransaction::logicMath_ContractDexTrade()
{

  uint256 blockHash;
  {
    LOCK(cs_main);

    CBlockIndex* pindex = chainActive[block];
    if (pindex == NULL)
    {
	      PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
	      return (PKT_ERROR_SP -20);
    }
    blockHash = pindex->GetBlockHash();
  }

  int result;

  struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_traded);
  id_contract = pfuture->fco_propertyId;

  (pfuture->fco_prop_type == ALL_PROPERTY_TYPE_CONTRACT) ? result = 5 : result = 6;

  // if(!t_tradelistdb->checkKYCRegister(sender,result))
  //     return PKT_ERROR_KYC -10;


  if (block > pfuture->fco_init_block + static_cast<int>(pfuture->fco_blocks_until_expiration) || block < pfuture->fco_init_block)
      return PKT_ERROR_SP -38;

  uint32_t colateralh = pfuture->fco_collateral_currency;
  int64_t marginRe = static_cast<int64_t>(pfuture->fco_margin_requirement);
  int64_t nBalance = GetTokenBalance(sender, colateralh, BALANCE);

  if(msc_debug_contractdex_tx) PrintToLog("%s():colateralh: %d, marginRe: %d, nBalance: %d\n",__func__, colateralh, marginRe, nBalance);

  // // rational_t conv = notionalChange(pfuture->fco_propertyId);

  rational_t conv = rational_t(1,1);
  int64_t num = conv.numerator().convert_to<int64_t>();
  int64_t den = conv.denominator().convert_to<int64_t>();
  arith_uint256 amountTR = (ConvertTo256(amount)*ConvertTo256(marginRe)*ConvertTo256(num))/(ConvertTo256(den)*ConvertTo256(leverage));
  int64_t amountToReserve = ConvertTo64(amountTR);

  if (nBalance < amountToReserve || nBalance == 0)
    {
      PrintToLog("%s(): rejected: sender %s has insufficient balance for contracts %d [%s < %s] \n",
		 __func__,
		 sender,
		 property,
		 FormatMP(property, nBalance),
		 FormatMP(property, amountToReserve));
      return (PKT_ERROR_SEND -25);
    }
  else
    {
      if (amountToReserve > 0)
	{
	  assert(update_tally_map(sender, colateralh, -amountToReserve, BALANCE));
	  assert(update_tally_map(sender, colateralh,  amountToReserve, CONTRACTDEX_MARGIN));
	}
      // int64_t reserva = getMPbalance(sender, colateralh, CONTRACTDEX_MARGIN);
      // std::string reserved = FormatDivisibleMP(reserva,false);
    }

  /*********************************************/
  /**Logic for Node Reward**/

  // const CConsensusParams &params = ConsensusParams();
  // int BlockInit = params.MSC_NODE_REWARD;
  // int nBlockNow = GetHeight();
  //
  // BlockClass NodeRewardObj(BlockInit, nBlockNow);
  // NodeRewardObj.SendNodeReward(sender);

  /*********************************************/

  // t_tradelistdb->recordNewTrade(txid, sender, id_contract, desired_property, block, tx_idx, 0);
  int rc = ContractDex_ADD(sender, id_contract, amount, block, txid, tx_idx, effective_price, trading_action,0);

  return rc;
}

/** Tx 32 */
int CMPTransaction::logicMath_ContractDexCancelEcosystem()
{
  if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
    PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
	       __func__,
	       type,
	       version,
	       property,
	       block);
    return (PKT_ERROR_CONTRACTDEX -20);
  }

  if (OMNI_PROPERTY_ALL != ecosystem && OMNI_PROPERTY_TALL != ecosystem) {
    PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, ecosystem);
    return (PKT_ERROR_SP -21);
  }

  struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_traded);
  uint32_t contractId = pfuture->fco_propertyId;

  int rc = ContractDex_CANCEL_EVERYTHING(txid, block, sender, ecosystem, contractId);

  return rc;
}

/** Tx 33 */
int CMPTransaction::logicMath_ContractDexClosePosition()
{
    if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
            __func__,
            type,
            version,
            property,
            block);
        return (PKT_ERROR_CONTRACTDEX -20);
    }

    if (OMNI_PROPERTY_ALL != ecosystem && OMNI_PROPERTY_TALL != ecosystem) {
        PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, ecosystem);
        return (PKT_ERROR_SP -21);
    }

    CMPSPInfo::Entry sp;
    {
        LOCK(cs_tally);
        if (!pDbSpInfo->getSP(contractId, sp)) {
            PrintToLog(" %s() : Property identifier %d does not exist\n",
                __func__,
                sender,
                contractId);
            return (PKT_ERROR_SEND -24);
        }
    }

    uint32_t collateralCurrency = sp.collateral_currency;
    int rc = ContractDex_CLOSE_POSITION(txid, block, sender, ecosystem, contractId, collateralCurrency);

    return rc;
}

int CMPTransaction::logicMath_ContractDex_Cancel_Orders_By_Block()
{
  if (!IsTransactionTypeAllowed(block, ecosystem, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              propertyId,
              block);
     return (PKT_ERROR_METADEX -22);
  }

  if (OMNI_PROPERTY_ALL != ecosystem && OMNI_PROPERTY_TALL != ecosystem) {
      PrintToLog("%s(): rejected: invalid ecosystem: %d\n", __func__, ecosystem);
      return (PKT_ERROR_SP -21);
  }

    ContractDex_CANCEL_FOR_BLOCK(txid, block, tx_idx, sender, ecosystem);

    return 0;
}

/** Tx 105 */
int CMPTransaction::logicMath_Set_Oracle()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(contractId)) {
        PrintToLog("%s(): rejected: oracle %d does not exist\n", __func__, property);
        return (PKT_ERROR_ORACLE -11);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(contractId, sp));

    if (sender != sp.issuer) {
        PrintToLog("%s(): rejected: sender %s is not the oracle address of the future contract %d [oracle address=%s]\n", __func__, sender, contractId, sp.issuer);
        return (PKT_ERROR_ORACLE -12);
    }


    // ------------------------------------------

    // putting data on memory
    oraclePrices[contractId].block = block;
    oraclePrices[contractId].high = oracle_high;
    oraclePrices[contractId].low = oracle_low;

    // saving on db
    sp.oracle_high = oracle_high;
    sp.oracle_low = oracle_low;
    sp.oracle_last_update = block;

    assert(pDbSpInfo->updateSP(contractId, sp));

    if (msc_debug_set_oracle) PrintToLog("oracle data for contract: block: %d,high:%d, low:%d\n",block, oracle_high, oracle_low);

    return 0;
}

/** Tx 107 */
int CMPTransaction::logicMath_CloseOracle()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(contractId)) {
        PrintToLog("%s(): rejected: oracle %d does not exist\n", __func__, property);
        return (PKT_ERROR_ORACLE -11);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(contractId, sp));

    if (sender != sp.backup_address) {
        PrintToLog("%s(): rejected: sender %s is not the backup address of the Oracle Future Contract\n", __func__,sender);
        return (PKT_ERROR_ORACLE -14);
    }

    // ------------------------------------------

    sp.blocks_until_expiration = 0;

    assert(pDbSpInfo->updateSP(contractId, sp));

    PrintToLog("%s(): Oracle Contract (id:%d) Closed\n", __func__,contractId);

    return 0;
}


/** Tx 106 */
int CMPTransaction::logicMath_OracleBackup()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(contractId)) {
        PrintToLog("%s(): rejected: oracle %d does not exist\n", __func__, property);
        return (PKT_ERROR_ORACLE -11);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(contractId, sp));

    if (sender != sp.backup_address) {
        PrintToLog("%s(): rejected: sender %s is not the backup address of the Oracle Future Contract\n", __func__,sender);
        return (PKT_ERROR_ORACLE -14);
    }

    // ------------------------------------------

    sp.issuer = sender;
    sp.update_block = blockHash;

    assert(pDbSpInfo->updateSP(contractId, sp));

    return 0;
}

/** Tx 104 */
int CMPTransaction::logicMath_Change_OracleRef()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(contractId)) {
        PrintToLog("%s(): rejected: oracle contract %d does not exist\n", __func__, property);
        return (PKT_ERROR_ORACLE -11);
    }

    CMPSPInfo::Entry sp;
    assert(pDbSpInfo->getSP(contractId, sp));

    if (sender != sp.issuer) {
        PrintToLog("%s(): rejected: sender %s is not issuer of contract %d [issuer=%s]\n", __func__, sender, property, sp.issuer);
        return (PKT_ERROR_ORACLE -12);
    }

    if (receiver.empty()) {
        PrintToLog("%s(): rejected: receiver is empty\n", __func__);
        return (PKT_ERROR_ORACLE -13);
    }

    // ------------------------------------------

    sp.issuer = receiver;
    sp.update_block = blockHash;

    assert(pDbSpInfo->updateSP(contractId, sp));

    return 0;
}

/** Tx 113*/
int CMPTransaction::logicMath_Create_Channel()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }


    // ------------------------------------------

    int expiry_height = block + block_forexpiry;

    channel chn;

    chn.multisig = channel_address;
    chn.first = sender;
    chn.second = receiver;
    chn.expiry_height = expiry_height;

    if(msc_create_channel) PrintToLog("checking channel elements : channel address: %s, first address: %d, second address: %d, expiry_height: %d \n", chn.multisig, chn.first, chn.second, chn.expiry_height);

    channels_Map[channel_address] = chn;

    pDbTradeList->recordNewChannel(channel_address,sender,receiver, expiry_height, tx_idx);

    return 0;
}

/** Tx 109 */
int CMPTransaction::logicMath_Withdrawal_FromChannel()
{
    uint256 blockHash;
    {
        LOCK(cs_main);

        CBlockIndex* pindex = chainActive[block];
        if (pindex == NULL) {
            PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
            return (PKT_ERROR_TOKENS -20);
        }
        blockHash = pindex->GetBlockHash();
    }

    if (!IsTransactionTypeAllowed(block, property, type, version)) {
        PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
                __func__,
                type,
                version,
                property,
                block);
        return (PKT_ERROR_TOKENS -22);
    }

    if (!IsPropertyIdValid(propertyId)) {
        PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
        return (PKT_ERROR_TOKENS -24);
    }

    if (!pDbTradeList->checkChannelAddress(receiver)) {
        PrintToLog("%s(): rejected: address %s doesn't belong to multisig channel\n", __func__, receiver);
        return (PKT_ERROR_CHANNELS -10);
    }


    // ------------------------------------------

    //checking balance of channelAddress
    uint64_t totalAmount = static_cast<uint64_t>(GetTokenBalance(receiver, propertyId, CHANNEL_RESERVE));

    if (msc_debug_withdrawal_from_channel) PrintToLog("%s(): amount_to_withdraw : %d, totalAmount in channel: %d\n", __func__, amount_to_withdraw, totalAmount);

    if (amount_to_withdraw > totalAmount)
    {
        PrintToLog("%s(): amount to withdrawal is bigger than totalAmount on channel\n", __func__);
        return (PKT_ERROR_TOKENS -25);
    }

    uint64_t amount_remaining = pDbTradeList->getRemaining(receiver, sender, propertyId);

    if (msc_debug_withdrawal_from_channel) PrintToLog("all the amount remaining for the receiver address : %s\n",amount_remaining);

    if (amount_to_withdraw > amount_remaining)
    {
        PrintToLog("%s(): amount to withdrawal is bigger than amount remaining in channel for the address %s\n", __func__, sender);
        return (PKT_ERROR_TOKENS -26);
    }

    withdrawalAccepted wthd;

    wthd.address = sender;
    wthd.deadline_block = block + 7;
    wthd.propertyId = propertyId;
    wthd.amount = amount_to_withdraw;

    if (msc_debug_withdrawal_from_channel) PrintToLog("checking wthd element : address: %s, deadline: %d, propertyId: %d, amount: %d \n", wthd.address, wthd.deadline_block, wthd.propertyId, wthd.amount);

    withdrawal_Map[receiver].push_back(wthd);

    pDbTradeList->recordNewWithdrawal(txid, receiver, sender, propertyId, amount_to_withdraw, block, tx_idx);

    return 0;
}

/** Tx 110 */
int CMPTransaction::logicMath_Instant_Trade()
{
  int rc = 0;

  if (!IsTransactionTypeAllowed(block, property, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_METADEX -22);
  }

  if (property == desired_property) {
      PrintToLog("%s(): rejected: property for sale %d and desired property %d must not be equal\n",
              __func__,
              property,
              desired_property);
      return (PKT_ERROR_CHANNELS -11);
  }

  if (isTestEcosystemProperty(property) != isTestEcosystemProperty(desired_property)) {
      PrintToLog("%s(): rejected: property for sale %d and desired property %d not in same ecosystem\n",
              __func__,
              property,
              desired_property);
      return (PKT_ERROR_CHANNELS -12);
  }

  if (!IsPropertyIdValid(property)) {
      PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
      return (PKT_ERROR_CHANNELS -13);
  }

  if (!IsPropertyIdValid(desired_property)) {
      PrintToLog("%s(): rejected: desired property %d does not exist\n", __func__, desired_property);
      return (PKT_ERROR_CHANNELS -14);
  }

  channel chnAddrs = pDbTradeList->getChannelAddresses(sender);

  if (sender.empty() || chnAddrs.first.empty() || chnAddrs.second.empty()) {
      PrintToLog("%s(): rejected: some address doesn't belong to multisig channel \n", __func__);
      return (PKT_ERROR_CHANNELS -15);
  }

  if (chnAddrs.expiry_height < block) {
      PrintToLog("%s(): rejected: out of channel deadline: actual block: %d, deadline: %d\n", __func__, block, chnAddrs.expiry_height);
      return (PKT_ERROR_CHANNELS -16);
  }

  int64_t nBalance = GetTokenBalance(sender, property, CHANNEL_RESERVE);
  if (property > 0 && nBalance < (int64_t) amount_forsale) {
      PrintToLog("%s(): rejected: channel address %s has insufficient balance of property %d [%s < %s]\n",
              __func__,
              sender,
              property,
              FormatMP(property, nBalance),
              FormatMP(property, amount_forsale));
      return (PKT_ERROR_CHANNELS -17);
  }

  nBalance = GetTokenBalance(sender, desired_property, CHANNEL_RESERVE);
  if (desired_property > 0 && nBalance < (int64_t) desired_value) {
      PrintToLog("%s(): rejected: channel address %s has insufficient balance of property %d [%s < %s]\n",
              __func__,
              sender,
              desired_property,
              FormatMP(desired_property, nBalance),
              FormatMP(desired_property, desired_value));
      return (PKT_ERROR_CHANNELS -17);
  }

  // ------------------------------------------

  // if property = 0 ; we are exchanging litecoins
  // if (false)
  if (property > 0 && desired_property > 0)
  {
      assert(update_tally_map(chnAddrs.second, property, amount_forsale, BALANCE));
      assert(update_tally_map(sender, property, -amount_forsale, CHANNEL_RESERVE));
      assert(update_tally_map(chnAddrs.first, desired_property, desired_value, BALANCE));
      assert(update_tally_map(sender, desired_property, -desired_value, CHANNEL_RESERVE));

      pDbTradeList->recordNewInstantTrade(txid, sender, chnAddrs.first, property, amount_forsale, desired_property, desired_value, block, tx_idx);

      // NOTE: require discount for address and tokens (to consider commits and withdrawals too)

      // updating last exchange block
      std::map<std::string,channel>::iterator it = channels_Map.find(sender);
      channel& chn = it->second;

      int difference = block - chn.last_exchange_block;

      if (msc_debug_instant_trade) PrintToLog("expiry height after update: %d\n",chn.expiry_height);

      if (difference < dayblocks)
      {
          // updating expiry_height
          chn.expiry_height += difference;

      }


  } else {

      assert(update_tally_map(chnAddrs.first, desired_property, desired_value, BALANCE));
      assert(update_tally_map(sender, desired_property, -desired_value, CHANNEL_RESERVE));
      rc = 1;
      if(msc_debug_instant_trade) PrintToLog("Trading litecoins vs tokens\n");

  }

  return rc;
}

/** Tx 114 */
int CMPTransaction::logicMath_Contract_Instant()
{
  int rc = 0;


  if (!IsTransactionTypeAllowed(block, property, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_METADEX -22);
  }


  if (!IsPropertyIdValid(property)) {
      PrintToLog("%s(): rejected: property for sale %d does not exist\n", __func__, property);
      return (PKT_ERROR_CHANNELS -13);
  }

  if (!IsPropertyIdValid(desired_property)) {
      PrintToLog("%s(): rejected: desired property %d does not exist\n", __func__, desired_property);
      return (PKT_ERROR_CHANNELS -14);
  }

  channel chnAddrs = pDbTradeList->getChannelAddresses(sender);

  if (sender.empty() || chnAddrs.first.empty() || chnAddrs.second.empty()) {
      PrintToLog("%s(): rejected: some address doesn't belong to multisig channel\n", __func__);
      return (PKT_ERROR_CHANNELS -15);
  }

  if (chnAddrs.expiry_height < block) {
      PrintToLog("%s(): rejected: out of channel deadline: actual block: %d, deadline: %d\n", __func__, block, chnAddrs.expiry_height);
      return (PKT_ERROR_CHANNELS -16);
  }

  CMPSPInfo::Entry sp;
  if (!pDbSpInfo->getSP(property, sp))
      return (PKT_ERROR_CHANNELS -13);


  if (block > sp.init_block + static_cast<int>(sp.blocks_until_expiration) || block < sp.init_block)
  {
      int initblock = sp.init_block ;
      int deadline = initblock + static_cast<int>(sp.blocks_until_expiration);
      PrintToLog("\nTrade out of deadline!!: actual block: %d, deadline: %d\n",initblock,deadline);
      return (PKT_ERROR_CHANNELS -16);
  }

  int result;

  (sp.prop_type == ALL_PROPERTY_TYPE_CONTRACT) ? result = 5 : result = 6;

  if(!pDbTradeList->checkKYCRegister(sender,result))
  {
      PrintToLog("%s: tx disable from kyc register!\n",__func__);
      return (PKT_ERROR_KYC -10);
  }

  uint32_t colateralh = sp.collateral_currency;
  int64_t marginRe = static_cast<int64_t>(sp.margin_requirement);
  int64_t nBalance = GetTokenBalance(sender, colateralh, CHANNEL_RESERVE);


  rational_t conv = rational_t(1,1);
  int64_t num = conv.numerator().convert_to<int64_t>();
  int64_t den = conv.denominator().convert_to<int64_t>();
  arith_uint256 amountTR = (ConvertTo256(instant_amount)*ConvertTo256(marginRe)*ConvertTo256(num))/(ConvertTo256(den)*ConvertTo256(ileverage));
  int64_t amountToReserve = ConvertTo64(amountTR);

  if(msc_debug_contract_instant_trade) PrintToLog("%s: AmountToReserve: %d, channel Balance: %d\n", __func__, amountToReserve,nBalance);

  //fees
  if(!mastercore::ContInst_Fees(chnAddrs.first, chnAddrs.second, chnAddrs.multisig, amountToReserve, sp.prop_type, sp.collateral_currency))
  {
      PrintToLog("\n %s: no enogh money to pay fees\n", __func__);
      return (PKT_ERROR_CHANNELS -18);
  }

  if (nBalance < (2 * amountToReserve) || nBalance == 0)
  {
      PrintToLog("%s(): rejected: sender %s has insufficient balance for contracts %d [%s < %s] \n",
      __func__,
      sender,
      colateralh,
      FormatMP(colateralh, nBalance),
      FormatMP(colateralh, amountToReserve));
      return (PKT_ERROR_CHANNELS -17);
  }
  else {

      if (amountToReserve > 0)
       {
           assert(update_tally_map(sender, colateralh, -amountToReserve, CHANNEL_RESERVE));
           assert(update_tally_map(chnAddrs.first, colateralh, ConvertTo64(amountTR), CONTRACTDEX_MARGIN));
           assert(update_tally_map(chnAddrs.second, colateralh, ConvertTo64(amountTR), CONTRACTDEX_MARGIN));
       }
  }

   /*********************************************/
   /**Logic for Node Reward**/

   // const CConsensusParams &params = ConsensusParams();
   // int BlockInit = params.MSC_NODE_REWARD;
   // int nBlockNow = GetHeight();
   //
   // BlockClass NodeRewardObj(BlockInit, nBlockNow);
   // NodeRewardObj.SendNodeReward(sender);

   /********************************************************/

   // updating last exchange block
   std::map<std::string,channel>::iterator it = channels_Map.find(sender);
   channel& chn = it->second;

   int difference = block - chn.last_exchange_block;

   if(msc_debug_contract_instant_trade) PrintToLog("%s: expiry height after update: %d\n",__func__, chn.expiry_height);

   if (difference < dayblocks)
   {
       // updating expiry_height
       chn.expiry_height += difference;

   }

   mastercore::Instant_x_Trade(txid, itrading_action, chnAddrs.multisig, chnAddrs.first, chnAddrs.second, property, instant_amount, price, block, tx_idx);

   // t_tradelistdb->recordNewInstContTrade(txid, receiver, sender, propertyId, amount_commited, block, tx_idx);
   // NOTE: add discount from channel of fees + amountToReserve

   if (msc_debug_contract_instant_trade)PrintToLog("%s: End of Logic Instant Contract Trade\n\n",__func__);


   return rc;
}

/** Tx 115 */
int CMPTransaction::logicMath_New_Id_Registration()
{
  uint256 blockHash;
  {
      LOCK(cs_main);

      CBlockIndex* pindex = chainActive[block];
      if (pindex == NULL) {
          PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
          return (PKT_ERROR_TOKENS -20);
      }
      blockHash = pindex->GetBlockHash();
  }

  if (!IsTransactionTypeAllowed(block, property, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_TOKENS -22);
  }

  // ---------------------------------------
  if (msc_debug_new_id_registration) PrintToLog("%s(): channelAddres in register: %s \n",__func__,receiver);

  pDbTradeList->recordNewIdRegister(txid, receiver, website, company_name, tokens, ltc, natives, oracles, block, tx_idx);

  // std::string dummy = "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P";
  // t_tradelistdb->updateIdRegister(txid,sender, dummy,block, tx_idx);
  return 0;
}

/** Tx 116 */
int CMPTransaction::logicMath_Update_Id_Registration()
{
  uint256 blockHash;
  {
      LOCK(cs_main);

      CBlockIndex* pindex = chainActive[block];
      if (pindex == NULL) {
          PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
          return (PKT_ERROR_TOKENS -20);
      }
      blockHash = pindex->GetBlockHash();
  }

  if (!IsTransactionTypeAllowed(block, property, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_TOKENS -22);
  }

  // ---------------------------------------

  pDbTradeList->updateIdRegister(txid,sender, receiver,block, tx_idx);

  return 0;
}

/** Tx 112 */
int CMPTransaction::logicMath_Transfer()
{
  uint256 blockHash;
  {
      LOCK(cs_main);

      CBlockIndex* pindex = chainActive[block];
      if (pindex == NULL) {
          PrintToLog("%s(): ERROR: block %d not in the active chain\n", __func__, block);
          return (PKT_ERROR_TOKENS -20);
      }
      blockHash = pindex->GetBlockHash();
  }

  if (!IsTransactionTypeAllowed(block, property, type, version)) {
      PrintToLog("%s(): rejected: type %d or version %d not permitted for property %d at block %d\n",
              __func__,
              type,
              version,
              property,
              block);
      return (PKT_ERROR_TOKENS -22);
  }

  if (!IsPropertyIdValid(propertyId)) {
      PrintToLog("%s(): rejected: property %d does not exist\n", __func__, property);
      return (PKT_ERROR_CHANNELS -13);
  }


  // ------------------------------------------


  // TRANSFER logic here

  assert(update_tally_map(sender, propertyId, -amount, CHANNEL_RESERVE));
  assert(update_tally_map(receiver, propertyId, amount, CHANNEL_RESERVE));

  // recordNewTransfer
  pDbTradeList->recordNewTransfer(txid, sender,receiver, propertyId, amount, block, tx_idx);

  return 0;

}

struct FutureContractObject *getFutureContractObject(uint32_t property_type, std::string identifier)
{
  struct FutureContractObject *pt_fco = new FutureContractObject;

  LOCK(cs_tally);
  uint32_t nextSPID = pDbSpInfo->peekNextSPID(1);
  for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
    {
      CMPSPInfo::Entry sp;
      if (pDbSpInfo->getSP(propertyId, sp))
	{
	  if ( (sp.prop_type == ALL_PROPERTY_TYPE_CONTRACT || sp.prop_type == ALL_PROPERTY_TYPE_ORACLE_CONTRACT) && sp.name == identifier )
	    {
	      pt_fco->fco_denomination = sp.denomination;
	      pt_fco->fco_blocks_until_expiration = sp.blocks_until_expiration;
	      pt_fco->fco_notional_size = sp.notional_size;
	      pt_fco->fco_collateral_currency = sp.collateral_currency;
	      pt_fco->fco_margin_requirement = sp.margin_requirement;
	      pt_fco->fco_name = sp.name;
	      pt_fco->fco_subcategory = sp.subcategory;
	      pt_fco->fco_issuer = sp.issuer;
	      pt_fco->fco_init_block = sp.init_block;
        pt_fco->fco_backup_address = sp.backup_address;
	      pt_fco->fco_propertyId = propertyId;
        pt_fco->fco_prop_type = sp.prop_type;
	    }
	  else if ( sp.prop_type == ALL_PROPERTY_TYPE_PEGGEDS && sp.name == identifier )
	    {
	      pt_fco->fco_denomination = sp.denomination;
	      pt_fco->fco_blocks_until_expiration = sp.blocks_until_expiration;
	      pt_fco->fco_notional_size = sp.notional_size;
	      pt_fco->fco_collateral_currency = sp.collateral_currency;
	      pt_fco->fco_margin_requirement = sp.margin_requirement;
	      pt_fco->fco_name = sp.name;
	      pt_fco->fco_subcategory = sp.subcategory;
	      pt_fco->fco_issuer = sp.issuer;
	      pt_fco->fco_init_block = sp.init_block;
	      pt_fco->fco_propertyId = propertyId;
	    }
	}
    }
  return pt_fco;
}

struct TokenDataByName *getTokenDataByName(std::string identifier)
{
  struct TokenDataByName *pt_data = new TokenDataByName;

  LOCK(cs_tally);
  uint32_t nextSPID = pDbSpInfo->peekNextSPID(1);
  for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++)
    {
      CMPSPInfo::Entry sp;
      if (pDbSpInfo->getSP(propertyId, sp) && sp.name == identifier)
	{
	  pt_data->data_denomination = sp.denomination;
	  pt_data->data_blocks_until_expiration = sp.blocks_until_expiration;
	  pt_data->data_notional_size = sp.notional_size;
	  pt_data->data_collateral_currency = sp.collateral_currency;
	  pt_data->data_margin_requirement = sp.margin_requirement;
	  pt_data->data_name = sp.name;
	  pt_data->data_subcategory = sp.subcategory;
	  pt_data->data_issuer = sp.issuer;
	  pt_data->data_init_block = sp.init_block;
	  pt_data->data_propertyId = propertyId;
	}
    }
  return pt_data;
}
