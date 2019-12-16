#ifndef BITCOIN_TRADELAYER_WALLETCACHE_H
#define BITCOIN_TRADELAYER_WALLETCACHE_H

class uint256;

#include <vector>

namespace mastercore
{
/** Updates the cache and returns whether any wallet addresses were changed */
int WalletCacheUpdate();
}

#endif // BITCOIN_TRADELAYER_WALLETCACHE_H
