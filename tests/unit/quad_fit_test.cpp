// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "must_have.hpp"

#include <cube/vision/quad_fit.hpp>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

using namespace cube::vision;

namespace {

// A face's N x N cell centres, in the perspective a camera gives a side that is
// not square to it: the unit square mapped onto a trapezium, then every cell
// mapped through the same homography.
struct DrawnFace {
    std::vector<cv::Point2f> centres;
    cv::Matx33d homography;
};

[[nodiscard]] DrawnFace drawn_face(int size, const std::vector<cv::Point2f>& quad) {
    const std::vector<cv::Point2f> unit{
        {0.0F, 0.0F},
        {1.0F, 0.0F},
        {1.0F, 1.0F},
        {0.0F, 1.0F},
    };
    const cv::Mat transform = cv::getPerspectiveTransform(unit, quad);
    DrawnFace face;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            face.homography(row, column) = transform.at<double>(row, column);
        }
    }
    const auto map = [&face](double u, double v) {
        const double w =
            (face.homography(2, 0) * u) + (face.homography(2, 1) * v) + face.homography(2, 2);
        return cv::Point2f(
            static_cast<float>(((face.homography(0, 0) * u) + (face.homography(0, 1) * v) +
                                face.homography(0, 2)) /
                               w),
            static_cast<float>(((face.homography(1, 0) * u) + (face.homography(1, 1) * v) +
                                face.homography(1, 2)) /
                               w));
    };
    const auto n = static_cast<std::size_t>(size);
    for (std::size_t row = 0; row < n; ++row) {
        for (std::size_t column = 0; column < n; ++column) {
            face.centres.push_back(map((static_cast<double>(column) + 0.5) / static_cast<double>(n),
                                       (static_cast<double>(row) + 0.5) / static_cast<double>(n)));
        }
    }
    return face;
}

// A 5x5 seen at an angle: the near edge 400 px across, the far edge 200, which
// is the pitch change across the face that an affine grid cannot carry.
[[nodiscard]] std::vector<cv::Point2f> trapezium() {
    return {
        {100.0F, 40.0F},
        {300.0F, 40.0F},
        {400.0F, 340.0F},
        {0.0F, 340.0F},
    };
}

}  // namespace

TEST_CASE("A face seen at an angle is read from its own four corners", "[vision][quad]") {
    const DrawnFace face = drawn_face(5, trapezium());
    REQUIRE(face.centres.size() == 25);

    const auto lattice_read = fit_quad_lattice(face.centres, 5, 12);
    const auto& lattice = cube::test::must_have(lattice_read);
    CHECK(lattice.centres.size() == 25);
    CHECK(lattice.blobs == 25);
    CHECK(lattice.rms < kMaxQuadRms);

    // Every cell the reader would sample is near the cell the face was drawn
    // with: the homography carries the perspective rather than fitting a
    // parallel projection through the middle of it.
    for (const cv::Point2f& drawn : face.centres) {
        double best = 1e9;
        for (const cv::Point2f& placed : lattice.centres) {
            best = std::min(best, cv::norm(drawn - placed));
        }
        CHECK(best < 2.0);
    }
}

TEST_CASE("A parallel projection of the same face is read too", "[vision][quad]") {
    // The square-on case, which the affine path already reads: the quad must
    // not be worse at it.
    const std::vector<cv::Point2f> square{
        {100.0F, 100.0F},
        {300.0F, 100.0F},
        {300.0F, 300.0F},
        {100.0F, 300.0F},
    };
    const DrawnFace face = drawn_face(4, square);
    const auto lattice_read = fit_quad_lattice(face.centres, 4, 8);
    const auto& lattice = cube::test::must_have(lattice_read);
    CHECK(lattice.centres.size() == 16);
    CHECK(lattice.rms < 0.05F);
}

TEST_CASE("A face with a sticker covered is still placed, and that cell is marked",
          "[vision][quad]") {
    // The centre of a 5x5 covered by a finger: 24 of 25 stickers left, and the
    // four corner stickers still there, which is what the quad's corner search
    // needs. The covered cell is placed by the homography and marked not found.
    const DrawnFace face = drawn_face(5, trapezium());
    std::vector<cv::Point2f> present;
    for (std::size_t i = 0; i < face.centres.size(); ++i) {
        if (i != 12) {
            present.push_back(face.centres.at(i));
        }
    }
    REQUIRE(present.size() == 24);

    const auto lattice_read = fit_quad_lattice(present, 5, 12);
    const auto& lattice = cube::test::must_have(lattice_read);
    CHECK(lattice.centres.size() == 25);
    CHECK(lattice.blobs == 24);
    CHECK(std::count(lattice.found.begin(), lattice.found.end(), true) == 24);
}

TEST_CASE("Too few stickers to place a face is not a face", "[vision][quad]") {
    const std::vector<cv::Point2f> only_a_few{
        {10.0F, 10.0F},
        {40.0F, 12.0F},
        {12.0F, 42.0F},
    };
    CHECK_FALSE(fit_quad_lattice(only_a_few, 3, 4).has_value());
}
