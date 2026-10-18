#include <cube/vision/quad_fit.hpp>

#include <cube/profiling.hpp>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>
namespace cube::vision {

namespace {

// The unit square, in the corner order QuadLattice documents.
[[nodiscard]] std::vector<cv::Point2f> unit_square() {
    return {
        {0.0F, 0.0F},
        {1.0F, 0.0F},
        {1.0F, 1.0F},
        {0.0F, 1.0F},
    };
}

// The unit co-ordinates of cell (row, column) of an N x N face.
[[nodiscard]] cv::Point2d cell_uv(int size, std::size_t index) {
    const auto n = static_cast<std::size_t>(size);
    const auto row = index / n;
    const auto column = index % n;
    return {
        (static_cast<double>(column) + 0.5) / static_cast<double>(n),
        (static_cast<double>(row) + 0.5) / static_cast<double>(n),
    };
}

[[nodiscard]] cv::Point2f map_through(const cv::Matx33d& homography, cv::Point2d uv) {
    const double w = (homography(2, 0) * uv.x) + (homography(2, 1) * uv.y) + homography(2, 2);
    const double divisor = std::abs(w) < 1e-12 ? 1e-12 : w;
    return {
        static_cast<float>(
            ((homography(0, 0) * uv.x) + (homography(0, 1) * uv.y) + homography(0, 2)) / divisor),
        static_cast<float>(
            ((homography(1, 0) * uv.x) + (homography(1, 1) * uv.y) + homography(1, 2)) / divisor),
    };
}

// Four points ordered top-left, top-right, bottom-right, bottom-left.
//
// The order matters: the homography is only orientation-preserving, and the
// cell order the reader promises ("row-major as the camera sees them") is this
// order. Sorting by angle about the centre gives the cycle, and starting from
// the corner with the smallest x + y makes that cycle start at the top-left.
[[nodiscard]] std::array<cv::Point2f, 4> order_corners(const std::vector<cv::Point2f>& four) {
    cv::Point2f centre(0.0F, 0.0F);
    for (const cv::Point2f& point : four) {
        centre += point;
    }
    centre *= 1.0F / static_cast<float>(four.size());

    std::vector<cv::Point2f> cycle = four;
    std::ranges::sort(cycle, [&centre](const cv::Point2f& l, const cv::Point2f& r) {
        return std::atan2(l.y - centre.y, l.x - centre.x) <
               std::atan2(r.y - centre.y, r.x - centre.x);
    });

    std::size_t start = 0;
    float least = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < cycle.size(); ++i) {
        const float sum = cycle.at(i).x + cycle.at(i).y;
        if (sum < least) {
            least = sum;
            start = i;
        }
    }
    std::array<cv::Point2f, 4> ordered{};
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        ordered.at(i) = cycle.at((start + i) % cycle.size());
    }
    return ordered;
}

// The four extreme points of a sticker cloud along the diagonals: the corners of
// the face, when the four corner stickers are there. Used only to start the fit;
// the homography is refined through the blobs afterwards.
[[nodiscard]] std::optional<std::vector<cv::Point2f>> extreme_corners(
    const std::vector<cv::Point2f>& points) {
    if (points.size() < 4) {
        return std::nullopt;
    }
    std::size_t min_sum = 0;
    std::size_t max_sum = 0;
    std::size_t min_diff = 0;
    std::size_t max_diff = 0;
    float least_sum = std::numeric_limits<float>::max();
    float most_sum = std::numeric_limits<float>::lowest();
    float least_diff = std::numeric_limits<float>::max();
    float most_diff = std::numeric_limits<float>::lowest();
    for (std::size_t i = 0; i < points.size(); ++i) {
        const cv::Point2f& point = points.at(i);
        const float sum = point.x + point.y;
        const float diff = point.x - point.y;
        if (sum < least_sum) {
            least_sum = sum;
            min_sum = i;
        }
        if (sum > most_sum) {
            most_sum = sum;
            max_sum = i;
        }
        if (diff < least_diff) {
            least_diff = diff;
            min_diff = i;
        }
        if (diff > most_diff) {
            most_diff = diff;
            max_diff = i;
        }
    }
    std::vector<cv::Point2f> corners{
        points.at(min_sum),
        points.at(max_diff),
        points.at(max_sum),
        points.at(min_diff),
    };
    for (std::size_t i = 0; i < corners.size(); ++i) {
        for (std::size_t j = i + 1; j < corners.size(); ++j) {
            if (cv::norm(corners.at(i) - corners.at(j)) < 2.0) {
                return std::nullopt;
            }
        }
    }
    return corners;
}

