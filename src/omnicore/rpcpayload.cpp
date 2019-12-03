#include <omnicore/createpayload.h>
#include <omnicore/rpcvalues.h>
#include <omnicore/rpcrequirements.h>
#include <omnicore/omnicore.h>
#include <omnicore/sp.h>
#include <omnicore/tx.h>

#include <rpc/server.h>
#include <rpc/util.h>
#include <util/strencodings.h>

#include <univalue.h>

using std::runtime_error;
using namespace mastercore;

static UniValue omni_createpayload_simplesend(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
           RPCHelpMan{"omni_createpayload_simplesend",
              "\nCreate the payload for a simple send transaction.\n",
              {
                  {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to send\n"},
                  {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount to send\n"},
              },
              RPCResult{
                  "\"payload\"             (string) the hex-encoded payload\n"
              },
              RPCExamples{
                  HelpExampleCli("omni_createpayload_simplesend", "1 \"100.0\"")
                  + HelpExampleRpc("omni_createpayload_simplesend", "1, \"100.0\"")
              }
           }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_sendall(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_sendall",
               "\nCreate the payload for a send all transaction.\n",
               {
                   {"ecosystem", RPCArg::Type::NUM, RPCArg::Optional::NO, "the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"},
               },
               RPCResult{
                   "\"payload\"               (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_sendall", "2")
                   + HelpExampleRpc("omni_createpayload_sendall", "2")
               }
            }.ToString());

    uint8_t ecosystem = ParseEcosystem(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_dexsell(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 6)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_dexsell",
               "\nCreate a payload to place, update or cancel a sell offer on the traditional distributed OMNI/BTC exchange.\n",
               {
                   {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, " the identifier of the tokens to list for sale (must be 1 for OMN or 2 for TOMN)\n"},
                   {"amountforsale", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to list for sale\n"},
                   {"amountdesired", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of bitcoins desired\n"},
                   {"paymentwindow", RPCArg::Type::NUM, RPCArg::Optional::NO, "a time limit in blocks a buyer has to pay following a successful accepting order\n"},
                   {"minacceptfee", RPCArg::Type::STR, RPCArg::Optional::NO, "a minimum mining fee a buyer has to pay to accept the offer\n"},
                   {"action", RPCArg::Type::NUM, RPCArg::Optional::NO, "the action to take (1 for new offers, 2 to update\", 3 to cancel)\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_dexsell", "1 \"1.5\" \"0.75\" 25 \"0.0005\" 1")
                   + HelpExampleRpc("omni_createpayload_dexsell", "1, \"1.5\", \"0.75\", 25, \"0.0005\", 1")
               }
            }.ToString());

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    uint8_t action = ParseDExAction(request.params[5]);

    int64_t amountForSale = 0; // depending on action
    int64_t amountDesired = 0; // depending on action
    uint8_t paymentWindow = 0; // depending on action
    int64_t minAcceptFee = 0;  // depending on action

    if (action <= CMPTransaction::UPDATE) { // actions 3 permit zero values, skip check
        amountForSale = ParseAmount(request.params[1], isPropertyDivisible(propertyIdForSale));
        amountDesired = ParseAmount(request.params[2], true); // BTC is divisible
        paymentWindow = ParseDExPaymentWindow(request.params[3]);
        minAcceptFee = ParseDExFee(request.params[4]);
    }

    std::vector<unsigned char> payload = CreatePayload_DExSell(propertyIdForSale, amountForSale, amountDesired, paymentWindow, minAcceptFee, action);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_dexaccept(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_dexaccept",
               "\nCreate the payload for an accept offer for the specified token and amount.\n"
                "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
               {
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the token to purchase\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount to accept\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_dexaccept", "1 \"15.0\"")
                   + HelpExampleRpc("omni_createpayload_dexaccept", "1, \"15.0\"")
               }
            }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_DExAccept(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_sto(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_sto",
               "\nCreates the payload for a send-to-owners transaction.\n"
               "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
               {
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to distribute\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount to distribute\n"},
                   {"distributionproperty", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "the identifier of the property holders to distribute to\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_sto", "3 \"5000\"")
                   + HelpExampleRpc("omni_createpayload_sto", "3, \"5000\"")
               }
            }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
    uint32_t distributionPropertyId = (request.params.size() > 2) ? ParsePropertyId(request.params[2]) : propertyId;

    std::vector<unsigned char> payload = CreatePayload_SendToOwners(propertyId, amount, distributionPropertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_issuancefixed(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 9)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_issuancefixed",
               "\nCreates the payload for a new tokens issuance with fixed supply.\n",
               {
                   {"ecosystem", RPCArg::Type::STR, RPCArg::Optional::NO, "the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"},
                   {"type", RPCArg::Type::NUM, RPCArg::Optional::NO, "the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"},
                   {"previousid", RPCArg::Type::NUM, RPCArg::Optional::NO, "an identifier of a predecessor token (use 0 for new tokens)\n"},
                   {"category", RPCArg::Type::STR, RPCArg::Optional::NO, "a category for the new tokens (can be \"\")\n"},
                   {"subcategory", RPCArg::Type::STR, RPCArg::Optional::NO, "a subcategory for the new tokens  (can be \"\")\n"},
                   {"name", RPCArg::Type::STR, RPCArg::Optional::NO, "the name of the new tokens to create\n"},
                   {"url", RPCArg::Type::STR, RPCArg::Optional::NO, "a URL for further information about the new tokens (can be \"\")\n"},
                   {"data", RPCArg::Type::STR, RPCArg::Optional::NO, "a description for the new tokens (can be \"\")\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the number of tokens to create\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_issuancefixed", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" \"1000000\"")
                   + HelpExampleRpc("omni_createpayload_issuancefixed", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", \"1000000\"")
               }
            }.ToString());

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string category = ParseText(request.params[3]);
    std::string subcategory = ParseText(request.params[4]);
    std::string name = ParseText(request.params[5]);
    std::string url = ParseText(request.params[6]);
    std::string data = ParseText(request.params[7]);
    int64_t amount = ParseAmount(request.params[8], type);

    RequirePropertyName(name);

    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(ecosystem, type, previousId, category, subcategory, name, url, data, amount);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_issuancecrowdsale(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 13)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_issuancecrowdsale",
               "\nCreates the payload for a new tokens issuance with crowdsale.\n",
               {
                   {"ecosystem", RPCArg::Type::STR, RPCArg::Optional::NO, "the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"},
                   {"type", RPCArg::Type::NUM, RPCArg::Optional::NO, "the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"},
                   {"previousid", RPCArg::Type::NUM, RPCArg::Optional::NO, "an identifier of a predecessor token (0 for new crowdsales)\n"},
                   {"category", RPCArg::Type::STR, RPCArg::Optional::NO, "a category for the new tokens (can be \"\")\n"},
                   {"subcategory", RPCArg::Type::STR, RPCArg::Optional::NO, "a subcategory for the new tokens  (can be \"\")\n"},
                   {"name", RPCArg::Type::STR, RPCArg::Optional::NO, "the name of the new tokens to create\n"},
                   {"url", RPCArg::Type::STR, RPCArg::Optional::NO, "a URL for further information about the new tokens (can be \"\")\n"},
                   {"data", RPCArg::Type::STR, RPCArg::Optional::NO, "a description for the new tokens (can be \"\")\n"},
                   {"propertyiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of a token eligible to participate in the crowdsale\n"},
                   {"tokensperunit", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens granted per unit invested in the crowdsale\n"},
                   {"deadline", RPCArg::Type::NUM, RPCArg::Optional::NO, "the deadline of the crowdsale as Unix timestamp\n"},
                   {"earlybonus", RPCArg::Type::NUM, RPCArg::Optional::NO, "an early bird bonus for participants in percent per week\n"},
                   {"issuerpercentage", RPCArg::Type::NUM, RPCArg::Optional::NO, "a percentage of tokens that will be granted to the issuer\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_issuancecrowdsale", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2")
                   + HelpExampleRpc("omni_createpayload_issuancecrowdsale", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2")
               }
            }.ToString());

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string category = ParseText(request.params[3]);
    std::string subcategory = ParseText(request.params[4]);
    std::string name = ParseText(request.params[5]);
    std::string url = ParseText(request.params[6]);
    std::string data = ParseText(request.params[7]);
    uint32_t propertyIdDesired = ParsePropertyId(request.params[8]);
    int64_t numTokens = ParseAmount(request.params[9], type);
    int64_t deadline = ParseDeadline(request.params[10]);
    uint8_t earlyBonus = ParseEarlyBirdBonus(request.params[11]);
    uint8_t issuerPercentage = ParseIssuerBonus(request.params[12]);

    RequirePropertyName(name);
    RequireSameEcosystem(ecosystem, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_IssuanceVariable(ecosystem, type, previousId, category, subcategory, name, url, data, propertyIdDesired, numTokens, deadline, earlyBonus, issuerPercentage);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_issuancemanaged(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 8)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_issuancemanaged",
               "\nCreates the payload for a new tokens issuance with manageable supply.\n",
               {
                   {"ecosystem", RPCArg::Type::STR, RPCArg::Optional::NO, "the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"},
                   {"type", RPCArg::Type::NUM, RPCArg::Optional::NO, "the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"},
                   {"previousid", RPCArg::Type::NUM, RPCArg::Optional::NO, "an identifier of a predecessor token (use 0 for new tokens)\n"},
                   {"category", RPCArg::Type::STR, RPCArg::Optional::NO, "a category for the new tokens (can be \"\")\n"},
                   {"subcategory", RPCArg::Type::STR, RPCArg::Optional::NO, "a subcategory for the new tokens  (can be \"\")\n"},
                   {"name", RPCArg::Type::STR, RPCArg::Optional::NO, "the name of the new tokens to create\n"},
                   {"url", RPCArg::Type::STR, RPCArg::Optional::NO, "a URL for further information about the new tokens (can be \"\")\n"},
                   {"data", RPCArg::Type::STR, RPCArg::Optional::NO, "a description for the new tokens (can be \"\")\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_issuancemanaged", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
                   + HelpExampleRpc("omni_createpayload_issuancemanaged", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
               }
            }.ToString());

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint16_t type = ParsePropertyType(request.params[1]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
    std::string category = ParseText(request.params[3]);
    std::string subcategory = ParseText(request.params[4]);
    std::string name = ParseText(request.params[5]);
    std::string url = ParseText(request.params[6]);
    std::string data = ParseText(request.params[7]);

    RequirePropertyName(name);

    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(ecosystem, type, previousId, category, subcategory, name, url, data);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_closecrowdsale(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_closecrowdsale",
               "\nCreates the payload to manually close a crowdsale.\n",
               {
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the crowdsale to close\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_closecrowdsale", "70")
                   + HelpExampleRpc("omni_createpayload_closecrowdsale", "70")
               }
            }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_CloseCrowdsale(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_grant(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_grant",
               "\nCreates the payload to issue or grant new units of managed tokens.\n"
               "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
               {
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to grant\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to create\n"},
                   {"memo", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "a text note attached to this transaction\n"},
               },
               RPCResult{
                   "1. propertyid           (number, required) "
                   "2. amount               (string, required) "
                   "3. memo                 (string, optional) "
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_grant", "51 \"7000\"")
                   + HelpExampleRpc("omni_createpayload_grant", "51, \"7000\"")
               }
            }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
    std::string memo = (request.params.size() > 2) ? ParseText(request.params[2]): "";

    std::vector<unsigned char> payload = CreatePayload_Grant(propertyId, amount, memo);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_revoke(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() < 2 || request.params.size() > 3)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_revoke",
               "\nCreates the payload to revoke units of managed tokens.\n"
               "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
               {
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to revoke\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to revoke\n"},
                   {"memo", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "a text note attached to this transaction\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_revoke", "51 \"100\"")
                   + HelpExampleRpc("omni_createpayload_revoke", "51, \"100\"")
               }
            }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], isPropertyDivisible(propertyId));
    std::string memo = (request.params.size() > 2) ? ParseText(request.params[2]): "";

    std::vector<unsigned char> payload = CreatePayload_Revoke(propertyId, amount, memo);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_changeissuer(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_changeissuer",
               "\nCreats the payload to change the issuer on record of the given tokens.\n",
               {
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_changeissuer", "3")
                   + HelpExampleRpc("omni_createpayload_changeissuer", "3")
               }
            }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_ChangeIssuer(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_trade(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_trade",
               "\nCreates the payload to place a trade offer on the distributed token exchange.\n"
               "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
               {
                   {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to list for sale\n"},
                   {"amountforsale", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to list for sale\n"},
                   {"propertiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens desired in exchange\n"},
                   {"amountdesired", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens desired in exchange\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_trade", "31 \"250.0\" 1 \"10.0\"")
                   + HelpExampleRpc("omni_createpayload_trade", "31, \"250.0\", 1, \"10.0\"")
               }
            }.ToString());

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    int64_t amountForSale = ParseAmount(request.params[1], isPropertyDivisible(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(request.params[2]);
    int64_t amountDesired = ParseAmount(request.params[3], isPropertyDivisible(propertyIdDesired));
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_MetaDExTrade(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_canceltradesbyprice(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_canceltradesbyprice",
               "\nCreates the payload to cancel offers on the distributed token exchange with the specified price.\n"
               "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
               {
                   {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens listed for sale\n"},
                   {"amountforsale", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to listed for sale\n"},
                   {"propertiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens desired in exchange\n"},
                   {"amountdesired", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens desired in exchange\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_canceltradesbyprice", "31 \"100.0\" 1 \"5.0\"")
                   + HelpExampleRpc("omni_createpayload_canceltradesbyprice", "31, \"100.0\", 1, \"5.0\"")
               }
            }.ToString());

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    int64_t amountForSale = ParseAmount(request.params[1], isPropertyDivisible(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(request.params[2]);
    int64_t amountDesired = ParseAmount(request.params[3], isPropertyDivisible(propertyIdDesired));
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPrice(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_canceltradesbypair(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_canceltradesbypair",
               "\nCreates the payload to cancel all offers on the distributed token exchange with the given currency pair.\n",
               {
                   {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens listed for sale\n"},
                   {"propertiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens desired in exchange\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_canceltradesbypair", "1 31")
                   + HelpExampleRpc("omni_createpayload_canceltradesbypair", "1, 31")
               }
            }.ToString());

    uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
    uint32_t propertyIdDesired = ParsePropertyId(request.params[1]);
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPair(propertyIdForSale, propertyIdDesired);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_cancelalltrades(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_cancelalltrades",
               "\nCreates the payload to cancel all offers on the distributed token exchange.\n",
               {
                   {"ecosystem", RPCArg::Type::NUM, RPCArg::Optional::NO, "the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_cancelalltrades", "1")
                   + HelpExampleRpc("omni_createpayload_cancelalltrades", "1")
               }
            }.ToString());

    uint8_t ecosystem = ParseEcosystem(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelEcosystem(ecosystem);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_enablefreezing(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_enablefreezing",
               "\nCreates the payload to enable address freezing for a centrally managed property.\n",
               {
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_enablefreezing", "3")
                   + HelpExampleRpc("omni_createpayload_enablefreezing", "3")
               }
            }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_EnableFreezing(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_disablefreezing(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_disablefreezing",
               "\nCreates the payload to disable address freezing for a centrally managed property.\n"
               "\nIMPORTANT NOTE:  Disabling freezing for a property will UNFREEZE all frozen addresses for that property!",
               {
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_disablefreezing", "3")
                   + HelpExampleRpc("omni_createpayload_disablefreezing", "3")
               }
            }.ToString());

    uint32_t propertyId = ParsePropertyId(request.params[0]);

    std::vector<unsigned char> payload = CreatePayload_DisableFreezing(propertyId);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_freeze(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_freeze",
               "\nCreates the payload to freeze an address for a centrally managed token.\n"
               "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
               {
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to freeze tokens for\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property to freeze tokens for (must be managed type and have freezing option enabled)\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to freeze (note: this is unused - once frozen an address cannot send any transactions)\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_freeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 1 \"100\"")
                   + HelpExampleRpc("omni_createpayload_freeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 1, \"100\"")
               }
            }.ToString());

    std::string refAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_FreezeTokens(propertyId, amount, refAddress);

    return HexStr(payload.begin(), payload.end());
}

static UniValue omni_createpayload_unfreeze(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            RPCHelpMan{"omni_createpayload_unfreeze",
               "\nCreates the payload to unfreeze an address for a centrally managed token.\n"
               "\nNote: if the server is not synchronized, amounts are considered as divisible, even if the token may have indivisible units!\n",
               {
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to unfreeze tokens for\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property to unfreeze tokens for (must be managed type and have freezing option enabled)\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to unfreeze (note: this is unused)\n"},
               },
               RPCResult{
                   "\"payload\"             (string) the hex-encoded payload\n"
               },
               RPCExamples{
                   HelpExampleCli("omni_createpayload_unfreeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 1 \"100\"")
                   + HelpExampleRpc("omni_createpayload_unfreeze", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 1, \"100\"")
               }
            }.ToString());

    std::string refAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));

    std::vector<unsigned char> payload = CreatePayload_UnfreezeTokens(propertyId, amount, refAddress);

    return HexStr(payload.begin(), payload.end());
}

static UniValue tl_createpayload_createcontract(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 7)
    throw runtime_error(
			"tl_createpayload_createcontract \" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline ( earlybonus issuerpercentage )\n"

			"Payload to create new Future Contract."

			"\nArguments:\n"
			"1. ecosystem                 (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
			"2. numerator                 (number, required) 4: ALL, 5: sLTC, 6: LTC.\n"
			"3. name                      (string, required) the name of the new tokens to create\n"
			"4. blocks until expiration   (number, required) life of contract, in blocks\n"
			"5. notional size             (number, required) notional size\n"
			"6. collateral currency       (number, required) collateral currency\n"
      "7. margin requirement        (number, required) margin requirement\n"

			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_createcontract", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2 4461 100 1 25")
			+ HelpExampleRpc("tl_createpayload_createcontract", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2, 4461, 100, 1, 25")
			);

  uint8_t ecosystem = ParseEcosystem(request.params[0]);
  uint32_t type = ParseContractType(request.params[1]);
  std::string name = ParseText(request.params[2]);
  uint32_t blocks_until_expiration = ParseNewValues(request.params[3]);
  uint32_t notional_size = ParseNewValues(request.params[4]);
  uint32_t collateral_currency = ParseNewValues(request.params[5]);
  uint32_t margin_requirement = ParseAmount(request.params[6], true);

  std::vector<unsigned char> payload = CreatePayload_CreateContract(ecosystem, type, name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement);

  return HexStr(payload.begin(), payload.end());
}

static UniValue tl_createpayload_tradecontract(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 5)
    throw runtime_error(
			"tl_createpayload_tradecontract \" propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

			"\nPayload to place a trade offer on the distributed Futures Contracts exchange.\n"

			"\nArguments:\n"
		  "1. propertyidforsale    (number, required) the identifier of the contract to list for trade\n"
			"2. amountforsale        (number, required) the amount of contracts to trade\n"
			"3. effective price      (number, required) limit price desired in exchange\n"
			"4. trading action       (number, required) 1 to BUY contracts, 2 to SELL contracts \n"
			"5. leverage             (number, required) leverage (2x, 3x, ... 10x)\n"
			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_tradecontract", "\"250.0\"1\"10.0\"70.0\"80.0\"")
			+ HelpExampleRpc("tl_tradecontract", ",\"250.0\",1,\"10.0,\"70.0,\"80.0\"")
			);

  std::string name_traded = ParseText(request.params[0]);
  int64_t amountForSale = ParseAmountContract(request.params[1]);
  uint64_t effective_price = ParseEffectivePrice(request.params[2]);
  uint8_t trading_action = ParseContractDexAction(request.params[3]);
  uint64_t leverage = ParseLeverage(request.params[4]);

  std::vector<unsigned char> payload = CreatePayload_ContractDexTrade(name_traded, amountForSale, effective_price, trading_action, leverage);

  return HexStr(payload.begin(), payload.end());
}

static UniValue tl_createpayload_cancelallcontractsbyaddress(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2)
    throw runtime_error(
			"tl_cancelallcontractsbyaddress \" ecosystem\n"

			"\nPayload to cancel all offers on a given Futures Contract .\n"

			"\nArguments:\n"
			"1. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"
			"2. contractId           (number, required) the Id of Future Contract \n"
			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_cancelallcontractsbyaddress", "\" 3")
			+ HelpExampleRpc("tl_createpayload_cancelallcontractsbyaddress", "\" 3")
			);

  uint8_t ecosystem = ParseEcosystem(request.params[0]);
  std::string name_traded = ParseText(request.params[1]);

  struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_traded);
  uint32_t contractId = pfuture->fco_propertyId;

  std::vector<unsigned char> payload = CreatePayload_ContractDexCancelEcosystem(ecosystem, contractId);

  return HexStr(payload.begin(), payload.end());
}

