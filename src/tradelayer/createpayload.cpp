// This file serves to provide payload creation functions.

#include <tradelayer/createpayload.h>

#include <tradelayer/log.h>
#include <tradelayer/parsing.h>

#include <base58.h>

#include <stdint.h>
#include <string>
#include <vector>

/**
 * Pushes bytes to the end of a vector.
 */
#define PUSH_BACK_BYTES(vector, value)\
    vector.insert(vector.end(), reinterpret_cast<unsigned char *>(&(value)),\
    reinterpret_cast<unsigned char *>(&(value)) + sizeof((value)));

/**
 * Pushes bytes to the end of a vector based on a pointer.
 */
#define PUSH_BACK_BYTES_PTR(vector, ptr, size)\
    vector.insert(vector.end(), reinterpret_cast<unsigned char *>((ptr)),\
    reinterpret_cast<unsigned char *>((ptr)) + (size));

/**
 * Returns a vector of bytes containing the version and hash160 for an address.
 */
static std::vector<unsigned char> AddressToBytes(const std::string& address)
{
    std::vector<unsigned char> addressBytes;
    bool success = DecodeBase58(address, addressBytes);
    if (!success) {
        PrintToLog("ERROR: failed to decode address %s.\n", address);
    }
    if (addressBytes.size() == 25) {
        addressBytes.resize(21); // truncate checksum
    } else {
        PrintToLog("ERROR: unexpected size from DecodeBase58 when decoding address %s.\n", address);
    }

    return addressBytes;
}

std::vector<unsigned char> CreatePayload_SimpleSend(uint32_t propertyId, uint64_t amount)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 0;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);
    SwapByteOrder64(amount);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, amount);

    return payload;
}

std::vector<unsigned char> CreatePayload_SendAll(uint8_t ecosystem)
{
    std::vector<unsigned char> payload;
    uint16_t messageVer = 0;
    uint16_t messageType = 4;
    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, ecosystem);

    return payload;
}

std::vector<unsigned char> CreatePayload_DExSell(uint32_t propertyId, uint64_t amountForSale, uint64_t amountDesired, uint8_t timeLimit, uint64_t minFee, uint8_t subAction)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 20;
    uint16_t messageVer = 1;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);
    SwapByteOrder64(amountForSale);
    SwapByteOrder64(amountDesired);
    SwapByteOrder64(minFee);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, amountForSale);
    PUSH_BACK_BYTES(payload, amountDesired);
    PUSH_BACK_BYTES(payload, timeLimit);
    PUSH_BACK_BYTES(payload, minFee);
    PUSH_BACK_BYTES(payload, subAction);

    return payload;
}

std::vector<unsigned char> CreatePayload_DExAccept(uint32_t propertyId, uint64_t amount)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 22;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);
    SwapByteOrder64(amount);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, amount);

    return payload;
}

std::vector<unsigned char> CreatePayload_SendToOwners(uint32_t propertyId, uint64_t amount, uint32_t distributionProperty)
{
    bool v0 = (propertyId == distributionProperty) ? true : false;

    std::vector<unsigned char> payload;

    uint16_t messageType = 3;
    uint16_t messageVer = (v0) ? 0 : 1;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);
    SwapByteOrder64(amount);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, amount);
    if (!v0) {
        SwapByteOrder32(distributionProperty);
        PUSH_BACK_BYTES(payload, distributionProperty);
    }

    return payload;
}

std::vector<unsigned char> CreatePayload_IssuanceFixed(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId, std::string category,
                                                       std::string subcategory, std::string name, std::string url, std::string data, uint64_t amount)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 50;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);
    SwapByteOrder16(propertyType);
    SwapByteOrder32(previousPropertyId);
    SwapByteOrder64(amount);
    if (category.size() > 255) category = category.substr(0,255);
    if (subcategory.size() > 255) subcategory = subcategory.substr(0,255);
    if (name.size() > 255) name = name.substr(0,255);
    if (url.size() > 255) url = url.substr(0,255);
    if (data.size() > 255) data = data.substr(0,255);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, ecosystem);
    PUSH_BACK_BYTES(payload, propertyType);
    PUSH_BACK_BYTES(payload, previousPropertyId);
    payload.insert(payload.end(), category.begin(), category.end());
    payload.push_back('\0');
    payload.insert(payload.end(), subcategory.begin(), subcategory.end());
    payload.push_back('\0');
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), url.begin(), url.end());
    payload.push_back('\0');
    payload.insert(payload.end(), data.begin(), data.end());
    payload.push_back('\0');
    PUSH_BACK_BYTES(payload, amount);

    return payload;
}

