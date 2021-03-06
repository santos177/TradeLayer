#include <tradelayer/rpcrequirements.h>
#include <tradelayer/uint256_extensions.h>
#include <tradelayer/mdex.h>
#include <tradelayer/dbspinfo.h>
#include <tradelayer/dbtxlist.h>
#include <tradelayer/dex.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/sp.h>
#include <tradelayer/utilsbitcoin.h>

#include <boost/algorithm/string.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>

#include <amount.h>
#include <validation.h>
#include <rpc/protocol.h>
#include <sync.h>
#include <tinyformat.h>

#include <stdint.h>
#include <string>

using boost::algorithm::token_compress_on;
typedef boost::rational<boost::multiprecision::checked_int128_t> rational_t;
extern int64_t factorE;
// extern uint64_t marketP[NPTYPES];
extern int lastBlockg;
extern int vestingActivationBlock;

void RequireBalance(const std::string& address, uint32_t propertyId, int64_t amount)
{
    int64_t balance = GetTokenBalance(address, propertyId, BALANCE);
    if (balance < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance");
    }
    int64_t balanceUnconfirmed = GetAvailableTokenBalance(address, propertyId);
    if (balanceUnconfirmed < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance (due to pending transactions)");
    }
}

void RequirePrimaryToken(uint32_t propertyId)
{
    if (propertyId < 1 || 2 < propertyId) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier must be 1 (OMN) or 2 (TOMN)");
    }
}

void RequirePropertyName(const std::string& name)
{
    if (name.empty()) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Property name must not be empty");
    }
}

void RequireExistingProperty(uint32_t propertyId)
{
    LOCK(cs_tally);
    if (!mastercore::IsPropertyIdValid(propertyId)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not exist");
    }
}

void RequireSameEcosystem(uint32_t propertyId, uint32_t otherId)
{
    if (mastercore::isTestEcosystemProperty(propertyId) != mastercore::isTestEcosystemProperty(otherId)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Properties must be in the same ecosystem");
    }
}

void RequireDifferentIds(uint32_t propertyId, uint32_t otherId)
{
    if (propertyId == otherId) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifiers must not be the same");
    }
}

void RequireManagedProperty(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::pDbSpInfo->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }
    if (sp.fixed || !sp.manual) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property identifier does not refer to a managed property");
    }
}

void RequireTokenIssuer(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::pDbSpInfo->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }
    if (address != sp.issuer) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender is not authorized to manage the property");
    }
}

void RequireMatchingDExOffer(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    if (!mastercore::DEx_offerExists(address, propertyId)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "No matching sell offer on the distributed exchange");
    }
}

void RequireNoOtherDExOffer(const std::string& address)
{
    LOCK(cs_tally);
    if (mastercore::DEx_hasOffer(address)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Another active sell offer from the given address already exists on the distributed exchange");
    }
}

void RequireSaneReferenceAmount(int64_t amount)
{
    if ((0.01 * COIN) < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Reference amount higher is than 0.01 BTC");
    }
}

void RequireSaneDExPaymentWindow(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    const CMPOffer* poffer = mastercore::DEx_getOffer(address, propertyId);
    if (poffer == nullptr) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Unable to load sell offer from the distributed exchange");
    }
    if (poffer->getBlockTimeLimit() < 10) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Payment window is less than 10 blocks (use override = true to continue)");
    }
}

void RequireSaneDExFee(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    const CMPOffer* poffer = mastercore::DEx_getOffer(address, propertyId);
    if (poffer == nullptr) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Unable to load sell offer from the distributed exchange");
    }
    if (poffer->getMinFee() > 1000000) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Minimum accept fee is higher than 0.01 BTC (use override = true to continue)");
    }
}

void RequireHeightInChain(int blockHeight)
{
    if (blockHeight < 0 || mastercore::GetHeight() < blockHeight) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Block height is out of range");
    }
}

void RequireContractTxId(std::string& txid)
{
    std::string result;
    uint256 tx;
    tx.SetHex(txid);
    if (!mastercore::pDbTransactionList->getTX(tx, result)) {
          throw JSONRPCError(RPC_INVALID_PARAMETER, "TxId doesn't exist\n");
    }

    std::vector<std::string> vstr;
    boost::split(vstr, result, boost::is_any_of(":"), token_compress_on);
    unsigned int type = atoi(vstr[2]);
    if (type != 29) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "TxId isn't a future contract trade\n");
    }

}

void RequireSaneName(std::string& name)
{
    LOCK(cs_tally);
    uint32_t nextSPID = mastercore::pDbSpInfo->peekNextSPID(1);
    for (uint32_t propertyId = 1; propertyId < nextSPID; propertyId++) {
        CMPSPInfo::Entry sp;
        if (mastercore::pDbSpInfo->getSP(propertyId, sp)) {
            PrintToConsole("Property Id: %d\n",propertyId);
            if (sp.name == name){
                throw JSONRPCError(RPC_INVALID_PARAMETER,"We have another property with the same name\n");
            }
        }
    }

}