static UniValue tl_createpayload_closeposition(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_closeposition \" ecosystem \" contractId\n"

            "\nPayload to close the position on a given Futures Contract .\n"

            "\nArguments:\n"
            "1. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"
            "2. contractId           (number, required) the Id of Future Contract \n"
            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_closeposition", "\" 1, 3")
            + HelpExampleRpc("tl_createpayload_closeposition", "\", 1, 3")
        );

    uint8_t ecosystem = ParseEcosystem(request.params[0]);
    uint32_t contractId = ParsePropertyId(request.params[1]);

    std::vector<unsigned char> payload = CreatePayload_ContractDexClosePosition(ecosystem, contractId);

    return HexStr(payload.begin(), payload.end());
}

// static UniValue tl_createpayload_sendissuance_pegged(const JSONRPCRequest& request)
// {
//   if (request.fHelp || request.params.size() != 7)
//     throw runtime_error(
// 			"tl_createpayload_sendissuance_pegged\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\"\n"
//
// 			"\nPayload to create new pegged currency with manageable supply.\n"
//
// 			"\nArguments:\n"
// 			"1. ecosystem             (string, required) the ecosystem to create the pegged currency in (1 for main ecosystem, 2 for test ecosystem)\n"
// 			"2. type                  (number, required) the type of the pegged to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
// 			"3. previousid            (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
// 			"4. name                  (string, required) the name of the new pegged to create\n"
// 			"5. collateralcurrency    (number, required) the collateral currency for the new pegged \n"
// 			"6. future contract name  (number, required) the future contract name for the new pegged \n"
// 			"7. amount of pegged      (number, required) amount of pegged to create \n"
// 			"\nResult:\n"
// 			"\"payload\"              (string) the hex-encoded payload\n"
//
// 			"\nExamples:\n"
// 			+ HelpExampleCli("tl_createpayload_sendissuance_pegged", "\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
// 			+ HelpExampleRpc("tl_createpayload_sendissuance_pegged", "\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
// 			);
//
//   uint8_t ecosystem = ParseEcosystem(request.params[0]);
//   uint16_t type = ParsePropertyType(request.params[1]);
//   uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
//   std::string name = ParseText(request.params[3]);
//   uint32_t propertyId = ParsePropertyId(request.params[4]);
//   std::string name_traded = ParseText(request.params[5]);
//   uint64_t amount = ParseAmount(request.params[6], isPropertyDivisible(propertyId));
//
//   struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_traded);
//   uint32_t contractId = pfuture->fco_propertyId;
//
//   std::vector<unsigned char> payload = CreatePayload_IssuancePegged(ecosystem, type, previousId, name, propertyId, contractId, amount);
//
//   return HexStr(payload.begin(), payload.end());
// }

