#include <omnicore/mdex.h>

#include <omnicore/dbfees.h>
#include <omnicore/dbtradelist.h>
#include <omnicore/dbtxlist.h>
#include <omnicore/log.h>
#include <omnicore/rules.h>
#include <omnicore/sp.h>
#include <omnicore/uint256_extensions.h>

#include <omnicore/tradelayer_matrices.h>
#include <omnicore/externfns.h>
#include <omnicore/operators_algo_clearing.h>

#include <arith_uint256.h>
#include <chain.h>
#include <validation.h>
#include <tinyformat.h>
#include <uint256.h>

#include <univalue.h>

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

#include <openssl/sha.h>

#include <assert.h>
#include <stdint.h>

#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <string>

extern volatile uint64_t marketPrice;
extern volatile int idx_q;
extern int64_t factorE;
extern uint64_t marketP[NPTYPES];
extern int expirationAchieve;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> market_priceMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> numVWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> denVWAPMap;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMap;
extern std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> numVWAPVector;
extern std::map<uint32_t, std::map<uint32_t, std::vector<int64_t>>> denVWAPVector;
extern std::map<uint32_t, std::map<uint32_t, int64_t>> VWAPMapSubVector;
extern std::map<uint32_t, std::vector<int64_t>> mapContractAmountTimesPrice;
extern std::map<uint32_t, std::vector<int64_t>> mapContractVolume;
extern std::map<uint32_t, int64_t> VWAPMapContracts;
extern std::map<uint32_t, int64_t> cachefees;
extern int n_cols;
extern int n_rows;
extern MatrixTLS *pt_ndatabase;
extern int64_t globalNumPrice;
extern int64_t globalDenPrice;
extern int lastBlockg;
extern int volumeToVWAP;
extern int BlockS;

typedef boost::multiprecision::cpp_dec_float_100 dec_float;
typedef boost::multiprecision::checked_int128_t int128_t;

using namespace mastercore;

//! Number of digits of unit price
#define DISPLAY_PRECISION_LEN  50

//! Global map for price and order data
md_PropertiesMap mastercore::metadex;

md_PricesMap* mastercore::get_Prices(uint32_t prop)
{
    md_PropertiesMap::iterator it = metadex.find(prop);

    if (it != metadex.end()) return &(it->second);

    return static_cast<md_PricesMap*>(nullptr);
}

md_Set* mastercore::get_Indexes(md_PricesMap* p, rational_t price)
{
    md_PricesMap::iterator it = p->find(price);

    if (it != p->end()) return &(it->second);

    return static_cast<md_Set*>(nullptr);
}

static const std::string getTradeReturnType(MatchReturnType ret)
{
    switch (ret) {
        case NOTHING: return "NOTHING";
        case TRADED: return "TRADED";
        case TRADED_MOREINSELLER: return "TRADED_MOREINSELLER";
        case TRADED_MOREINBUYER: return "TRADED_MOREINBUYER";
        case ADDED: return "ADDED";
        case CANCELLED: return "CANCELLED";
        default: return "* unknown *";
    }
}

// Used by rangeInt64, xToInt64
static bool rangeInt64(const int128_t& value)
{
    return (std::numeric_limits<int64_t>::min() <= value && value <= std::numeric_limits<int64_t>::max());
}

// Used by xToString
static bool rangeInt64(const rational_t& value)
{
    return (rangeInt64(value.numerator()) && rangeInt64(value.denominator()));
}

// Used by CMPMetaDEx::displayUnitPrice
static int64_t xToRoundUpInt64(const rational_t& value)
{
    // for integer rounding up: ceil(num / denom) => 1 + (num - 1) / denom
    int128_t result = int128_t(1) + (value.numerator() - int128_t(1)) / value.denominator();

    assert(rangeInt64(result));

    return result.convert_to<int64_t>();
}

std::string xToString(const dec_float& value)
{
    return value.str(DISPLAY_PRECISION_LEN, std::ios_base::fixed);
}

std::string xToString(const int128_t& value)
{
    return strprintf("%s", boost::lexical_cast<std::string>(value));
}

std::string xToString(const rational_t& value)
{
    if (rangeInt64(value)) {
        int64_t num = value.numerator().convert_to<int64_t>();
        int64_t denom = value.denominator().convert_to<int64_t>();
        dec_float x = dec_float(num) / dec_float(denom);
        return xToString(x);
    } else {
        return strprintf("%s / %s", xToString(value.numerator()), xToString(value.denominator()));
    }
}

std::string xToString(const uint64_t &price)
{
  return strprintf("%s", boost::lexical_cast<std::string>(price));
}

std::string xToString(const int64_t &price)
{
  return strprintf("%s", boost::lexical_cast<std::string>(price));
}

std::string xToString(const uint32_t &value)
{
  return strprintf("%s", boost::lexical_cast<std::string>(value));
}

// find the best match on the market
// NOTE: sometimes I refer to the older order as seller & the newer order as buyer, in this trade
// INPUT: property, desprop, desprice = of the new order being inserted; the new object being processed
// RETURN:
static MatchReturnType x_Trade(CMPMetaDEx* const pnew)
{
    const uint32_t propertyForSale = pnew->getProperty();
    const uint32_t propertyDesired = pnew->getDesProperty();
    MatchReturnType NewReturn = NOTHING;
    bool bBuyerSatisfied = false;

    if (msc_debug_metadex1) PrintToLog("%s(%s: prop=%d, desprop=%d, desprice= %s);newo: %s\n",
        __FUNCTION__, pnew->getAddr(), propertyForSale, propertyDesired, xToString(pnew->inversePrice()), pnew->ToString());

    md_PricesMap* const ppriceMap = get_Prices(propertyDesired);

    // nothing for the desired property exists in the market, sorry!
    if (!ppriceMap) {
        PrintToLog("%s()=%d:%s NOT FOUND ON THE MARKET\n", __FUNCTION__, NewReturn, getTradeReturnType(NewReturn));
        return NewReturn;
    }

    // within the desired property map (given one property) iterate over the items looking at prices
    for (md_PricesMap::iterator priceIt = ppriceMap->begin(); priceIt != ppriceMap->end(); ++priceIt) { // check all prices
        const rational_t sellersPrice = priceIt->first;

        if (msc_debug_metadex2) PrintToLog("comparing prices: desprice %s needs to be GREATER THAN OR EQUAL TO %s\n",
            xToString(pnew->inversePrice()), xToString(sellersPrice));

        // Is the desired price check satisfied? The buyer's inverse price must be larger than that of the seller.
        if (pnew->inversePrice() < sellersPrice) {
            continue;
        }

        md_Set* const pofferSet = &(priceIt->second);

        // at good (single) price level and property iterate over offers looking at all parameters to find the match
        md_Set::iterator offerIt = pofferSet->begin();
        while (offerIt != pofferSet->end()) { // specific price, check all properties
            const CMPMetaDEx* const pold = &(*offerIt);
            assert(pold->unitPrice() == sellersPrice);

            if (msc_debug_metadex1) PrintToLog("Looking at existing: %s (its prop= %d, its des prop= %d) = %s\n",
                xToString(sellersPrice), pold->getProperty(), pold->getDesProperty(), pold->ToString());

            // does the desired property match?
            if (pold->getDesProperty() != propertyForSale) {
                ++offerIt;
                continue;
            }

            if (msc_debug_metadex1) PrintToLog("MATCH FOUND, Trade: %s = %s\n", xToString(sellersPrice), pold->ToString());

            // match found, execute trade now!
            const int64_t seller_amountForSale = pold->getAmountRemaining();
            const int64_t buyer_amountOffered = pnew->getAmountRemaining();

            if (msc_debug_metadex1) PrintToLog("$$ trading using price: %s; seller: forsale=%d, desired=%d, remaining=%d, buyer amount offered=%d\n",
                xToString(sellersPrice), pold->getAmountForSale(), pold->getAmountDesired(), pold->getAmountRemaining(), pnew->getAmountRemaining());
            if (msc_debug_metadex1) PrintToLog("$$ old: %s\n", pold->ToString());
            if (msc_debug_metadex1) PrintToLog("$$ new: %s\n", pnew->ToString());

            ///////////////////////////

            // preconditions
            assert(0 < pold->getAmountRemaining());
            assert(0 < pnew->getAmountRemaining());
            assert(pnew->getProperty() != pnew->getDesProperty());
            assert(pnew->getProperty() == pold->getDesProperty());
            assert(pold->getProperty() == pnew->getDesProperty());
            assert(pold->unitPrice() <= pnew->inversePrice());
            assert(pnew->unitPrice() <= pold->inversePrice());

            ///////////////////////////

            // First determine how many representable (indivisible) tokens Alice can
            // purchase from Bob, using Bob's unit price
            // This implies rounding down, since rounding up is impossible, and would
            // require more tokens than Alice has
            arith_uint256 iCouldBuy = (ConvertTo256(pnew->getAmountRemaining()) * ConvertTo256(pold->getAmountForSale())) / ConvertTo256(pold->getAmountDesired());

            int64_t nCouldBuy = 0;
            if (iCouldBuy < ConvertTo256(pold->getAmountRemaining())) {
                nCouldBuy = ConvertTo64(iCouldBuy);
            } else {
                nCouldBuy = pold->getAmountRemaining();
            }

            if (nCouldBuy == 0) {
                if (msc_debug_metadex1) PrintToLog(
                        "-- buyer has not enough tokens for sale to purchase one unit!\n");
                ++offerIt;
                continue;
            }

            // If the amount Alice would have to pay to buy Bob's tokens at his price
            // is fractional, always round UP the amount Alice has to pay
            // This will always be better for Bob. Rounding in the other direction
            // will always be impossible, because it would violate Bob's accepted price
            arith_uint256 iWouldPay = DivideAndRoundUp((ConvertTo256(nCouldBuy) * ConvertTo256(pold->getAmountDesired())), ConvertTo256(pold->getAmountForSale()));
            int64_t nWouldPay = ConvertTo64(iWouldPay);

            // If the resulting adjusted unit price is higher than Alice' price, the
            // orders shall not execute, and no representable fill is made
            const rational_t xEffectivePrice(nWouldPay, nCouldBuy);

            if (xEffectivePrice > pnew->inversePrice()) {
                if (msc_debug_metadex1) PrintToLog(
                        "-- effective price is too expensive: %s\n", xToString(xEffectivePrice));
                ++offerIt;
                continue;
            }

            const int64_t buyer_amountGot = nCouldBuy;
            const int64_t seller_amountGot = nWouldPay;
            const int64_t buyer_amountLeft = pnew->getAmountRemaining() - seller_amountGot;
            const int64_t seller_amountLeft = pold->getAmountRemaining() - buyer_amountGot;

            if (msc_debug_metadex1) PrintToLog("$$ buyer_got= %d, seller_got= %d, seller_left_for_sale= %d, buyer_still_for_sale= %d\n",
                buyer_amountGot, seller_amountGot, seller_amountLeft, buyer_amountLeft);

            ///////////////////////////

            // postconditions
            assert(xEffectivePrice >= pold->unitPrice());
            assert(xEffectivePrice <= pnew->inversePrice());
            assert(0 <= seller_amountLeft);
            assert(0 <= buyer_amountLeft);
            assert(seller_amountForSale == seller_amountLeft + buyer_amountGot);
            assert(buyer_amountOffered == buyer_amountLeft + seller_amountGot);

            ///////////////////////////

            int64_t buyer_amountGotAfterFee = buyer_amountGot;
            int64_t tradingFee = 0;

            // strip a 0.05% fee from non-OMNI pairs if fees are activated
            if (IsFeatureActivated(FEATURE_FEES, pnew->getBlock())) {
                if (pold->getProperty() > OMNI_PROPERTY_TMSC && pold->getDesProperty() > OMNI_PROPERTY_TMSC) {
                    int64_t feeDivider = 2000; // 0.05%
                    tradingFee = buyer_amountGot / feeDivider;

                    // subtract the fee from the amount the seller will receive
                    buyer_amountGotAfterFee = buyer_amountGot - tradingFee;

                    // add the fee to the fee cache
                    pDbFeeCache->AddFee(pnew->getDesProperty(), pnew->getBlock(), tradingFee);
                } else {
                    if (msc_debug_fees) PrintToLog("Skipping fee reduction for trade match %s:%s as one of the properties is Omni\n", pold->getHash().GetHex(), pnew->getHash().GetHex());
                }
            }

            // transfer the payment property from buyer to seller
            assert(update_tally_map(pnew->getAddr(), pnew->getProperty(), -seller_amountGot, BALANCE));
            assert(update_tally_map(pold->getAddr(), pold->getDesProperty(), seller_amountGot, BALANCE));

            // transfer the market (the one being sold) property from seller to buyer
            assert(update_tally_map(pold->getAddr(), pold->getProperty(), -buyer_amountGot, METADEX_RESERVE));
            assert(update_tally_map(pnew->getAddr(), pnew->getDesProperty(), buyer_amountGotAfterFee, BALANCE));

            NewReturn = TRADED;

            CMPMetaDEx seller_replacement = *pold; // < can be moved into last if block
            seller_replacement.setAmountRemaining(seller_amountLeft, "seller_replacement");

            pnew->setAmountRemaining(buyer_amountLeft, "buyer");

            if (0 < buyer_amountLeft) {
                NewReturn = TRADED_MOREINBUYER;
            }

            if (0 == buyer_amountLeft) {
                bBuyerSatisfied = true;
            }

            if (0 < seller_amountLeft) {
                NewReturn = TRADED_MOREINSELLER;
            }

            if (msc_debug_metadex1) PrintToLog("==== TRADED !!! %u=%s\n", NewReturn, getTradeReturnType(NewReturn));

            // record the trade in MPTradeList
            pDbTradeList->recordMatchedTrade(pold->getHash(), pnew->getHash(), // < might just pass pold, pnew
                pold->getAddr(), pnew->getAddr(), pold->getDesProperty(), pnew->getDesProperty(), seller_amountGot, buyer_amountGotAfterFee, pnew->getBlock(), tradingFee);

            if (msc_debug_metadex1) PrintToLog("++ erased old: %s\n", offerIt->ToString());
            // erase the old seller element
            pofferSet->erase(offerIt++);

            // insert the updated one in place of the old
            if (0 < seller_replacement.getAmountRemaining()) {
                PrintToLog("++ inserting seller_replacement: %s\n", seller_replacement.ToString());
                pofferSet->insert(seller_replacement);
            }

            if (bBuyerSatisfied) {
                assert(buyer_amountLeft == 0);
                break;
            }
        } // specific price, check all properties

        if (bBuyerSatisfied) break;
    } // check all prices

    PrintToLog("%s()=%d:%s\n", __FUNCTION__, NewReturn, getTradeReturnType(NewReturn));

    return NewReturn;
}

