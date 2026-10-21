// Working out how a cube of any size is held, from a side of it square to the camera:
// cube::nxn::camera_view and the N x N overloads of cube::vision::read_hold, on drawn
// colours (the camera's measurements stood in for by the session's own colours, pushed about
// by a few Lab units). docs/nxn-camera-follow.md says what these numbers are for.
#include <catch2/catch_test_macros.hpp>

#include <cube/nxn/cube.hpp>
#include <cube/nxn/outside_view.hpp>
#include <cube/solver/cube.hpp>
#include <cube/solver/hold.hpp>
#include <cube/solver/solver.hpp>
#include <cube/vision/direct_scan.hpp>
#include <cube/vision/held_cube.hpp>

#include <opencv2/core.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <vector>

using cube::nxn::LayerMove;
using cube::nxn::NCube;
using cube::solver::all_holds;
using cube::solver::Color;
using cube::solver::Face;
using cube::solver::Hold;
using cube::vision::SessionColours;
using cube::vision::SideRead;

namespace {

[[nodiscard]] SessionColours session() {
    SessionColours colours{};
    colours.at(static_cast<std::size_t>(Color::White)) = {92.0F, 0.0F, 3.0F};
    colours.at(static_cast<std::size_t>(Color::Yellow)) = {86.0F, -6.0F, 72.0F};
    colours.at(static_cast<std::size_t>(Color::Green)) = {56.0F, -52.0F, 34.0F};
    colours.at(static_cast<std::size_t>(Color::Blue)) = {36.0F, 8.0F, -52.0F};
    colours.at(static_cast<std::size_t>(Color::Orange)) = {62.0F, 40.0F, 50.0F};
    colours.at(static_cast<std::size_t>(Color::Red)) = {50.0F, 46.0F, 44.0F};
    return colours;
}

// What the reader would hand over for `cube` held as `hold`: every sticker found, each its
// colour as the session measured it and pushed about by `wobble`.
[[nodiscard]] SideRead seen(const NCube& cube, const Hold& hold, float wobble = 0.0F) {
    const SessionColours colours = session();
    const std::vector<Color> shown = cube::nxn::camera_view(cube, hold);
    SideRead read;
    read.size = cube.size();
    read.centres.assign(shown.size(), cv::Point2f{});
    read.lab.assign(shown.size(), cv::Vec3f{});
    read.found.assign(shown.size(), true);
    read.stickers_found = static_cast<int>(shown.size());
    for (std::size_t cell = 0; cell < shown.size(); ++cell) {
        const auto sign = [cell](std::size_t shift) {
            return ((cell >> shift) & 1U) == 0U ? 1.0F : -1.0F;
        };
        read.lab.at(cell) = colours.at(static_cast<std::size_t>(shown.at(cell))) +
                            cv::Vec3f(sign(0) * wobble, sign(1) * wobble, sign(2) * wobble);
    }
    return read;
}

[[nodiscard]] NCube scrambled(int size, unsigned seed) {
    return cube::nxn::random_scramble({.size = size, .seed = seed, .moves = 25 * size});
}

}  // namespace

TEST_CASE("a cube of three is seen by the camera as the 3x3 library sees it",
          "[vision][held-cube][nxn]") {
    for (const unsigned seed : {3U, 4U, 5U}) {
        const cube::solver::Cube cube = cube::solver::random_scramble(seed);
        const NCube wide = NCube::from_cube3(cube);
        for (const Hold& hold : all_holds()) {
            const std::array<Color, 9> small = cube::solver::camera_view(cube, hold);
            const std::vector<Color> big = cube::nxn::camera_view(wide, hold);
            REQUIRE(big.size() == 9);
            CHECK(std::equal(big.begin(), big.end(), small.begin()));
        }
    }
}