// static UniValue tl_createpayload_send_pegged(const JSONRPCRequest& request)
// {
//   if (request.fHelp || request.params.size() != 2)
//     throw runtime_error(
// 			"tl_send \" propertyid \"amount\" ( \"redeemaddress\" \"referenceamount\" )\n"
//
// 			"\nPayload to send the pegged currency to other addresses.\n"
//
// 			"\nArguments:\n"
// 			"1. property name        (string, required) the identifier of the tokens to send\n"
// 			"2. amount               (string, required) the amount to send\n"
//
//
// 			"\nResult:\n"
// 			"\"payload\"             (string) the hex-encoded payload\n"
//
// 			"\nExamples:\n"
// 			+ HelpExampleCli("tl_createpayload_send_pegged", "\" 1 \"100.0\"")
// 			+ HelpExampleRpc("tl_createpayload_send_pegged", "\", 1, \"100.0\"")
// 			);
//
//   std::string name_pegged = ParseText(request.params[0]);
//
//   struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_PEGGEDS, name_pegged);
//   uint32_t propertyId = pfuture->fco_propertyId;
//
//   int64_t amount = ParseAmount(request.params[1], true);
//
//   std::vector<unsigned char> payload = CreatePayload_SendPeggedCurrency(propertyId, amount);
//
//   return HexStr(payload.begin(), payload.end());
// }