/**
 * Used for display of unit prices to 8 decimal places at UI layer.
 *
 * Automatically returns unit or inverse price as needed.
 */
std::string CMPMetaDEx::displayUnitPrice() const
{
     rational_t tmpDisplayPrice;
     if (getDesProperty() == OMNI_PROPERTY_MSC || getDesProperty() == OMNI_PROPERTY_TMSC) {
         tmpDisplayPrice = unitPrice();
         if (isPropertyDivisible(getProperty())) tmpDisplayPrice = tmpDisplayPrice * COIN;
     } else {
         tmpDisplayPrice = inversePrice();
         if (isPropertyDivisible(getDesProperty())) tmpDisplayPrice = tmpDisplayPrice * COIN;
     }

     // offers with unit prices under 0.00000001 will be excluded from UI layer - TODO: find a better way to identify sub 0.00000001 prices
     std::string tmpDisplayPriceStr = xToString(tmpDisplayPrice);
     if (!tmpDisplayPriceStr.empty()) { if (tmpDisplayPriceStr.substr(0,1) == "0") return "0.00000000"; }

     // we must always round up here - for example if the actual price required is 0.3333333344444
     // round: 0.33333333 - price is insufficient and thus won't result in a trade
     // round: 0.33333334 - price will be sufficient to result in a trade
     std::string displayValue = FormatDivisibleMP(xToRoundUpInt64(tmpDisplayPrice));
     return displayValue;
}

/**
 * Used for display of unit prices with 50 decimal places at RPC layer.
 *
 * Note: unit price is no longer always shown in OMNI and/or inverted
 */
std::string CMPMetaDEx::displayFullUnitPrice() const
{
    rational_t tempUnitPrice = unitPrice();

    /* Matching types require no action (divisible/divisible or indivisible/indivisible)
       Non-matching types require adjustment for display purposes
           divisible/indivisible   : *COIN
           indivisible/divisible   : /COIN
    */
    if ( isPropertyDivisible(getProperty()) && !isPropertyDivisible(getDesProperty()) ) tempUnitPrice = tempUnitPrice*COIN;
    if ( !isPropertyDivisible(getProperty()) && isPropertyDivisible(getDesProperty()) ) tempUnitPrice = tempUnitPrice/COIN;

    std::string unitPriceStr = xToString(tempUnitPrice);
    return unitPriceStr;
}

rational_t CMPMetaDEx::unitPrice() const
{
    rational_t effectivePrice;
    if (amount_forsale) effectivePrice = rational_t(amount_desired, amount_forsale);
    return effectivePrice;
}

rational_t CMPMetaDEx::inversePrice() const
{
    rational_t inversePrice;
    if (amount_desired) inversePrice = rational_t(amount_forsale, amount_desired);
    return inversePrice;
}

int64_t CMPMetaDEx::getAmountToFill() const
{
    // round up to ensure that the amount we present will actually result in buying all available tokens
    arith_uint256 iAmountNeededToFill = DivideAndRoundUp((ConvertTo256(amount_remaining) * ConvertTo256(amount_desired)), ConvertTo256(amount_forsale));
    int64_t nAmountNeededToFill = ConvertTo64(iAmountNeededToFill);
    return nAmountNeededToFill;
}

int64_t CMPMetaDEx::getBlockTime() const
{
    CBlockIndex* pblockindex = chainActive[block];
    return pblockindex->GetBlockTime();
}

void CMPMetaDEx::setAmountRemaining(int64_t amount, const std::string& label)
{
    amount_remaining = amount;
    PrintToLog("update remaining amount still up for sale (%ld %s):%s\n", amount, label, ToString());
}

std::string CMPMetaDEx::ToString() const
{
    return strprintf("%s:%34s in %d/%03u, txid: %s , trade #%u %s for #%u %s",
        xToString(unitPrice()), addr, block, idx, txid.ToString().substr(0, 10),
        property, FormatMP(property, amount_forsale), desired_property, FormatMP(desired_property, amount_desired));
}

void CMPMetaDEx::saveOffer(std::ofstream& file, SHA256_CTX* shaCtx) const
{
    std::string lineOut = strprintf("%s,%d,%d,%d,%d,%d,%d,%d,%s,%d",
        addr,
        block,
        amount_forsale,
        property,
        amount_desired,
        desired_property,
        subaction,
        idx,
        txid.ToString(),
        amount_remaining
    );

    // add the line to the hash
    SHA256_Update(shaCtx, lineOut.c_str(), lineOut.length());

    // write the line
    file << lineOut << std::endl;
}

bool MetaDEx_compare::operator()(const CMPMetaDEx &lhs, const CMPMetaDEx &rhs) const
{
    if (lhs.getBlock() == rhs.getBlock()) return lhs.getIdx() < rhs.getIdx();
    else return lhs.getBlock() < rhs.getBlock();
}

bool mastercore::MetaDEx_INSERT(const CMPMetaDEx& objMetaDEx)
{
    // Create an empty price map (to use in case price map for this property does not already exist)
    md_PricesMap temp_prices;
    // Attempt to obtain the price map for the property
    md_PricesMap *p_prices = get_Prices(objMetaDEx.getProperty());

    // Create an empty set of metadex objects (to use in case no set currently exists at this price)
    md_Set temp_indexes;
    md_Set *p_indexes = nullptr;

    // Prepare for return code
    std::pair <md_Set::iterator, bool> ret;

    // Attempt to obtain a set of metadex objects for this price from the price map
    if (p_prices) p_indexes = get_Indexes(p_prices, objMetaDEx.unitPrice());
    // See if the set was populated, if not no set exists at this price level, use the empty set that we created earlier
    if (!p_indexes) p_indexes = &temp_indexes;

    // Attempt to insert the metadex object into the set
    ret = p_indexes->insert(objMetaDEx);
    if (false == ret.second) return false;

    // If a prices map did not exist for this property, set p_prices to the temp empty price map
    if (!p_prices) p_prices = &temp_prices;

    // Update the prices map with the new set at this price
    (*p_prices)[objMetaDEx.unitPrice()] = *p_indexes;

    // Set the metadex map for the property to the updated (or new if it didn't exist) price map
    metadex[objMetaDEx.getProperty()] = *p_prices;

    return true;
}

