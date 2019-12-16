#include <tradelayer/version.h>

#include <clientversion.h>
#include <tinyformat.h>

#include <string>

#ifdef HAVE_BUILD_INFO
#include <obj/build.h>
#endif

#ifdef TRADELAYER_VERSION_STATUS
#    define TRADELAYER_VERSION_SUFFIX STRINGIZE(TRADELAYER_VERSION_STATUS)
#else
#    define TRADELAYER_VERSION_SUFFIX ""
#endif

//! Name of client reported in the user aagent message.
const std::string TL_CLIENT_NAME("TL");

//! Returns formatted Trade Layer version, e.g. "1.2.0" or "1.3.4.1"
const std::string TradeLayerVersion()
{
    if (TRADELAYER_VERSION_BUILD) {
        return strprintf("%d.%d.%d.%d",
                TRADELAYER_VERSION_MAJOR,
                TRADELAYER_VERSION_MINOR,
                TRADELAYER_VERSION_PATCH,
                TRADELAYER_VERSION_BUILD);
    } else {
        return strprintf("%d.%d.%d",
                TRADELAYER_VERSION_MAJOR,
                TRADELAYER_VERSION_MINOR,
                TRADELAYER_VERSION_PATCH);
    }
}

//! Returns formatted Bitcoin Core version, e.g. "0.10", "0.9.3"
const std::string BitcoinCoreVersion()
{
    if (CLIENT_VERSION_BUILD) {
        return strprintf("%d.%d.%d.%d",
                CLIENT_VERSION_MAJOR,
                CLIENT_VERSION_MINOR,
                CLIENT_VERSION_REVISION,
                CLIENT_VERSION_BUILD);
    } else {
        return strprintf("%d.%d.%d",
                CLIENT_VERSION_MAJOR,
                CLIENT_VERSION_MINOR,
                CLIENT_VERSION_REVISION);
    }
}