// static UniValue tl_createpayload_redemption_pegged(const JSONRPCRequest& request)
// {
//     if (request.fHelp || request.params.size() != 3)
//        throw runtime_error(
// 			  "tl_createpayload_redemption_pegged \" propertyid \"amount\" ( \"redeemaddress\" distributionproperty )\n"
//
// 			  "\n Payload to redeem pegged currency .\n"
//
// 			  "\nArguments:\n"
// 			  "1. name of pegged       (string, required) name of the tokens to redeem\n"
// 			  "2. amount               (number, required) the amount of pegged currency for redemption"
// 			  "3. name of contract     (string, required) the identifier of the future contract involved\n"
// 			  "\nResult:\n"
// 			  "\"payload\"             (string) the hex-encoded payload\n"
//
// 		  	"\nExamples:\n"
// 		  	+ HelpExampleCli("tl_createpayload_redemption_pegged", "\" , 1")
// 		  	+ HelpExampleRpc("tl_createpayload_redemption_pegged", "\", 1")
// 			  );
//
//     std::string name_pegged = ParseText(request.params[0]);
//     std::string name_contract = ParseText(request.params[2]);
//     struct FutureContractObject *pfuture_pegged = getFutureContractObject(ALL_PROPERTY_TYPE_PEGGEDS, name_pegged);
//     uint32_t propertyId = pfuture_pegged->fco_propertyId;
//
//     uint64_t amount = ParseAmount(request.params[1], true);
//
//     struct FutureContractObject *pfuture_contract = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_contract);
//     uint32_t contractId = pfuture_contract->fco_propertyId;
//
//     std::vector<unsigned char> payload = CreatePayload_RedemptionPegged(propertyId, contractId, amount);
//
//     return HexStr(payload.begin(), payload.end());
// }