// pretty much directly linked to the ADD TX21 command off the wire
int mastercore::MetaDEx_ADD(const std::string& sender_addr, uint32_t prop, int64_t amount, int block, uint32_t property_desired, int64_t amount_desired, const uint256& txid, unsigned int idx)
{
    int rc = METADEX_ERROR -1;

    // Create a MetaDEx object from parameters
    CMPMetaDEx new_mdex(sender_addr, block, prop, amount, property_desired, amount_desired, txid, idx, CMPTransaction::ADD);
    if (msc_debug_metadex1) PrintToLog("%s(); buyer obj: %s\n", __FUNCTION__, new_mdex.ToString());

    // Ensure this is not a badly priced trade (for example due to zero amounts)
    if (0 >= new_mdex.unitPrice()) return METADEX_ERROR -66;

    // Match against existing trades, remainder of the order will be put into the order book
    if (msc_debug_metadex3) MetaDEx_debug_print();
    x_Trade(&new_mdex);
    if (msc_debug_metadex3) MetaDEx_debug_print();

    // Insert the remaining order into the MetaDEx maps
    if (0 < new_mdex.getAmountRemaining()) { //switch to getAmountRemaining() when ready
        if (!MetaDEx_INSERT(new_mdex)) {
            PrintToLog("%s() ERROR: ALREADY EXISTS, line %d, file: %s\n", __FUNCTION__, __LINE__, __FILE__);
            return METADEX_ERROR -70;
        } else {
            // move tokens into reserve
            assert(update_tally_map(sender_addr, prop, -new_mdex.getAmountRemaining(), BALANCE));
            assert(update_tally_map(sender_addr, prop, new_mdex.getAmountRemaining(), METADEX_RESERVE));

            if (msc_debug_metadex1) PrintToLog("==== INSERTED: %s= %s\n", xToString(new_mdex.unitPrice()), new_mdex.ToString());
            if (msc_debug_metadex3) MetaDEx_debug_print();
        }
    }

    rc = 0;
    return rc;
}

int mastercore::MetaDEx_CANCEL_AT_PRICE(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, int64_t amount, uint32_t property_desired, int64_t amount_desired)
{
    int rc = METADEX_ERROR -20;
    CMPMetaDEx mdex(sender_addr, 0, prop, amount, property_desired, amount_desired, uint256(), 0, CMPTransaction::CANCEL_AT_PRICE);
    md_PricesMap* prices = get_Prices(prop);
    const CMPMetaDEx* p_mdex = nullptr;

    if (msc_debug_metadex1) PrintToLog("%s():%s\n", __FUNCTION__, mdex.ToString());

    if (msc_debug_metadex2) MetaDEx_debug_print();

    if (!prices) {
        PrintToLog("%s() NOTHING FOUND for %s\n", __FUNCTION__, mdex.ToString());
        return rc -1;
    }

    // within the desired property map (given one property) iterate over the items
    for (md_PricesMap::iterator my_it = prices->begin(); my_it != prices->end(); ++my_it) {
        rational_t sellers_price = my_it->first;

        if (mdex.unitPrice() != sellers_price) continue;

        md_Set* indexes = &(my_it->second);

        for (md_Set::iterator iitt = indexes->begin(); iitt != indexes->end();) {
            p_mdex = &(*iitt);

            if (msc_debug_metadex3) PrintToLog("%s(): %s\n", __FUNCTION__, p_mdex->ToString());

            if ((p_mdex->getDesProperty() != property_desired) || (p_mdex->getAddr() != sender_addr)) {
                ++iitt;
                continue;
            }

            rc = 0;
            PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, p_mdex->ToString());

            // move from reserve to main
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), -p_mdex->getAmountRemaining(), METADEX_RESERVE));
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), p_mdex->getAmountRemaining(), BALANCE));

            // record the cancellation
            bool bValid = true;
            pDbTransactionList->recordMetaDExCancelTX(txid, p_mdex->getHash(), bValid, block, p_mdex->getProperty(), p_mdex->getAmountRemaining());

            indexes->erase(iitt++);
        }
    }

    if (msc_debug_metadex2) MetaDEx_debug_print();

    return rc;
}

int mastercore::MetaDEx_CANCEL_ALL_FOR_PAIR(const uint256& txid, unsigned int block, const std::string& sender_addr, uint32_t prop, uint32_t property_desired)
{
    int rc = METADEX_ERROR -30;
    md_PricesMap* prices = get_Prices(prop);
    const CMPMetaDEx* p_mdex = nullptr;

    PrintToLog("%s(%d,%d)\n", __FUNCTION__, prop, property_desired);

    if (msc_debug_metadex3) MetaDEx_debug_print();

    if (!prices) {
        PrintToLog("%s() NOTHING FOUND\n", __FUNCTION__);
        return rc -1;
    }

    // within the desired property map (given one property) iterate over the items
    for (md_PricesMap::iterator my_it = prices->begin(); my_it != prices->end(); ++my_it) {
        md_Set* indexes = &(my_it->second);

        for (md_Set::iterator iitt = indexes->begin(); iitt != indexes->end();) {
            p_mdex = &(*iitt);

            if (msc_debug_metadex3) PrintToLog("%s(): %s\n", __FUNCTION__, p_mdex->ToString());

            if ((p_mdex->getDesProperty() != property_desired) || (p_mdex->getAddr() != sender_addr)) {
                ++iitt;
                continue;
            }

            rc = 0;
            PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, p_mdex->ToString());

            // move from reserve to main
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), -p_mdex->getAmountRemaining(), METADEX_RESERVE));
            assert(update_tally_map(p_mdex->getAddr(), p_mdex->getProperty(), p_mdex->getAmountRemaining(), BALANCE));

            // record the cancellation
            bool bValid = true;
            pDbTransactionList->recordMetaDExCancelTX(txid, p_mdex->getHash(), bValid, block, p_mdex->getProperty(), p_mdex->getAmountRemaining());

            indexes->erase(iitt++);
        }
    }

    if (msc_debug_metadex3) MetaDEx_debug_print();

    return rc;
}

/**
 * Scans the orderbook and remove everything for an address.
 */
int mastercore::MetaDEx_CANCEL_EVERYTHING(const uint256& txid, unsigned int block, const std::string& sender_addr, unsigned char ecosystem)
{
    int rc = METADEX_ERROR -40;

    PrintToLog("%s()\n", __FUNCTION__);

    if (msc_debug_metadex2) MetaDEx_debug_print();

    PrintToLog("<<<<<<\n");

    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        unsigned int prop = my_it->first;

        // skip property, if it is not in the expected ecosystem
        if (isMainEcosystemProperty(ecosystem) && !isMainEcosystemProperty(prop)) continue;
        if (isTestEcosystemProperty(ecosystem) && !isTestEcosystemProperty(prop)) continue;

        PrintToLog(" ## property: %u\n", prop);
        md_PricesMap& prices = my_it->second;

        for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            rational_t price = it->first;
            md_Set& indexes = it->second;

            PrintToLog("  # Price Level: %s\n", xToString(price));

            for (md_Set::iterator it = indexes.begin(); it != indexes.end();) {
                PrintToLog("%s= %s\n", xToString(price), it->ToString());

                if (it->getAddr() != sender_addr) {
                    ++it;
                    continue;
                }

                rc = 0;
                PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, it->ToString());

                // move from reserve to balance
                assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountRemaining(), METADEX_RESERVE));
                assert(update_tally_map(it->getAddr(), it->getProperty(), it->getAmountRemaining(), BALANCE));

                // record the cancellation
                bool bValid = true;
                pDbTransactionList->recordMetaDExCancelTX(txid, it->getHash(), bValid, block, it->getProperty(), it->getAmountRemaining());

                indexes.erase(it++);
            }
        }
    }
    PrintToLog(">>>>>>\n");

    if (msc_debug_metadex2) MetaDEx_debug_print();

    return rc;
}

/**
 * Scans the orderbook and removes every all-pair order
 */
int mastercore::MetaDEx_SHUTDOWN_ALLPAIR()
{
    int rc = 0;
    PrintToLog("%s()\n", __FUNCTION__);
    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        md_PricesMap& prices = my_it->second;
        for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            md_Set& indexes = it->second;
            for (md_Set::iterator it = indexes.begin(); it != indexes.end();) {
                if (it->getDesProperty() > OMNI_PROPERTY_TMSC && it->getProperty() > OMNI_PROPERTY_TMSC) { // no OMN/TOMN side to the trade
                    PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, it->ToString());
                    // move from reserve to balance
                    assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountRemaining(), METADEX_RESERVE));
                    assert(update_tally_map(it->getAddr(), it->getProperty(), it->getAmountRemaining(), BALANCE));
                    indexes.erase(it++);
                }
            }
        }
    }
    return rc;
}

/**
 * Scans the orderbook and removes every order
 */
int mastercore::MetaDEx_SHUTDOWN()
{
    int rc = 0;
    PrintToLog("%s()\n", __FUNCTION__);
    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        md_PricesMap& prices = my_it->second;
        for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            md_Set& indexes = it->second;
            for (md_Set::iterator it = indexes.begin(); it != indexes.end();) {
                PrintToLog("%s(): REMOVING %s\n", __FUNCTION__, it->ToString());
                // move from reserve to balance
                assert(update_tally_map(it->getAddr(), it->getProperty(), -it->getAmountRemaining(), METADEX_RESERVE));
                assert(update_tally_map(it->getAddr(), it->getProperty(), it->getAmountRemaining(), BALANCE));
                indexes.erase(it++);
            }
        }
    }
    return rc;
}

// searches the metadex maps to see if a trade is still open
// allows search to be optimized if propertyIdForSale is specified
bool mastercore::MetaDEx_isOpen(const uint256& txid, uint32_t propertyIdForSale)
{
    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        if (propertyIdForSale != 0 && propertyIdForSale != my_it->first) continue;
        md_PricesMap & prices = my_it->second;
        for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            md_Set & indexes = (it->second);
            for (md_Set::iterator it = indexes.begin(); it != indexes.end(); ++it) {
                CMPMetaDEx obj = *it;
                if( obj.getHash().GetHex() == txid.GetHex() ) return true;
            }
        }
    }
    return false;
}

/**
 * Returns a string describing the status of a trade
 *
 */
std::string mastercore::MetaDEx_getStatusText(int tradeStatus)
{
    switch (tradeStatus) {
        case TRADE_OPEN: return "open";
        case TRADE_OPEN_PART_FILLED: return "open part filled";
        case TRADE_FILLED: return "filled";
        case TRADE_CANCELLED: return "cancelled";
        case TRADE_CANCELLED_PART_FILLED: return "cancelled part filled";
        case TRADE_INVALID: return "trade invalid";
        default: return "unknown";
    }
}

/**
 * Returns the status of a MetaDEx trade
 *
 */