// The homography through correspondences, with h33 fixed at 1: the eight
// unknowns solved from the normal equations by Gaussian elimination with
// partial pivoting.
//
// This is the step that makes the quad worth having. Re-deriving the corners
// from the blobs that matched cannot recover a corner sticker that is missing -
// the matched set is itself inset - while a fit from cell to pixel extrapolates
// through the missing cells, because the cells are still there in the grid.
[[nodiscard]] std::optional<cv::Matx33d> homography_through(
    const std::vector<std::pair<cv::Point2d, cv::Point2f>>& pairs) {
    if (pairs.size() < 4) {
        return std::nullopt;
    }
    // Normal equations for A h = b in eight unknowns.
    std::array<std::array<double, 8>, 8> normal{};
    std::array<double, 8> rhs{};
    for (const auto& [uv, pixel] : pairs) {
        const auto pixel_x = static_cast<double>(pixel.x);
        const auto pixel_y = static_cast<double>(pixel.y);
        const std::array<double, 8> row_x{uv.x,           uv.y, 1.0, 0.0, 0.0, 0.0, -pixel_x * uv.x,
                                          -pixel_x * uv.y};
        const std::array<double, 8> row_y{
            0.0, 0.0, 0.0, uv.x, uv.y, 1.0, -pixel_y * uv.x, -pixel_y * uv.y};
        for (std::size_t i = 0; i < 8; ++i) {
            rhs.at(i) += (row_x.at(i) * pixel_x) + (row_y.at(i) * pixel_y);
            for (std::size_t j = 0; j < 8; ++j) {
                normal.at(i).at(j) += (row_x.at(i) * row_x.at(j)) + (row_y.at(i) * row_y.at(j));
            }
        }
    }
    for (std::size_t column = 0; column < 8; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < 8; ++row) {
            if (std::abs(normal.at(row).at(column)) > std::abs(normal.at(pivot).at(column))) {
                pivot = row;
            }
        }
        if (std::abs(normal.at(pivot).at(column)) < 1e-9) {
            return std::nullopt;
        }
        std::swap(normal.at(column), normal.at(pivot));
        std::swap(rhs.at(column), rhs.at(pivot));
        const double divisor = normal.at(column).at(column);
        for (std::size_t i = column; i < 8; ++i) {
            normal.at(column).at(i) /= divisor;
        }
        rhs.at(column) /= divisor;
        for (std::size_t row = 0; row < 8; ++row) {
            if (row == column) {
                continue;
            }
            const double factor = normal.at(row).at(column);
            if (factor == 0.0) {
                continue;
            }
            for (std::size_t i = column; i < 8; ++i) {
                normal.at(row).at(i) -= factor * normal.at(column).at(i);
            }
            rhs.at(row) -= factor * rhs.at(column);
        }
    }
    return cv::Matx33d(rhs.at(0), rhs.at(1), rhs.at(2), rhs.at(3), rhs.at(4), rhs.at(5), rhs.at(6),
                       rhs.at(7), 1.0);
}

// What one mapping of the unit square made of the blobs.
struct Placed {
    cv::Matx33d homography{};
    std::vector<cv::Point2f> centres;
    std::vector<bool> found;
    std::vector<std::size_t> matched_cells;
    std::vector<cv::Point2f> matched;
    float pitch = 0.0F;
    float rms = 0.0F;
};