static UniValue tl_createpayload_cancelorderbyblock(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_cancelorderbyblock \"block\" idx\n"

            "\nPayload to cancel an specific offer on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. block           (number, required) the block of order to cancel\n"
            "2. idx             (number, required) the idx in block of order to cancel\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_cancelorderbyblock", "\" 1, 2")
            + HelpExampleRpc("tl_createpayload_cancelorderbyblock", "\", 1, 2")
        );

    int block = static_cast<int>(ParseNewValues(request.params[0]));
    int idx = static_cast<int>(ParseNewValues(request.params[1]));

    std::vector<unsigned char> payload = CreatePayload_ContractDexCancelOrderByTxId(block,idx);

    return HexStr(payload.begin(), payload.end());
}

// static UniValue tl_createpayload_dexoffer(const JSONRPCRequest& request)
// {
//   if (request.fHelp || request.params.size() != 7) {
//     throw runtime_error(
// 			"tl_createpayload_dexsell \" propertyidforsale \"amountforsale\" \"amountdesired\" paymentwindow minacceptfee action\n"
//
// 			"\nPayload to place, update or cancel a sell offer on the traditional distributed Trade Layer/LTC exchange.\n"
//
// 			"\nArguments:\n"
// 			"1. propertyidoffer     (number, required) the identifier of the tokens to list for sale (must be 1 for OMNI or 2 for TOMNI)\n"
// 			"2. amountoffering      (string, required) the amount of tokens to list for sale\n"
// 			"3. price               (string, required) the price in litecoin of the offer \n"
// 			"4. paymentwindow       (number, required) a time limit in blocks a buyer has to pay following a successful accepting order\n"
// 			"5. minacceptfee        (string, required) a minimum mining fee a buyer has to pay to accept the offer\n"
// 			"6. option              (number, required) 1 for buy tokens, 2 to sell\n"
// 			"7. action              (number, required) the action to take (1 for new offers, 2 to update\", 3 to cancel)\n"
//
// 			"\nResult:\n"
// 			"\"payload\"             (string) the hex-encoded payload\n"
//
// 			"\nExamples:\n"
// 			+ HelpExampleCli("tl_createpayload_dexsell", "\" 1 \"1.5\" \"0.75\" 25 \"0.0005\" 1")
// 			+ HelpExampleRpc("tl_createpayload_dexsell", "\", 1, \"1.5\", \"0.75\", 25, \"0.0005\", 1")
// 			);
//   }
//
//   uint32_t propertyIdForSale = ParsePropertyId(request.params[0]);
//   int64_t amountForSale = ParseAmount(request.params[1], true); // TMSC/MSC is divisible
//   int64_t price = ParseAmount(request.params[2], true); // BTC is divisible
//   uint8_t paymentWindow = ParseDExPaymentWindow(request.params[3]);
//   int64_t minAcceptFee = ParseDExFee(request.params[4]);
//   int64_t option = ParseAmount(request.params[5], false);  // buy : 1 ; sell : 2;
//   uint8_t action = ParseDExAction(request.params[6]);
//
//   std::vector<unsigned char> payload;
//
//   if (option == 1)
//   {
//       payload = CreatePayload_DEx(propertyIdForSale, amountForSale, price, paymentWindow, minAcceptFee, action);
//   } else if (option == 2) {
//       payload = CreatePayload_DExSell(propertyIdForSale, amountForSale, price, paymentWindow, minAcceptFee, action);
//   }
//
//   return HexStr(payload.begin(), payload.end());
// }

