/**
 * @file rpctx.cpp
 *
 * This file contains RPC calls for creating and sending Trade Layer transactions.
 */

#include <tradelayer/createpayload.h>
#include <tradelayer/dex.h>
#include <tradelayer/errors.h>
#include <tradelayer/tradelayer.h>
#include <tradelayer/pending.h>
#include <tradelayer/rpcrequirements.h>
#include <tradelayer/rpcvalues.h>
#include <tradelayer/rules.h>
#include <tradelayer/sp.h>
#include <tradelayer/tx.h>
#include <tradelayer/utilsbitcoin.h>
#include <tradelayer/wallettxbuilder.h>

#include <interfaces/wallet.h>
#include <init.h>
#include <validation.h>
#include <wallet/rpcwallet.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <sync.h>
#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

#include <univalue.h>

#include <stdint.h>
#include <stdexcept>
#include <string>

using std::runtime_error;
using namespace mastercore;

extern volatile int64_t LTCPriceOffer;

static UniValue tl_funded_send(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 5)
        throw runtime_error(
            RPCHelpMan{"tl_funded_send",
               "\nCreates and sends a funded simple send transaction.\n"
               "\nAll bitcoins from the sender are consumed and if there are bitcoins missing, they are taken from the specified fee source. Change is sent to the fee source!\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send the tokens from\n"},
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address of the receiver\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to send\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount to send\n"},
                   {"feeaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address that is used for change and to pay for fees, if needed\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_funded_send", "\"1DFa5bT6KMEr6ta29QJouainsjaNBsJQhH\" \"15cWrfuvMxyxGst2FisrQcvcpF48x6sXoH\" 1 \"100.0\" \"15Jhzz4omEXEyFKbdcccJwuVPea5LqsKM1\"")
                   + HelpExampleRpc("tl_funded_send", "\"1DFa5bT6KMEr6ta29QJouainsjaNBsJQhH\", \"15cWrfuvMxyxGst2FisrQcvcpF48x6sXoH\", 1, \"100.0\", \"15Jhzz4omEXEyFKbdcccJwuVPea5LqsKM1\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
    std::string feeAddress = ParseAddress(request.params[4]);

    // perform checks
    RequireExistingProperty(propertyId);
    RequireBalance(fromAddress, propertyId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    // create the raw transaction
    uint256 retTxid;
    int result = CreateFundedTransaction(fromAddress, toAddress, feeAddress, payload, retTxid, pwallet.get());
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    }

    return retTxid.ToString();
}

static UniValue tl_funded_sendall(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            RPCHelpMan{"tl_funded_sendall",
               "\nCreates and sends a transaction that transfers all available tokens in the given ecosystem to the recipient.\n"
               "\nAll bitcoins from the sender are consumed and if there are bitcoins missing, they are taken from the specified fee source. Change is sent to the fee source!\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to the tokens send from\n"},
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address of the receiver\n"},
                   {"ecosystem", RPCArg::Type::NUM, RPCArg::Optional::NO, "the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"},
                   {"feeaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address that is used for change and to pay for fees, if needed\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_funded_sendall", "\"1DFa5bT6KMEr6ta29QJouainsjaNBsJQhH\" \"15cWrfuvMxyxGst2FisrQcvcpF48x6sXoH\" 1 \"15Jhzz4omEXEyFKbdcccJwuVPea5LqsKM1\"")
                   + HelpExampleRpc("tl_funded_sendall", "\"1DFa5bT6KMEr6ta29QJouainsjaNBsJQhH\", \"15cWrfuvMxyxGst2FisrQcvcpF48x6sXoH\", 1, \"15Jhzz4omEXEyFKbdcccJwuVPea5LqsKM1\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint8_t ecosystem = ParseEcosystem(request.params[2]);
    std::string feeAddress = ParseAddress(request.params[3]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    // create the raw transaction
    uint256 retTxid;
    int result = CreateFundedTransaction(fromAddress, toAddress, feeAddress, payload, retTxid, pwallet.get());
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    }

    return retTxid.ToString();
}

static UniValue tl_sendrawtx(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() < 2 || request.params.size() > 5)
        throw runtime_error(
            RPCHelpMan{"tl_sendrawtx",
               "\nBroadcasts a raw Trade Layer transaction.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
                   {"rawtransaction", RPCArg::Type::STR, RPCArg::Optional::NO, "the hex-encoded raw transaction"},
                   {"referenceaddress", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "a reference address (none by default)\n"},
                   {"redeemaddress", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "an address that can spent the transaction dust (sender by default)\n"},
                   {"referenceamount", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "a bitcoin amount that is sent to the receiver (minimal by default)\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendrawtx", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\" \"000000000000000100000000017d7840\" \"1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV\"")
                   + HelpExampleRpc("tl_sendrawtx", "\"1MCHESTptvd2LnNp7wmr2sGTpRomteAkq8\", \"000000000000000100000000017d7840\", \"1EqTta1Rt8ixAA32DuC29oukbsSWU62qAV\"")
               }
            }.ToString());

    std::string fromAddress = ParseAddress(request.params[0]);
    std::vector<unsigned char> data = ParseHexV(request.params[1], "raw transaction");
    std::string toAddress = (request.params.size() > 2) ? ParseAddressOrEmpty(request.params[2]): "";
    std::string redeemAddress = (request.params.size() > 3) ? ParseAddressOrEmpty(request.params[3]): "";
    int64_t referenceAmount = (request.params.size() > 4) ? ParseAmount(request.params[4], true): 0;

    //some sanity checking of the data supplied?
    uint256 newTX;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, redeemAddress, referenceAmount, data, newTX, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return newTX.GetHex();
        }
    }
}