void RequireContract(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::pDbSpInfo->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }
    if (sp.prop_type != ALL_PROPERTY_TYPE_CONTRACT) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "contractId must be future contract\n");
    }
}

void RequireOracleContract(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::pDbSpInfo->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }

    if (sp.prop_type !=ALL_PROPERTY_TYPE_ORACLE_CONTRACT) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "contractId must be oracle future contract\n");
    }
}

void RequireContractOrder(std::string& fromAddress, uint32_t contractId)
{
  LOCK(cs_tally);
  bool found = false;
  for (mastercore::cd_PropertiesMap::const_iterator my_it = mastercore::contractdex.begin(); my_it != mastercore::contractdex.end(); ++my_it) {
      const mastercore::cd_PricesMap& prices = my_it->second;
      for (mastercore::cd_PricesMap::const_iterator it = prices.begin(); it != prices.end(); ++it) {
          const mastercore::cd_Set& indexes = it->second;
          for (mastercore::cd_Set::const_iterator it = indexes.begin(); it != indexes.end(); ++it) {
              const CMPContractDex& obj = *it;
              if (obj.getProperty() != contractId || obj.getAddr() != fromAddress) continue;
              PrintToLog("Order found!\n");
              found = true;
              break;
          }
      }
  }

  if (!found) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,"There's no order in this future contract\n");
  }

}


void RequireNoOtherDExOffer(const std::string& address, uint32_t propertyId)
{
    LOCK(cs_tally);
    if (mastercore::DEx_offerExists(address, propertyId)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Another active sell offer from the given address already exists on the distributed exchange");
    }
}

void RequirePeggedSaneName(std::string& name)
{
    if (name.empty()) { throw JSONRPCError(RPC_INVALID_PARAMETER,"Pegged need a name!\n"); }
}

void RequireNotContract(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::pDbSpInfo->getSP(propertyId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }
    if (sp.prop_type == ALL_PROPERTY_TYPE_CONTRACT) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Property must not be future contract\n");
    }
}

void RequireShort(std::string& fromAddress, uint32_t contractId, uint64_t amount)
{
    LOCK(cs_tally);
    int64_t contractsNeeded = 0;
    // int index = static_cast<int>(contractId);

    CMPSPInfo::Entry sp;
    if (!mastercore::pDbSpInfo->getSP(contractId, sp)) {
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }

    if (sp.prop_type == ALL_PROPERTY_TYPE_CONTRACT) {
        int64_t notionalSize = static_cast<int64_t>(sp.notional_size);
        int64_t position = GetTokenBalance(fromAddress, contractId, NEGATIVE_BALANCE);
        // rational_t conv = mastercore::notionalChange(contractId);
        rational_t conv = rational_t(1,1);
        int64_t num = conv.numerator().convert_to<int64_t>();
        int64_t denom = conv.denominator().convert_to<int64_t>();
        arith_uint256 Amount = mastercore::ConvertTo256(num) * mastercore::ConvertTo256(amount) / mastercore::ConvertTo256(denom); // Alls needed
        arith_uint256 contracts = mastercore::DivideAndRoundUp(Amount * mastercore::ConvertTo256(notionalSize), mastercore::ConvertTo256(1)) ;
        contractsNeeded = mastercore::ConvertTo64(contracts);
        if (contractsNeeded > position) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Not enough short position\n");
        }

    } else  {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "contractId must be future contract\n");
    }
}

void RequireForPegged(const std::string& address, uint32_t propertyId, uint32_t contractId, uint64_t amount)
{
    uint64_t balance = static_cast<uint64_t>(GetTokenBalance(address, propertyId, BALANCE));
    int64_t position = GetTokenBalance(address, contractId, NEGATIVE_BALANCE);
    if (balance < amount) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has not enough amount of collateral currency");
    }

    if (position == 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Sender has not the short position required on this contract");
    }
    // int64_t balanceUnconfirmed = getUserAvailableMPbalance(address, propertyId); // check the pending amounts
    // if (balanceUnconfirmed > 0) {
    //     throw JSONRPCError(RPC_TYPE_ERROR, "Sender has insufficient balance (due to pending transactions)");
    // }


}

void RequirePeggedCurrency(uint32_t propertyId)
{
    LOCK(cs_tally);
    CMPSPInfo::Entry sp;
    if (!mastercore::pDbSpInfo->getSP(propertyId, sp)) {
      throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
    }

    if (sp.prop_type != ALL_PROPERTY_TYPE_PEGGEDS) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "propertyId must be a pegged currency\n");
    }
}

void RequireAssociation(uint32_t propertyId,uint32_t contractId)
{
  LOCK(cs_tally);
  CMPSPInfo::Entry sp;
  if (!mastercore::pDbSpInfo->getSP(propertyId, sp)) {
      throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to retrieve property");
  }
  if (sp.contract_associated != contractId) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Pegged does not comes from this contract");
  }
}