TEST_CASE("the camera sees a cube of any size from each of its 24 holds differently",
          "[vision][held-cube][nxn]") {
    for (const int size : {2, 4, 5, 6}) {
        INFO("size " << size);
        const NCube cube = scrambled(size, 5);
        std::vector<std::vector<Color>> views;
        for (const Hold& hold : all_holds()) {
            views.push_back(cube::nxn::camera_view(cube, hold));
            REQUIRE(views.back().size() == static_cast<std::size_t>(size * size));
        }
        std::ranges::sort(views);
        // A scrambled cube has no turn symmetry to speak of.
        CHECK(std::ranges::adjacent_find(views) == views.end());
    }
}

TEST_CASE("a side square to the camera says how a cube of any size is held",
          "[vision][held-cube][nxn]") {
    for (const int size : {2, 4, 5, 6}) {
        for (const unsigned seed : {1U, 2U, 3U}) {
            const NCube cube = scrambled(size, seed);
            for (const Hold& hold : all_holds()) {
                INFO("size " << size << " seed " << seed);
                const auto reading =
                    cube::vision::read_hold(seen(cube, hold, 1.5F), cube, session());
                REQUIRE(reading.shows(hold));
                CHECK(reading.distance < cube::vision::kHoldFitDistance);
            }
        }
    }
}

TEST_CASE("a read of another size fits no hold", "[vision][held-cube][nxn]") {
    const NCube four = scrambled(4, 1);
    const NCube five = scrambled(5, 1);
    const auto reading = cube::vision::read_hold(seen(five, Hold{}), four, session());
    CHECK(reading.holds.empty());
}

TEST_CASE("the margin between two holds is no more than a 3x3 has", "[vision][held-cube][nxn]") {
    CHECK(cube::vision::hold_clear_distance(3) == cube::vision::kHoldClearDistance);
    CHECK(cube::vision::hold_clear_distance(2) == cube::vision::kHoldClearDistance);
    CHECK(cube::vision::hold_clear_distance(6) < cube::vision::hold_clear_distance(4));
    CHECK(cube::vision::hold_clear_distance(4) < cube::vision::kHoldClearDistance);
}

// What a camera can see of a turn, for docs/nxn-camera-follow.md: for every layer turn of a
// cube and every way of holding it, how many of the side's N^2 cells it changes. Hidden;
// prints a table.
// NOLINTNEXTLINE(readability-function-cognitive-complexity): a printed table, not logic
TEST_CASE("what the camera can see of a turn", "[.observability]") {
    std::cout << "size  layer-turns-x-holds  seen  unseen  fewest-cells  median-cells  "
                 "same-as-rolling-the-cube\n";
    for (const int size : {2, 3, 4, 5, 6}) {
        std::size_t seen_count = 0;
        std::size_t unseen_count = 0;
        std::size_t rolled = 0;
        std::vector<std::size_t> changed;
        for (unsigned seed = 1; seed <= 3; ++seed) {
            const NCube cube = scrambled(size, seed);
            for (const Face face : cube::solver::kAllFaces) {
                for (int depth = 0; depth < size; ++depth) {
                    for (const int turns : {1, 2, 3}) {
                        NCube moved = cube;
                        moved.apply(LayerMove{.face = face, .depth = depth, .turns = turns});
                        for (const Hold& hold : all_holds()) {
                            const auto before = cube::nxn::camera_view(cube, hold);
                            const auto after = cube::nxn::camera_view(moved, hold);
                            std::size_t cells = 0;
                            for (std::size_t i = 0; i < before.size(); ++i) {
                                cells += before.at(i) != after.at(i) ? 1U : 0U;
                            }
                            if (cells == 0) {
                                ++unseen_count;
                                continue;
                            }
                            ++seen_count;
                            changed.push_back(cells);
                            // The turned side facing the camera shows what the whole cube,
                            // rolled, would.
                            for (const Hold& other : all_holds()) {
                                if (other != hold && cube::nxn::camera_view(cube, other) == after) {
                                    ++rolled;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        std::ranges::sort(changed);
        std::cout << size << "  " << (seen_count + unseen_count) << "  " << seen_count << "  "
                  << unseen_count << "  " << changed.front() << "  "
                  << changed.at(changed.size() / 2) << "  " << rolled << "\n";
    }
}
