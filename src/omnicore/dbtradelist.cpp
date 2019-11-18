#include <omnicore/dbtradelist.h>
#include <omnicore/uint256_extensions.h>
#include <omnicore/sp.h>

#include <omnicore/log.h>
#include <omnicore/mdex.h>
#include <omnicore/sp.h>
#include <omnicore/omnicore.h>

#include <amount.h>
#include <fs.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <tinyformat.h>

#include <univalue.h>

#include <leveldb/iterator.h>
#include <leveldb/slice.h>
#include <leveldb/status.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <stddef.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

using mastercore::isPropertyDivisible;
using leveldb::Status;

extern double globalPNLALL_DUSD;
extern int64_t globalVolumeALL_DUSD;
extern volatile int64_t globalVolumeALL_LTC;
extern std::vector<std::map<std::string, std::string>> path_elef;

/** TWAP containers **/
extern std::map<uint32_t, std::vector<uint64_t>> cdextwap_ele;
extern std::map<uint32_t, std::vector<uint64_t>> cdextwap_vec;


CMPTradeList::CMPTradeList(const fs::path& path, bool fWipe)
{
    leveldb::Status status = Open(path, fWipe);
    PrintToConsole("Loading trades database: %s\n", status.ToString());
}

CMPTradeList::~CMPTradeList()
{
    if (msc_debug_persistence) PrintToLog("CMPTradeList closed\n");
}

void CMPTradeList::recordMatchedTrade(const uint256& txid1, const uint256& txid2, const std::string& address1, const std::string& address2, uint32_t prop1, uint32_t prop2, int64_t amount1, int64_t amount2, int blockNum, int64_t fee)
{
    if (!pdb) return;
    const std::string key = txid1.ToString() + "+" + txid2.ToString();
    const std::string value = strprintf("%s:%s:%u:%u:%lu:%lu:%d:%d", address1, address2, prop1, prop2, amount1, amount2, blockNum, fee);
    leveldb::Status status = pdb->Put(writeoptions, key, value);
    ++nWritten;
    if (msc_debug_tradedb) PrintToLog("%s: %s\n", __func__, status.ToString());
}

void CMPTradeList::recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex)
{
    if (!pdb) return;
    std::string strValue = strprintf("%s:%d:%d:%d:%d", address, propertyIdForSale, propertyIdDesired, blockNum, blockIndex);
    leveldb::Status status = pdb->Put(writeoptions, txid.ToString(), strValue);
    ++nWritten;
    if (msc_debug_tradedb) PrintToLog("%s: %s\n", __func__, status.ToString());
}

/**
 * This function deletes records of trades above/equal to a specific block from the trade database.
 *
 * Returns the number of records changed.
 */
int CMPTradeList::deleteAboveBlock(int blockNum)
{
    leveldb::Slice skey, svalue;
    unsigned int count = 0;
    std::vector<std::string> vstr;
    int block = 0;
    unsigned int n_found = 0;
    leveldb::Iterator* it = NewIterator();
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        skey = it->key();
        svalue = it->value();
        ++count;
        std::string strvalue = it->value().ToString();
        boost::split(vstr, strvalue, boost::is_any_of(":"), boost::token_compress_on);
        if (7 == vstr.size()) block = atoi(vstr[6]); // trade matches have 7 tokens, key is txid+txid, only care about block
        if (5 == vstr.size()) block = atoi(vstr[3]); // trades have 5 tokens, key is txid, only care about block
        if (block >= blockNum) {
            ++n_found;
            PrintToLog("%s() DELETING FROM TRADEDB: %s=%s\n", __func__, skey.ToString(), svalue.ToString());
            pdb->Delete(writeoptions, skey);
        }
    }

    delete it;

    PrintToLog("%s(%d); tradedb n_found= %d\n", __func__, blockNum, n_found);

    return n_found;
}

void CMPTradeList::printStats()
{
    PrintToLog("CMPTradeList stats: tWritten= %d , tRead= %d\n", nWritten, nRead);
}

void CMPTradeList::printAll()
{
    int count = 0;
    leveldb::Slice skey, svalue;
    leveldb::Iterator* it = NewIterator();

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        skey = it->key();
        svalue = it->value();
        ++count;
        PrintToConsole("entry #%8d= %s:%s\n", count, skey.ToString(), svalue.ToString());
    }

    delete it;
}

