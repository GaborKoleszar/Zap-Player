#include <doctest.h>
#include <mpv/client.h>

TEST_CASE("mpv client api is linkable") {
    CHECK(mpv_client_api_version() >= MPV_MAKE_VERSION(2, 0));
}