static UniValue tl_createpayload_dexaccept(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_createpayload_dexaccept \" propertyid \"amount\n"

            "\nPayload to create and broadcast an accept offer for the specified token and amount.\n"

            "\nArguments:\n"
            "1. propertyid           (number, required) the identifier of the token traded\n"
            "2. amount               (string, required) the amount traded\n"

            "\nResult:\n"
            "\"payload\"             (string) the hex-encoded payload\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_createpayload_dexaccept", "\" 1 \"15.0\"")
            + HelpExampleRpc("tl_createpayload_dexaccept", "\1, \"15.0\"")
        );

    uint32_t propertyId = ParsePropertyId(request.params[0]);
    int64_t amount = ParseAmount(request.params[1], true); // MSC/TMSC is divisible

    std::vector<unsigned char> payload = CreatePayload_DExAccept(propertyId, amount);

    return HexStr(payload.begin(), payload.end());
}

static UniValue tl_createpayload_sendvesting(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() < 2 || request.params.size() > 2)
    throw runtime_error(
			"tl_createpayload_sendvesting \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

			"\nCreate and broadcast a simple send transaction.\n"

			"\nArguments:\n"
			"1. propertyid           (number, required) the identifier of the tokens to send\n"
			"2. amount               (string, required) the amount of vesting tokens to send\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_sendvesting", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
			+ HelpExampleRpc("tl_createpayload_sendvesting", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
			);

  // obtain parameters & info
  uint32_t propertyId = ParsePropertyId(request.params[0]); /** id=3 Vesting Tokens**/
  int64_t amount = ParseAmount(request.params[1], true);

  PrintToLog("propertyid = %d\n", propertyId);
  PrintToLog("amount = %d\n", amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendVestingTokens(propertyId, amount);

    return HexStr(payload.begin(), payload.end());

}


static UniValue tl_createpayload_instant_trade(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() < 5)
    throw runtime_error(
			"tl_createpayload_instant_trade \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

			"\nCreate an instant trade payload.\n"

			"\nArguments:\n"
			"1. propertyId            (number, required) the identifier of the property\n"
			"2. amount                (string, required) the amount of the property traded for the first address of channel\n"
      "3. blockheight_expiry    (string, required) block of expiry\n"
      "4. propertyDesired       (number, optional) the identifier of the property traded for the second address of channel\n"
      "5. amountDesired         (string, optional) the amount desired of tokens\n"


			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_instant_trade", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
			+ HelpExampleRpc("tl_createpayload_instant_trade", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
			);

  // obtain parameters & info
  uint32_t propertyId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], true);
  uint32_t blockheight_expiry = request.params[2].get_int();
  uint32_t propertyDesired = ParsePropertyId(request.params[3]);
  int64_t amountDesired = ParseAmount(request.params[4],true);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_Instant_Trade(propertyId, amount, blockheight_expiry, propertyDesired, amountDesired);

  return HexStr(payload.begin(), payload.end());

}