int mastercore::MetaDEx_getStatus(const uint256& txid, uint32_t propertyIdForSale, int64_t amountForSale, int64_t totalSold)
{
    // NOTE: If the calling code is already aware of the total amount sold, pass the value in to this function to avoid duplication of
    //       work.  If the calling code doesn't know the amount, leave default (-1) and we will calculate it from levelDB lookups.
    if (totalSold == -1) {
        UniValue tradeArray(UniValue::VARR);
        int64_t totalReceived;
        pDbTradeList->getMatchingTrades(txid, propertyIdForSale, tradeArray, totalSold, totalReceived);
    }

    // Return a "trade invalid" status if the trade was invalidated at parsing/interpretation (eg insufficient funds)
    if (!pDbTransactionList->getValidMPTX(txid)) return TRADE_INVALID;

    // Calculate and return the status of the trade via the amount sold and open/closed attributes.
    if (MetaDEx_isOpen(txid, propertyIdForSale)) {
        if (totalSold == 0) {
            return TRADE_OPEN;
        } else {
            return TRADE_OPEN_PART_FILLED;
        }
    } else {
        if (totalSold == 0) {
            return TRADE_CANCELLED;
        } else if (totalSold < amountForSale) {
            return TRADE_CANCELLED_PART_FILLED;
        } else {
            return TRADE_FILLED;
        }
    }
}

void mastercore::MetaDEx_debug_print(bool bShowPriceLevel, bool bDisplay)
{
    PrintToLog("<<<\n");
    for (md_PropertiesMap::iterator my_it = metadex.begin(); my_it != metadex.end(); ++my_it) {
        uint32_t prop = my_it->first;

        PrintToLog(" ## property: %u\n", prop);
        md_PricesMap& prices = my_it->second;

        for (md_PricesMap::iterator it = prices.begin(); it != prices.end(); ++it) {
            rational_t price = it->first;
            md_Set& indexes = it->second;

            if (bShowPriceLevel) PrintToLog("  # Price Level: %s\n", xToString(price));

            for (md_Set::iterator it = indexes.begin(); it != indexes.end(); ++it) {
                const CMPMetaDEx& obj = *it;

                if (bDisplay) PrintToConsole("%s= %s\n", xToString(price), obj.ToString());
                else PrintToLog("%s= %s\n", xToString(price), obj.ToString());
            }
        }
    }
    PrintToLog(">>>\n");
}

/**
 * Locates a trade in the MetaDEx maps via txid and returns the trade object
 *
 */
const CMPMetaDEx* mastercore::MetaDEx_RetrieveTrade(const uint256& txid)
{
    for (md_PropertiesMap::iterator propIter = metadex.begin(); propIter != metadex.end(); ++propIter) {
        md_PricesMap & prices = propIter->second;
        for (md_PricesMap::iterator pricesIter = prices.begin(); pricesIter != prices.end(); ++pricesIter) {
            md_Set & indexes = pricesIter->second;
            for (md_Set::iterator tradesIter = indexes.begin(); tradesIter != indexes.end(); ++tradesIter) {
                if (txid == (*tradesIter).getHash()) return &(*tradesIter);
            }
        }
    }
    return static_cast<CMPMetaDEx*>(nullptr);
}


