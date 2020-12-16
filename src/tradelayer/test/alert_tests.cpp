#include <tradelayer/notifications.h>
#include <tradelayer/version.h>

#include <util/system.h>
#include <test/test_bitcoin.h>
#include <tinyformat.h>

#include <boost/test/unit_test.hpp>

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

using namespace mastercore;

BOOST_FIXTURE_TEST_SUITE(tradelayer_alert_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(alert_positive_authorization)
{
    // Confirm authorized sources for mainnet
    BOOST_CHECK(CheckAlertAuthorization("17xr7sbehYY4YSZX9yuJe6gK9rrdRrZx26"));
    BOOST_CHECK(CheckAlertAuthorization("1883ZMsRJfzKNozUBJBTCxQ7EaiNioNDWz"));
    BOOST_CHECK(CheckAlertAuthorization("1HHv91gRxqBzQ3gydMob3LU8hqXcWoLfvd"));
    BOOST_CHECK(CheckAlertAuthorization("16oDZYCspsczfgKXVj3xyvsxH21NpEj94F"));  
}

BOOST_AUTO_TEST_CASE(alert_unauthorized_source)
{
    // Confirm unauthorized sources are not accepted
    BOOST_CHECK(!CheckAlertAuthorization("1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T"));
}

BOOST_AUTO_TEST_CASE(alert_manual_sources)
{
    std::vector<std::string> tlalertallowsender = gArgs.GetArgs("-tlalertallowsender");
    std::vector<std::string> tlalertignoresender = gArgs.GetArgs("-tlalertignoresender");

    // Add 1JwSSu as allowed source for alerts
    gArgs.ForceSetArg("-tlalertallowsender", "1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T");
    BOOST_CHECK_EQUAL(gArgs.GetArgs("-tlalertallowsender").size(), 1);
    BOOST_CHECK(CheckAlertAuthorization("1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T"));

    // Then ignore some sources explicitly
    gArgs.ForceSetArgs("-tlalertignoresender", {"1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T", "16oDZYCspsczfgKXVj3xyvsxH21NpEj94F"});
    BOOST_CHECK(CheckAlertAuthorization("1HHv91gRxqBzQ3gydMob3LU8hqXcWoLfvd")); // should still be authorized
    BOOST_CHECK(!CheckAlertAuthorization("1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T"));
    BOOST_CHECK(!CheckAlertAuthorization("16oDZYCspsczfgKXVj3xyvsxH21NpEj94F"));

    gArgs.ForceSetArgs("-tlalertallowsender", tlalertallowsender);
    gArgs.ForceSetArgs("-tlalertignoresender", tlalertignoresender);
}

BOOST_AUTO_TEST_CASE(alert_authorize_any_source)
{
    std::vector<std::string> tlalertallowsender = gArgs.GetArgs("-tlalertallowsender");

    // Allow any source (e.g. for tests!)
    gArgs.ForceSetArg("-tlalertallowsender", "any");
    BOOST_CHECK(CheckAlertAuthorization("1JwSSubhmg6iPtRjtyqhUYYH7bZg3Lfy1T"));
    BOOST_CHECK(CheckAlertAuthorization("137uFtQ5EgMsreg4FVvL3xuhjkYGToVPqs"));
    BOOST_CHECK(CheckAlertAuthorization("16oDZYCspsczfgKXVj3xyvsxH21NpEj94F"));

    gArgs.ForceSetArgs("-tlalertallowsender", tlalertallowsender);
}

BOOST_AUTO_TEST_SUITE_END()
