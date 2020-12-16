#ifndef BITCOIN_TRADELAYER_ENCODING_H
#define BITCOIN_TRADELAYER_ENCODING_H

class CPubKey;
class CTxOut;

#include <script/script.h>

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

/** Embeds a payload in obfuscated multisig outputs, and adds an Exodus marker output. */
bool TradeLayer_Encode_ClassB(const std::string& senderAddress, const CPubKey& redeemingPubKey, const std::vector<unsigned char>& vchPayload, std::vector<std::pair<CScript, int64_t> >& vecOutputs);

/** Embeds a payload in an OP_RETURN output, prefixed with a transaction marker. */
bool TradeLayer_Encode_ClassC(const std::vector<unsigned char>& vecPayload, std::vector<std::pair<CScript, int64_t> >& vecOutputs);


#endif // BITCOIN_TRADELAYER_ENCODING_H