// Map the N x N cells through `homography` and see which blobs sit on them.
[[nodiscard]] Placed place_cells(const cv::Matx33d& homography, int size,
                                 const std::vector<cv::Point2f>& blobs) {
    Placed placed;
    placed.homography = homography;
    const auto n = static_cast<std::size_t>(size);
    const auto cells = n * n;
    placed.centres.reserve(cells);
    placed.found.assign(cells, false);
    for (std::size_t index = 0; index < cells; ++index) {
        placed.centres.push_back(map_through(homography, cell_uv(size, index)));
    }

    // The pitch: the mean of the steps between neighbouring cells, along each
    // axis, which is the scale a colour patch is sized by.
    double step_sum = 0.0;
    std::size_t steps = 0;
    for (std::size_t row = 0; row < n; ++row) {
        for (std::size_t column = 0; column + 1 < n; ++column) {
            step_sum += cv::norm(placed.centres.at((row * n) + column + 1) -
                                 placed.centres.at((row * n) + column));
            ++steps;
        }
    }
    for (std::size_t row = 0; row + 1 < n; ++row) {
        for (std::size_t column = 0; column < n; ++column) {
            step_sum += cv::norm(placed.centres.at(((row + 1) * n) + column) -
                                 placed.centres.at((row * n) + column));
            ++steps;
        }
    }
    placed.pitch = steps == 0 ? 0.0F : static_cast<float>(step_sum / static_cast<double>(steps));
    if (!(placed.pitch > 0.0F)) {
        return placed;
    }

    const auto reach = static_cast<double>(kQuadMatchFraction) * static_cast<double>(placed.pitch);
    double squared = 0.0;
    for (const cv::Point2f& blob : blobs) {
        std::size_t nearest = 0;
        double best = std::numeric_limits<double>::max();
        for (std::size_t i = 0; i < placed.centres.size(); ++i) {
            const double distance = cv::norm(blob - placed.centres.at(i));
            if (distance < best) {
                best = distance;
                nearest = i;
            }
        }
        if (best > reach || placed.found.at(nearest)) {
            continue;
        }
        placed.found.at(nearest) = true;
        placed.matched.push_back(blob);
        placed.matched_cells.push_back(nearest);
        squared += best * best;
    }
    placed.rms =
        placed.matched.empty()
            ? std::numeric_limits<float>::max()
            : static_cast<float>(std::sqrt(squared / static_cast<double>(placed.matched.size())) /
                                 static_cast<double>(placed.pitch));
    return placed;
}

}  // namespace