bool CMPTradeList::getMatchingTrades(const uint256& txid, uint32_t propertyId, UniValue& tradeArray, int64_t& totalSold, int64_t& totalReceived)
{
    if (!pdb) return false;

    int count = 0;
    totalReceived = 0;
    totalSold = 0;

    std::vector<std::string> vstr;
    std::string txidStr = txid.ToString();
    leveldb::Iterator* it = NewIterator();
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        // search key to see if this is a matching trade
        std::string strKey = it->key().ToString();
        std::string strValue = it->value().ToString();
        std::string matchTxid;
        size_t txidMatch = strKey.find(txidStr);
        if (txidMatch == std::string::npos) continue; // no match

        // sanity check key is the correct length for a matched trade
        if (strKey.length() != 129) continue;

        // obtain the txid of the match
        if (txidMatch == 0) {
            matchTxid = strKey.substr(65, 64);
        } else {
            matchTxid = strKey.substr(0, 64);
        }

        // ensure correct amount of tokens in value std::string
        boost::split(vstr, strValue, boost::is_any_of(":"), boost::token_compress_on);
        if (vstr.size() != 8) {
            PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }

        // decode the details from the value string
        std::string address1 = vstr[0];
        std::string address2 = vstr[1];
        uint32_t prop1 = boost::lexical_cast<uint32_t>(vstr[2]);
        uint32_t prop2 = boost::lexical_cast<uint32_t>(vstr[3]);
        int64_t amount1 = boost::lexical_cast<int64_t>(vstr[4]);
        int64_t amount2 = boost::lexical_cast<int64_t>(vstr[5]);
        int blockNum = atoi(vstr[6]);
        int64_t tradingFee = boost::lexical_cast<int64_t>(vstr[7]);

        std::string strAmount1 = FormatMP(prop1, amount1);
        std::string strAmount2 = FormatMP(prop2, amount2);
        std::string strTradingFee = FormatMP(prop2, tradingFee);
        std::string strAmount2PlusFee = FormatMP(prop2, amount2 + tradingFee);

        // populate trade object and add to the trade array, correcting for orientation of trade
        UniValue trade(UniValue::VOBJ);
        trade.pushKV("txid", matchTxid);
        trade.pushKV("block", blockNum);
        if (prop1 == propertyId) {
            trade.pushKV("address", address1);
            trade.pushKV("amountsold", strAmount1);
            trade.pushKV("amountreceived", strAmount2);
            trade.pushKV("tradingfee", strTradingFee);
            totalReceived += amount2;
            totalSold += amount1;
        } else {
            trade.pushKV("address", address2);
            trade.pushKV("amountsold", strAmount2PlusFee);
            trade.pushKV("amountreceived", strAmount1);
            trade.pushKV("tradingfee", FormatMP(prop1, 0)); // not the liquidity taker so no fee for this participant - include attribute for standardness
            totalReceived += amount1;
            totalSold += amount2;
        }
        tradeArray.push_back(trade);
        ++count;
    }

    // clean up
    delete it;
    if (count) {
        return true;
    } else {
        return false;
    }
}