std::vector<unsigned char> CreatePayload_IssuanceVariable(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId, std::string category,
                                                          std::string subcategory, std::string name, std::string url, std::string data, uint32_t propertyIdDesired,
                                                          uint64_t amountPerUnit, uint64_t deadline, uint8_t earlyBonus, uint8_t issuerPercentage)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 51;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);
    SwapByteOrder16(propertyType);
    SwapByteOrder32(previousPropertyId);
    SwapByteOrder32(propertyIdDesired);
    SwapByteOrder64(amountPerUnit);
    SwapByteOrder64(deadline);
    if (category.size() > 255) category = category.substr(0,255);
    if (subcategory.size() > 255) subcategory = subcategory.substr(0,255);
    if (name.size() > 255) name = name.substr(0,255);
    if (url.size() > 255) url = url.substr(0,255);
    if (data.size() > 255) data = data.substr(0,255);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, ecosystem);
    PUSH_BACK_BYTES(payload, propertyType);
    PUSH_BACK_BYTES(payload, previousPropertyId);
    payload.insert(payload.end(), category.begin(), category.end());
    payload.push_back('\0');
    payload.insert(payload.end(), subcategory.begin(), subcategory.end());
    payload.push_back('\0');
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), url.begin(), url.end());
    payload.push_back('\0');
    payload.insert(payload.end(), data.begin(), data.end());
    payload.push_back('\0');
    PUSH_BACK_BYTES(payload, propertyIdDesired);
    PUSH_BACK_BYTES(payload, amountPerUnit);
    PUSH_BACK_BYTES(payload, deadline);
    PUSH_BACK_BYTES(payload, earlyBonus);
    PUSH_BACK_BYTES(payload, issuerPercentage);

    return payload;
}

std::vector<unsigned char> CreatePayload_IssuanceManaged(uint8_t ecosystem, uint16_t propertyType, uint32_t previousPropertyId, std::string category,
                                                       std::string subcategory, std::string name, std::string url, std::string data)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 54;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);
    SwapByteOrder16(propertyType);
    SwapByteOrder32(previousPropertyId);
    if (category.size() > 255) category = category.substr(0,255);
    if (subcategory.size() > 255) subcategory = subcategory.substr(0,255);
    if (name.size() > 255) name = name.substr(0,255);
    if (url.size() > 255) url = url.substr(0,255);
    if (data.size() > 255) data = data.substr(0,255);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, ecosystem);
    PUSH_BACK_BYTES(payload, propertyType);
    PUSH_BACK_BYTES(payload, previousPropertyId);
    payload.insert(payload.end(), category.begin(), category.end());
    payload.push_back('\0');
    payload.insert(payload.end(), subcategory.begin(), subcategory.end());
    payload.push_back('\0');
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    payload.insert(payload.end(), url.begin(), url.end());
    payload.push_back('\0');
    payload.insert(payload.end(), data.begin(), data.end());
    payload.push_back('\0');

    return payload;
}

std::vector<unsigned char> CreatePayload_CloseCrowdsale(uint32_t propertyId)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 53;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);

    return payload;
}

std::vector<unsigned char> CreatePayload_Grant(uint32_t propertyId, uint64_t amount, std::string memo)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 55;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);
    SwapByteOrder64(amount);
    if (memo.size() > 255) memo = memo.substr(0,255);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, amount);
    payload.insert(payload.end(), memo.begin(), memo.end());
    payload.push_back('\0');

    return payload;
}


std::vector<unsigned char> CreatePayload_Revoke(uint32_t propertyId, uint64_t amount, std::string memo)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 56;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);
    SwapByteOrder64(amount);
    if (memo.size() > 255) memo = memo.substr(0,255);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, amount);
    payload.insert(payload.end(), memo.begin(), memo.end());
    payload.push_back('\0');

    return payload;
}

std::vector<unsigned char> CreatePayload_ChangeIssuer(uint32_t propertyId)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 70;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);

    return payload;
}

std::vector<unsigned char> CreatePayload_EnableFreezing(uint32_t propertyId)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 71;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);

    return payload;
}

std::vector<unsigned char> CreatePayload_DisableFreezing(uint32_t propertyId)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 72;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);

    return payload;
}

