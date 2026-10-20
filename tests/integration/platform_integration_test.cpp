#include <catch2/catch_test_macros.hpp>

#include <interfaces/paths.hpp>
#include <interfaces/platform_info.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

TEST_CASE("platform_info::current offers only graphics backends the app can apply",
          "[integration][platform]") {
    const auto info = cube::platform::platform_info::current();

    // desktop/graphics_backend.cpp applies exactly these keys; a platform that
    // lists another would offer a choice that silently does nothing on the
    // next launch.
    constexpr std::array<std::string_view, 5> kApplicable{
        "d3d11", "d3d12", "vulkan", "opengl", "metal",
    };
    REQUIRE_FALSE(info.graphics_backends.empty());

    std::vector<std::string> keys;
    for (const auto& backend : info.graphics_backends) {
        INFO("backend key: " << backend.key);
        CHECK(std::ranges::find(kApplicable, backend.key) != kApplicable.end());
        CHECK_FALSE(backend.label.empty());
        keys.push_back(backend.key);
    }

    // Listed once: the Settings screen keys its chips by it.
    std::ranges::sort(keys);
    CHECK(std::ranges::adjacent_find(keys) == keys.end());
}

TEST_CASE("platform_info::current's automatic chain only names what the app can apply",
          "[integration][platform]") {
    const auto info = cube::platform::platform_info::current();

    // The chain is what the application falls back along when a window cannot
    // be made, so a name in it that the platform cannot apply would be a
    // fallback that does nothing. "software" is not in the Settings picker (it
    // is the floor of a chain, and --graphics-api=software), so it is allowed
    // where the platform says what it draws with.
    std::vector<std::string> applicable;
    for (const auto& backend : info.graphics_backends) {
        applicable.push_back(backend.key);
    }
    if (!info.software_graphics_api.empty()) {
        applicable.emplace_back("software");
        CHECK(std::ranges::find(applicable, info.software_graphics_api) != applicable.end());
    }
    for (const auto& key : info.automatic_graphics_backends) {
        INFO("automatic backend: " << key);
        CHECK(std::ranges::find(applicable, key) != applicable.end());
    }

    std::vector<std::string> chain = info.automatic_graphics_backends;
    std::ranges::sort(chain);
    CHECK(std::ranges::adjacent_find(chain) == chain.end());
}