// obtains a vector of txids where the supplied address participated in a trade (needed for gettradehistory_MP)
// optional property ID parameter will filter on propertyId transacted if supplied
// sorted by block then index
void CMPTradeList::getTradesForAddress(const std::string& address, std::vector<uint256>& vecTransactions, uint32_t propertyIdFilter)
{
    if (!pdb) return;

    std::map<std::string, uint256> mapTrades;
    leveldb::Iterator* it = NewIterator();
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string strKey = it->key().ToString();
        std::string strValue = it->value().ToString();
        std::vector<std::string> vecValues;
        if (strKey.size() != 64) continue; // only interested in trades
        uint256 txid = uint256S(strKey);
        size_t addressMatch = strValue.find(address);
        if (addressMatch == std::string::npos) continue;
        boost::split(vecValues, strValue, boost::is_any_of(":"), boost::token_compress_on);
        if (vecValues.size() != 5) {
            PrintToLog("TRADEDB error - unexpected number of tokens in value (%s)\n", strValue);
            continue;
        }
        uint32_t propertyIdForSale = boost::lexical_cast<uint32_t>(vecValues[1]);
        uint32_t propertyIdDesired = boost::lexical_cast<uint32_t>(vecValues[2]);
        int64_t blockNum = boost::lexical_cast<uint32_t>(vecValues[3]);
        int64_t txIndex = boost::lexical_cast<uint32_t>(vecValues[4]);
        if (propertyIdFilter != 0 && propertyIdFilter != propertyIdForSale && propertyIdFilter != propertyIdDesired) continue;
        std::string sortKey = strprintf("%06d%010d", blockNum, txIndex);
        mapTrades.insert(std::make_pair(sortKey, txid));
    }
    delete it;

    for (std::map<std::string, uint256>::iterator it = mapTrades.begin(); it != mapTrades.end(); it++) {
        vecTransactions.push_back(it->second);
    }
}

static bool CompareTradePair(const std::pair<int64_t, UniValue>& firstJSONObj, const std::pair<int64_t, UniValue>& secondJSONObj)
{
    return firstJSONObj.first > secondJSONObj.first;
}

// obtains an array of matching trades with pricing and volume details for a pair sorted by blocknumber
void CMPTradeList::getTradesForPair(uint32_t propertyIdSideA, uint32_t propertyIdSideB, UniValue& responseArray, uint64_t count)
{
    if (!pdb) return;
    leveldb::Iterator* it = NewIterator();
    std::vector<std::pair<int64_t, UniValue> > vecResponse;
    bool propertyIdSideAIsDivisible = isPropertyDivisible(propertyIdSideA);
    bool propertyIdSideBIsDivisible = isPropertyDivisible(propertyIdSideB);
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string strKey = it->key().ToString();
        std::string strValue = it->value().ToString();
        std::vector<std::string> vecKeys;
        std::vector<std::string> vecValues;
        uint256 sellerTxid, matchingTxid;
        std::string sellerAddress, matchingAddress;
        int64_t amountReceived = 0, amountSold = 0;
        if (strKey.size() != 129) continue; // only interested in matches
        boost::split(vecKeys, strKey, boost::is_any_of("+"), boost::token_compress_on);
        boost::split(vecValues, strValue, boost::is_any_of(":"), boost::token_compress_on);
        if (vecKeys.size() != 2 || vecValues.size() != 8) {
            PrintToLog("TRADEDB error - unexpected number of tokens (%s:%s)\n", strKey, strValue);
            continue;
        }
        uint32_t tradePropertyIdSideA = boost::lexical_cast<uint32_t>(vecValues[2]);
        uint32_t tradePropertyIdSideB = boost::lexical_cast<uint32_t>(vecValues[3]);
        if (tradePropertyIdSideA == propertyIdSideA && tradePropertyIdSideB == propertyIdSideB) {
            sellerTxid.SetHex(vecKeys[1]);
            sellerAddress = vecValues[1];
            amountSold = boost::lexical_cast<int64_t>(vecValues[4]);
            matchingTxid.SetHex(vecKeys[0]);
            matchingAddress = vecValues[0];
            amountReceived = boost::lexical_cast<int64_t>(vecValues[5]);
        } else if (tradePropertyIdSideB == propertyIdSideA && tradePropertyIdSideA == propertyIdSideB) {
            sellerTxid.SetHex(vecKeys[0]);
            sellerAddress = vecValues[0];
            amountSold = boost::lexical_cast<int64_t>(vecValues[5]);
            matchingTxid.SetHex(vecKeys[1]);
            matchingAddress = vecValues[1];
            amountReceived = boost::lexical_cast<int64_t>(vecValues[4]);
        } else {
            continue;
        }

        rational_t unitPrice(amountReceived, amountSold);
        rational_t inversePrice(amountSold, amountReceived);
        if (!propertyIdSideAIsDivisible) unitPrice = unitPrice / COIN;
        if (!propertyIdSideBIsDivisible) inversePrice = inversePrice / COIN;
        std::string unitPriceStr = xToString(unitPrice); // TODO: not here!
        std::string inversePriceStr = xToString(inversePrice);

        int64_t blockNum = boost::lexical_cast<int64_t>(vecValues[6]);

        UniValue trade(UniValue::VOBJ);
        trade.pushKV("block", blockNum);
        trade.pushKV("unitprice", unitPriceStr);
        trade.pushKV("inverseprice", inversePriceStr);
        trade.pushKV("sellertxid", sellerTxid.GetHex());
        trade.pushKV("selleraddress", sellerAddress);
        if (propertyIdSideAIsDivisible) {
            trade.pushKV("amountsold", FormatDivisibleMP(amountSold));
        } else {
            trade.pushKV("amountsold", FormatIndivisibleMP(amountSold));
        }
        if (propertyIdSideBIsDivisible) {
            trade.pushKV("amountreceived", FormatDivisibleMP(amountReceived));
        } else {
            trade.pushKV("amountreceived", FormatIndivisibleMP(amountReceived));
        }
        trade.pushKV("matchingtxid", matchingTxid.GetHex());
        trade.pushKV("matchingaddress", matchingAddress);
        vecResponse.push_back(std::make_pair(blockNum, trade));
    }

    // sort the response most recent first before adding to the array
    std::sort(vecResponse.begin(), vecResponse.end(), CompareTradePair);
    uint64_t processed = 0;
    for (std::vector<std::pair<int64_t, UniValue> >::iterator it = vecResponse.begin(); it != vecResponse.end(); ++it) {
        responseArray.push_back(it->second);
        processed++;
        if (processed >= count) break;
    }

    std::vector<UniValue> responseArrayValues = responseArray.getValues();
    std::reverse(responseArrayValues.begin(), responseArrayValues.end());
    responseArray.clear();
    for (std::vector<UniValue>::iterator it = responseArrayValues.begin(); it != responseArrayValues.end(); ++it) {
        responseArray.push_back(*it);
    }

    delete it;
}

