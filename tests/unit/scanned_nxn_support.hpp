#pragma once

// What the tests that show a cube bigger than a 3x3 to the camera have in common: a cube
// whose six flat sides make that cube and no other, and the six drawn sides handed to the
// scan.

#include "controllers/direct_scan.hpp"
#include "drawn_cube.hpp"

#include <cube/nxn/cube.hpp>
#include <cube/nxn/outside_view.hpp>
#include <cube/nxn/scan_assembly.hpp>
#include <cube/solver/cube.hpp>
#include <cube/solver/hold.hpp>
#include <cube/vision/direct_scan.hpp>

#include <cstddef>
#include <optional>

namespace cube::test {

// A scrambled cube of `size` whose six flat sides make one cube and no other, so that a scan
// of it completes without corner views; the first seed from `seed` that does.
[[nodiscard]] inline nxn::NCube unambiguous_cube(int size, unsigned seed) {
    for (;; ++seed) {
        const nxn::NCube tried =
            nxn::random_scramble({.size = size, .seed = seed, .moves = 25 * size});
        nxn::ScannedSides sides;
        for (std::size_t i = 0; i < 6; ++i) {
            sides.at(i) = nxn::outside_view(tried, solver::kAllFaces.at(i));
        }
        if (nxn::assemble_all_scanned_sides(size, sides, 8).size() == 1) {
            return tried;
        }
    }
}

// The pose a side of `size` is drawn in so that the scan reads it.
[[nodiscard]] inline Pose pose_for(int size) {
    Pose pose;
    pose.pitch = 180.0F / static_cast<float>(size);
    return pose;
}

// Shows the scan the six sides of `cube`, one after the other with an empty room between.
// The scan must be at the cube's size and not watching.
inline void show_all_sides(app::DirectScan& scan, const nxn::NCube& cube) {
    for (const solver::Face face : solver::kAllFaces) {
        for (int i = 0; i < static_cast<int>(vision::SideScan::kMinReads) + 4; ++i) {
            scan.processFrame(draw_side(nxn::outside_view(cube, face), pose_for(cube.size())));
        }
        scan.processFrame(room({640, 360}));
    }
}

// How many cells of the side a camera sees change when `before` becomes `after`, held as `hold`.
[[nodiscard]] inline std::size_t cells_changed(const nxn::NCube& before, const nxn::NCube& after,
                                               const solver::Hold& hold) {
    const auto a = nxn::camera_view(before, hold);
    const auto b = nxn::camera_view(after, hold);
    std::size_t cells = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        cells += a.at(i) != b.at(i) ? 1U : 0U;
    }
    return cells;
}

// A hold, other than the stated one, from which the camera sees at least `least` cells of the
// side change when `before` becomes `after`, and from which the turned side is neither the one
// facing the camera (which looks like a rolled cube) nor the one behind it.
[[nodiscard]] inline std::optional<solver::Hold> hold_that_sees_n(const nxn::NCube& before,
                                                                  const nxn::NCube& after,
                                                                  std::size_t least,
                                                                  solver::Face turned) {
    for (const solver::Hold& hold : solver::all_holds()) {
        if (hold != solver::kStatedHold && solver::side_at(hold, solver::Place::Back) != turned &&
            solver::side_at(hold, solver::Place::Back) != solver::opposite(turned) &&
            cells_changed(before, after, hold) >= least) {
            return hold;
        }
    }
    return std::nullopt;
}

}  // namespace cube::test