std::vector<unsigned char> CreatePayload_FreezeTokens(uint32_t propertyId, uint64_t amount, const std::string& address)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 185;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);
    SwapByteOrder64(amount);
    std::vector<unsigned char> addressBytes = AddressToBytes(address);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, amount);
    payload.insert(payload.end(), addressBytes.begin(), addressBytes.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_UnfreezeTokens(uint32_t propertyId, uint64_t amount, const std::string& address)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 186;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);
    SwapByteOrder32(propertyId);
    SwapByteOrder64(amount);
    std::vector<unsigned char> addressBytes = AddressToBytes(address);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, amount);
    payload.insert(payload.end(), addressBytes.begin(), addressBytes.end());

    return payload;
}

std::vector<unsigned char> CreatePayload_MetaDExTrade(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 25;
    uint16_t messageVer = 0;

    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);
    SwapByteOrder32(propertyIdForSale);
    SwapByteOrder64(amountForSale);
    SwapByteOrder32(propertyIdDesired);
    SwapByteOrder64(amountDesired);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyIdForSale);
    PUSH_BACK_BYTES(payload, amountForSale);
    PUSH_BACK_BYTES(payload, propertyIdDesired);
    PUSH_BACK_BYTES(payload, amountDesired);

    return payload;
}

std::vector<unsigned char> CreatePayload_MetaDExCancelPrice(uint32_t propertyIdForSale, uint64_t amountForSale, uint32_t propertyIdDesired, uint64_t amountDesired)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 26;
    uint16_t messageVer = 0;

    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);
    SwapByteOrder32(propertyIdForSale);
    SwapByteOrder64(amountForSale);
    SwapByteOrder32(propertyIdDesired);
    SwapByteOrder64(amountDesired);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyIdForSale);
    PUSH_BACK_BYTES(payload, amountForSale);
    PUSH_BACK_BYTES(payload, propertyIdDesired);
    PUSH_BACK_BYTES(payload, amountDesired);

    return payload;
}

std::vector<unsigned char> CreatePayload_MetaDExCancelPair(uint32_t propertyIdForSale, uint32_t propertyIdDesired)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 27;
    uint16_t messageVer = 0;

    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);
    SwapByteOrder32(propertyIdForSale);
    SwapByteOrder32(propertyIdDesired);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyIdForSale);
    PUSH_BACK_BYTES(payload, propertyIdDesired);

    return payload;
}

std::vector<unsigned char> CreatePayload_MetaDExCancelEcosystem(uint8_t ecosystem)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 28;
    uint16_t messageVer = 0;

    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, ecosystem);

    return payload;
}

std::vector<unsigned char> CreatePayload_DeactivateFeature(uint16_t featureId)
{
    std::vector<unsigned char> payload;

    uint16_t messageVer = 65535;
    uint16_t messageType = 65533;

    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);
    SwapByteOrder16(featureId);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, featureId);

    return payload;
}

std::vector<unsigned char> CreatePayload_ActivateFeature(uint16_t featureId, uint32_t activationBlock, uint32_t minClientVersion)
{
    std::vector<unsigned char> payload;

    uint16_t messageVer = 65535;
    uint16_t messageType = 65534;

    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);
    SwapByteOrder16(featureId);
    SwapByteOrder32(activationBlock);
    SwapByteOrder32(minClientVersion);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, featureId);
    PUSH_BACK_BYTES(payload, activationBlock);
    PUSH_BACK_BYTES(payload, minClientVersion);

    return payload;
}

std::vector<unsigned char> CreatePayload_TradelayerAlert(uint16_t alertType, uint32_t expiryValue, const std::string& alertMessage)
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 65535;
    uint16_t messageVer = 65535;

    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);
    SwapByteOrder16(alertType);
    SwapByteOrder32(expiryValue);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, alertType);
    PUSH_BACK_BYTES(payload, expiryValue);
    payload.insert(payload.end(), alertMessage.begin(), alertMessage.end());
    payload.push_back('\0');

    return payload;
}

std::vector<unsigned char> CreatePayload_DEx_Payment()
{
    std::vector<unsigned char> payload;
    uint16_t messageType = 117;
    uint16_t messageVer = 0;
    SwapByteOrder16(messageType);
    SwapByteOrder16(messageVer);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);

    return payload;
}

std::vector<unsigned char> CreatePayload_SendVestingTokens(uint32_t propertyId, uint64_t amount)
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 5;
  uint16_t messageVer = 0;

  SwapByteOrder16(messageType);
  SwapByteOrder16(messageVer);
  SwapByteOrder32(propertyId);
  SwapByteOrder64(amount);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);
  PUSH_BACK_BYTES(payload, propertyId);
  PUSH_BACK_BYTES(payload, amount);

  return payload;
}