static UniValue tl_createpayload_contract_instant_trade(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() < 6)
    throw runtime_error(
			"tl_createpayload_instant_trade \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

			"\nCreate an contract instant trade payload.\n"

			"\nArguments:\n"
			"1. contractId            (number, required) the identifier of the property\n"
			"2. amount                (string, required) the amount of the property traded for the first address of channel\n"
      "3. blockheight_expiry    (string, required) block of expiry\n"
      "4. effective price       (string, required) limit price desired in exchange\n"
			"5. trading action        (number, required) 1 to BUY contracts, 2 to SELL contracts \n"
			"6. leverage              (number, required) leverage (2x, 3x, ... 10x)\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_contract_instant_trade", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
			+ HelpExampleRpc("tl_createpayload_contract_instant_trade", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
			);

  // obtain parameters & info
  uint32_t contractId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], true);
  uint32_t blockheight_expiry = request.params[2].get_int();
  uint64_t price = ParseAmount(request.params[3],true);
  uint8_t trading_action = ParseContractDexAction(request.params[4]);
  uint64_t leverage = ParseLeverage(request.params[5]);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_Contract_Instant_Trade(contractId, amount, blockheight_expiry, price, trading_action, leverage);

  return HexStr(payload.begin(), payload.end());

}

// static UniValue tl_createpayload_pnl_update(const JSONRPCRequest& request)
// {
//   if (request.fHelp || request.params.size() != 3)
//     throw runtime_error(
// 			"tl_createpayload_pnl_update \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"
//
// 			"\nCreate an pnl update payload.\n"
//
// 			"\nArguments:\n"
// 			"1. propertyId            (number, required) the identifier of the property\n"
// 			"2. amount                (string, required) the amount of the property traded\n"
//       "3. blockheight expiry    (number, required) block of expiry\n"
//
// 			"\nResult:\n"
// 			"\"hash\"                  (string) the hex-encoded transaction hash\n"
//
// 			"\nExamples:\n"
// 			+ HelpExampleCli("tl_createpayload_pnl_update", "\" 1 \"15.0\"")
// 			+ HelpExampleRpc("tl_createpayload_pnl_update", "\", 1 \"15.0\"")
// 			);
//
//   // obtain parameters & info
//   uint32_t propertyId = ParsePropertyId(request.params[0]);
//   int64_t amount = ParseAmount(request.params[1], true);
//   uint32_t blockheight_expiry = request.params[2].get_int();
//
//   // create a payload for the transaction
//   std::vector<unsigned char> payload = CreatePayload_PNL_Update(propertyId, amount, blockheight_expiry);
//
//   return HexStr(payload.begin(), payload.end());
//
// }

//Params: propertyid, amount, reference vOut for destination address

static UniValue tl_createpayload_transfer(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 2)
    throw runtime_error(
			"tl_createpayload_transfer \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

			"\nCreate an transfer payload.\n"

			"\nArguments:\n"
			"1. propertyId            (number, required) the identifier of the property\n"
			"2. amount                (string, required) the amount of the property traded\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createpayload_transfer", "\" 1 \"15.0\"")
			+ HelpExampleRpc("tl_createpayload_transfer", "\", 1 \"15.0\"")
			);

  // obtain parameters & info
  uint32_t propertyId = ParsePropertyId(request.params[0]);
  int64_t amount = ParseAmount(request.params[1], true);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_Transfer(propertyId, amount);

  return HexStr(payload.begin(), payload.end());

}

static UniValue tl_createpayload_dex_payment(const JSONRPCRequest& request)
{
  if (request.fHelp || request.params.size() != 0)
    throw runtime_error(
			"tl_createpayload_dex_payment\n"

			"\nCreate an transfer payload.\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
		  + HelpExampleCli("tl_createpayload_dex_payment", "\"")
			+ HelpExampleRpc("tl_createpayload_dex_payment", "\"")
			);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_DEx_Payment();

  return HexStr(payload.begin(), payload.end());

}

