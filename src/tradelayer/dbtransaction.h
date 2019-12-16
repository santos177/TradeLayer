#ifndef BITCOIN_TRADELAYER_DBTRANSACTION_H
#define BITCOIN_TRADELAYER_DBTRANSACTION_H

#include <tradelayer/dbbase.h>

#include <fs.h>
#include <uint256.h>

#include <stdint.h>

#include <string>
#include <vector>

/** LevelDB based storage for storing Trade Layer transaction validation and position in block data.
 */
class CTLTransactionDB : public CDBBase
{
public:
    CTLTransactionDB(const fs::path& path, bool fWipe);
    virtual ~CTLTransactionDB();

    /** Stores position in block and validation result for a transaction. */
    void RecordTransaction(const uint256& txid, uint32_t posInBlock, int processingResult);

    /** Returns the position of a transaction in a block. */
    uint32_t FetchTransactionPosition(const uint256& txid);

    /** Returns the reason why a transaction is invalid. */
    std::string FetchInvalidReason(const uint256& txid);

private:
    /** Retrieves the serialized transaction details from the DB. */
    std::vector<std::string> FetchTransactionDetails(const uint256& txid);
};

namespace mastercore
{
    //! LevelDB based storage for storing Trade Layer transaction validation and position in block data
    extern CTLTransactionDB* pDbTransaction;
}

#endif // BITCOIN_TRADELAYER_DBTRANSACTION_H