std::optional<QuadLattice> fit_quad_lattice(const std::vector<cv::Point2f>& blobs, int size,
                                            std::size_t least_matched) {
    CUBE_ZONE_SCOPED_N("vision.fit_quad_lattice");
    if (size < 2 || size > 9 || blobs.size() < 4) {
        return std::nullopt;
    }
    const std::size_t cells = static_cast<std::size_t>(size) * static_cast<std::size_t>(size);
    const std::size_t wanted = std::min(cells, std::max<std::size_t>(4, least_matched));

    const std::optional<std::vector<cv::Point2f>> corners = extreme_corners(blobs);
    if (!corners.has_value()) {
        return std::nullopt;
    }

    // Start from the quad the extreme stickers make, then refine through the
    // blobs that sat on the grid, which is what recovers a face whose corner
    // stickers are missing or bridged.
    //
    // Two starts are weighed: the extremes as they are, and the same quad grown
    // by half a cell all round. The second is what a face with its four corner
    // stickers missing needs - its extremes are then the ring of cells inside
    // them, half a pitch in - and the first is what a face with all of them
    // needs. The better of the two is refined.
    const std::array<cv::Point2f, 4> ordered = order_corners(*corners);
    cv::Point2f centroid(0.0F, 0.0F);
    for (const cv::Point2f& corner : ordered) {
        centroid += corner;
    }
    centroid *= 0.25F;
    const auto grow = static_cast<float>(size) / static_cast<float>(size - 1);
    std::array<cv::Point2f, 4> grown{};
    for (std::size_t i = 0; i < grown.size(); ++i) {
        grown.at(i) = centroid + ((ordered.at(i) - centroid) * grow);
    }

    const std::vector<cv::Point2f> unit = unit_square();
    std::optional<Placed> best;
    std::optional<cv::Matx33d> start;
    for (const std::array<cv::Point2f, 4>& candidate : {ordered, grown}) {
        const std::vector<cv::Point2f> quad(candidate.begin(), candidate.end());
        const cv::Mat transform = cv::getPerspectiveTransform(unit, quad);
        if (transform.empty()) {
            continue;
        }
        cv::Matx33d homography;
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                homography(row, column) = transform.at<double>(row, column);
            }
        }
        Placed placed = place_cells(homography, size, blobs);
        const bool better =
            !best.has_value() || placed.matched.size() > best->matched.size() ||
            (placed.matched.size() == best->matched.size() && placed.rms < best->rms);
        if (better) {
            best = std::move(placed);
            start = homography;
        }
    }
    if (!best.has_value() || !start.has_value()) {
        return std::nullopt;
    }

    cv::Matx33d homography = *start;
    for (int attempt = 0; attempt < 6; ++attempt) {
        Placed placed = place_cells(homography, size, blobs);
        const bool better =
            !best.has_value() || placed.matched.size() > best->matched.size() ||
            (placed.matched.size() == best->matched.size() && placed.rms < best->rms);
        if (better) {
            best = placed;
        }
        if (placed.matched.size() < 4) {
            break;
        }
        // Refit from the cells the blobs claimed, then look again.
        std::vector<std::pair<cv::Point2d, cv::Point2f>> pairs;
        pairs.reserve(placed.matched.size());
        for (std::size_t i = 0; i < placed.matched.size(); ++i) {
            pairs.emplace_back(cell_uv(size, placed.matched_cells.at(i)), placed.matched.at(i));
        }
        const std::optional<cv::Matx33d> refined = homography_through(pairs);
        if (!refined.has_value()) {
            break;
        }
        const double shift = std::abs(refined->operator()(0, 2) - homography(0, 2)) +
                             std::abs(refined->operator()(1, 2) - homography(1, 2));
        homography = *refined;
        if (shift < 0.01 && placed.rms <= kMaxQuadRms) {
            break;
        }
    }
    if (!best.has_value() || best->matched.size() < wanted || best->rms > kMaxQuadRms) {
        return std::nullopt;
    }

    // The fitted grid's own scale has to be a sticker's, or the quad is not a
    // face: four blobs in a line, or two that a stray has pulled together, give
    // a homography that maps the cells millions of pixels apart. That is not
    // only wrong, it is expensive - the colour patch is sized from the pitch,
    // and a pitch of a million asks the sampler for a patch the size of the
    // frame. The quad's own diagonal is the bound: no cell can be further from
    // its neighbour than half the face is across.
    const double diagonal = cv::norm(map_through(best->homography, {0.0, 0.0}) -
                                     map_through(best->homography, {1.0, 1.0}));
    const double largest_pitch = (2.0 * diagonal) / static_cast<double>(size);
    if (!std::isfinite(best->pitch) || best->pitch < 2.0F ||
        static_cast<double>(best->pitch) > largest_pitch) {
        return std::nullopt;
    }

    QuadLattice lattice;
    // The corners the reader reports are the grid's own four, mapped back from
    // (0, 0), (1, 0), (1, 1) and (0, 1): the quad the cells lie in, not the
    // blurred extreme blobs it started from.
    lattice.corners = {
        map_through(best->homography, {0.0, 0.0}),
        map_through(best->homography, {1.0, 0.0}),
        map_through(best->homography, {1.0, 1.0}),
        map_through(best->homography, {0.0, 1.0}),
    };
    lattice.homography = best->homography;
    lattice.centres = std::move(best->centres);
    lattice.found = std::move(best->found);
    lattice.blobs = best->matched.size();
    lattice.rms = best->rms;
    lattice.pitch = best->pitch;
    return lattice;
}

}  // namespace cube::vision