int CMPTradeList::getMPTradeCountTotal()
{
    int count = 0;
    leveldb::Iterator* it = NewIterator();
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        ++count;
    }
    delete it;
    return count;
}

const std::string gettingLineOut(std::string address1, std::string s_status1, int64_t lives_maker, std::string address2, std::string s_status2, int64_t lives_taker, int64_t nCouldBuy, uint64_t effective_price)
{
  const std::string lineOut = strprintf("%s\t %s\t %d\t %s\t %s\t %d\t %d\t %d",
				   address1, s_status1, FormatContractShortMP(lives_maker),
				   address2, s_status2, FormatContractShortMP(lives_taker),
				   FormatContractShortMP(nCouldBuy), FormatContractShortMP(effective_price));
  return lineOut;
}

void buildingEdge(std::map<std::string, std::string> &edgeEle, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, int64_t lives_src, int64_t lives_trk, int64_t amount_path, int64_t matched_price, int idx_q, int ghost_edge)
{
  edgeEle["addrs_src"]     = addrs_src;
  edgeEle["addrs_trk"]     = addrs_trk;
  edgeEle["status_src"]    = status_src;
  edgeEle["status_trk"]    = status_trk;
  edgeEle["lives_src"]     = std::to_string(FormatShortIntegerMP(lives_src));
  edgeEle["lives_trk"]     = std::to_string(FormatShortIntegerMP(lives_trk));
  edgeEle["amount_trd"]    = std::to_string(FormatShortIntegerMP(amount_path));
  edgeEle["matched_price"] = std::to_string(FormatContractShortMP(matched_price));
  edgeEle["edge_row"]      = std::to_string(idx_q);
  edgeEle["ghost_edge"]    = std::to_string(ghost_edge);
}