std::vector<unsigned char> CreatePayload_CreateContract(uint8_t ecosystem, uint32_t denomType, std::string name, uint32_t blocks_until_expiration, uint32_t notional_size, uint32_t collateral_currency, uint32_t margin_requirement)
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 41;
  uint16_t messageVer = 0;

  SwapByteOrder16(messageVer);
  SwapByteOrder16(messageType);
  SwapByteOrder32(blocks_until_expiration);
  SwapByteOrder32(notional_size);
  SwapByteOrder32(collateral_currency);
  SwapByteOrder32(margin_requirement);

  if (name.size() > 255) name = name.substr(0,255);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);

  PUSH_BACK_BYTES(payload, ecosystem);
  PUSH_BACK_BYTES(payload, blocks_until_expiration);
  PUSH_BACK_BYTES(payload, notional_size);
  PUSH_BACK_BYTES(payload, collateral_currency);
  PUSH_BACK_BYTES(payload, margin_requirement);
  payload.insert(payload.end(), name.begin(), name.end());
  payload.push_back('\0');


  return payload;
}

std::vector<unsigned char> CreatePayload_CreateOracleContract(uint8_t ecosystem, uint32_t denomType, std::string name, uint32_t blocks_until_expiration, uint32_t notional_size, uint32_t collateral_currency, uint32_t margin_requirement)
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 103;
  uint16_t messageVer = 0;

  SwapByteOrder16(messageVer);
  SwapByteOrder16(messageType);
  SwapByteOrder32(blocks_until_expiration);
  SwapByteOrder32(notional_size);
  SwapByteOrder32(collateral_currency);
  SwapByteOrder32(margin_requirement);

  if (name.size() > 255) name = name.substr(0,255);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);

  PUSH_BACK_BYTES(payload, ecosystem);
  PUSH_BACK_BYTES(payload, blocks_until_expiration);
  PUSH_BACK_BYTES(payload, notional_size);
  PUSH_BACK_BYTES(payload, collateral_currency);
  PUSH_BACK_BYTES(payload, margin_requirement);
  payload.insert(payload.end(), name.begin(), name.end());
  payload.push_back('\0');

  return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexTrade(std::string name_traded, uint64_t amountForSale, uint64_t effective_price, uint8_t trading_action, uint64_t leverage)
{
  std::vector<unsigned char> payload;

  uint16_t messageVer = 0;
  uint16_t messageType = 29;

  SwapByteOrder16(messageVer);
  SwapByteOrder16(messageType);
  SwapByteOrder64(amountForSale);
  SwapByteOrder64(effective_price);
  SwapByteOrder64(leverage);

  if (name_traded.size() > 255) name_traded = name_traded.substr(0,255);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);

  PUSH_BACK_BYTES(payload, amountForSale);
  PUSH_BACK_BYTES(payload, effective_price);
  PUSH_BACK_BYTES(payload, leverage);
  PUSH_BACK_BYTES(payload, trading_action);

  payload.insert(payload.end(), name_traded.begin(), name_traded.end());
  payload.push_back('\0');

  return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexCancelEcosystem(uint8_t ecosystem, uint32_t contractId)
{
  std::vector<unsigned char> payload;

    uint16_t messageType = 32;
    uint16_t messageVer = 0;

    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);

    PUSH_BACK_BYTES(payload, ecosystem);

    return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexClosePosition(uint8_t ecosystem, uint32_t contractId)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 33;
    uint16_t messageVer = 0;

    SwapByteOrder16(messageVer);
    SwapByteOrder16(messageType);
    SwapByteOrder32(contractId);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);

    PUSH_BACK_BYTES(payload, ecosystem);
    PUSH_BACK_BYTES(payload, contractId);

    return payload;
}

std::vector<unsigned char> CreatePayload_ContractDexCancelOrderByTxId(int block, unsigned int idx)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 34;
    uint16_t messageVer = 0;

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);

    PUSH_BACK_BYTES(payload, block);
    PUSH_BACK_BYTES(payload,idx);

    return payload;
}

/* Tx 104 */
std::vector<unsigned char> CreatePayload_Change_OracleRef(uint32_t contractId)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 104;
    uint16_t messageVer = 0;

    SwapByteOrder32(contractId);
    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, contractId);

    return payload;
}

