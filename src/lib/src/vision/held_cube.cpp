// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <cube/vision/held_cube.hpp>

#include <cube/nxn/outside_view.hpp>
#include <cube/profiling.hpp>

#include <cstddef>
#include <iterator>
#include <limits>

namespace cube::vision {

namespace {

constexpr std::array<bool, 9> kAllNine = {true, true, true, true, true, true, true, true, true};

// The holds are of a 3x3: a read of any other size is not one of them.
[[nodiscard]] std::array<cv::Vec3f, 9> nine_of(std::span<const cv::Vec3f> lab) {
    std::array<cv::Vec3f, 9> out{};
    std::ranges::copy(lab.first(std::min(lab.size(), out.size())), out.begin());
    return out;
}

[[nodiscard]] std::array<bool, 9> nine_found(const SideRead& read) {
    std::array<bool, 9> out{};
    for (std::size_t i = 0; i < out.size() && i < read.found.size(); ++i) {
        out.at(i) = read.found.at(i);
    }
    return out;
}

[[nodiscard]] std::array<HoldFit, 24> fit(const std::array<cv::Vec3f, 9>& lab,
                                          const std::array<bool, 9>& counted,
                                          const solver::Cube& cube, const SessionColours& colours) {
    CUBE_ZONE_SCOPED_N("vision.held_cube.fit");
    std::array<HoldFit, 24> fits{};
    const std::array<solver::Hold, 24> holds = solver::all_holds();
    for (std::size_t i = 0; i < holds.size(); ++i) {
        const std::array<solver::Color, 9> shown = solver::camera_view(cube, holds.at(i));
        float total = 0.0F;
        int cells = 0;
        for (std::size_t cell = 0; cell < 9; ++cell) {
            if (!counted.at(cell)) {
                continue;
            }
            total +=
                colour_distance(lab.at(cell), colours.at(static_cast<std::size_t>(shown.at(cell))));
            ++cells;
        }
        fits.at(i) = HoldFit{
            .hold = holds.at(i),
            .distance =
                cells > 0 ? total / static_cast<float>(cells) : std::numeric_limits<float>::max(),
        };
    }
    // Stable, so holds that fit equally keep the order all_holds() gives
    // them and the answer does not depend on the sort.
    std::ranges::stable_sort(
        fits, [](const HoldFit& a, const HoldFit& b) { return a.distance < b.distance; });
    return fits;
}

// What the fits say: the best, and every hold that would look the same to
// the camera in the cells that were counted - provided nothing that would
// look different there comes close.
[[nodiscard]] HoldReading reading_of(const std::array<HoldFit, 24>& fits,
                                     const std::array<bool, 9>& counted, const solver::Cube& cube) {
    HoldReading reading;
    reading.distance = fits.front().distance;
    if (reading.distance > kHoldFitDistance) {
        return reading;
    }
    const auto same_where_counted = [&counted](const std::array<solver::Color, 9>& a,
                                               const std::array<solver::Color, 9>& b) {
        for (std::size_t cell = 0; cell < 9; ++cell) {
            if (counted.at(cell) && a.at(cell) != b.at(cell)) {
                return false;
            }
        }
        return true;
    };
    const std::array<solver::Color, 9> shown = solver::camera_view(cube, fits.front().hold);
    for (const HoldFit& candidate : fits) {
        if (same_where_counted(solver::camera_view(cube, candidate.hold), shown)) {
            reading.holds.push_back(candidate.hold);
        } else if (candidate.distance < reading.distance + kHoldClearDistance) {
            reading.holds.clear();
            return reading;
        }
    }
    return reading;
}

// The same for a cube of any size. The 3x3's above is left as it was; this one takes the
// cells to count as a list, and what each hold shows as the N x N stickers of
// nxn::camera_view. A read of another size has no cell that corresponds to the cube's.
using Views = std::array<std::vector<solver::Color>, 24>;

[[nodiscard]] Views views_of(const nxn::NCube& cube) {
    Views views;
    const std::array<solver::Hold, 24> holds = solver::all_holds();
    for (std::size_t i = 0; i < holds.size(); ++i) {
        views.at(i) = nxn::camera_view(cube, holds.at(i));
    }
    return views;
}

[[nodiscard]] bool same_size(const SideRead& read, const nxn::NCube& cube) {
    const auto cells =
        static_cast<std::size_t>(cube.size()) * static_cast<std::size_t>(cube.size());
    return read.size == cube.size() && read.lab.size() == cells && read.found.size() == cells;
}

[[nodiscard]] std::array<HoldFit, 24> fit_n(const SideRead& read, const Views& views,
                                            const SessionColours& colours) {
    CUBE_ZONE_SCOPED_N("vision.held_cube.fit_n");
    std::array<HoldFit, 24> fits{};
    const std::array<solver::Hold, 24> holds = solver::all_holds();
    for (std::size_t i = 0; i < holds.size(); ++i) {
        float total = 0.0F;
        int cells = 0;
        for (std::size_t cell = 0; cell < views.at(i).size(); ++cell) {
            if (!read.found.at(cell)) {
                continue;
            }
            total += colour_distance(read.lab.at(cell),
                                     colours.at(static_cast<std::size_t>(views.at(i).at(cell))));
            ++cells;
        }
        fits.at(i) = HoldFit{
            .hold = holds.at(i),
            .distance =
                cells > 0 ? total / static_cast<float>(cells) : std::numeric_limits<float>::max(),
        };
    }
    std::ranges::stable_sort(
        fits, [](const HoldFit& a, const HoldFit& b) { return a.distance < b.distance; });
    return fits;
}

[[nodiscard]] std::size_t index_of(const solver::Hold& hold) {
    const std::array<solver::Hold, 24> holds = solver::all_holds();
    return static_cast<std::size_t>(std::ranges::find(holds, hold) - holds.begin());
}

}  // namespace

std::array<HoldFit, 24> fit_holds(std::span<const cv::Vec3f> lab, const solver::Cube& cube,
                                  const SessionColours& colours) {
    return fit(nine_of(lab), kAllNine, cube, colours);
}

std::array<HoldFit, 24> fit_holds(const SideRead& read, const solver::Cube& cube,
                                  const SessionColours& colours) {
    return fit(nine_of(read.lab), nine_found(read), cube, colours);
}

bool HoldReading::shows(const solver::Hold& hold) const noexcept {
    return std::ranges::find(holds, hold) != holds.end();
}

HoldReading read_hold(std::span<const cv::Vec3f> lab, const solver::Cube& cube,
                      const SessionColours& colours) {
    return reading_of(fit(nine_of(lab), kAllNine, cube, colours), kAllNine, cube);
}

HoldReading read_hold(const SideRead& read, const solver::Cube& cube,
                      const SessionColours& colours) {
    const std::array<bool, 9> found = nine_found(read);
    return reading_of(fit(nine_of(read.lab), found, cube, colours), found, cube);
}

int stickers_off(const SideRead& read, const std::array<solver::Color, 9>& expected,
                 const SessionColours& colours) {
    int off = 0;
    for (std::size_t cell = 0; cell < 9; ++cell) {
        if (!read.found.at(cell)) {
            continue;
        }
        const auto own = static_cast<std::size_t>(expected.at(cell));
        const float to_own = colour_distance(read.lab.at(cell), colours.at(own));
        for (std::size_t other = 0; other < colours.size(); ++other) {
            if (other != own && colour_distance(read.lab.at(cell), colours.at(other)) < to_own) {
                ++off;
                break;
            }
        }
    }
    return off;
}

std::array<HoldFit, 24> fit_holds(const SideRead& read, const nxn::NCube& cube,
                                  const SessionColours& colours) {
    if (!same_size(read, cube)) {
        return std::array<HoldFit, 24>{};
    }
    return fit_n(read, views_of(cube), colours);
}

float hold_clear_distance(int size) noexcept {
    const auto cells = static_cast<float>(size * size);
    return kHoldClearDistance * std::min(1.0F, 9.0F / cells);
}

HoldReading read_hold(const SideRead& read, const nxn::NCube& cube, const SessionColours& colours) {
    HoldReading reading;
    if (!same_size(read, cube)) {
        return reading;
    }
    const Views views = views_of(cube);
    const std::array<HoldFit, 24> fits = fit_n(read, views, colours);
    reading.distance = fits.front().distance;
    if (reading.distance > kHoldFitDistance) {
        return reading;
    }
    const auto same_where_counted = [&read](const std::vector<solver::Color>& a,
                                            const std::vector<solver::Color>& b) {
        for (std::size_t cell = 0; cell < a.size(); ++cell) {
            if (read.found.at(cell) && a.at(cell) != b.at(cell)) {
                return false;
            }
        }
        return true;
    };
    const float clear = hold_clear_distance(cube.size());
    const std::vector<solver::Color>& shown = views.at(index_of(fits.front().hold));
    for (const HoldFit& candidate : fits) {
        if (same_where_counted(views.at(index_of(candidate.hold)), shown)) {
            reading.holds.push_back(candidate.hold);
        } else if (candidate.distance < reading.distance + clear) {
            reading.holds.clear();
            return reading;
        }
    }
    return reading;
}

int stickers_off(const SideRead& read, std::span<const solver::Color> expected,
                 const SessionColours& colours) {
    int off = 0;
    for (std::size_t cell = 0; cell < expected.size() && cell < read.found.size(); ++cell) {
        if (!read.found.at(cell)) {
            continue;
        }
        const auto own = static_cast<std::size_t>(
            *std::next(expected.begin(), static_cast<std::ptrdiff_t>(cell)));
        const float to_own = colour_distance(read.lab.at(cell), colours.at(own));
        for (std::size_t other = 0; other < colours.size(); ++other) {
            if (other != own && colour_distance(read.lab.at(cell), colours.at(other)) < to_own) {
                ++off;
                break;
            }
        }
    }
    return off;
}

}  // namespace cube::vision
