// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "desktop/graphics_backend.hpp"

#include <QDir>
#include <QSettings>
#include <QTemporaryDir>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

// What the guard remembers has to come back from the settings store exactly,
// because a launch that fails to read it would forget the strike that is meant
// to send it down the chain. (The choice itself is
// graphics_backend_choice_test.cpp; this is only the store.)
TEST_CASE("The guard's record survives the settings file", "[graphics]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath("settings.ini");

    cube::graphics::record written;
    written.pending = "vulkan";
    written.suspect = "d3d11";
    written.strikes = 1;
    written.failed = {
        {.backend = "vulkan",
         .build = "0.2.0-beta.1 / Qt 6.12.0",
         .when = 1'800'000'000,
         .setting = "auto"},
    };

    {
        QSettings settings(path, QSettings::IniFormat);
        cube::app::graphics::store_record(settings, written);
    }

    const QSettings reread(path, QSettings::IniFormat);
    const cube::graphics::record read = cube::app::graphics::load_record(reread);
    CHECK(read.pending == written.pending);
    CHECK(read.suspect == written.suspect);
    CHECK(read.strikes == written.strikes);
    REQUIRE(read.failed.size() == 1);
    CHECK(read.failed[0] == written.failed[0]);
}

TEST_CASE("An empty settings store is an empty record", "[graphics]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QSettings settings(directory.filePath("empty.ini"), QSettings::IniFormat);

    const cube::graphics::record record = cube::app::graphics::load_record(settings);
    CHECK(record == cube::graphics::record{});
}

TEST_CASE("A damaged failure entry is skipped, not trusted", "[graphics]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const QString path = directory.filePath("damaged.ini");
    {
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue("rendering/guard/failed",
                          QStringList{"vulkan", "vulkan\tb\tnot-a-number\tauto", "\tb\t5\tauto",
                                      "vulkan\tb\t5", "d3d12\tb\t7\tvulkan"});
    }
    const QSettings settings(path, QSettings::IniFormat);
    const auto record = cube::app::graphics::load_record(settings);
    REQUIRE(record.failed.size() == 1);
    CHECK(record.failed.front().backend == "d3d12");
    CHECK(record.failed.front().when == 7);
    CHECK(record.failed.front().setting == "vulkan");
    CHECK(record.failed.front().setting == "vulkan");
}

TEST_CASE("The build identity names the application and Qt", "[graphics]") {
    const std::string identity = cube::app::graphics::build_identity();
    CHECK_FALSE(identity.empty());
    CHECK(identity.find("Qt ") != std::string::npos);
}

namespace {

// A store of its own, so the test never touches the person's settings.
std::unique_ptr<QSettings> store_in(const QTemporaryDir& directory) {
    return std::make_unique<QSettings>(directory.filePath("launch.ini"), QSettings::IniFormat);
}

cube::graphics::platform chain_of(std::vector<std::string> automatic) {
    return {.available = {"d3d11", "d3d12", "vulkan", "opengl", "software"},
            .automatic = std::move(automatic)};
}

}  // namespace

TEST_CASE("A launch goes down the chain past a backend that cannot start here", "[graphics]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    std::vector<std::string> asked;
    const cube::app::graphics::Probe probe = [&](std::string_view backend) {
        asked.emplace_back(backend);
        return backend != "vulkan" && backend != "d3d12";
    };

    const cube::app::graphics::Launch launch(
        store_in(directory), {}, chain_of({"vulkan", "d3d12", "d3d11"}), "d3d11", probe);

    CHECK(launch.backend() == "d3d11");
    // Asked about each backend before the one chosen, and not about it.
    CHECK(asked == std::vector<std::string>{"vulkan", "d3d12"});
    CHECK(launch.choice().notes.size() == 2);

    // It is the pending one now: if this launch never draws, d3d11 is what gets the strike.
    const QSettings reread(directory.filePath("launch.ini"), QSettings::IniFormat);
    CHECK(cube::app::graphics::load_record(reread).pending == "d3d11");
}

TEST_CASE("The first backend that can start is used, and nothing after it is asked about",
          "[graphics]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    std::vector<std::string> asked;
    const cube::app::graphics::Probe probe = [&](std::string_view backend) {
        asked.emplace_back(backend);
        return true;
    };

    const cube::app::graphics::Launch launch(
        store_in(directory), {}, chain_of({"vulkan", "d3d12", "d3d11"}), "d3d11", probe);

    CHECK(launch.backend() == "vulkan");
    CHECK(asked == std::vector<std::string>{"vulkan"});
    CHECK(launch.choice().notes.empty());
}

TEST_CASE("The last backend of the chain is used without being asked about", "[graphics]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    std::vector<std::string> asked;
    const cube::app::graphics::Probe nothing_starts = [&](std::string_view backend) {
        asked.emplace_back(backend);
        return false;
    };

    const cube::app::graphics::Launch launch(store_in(directory), {}, chain_of({"vulkan", "d3d11"}),
                                             "d3d11", nothing_starts);

    CHECK(launch.backend() == "d3d11");
    CHECK(asked == std::vector<std::string>{"vulkan"});
}

TEST_CASE("A launch that never drew is a strike against its backend on the next", "[graphics]") {
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto yes = [](std::string_view) { return true; };

    // Two launches that set out with vulkan and never reached a frame ...
    for (int i = 0; i < 2; ++i) {
        const cube::app::graphics::Launch crashed(store_in(directory), {},
                                                  chain_of({"vulkan", "d3d11"}), "d3d11", yes);
        REQUIRE(crashed.backend() == "vulkan");
    }
    // ... and the third goes past it.
    const cube::app::graphics::Launch third(store_in(directory), {}, chain_of({"vulkan", "d3d11"}),
                                            "d3d11", yes);
    CHECK(third.backend() == "d3d11");
}
