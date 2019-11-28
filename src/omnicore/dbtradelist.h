#ifndef BITCOIN_OMNICORE_DBTRADELIST_H
#define BITCOIN_OMNICORE_DBTRADELIST_H

#include <omnicore/dbbase.h>

#include <fs.h>
#include <uint256.h>

#include <univalue.h>

#include <stdint.h>

#include <string>
#include <vector>

/** LevelDB based storage for the MetaDEx trade history. Trades are listed with key "txid1+txid2".
 */
class CMPTradeList : public CDBBase
{
public:
    CMPTradeList(const fs::path& path, bool fWipe);
    virtual ~CMPTradeList();

    void recordMatchedTrade(const uint256& txid1, const uint256& txid2, const std::string& address1, const std::string& address2, uint32_t prop1, uint32_t prop2, int64_t amount1, int64_t amount2, int blockNum, int64_t fee);
    void recordMatchedTrade(const uint256 txid1, const uint256 txid2, std::string address1, std::string address2, uint64_t effective_price, uint64_t amount_maker, uint64_t amount_taker, int blockNum1, int blockNum2, uint32_t property_traded, std::string tradeStatus, int64_t lives_s0, int64_t lives_s1, int64_t lives_s2, int64_t lives_s3, int64_t lives_b0, int64_t lives_b1, int64_t lives_b2, int64_t lives_b3, std::string s_maker0, std::string s_taker0, std::string s_maker1, std::string s_taker1, std::string s_maker2, std::string s_taker2, std::string s_maker3, std::string s_taker3, int64_t nCouldBuy0, int64_t nCouldBuy1, int64_t nCouldBuy2, int64_t nCouldBuy3, uint64_t amountpnew, uint64_t amountpold);
    void recordNewTrade(const uint256& txid, const std::string& address, uint32_t propertyIdForSale, uint32_t propertyIdDesired, int blockNum, int blockIndex);
    int deleteAboveBlock(int blockNum);
    bool exists(const uint256 &txid);
    void printStats();
    void printAll();
    bool getMatchingTrades(const uint256& txid, uint32_t propertyId, UniValue& tradeArray, int64_t& totalSold, int64_t& totalBought);
    void getTradesForAddress(const std::string& address, std::vector<uint256>& vecTransactions, uint32_t propertyIdFilter = 0);
    void getTradesForPair(uint32_t propertyIdSideA, uint32_t propertyIdSideB, UniValue& response, uint64_t count);
    int getMPTradeCountTotal();
};

namespace mastercore
{
    //! LevelDB based storage for the MetaDEx trade history
    extern CMPTradeList* pDbTradeList;
}

const std::string gettingLineOut(std::string address1, std::string s_status1, int64_t lives_maker, std::string address2, std::string s_status2, int64_t lives_taker, int64_t nCouldBuy, uint64_t effective_price);
void buildingEdge(std::map<std::string, std::string> &edgeEle, std::string addrs_src, std::string addrs_trk, std::string status_src, std::string status_trk, int64_t lives_src, int64_t lives_trk, int64_t amount_path, int64_t matched_price, int idx_q, int ghost_edge);

#endif // BITCOIN_OMNICORE_DBTRADELIST_H
