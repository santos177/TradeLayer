// Smart Properties 

#include <tradelayer/sp.h>

#include <tradelayer/log.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/uint256_extensions.h>

#include <arith_uint256.h>
#include <validation.h>
#include <tinyformat.h>
#include <uint256.h>
#include <util/time.h>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include <stdint.h>

#include <map>
#include <string>
#include <vector>
#include <utility>

using namespace mastercore;

bool mastercore::IsPropertyIdValid(uint32_t propertyId)
{
    if (propertyId == 0) return false;

    uint32_t nextId = 0;

    if (propertyId < TEST_ECO_PROPERTY_1) {
        nextId = pDbSpInfo->peekNextSPID(1);
    } else {
        nextId = pDbSpInfo->peekNextSPID(2);
    }

    if (propertyId < nextId) {
        return true;
    }

    return false;
}

bool mastercore::isPropertyDivisible(uint32_t propertyId)
{
    // TODO: is a lock here needed
    CMPSPInfo::Entry sp;

    if (pDbSpInfo->getSP(propertyId, sp)) return sp.isDivisible();

    return true;
}



std::string mastercore::getPropertyName(uint32_t propertyId)
{
    CMPSPInfo::Entry sp;
    if (pDbSpInfo->getSP(propertyId, sp)) return sp.name;
    return "Property Name Not Found";
}

std::string mastercore::strPropertyType(uint16_t propertyType)
{
    switch (propertyType) {
        case MSC_PROPERTY_TYPE_DIVISIBLE: return "divisible";
        case MSC_PROPERTY_TYPE_INDIVISIBLE: return "indivisible";
    }

    return "unknown";
}

std::string mastercore::strEcosystem(uint8_t ecosystem)
{
    switch (ecosystem) {
        case TL_PROPERTY_MSC: return "main";
        case TL_PROPERTY_TMSC: return "test";
    }

    return "unknown";
}