static UniValue tl_send(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() < 4 || request.params.size() > 6)
        throw runtime_error(
            RPCHelpMan{"tl_send",
               "\nCreate and broadcast a simple send transaction.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address of the receiver\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to send\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount to send\n"},
                   {"redeemaddress", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "an address that can spend the transaction dust (sender by default)\n"},
                   {"referenceamount", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "a bitcoin amount that is sent to the receiver (minimal by default)\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_send", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
                   + HelpExampleRpc("tl_send", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
    std::string redeemAddress = (request.params.size() > 4 && !ParseText(request.params[4]).empty()) ? ParseAddress(request.params[4]): "";
    int64_t referenceAmount = (request.params.size() > 5) ? ParseAmount(request.params[5], true): 0;

    // perform checks
    RequireExistingProperty(propertyId);
    RequireBalance(fromAddress, propertyId, amount);
    RequireSaneReferenceAmount(referenceAmount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SimpleSend(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, redeemAddress, referenceAmount, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_SIMPLE_SEND, propertyId, amount);
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendall(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 5)
        throw runtime_error(
            RPCHelpMan{"tl_sendall",
               "\nTransfers all available tokens in the given ecosystem to the recipient.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address of the receiver\n"},
                   {"ecosystem", RPCArg::Type::NUM, RPCArg::Optional::NO, "the ecosystem of the tokens to send (1 for main ecosystem, 2 for test ecosystem)\n"},
                   {"redeemaddress", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "an address that can spend the transaction dust (sender by default)\n"},
                   {"referenceamount", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "a bitcoin amount that is sent to the receiver (minimal by default)\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendall", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 2")
                   + HelpExampleRpc("tl_sendall", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 2")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint8_t ecosystem = ParseEcosystem(request.params[2]);
    std::string redeemAddress = (request.params.size() > 3 && !ParseText(request.params[3]).empty()) ? ParseAddress(request.params[3]): "";
    int64_t referenceAmount = (request.params.size() > 4) ? ParseAmount(request.params[4], true): 0;

    // perform checks
    RequireSaneReferenceAmount(referenceAmount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendAll(ecosystem);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, redeemAddress, referenceAmount, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            // TODO: pending
            return txid.GetHex();
        }
    }
}

/* The DEX 1 rpcs */
UniValue tl_senddexoffer(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
  std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
  std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
  std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
  if (request.fHelp || request.params.size() != 8) {
    throw runtime_error(
			"tl_senddexsell \"fromaddress\" propertyidforsale \"amountforsale\" \"amountdesired\" paymentwindow minacceptfee action\n"

			"\nPlace, update or cancel a sell offer on the traditional distributed Trade Layer/LTC exchange.\n"

			"\nArguments:\n"

			"1. fromaddress         (string, required) the address to send from\n"
			"2. propertyidoffer     (number, required) the identifier of the tokens to list for sale (must be 1 for TL or 2 for TTL)\n"
			"3. amountoffering      (string, required) the amount of tokens to list for sale\n"
			"4. price               (string, required) the price in litecoin of the offer \n"
			"5. paymentwindow       (number, required) a time limit in blocks a buyer has to pay following a successful accepting order\n"
			"6. minacceptfee        (string, required) a minimum mining fee a buyer has to pay to accept the offer\n"
			"7. option              (number, required) 1 for buy tokens, 2 to sell\n"
			"8. action              (number, required) the action to take (1 for new offers, 2 to update\", 3 to cancel)\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_senddexsell", "\"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"1.5\" \"0.75\" 25 \"0.0005\" 1")
			+ HelpExampleRpc("tl_senddexsell", "\"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"1.5\", \"0.75\", 25, \"0.0005\", 1")
			);
  }
  // obtain parameters & info

  std::string fromAddress = ParseAddress(request.params[0]);
  uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
  int64_t amountForSale = ParseAmount(request.params[2], true); // TMSC/MSC is divisible
  int64_t price = ParseAmount(request.params[3], true); // BTC is divisible
  uint8_t paymentWindow = ParseDExPaymentWindow(request.params[4]);
  int64_t minAcceptFee = ParseDExFee(request.params[5]);
  int64_t option = ParseAmount(request.params[6], false);  // buy : 1 ; sell : 2;
  uint8_t action = ParseDExAction(request.params[7]);

  std::vector<unsigned char> payload;

  RequireNoOtherDExOffer(fromAddress, propertyIdForSale);

  if (option == 1)
  {
      payload = CreatePayload_DEx(propertyIdForSale, amountForSale, price, paymentWindow, minAcceptFee, action);
  } else {
      RequireBalance(fromAddress, propertyIdForSale, amountForSale);
      payload = CreatePayload_DExSell(propertyIdForSale, amountForSale, price, paymentWindow, minAcceptFee, action);
  }

  LTCPriceOffer = price;
  // request the wallet build the transaction (and if needed commit it)
  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());
  // check error and return the txid (or raw hex depending on autocommit)
  if (result != 0) {
    throw JSONRPCError(result, error_str(result));
  } else {
    if (!autoCommit) {
      return rawHex;
    } else {
      // bool fSubtract = (action <= CMPTransaction::UPDATE); // no pending balances for cancels
      // PendingAdd(txid, fromAddress, MSC_TYPE_TRADE_OFFER, propertyIdForSale, amountForSale, fSubtract);
      return txid.GetHex();
    }
  }
}

static UniValue tl_senddexaccept(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() < 4 || request.params.size() > 5)
        throw runtime_error(
            RPCHelpMan{"tl_senddexaccept",
               "\nCreate and broadcast an accept offer for the specified token and amount.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address of the seller\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the token to purchase\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount to accept\n"},
                   {"override", RPCArg::Type::BOOL, RPCArg::Optional::OMITTED, "override minimum accept fee and payment window checks (use with caution!)\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_senddexaccept", "\"35URq1NN3xL6GeRKUP6vzaQVcxoJiiJKd8\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"15.0\"")
                   + HelpExampleRpc("tl_senddexaccept", "\"35URq1NN3xL6GeRKUP6vzaQVcxoJiiJKd8\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"15.0\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
    bool override = (request.params.size() > 4) ? request.params[4].get_bool(): false;

    // perform checks
    RequirePrimaryToken(propertyId);
    RequireMatchingDExOffer(toAddress, propertyId);

    if (!override) { // reject unsafe accepts - note client maximum tx fee will always be respected regardless of override here
        RequireSaneDExFee(toAddress, propertyId);
        RequireSaneDExPaymentWindow(toAddress, propertyId);
    }

#ifdef ENABLE_WALLET
    // use new 0.10 custom fee to set the accept minimum fee appropriately
    int64_t nMinimumAcceptFee = 0;
    {
        LOCK(cs_tally);
        const CMPOffer* sellOffer = DEx_getOffer(toAddress, propertyId);
        if (sellOffer == nullptr) throw JSONRPCError(RPC_TYPE_ERROR, "Unable to load sell offer from the distributed exchange");
        nMinimumAcceptFee = sellOffer->getMinFee();
    }

    // temporarily update the global transaction fee to pay enough for the accept fee
    CFeeRate payTxFeeOriginal = wallet->m_pay_tx_fee;
    wallet->m_pay_tx_fee = CFeeRate(nMinimumAcceptFee, 225); // TODO: refine!
    // fPayAtLeastCustomFee = true;
#endif

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DExAccept(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

#ifdef ENABLE_WALLET
    // set the custom fee back to original
    wallet->m_pay_tx_fee = payTxFeeOriginal;
#endif

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendissuancefixed(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 10)
        throw runtime_error(
            RPCHelpMan{"tl_sendissuancefixed",
               "\nCreate new tokens with fixed supply.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
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
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendissuancefixed", "\"3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" \"1000000\"")
                   + HelpExampleRpc("tl_sendissuancefixed", "\"3Ck2kEGLJtZw9ENj2tameMCtS3HB7uRar3\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", \"1000000\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint16_t type = ParsePropertyType(request.params[2]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[3]);
    std::string category = ParseText(request.params[4]);
    std::string subcategory = ParseText(request.params[5]);
    std::string name = ParseText(request.params[6]);
    std::string url = ParseText(request.params[7]);
    std::string data = ParseText(request.params[8]);
    int64_t amount = ParseAmount(request.params[9], type);

    // perform checks
    RequirePropertyName(name);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceFixed(ecosystem, type, previousId, category, subcategory, name, url, data, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendissuancemanaged(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 9)
        throw runtime_error(
            RPCHelpMan{"tl_sendissuancemanaged",
               "\nCreate new tokens with manageable supply.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
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
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendissuancemanaged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
                   + HelpExampleRpc("tl_sendissuancemanaged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint16_t type = ParsePropertyType(request.params[2]);
    uint32_t previousId = ParsePreviousPropertyId(request.params[3]);
    std::string category = ParseText(request.params[4]);
    std::string subcategory = ParseText(request.params[5]);
    std::string name = ParseText(request.params[6]);
    std::string url = ParseText(request.params[7]);
    std::string data = ParseText(request.params[8]);

    // perform checks
    RequirePropertyName(name);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_IssuanceManaged(ecosystem, type, previousId, category, subcategory, name, url, data);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendsto(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 5)
        throw runtime_error(
            RPCHelpMan{"tl_sendsto",
               "\nCreate and broadcast a send-to-owners transaction.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to distribute\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount to distribute\n"},
                   {"redeemaddress", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "an address that can spend the transaction dust (sender by default)\n"},
                   {"distributionproperty", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "the identifier of the property holders to distribute to\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendsto", "\"32Z3tJccZuqQZ4PhJR2hxHC3tjgjA8cbqz\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 3 \"5000\"")
                   + HelpExampleRpc("tl_sendsto", "\"32Z3tJccZuqQZ4PhJR2hxHC3tjgjA8cbqz\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 3, \"5000\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));
    std::string redeemAddress = (request.params.size() > 3 && !ParseText(request.params[3]).empty()) ? ParseAddress(request.params[3]): "";
    uint32_t distributionPropertyId = (request.params.size() > 4) ? ParsePropertyId(request.params[4]) : propertyId;

    // perform checks
    RequireBalance(fromAddress, propertyId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_SendToOwners(propertyId, amount, distributionPropertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", redeemAddress, 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_SEND_TO_OWNERS, propertyId, amount);
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendgrant(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() < 4 || request.params.size() > 5)
        throw runtime_error(
            RPCHelpMan{"tl_sendgrant",
               "\nIssue or grant new units of managed tokens.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the receiver of the tokens (sender by default, can be \"\")\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to grant\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to create\n"},
                   {"memo", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "a text note attached to this transaction (none by default)\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendgrant", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" \"\" 51 \"7000\"")
                   + HelpExampleRpc("tl_sendgrant", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", \"\", 51, \"7000\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = !ParseText(request.params[1]).empty() ? ParseAddress(request.params[1]): "";
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));
    std::string memo = (request.params.size() > 4) ? ParseText(request.params[4]): "";

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Grant(propertyId, amount, memo);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendrevoke(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() < 3 || request.params.size() > 4)
        throw runtime_error(
            RPCHelpMan{"tl_sendrevoke",
               "\nRevoke units of managed tokens.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to revoke the tokens from\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to revoke\n"},
                   {"amount", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to revoke\n"},
                   {"memo", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "a text note attached to this transaction (none by default)\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendrevoke", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" \"\" 51 \"100\"")
                   + HelpExampleRpc("tl_sendrevoke", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", \"\", 51, \"100\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], isPropertyDivisible(propertyId));
    std::string memo = (request.params.size() > 3) ? ParseText(request.params[3]): "";

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);
    RequireBalance(fromAddress, propertyId, amount);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Revoke(propertyId, amount, memo);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendtrade(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 5)
        throw runtime_error(
            RPCHelpMan{"tl_sendtrade",
               "\nPlace a trade offer on the distributed token exchange.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to trade with\n"},
                   {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to list for sale\n"},
                   {"amountforsale", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to list for sale\n"},
                   {"propertiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens desired in exchange\n"},
                   {"amountdesired", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens desired in exchange\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendtrade", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 31 \"250.0\" 1 \"10.0\"")
                   + HelpExampleRpc("tl_sendtrade", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 31, \"250.0\", 1, \"10.0\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
    int64_t amountForSale = ParseAmount(request.params[2], isPropertyDivisible(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(request.params[3]);
    int64_t amountDesired = ParseAmount(request.params[4], isPropertyDivisible(propertyIdDesired));

    // perform checks
    RequireExistingProperty(propertyIdForSale);
    RequireExistingProperty(propertyIdDesired);
    RequireBalance(fromAddress, propertyIdForSale, amountForSale);
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExTrade(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_METADEX_TRADE, propertyIdForSale, amountForSale);
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendcanceltradesbyprice(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 5)
        throw runtime_error(
            RPCHelpMan{"tl_sendcanceltradesbyprice",
               "\nCancel offers on the distributed token exchange with the specified price.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to trade with\n"},
                   {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens listed for sale\n"},
                   {"amountforsale", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to listed for sale\n"},
                   {"propertiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens desired in exchange\n"},
                   {"amountdesired", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens desired in exchange\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendcanceltradesbyprice", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 31 \"100.0\" 1 \"5.0\"")
                   + HelpExampleRpc("tl_sendcanceltradesbyprice", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 31, \"100.0\", 1, \"5.0\"")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
    int64_t amountForSale = ParseAmount(request.params[2], isPropertyDivisible(propertyIdForSale));
    uint32_t propertyIdDesired = ParsePropertyId(request.params[3]);
    int64_t amountDesired = ParseAmount(request.params[4], isPropertyDivisible(propertyIdDesired));

    // perform checks
    RequireExistingProperty(propertyIdForSale);
    RequireExistingProperty(propertyIdDesired);
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);
    // TODO: check, if there are matching offers to cancel

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPrice(propertyIdForSale, amountForSale, propertyIdDesired, amountDesired);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_METADEX_CANCEL_PRICE, propertyIdForSale, amountForSale, false);
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendcanceltradesbypair(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            RPCHelpMan{"tl_sendcanceltradesbypair",
               "\nCancel all offers on the distributed token exchange with the given currency pair.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to trade with\n"},
                   {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens listed for sale\n"},
                   {"propertiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens desired in exchange\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendcanceltradesbypair", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 1 31")
                   + HelpExampleRpc("tl_sendcanceltradesbypair", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 1, 31")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyIdForSale = ParsePropertyId(request.params[1]);
    uint32_t propertyIdDesired = ParsePropertyId(request.params[2]);

    // perform checks
    RequireExistingProperty(propertyIdForSale);
    RequireExistingProperty(propertyIdDesired);
    RequireSameEcosystem(propertyIdForSale, propertyIdDesired);
    RequireDifferentIds(propertyIdForSale, propertyIdDesired);
    // TODO: check, if there are matching offers to cancel

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelPair(propertyIdForSale, propertyIdDesired);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_METADEX_CANCEL_PAIR, propertyIdForSale, 0, false);
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendcancelalltrades(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            RPCHelpMan{"tl_sendcancelalltrades",
               "\nCancel all offers on the distributed token exchange.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to trade with\n"},
                   {"ecosystem", RPCArg::Type::NUM, RPCArg::Optional::NO, "the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendcancelalltrades", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 1")
                   + HelpExampleRpc("tl_sendcancelalltrades", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 1")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);

    // perform checks
    // TODO: check, if there are matching offers to cancel

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_MetaDExCancelEcosystem(ecosystem);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_METADEX_CANCEL_ECOSYSTEM, ecosystem, 0, false);
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendchangeissuer(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            RPCHelpMan{"tl_sendchangeissuer",
               "\nChange the issuer on record of the given tokens.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address associated with the tokens\n"},
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to transfer administrative control to\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendchangeissuer", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 3")
                   + HelpExampleRpc("tl_sendchangeissuer", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 3")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);

    // perform checks
    RequireExistingProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ChangeIssuer(propertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendenablefreezing(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            RPCHelpMan{"tl_sendenablefreezing",
               "\nEnables address freezing for a centrally managed property.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the issuer of the tokens\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendenablefreezing", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 3")
                   + HelpExampleRpc("tl_sendenablefreezing", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 3")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_EnableFreezing(propertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_senddisablefreezing(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            RPCHelpMan{"tl_senddisablefreezing",
               "\nDisables address freezing for a centrally managed property.\n"
               "\nIMPORTANT NOTE:  Disabling freezing for a property will UNFREEZE all frozen addresses for that property!",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the issuer of the tokens\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_senddisablefreezing", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 3")
                   + HelpExampleRpc("tl_senddisablefreezing", "\"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 3")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint32_t propertyId = ParsePropertyId(request.params[1]);

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DisableFreezing(propertyId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendfreeze(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            RPCHelpMan{"tl_sendfreeze",
               "\nFreeze an address for a centrally managed token.\n"
               "\nNote: Only the issuer may freeze tokens, and only if the token is of the managed type with the freezing option enabled.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from (must be the issuer of the property)\n"},
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to freeze tokens for\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property to freeze tokens for (must be managed type and have freezing option enabled)\n"},
                   {"amount", RPCArg::Type::NUM, RPCArg::Optional::NO, "the amount of tokens to freeze (note: this is unused - once frozen an address cannot send any transactions for the property)\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendfreeze", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 1 0")
                   + HelpExampleRpc("tl_sendfreeze", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 1, 0")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string refAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_FreezeTokens(propertyId, amount, refAddress);

    // request the wallet build the transaction (and if needed commit it)
    // Note: no ref address is sent to WalletTxBuilder as the ref address is contained within the payload
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendunfreeze(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            RPCHelpMan{"tl_sendunfreeze",
               "\nUnfreezes an address for a centrally managed token.\n"
               "\nNote: Only the issuer may unfreeze tokens.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from (must be the issuer of the property)\n"},
                   {"toaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to unfreeze tokens for\n"},
                   {"propertyid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the property to unfreeze tokens for (must be managed type and have freezing option enabled)\n"},
                   {"amount", RPCArg::Type::NUM, RPCArg::Optional::NO, "the amount of tokens to unfreeze (note: this is unused - once frozen an address cannot send any transactions for the property)\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendunfreeze", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\" 1 0")
                   + HelpExampleRpc("tl_sendunfreeze", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", 1, 0")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string refAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], isPropertyDivisible(propertyId));

    // perform checks
    RequireExistingProperty(propertyId);
    RequireManagedProperty(propertyId);
    RequireTokenIssuer(fromAddress, propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_UnfreezeTokens(propertyId, amount, refAddress);

    // request the wallet build the transaction (and if needed commit it)
    // Note: no ref address is sent to WalletTxBuilder as the ref address is contained within the payload
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendactivation(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            RPCHelpMan{"tl_sendactivation",
               "\nActivate a protocol feature.\n"
               "\nNote: Trade Layer ignores activations from unauthorized sources.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
                   {"featureid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the feature to activate\n"},
                   {"block", RPCArg::Type::NUM, RPCArg::Optional::NO, "the activation block\n"},
                   {"minclientversion", RPCArg::Type::NUM, RPCArg::Optional::NO, "the minimum supported client version\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1 370000 999")
                   + HelpExampleRpc("tl_sendactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1, 370000, 999")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint16_t featureId = request.params[1].get_int();
    uint32_t activationBlock = request.params[2].get_int();
    uint32_t minClientVersion = request.params[3].get_int();

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ActivateFeature(featureId, activationBlock, minClientVersion);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_senddeactivation(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            RPCHelpMan{"tl_senddeactivation",
               "\nDeactivate a protocol feature.  For Emergency Use Only.\n"
               "\nNote: Trade Layer ignores deactivations from unauthorized sources.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
                   {"featureid", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the feature to activate\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_senddeactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\" 1")
                   + HelpExampleRpc("tl_senddeactivation", "\"1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P\", 1")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint16_t featureId = request.params[1].get_int64();

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DeactivateFeature(featureId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

static UniValue tl_sendalert(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif

    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            RPCHelpMan{"tl_sendalert",
               "\nCreates and broadcasts an Trade Layer alert.\n"
               "\nNote: Trade Layer ignores alerts from unauthorized sources.\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
                   {"alerttype", RPCArg::Type::NUM, RPCArg::Optional::NO, "the alert type\n"},
                   {"expiryvalue", RPCArg::Type::NUM, RPCArg::Optional::NO, "the value when the alert expires (depends on alert type)\n"},
                   {"message", RPCArg::Type::STR, RPCArg::Optional::NO, "the user-faced alert message\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   HelpExampleCli("tl_sendalert", "")
                   + HelpExampleRpc("tl_sendalert", "")
               }
            }.ToString());

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    int64_t tempAlertType = request.params[1].get_int64();
    if (tempAlertType < 1 || 65535 < tempAlertType) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Alert type is out of range");
    }
    uint16_t alertType = static_cast<uint16_t>(tempAlertType);
    int64_t tempExpiryValue = request.params[2].get_int64();
    if (tempExpiryValue < 1 || 4294967295LL < tempExpiryValue) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Expiry value is out of range");
    }
    uint32_t expiryValue = static_cast<uint32_t>(tempExpiryValue);
    std::string alertMessage = ParseText(request.params[3]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_TradeLayerAlert(alertType, expiryValue, alertMessage);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}


static UniValue trade_MP(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 6)
        throw runtime_error(
            RPCHelpMan{"trade_MP",
               "\nNote: this command is depreciated, and was replaced by:\n",
               {
                   {"fromaddress", RPCArg::Type::STR, RPCArg::Optional::NO, "the address to send from\n"},
                   {"propertyidforsale", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens to list for sale\n"},
                   {"amountforsale", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens to listed for sale\n"},
                   {"propertyiddesired", RPCArg::Type::NUM, RPCArg::Optional::NO, "the identifier of the tokens desired in exchange\n"},
                   {"amountdesired", RPCArg::Type::STR, RPCArg::Optional::NO, "the amount of tokens desired in exchange\n"},
                   {"action", RPCArg::Type::NUM, RPCArg::Optional::NO, "trade action to take\n"},
               },
               RPCResult{
                   "\"hash\"                  (string) the hex-encoded transaction hash\n"
               },
               RPCExamples{
                   " - sendtrade_TL\n"
                   " - sendcanceltradebyprice_TL\n"
                   " - sendcanceltradebypair_TL\n"
                   " - sendcanceltradebypair_TL\n"
               }
            }.ToString());

    UniValue values(UniValue::VARR);
    uint8_t action = ParseMetaDExAction(request.params[5]);

    // Forward to the new commands, based on action value
    switch (action) {
        case CMPTransaction::ADD:
        {
            values.push_back(request.params[0]); // fromAddress
            values.push_back(request.params[1]); // propertyIdForSale
            values.push_back(request.params[2]); // amountForSale
            values.push_back(request.params[3]); // propertyIdDesired
            values.push_back(request.params[4]); // amountDesired
            return tl_sendtrade(request);
        }
        case CMPTransaction::CANCEL_AT_PRICE:
        {
            values.push_back(request.params[0]); // fromAddress
            values.push_back(request.params[1]); // propertyIdForSale
            values.push_back(request.params[2]); // amountForSale
            values.push_back(request.params[3]); // propertyIdDesired
            values.push_back(request.params[4]); // amountDesired
            return tl_sendcanceltradesbyprice(request);
        }
        case CMPTransaction::CANCEL_ALL_FOR_PAIR:
        {
            values.push_back(request.params[0]); // fromAddress
            values.push_back(request.params[1]); // propertyIdForSale
            values.push_back(request.params[3]); // propertyIdDesired
            return tl_sendcanceltradesbypair(request);
        }
        case CMPTransaction::CANCEL_EVERYTHING:
        {
            uint8_t ecosystem = 0;
            if (isMainEcosystemProperty(request.params[1].get_int64())
                    && isMainEcosystemProperty(request.params[3].get_int64())) {
                ecosystem = TL_PROPERTY_MSC;
            }
            if (isTestEcosystemProperty(request.params[1].get_int64())
                    && isTestEcosystemProperty(request.params[3].get_int64())) {
                ecosystem = TL_PROPERTY_TMSC;
            }
            values.push_back(request.params[0]); // fromAddress
            values.push_back(ecosystem);
            return tl_sendcancelalltrades(request);
        }
    }

    throw JSONRPCError(RPC_TYPE_ERROR, "Invalid action (1,2,3,4 only)");
}

static UniValue tl_send_dex_payment(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_send_dex_payment \"fromaddress\" \"toaddress\"amount\" \n"

            "\nCreate and broadcast a dex payment.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. toaddress            (string, required) the address of the receiver\n"
            "3. amount               (string, required) the amount of Litecoins to send\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_send_dex_payment", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\"100.0\"")
            + HelpExampleRpc("tl_send_dex_payment", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\",\"100.0\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    int64_t amount = ParseAmount(request.params[2], true);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_DEx_Payment();

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;

    int result = WalletTxBuilder(fromAddress, toAddress,"",amount, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue tl_sendvesting(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
  if (request.fHelp || request.params.size() < 4 || request.params.size() > 6)
    throw runtime_error(
			"tl_send \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"referenceamount\" )\n"

			"\nCreate and broadcast a simple send transaction.\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to send from\n"
			"2. toaddress            (string, required) the address of the receiver\n"
			"3. propertyid           (number, required) the identifier of the tokens to send\n"
			"4. amount               (string, required) the amount of vesting tokens to send\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_send", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
			+ HelpExampleRpc("tl_send", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
			);

  // obtain parameters & info
  std::string fromAddress = ParseAddress(request.params[0]);
  std::string toAddress = ParseAddress(request.params[1]);
  uint32_t propertyId = ParsePropertyId(request.params[2]); /** id=3 Vesting Tokens**/
  int64_t amount = ParseAmount(request.params[3], true);

  PrintToLog("propertyid = %d\n", propertyId);
  PrintToLog("amount = %d\n", amount);
  PrintToLog("fromAddress = %s", fromAddress);
  PrintToLog("toAddress = %s", toAddress);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_SendVestingTokens(propertyId, amount);

  // request the wallet build the transaction (and if needed commit it)
  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, toAddress,"",0, payload, txid, rawHex, autoCommit, pwallet.get());
  // check error and return the txid (or raw hex depending on autocommit)
  if (result != 0) {
    throw JSONRPCError(result, error_str(result));
  } else {
    if (!autoCommit) {
      return rawHex;
    } else {
      PendingAdd(txid, fromAddress, MSC_TYPE_SEND_VESTING, propertyId, amount);
      return txid.GetHex();
    }
  }
}

UniValue tl_createcontract(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
  if (request.fHelp || request.params.size() != 8)
    throw runtime_error(
			"tl_createcontract \"fromaddress\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline ( earlybonus issuerpercentage )\n"

			"Create new Future Contract."

			"\nArguments:\n"
			"1. fromaddress               (string, required) the address to send from\n"
			"2. ecosystem                 (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
			"3. numerator                 (number, required) 4: ALL, 5: sLTC, 6: LTC.\n"
			"4. name                      (string, required) the name of the new tokens to create\n"
			"5. blocks until expiration   (number, required) life of contract, in blocks\n"
			"6. notional size             (number, required) notional size\n"
			"7. collateral currency       (number, required) collateral currency\n"
			"8. margin requirement        (number, required) margin requirement\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_createcontract", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2 4461 100 1 25")
			+ HelpExampleRpc("tl_createcontract", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2, 4461, 100, 1, 25")
			);

  std::string fromAddress = ParseAddress(request.params[0]);
  uint8_t ecosystem = ParseEcosystem(request.params[1]);
  uint32_t type = ParseContractType(request.params[2]);
  std::string name = ParseText(request.params[3]);
  uint32_t blocks_until_expiration = request.params[4].get_int();
  uint32_t notional_size = ParseAmount32t(request.params[5]);
  uint32_t collateral_currency = request.params[6].get_int();
  uint32_t margin_requirement = ParseAmount32t(request.params[7]);

  PrintToLog("\nRPC tl_createcontract: notional_size = %s\t margin_requirement = %s\t blocks_until_expiration = %d\t collateral_currency=%d\t ecosystem = %d\t type = %d\n", FormatDivisibleMP(notional_size), FormatDivisibleMP(margin_requirement), blocks_until_expiration, collateral_currency, ecosystem, type);

  RequirePropertyName(name);
  RequireSaneName(name);

  std::vector<unsigned char> payload = CreatePayload_CreateContract(ecosystem, type, name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement);

  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, "","",0, payload, txid, rawHex, autoCommit, pwallet.get());

  if ( result != 0 )
    {
      throw JSONRPCError(result, error_str(result));
    }
  else
    {
      if (!autoCommit)
	{
	  return rawHex;
	}
      else
	{
	  return txid.GetHex();
	}
    }
}

UniValue tl_create_oraclecontract(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
  if (request.fHelp || request.params.size() != 9)
    throw runtime_error(
			"tl_create_oraclecontract \"address\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\" propertyiddesired tokensperunit deadline ( earlybonus issuerpercentage )\n"

			"Create new Oracle Future Contract."

			"\nArguments:\n"
			"1. oracle address            (string, required) the address to send from (admin)\n"
			"2. ecosystem                 (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)\n"
			"3. numerator                 (number, required) 4: ALL, 5: sLTC, 6: LTC.\n"
			"4. name                      (string, required) the name of the new tokens to create\n"
			"5. blocks until expiration   (number, required) life of contract, in blocks\n"
			"6. notional size             (number, required) notional size\n"
			"7. collateral currency       (number, required) collateral currency\n"
			"8. margin requirement        (number, required) margin requirement\n"
      "9. backup address            (string, required) backup admin address contract\n"

			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_create_oraclecontract", "2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\" 2 \"100\" 1483228800 30 2 4461 100 1 25")
			+ HelpExampleRpc("tl_create_oraclecontract", "2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\", 2, \"100\", 1483228800, 30, 2, 4461, 100, 1, 25")
			);

  std::string fromAddress = ParseAddress(request.params[0]);
  uint8_t ecosystem = ParseEcosystem(request.params[1]);
  uint32_t type = ParseContractType(request.params[2]);
  std::string name = ParseText(request.params[3]);
  uint32_t blocks_until_expiration = request.params[4].get_int();
  uint32_t notional_size = ParseAmount32t(request.params[5]);
  uint32_t collateral_currency = request.params[6].get_int();
  uint32_t margin_requirement = ParseAmount32t(request.params[7]);
  std::string oracleAddress = ParseAddress(request.params[8]);

  PrintToLog("\nRPC tl_create_oraclecontract: notional_size = %s\t margin_requirement = %s\t blocks_until_expiration = %d\t collateral_currency=%d\t ecosystem = %d\t type = %d\n", FormatDivisibleMP(notional_size), FormatDivisibleMP(margin_requirement), blocks_until_expiration, collateral_currency, ecosystem, type);

  RequirePropertyName(name);
  RequireSaneName(name);

  std::vector<unsigned char> payload = CreatePayload_CreateOracleContract(ecosystem, type, name, blocks_until_expiration, notional_size, collateral_currency, margin_requirement);

  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, oracleAddress,"",0, payload, txid, rawHex, autoCommit, pwallet.get());

  if ( result != 0 )
    {
      throw JSONRPCError(result, error_str(result));
    }
  else
    {
      if (!autoCommit)
	{
	  return rawHex;
	}
      else
	{
	  return txid.GetHex();
	}
    }
}

UniValue tl_tradecontract(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
  if (request.fHelp || request.params.size() != 6)
    throw runtime_error(
			"tl_tradecontract \"fromaddress\" propertyidforsale \"amountforsale\" propertiddesired \"amountdesired\"\n"

			"\nPlace a trade offer on the distributed Futures Contracts exchange.\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to trade with\n"
			"2. propertyidforsale    (number, required) the identifier of the contract to list for trade\n"
			"3. amountforsale        (number, required) the amount of contracts to trade\n"
			"4. effective price      (number, required) limit price desired in exchange\n"
			"5. trading action       (number, required) 1 to BUY contracts, 2 to SELL contracts \n"
			"6. leverage             (number, required) leverage (2x, 3x, ... 10x)\n"
			"\nResult:\n"
			"\"payload\"             (string) the hex-encoded payload\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_tradecontract", "31\"250.0\"1\"10.0\"70.0\"80.0\"")
			+ HelpExampleRpc("tl_tradecontract", "31,\"250.0\",1,\"10.0,\"70.0,\"80.0\"")
			);

  std::string fromAddress = ParseAddress(request.params[0]);
  std::string name_traded = ParseText(request.params[1]);
  int64_t amountForSale = ParseAmountContract(request.params[2]);
  uint64_t effective_price = ParseEffectivePrice(request.params[3]);
  uint8_t trading_action = ParseContractDexAction(request.params[4]);
  uint64_t leverage = ParseLeverage(request.params[5]);

  //RequireCollateral(fromAddress,name_traded);

  std::vector<unsigned char> payload = CreatePayload_ContractDexTrade(name_traded, amountForSale, effective_price, trading_action, leverage);

  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

  if (result != 0)
    {
      throw JSONRPCError(result, error_str(result));
    }
  else
    {
      if (!autoCommit)
	{
	  return rawHex;
        }
      else
	{ //TODO: PendingAdd function
	  // PendingAdd(txid, fromAddress, MSC_TYPE_CONTRACTDEX_TRADE, propertyIdForSale, amountForSale);
	  return txid.GetHex();
        }
    }
}


UniValue tl_cancelallcontractsbyaddress(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
  if (request.fHelp || request.params.size() != 3)
    throw runtime_error(
			"tl_cancelallcontractsbyaddress \"fromaddress\" ecosystem\n"

			"\nCancel all offers on a given Futures Contract .\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to trade with\n"
			"2. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"
			"3. contractId           (number, required) the Id of Future Contract \n"
			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_cancelallcontractsbyaddress", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 1, 3")
			+ HelpExampleRpc("tl_cancelallcontractsbyaddress", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 1, 3")
			);

  // obtain parameters & info
  std::string fromAddress = ParseAddress(request.params[0]);
  uint8_t ecosystem = ParseEcosystem(request.params[1]);
  std::string name_traded = ParseText(request.params[2]);

  struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_traded);
  uint32_t contractId = pfuture->fco_propertyId;

  // perform checks
  RequireContract(contractId);
  // check, if there are matching offers to cancel
  RequireContractOrder(fromAddress, contractId);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_ContractDexCancelEcosystem(ecosystem, contractId);

  // request the wallet build the transaction (and if needed commit it)
  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

  // check error and return the txid (or raw hex depending on autocommit)
  if (result != 0) {
    throw JSONRPCError(result, error_str(result));
  } else {
    if (!autoCommit) {
      return rawHex;
    } else {
      PendingAdd(txid, fromAddress, MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM, ecosystem, 0, false);
      return txid.GetHex();
    }
  }
}

UniValue tl_closeposition(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_closeposition \"fromaddress\" ecosystem\n"

            "\nClose the position on a given Futures Contract .\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address to trade with\n"
            "2. ecosystem            (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)\n"
            "3. contractId           (number, required) the Id of Future Contract \n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_closeposition", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 1, 3")
            + HelpExampleRpc("tl_closeposition", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 1, 3")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    uint8_t ecosystem = ParseEcosystem(request.params[1]);
    uint32_t contractId = ParsePropertyId(request.params[2]);
    // perform checks
    RequireContract(contractId);
    // TODO: check, if there are matching offers to cancel

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ContractDexClosePosition(ecosystem, contractId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            PendingAdd(txid, fromAddress, MSC_TYPE_CONTRACTDEX_CANCEL_ECOSYSTEM, ecosystem, 0, false);
            return txid.GetHex();
        }
    }
}

UniValue tl_cancelorderbyblock(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_cancelorderbyblock \"fromaddress\" ecosystem\n"

            "\nCancel an specific offer on the distributed token exchange.\n"

            "\nArguments:\n"
            "1. address         (string, required) the txid of order to cancel\n"
            "2. block           (number, required) the block of order to cancel\n"
            "2. idx             (number, required) the idx in block of order to cancel\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_cancelorderbyblock", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\" 1, 2")
            + HelpExampleRpc("tl_cancelorderbyblock", "\"3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR\", 1, 2")
        );

    // obtain parameters & info
       std::string fromAddress = ParseAddress(request.params[0]);
       int block = static_cast<int>(ParseNewValues(request.params[1]));
       int idx = static_cast<int>(ParseNewValues(request.params[2]));

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_ContractDexCancelOrderByTxId(block,idx);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    //check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue tl_setoracle(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "tl_setoracle \"fromaddress\" \"contract name\" price\n"

            "\nSet the price for an oracle address.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the oracle address for the Future Contract\n"
            "2. contract name        (string, required) the name of the Future Contract\n"
            "3. high price           (number, required) the highest price of the asset\n"
            "4. low price            (number, required) the lowest price of the asset\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_setoracle", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" ,\"3HTHRxu3aSDV4de+akjC7VmsiUp7c6dfbvs\" ,\"Contract 1\"")
            + HelpExampleRpc("tl_setoracle", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", \"Contract 1\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string name_contract = ParseText(request.params[1]);
    uint64_t high = ParseEffectivePrice(request.params[2]);
    uint64_t low = ParseEffectivePrice(request.params[3]);
    struct FutureContractObject *pfuture_contract = getFutureContractObject(ALL_PROPERTY_TYPE_ORACLE_CONTRACT, name_contract);
    uint32_t contractId = pfuture_contract->fco_propertyId;
    std::string oracleAddress = pfuture_contract->fco_issuer;

    // checks

    if (oracleAddress != fromAddress)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "address is not the oracle address of contract");

    RequireExistingProperty(contractId);
    RequireOracleContract(contractId);


    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Set_Oracle(contractId,high,low);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue tl_change_oracleref(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 3)
        throw runtime_error(
            "tl_change_oracleref \"fromaddress\" \"toaddress\" contract name\n"

            "\nChange the issuer on record of the Oracle Future Contract.\n"

            "\nArguments:\n"
            "1. fromaddress          (string, required) the address associated with the oracle Future Contract\n"
            "2. toaddress            (string, required) the address to transfer administrative control to\n"
            "3. contract name        (string, required) the name of the Future Contract\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_change_oracleref", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" ,\"3HTHRxu3aSDV4de+akjC7VmsiUp7c6dfbvs\" ,\"Contract 1\"")
            + HelpExampleRpc("tl_change_oracleref", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"3HTHRxu3aSDV4deakjC7VmsiUp7c6dfbvs\", \"Contract 1\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string toAddress = ParseAddress(request.params[1]);
    std::string name_contract = ParseText(request.params[2]);
    struct FutureContractObject *pfuture_contract = getFutureContractObject(ALL_PROPERTY_TYPE_ORACLE_CONTRACT, name_contract);
    uint32_t contractId = pfuture_contract->fco_propertyId;
    std::string oracleAddress = pfuture_contract->fco_issuer;

    // checks
    if (oracleAddress != fromAddress)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "address is not the oracle address of contract");

    RequireExistingProperty(contractId);
    RequireOracleContract(contractId);  //RequireOracleContract


    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Change_OracleRef(contractId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, toAddress, "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}


UniValue tl_oraclebackup(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_oraclebackup \"oracle address\" \"contract name\n"

            "\n Activation of backup address (backup is now the new oracle address).\n"

            "\nArguments:\n"
            "1. backup address          (string, required) the address associated with the oracle Future Contract\n"
            "2. contract name           (string, required) the name of the Future Contract\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_oraclebackup", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" ,\"Contract 1\"")
            + HelpExampleRpc("tl_oraclebackup", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"Contract 1\"")
        );

    // obtain parameters & info
    std::string fromAddress = ParseAddress(request.params[0]);
    std::string name_contract = ParseText(request.params[1]);
    struct FutureContractObject *pfuture_contract = getFutureContractObject(ALL_PROPERTY_TYPE_ORACLE_CONTRACT, name_contract);
    uint32_t contractId = pfuture_contract->fco_propertyId;
    std::string backupAddress = pfuture_contract->fco_backup_address;

    // checks
    if (backupAddress != fromAddress)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "address is not the backup address of contract");

    RequireExistingProperty(contractId);
    RequireOracleContract(contractId);  //RequireOracleContract


    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_OracleBackup(contractId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue tl_closeoracle(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_closeoracle \"backupaddress\" \"contract name\n"

            "\nClose an Oracle Future Contract.\n"

            "\nArguments:\n"
            "1. backup address         (string, required) the backup address associated with the oracle Future Contract\n"
            "2. contract name          (string, required) the name of the Oracle Future Contract\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_closeoracle", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\" , \"Contract 1\"")
            + HelpExampleRpc("tl_closeoracle", "\"1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu\", \"Contract 1\"")
        );

    // obtain parameters & info
    std::string backupAddress = ParseAddress(request.params[0]);
    std::string name_contract = ParseText(request.params[1]);
    struct FutureContractObject *pfuture_contract = getFutureContractObject(ALL_PROPERTY_TYPE_ORACLE_CONTRACT, name_contract);
    uint32_t contractId = pfuture_contract->fco_propertyId;
    std::string bckup_address = pfuture_contract->fco_backup_address;

    // checks
    if (bckup_address != backupAddress)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "address is not the backup address of contract");

    RequireExistingProperty(contractId);
    RequireOracleContract(contractId);  //RequireOracleContract


    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Close_Oracle(contractId);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(backupAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue tl_commit_tochannel(const JSONRPCRequest& request)
{
  #ifdef ENABLE_WALLET
      std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
      std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
  #else
      std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
  #endif
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "tl_commit_tochannel \"sender\" \"channel address\" \"propertyId\" \"amount\"vout\n"

            "\nCommit fundings into the channel.\n"

            "\nArguments:\n"
            "1. sender                 (string, required) the sender address that commit into the channel\n"
            "2. channel address        (string, required) multisig address of channel\n"
            "3. propertyId             (number, required) the propertyId of token commited into the channel\n"
            "4. amount                 (number, required) amount of tokens traded in the channel\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"

            + HelpExampleCli("tl_commit_tochannel", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 3 100 \"1\"")
            + HelpExampleRpc("tl_commit_tochannel", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\",3, 100, \"1\"")
        );

    // obtain parameters & info
    std::string senderAddress = ParseAddress(request.params[0]);
    std::string channelAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], true);


    RequireExistingProperty(propertyId);
    RequireBalance(senderAddress, propertyId, amount);

    PrintToLog("channelAddress inside rpctx : %s\n",channelAddress);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Commit_Channel(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(senderAddress, channelAddress,"", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue tl_withdrawal_fromchannel(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "tl_withdrawal_fromchannel \"sender\" \"channel address\" \"propertyId\" \"amount\"vout\n"

            "\nwithdrawal from the channel.\n"

            "\nArguments:\n"
            "1. sender                 (string, required) the sender address that commit into the channel\n"
            "2. channel address        (string, required) multisig address of channel\n"
            "3. propertyId             (number, required) the propertyId of token commited into the channel\n"
            "4. amount                 (number, required) amount to withdrawal from channel\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_withdrawal_fromchannel", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 3 100 \"1\"")
            + HelpExampleRpc("tl_withdrawal_fromchannel", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\",3, 100, \"1\"")
        );

    // obtain parameters & info
    std::string senderAddress = ParseAddress(request.params[0]);
    std::string channelAddress = ParseAddress(request.params[1]);
    uint32_t propertyId = ParsePropertyId(request.params[2]);
    int64_t amount = ParseAmount(request.params[3], true);


    RequireExistingProperty(propertyId);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Withdrawal_FromChannel(propertyId, amount);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(senderAddress, channelAddress,"", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue tl_create_channel(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 4)
        throw runtime_error(
            "tl_create_channel \"sender\" \"channel address\" \"propertyId\" \"amount\"vout\n"

            "\nsetting multisig address channel.\n"

            "\nArguments:\n"
            "1. first address            (string, required) the first address that commit into the channel\n"
            "2. second address           (string, required) the second address that commit into the channel\n"
            "3. channel address          (string, required) multisig address of channel\n"
            "4. blocks          (string, required) blocks until channel expiration\n"
            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_withdrawal_fromchannel", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 3 100 \"1\"")
            + HelpExampleRpc("tl_withdrawal_fromchannel", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\",3, 100, \"1\"")
        );

    // obtain parameters & info
    std::string firstAddress = ParseAddress(request.params[0]);
    std::string secondAddress = ParseAddress(request.params[1]);
    std::string channelAddress = ParseAddress(request.params[2]);
    uint32_t blocks = request.params[3].get_int();

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Create_Channel(channelAddress,blocks);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(firstAddress, secondAddress, "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue tl_new_id_registration(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 8)
        throw runtime_error(
            "tl_new_id_registration \"sender\" \"address\" \"website url\" \"company name\" \n"

            "\nsetting identity registrar Id number for address.\n"

            "\nArguments:\n"
            "1. sender                       (string, required) sender address\n"
            "2. channel address              (string, required) channel address\n"
            "3. website url                  (string, required) the second address that commit into the channel\n"
            "4. company name                 (string, required) multisig address of channel\n"
            "5. token/token permission       (int, required) trading token for tokens (0 = false, 1 = true)\n"
            "6. ltc/token permission         (int, required) trading litecoins for tokens (0 = false, 1 = true)\n"
            "7. native-contract permission   (int, required) trading native contracts (0 = false, 1 = true)\n"
            "8. oracle-contract permission   (int, required) trading oracle contracts (0 = false, 1 = true)\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_new_id_registration", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"www.companyone.com\" company one , 1,0,0,0 \"")
            + HelpExampleRpc("tl_new_id_registration", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"www.companyone.com\",company one, 1,1,0,0 \"")
        );

    // obtain parameters & info
    std::string sender = ParseAddress(request.params[0]);
    std::string address = ParseAddress(request.params[1]);
    std::string website = ParseText(request.params[2]);
    std::string name = ParseText(request.params[3]);
    uint8_t tokens = ParsePermission(request.params[4]);
    uint8_t ltc = ParsePermission(request.params[5]);
    uint8_t natives = ParsePermission(request.params[6]);
    uint8_t oracles = ParsePermission(request.params[7]);

    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_New_Id_Registration(website, name, tokens, ltc, natives, oracles);

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(sender, address, "", 0, payload, txid, rawHex, autoCommit, pwallet.get());
    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue tl_update_id_registration(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
    if (request.fHelp || request.params.size() != 2)
        throw runtime_error(
            "tl_update_id_registration \"address\" \"new address\" \n"

            "\nupdate the address on id registration.\n"

            "\nArguments:\n"
            "1. address                      (string, required) old address registered\n"
            "2. new address                  (string, required) new address into register\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"

            "\nExamples:\n"
            + HelpExampleCli("tl_update_id_registration", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" , \"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\"")
            + HelpExampleRpc("tl_update_id_registration", "\"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\",  \"1M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\"")
        );

    // obtain parameters & info
    std::string address = ParseAddress(request.params[0]);
    std::string newAddr = ParseAddress(request.params[1]);
    // create a payload for the transaction
    std::vector<unsigned char> payload = CreatePayload_Update_Id_Registration();

    // request the wallet build the transaction (and if needed commit it)
    uint256 txid;
    std::string rawHex;
    int result = WalletTxBuilder(address, newAddr, "", 0, payload, txid, rawHex, autoCommit, pwallet.get());
    // check error and return the txid (or raw hex depending on autocommit)
    if (result != 0) {
        throw JSONRPCError(result, error_str(result));
    } else {
        if (!autoCommit) {
            return rawHex;
        } else {
            return txid.GetHex();
        }
    }
}

UniValue tl_sendissuance_pegged(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
  if (request.fHelp || request.params.size() != 7)
    throw runtime_error(
			"tl_sendissuance_pegged\"fromaddress\" ecosystem type previousid \"category\" \"subcategory\" \"name\" \"url\" \"data\"\n"

			"\nCreate new pegged currency with manageable supply.\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to send from\n"
			"2. ecosystem            (string, required) the ecosystem to create the pegged currency in (1 for main ecosystem, 2 for test ecosystem)\n"
			"3. type                 (number, required) the type of the pegged to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
			"4. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
			"5. name                 (string, required) the name of the new pegged to create\n"
			"6. collateralcurrency  (number, required) the collateral currency for the new pegged \n"
			"7. future contract name  (number, required) the future contract name for the new pegged \n"
			"8. amount of pegged    (number, required) amount of pegged to create \n"
			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_sendissuance_pegged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\" 2 1 0 \"Companies\" \"Bitcoin Mining\" \"Quantum Miner\" \"\" \"\"")
			+ HelpExampleRpc("tl_sendissuance_pegged", "\"3HsJvhr9qzgRe3ss97b1QHs38rmaLExLcH\", 2, 1, 0, \"Companies\", \"Bitcoin Mining\", \"Quantum Miner\", \"\", \"\"")
			);

  // obtain parameters & info
  std::string fromAddress = ParseAddress(request.params[0]);
  uint8_t ecosystem = ParseEcosystem(request.params[1]);
  uint32_t previousId = ParsePreviousPropertyId(request.params[2]);
  std::string name = ParseText(request.params[3]);
  uint32_t propertyId = ParsePropertyId(request.params[4]);
  std::string name_traded = ParseText(request.params[5]);
  uint64_t amount = ParseAmount(request.params[6], isPropertyDivisible(propertyId));

  struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_traded);
  uint32_t contractId = pfuture->fco_propertyId;

  // perform checks
  RequirePeggedSaneName(name);

  // Checking existing
  RequireExistingProperty(propertyId);

  // Property must not be a future contract
  RequireNotContract(propertyId);

  // Checking for future contract
  RequireContract(contractId);

  // Checking for short position in given future contract
  RequireShort(fromAddress, contractId, amount);

  // checking for collateral balance, checking for short position in given contract
  RequireForPegged(fromAddress, propertyId, contractId, amount);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_IssuancePegged(ecosystem, previousId, name, propertyId, contractId, amount);

  // request the wallet build the transaction (and if needed commit it)
  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

  // check error and return the txid (or raw hex depending on autocommit)
  if (result != 0) {
    throw JSONRPCError(result, error_str(result));
  } else {
    if (!autoCommit) {
      return rawHex;
    } else {
      return txid.GetHex();
    }
  }
}

UniValue tl_send_pegged(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
  if (request.fHelp || request.params.size() != 4)
    throw runtime_error(
			"tl_send \"fromaddress\" \"toaddress\" propertyid \"amount\" ( \"redeemaddress\" \"referenceamount\" )\n"

			"\nSend the pegged currency to other addresses.\n"

			"\nArguments:\n"
			"1. fromaddress          (string, required) the address to send from\n"
			"2. toaddress            (string, required) the address of the receiver\n"
			"3. property name        (string, required) the identifier of the tokens to send\n"
			"4. amount               (string, required) the amount to send\n"


			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_send_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\" 1 \"100.0\"")
			+ HelpExampleRpc("tl_send_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", \"37FaKponF7zqoMLUjEiko25pDiuVH5YLEa\", 1, \"100.0\"")
			);

  // obtain parameters & info
  std::string fromAddress = ParseAddress(request.params[0]);
  std::string toAddress = ParseAddress(request.params[1]);
  std::string name_pegged = ParseText(request.params[2]);

  struct FutureContractObject *pfuture = getFutureContractObject(ALL_PROPERTY_TYPE_PEGGEDS, name_pegged);
  uint32_t propertyId = pfuture->fco_propertyId;

  RequirePeggedCurrency(propertyId);

  int64_t amount = ParseAmount(request.params[3], true);

  // perform checks
  RequireExistingProperty(propertyId);
  RequireBalance(fromAddress, propertyId, amount);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_SendPeggedCurrency(propertyId, amount);

  // request the wallet build the transaction (and if needed commit it)
  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, toAddress, "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

  // check error and return the txid (or raw hex depending on autocommit)
  if (result != 0) {
    throw JSONRPCError(result, error_str(result));
  } else {
    if (!autoCommit) {
      return rawHex;
    } else {
      PendingAdd(txid, fromAddress, MSC_TYPE_SEND_PEGGED_CURRENCY, propertyId, amount);
      return txid.GetHex();
    }
  }
}

UniValue tl_redemption_pegged(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(wallet);
#else
    std::unique_ptr<interfaces::Wallet> pwallet = interfaces::MakeWallet(nullptr);
#endif
  if (request.fHelp || request.params.size() != 4)
    throw runtime_error(
			"tl_redemption_pegged \"fromaddress\" propertyid \"amount\" ( \"redeemaddress\" distributionproperty )\n"

			"\n Redemption of the pegged currency .\n"

			"\nArguments:\n"
			"1. redeemaddress        (string, required) the address of owner \n"
			"2. name of pegged       (string, required) name of the tokens to redeem\n"
			"3. amount               (number, required) the amount of pegged currency for redemption"
			"4. name of contract     (string, required) the identifier of the future contract involved\n"
			"\nResult:\n"
			"\"hash\"                  (string) the hex-encoded transaction hash\n"

			"\nExamples:\n"
			+ HelpExampleCli("tl_redemption_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\" , 1")
			+ HelpExampleRpc("tl_redemption_pegged", "\"3M9qvHKtgARhqcMtM5cRT9VaiDJ5PSfQGY\", 1")
			);

  // obtain parameters & info
  std::string fromAddress = ParseAddress(request.params[0]);

  std::string name_pegged = ParseText(request.params[1]);
  std::string name_contract = ParseText(request.params[3]);
  struct FutureContractObject *pfuture_pegged = getFutureContractObject(ALL_PROPERTY_TYPE_PEGGEDS, name_pegged);
  uint32_t propertyId = pfuture_pegged->fco_propertyId;

  uint64_t amount = ParseAmount(request.params[2], true);

  struct FutureContractObject *pfuture_contract = getFutureContractObject(ALL_PROPERTY_TYPE_CONTRACT, name_contract);
  uint32_t contractId = pfuture_contract->fco_propertyId;

  // perform checks
  RequireExistingProperty(propertyId);
  RequirePeggedCurrency(propertyId);
  RequireContract(contractId);
  RequireBalance(fromAddress, propertyId, amount);

  // is given contract origin of pegged?
  RequireAssociation(propertyId,contractId);

  // create a payload for the transaction
  std::vector<unsigned char> payload = CreatePayload_RedemptionPegged(propertyId, contractId, amount);

  // request the wallet build the transaction (and if needed commit it)
  uint256 txid;
  std::string rawHex;
  int result = WalletTxBuilder(fromAddress, "", "", 0, payload, txid, rawHex, autoCommit, pwallet.get());

  // check error and return the txid (or raw hex depending on autocommit)
  if (result != 0) {
    throw JSONRPCError(result, error_str(result));
  } else {
    if (!autoCommit) {
      return rawHex;
    } else {
      return txid.GetHex();
    }
  }
}

static const CRPCCommand commands[] =
{ //  category                             name                            actor (function)               okSafeMode
  //  ------------------------------------ ------------------------------- ------------------------------ ----------
#ifdef ENABLE_WALLET
    { "trade layer (transaction creation)", "tl_sendrawtx",               &tl_sendrawtx,               {"fromaddress", "rawtransaction", "referenceaddress", "redeemaddress", "referenceamount"} },
    { "trade layer (transaction creation)", "tl_send",                    &tl_send,                    {"fromaddress", "toaddress", "propertyid", "amount", "redeemaddress", "referenceamount"} },
    { "trade layer (transaction creation)", "tl_senddexoffer",              &tl_senddexoffer,              {} },
    { "trade layer (transaction creation)", "tl_senddexaccept",           &tl_senddexaccept,           {"fromaddress", "toaddress", "propertyid", "amount", "override"} },
    { "trade layer (transaction creation)", "tl_sendissuancefixed",       &tl_sendissuancefixed,       {"fromaddress", "ecosystem", "type", "previousid", "category", "subcategory", "name", "url", "data", "amount"} },
    { "trade layer (transaction creation)", "tl_sendissuancemanaged",     &tl_sendissuancemanaged,     {"fromaddress", "ecosystem", "type", "previousid", "category", "subcategory", "name", "url", "data"} },
    { "trade layer (transaction creation)", "tl_sendtrade",               &tl_sendtrade,               {"fromaddress", "propertyidforsale", "amountforsale", "propertiddesired", "amountdesired"} },
    { "trade layer (transaction creation)", "tl_sendcanceltradesbyprice", &tl_sendcanceltradesbyprice, {"fromaddress", "propertyidforsale", "amountforsale", "propertiddesired", "amountdesired"} },
    { "trade layer (transaction creation)", "tl_sendcanceltradesbypair",  &tl_sendcanceltradesbypair,  {"fromaddress", "propertyidforsale", "propertiddesired"} },
    { "trade layer (transaction creation)", "tl_sendcancelalltrades",     &tl_sendcancelalltrades,     {"fromaddress", "ecosystem"} },
    { "trade layer (transaction creation)", "tl_sendsto",                 &tl_sendsto,                 {"fromaddress", "propertyid", "amount", "redeemaddress", "distributionproperty"} },
    { "trade layer (transaction creation)", "tl_sendgrant",               &tl_sendgrant,               {"fromaddress", "toaddress", "propertyid", "amount", "memo"} },
    { "trade layer (transaction creation)", "tl_sendrevoke",              &tl_sendrevoke,              {"fromaddress", "propertyid", "amount", "memo"} },
    { "trade layer (transaction creation)", "tl_sendchangeissuer",        &tl_sendchangeissuer,        {"fromaddress", "toaddress", "propertyid"} },
    { "trade layer (transaction creation)", "tl_sendall",                 &tl_sendall,                 {"fromaddress", "toaddress", "ecosystem", "redeemaddress", "referenceamount"} },
    { "trade layer (transaction creation)", "tl_sendenablefreezing",      &tl_sendenablefreezing,      {"fromaddress", "propertyid"} },
    { "trade layer (transaction creation)", "tl_senddisablefreezing",     &tl_senddisablefreezing,     {"fromaddress", "propertyid"} },
    { "trade layer (transaction creation)", "tl_sendfreeze",              &tl_sendfreeze,              {"fromaddress", "toaddress", "propertyid", "amount"} },
    { "trade layer (transaction creation)", "tl_sendunfreeze",            &tl_sendunfreeze,            {"fromaddress", "toaddress", "propertyid", "amount"} },
    { "hidden",                            "tl_senddeactivation",        &tl_senddeactivation,        {"fromaddress", "featureid"} },
    { "hidden",                            "tl_sendactivation",          &tl_sendactivation,          {"fromaddress", "featureid", "block", "minclientversion"} },
    { "hidden",                            "tl_sendalert",               &tl_sendalert,               {"fromaddress", "alerttype", "expiryvalue", "message"} },
    { "tl layer (transaction creation)", "tl_funded_send",             &tl_funded_send,             {"fromaddress", "toaddress", "propertyid", "amount", "feeaddress"} },
    { "tl layer (transaction creation)", "tl_funded_sendall",          &tl_funded_sendall,          {"fromaddress", "toaddress", "ecosystem", "feeaddress"} },
    { "trade layer (transaction creation)",  "tl_send_dex_payment",         &tl_send_dex_payment,                {} },
    { "trade layer (transaction creation)",  "tl_sendvesting",              &tl_sendvesting,                     {} },
    { "trade layer (transaction creation)",  "tl_createcontract",           &tl_createcontract,                  {} },
    { "trade layer (transaction creation)",  "tl_create_oraclecontract",    &tl_create_oraclecontract,           {} },
    { "trade layer (transaction creation)",  "tl_tradecontract",            &tl_tradecontract,                   {} },
    { "trade layer (transaction creation)",  "tl_closeposition",            &tl_closeposition,                   {} },
    { "trade layer (transaction creation)",  "tl_cancelorderbyblock",       &tl_cancelorderbyblock,              {} },
    { "trade layer (transaction creation)",  "tl_setoracle",                &tl_setoracle,                       {} },

    { "trade layer (transaction creation)",  "tl_change_oracleref",         &tl_change_oracleref,                {} },
    { "trade layer (transaction creation)",  "tl_oraclebackup",             &tl_oraclebackup,                    {} },
    { "trade layer (transaction creation)",  "tl_closeoracle",              &tl_closeoracle,                     {} },
    { "trade layer (transaction creation)",  "tl_commit_tochannel",         &tl_commit_tochannel,                {} },
    { "trade layer (transaction creation)",  "tl_withdrawal_fromchannel",   &tl_withdrawal_fromchannel,          {} },
    { "trade layer (transaction creation)",  "tl_create_channel",           &tl_create_channel,                  {} },
    { "trade layer (transaction cration)",   "tl_new_id_registration",      &tl_new_id_registration,             {} },
    { "trade layer (transaction cration)",   "tl_update_id_registration",   &tl_update_id_registration,          {} },
    { "trade layer (transaction cration)",   "tl_send_dex_payment",         &tl_send_dex_payment,                {} },
    { "trade layer (transaction creation)",  "tl_commit_tochannel",         &tl_commit_tochannel,                {} },

    /* depreciated: */
    { "hidden",                            "sendrawtx_MP",                 &tl_sendrawtx,               {"fromaddress", "rawtransaction", "referenceaddress", "redeemaddress", "referenceamount"} },
    { "hidden",                            "send_MP",                      &tl_send,                    {"fromaddress", "toaddress", "propertyid", "amount", "redeemaddress", "referenceamount"} },
    { "hidden",                            "sendtoowners_MP",              &tl_sendsto,                 {"fromaddress", "propertyid", "amount", "redeemaddress", "distributionproperty"} },
    { "hidden",                            "trade_MP",                     &trade_MP,                     {"fromaddress", "propertyidforsale", "amountforsale", "propertiddesired", "amountdesired", "action"} },
#endif
};

void RegisterTLTransactionCreationRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
