// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/client.h>
#include <rpc/protocol.h>
#include <util/system.h>

#include <set>
#include <stdint.h>

class CRPCConvertParam
{
public:
    std::string methodName; //!< method whose params want conversion
    int paramIdx;           //!< 0-based idx of param to convert
    std::string paramName;  //!< parameter name
};

// clang-format off
/**
 * Specify a (method, idx, name) here if the argument is a non-string RPC
 * argument and needs to be converted from JSON.
 *
 * @note Parameter indexes start from 0.
 */
static const CRPCConvertParam vRPCConvertParams[] =
{
    { "setmocktime", 0, "timestamp" },
    { "generate", 0, "nblocks" },
    { "generate", 1, "maxtries" },
    { "generatetoaddress", 0, "nblocks" },
    { "generatetoaddress", 2, "maxtries" },
    { "getnetworkhashps", 0, "nblocks" },
    { "getnetworkhashps", 1, "height" },
    { "sendtoaddress", 1, "amounttosend" },
    { "sendtoaddress", 4, "subtractfeefromamount" },
    { "sendtoaddress", 5 , "replaceable" },
    { "sendtoaddress", 6 , "conf_target" },
    { "settxfee", 0, "feeamount" },
    { "sethdseed", 0, "newkeypool" },
    { "getreceivedbyaddress", 1, "minconf" },
    { "getreceivedbylabel", 1, "minconf" },
    { "listreceivedbyaddress", 0, "minconf" },
    { "listreceivedbyaddress", 1, "include_empty" },
    { "listreceivedbyaddress", 2, "include_watchonly" },
    { "listreceivedbylabel", 0, "minconf" },
    { "listreceivedbylabel", 1, "include_empty" },
    { "listreceivedbylabel", 2, "include_watchonly" },
    { "getbalance", 1, "minconf" },
    { "getbalance", 2, "include_watchonly" },
    { "getblockhash", 0, "height" },
    { "waitforblockheight", 0, "height" },
    { "waitforblockheight", 1, "timeout" },
    { "waitforblock", 1, "timeout" },
    { "waitfornewblock", 0, "timeout" },
    { "listtransactions", 1, "count" },
    { "listtransactions", 2, "skip" },
    { "listtransactions", 3, "include_watchonly" },
    { "walletpassphrase", 1, "timeout" },
    { "getblocktemplate", 0, "template_request" },
    { "listsinceblock", 1, "target_confirmations" },
    { "listsinceblock", 2, "include_watchonly" },
    { "listsinceblock", 3, "include_removed" },
    { "sendmany", 1, "amounts" },
    { "sendmany", 2, "minconf" },
    { "sendmany", 4, "subtractfeefrom" },
    { "sendmany", 5 , "replaceable" },
    { "sendmany", 6 , "conf_target" },
    { "deriveaddresses", 1, "range" },
    { "scantxoutset", 1, "scanobjects" },
    { "addmultisigaddress", 0, "nrequired" },
    { "addmultisigaddress", 1, "keys" },
    { "createmultisig", 0, "nrequired" },
    { "createmultisig", 1, "keys" },
    { "listunspent", 0, "minconf" },
    { "listunspent", 1, "maxconf" },
    { "listunspent", 2, "addresses" },
    { "listunspent", 3, "include_unsafe" },
    { "listunspent", 4, "query_options" },
    { "getblock", 1, "verbosity" },
    { "getblock", 1, "verbose" },
    { "getblockheader", 1, "verbose" },
    { "getchaintxstats", 0, "nblocks" },
    { "gettransaction", 1, "include_watchonly" },
    { "getrawtransaction", 1, "verbose" },
    { "createrawtransaction", 0, "inputs" },
    { "createrawtransaction", 1, "outputs" },
    { "createrawtransaction", 2, "locktime" },
    { "createrawtransaction", 3, "replaceable" },
    { "decoderawtransaction", 1, "iswitness" },
    { "signrawtransaction", 1, "prevtxs" },
    { "signrawtransaction", 2, "privkeys" },
    { "signrawtransactionwithkey", 1, "privkeys" },
    { "signrawtransactionwithkey", 2, "prevtxs" },
    { "signrawtransactionwithwallet", 1, "prevtxs" },
    { "sendrawtransaction", 1, "allowhighfees" },
    { "testmempoolaccept", 0, "rawtxs" },
    { "testmempoolaccept", 1, "allowhighfees" },
    { "combinerawtransaction", 0, "txs" },
    { "fundrawtransaction", 1, "options" },
    { "fundrawtransaction", 2, "iswitness" },
    { "walletcreatefundedpsbt", 0, "inputs" },
    { "walletcreatefundedpsbt", 1, "outputs" },
    { "walletcreatefundedpsbt", 2, "locktime" },
    { "walletcreatefundedpsbt", 3, "options" },
    { "walletcreatefundedpsbt", 4, "bip32derivs" },
    { "walletprocesspsbt", 1, "sign" },
    { "walletprocesspsbt", 3, "bip32derivs" },
    { "createpsbt", 0, "inputs" },
    { "createpsbt", 1, "outputs" },
    { "createpsbt", 2, "locktime" },
    { "createpsbt", 3, "replaceable" },
    { "combinepsbt", 0, "txs"},
    { "joinpsbts", 0, "txs"},
    { "finalizepsbt", 1, "extract"},
    { "converttopsbt", 1, "permitsigdata"},
    { "converttopsbt", 2, "iswitness"},
    { "gettxout", 1, "n" },
    { "gettxout", 2, "include_mempool" },
    { "gettxoutproof", 0, "txids" },
    { "lockunspent", 0, "unlock" },
    { "lockunspent", 1, "transactions" },
    { "importprivkey", 2, "rescan" },
    { "importaddress", 2, "rescan" },
    { "importaddress", 3, "p2sh" },
    { "importpubkey", 2, "rescan" },
    { "importmulti", 0, "requests" },
    { "importmulti", 1, "options" },
    { "verifychain", 0, "checklevel" },
    { "verifychain", 1, "nblocks" },
    { "getblockstats", 0, "hash_or_height" },
    { "getblockstats", 1, "stats" },
    { "pruneblockchain", 0, "height" },
    { "keypoolrefill", 0, "newsize" },
    { "getrawmempool", 0, "verbose" },
    { "estimatesmartfee", 0, "conf_target" },
    { "estimaterawfee", 0, "conf_target" },
    { "estimaterawfee", 1, "threshold" },
    { "prioritisetransaction", 1, "dummy" },
    { "prioritisetransaction", 2, "fee_delta" },
    { "setban", 2, "bantime" },
    { "setban", 3, "absolute" },
    { "setnetworkactive", 0, "state" },
    { "getmempoolancestors", 1, "verbose" },
    { "getmempooldescendants", 1, "verbose" },
    { "bumpfee", 1, "options" },
    { "logging", 0, "include" },
    { "logging", 1, "exclude" },
    { "disconnectnode", 1, "nodeid" },
    // Echo with conversion (For testing only)
    { "echojson", 0, "arg0" },
    { "echojson", 1, "arg1" },
    { "echojson", 2, "arg2" },
    { "echojson", 3, "arg3" },
    { "echojson", 4, "arg4" },
    { "echojson", 5, "arg5" },
    { "echojson", 6, "arg6" },
    { "echojson", 7, "arg7" },
    { "echojson", 8, "arg8" },
    { "echojson", 9, "arg9" },
    { "rescanblockchain", 0, "start_height"},
    { "rescanblockchain", 1, "stop_height"},
    { "createwallet", 1, "disable_private_keys"},
    { "createwallet", 2, "blank"},
    { "getnodeaddresses", 0, "count"},
    { "stop", 0, "wait" },


    /* Trade Layer - data retrieval calls */
    { "tl_gettradehistoryforaddress", 1 , "count"},
    { "tl_gettradehistoryforaddress", 2, "propertyid" },
    { "tl_gettradehistoryforpair", 0, "propertyid" },
    { "tl_gettradehistoryforpair", 1, "propertyidsecond" },
    { "tl_gettradehistoryforpair", 2, "count" },
    { "tl_setautocommit", 0, "flag" },
    { "tl_getgrants", 0, "propertyid" },
    { "tl_getbalance", 1, "propertyid" },
    { "tl_getproperty", 0, "propertyid" },
    { "tl_listtransactions", 1, "count" },
    { "tl_listtransactions", 2, "skip" },
    { "tl_listtransactions", 3, "startblock" },
    { "tl_listtransactions", 4, "endblock" },
    { "tl_getallbalancesforid", 0, "propertyid" },
    { "tl_listblocktransactions", 0, "index" },
    { "tl_listblockstransactions", 0, "firstblock" },
    { "tl_listblockstransactions", 1, "lastblock" },
    { "tl_getorderbook", 0, "propertyid" },
    { "tl_getorderbook", 1, "propertyid" },
    { "tl_getseedblocks", 0, "startblock" },
    { "tl_getseedblocks", 1, "endblock" },
    { "tl_getmetadexhash", 0, "propertyid" },
    { "tl_getfeecache", 0, "propertyid" },
    { "tl_getfeeshare", 1, "ecosystem" },
    { "tl_getfeetrigger", 0, "propertyid" },
    { "tl_getfeedistribution", 0, "distributionid" },
    { "tl_getfeedistributions", 0, "propertyid" },
    { "tl_getbalanceshash", 0, "propertyid" },
    { "tl_getwalletbalances", 0, "includewatchonly" },
    { "tl_getwalletaddressbalances", 0, "includewatchonly" },

    /* Trade Layer - transaction calls */
    { "tl_send", 2, "propertyid" },
    { "tl_sendsto", 1, "propertyid" },
    { "tl_sendsto", 4, "distributionproperty" },
    { "tl_sendall", 2, "ecosystem" },
    { "tl_sendtrade", 1, "propertyidforsale" },
    { "tl_sendtrade", 3, "propertiddesired" },
    { "tl_sendcanceltradesbyprice", 1, "propertyidforsale" },
    { "tl_sendcanceltradesbyprice", 3, "propertiddesired" },
    { "tl_sendcanceltradesbypair", 1, "propertyidforsale" },
    { "tl_sendcanceltradesbypair", 2, "propertiddesired" },
    { "tl_sendcancelalltrades", 1, "ecosystem" },
    { "tl_sendissuancefixed", 1, "ecosystem" },
    { "tl_sendissuancefixed", 2, "type" },
    { "tl_sendissuancefixed", 3, "previousid" },
    { "tl_sendissuancemanaged", 1, "ecosystem" },
    { "tl_sendissuancemanaged", 2, "type" },
    { "tl_sendissuancemanaged", 3, "previousid" },
    { "tl_senddexaccept", 2, "propertyid" },
    { "tl_senddexaccept", 4, "override" },
    { "tl_sendgrant", 2, "propertyid" },
    { "tl_sendrevoke", 1, "propertyid" },
    { "tl_sendchangeissuer", 2, "propertyid" },
    { "tl_sendenablefreezing", 1, "propertyid" },
    { "tl_senddisablefreezing", 1, "propertyid" },
    { "tl_sendfreeze", 2, "propertyid" },
    { "tl_sendunfreeze", 2, "propertyid" },
    { "tl_senddeactivation", 1, "featureid" },
    { "tl_sendactivation", 1, "featureid" },
    { "tl_sendactivation", 2, "block" },
    { "tl_sendactivation", 3, "minclientversion" },
    { "tl_sendalert", 1, "alerttype" },
    { "tl_sendalert", 2, "expiryvalue" },
    { "tl_funded_send", 2, "propertyid" },
    { "tl_funded_sendall", 2, "ecosystem" },
    { "tl_sendvesting", 2, "arg2" },
    { "tl_createcontract", 1, "arg1"},
    { "tl_createcontract", 2, "arg2"},
    { "tl_createcontract", 4, "arg4"},
    { "tl_createcontract", 5, "arg5"},
    { "tl_createcontract", 6, "arg6"},
    { "tl_createcontract", 7, "arg7"},
    { "tl_create_oraclecontract", 1, "arg1"},
    { "tl_create_oraclecontract", 2, "arg2"},
    { "tl_create_oraclecontract", 4, "arg4"},
    { "tl_create_oraclecontract", 5, "arg5"},
    { "tl_create_oraclecontract", 6, "arg6"},
    { "tl_create_oraclecontract", 7, "arg7"},
    { "tl_tradecontract", 3, "arg3"},
    { "tl_tradecontract", 4, "arg4"},
    { "tl_cancelallcontractsbyaddress", 1, "arg1"},
    { "tl_closeposition", 1, "arg1" },
    { "tl_closeposition", 2, "arg2" },
    { "tl_cancelorderbyblock", 1, "arg1"},
    { "tl_cancelorderbyblock", 2, "arg2" },
    { "tl_commit_tochannel", 2, "arg2" },
    { "tl_commit_tochannel", 4, "arg4" },
    { "tl_withdrawal_fromchannel", 2, "arg2" },
    { "tl_withdrawal_fromchannel", 4, "arg4" },
    { "tl_create_channel", 3, "arg3" },
    { "tl_new_id_registration", 4, "arg3" },
    { "tl_new_id_registration", 5, "arg4" },
    { "tl_new_id_registration", 6, "arg5" },
    { "tl_new_id_registration", 7, "arg6" },
    { "tl_senddexoffer", 1, "arg1" },
    { "tl_senddexoffer", 4, "arg4" },
    { "tl_senddexoffer", 7, "arg7" },

    /* Trade Layer - raw transaction calls */
    { "tl_decodetransaction", 1, "prevtxs" },
    { "tl_decodetransaction", 2, "height" },
    { "tl_createrawtx_input", 2, "n" },
    { "tl_createrawtx_change", 1, "prevtxs" },
    { "tl_createrawtx_change", 3, "fee" },
    { "tl_createrawtx_change", 4, "position" },

    /* Trade Layer - payload creation */
   { "tl_createpayload_sendactivation", 0, "arg0"},
   { "tl_createpayload_sendactivation", 1, "arg1"},
   { "tl_createpayload_sendactivation", 2, "arg2"},
   { "tl_createpayload_senddeactivation", 0, "arg0"},
   { "tl_createpayload_sendalert", 0, "arg0"},
   { "tl_createpayload_sendalert", 1, "arg1"},
   { "tl_createpayload_simplesend", 0, "arg0" },
   { "tl_createpayload_sendvestingtokens", 0, "arg0"},
   { "tl_createpayload_sendall", 0, "arg0" },
   { "tl_createpayload_dexsell", 0, "arg0" },
   { "tl_createpayload_dexsell", 3, "arg3" },
   { "tl_createpayload_dexsell", 5, "arg5" },
   { "tl_createpayload_dexaccept", 0, "arg0" },
   { "tl_createpayload_issuancefixed", 0, "arg0" },
   { "tl_createpayload_issuancefixed", 1, "arg1" },
   { "tl_createpayload_issuancefixed", 2, "arg2" },
   { "tl_createpayload_issuancemanaged", 0, "arg0" },
   { "tl_createpayload_issuancemanaged", 1, "arg1" },
   { "tl_createpayload_issuancemanaged", 2, "arg2" },
   { "tl_createpayload_sendgrant", 0, "arg0" },
   { "tl_createpayload_sendrevoke", 0, "arg0" },
   { "tl_createpayload_changeissuer", 0, "arg0" },
   { "tl_createpayload_sendtrade", 0, "arg0" },
   { "tl_createpayload_sendtrade", 2, "arg2" },
   { "tl_createpayload_createcontract", 0, "arg0"},
   { "tl_createpayload_createcontract", 1, "arg1"},
   { "tl_createpayload_createcontract", 3, "arg3"},
   { "tl_createpayload_createcontract", 4, "arg4"},
   { "tl_createpayload_createcontract", 5, "arg5"},
   { "tl_createpayload_tradecontract", 2, "arg2"},
   { "tl_createpayload_tradecontract", 3, "arg3"},
   { "tl_createpayload_cancelallcontractsbyaddress", 0, "arg0"},
   { "tl_createpayload_closeposition", 0, "arg0" },
   { "tl_createpayload_closeposition", 1, "arg1" },
   { "tl_createpayload_sendissuance_pegged", 0, "arg0" },
   { "tl_createpayload_sendissuance_pegged", 1, "arg1" },
   { "tl_createpayload_sendissuance_pegged", 2, "arg2" },
   { "tl_createpayload_sendissuance_pegged", 4, "arg4" },
   { "tl_createpayload_cancelorderbyblock", 0, "arg0"},
   { "tl_createpayload_cancelorderbyblock", 1, "arg1" },
   { "tl_createpayload_dexoffer", 0, "arg0" },
   { "tl_createpayload_dexoffer", 3, "arg3" },
   { "tl_createpayload_dexoffer", 6, "arg6" },
   { "tl_createpayload_senddexaccept", 1, "arg1" },
   { "tl_createpayload_sendvesting", 0, "arg0"},
   { "tl_createpayload_instant_trade", 0, "arg0"},
   { "tl_createpayload_instant_trade", 2, "arg2"},
   { "tl_createpayload_instant_trade", 3, "arg3"},

   { "tl_createpayload_contract_instant_trade", 0, "arg0"},
   { "tl_createpayload_contract_instant_trade", 2, "arg2"},
   { "tl_createpayload_contract_instant_trade", 4, "arg4"},
   // { "tl_createpayload_contract_instant_trade", 5, "arg5"},


   { "tl_createpayload_pnl_update", 0, "arg0"},
   { "tl_createpayload_pnl_update", 2, "arg2"},
   { "tl_createpayload_pnl_update", 3, "arg3"},
   { "tl_createpayload_pnl_update", 4, "arg4"},

   /* Trade Layer - raw transaction calls */
   { "tl_decodetransaction", 1, "arg1" },
   { "tl_decodetransaction", 2, "arg2" },
   { "tl_createrawtx_reference", 2, "arg2" },
   { "tl_createrawtx_input", 2, "arg2" },
   { "tl_createrawtx_change", 1, "arg1" },
   { "tl_createrawtx_change", 3, "arg3" },
   { "tl_createrawtx_change", 4, "arg4" },


    /* tl Core - payload creation */
    { "tl_createpayload_simplesend", 0, "propertyid" },
    { "tl_createpayload_sendall", 0, "ecosystem" },
    { "tl_createpayload_dexsell", 0, "propertyidforsale" },
    { "tl_createpayload_dexsell", 3, "paymentwindow" },
    { "tl_createpayload_dexsell", 5, "action" },
    { "tl_createpayload_dexaccept", 0, "propertyid" },
    { "tl_createpayload_sto", 0, "propertyid" },
    { "tl_createpayload_sto", 2, "distributionproperty" },
    { "tl_createpayload_issuancefixed", 0, "ecosystem" },
    { "tl_createpayload_issuancefixed", 1, "type" },
    { "tl_createpayload_issuancefixed", 2, "previousid" },
    { "tl_createpayload_issuancemanaged", 0, "ecosystem" },
    { "tl_createpayload_issuancemanaged", 1, "type" },
    { "tl_createpayload_issuancemanaged", 2, "previousid" },
    { "tl_createpayload_grant", 0, "propertyid" },
    { "tl_createpayload_revoke", 0, "propertyid" },
    { "tl_createpayload_changeissuer", 0, "propertyid" },
    { "tl_createpayload_trade", 0, "propertyidforsale" },
    { "tl_createpayload_trade", 2, "propertiddesired" },
    { "tl_createpayload_canceltradesbyprice", 0, "propertyidforsale" },
    { "tl_createpayload_canceltradesbyprice", 2, "propertiddesired" },
    { "tl_createpayload_canceltradesbypair", 0, "propertyidforsale" },
    { "tl_createpayload_canceltradesbypair", 1, "propertiddesired" },
    { "tl_createpayload_cancelalltrades", 0, "ecosystem" },
    { "tl_createpayload_enablefreezing", 0, "propertyid" },
    { "tl_createpayload_disablefreezing", 0, "propertyid" },
    { "tl_createpayload_freeze", 1, "propertyid" },
    { "tl_createpayload_unfreeze", 1, "propertyid" },

    /* Trade Layer - backwards compatibility */
    { "getgrants_MP", 0, "propertyid" },
    { "send_MP", 2, "propertyid" },
    { "getbalance_MP", 1, "propertyid" },
    { "sendtoowners_MP", 1, "propertyid" },
    { "sendtoowners_MP", 4, "distributionproperty" },
    { "getproperty_MP", 0, "propertyid" },
    { "listtransactions_MP", 1, "count" },
    { "listtransactions_MP", 2, "skip" },
    { "listtransactions_MP", 3, "startblock" },
    { "listtransactions_MP", 4, "endblock" },
    { "getallbalancesforid_MP", 0, "propertyid" },
    { "listblocktransactions_MP", 0, "index" },
    { "getorderbook_MP", 0, "propertyid" },
    { "getorderbook_MP", 1, "propertyiddesired" },
    { "trade_MP", 1, "propertyidforsale" }, // deprecated
    { "trade_MP", 3, "propertiddesired" }, // deprecated
    { "trade_MP", 5, "action" }, // deprecated
};
// clang-format on