/* Tx 105 */
std::vector<unsigned char> CreatePayload_Set_Oracle(uint32_t contractId, uint64_t high, uint64_t low)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 105;
    uint16_t messageVer = 0;

    SwapByteOrder32(contractId);
    SwapByteOrder64(low);
    SwapByteOrder64(high);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, contractId);
    PUSH_BACK_BYTES(payload, low);
    PUSH_BACK_BYTES(payload, high);

    return payload;
}

std::vector<unsigned char> CreatePayload_OracleBackup(uint32_t contractId)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 106;
    uint16_t messageVer = 0;

    SwapByteOrder32(contractId);
    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, contractId);

    return payload;
}

std::vector<unsigned char> CreatePayload_Close_Oracle(uint32_t contractId)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 107;
    uint16_t messageVer = 0;

    SwapByteOrder32(contractId);
    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, contractId);

    return payload;
}

std::vector<unsigned char> CreatePayload_Commit_Channel(uint32_t propertyId, uint64_t amount)
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 108;
  uint16_t messageVer = 0;

  SwapByteOrder32(propertyId);
  SwapByteOrder64(amount);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);
  PUSH_BACK_BYTES(payload, propertyId);
  PUSH_BACK_BYTES(payload, amount);

  return payload;
}

std::vector<unsigned char> CreatePayload_Withdrawal_FromChannel(uint32_t propertyId, uint64_t amount)
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 109;
  uint16_t messageVer = 0;

  SwapByteOrder32(propertyId);
  SwapByteOrder64(amount);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);
  PUSH_BACK_BYTES(payload, propertyId);
  PUSH_BACK_BYTES(payload, amount);

  return payload;
}

std::vector<unsigned char> CreatePayload_Instant_Trade(uint32_t propertyId, uint64_t amount, uint32_t blockheight_expiry, uint32_t propertyDesired, uint64_t amountDesired)
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 110;
  uint16_t messageVer = 0;

  SwapByteOrder32(propertyId);
  SwapByteOrder64(amount);
  SwapByteOrder32(propertyDesired);
  SwapByteOrder32(blockheight_expiry);
  SwapByteOrder64(amountDesired);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);
  PUSH_BACK_BYTES(payload, propertyId);
  PUSH_BACK_BYTES(payload, amount);
  PUSH_BACK_BYTES(payload, blockheight_expiry);
  PUSH_BACK_BYTES(payload, propertyDesired);
  PUSH_BACK_BYTES(payload, amountDesired);

  return payload;
}

std::vector<unsigned char> CreatePayload_Contract_Instant_Trade(uint32_t contractId, uint64_t amount, uint32_t blockheight_expiry, uint64_t price, uint8_t trading_action, uint64_t leverage)
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 114;
  uint16_t messageVer = 0;

  SwapByteOrder32(contractId);
  SwapByteOrder64(amount);
  SwapByteOrder32(blockheight_expiry);
  SwapByteOrder64(price);
  SwapByteOrder64(leverage);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);
  PUSH_BACK_BYTES(payload, contractId);
  PUSH_BACK_BYTES(payload, amount);
  PUSH_BACK_BYTES(payload, blockheight_expiry);
  PUSH_BACK_BYTES(payload, price);
  PUSH_BACK_BYTES(payload, trading_action);
  PUSH_BACK_BYTES(payload, leverage);

  return payload;
}

// std::vector<unsigned char> CreatePayload_PNL_Update(uint32_t propertyId, uint64_t amount, uint32_t blockheight_expiry)
// {
//   std::vector<unsigned char> payload;
//
//   uint64_t messageType = 111;
//   uint64_t messageVer = 0;
//
//   std::vector<uint8_t> vecMessageType = CompressInteger((uint64_t)messageType);
//   std::vector<uint8_t> vecMessageVer = CompressInteger((uint64_t)messageVer);
//   std::vector<uint8_t> vecPropertyId = CompressInteger((uint64_t)propertyId);
//   std::vector<uint8_t> vecAmount = CompressInteger((uint64_t)amount);
//   std::vector<uint8_t> vecBlock = CompressInteger((uint64_t)blockheight_expiry);
//
//   payload.insert(payload.end(), vecMessageVer.begin(), vecMessageVer.end());
//   payload.insert(payload.end(), vecMessageType.begin(), vecMessageType.end());
//   payload.insert(payload.end(), vecPropertyId.begin(), vecPropertyId.end());
//   payload.insert(payload.end(), vecAmount.begin(), vecAmount.end());
//   payload.insert(payload.end(), vecBlock.begin(), vecBlock.end());
//
//   return payload;
// }