static const CRPCCommand commands[] =
{ //  category                         name                                      actor (function)                         okSafeMode
  //  -------------------------------- ----------------------------------------- ---------------------------------------- ----------
    { "omni layer (payload creation)", "omni_createpayload_simplesend",          &omni_createpayload_simplesend,          {"propertyid", "amount"} },
    { "omni layer (payload creation)", "omni_createpayload_sendall",             &omni_createpayload_sendall,             {"ecosystem"} },
    { "omni layer (payload creation)", "omni_createpayload_dexsell",             &omni_createpayload_dexsell,             {"propertyidforsale", "amountforsale", "amountdesired", "paymentwindow", "minacceptfee", "action"} },
    { "omni layer (payload creation)", "omni_createpayload_dexaccept",           &omni_createpayload_dexaccept,           {"propertyid", "amount"} },
    { "omni layer (payload creation)", "omni_createpayload_sto",                 &omni_createpayload_sto,                 {"propertyid", "amount", "distributionproperty"} },
    { "omni layer (payload creation)", "omni_createpayload_grant",               &omni_createpayload_grant,               {"propertyid", "amount", "memo"} },
    { "omni layer (payload creation)", "omni_createpayload_revoke",              &omni_createpayload_revoke,              {"propertyid", "amount", "memo"} },
    { "omni layer (payload creation)", "omni_createpayload_changeissuer",        &omni_createpayload_changeissuer,        {"propertyid"} },
    { "omni layer (payload creation)", "omni_createpayload_trade",               &omni_createpayload_trade,               {"propertyidforsale", "amountforsale", "propertiddesired", "amountdesired"} },
    { "omni layer (payload creation)", "omni_createpayload_issuancefixed",       &omni_createpayload_issuancefixed,       {"ecosystem", "type", "previousid", "category", "subcategory", "name", "url", "data", "amount"} },
    { "omni layer (payload creation)", "omni_createpayload_issuancecrowdsale",   &omni_createpayload_issuancecrowdsale,   {"ecosystem", "type", "previousid", "category", "subcategory", "name", "url", "data", "propertyiddesired", "tokensperunit", "deadline", "earlybonus", "issuerpercentage"} },
    { "omni layer (payload creation)", "omni_createpayload_issuancemanaged",     &omni_createpayload_issuancemanaged,     {"ecosystem", "type", "previousid", "category", "subcategory", "name", "url", "data"} },
    { "omni layer (payload creation)", "omni_createpayload_closecrowdsale",      &omni_createpayload_closecrowdsale,      {"propertyid"} },
    { "omni layer (payload creation)", "omni_createpayload_canceltradesbyprice", &omni_createpayload_canceltradesbyprice, {"propertyidforsale", "amountforsale", "propertiddesired", "amountdesired"} },
    { "omni layer (payload creation)", "omni_createpayload_canceltradesbypair",  &omni_createpayload_canceltradesbypair,  {"propertyidforsale", "propertiddesired"} },
    { "omni layer (payload creation)", "omni_createpayload_cancelalltrades",     &omni_createpayload_cancelalltrades,     {"ecosystem"} },
    { "omni layer (payload creation)", "omni_createpayload_enablefreezing",      &omni_createpayload_enablefreezing,      {"propertyid"} },
    { "omni layer (payload creation)", "omni_createpayload_disablefreezing",     &omni_createpayload_disablefreezing,     {"propertyid"} },
    { "omni layer (payload creation)", "omni_createpayload_freeze",              &omni_createpayload_freeze,              {"toaddress", "propertyid", "amount"} },
    { "omni layer (payload creation)", "omni_createpayload_unfreeze",            &omni_createpayload_unfreeze,            {"toaddress", "propertyid", "amount"} },
    { "trade layer (payload creation)", "tl_createpayload_createcontract",                &tl_createpayload_createcontract,                  {}   },
    { "trade layer (payload creation)", "tl_createpayload_tradecontract",                 &tl_createpayload_tradecontract,                   {}   },
    { "trade layer (payload creation)", "tl_createpayload_cancelallcontractsbyaddress",   &tl_createpayload_cancelallcontractsbyaddress,     {}   },
    { "trade layer (payload creation)", "tl_createpayload_closeposition",                 &tl_createpayload_closeposition,                   {}   },
    // { "trade layer (payload creation)", "tl_createpayload_sendissuance_pegged",           &tl_createpayload_sendissuance_pegged,             {}   },
    // { "trade layer (payload creation)", "tl_createpayload_send_pegged",                   &tl_createpayload_send_pegged,                     {}   },
    // { "trade layer (payload creation)", "tl_createpayload_redemption_pegged",             &tl_createpayload_redemption_pegged,               {}   },
    { "trade layer (payload creation)", "tl_createpayload_cancelorderbyblock",            &tl_createpayload_cancelorderbyblock,              {}   },
    // { "trade layer (payload creation)", "tl_createpayload_dexoffer",                      &tl_createpayload_dexoffer,                        {}   },
    { "trade layer (payload creation)", "tl_createpayload_dexaccept",                     &tl_createpayload_dexaccept,                       {}   },
    { "trade layer (payload creation)", "tl_createpayload_sendvesting",                   &tl_createpayload_sendvesting,                     {}   },
    { "trade layer (payload creation)", "tl_createpayload_instant_trade",                 &tl_createpayload_instant_trade,                   {}   },
    // { "trade layer (payload creation)", "tl_createpayload_pnl_update",                    &tl_createpayload_pnl_update,                      {}   },
    { "trade layer (payload creation)", "tl_createpayload_transfer",                      &tl_createpayload_transfer,                        {}   },
    { "trade layer (payload creation)", "tl_createpayload_dex_payment",                   &tl_createpayload_dex_payment,                     {}   },
    { "trade layer (payload creation)", "tl_createpayload_contract_instant_trade",        &tl_createpayload_contract_instant_trade,          {}   }
};

void RegisterOmniPayloadCreationRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