void CMPTradeList::recordMatchedTrade(const uint256 txid1, const uint256 txid2, std::string address1, std::string address2, uint64_t effective_price, uint64_t amount_maker, uint64_t amount_taker, int blockNum1, int blockNum2, uint32_t property_traded, std::string tradeStatus, int64_t lives_s0, int64_t lives_s1, int64_t lives_s2, int64_t lives_s3, int64_t lives_b0, int64_t lives_b1, int64_t lives_b2, int64_t lives_b3, std::string s_maker0, std::string s_taker0, std::string s_maker1, std::string s_taker1, std::string s_maker2, std::string s_taker2, std::string s_maker3, std::string s_taker3, int64_t nCouldBuy0, int64_t nCouldBuy1, int64_t nCouldBuy2, int64_t nCouldBuy3,uint64_t amountpnew, uint64_t amountpold)
{
  if (!pdb) return;

  extern volatile int idx_q;
  //extern volatile unsigned int path_length;
  std::map<std::string, std::string> edgeEle;
  std::map<std::string, double>::iterator it_addrs_upnlm;
  std::map<uint32_t, std::map<std::string, double>>::iterator it_addrs_upnlc;
  std::vector<std::map<std::string, std::string>>::iterator it_path_ele;
  std::vector<std::map<std::string, std::string>>::reverse_iterator reit_path_ele;
  //std::vector<std::map<std::string, std::string>> path_eleh;
  bool savedata_bool = false;
  extern volatile int64_t factorALLtoLTC;
  std::string sblockNum2 = std::to_string(blockNum2);
  double UPNL1 = 0, UPNL2 = 0;
  /********************************************************************/
  const std::string key =  sblockNum2 + "+" + txid1.ToString() + "+" + txid2.ToString(); //order with block of taker.
  const std::string value = strprintf("%s:%s:%lu:%lu:%lu:%d:%d:%s:%s:%d:%d:%d:%s:%s:%d:%d:%d", address1, address2, effective_price, amount_maker, amount_taker, blockNum1, blockNum2, s_maker0, s_taker0, lives_s0, lives_b0, property_traded, txid1.ToString(), txid2.ToString(), nCouldBuy0,amountpold, amountpnew);

  const std::string line0 = gettingLineOut(address1, s_maker0, lives_s0, address2, s_taker0, lives_b0, nCouldBuy0, effective_price);
  const std::string line1 = gettingLineOut(address1, s_maker1, lives_s1, address2, s_taker1, lives_b1, nCouldBuy1, effective_price);
  const std::string line2 = gettingLineOut(address1, s_maker2, lives_s2, address2, s_taker2, lives_b2, nCouldBuy2, effective_price);
  const std::string line3 = gettingLineOut(address1, s_maker3, lives_s3, address2, s_taker3, lives_b3, nCouldBuy3, effective_price);

  bool status_bool1 = s_maker0 == "OpenShortPosByLongPosNetted" || s_maker0 == "OpenLongPosByShortPosNetted";
  bool status_bool2 = s_taker0 == "OpenShortPosByLongPosNetted" || s_taker0 == "OpenLongPosByShortPosNetted";

  std::fstream fileSixth;
  fileSixth.open ("graphInfoSixth.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  if ( status_bool1 || status_bool2 )
    {
      if ( s_maker3 == "EmptyStr" && s_taker3 == "EmptyStr" ) savedata_bool = true;
      saveDataGraphs(fileSixth, line1, line2, line3, savedata_bool);
    }
  else saveDataGraphs(fileSixth, line0);
  fileSixth.close();

  /********************************************************************/
  int number_lines = 0;
  if ( status_bool1 || status_bool2 )
    {
      buildingEdge(edgeEle, address1, address2, s_maker1, s_taker1, lives_s1, lives_b1, nCouldBuy1, effective_price, idx_q, 0);
      //path_ele.push_back(edgeEle);
      //path_eleh.push_back(edgeEle);

      path_elef.push_back(edgeEle);
      buildingEdge(edgeEle, address1, address2, s_maker2, s_taker2, lives_s2, lives_b2, nCouldBuy2, effective_price, idx_q, 0);
      //path_ele.push_back(edgeEle);
      //path_eleh.push_back(edgeEle);

      path_elef.push_back(edgeEle);
      // PrintToLog("Line 1: %s\n", line1);
      // PrintToLog("Line 2: %s\n", line2);
      number_lines += 2;
      if ( s_maker3 != "EmptyStr" && s_taker3 != "EmptyStr" )
	{
	  buildingEdge(edgeEle, address1, address2, s_maker3, s_taker3, lives_s3, lives_b3,nCouldBuy3,effective_price,idx_q,0);
	  //path_ele.push_back(edgeEle);
	  //path_eleh.push_back(edgeEle);

	  path_elef.push_back(edgeEle);
	  if (msc_debug_tradedb) PrintToLog("Line 3: %s\n", line3);
	  number_lines += 1;
	}
    }
  else
    {
      buildingEdge(edgeEle, address1, address2, s_maker0, s_taker0, lives_s0, lives_b0, nCouldBuy0, effective_price, idx_q, 0);
      //path_ele.push_back(edgeEle);
      //path_eleh.push_back(edgeEle);

      path_elef.push_back(edgeEle);
      if (msc_debug_tradedb) PrintToLog("Line 0: %s\n", line0);
      number_lines += 1;
    }

  if (msc_debug_tradedb) PrintToLog("\nPath Ele inside recordMatchedTrade. Length last match = %d\n", number_lines);
  // for (it_path_ele = path_ele.begin(); it_path_ele != path_ele.end(); ++it_path_ele) printing_edges_database(*it_path_ele);

  /********************************************/
  /** Building TWAP vector CDEx **/

  Filling_Twap_Vec(cdextwap_ele, cdextwap_vec, property_traded, 0, effective_price);
  if (msc_debug_tradedb) PrintToLog("\ncdextwap_ele.size() = %d\n", cdextwap_ele[property_traded].size());
  // PrintToLog("\nVector CDExtwap_ele =\n");
  // for (unsigned int i = 0; i < cdextwap_ele[property_traded].size(); i++)
  //   PrintToLog("%s\n", FormatDivisibleMP(cdextwap_ele[property_traded][i]));

  /********************************************/

  // loopForUPNL(path_ele, path_eleh, path_length, address1, address2, s_maker0, s_taker0, UPNL1, UPNL2, effective_price, nCouldBuy0);
  // unsigned int limSup = path_ele.size()-path_length;
  // path_length = path_ele.size();

  // // PrintToLog("UPNL1 = %d, UPNL2 = %d\n", UPNL1, UPNL2);
  // addrs_upnlc[property_traded][address1] = UPNL1;
  // addrs_upnlc[property_traded][address2] = UPNL2;

  // for (it_addrs_upnlc = addrs_upnlc.begin(); it_addrs_upnlc != addrs_upnlc.end(); ++it_addrs_upnlc)
  //   {
  //     for (it_addrs_upnlm = it_addrs_upnlc->second.begin(); it_addrs_upnlm != it_addrs_upnlc->second.end(); ++it_addrs_upnlm)
  //     	{
  // 	  if (it_addrs_upnlm->first != address1 && it_addrs_upnlm->first != address2)
  // 	    {
  // 	      double entry_price_first = 0;
  // 	      int idx_price_first = 0;
  // 	      uint64_t entry_pricefirst_num = 0;
  // 	      double exit_priceh = (double)effective_price/COIN;
  // 	      uint64_t amount = 0;
  // 	      std::std::string status = "";
  // 	      std::std::string last_match_status = "";

  // 	      for (reit_path_ele = path_ele.rbegin(); reit_path_ele != path_ele.rend(); ++reit_path_ele)
  // 		{
  // 		  if(finding_std::string(it_addrs_upnlm->first, (*reit_path_ele)["addrs_src"]))
  // 		    {
  // 		      last_match_status = (*reit_path_ele)["status_src"];
  // 		      break;
  // 		    }
  // 		  else if(finding_std::string(it_addrs_upnlm->first, (*reit_path_ele)["addrs_trk"]))
  // 		    {
  // 		      last_match_status = (*reit_path_ele)["status_trk"];
  // 		      break;
  // 		    }
  // 		}
  // 	      loopforEntryPrice(path_ele, path_eleh, it_addrs_upnlm->first, last_match_status, entry_price_first, idx_price_first, entry_pricefirst_num, limSup, exit_priceh, amount, status);
  // 	      // PrintToLog("\namount for UPNL_show: %d\n", amount);
  // 	      double UPNL_show = PNL_function(entry_price_first, exit_priceh, amount, status);
  // 	      // PrintToLog("\nUPNL_show = %d\n", UPNL_show);
  // 	      addrs_upnlc[it_addrs_upnlc->first][it_addrs_upnlm->first] = UPNL_show;
  // 	    }
  // 	}
  //   }

  // for (it_addrs_upnlc = addrs_upnlc.begin(); it_addrs_upnlc != addrs_upnlc.end(); ++it_addrs_upnlc)
  //   {
  //     // PrintToLog("\nMap with addrs:upnl for propertyId = %d\n", it_addrs_upnlc->first);
  //     for (it_addrs_upnlm = it_addrs_upnlc->second.begin(); it_addrs_upnlm != it_addrs_upnlc->second.end(); ++it_addrs_upnlm)
  //     	{
  // 	  // PrintToLog("ADDRS = %s, UPNL = %d\n", it_addrs_upnlm->first, it_addrs_upnlm->second);
  // 	}

  unsigned int contractId = static_cast<unsigned int>(property_traded);
  CMPSPInfo::Entry sp;
  assert(mastercore::pDbSpInfo->getSP(property_traded, sp));
  uint32_t NotionalSize = sp.notional_size;
  globalPNLALL_DUSD += UPNL1 + UPNL2;
  globalVolumeALL_DUSD += nCouldBuy0;


  arith_uint256 volumeALL256_t = mastercore::ConvertTo256(NotionalSize)*mastercore::ConvertTo256(nCouldBuy0)/COIN;
  if (msc_debug_tradedb) PrintToLog("ALLs involved in the traded 256 Bits ~ %s ALL\n", volumeALL256_t.ToString());

  int64_t volumeALL64_t = mastercore::ConvertTo64(volumeALL256_t);
  if (msc_debug_tradedb) PrintToLog("ALLs involved in the traded 64 Bits ~ %s ALL\n", FormatDivisibleMP(volumeALL64_t));

  arith_uint256 volumeLTC256_t = mastercore::ConvertTo256(factorALLtoLTC)*mastercore::ConvertTo256(volumeALL64_t)/COIN;
  if (msc_debug_tradedb) PrintToLog("LTCs involved in the traded 256 Bits ~ %s LTC\n", volumeLTC256_t.ToString());

  int64_t volumeLTC64_t = mastercore::ConvertTo64(volumeLTC256_t);
  if (msc_debug_tradedb) PrintToLog("LTCs involved in the traded 64 Bits ~ %d LTC\n", FormatDivisibleMP(volumeLTC64_t));

  globalVolumeALL_LTC += volumeLTC64_t;
  if (msc_debug_tradedb) PrintToLog("\nGlobal LTC Volume Updated: CMPContractDEx = %d \n", FormatDivisibleMP(globalVolumeALL_LTC));

  int64_t volumeToCompare = 0;
  bool perpetualBool = callingPerpetualSettlement(globalPNLALL_DUSD, globalVolumeALL_DUSD, volumeToCompare);
  if (perpetualBool) PrintToLog("Perpetual Settlement Online");

  if (msc_debug_tradedb) PrintToLog("\nglobalPNLALL_DUSD = %d, globalVolumeALL_DUSD = %d, contractId = %d\n", globalPNLALL_DUSD, globalVolumeALL_DUSD, contractId);

  std::fstream fileglobalPNLALL_DUSD;
  fileglobalPNLALL_DUSD.open ("globalPNLALL_DUSD.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  if ( contractId == ALL_PROPERTY_TYPE_CONTRACT )
    saveDataGraphs(fileglobalPNLALL_DUSD, std::to_string(globalPNLALL_DUSD));
  fileglobalPNLALL_DUSD.close();

  std::fstream fileglobalVolumeALL_DUSD;
  fileglobalVolumeALL_DUSD.open ("globalVolumeALL_DUSD.txt", std::fstream::in | std::fstream::out | std::fstream::app);
  if ( contractId == ALL_PROPERTY_TYPE_CONTRACT )
    saveDataGraphs(fileglobalVolumeALL_DUSD, std::to_string(FormatShortIntegerMP(globalVolumeALL_DUSD)));
  fileglobalVolumeALL_DUSD.close();

  Status status;
  if (pdb)
    {
      status = pdb->Put(writeoptions, key, value);
      ++nWritten;
    }

}