class CRPCConvertTable
{
private:
    std::set<std::pair<std::string, int>> members;
    std::set<std::pair<std::string, std::string>> membersByName;

public:
    CRPCConvertTable();

    bool convert(const std::string& method, int idx) {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
    bool convert(const std::string& method, const std::string& name) {
        return (membersByName.count(std::make_pair(method, name)) > 0);
    }
};

CRPCConvertTable::CRPCConvertTable()
{
    const unsigned int n_elem =
        (sizeof(vRPCConvertParams) / sizeof(vRPCConvertParams[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                      vRPCConvertParams[i].paramIdx));
        membersByName.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                            vRPCConvertParams[i].paramName));
    }
}

static CRPCConvertTable rpcCvtTable;

/** Non-RFC4627 JSON parser, accepts internal values (such as numbers, true, false, null)
 * as well as objects and arrays.
 */
UniValue ParseNonRFCJSONValue(const std::string& strVal)
{
    UniValue jVal;
    if (!jVal.read(std::string("[")+strVal+std::string("]")) ||
        !jVal.isArray() || jVal.size()!=1)
        throw std::runtime_error(std::string("Error parsing JSON:")+strVal);
    return jVal[0];
}

UniValue RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    UniValue params(UniValue::VARR);

    for (unsigned int idx = 0; idx < strParams.size(); idx++) {
        const std::string& strVal = strParams[idx];

        if (!rpcCvtTable.convert(strMethod, idx)) {
            // insert string value directly
            params.push_back(strVal);
        } else {
            // parse string as JSON, insert bool/number/object/etc. value
            params.push_back(ParseNonRFCJSONValue(strVal));
        }
    }

    return params;
}

UniValue RPCConvertNamedValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    UniValue params(UniValue::VOBJ);

    for (const std::string &s: strParams) {
        size_t pos = s.find('=');
        if (pos == std::string::npos) {
            throw(std::runtime_error("No '=' in named argument '"+s+"', this needs to be present for every argument (even if it is empty)"));
        }

        std::string name = s.substr(0, pos);
        std::string value = s.substr(pos+1);

        if (!rpcCvtTable.convert(strMethod, name)) {
            // insert string value directly
            params.pushKV(name, value);
        } else {
            // parse string as JSON, insert bool/number/object/etc. value
            params.pushKV(name, ParseNonRFCJSONValue(value));
        }
    }

    return params;
}