/**
 * ContractDEx functions.
 */
 cd_PropertiesMap mastercore::contractdex;

 cd_PricesMap *mastercore::get_PricesCd(uint32_t prop)
 {
     cd_PropertiesMap::iterator it = contractdex.find(prop);

     if (it != contractdex.end()) return &(it->second);

     return (cd_PricesMap*) NULL;
 }

 MatchReturnType x_Trade(CMPContractDex* const pnew)
 {
   const uint32_t propertyForSale = pnew->getProperty();
   uint8_t trdAction = pnew->getTradingAction();
   MatchReturnType NewReturn = NOTHING;

   cd_PricesMap* const ppriceMap = get_PricesCd(propertyForSale);

   if (!ppriceMap)
     {
       PrintToLog("%s()=%d:%s NOT FOUND ON THE MARKET\n", __FUNCTION__, NewReturn, getTradeReturnType(NewReturn));
       return NewReturn;
     }

   LoopBiDirectional(ppriceMap, trdAction, NewReturn, pnew, propertyForSale);

   return NewReturn;
 }

 void mastercore::LoopBiDirectional(cd_PricesMap* const ppriceMap, uint8_t trdAction, MatchReturnType &NewReturn, CMPContractDex* const pnew, const uint32_t propertyForSale)
 {
   cd_PricesMap::iterator it_fwdPrices;
   cd_PricesMap::reverse_iterator it_bwdPrices;

   if ( trdAction == BUY )
     {
       for (it_fwdPrices = ppriceMap->begin(); it_fwdPrices != ppriceMap->end(); ++it_fwdPrices)
 	{
 	  const uint64_t sellerPrice = it_fwdPrices->first;
 	  if ( pnew->getEffectivePrice() < sellerPrice )
 	    {
 	      continue;
 	    }
 	  x_TradeBidirectional(it_fwdPrices, it_bwdPrices, trdAction, pnew, sellerPrice, propertyForSale, NewReturn);
 	}
     }
   else
     {
       for (it_bwdPrices = ppriceMap->rbegin(); it_bwdPrices != ppriceMap->rend(); ++it_bwdPrices)
 	{
 	  const uint64_t sellerPrice = it_bwdPrices->first;

 	  if ( pnew->getEffectivePrice() > sellerPrice )
 	    {
 	      continue;
 	    }
 	  x_TradeBidirectional(it_fwdPrices, it_bwdPrices, trdAction, pnew, sellerPrice, propertyForSale, NewReturn);
 	}
     }
 }

 void mastercore::x_TradeBidirectional(typename cd_PricesMap::iterator &it_fwdPrices, typename cd_PricesMap::reverse_iterator &it_bwdPrices, uint8_t trdAction, CMPContractDex* const pnew, const uint64_t sellerPrice, const uint32_t propertyForSale, MatchReturnType &NewReturn)
 {
   cd_Set* const pofferSet = trdAction == BUY ? &(it_fwdPrices->second) : &(it_bwdPrices->second);

   /** At good (single) price level and property iterate over offers looking at all parameters to find the match */
   cd_Set::iterator offerIt = pofferSet->begin();

   while ( offerIt != pofferSet->end() )  /** Specific price, check all properties */
     {
       const CMPContractDex* const pold = &(*offerIt);

       assert(pold->getEffectivePrice() == sellerPrice);

       std::string tradeStatus = pold->getEffectivePrice() == sellerPrice ? "Matched" : "NoMatched";

       /** Match Conditions */
       bool boolProperty  = pold->getProperty() != propertyForSale;
       bool boolTrdAction = pold->getTradingAction() == pnew->getTradingAction();
       bool boolAddresses = pold->getAddr() != pnew->getAddr();

  //      if ( findTrueValue(boolProperty, boolTrdAction, !boolAddresses) )
  //    	{
 	//   ++offerIt;
 	//   continue;
 	// }

       idx_q += 1;
       // const int idx_qp = idx_q;

       /********************************************************/
       /** Preconditions */
       assert(pold->getProperty() == pnew->getProperty());

       if(msc_debug_x_trade_bidirectional)
       {
           PrintToLog("________________________________________________________\n");
           PrintToLog("Inside x_trade:\n");
           PrintToLog("Checking effective prices and trading actions:\n");
           PrintToLog("Effective price pold: %d\n", FormatContractShortMP(pold->getEffectivePrice()) );
           PrintToLog("Effective price pnew: %d\n", FormatContractShortMP(pnew->getEffectivePrice()) );
           PrintToLog("Amount for sale pold: %d\n", pold->getAmountForSale() );
           PrintToLog("Amount for sale pnew: %d\n", pnew->getAmountForSale() );
           PrintToLog("Trading action pold: %d\n", pold->getTradingAction() );
           PrintToLog("Trading action pnew: %d\n", pnew->getTradingAction() );
           PrintToLog("Trade Status: %s\n", tradeStatus);
           PrintToLog("propertyForSale = %d", propertyForSale);
           PrintToLog("\nlastBlockg = %s\n", lastBlockg);
       }

       /********************************************************/
       uint32_t property_traded = pold->getProperty();
       uint64_t amountpnew = pnew->getAmountForSale();
       uint64_t amountpold = pold->getAmountForSale();

       int64_t poldPositiveBalanceB = GetTokenBalance(pold->getAddr(), property_traded, POSSITIVE_BALANCE);
       int64_t pnewPositiveBalanceB = GetTokenBalance(pnew->getAddr(), property_traded, POSSITIVE_BALANCE);
       int64_t poldNegativeBalanceB = GetTokenBalance(pold->getAddr(), property_traded, NEGATIVE_BALANCE);
       int64_t pnewNegativeBalanceB = GetTokenBalance(pnew->getAddr(), property_traded, NEGATIVE_BALANCE);

       if(msc_debug_x_trade_bidirectional)
       {
           PrintToLog("poldPositiveBalanceB: %d, poldNegativeBalanceB: %d\n", poldPositiveBalanceB, poldNegativeBalanceB);
           PrintToLog("pnewPositiveBalanceB: %d, pnewNegativeBalanceB: %d\n", pnewPositiveBalanceB, pnewNegativeBalanceB);
       }

       int64_t possitive_sell = (pold->getTradingAction() == SELL) ? poldPositiveBalanceB : pnewPositiveBalanceB;
       int64_t negative_sell  = (pold->getTradingAction() == SELL) ? poldNegativeBalanceB : pnewNegativeBalanceB;
       int64_t possitive_buy  = (pold->getTradingAction() == SELL) ? pnewPositiveBalanceB : poldPositiveBalanceB;
       int64_t negative_buy   = (pold->getTradingAction() == SELL) ? pnewNegativeBalanceB : poldNegativeBalanceB;

       int64_t seller_amount  = (pold->getTradingAction() == SELL) ? pold->getAmountForSale() : pnew->getAmountForSale();
       int64_t buyer_amount   = (pold->getTradingAction() == SELL) ? pnew->getAmountForSale() : pold->getAmountForSale();
       std::string seller_address = (pold->getTradingAction() == SELL) ? pold->getAddr() : pnew->getAddr();
       std::string buyer_address  = (pold->getTradingAction() == SELL) ? pnew->getAddr() : pold->getAddr();

       int64_t nCouldBuy = buyer_amount < seller_amount ? buyer_amount : seller_amount;

       if (nCouldBuy == 0)
       	{
       	  ++offerIt;
       	  continue;
       	}

       /*************************************************************************************************/
       /** Computing VWAP Price**/

       CMPSPInfo::Entry sp;
       assert(pDbSpInfo->getSP(property_traded, sp));

       uint32_t NotionalSize = sp.notional_size;

       arith_uint256 Volume256_t = mastercore::ConvertTo256(NotionalSize)*mastercore::ConvertTo256(nCouldBuy)/COIN;
       int64_t Volume64_t = mastercore::ConvertTo64(Volume256_t);

       if(msc_debug_x_trade_bidirectional) PrintToLog("\nNotionalSize = %s\t nCouldBuy = %s\t Volume64_t = %s\n",
 		  FormatDivisibleMP(NotionalSize), FormatDivisibleMP(nCouldBuy), FormatDivisibleMP(Volume64_t));

       arith_uint256 numVWAP256_t = mastercore::ConvertTo256(sellerPrice)*mastercore::ConvertTo256(Volume64_t)/COIN;
       int64_t numVWAP64_t = mastercore::ConvertTo64(numVWAP256_t);

       threading(property_traded, numVWAP64_t, "cdex_price");
       threading(property_traded, Volume64_t, "cdex_volume");

       std::vector<int64_t> numVWAPpriceContract(mapContractAmountTimesPrice[property_traded].end()-
 						std::min(int(mapContractAmountTimesPrice[property_traded].size()), volumeToVWAP),
 						mapContractAmountTimesPrice[property_traded].end());
       std::vector<int64_t> denVWAPpriceContract(mapContractVolume[property_traded].end()-
 						std::min(int(mapContractVolume[property_traded].size()), volumeToVWAP),
 						mapContractVolume[property_traded].end());
       int64_t numVWAPriceh = 0, denVWAPriceh = 0;

       int vwaplength = denVWAPpriceContract.size();
       for (int i = 0; i < vwaplength; i++)
 	{
 	  numVWAPriceh += numVWAPpriceContract[i];
 	  denVWAPriceh += denVWAPpriceContract[i];
 	}

       rational_t vwapPricehRat(numVWAPriceh, denVWAPriceh);
       int64_t vwapPriceh64_t = mastercore::RationalToInt64(vwapPricehRat);
       threading(property_traded, vwapPriceh64_t, "cdex_vwap");

       /********************************************************/
       int64_t difference_s = 0, difference_b = 0;
       if (boolAddresses)
 	{
 	  if ( possitive_sell != 0 )
 	    {
 	      difference_s = possitive_sell - nCouldBuy;
 	      if (difference_s >= 0)
 		assert(update_tally_map(seller_address, property_traded, -nCouldBuy, POSSITIVE_BALANCE));
 	      else
 		{
 		  assert(update_tally_map(seller_address, property_traded, -possitive_sell, POSSITIVE_BALANCE));
 		  assert(update_tally_map(seller_address, property_traded, -difference_s, NEGATIVE_BALANCE));
 		}
 	    }
 	  else if ( negative_sell != 0 || negative_sell == 0 || possitive_sell == 0 )
 	    assert(update_tally_map(seller_address, property_traded, nCouldBuy, NEGATIVE_BALANCE));

 	  if ( negative_buy != 0 )
 	    {
 	      difference_b = negative_buy - nCouldBuy;
 	      if (difference_b >= 0)
 		assert(update_tally_map(buyer_address, property_traded, -nCouldBuy, NEGATIVE_BALANCE));
 	      else
 		{
 		  assert(update_tally_map(buyer_address, property_traded, -negative_buy, NEGATIVE_BALANCE));
 		  assert(update_tally_map(buyer_address, property_traded, -difference_b, POSSITIVE_BALANCE));
 		}
 	    }
 	  else if ( possitive_buy != 0 || possitive_buy == 0 || negative_buy == 0 )
 	    assert(update_tally_map(buyer_address, property_traded, nCouldBuy, POSSITIVE_BALANCE));
 	}
       /********************************************************/
       int64_t poldPositiveBalanceL = GetTokenBalance(pold->getAddr(), property_traded, POSSITIVE_BALANCE);
       int64_t pnewPositiveBalanceL = GetTokenBalance(pnew->getAddr(), property_traded, POSSITIVE_BALANCE);
       int64_t poldNegativeBalanceL = GetTokenBalance(pold->getAddr(), property_traded, NEGATIVE_BALANCE);
       int64_t pnewNegativeBalanceL = GetTokenBalance(pnew->getAddr(), property_traded, NEGATIVE_BALANCE);

       std::string Status_s = "Empty";
       std::string Status_b = "Empty";

       NewReturn = TRADED;
       CMPContractDex contract_replacement = *pold;
       int64_t creplNegativeBalance = GetTokenBalance(contract_replacement.getAddr(), property_traded, NEGATIVE_BALANCE);
       int64_t creplPositiveBalance = GetTokenBalance(contract_replacement.getAddr(), property_traded, POSSITIVE_BALANCE);

       if(msc_debug_x_trade_bidirectional)
       {
           PrintToLog("poldPositiveBalance: %d, poldNegativeBalance: %d\n", poldPositiveBalanceL, poldNegativeBalanceL);
           PrintToLog("pnewPositiveBalance: %d, pnewNegativeBalance: %d\n", pnewPositiveBalanceL, pnewNegativeBalanceL);
           PrintToLog("creplPositiveBalance: %d, creplNegativeBalance: %d\n", creplPositiveBalance, creplNegativeBalance);
       }

       int64_t remaining = seller_amount >= buyer_amount ? seller_amount - buyer_amount : buyer_amount - seller_amount;

       if ( (seller_amount > buyer_amount && pold->getTradingAction() == SELL) || (seller_amount < buyer_amount && pold->getTradingAction() == BUY))
       	{
       	  contract_replacement.setAmountForsale(remaining, "moreinseller");
       	  pnew->setAmountForsale(0, "no_remaining");
       	  NewReturn = TRADED_MOREINSELLER;
       	}
       else if ( (seller_amount < buyer_amount && pold->getTradingAction() == SELL) || (seller_amount > buyer_amount && pold->getTradingAction() == BUY))
       	{
       	  contract_replacement.setAmountForsale(0, "no_remaining");
       	  pnew->setAmountForsale(remaining, "moreinbuyer");
       	  NewReturn = TRADED_MOREINBUYER;
       	}
       else if (seller_amount == buyer_amount)
       	{
       	  pnew->setAmountForsale(0, "no_remaining");
       	  contract_replacement.setAmountForsale(0, "no_remaining");
       	  NewReturn = TRADED;
       	}
       /********************************************************/
       int64_t countClosedSeller = 0, countClosedBuyer  = 0;
       if ( possitive_sell > 0 && negative_sell == 0 )
       	{
       	  if ( pold->getTradingAction() == SELL )
       	    {
       	      Status_s = possitive_sell > creplPositiveBalance && creplPositiveBalance != 0 ? "LongPosNettedPartly" : ( creplPositiveBalance == 0 && creplNegativeBalance == 0 ? "LongPosNetted" : ( creplPositiveBalance == 0 && creplNegativeBalance > 0 ? "OpenShortPosByLongPosNetted" : "LongPosIncreased") );
       	      countClosedSeller = creplPositiveBalance == 0 ? possitive_sell : abs( possitive_sell - creplPositiveBalance );
       	    }
       	  else
       	    {
       	      Status_s = possitive_sell > pnewPositiveBalanceL && pnewPositiveBalanceL != 0 ? "LongPosNettedPartly" : ( pnewPositiveBalanceL == 0 && pnewNegativeBalanceL == 0 ? "LongPosNetted" : ( pnewPositiveBalanceL == 0 && pnewNegativeBalanceL > 0 ? "OpenShortPosByLongPosNetted": "LongPosIncreased") );
       	      countClosedSeller = pnewPositiveBalanceL == 0 ? possitive_sell : abs( possitive_sell - pnewPositiveBalanceL );
       	    }
       	}
       else if ( negative_sell > 0 && possitive_sell == 0 )
       	{
       	  if ( pold->getTradingAction() == SELL )
       	    {
       	      Status_s = negative_sell > creplNegativeBalance && creplNegativeBalance != 0 ? "ShortPosNettedPartly" : ( creplNegativeBalance == 0 && creplPositiveBalance == 0 ? "ShortPosNetted" : ( creplNegativeBalance == 0 && creplPositiveBalance > 0 ? "OpenLongPosByShortPosNetted" : "ShortPosIncreased") );
       	      countClosedSeller = creplNegativeBalance == 0 ? negative_sell : abs( negative_sell - creplNegativeBalance );
       	    }
       	  else
       	    {
       	      Status_s = negative_sell > pnewNegativeBalanceL && pnewNegativeBalanceL != 0 ? "ShortPosNettedPartly" : ( pnewNegativeBalanceL == 0 && pnewPositiveBalanceL == 0 ? "ShortPosNetted" : ( pnewNegativeBalanceL == 0 && pnewPositiveBalanceL > 0 ? "OpenLongPosByShortPosNetted" : "ShortPosIncreased") );
       	      countClosedSeller = pnewNegativeBalanceL == 0 ? negative_sell : abs( negative_sell - pnewNegativeBalanceL );
       	    }
       	}
       else if ( negative_sell == 0 && possitive_sell == 0 )
       	{
       	  if ( pold->getTradingAction() == SELL )
       	    Status_s = creplPositiveBalance > 0 ? "OpenLongPosition" : "OpenShortPosition";
       	  else
       	    Status_s = pnewPositiveBalanceL  > 0 ? "OpenLongPosition" : "OpenShortPosition";
       	  countClosedSeller = 0;
       	}
       /********************************************************/
       if ( possitive_buy > 0 && negative_buy == 0 )
       	{
       	  if ( pold->getTradingAction() == BUY )
       	    {
       	      Status_b = possitive_buy > creplPositiveBalance && creplPositiveBalance != 0 ? "LongPosNettedPartly" : ( creplPositiveBalance == 0 && creplNegativeBalance == 0 ? "LongPosNetted" : ( creplPositiveBalance == 0 && creplNegativeBalance > 0  ? "OpenShortPosByLongPosNetted" : "LongPosIncreased") );
       	      countClosedBuyer = creplPositiveBalance == 0 ? possitive_buy : abs( possitive_buy - creplPositiveBalance );
       	    }
       	  else
       	    {
       	      Status_b = possitive_buy > pnewPositiveBalanceL && pnewPositiveBalanceL != 0 ? "LongPosNettedPartly" : ( pnewPositiveBalanceL == 0 && pnewNegativeBalanceL == 0 ? "LongPosNetted" : ( pnewPositiveBalanceL == 0 && pnewNegativeBalanceL > 0 ? "OpenShortPosByLongPosNetted" : "LongPosIncreased") );
       	      countClosedBuyer = pnewPositiveBalanceL == 0 ? possitive_buy : abs( possitive_buy - pnewPositiveBalanceL );
       	    }
       	}
       else if ( negative_buy > 0 && possitive_buy == 0 )
       	{
       	  if ( pold->getTradingAction() == BUY )
       	    {
       	      Status_b = negative_buy > creplNegativeBalance && creplNegativeBalance != 0 ? "ShortPosNettedPartly" : ( creplNegativeBalance == 0 && creplPositiveBalance == 0 ? "ShortPosNetted" : ( creplNegativeBalance == 0 && creplPositiveBalance > 0 ? "OpenLongPosByShortPosNetted" : "ShortPosIncreased" ) );
       	      countClosedBuyer = creplNegativeBalance == 0 ? negative_buy : abs( negative_buy - creplNegativeBalance );
       	    }
       	  else
       	    {
       	      Status_b = negative_buy > pnewNegativeBalanceL && pnewNegativeBalanceL != 0 ? "ShortPosNettedPartly" : ( pnewNegativeBalanceL == 0 && pnewPositiveBalanceL == 0 ? "ShortPosNetted" : ( pnewNegativeBalanceL == 0 && pnewPositiveBalanceL > 0 ? "OpenLongPosByShortPosNetted" : "ShortPosIncreased") );
       	      countClosedBuyer = pnewNegativeBalanceL == 0 ? negative_buy : abs( negative_buy - pnewNegativeBalanceL );
       	    }
       	}
       else if ( negative_buy == 0 && possitive_buy == 0 )
       	{
       	  if ( pold->getTradingAction() == BUY )
       	    Status_b = creplPositiveBalance > 0 ? "OpenLongPosition" : "OpenShortPosition";
       	  else
       	    Status_b = pnewPositiveBalanceL > 0 ? "OpenLongPosition" : "OpenShortPosition";
       	  countClosedBuyer = 0;
       	}
       /********************************************************/
       int64_t lives_maker = 0, lives_taker = 0;

       if( creplPositiveBalance > 0 && creplNegativeBalance == 0 )
       	lives_maker = creplPositiveBalance;
       else if( creplNegativeBalance > 0 && creplPositiveBalance == 0 )
       	lives_maker = creplNegativeBalance;

       if( pnewPositiveBalanceL && pnewNegativeBalanceL == 0 )
       	lives_taker = pnewPositiveBalanceL;
       else if( pnewNegativeBalanceL > 0 && pnewPositiveBalanceL == 0 )
       	lives_taker = pnewNegativeBalanceL;

       if ( countClosedSeller < 0 ) countClosedSeller = 0;
       if ( countClosedBuyer  < 0 ) countClosedBuyer  = 0;
       /********************************************************/
       std::string Status_maker = "", Status_taker = "";
       if (pold->getAddr() == seller_address)
       	{
       	  Status_maker = Status_s;
       	  Status_taker = Status_b;
       	}
       else
       	{
       	  Status_maker = Status_b;
       	  Status_taker = Status_s;
       	}

       if(msc_debug_x_trade_bidirectional) PrintToLog("Status_maker = %d, Status_taker = %d\n", Status_maker, Status_taker);

       std::string Status_s0 = "EmptyStr", Status_s1 = "EmptyStr", Status_s2 = "EmptyStr", Status_s3 = "EmptyStr";
       std::string Status_b0 = "EmptyStr", Status_b1 = "EmptyStr", Status_b2 = "EmptyStr", Status_b3 = "EmptyStr";

       int64_t lives_maker0 = 0, lives_maker1 = 0, lives_maker2 = 0, lives_maker3 = 0;
       int64_t lives_taker0 = 0, lives_taker1 = 0, lives_taker2 = 0, lives_taker3 = 0;
       int64_t nCouldBuy0 = 0, nCouldBuy1 = 0, nCouldBuy2 = 0, nCouldBuy3 = 0;

       lives_maker0 = lives_maker;
       lives_taker0 = lives_taker;
       nCouldBuy0 = nCouldBuy;
       /********************************************************/
       if ( pold->getTradingAction() == SELL )
 	{
 	  // If maker Sell and Open Short by Long Netted: status_sj -> makers
 	  if ( Status_maker == "OpenShortPosByLongPosNetted" )
 	    {
 	      if ( Status_taker == "OpenLongPosByShortPosNetted" )
 		{
 		  if ( possitive_sell > negative_buy )
 		    {
 		      Status_s1  = "LongPosNettedPartly";
 		      lives_maker1   = possitive_sell - negative_buy;
 		      Status_b1  = "ShortPosNetted";
 		      lives_taker1   = 0;
 		      nCouldBuy1 = negative_buy;

 		      Status_s2  = "LongPosNetted";
 		      lives_maker2   = 0;
 		      Status_b2  = "OpenLongPosition";
 		      lives_taker2   = lives_maker1;
 		      nCouldBuy2 = lives_maker1;

 		      Status_s3  = "OpenShortPosition";
 		      lives_maker3   = nCouldBuy - possitive_sell;
 		      Status_b3  = "LongPosIncreased";
 		      lives_taker3   = lives_taker2 + lives_maker3;
 		      nCouldBuy3 = lives_maker3;

 		    }
 		  else if ( possitive_sell < negative_buy )
 		    {
 		      Status_s1  = "LongPosNetted";
 		      lives_maker1   = 0;
 		      Status_b1  = "ShortPosNettedPartly";
 		      lives_taker1   = negative_buy - possitive_sell;
 		      nCouldBuy1 = possitive_sell;

 		      Status_s2  = "OpenShortPosition";
 		      lives_maker2   = negative_buy - possitive_sell;
 		      Status_b2  = "ShortPosNetted";
 		      lives_taker2   = 0;
 		      nCouldBuy2 = lives_maker2;

 		      Status_b3  = "OpenLongPosition";
 		      lives_taker3   = nCouldBuy - negative_buy;
 		      Status_s3  = "ShortPosIncreased";
 		      lives_maker3   = lives_maker2 + lives_taker3;
 		      nCouldBuy3 = lives_taker3;

 		    }
 		  else if ( possitive_sell == negative_buy )
 		    {
 		      Status_s1  = "LongPosNetted";
 		      lives_maker1   = 0;
 		      Status_b1  = "ShortPosNetted";
 		      lives_taker1   = 0;
 		      nCouldBuy1 = possitive_sell;

 		      Status_s2  = "OpenShortPosition";
 		      lives_maker2   = nCouldBuy - possitive_sell;
 		      Status_b2  = "OpenLongPosition";
 		      lives_taker2   = lives_maker2;
 		      nCouldBuy2 = lives_maker2;
 		    }
 		}
 	      else if ( Status_taker == "ShortPosNettedPartly" )
 		{
 		  Status_s1  = "LongPosNetted";
 		  lives_maker1   = 0;
 		  Status_b1  = "ShortPosNettedPartly";
 		  lives_taker1   = negative_buy - possitive_sell;
 		  nCouldBuy1 = possitive_sell;

 		  Status_s2  = "OpenShortPosition";
 		  lives_maker2   = nCouldBuy - possitive_sell;
 		  Status_b2  = "ShortPosNettedPartly";
 		  lives_taker2   = lives_taker1 - lives_maker2;
 		  nCouldBuy2 = lives_maker2;

 		}
 	      else if ( Status_taker == "ShortPosNetted" )
 		{
 		  Status_s1  = "LongPosNetted";
 		  lives_maker1   = 0;
 		  Status_b1  = "ShortPosNettedPartly";
 		  lives_taker1   = negative_buy - possitive_sell;
 		  nCouldBuy1 = possitive_sell;

 		  Status_s2  = "OpenShortPosition";
 		  lives_maker2   = nCouldBuy - possitive_sell;
 		  Status_b2  = "ShortPosNetted";
 		  lives_taker2   = 0;
 		  nCouldBuy2 = lives_maker2;

 		}
 	      else if ( Status_taker == "OpenLongPosition" )
 		{
 		  Status_s1  = "LongPosNetted";
 		  lives_maker1   = 0;
 		  Status_b1  = "OpenLongPosition";
 		  lives_taker1   = possitive_sell;
 		  nCouldBuy1 = possitive_sell;

 		  Status_s2  = "OpenShortPosition";
 		  lives_maker2   = nCouldBuy - possitive_sell;
 		  Status_b2  = "LongPosIncreased";
 		  lives_taker2   = lives_taker1 + lives_maker2;
 		  nCouldBuy2 = lives_maker2;

 		}
 	      else if ( Status_taker == "LongPosIncreased" )
 		{
 		  Status_s1  = "LongPosNetted";
 		  lives_maker1   = 0;
 		  Status_b1  = "LongPosIncreased";
 		  lives_taker1   = possitive_buy + possitive_sell;
 		  nCouldBuy1 = possitive_sell;

 		  Status_s2  = "OpenShortPosition";
 		  lives_maker2   = nCouldBuy - possitive_sell;
 		  Status_b2  = "LongPosIncreased";
 		  lives_taker2   = lives_taker1 + lives_maker2;
 		  nCouldBuy2 = lives_maker2;
 		}
 	    }
 	  // Checked
 	}
       else
 	{
 	  // If maker Buy and Open Long by Short Netted: status_bj -> makers
 	  if ( Status_maker == "OpenLongPosByShortPosNetted" )
 	    {
 	      if ( Status_taker == "OpenShortPosByLongPosNetted" )
 		{
 		  if ( negative_buy < possitive_sell )
 		    {
 		      Status_b1  = "ShortPosNetted";
 		      lives_maker1   = 0;
 		      Status_s1  = "LongPosNettedPartly";
 		      lives_taker1   = possitive_sell - negative_buy;
 		      nCouldBuy1 = negative_buy;

 		      Status_b2  = "OpenLongPosition";
 		      lives_maker2   = lives_taker1;
 		      Status_s2  = "LongPosNetted";
 		      lives_taker2   = 0;
 		      nCouldBuy2 = lives_taker1;

 		      Status_b3  = "LongPosIncreased";
 		      lives_maker3   = lives_maker2 + nCouldBuy - possitive_sell;
 		      Status_s3  = "OpenShortPosition";
 		      lives_taker3   = nCouldBuy - possitive_sell;
 		      nCouldBuy3 = lives_taker3;

 		    }
 		  else if ( negative_buy > possitive_sell )
 		    {
 		      Status_b1  = "ShortPosNettedPartly";
 		      lives_maker1   = negative_buy - possitive_sell;
 		      Status_s1  = "LongPosNetted";
 		      lives_taker1   = 0;
 		      nCouldBuy1 = possitive_sell;

 		      Status_b2  = "ShortPosNetted";
 		      lives_maker2   = 0;
 		      Status_s2  = "OpenShortPosition";
 		      lives_taker2   = lives_maker1;
 		      nCouldBuy2 = lives_maker1;

 		      Status_b3  = "OpenLongPosition";
 		      lives_maker3   = nCouldBuy - negative_buy;
 		      Status_s3  = "ShortPosIncreased";
 		      lives_taker3   = lives_taker2 + lives_maker3;
 		      nCouldBuy3 = lives_maker3;

 		    }
 		  else if ( negative_buy == possitive_sell )
 		    {
 		      Status_b1  = "ShortPosNetted";
 		      lives_maker1   = 0;
 		      Status_s1  = "LongPosNetted";
 		      lives_taker1   = 0;
 		      nCouldBuy1 = possitive_sell;

 		      Status_b2  = "OpenLongPosition";
 		      lives_maker2   = nCouldBuy - possitive_sell;
 		      Status_s2  = "OpenShortPosition";
 		      lives_taker2   = lives_maker2;
 		      nCouldBuy2 = lives_maker2;
 		    }
 		}
 	      else if ( Status_taker == "LongPosNettedPartly" )
 		{
 		  Status_b1  = "ShortPosNetted";
 		  lives_maker1   = 0;
 		  Status_s1  = "LongPosNettedPartly";
 		  lives_taker1 = possitive_sell - negative_buy;
 		  nCouldBuy1 = negative_buy;

 		  Status_b2  = "OpenLongPosition";
 		  lives_maker2  = nCouldBuy - negative_buy;
 		  Status_s2  = "LongPosNettedPartly";
 		  lives_taker2  = lives_taker1 - lives_maker2;
 		  nCouldBuy2 = lives_maker2;

 		}
 	      else if ( Status_taker == "LongPosNetted" )
 		{
 		  Status_b1  = "ShortPosNetted";
 		  lives_maker1   = 0;
 		  Status_s1  = "LongPosNettedPartly";
 		  lives_taker1   = possitive_sell - negative_buy;
 		  nCouldBuy1 = negative_buy;

 		  Status_b2  = "OpenLongPosition";
 		  lives_maker2   = nCouldBuy - negative_buy;
 		  Status_s2  = "LongPosNetted";
 		  lives_taker2   = 0;
 		  nCouldBuy2 = lives_maker2;

 		}
 	      else if ( Status_taker == "OpenShortPosition" )
 		{
 		  Status_b1  = "ShortPosNetted";
 		  lives_maker1   = 0;
 		  Status_s1  = "OpenShortPosition";
 		  lives_taker1   = negative_buy;
 		  nCouldBuy1 = negative_buy;

 		  Status_b2  = "OpenLongPosition";
 		  lives_maker2   = nCouldBuy - negative_buy;
 		  Status_s2  = "ShortPosIncreased";
 		  lives_taker2   = lives_taker1 + lives_maker2;
 		  nCouldBuy2 = lives_maker2;

 		}
 	      else if ( Status_taker == "ShortPosIncreased" )
 		{
 		  Status_b1  = "ShortPosNetted";
 		  lives_maker1   = 0;
 		  Status_s1  = "ShortPosIncreased";
 		  lives_taker1   = negative_sell + negative_buy;
 		  nCouldBuy1 = negative_buy;

 		  Status_b2  = "OpenLongPosition";
 		  lives_maker2   = nCouldBuy - negative_buy;
 		  Status_s2  = "ShortPosIncreased";
 		  lives_taker2   = lives_taker1 + lives_maker2;
 		  nCouldBuy2 = lives_maker2;
 		}
 	    }
 	  // Checked
 	}
       /********************************************************/
       if ( pold->getTradingAction() == BUY )
 	{
 	  // If taker Sell and Open Short by Long Netted: status_sj -> taker
 	  if ( Status_taker == "OpenShortPosByLongPosNetted" )
 	    {
 	      if ( Status_maker == "OpenLongPosByShortPosNetted" )
 		{
 		  if ( possitive_sell > negative_buy )
 		    {
 		      Status_s1  = "LongPosNettedPartly";
 		      lives_taker1   = possitive_sell - negative_buy;
 		      Status_b1  = "ShortPosNetted";
 		      lives_maker1   = 0;
 		      nCouldBuy1 = negative_buy;

 		      Status_s2  = "LongPosNetted";
 		      lives_taker2   = 0;
 		      Status_b2  = "OpenLongPosition";
 		      lives_maker2   = lives_taker1;
 		      nCouldBuy2 = lives_taker1;

 		      Status_s3  = "OpenShortPosition";
 		      lives_taker3   = nCouldBuy - possitive_sell;
 		      Status_b3  = "LongPosIncreased";
 		      lives_maker3   = lives_maker2 + lives_taker3;
 		      nCouldBuy3 = lives_taker3;

 		    }
 		  else if ( possitive_sell < negative_buy )
 		    {
 		      Status_s1  = "LongPosNetted";
 		      lives_taker1   = 0;
 		      Status_b1  = "ShortPosNettedPartly";
 		      lives_maker1   = negative_buy - possitive_sell ;
 		      nCouldBuy1 = possitive_sell;

 		      Status_s2  = "OpenShortPosition";
 		      lives_taker2   = lives_maker1;
 		      Status_b2  = "ShortPosNetted";
 		      lives_maker2   = 0;
 		      nCouldBuy2 = lives_taker2;

 		      Status_b3  = "OpenLongPosition";
 		      lives_maker3   = nCouldBuy - negative_buy;
 		      Status_s3  = "ShortPosIncreased";
 		      lives_taker3   = lives_taker2 + lives_maker3;
 		      nCouldBuy3 = lives_maker3;

 		    }
 		  else if ( possitive_sell == negative_buy )
 		    {
 		      Status_s1  = "LongPosNetted";
 		      lives_taker1   = 0;
 		      Status_b1  = "ShortPosNetted";
 		      lives_maker1   = 0;
 		      nCouldBuy1 = possitive_sell;

 		      Status_s2  = "OpenShortPosition";
 		      lives_taker2   = nCouldBuy - possitive_sell;
 		      Status_b2  = "OpenLongPosition";
 		      lives_maker2   = lives_taker2;
 		      nCouldBuy2 = lives_taker2;
 		    }
 		}
 	      else if ( Status_maker == "ShortPosNettedPartly" )
 		{
 		  Status_s1  = "LongPosNetted";
 		  lives_taker1   = 0;
 		  Status_b1  = "ShortPosNettedPartly";
 		  lives_maker1   = negative_buy - possitive_sell;
 		  nCouldBuy1 = possitive_sell;

 		  Status_s2  = "OpenShortPosition";
 		  lives_taker2   = nCouldBuy - possitive_sell;
 		  Status_b2  = "ShortPosNettedPartly";
 		  lives_maker2   = lives_maker1 - lives_taker2;
 		  nCouldBuy2 = lives_taker2;

 		}
 	      else if ( Status_maker == "ShortPosNetted" )
 		{
 		  Status_s1  = "LongPosNetted";
 		  lives_taker1   = 0;
 		  Status_b1  = "ShortPosNettedPartly";
 		  lives_maker1   = negative_buy - possitive_sell;
 		  nCouldBuy1 = possitive_sell;

 		  Status_s2  = "OpenShortPosition";
 		  lives_taker2   = nCouldBuy - possitive_sell;
 		  Status_b2  = "ShortPosNetted";
 		  lives_maker2   = 0;
 		  nCouldBuy2 = lives_taker2;

 		}
 	      else if ( Status_maker == "OpenLongPosition" )
 		{
 		  Status_s1  = "LongPosNetted";
 		  lives_taker1   = 0;
 		  Status_b1  = "OpenLongPosition";
 		  lives_maker1   = possitive_sell;
 		  nCouldBuy1 = possitive_sell;

 		  Status_s2  = "OpenShortPosition";
 		  lives_taker2   = nCouldBuy - possitive_sell;
 		  Status_b2  = "LongPosIncreased";
 		  lives_maker2   = lives_maker1 + lives_taker2;
 		  nCouldBuy2 = lives_taker2;

 		}
 	      else if ( Status_maker == "LongPosIncreased" )
 		{
 		  Status_s1  = "LongPosNetted";
 		  lives_taker1   = 0;
 		  Status_b1  = "LongPosIncreased";
 		  lives_maker1   = possitive_buy + possitive_sell;
 		  nCouldBuy1 = possitive_sell;

 		  Status_s2  = "OpenShortPosition";
 		  lives_taker2   = nCouldBuy - possitive_sell;
 		  Status_b2  = "LongPosIncreased";
 		  lives_maker2   = lives_maker1 + lives_taker2;
 		  nCouldBuy2 = lives_taker2;
 		}
 	    }
 	  // Checked
 	}
       else
 	{
 	  // If taker Buy and Open Long by Short Netted: status_bj -> taker
 	  if ( Status_taker == "OpenLongPosByShortPosNetted" )
 	    {
 	      if ( Status_maker == "OpenShortPosByLongPosNetted" )
 		{
 		  if ( negative_buy < possitive_sell )
 		    {
 		      Status_b1  = "ShortPosNetted";
 		      lives_taker1   = 0;
 		      Status_s1  = "LongPosNettedPartly";
 		      lives_maker1   = possitive_sell - negative_buy;
 		      nCouldBuy1 = negative_buy;

 		      Status_b2  = "OpenLongPosition";
 		      lives_taker2   = lives_maker1;
 		      Status_s2  = "LongPosNetted";
 		      lives_maker2   = 0;
 		      nCouldBuy2 = lives_maker1;

 		      Status_b3  = "LongPosIncreased";
 		      lives_taker3   = lives_taker2 + nCouldBuy - possitive_sell;
 		      Status_s3  = "OpenShortPosition";
 		      lives_maker3   = nCouldBuy - possitive_sell;
 		      nCouldBuy3 = lives_maker3;

 		    }
 		  else if ( negative_buy > possitive_sell )
 		    {
 		      Status_b1  = "ShortPosNettedPartly";
 		      lives_taker1   = negative_buy - possitive_sell;
 		      Status_s1  = "LongPosNetted";
 		      lives_maker1   = 0;
 		      nCouldBuy1 = lives_taker1;

 		      Status_b2  = "ShortPosNetted";
 		      lives_taker2   = 0;
 		      Status_s2  = "OpenShortPosition";
 		      lives_maker2   = negative_buy - possitive_sell;
 		      nCouldBuy2 = negative_buy - possitive_sell;

 		      Status_b3  = "OpenLongPosition";
 		      lives_taker3   = nCouldBuy - negative_buy;
 		      Status_s3  = "ShortPosIncreased";
 		      lives_maker3   = lives_maker2 + lives_taker3;
 		      nCouldBuy3 = lives_taker3;

 		    }
 		  else if ( negative_buy == possitive_sell )
 		    {
 		      Status_b1  = "ShortPosNetted";
 		      lives_taker1   = 0;
 		      Status_s1  = "LongPosNetted";
 		      lives_maker1   = 0;
 		      nCouldBuy1 = possitive_sell;

 		      Status_b2  = "OpenLongPosition";
 		      lives_taker2   = nCouldBuy - possitive_sell;
 		      Status_s2  = "OpenShortPosition";
 		      lives_maker2   = lives_taker2;
 		      nCouldBuy2 = lives_taker2;
 		    }
 		}
 	      else if ( Status_maker == "LongPosNettedPartly" )
 		{
 		  Status_b1  = "ShortPosNetted";
 		  lives_taker1   = 0;
 		  Status_s1  = "LongPosNettedPartly";
 		  lives_maker1   = possitive_sell - negative_buy;
 		  nCouldBuy1 = negative_buy;

 		  Status_b2  = "OpenLongPosition";
 		  lives_taker2   = nCouldBuy - negative_buy;
 		  Status_s2  = "LongPosNettedPartly";
 		  lives_maker2   = lives_maker1 - lives_taker2;
 		  nCouldBuy2 = lives_taker2;

 		}
 	      else if ( Status_maker == "LongPosNetted" )
 		{
 		  Status_b1  = "ShortPosNetted";
 		  lives_taker1   = 0;
 		  Status_s1  = "LongPosNettedPartly";
 		  lives_maker1   = possitive_sell - negative_buy;
 		  nCouldBuy1 = negative_buy;

 		  Status_b2  = "OpenLongPosition";
 		  lives_taker2   = nCouldBuy - negative_buy;
 		  Status_s2  = "LongPosNetted";
 		  lives_maker2   = 0;
 		  nCouldBuy2 = lives_taker2;

 		}
 	      else if ( Status_maker == "OpenShortPosition" )
 		{
 		  Status_b1  = "ShortPosNetted";
 		  lives_taker1   = 0;
 		  Status_s1  = "OpenShortPosition";
 		  lives_maker1   = negative_buy;
 		  nCouldBuy1 = negative_buy;

 		  Status_b2  = "OpenLongPosition";
 		  lives_taker2   = nCouldBuy - negative_buy;
 		  Status_s2  = "ShortPosIncreased";
 		  lives_maker2   = lives_maker1 + lives_taker2;
 		  nCouldBuy2 = lives_taker2;

 		}
 	      else if ( Status_maker == "ShortPosIncreased" )
 		{
 		  Status_b1  = "ShortPosNetted";
 		  lives_taker1   = 0;
 		  Status_s1  = "ShortPosIncreased";
 		  lives_maker1   = negative_sell + negative_buy;
 		  nCouldBuy1 = negative_buy;

 		  Status_b2  = "OpenLongPosition";
 		  lives_taker2   = nCouldBuy - negative_buy;
 		  Status_s2  = "ShortPosIncreased";
 		  lives_maker2   = lives_maker1 + lives_taker2;
 		  nCouldBuy2 = lives_taker2;
 		}
 	    }
 	  // Checked
 	}
       /********************************************************************/
       std::string Status_maker0="EmptyStr", Status_maker1="EmptyStr", Status_maker2="EmptyStr", Status_maker3="EmptyStr";
       std::string Status_taker0="EmptyStr", Status_taker1="EmptyStr", Status_taker2="EmptyStr", Status_taker3="EmptyStr";

       std::vector<std::string> v_status;
       std::vector<int64_t> v_livesc;
       std::vector<int64_t> v_ncouldbuy;

       v_ncouldbuy.push_back(nCouldBuy0);
       v_ncouldbuy.push_back(nCouldBuy1);
       v_ncouldbuy.push_back(nCouldBuy2);
       v_ncouldbuy.push_back(nCouldBuy3);

       v_livesc.push_back(lives_maker0);
       v_livesc.push_back(lives_taker0);
       v_livesc.push_back(lives_maker1);
       v_livesc.push_back(lives_taker1);
       v_livesc.push_back(lives_maker2);
       v_livesc.push_back(lives_taker2);
       v_livesc.push_back(lives_maker3);
       v_livesc.push_back(lives_taker3);

       if ( pold->getAddr() == seller_address )
 	{
 	  v_status.push_back(Status_s);
 	  v_status.push_back(Status_b);
 	  v_status.push_back(Status_s1);
 	  v_status.push_back(Status_b1);
 	  v_status.push_back(Status_s2);
 	  v_status.push_back(Status_b2);
 	  v_status.push_back(Status_s3);
 	  v_status.push_back(Status_b3);

 	}
       else
 	{
 	  v_status.push_back(Status_b);
 	  v_status.push_back(Status_s);
 	  v_status.push_back(Status_b1);
 	  v_status.push_back(Status_s1);
 	  v_status.push_back(Status_b2);
 	  v_status.push_back(Status_s2);
 	  v_status.push_back(Status_b3);
 	  v_status.push_back(Status_s3);
 	}

       Status_maker0 = Status_maker;
       Status_taker0 = Status_taker;

       if ( pold->getAddr() == seller_address )
 	{
 	  Status_maker1 = Status_s1;
 	  Status_taker1 = Status_b1;
 	  Status_maker2 = Status_s2;
 	  Status_taker2 = Status_b2;
 	  Status_maker3 = Status_s3;
 	  Status_taker3 = Status_b3;
 	}
       else
 	{
 	  Status_maker1 = Status_b1;
 	  Status_taker1 = Status_s1;
 	  Status_maker2 = Status_b2;
 	  Status_taker2 = Status_s2;
 	  Status_maker3 = Status_b3;
 	  Status_taker3 = Status_s3;
 	}

   /**
    * Fees calculations for maker and taker.
    *
    *
    */
     // mastercore::ContractDex_Fees(pnew->getAddr(),pold->getAddr(), nCouldBuy, property_traded);

     if(msc_debug_x_trade_bidirectional)
     {
         PrintToLog("Checking all parameters inside recordMatchedTrade:\n");
         PrintToLog("txmaker: %s, txtaker: %s, makeraddress: %s, takeraddress: %s, price: %d, maker_crgafs: %d\n", pold->getHash().ToString(), pnew->getHash().ToString(), pold->getAddr(), pnew->getAddr(), pold->getEffectivePrice(),contract_replacement.getAmountForSale());
         PrintToLog("takergetAmounForSale: %d, makerblock: %d, takerblock: %d, property: %d, tradestatus: %s\n", pnew->getAmountForSale(), pold->getBlock(), pnew->getBlock(), property_traded, tradeStatus);
         PrintToLog("lives_maker0: %d, lives_maker1: %d, lives_maker2: %d, lives_maker3: %d, lives_taker0: %d, lives_taker1: %d, lives_taker2: %d, lives_taker3:%d\n",lives_maker0, lives_maker1, lives_maker2, lives_maker3, lives_taker0, lives_taker1, lives_taker2, lives_taker3);
         PrintToLog("Status_maker0: %d, Status_taker0: %d, Status_maker1: %d, Status_taker1: %d, Status_maker2: %d, Status_taker2: %d, Status_maker3: %d, Status_taker3:%d\n",Status_maker0, Status_taker0, Status_maker1, Status_taker1, Status_maker2, Status_taker2, Status_maker3, Status_taker3);
         PrintToLog("nCouldBuy0: %d, nCouldBuy1: %d, nCouldBuy2: %d, nCouldBuy3: %d, amountpnew: %d, amountpold: %d\n",nCouldBuy0, nCouldBuy1, nCouldBuy2, nCouldBuy3, amountpnew, amountpold);
     }
    /********************************************************/
    // pDbTradeList->recordMatchedTrade(pold->getHash(),
 		// 			pnew->getHash(),
 		// 			pold->getAddr(),
 		// 			pnew->getAddr(),
 		// 			pold->getEffectivePrice(),
 		// 			contract_replacement.getAmountForSale(),
 		// 			pnew->getAmountForSale(),
 		// 			pold->getBlock(),
 		// 			pnew->getBlock(),
 		// 			property_traded,
 		// 			tradeStatus,
 		// 			lives_maker0,
 		// 			lives_maker1,
 		// 			lives_maker2,
 		// 			lives_maker3,
 		// 			lives_taker0,
 		// 			lives_taker1,
 		// 			lives_taker2,
 		// 			lives_taker3,
 		// 			Status_maker0,
 		// 			Status_taker0,
 		// 			Status_maker1,
 		// 			Status_taker1,
 		// 			Status_maker2,
 		// 			Status_taker2,
 		// 			Status_maker3,
 		// 			Status_taker3,
 		// 			nCouldBuy0,
 		// 			nCouldBuy1,
 		// 			nCouldBuy2,
 		// 			nCouldBuy3,
 		// 			amountpnew,
 		// 			amountpold);
           /********************************************************/
           int index = static_cast<unsigned int>(property_traded);

           marketP[index] = pold->getEffectivePrice();
           // uint64_t marketPriceNow = marketP[index];
           if(msc_debug_x_trade_bidirectional) PrintToLog("%s: marketP[index] = %d\n",__func__, marketP[index]);
           // t_tradelistdb->recordForUPNL(pnew->getHash(),pnew->getAddr(),property_traded,pold->getEffectivePrice());

           if(msc_debug_x_trade_bidirectional) PrintToLog("++ erased old: %s\n", offerIt->ToString());
           pofferSet->erase(offerIt++);

           if (0 < remaining)
 	            pofferSet->insert(contract_replacement);
       }
 }

 void CMPMetaDEx::setAmountForsale(int64_t amount, const std::string& label)
 {
     amount_forsale = amount;
     PrintToLog("update remaining amount still up for sale (%ld %s):%s\n", amount, label, ToString());
 }

 void CMPContractDex::setPrice(int64_t price)
 {
     effective_price = price;
     // PrintToLog("update price still up for sale (%ld):%s\n", price, ToString());
 }

 bool ContractDex_compare::operator()(const CMPContractDex &lhs, const CMPContractDex &rhs) const
 {
     if (lhs.getBlock() == rhs.getBlock()) return lhs.getIdx() < rhs.getIdx();
     else return lhs.getBlock() < rhs.getBlock();
 }

 std::string CMPContractDex::ToString() const
 {
     return strprintf("%s:%34s in %d/%03u, txid: %s , trade #%u %s for #%u %s",
         xToString(getEffectivePrice()), getAddr(), getBlock(), getIdx(), getHash().ToString().substr(0, 10),
         getProperty(), FormatMP(getProperty(), getAmountForSale()));
 }
