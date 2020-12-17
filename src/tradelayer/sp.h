#ifndef BITCOIN_TRADELAYER_SP_H
#define BITCOIN_TRADELAYER_SP_H

#include <tradelayer/dbbase.h>
#include <tradelayer/dbspinfo.h>
#include <tradelayer/log.h>

class CBlockIndex;
class uint256;

#include <openssl/sha.h>

#include <stdint.h>
#include <stdio.h>

#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>


namespace mastercore
{

//! LevelDB based storage for currencies, smart properties and tokens
extern CMPSPInfo* pDbSpInfo;

std::string strPropertyType(uint16_t propertyType);
std::string strEcosystem(uint8_t ecosystem);

std::string getPropertyName(uint32_t propertyId);
bool isPropertyDivisible(uint32_t propertyId);
bool IsPropertyIdValid(uint32_t propertyId);

}


#endif // BITCOIN_TRADELAYER_SP_H