std::vector<unsigned char> CreatePayload_Transfer(uint32_t propertyId, uint64_t amount)
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 112;
  uint16_t messageVer = 0;

  SwapByteOrder32(propertyId);
  SwapByteOrder64(amount);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);
  PUSH_BACK_BYTES(payload, propertyId);
  PUSH_BACK_BYTES(payload, amount);

  return payload;
}

std::vector<unsigned char> CreatePayload_Create_Channel(std::string channelAddress, uint32_t blocks)
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 113;
  uint16_t messageVer = 0;

  SwapByteOrder32(blocks);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);
  PUSH_BACK_BYTES(payload, blocks);

  if ((channelAddress).size() > 255) channelAddress = channelAddress.substr(0,255);

  payload.insert(payload.end(), channelAddress.begin(), channelAddress.end());
  payload.push_back('\0');

  return payload;
}

std::vector<unsigned char> CreatePayload_New_Id_Registration(std::string website, std::string name, uint8_t tokens, uint8_t ltc, uint8_t natives, uint8_t oracles)
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 115;
  uint16_t messageVer = 0;

  if ((website).size() > 255) website = website.substr(0,255);
  if ((name).size() > 255) name = name.substr(0,255);

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);
  PUSH_BACK_BYTES(payload, tokens);
  PUSH_BACK_BYTES(payload, ltc);
  PUSH_BACK_BYTES(payload, natives);
  PUSH_BACK_BYTES(payload, oracles);

  payload.insert(payload.end(), website.begin(), website.end());
  payload.push_back('\0');
  payload.insert(payload.end(), name.begin(), name.end());
  payload.push_back('\0');

  return payload;
}

std::vector<unsigned char> CreatePayload_Update_Id_Registration()
{
  std::vector<unsigned char> payload;

  uint16_t messageType = 116;
  uint16_t messageVer = 0;

  PUSH_BACK_BYTES(payload, messageVer);
  PUSH_BACK_BYTES(payload, messageType);

  return payload;
}

std::vector<unsigned char> CreatePayload_DEx(uint32_t propertyId, uint64_t amount, uint64_t price,  uint8_t timeLimit, uint64_t minFee, uint8_t subAction)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 21;
    uint16_t messageVer = 0;

    SwapByteOrder32(propertyId);
    SwapByteOrder64(amount);
    SwapByteOrder64(price);
    SwapByteOrder64(minFee);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, amount);
    PUSH_BACK_BYTES(payload, timeLimit);
    PUSH_BACK_BYTES(payload, minFee);
    PUSH_BACK_BYTES(payload, subAction);

    return payload;
}

std::vector<unsigned char> CreatePayload_IssuancePegged(uint8_t ecosystem, uint32_t previousPropertyId, std::string name, uint32_t propertyId, uint32_t contractId, uint64_t amount)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 100;
    uint16_t messageVer = 0;

    SwapByteOrder32(previousPropertyId);
    SwapByteOrder32(propertyId);
    SwapByteOrder32(contractId);
    SwapByteOrder64(amount);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, ecosystem);
    PUSH_BACK_BYTES(payload, previousPropertyId);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, contractId);
    PUSH_BACK_BYTES(payload, amount);


    if (name.size() > 255) name = name.substr(0,255);

    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');

    return payload;
}

std::vector<unsigned char> CreatePayload_SendPeggedCurrency(uint32_t propertyId, uint64_t amount)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 102;
    uint16_t messageVer = 0;

    SwapByteOrder32(propertyId);
    SwapByteOrder64(amount);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, amount);

    return payload;
}

std::vector<unsigned char> CreatePayload_RedemptionPegged(uint32_t propertyId, uint32_t contractId, uint64_t amount)
{
    std::vector<unsigned char> payload;

    uint16_t messageType = 101;
    uint16_t messageVer = 0;

    SwapByteOrder32(propertyId);
    SwapByteOrder32(contractId);
    SwapByteOrder64(amount);

    PUSH_BACK_BYTES(payload, messageVer);
    PUSH_BACK_BYTES(payload, messageType);
    PUSH_BACK_BYTES(payload, propertyId);
    PUSH_BACK_BYTES(payload, contractId);
    PUSH_BACK_BYTES(payload, amount);

    return payload;

}

#undef PUSH_BACK_BYTES
#undef PUSH_BACK_BYTES_PTR
