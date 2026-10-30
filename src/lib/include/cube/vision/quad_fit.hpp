// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <opencv2/core.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace cube::vision {

// An N x N lattice placed through a face's own quadrilateral rather than grown
// from a sticker and its neighbours.
//
// The reader's square-on path lays an affine grid at the nearest-neighbour
// pitch. Two things follow from that and both cost sides that a person holding
// a cube would call readable. An affine grid is a parallel projection, so a
// side held close to a wide-angle camera has its far cells drift off their
// stickers as the pitch changes across the face; and the grid is only accepted
// when its two steps are near-equal and near-perpendicular, which is a test of
// how the side is held, not of whether it is a side. A 4x4 or a 5x5 held at a
// natural distance fails both.
//
// A face has four corners and they are the strongest edge evidence in the
// frame. This finds them (the extreme stickers along the two diagonals), maps
// the unit square onto them with a homography, and reads the N x N cells off
// that homography: the pose is solved rather than assumed, so a side seen at an
// angle is a side like any other, and a cell can be placed where no blob was
// found.
struct QuadLattice {
    // The face's corners, ordered top-left, top-right, bottom-right,
    // bottom-left as the camera sees them.
    std::array<cv::Point2f, 4> corners{};
    // The unit square onto those corners: (u, v) in [0, 1]^2 to working pixels,
    // u running rightwards and v downwards as the corners are ordered.
    cv::Matx33d homography;
    // N^2 cell centres in working pixels, row-major: cell (row, column) is at
    // row * N + column, and rows run with v.
    std::vector<cv::Point2f> centres;
    // Whether a sticker blob sat on each of those cells.
    std::vector<bool> found;
    // How many blobs sat on the lattice.
    std::size_t blobs = 0;
    // The matched blobs' distance from their cells, in cell pitches: how well
    // the homography explains them.
    float rms = 0.0F;
    // The mean distance between neighbouring cell centres, in pixels: what the
    // colour patch is scaled by.
    float pitch = 0.0F;
};

// The best N x N quadrilateral lattice through `blobs`, or nothing.
//
// `least_matched` is how many cells must hold a blob: a homography places the
// rest, so a side can be read with fewer stickers present than the square grid
// needs to be found at all. At least four are needed to place anything, and the
// lattice is refused if the blobs do not lie on it within kMaxQuadRms.
//
// The face's four CORNER stickers must be among the blobs, because the corners
// are what the quad is found from: the extreme blobs along the two diagonals
// are the four corners of a face that has them, and of something turned 45
// degrees - the ring of cells inside them - when it does not. A side missing a
// corner sticker is therefore not read here, and is left to the square grid and
// the other looks; that is the trade for a look that does not care how the side
// is held.
[[nodiscard]] std::optional<QuadLattice> fit_quad_lattice(const std::vector<cv::Point2f>& blobs,
                                                          int size, std::size_t least_matched);

// How far a blob may be from a cell and still be that cell's sticker, as a
// share of a cell pitch.
inline constexpr float kQuadMatchFraction = 0.30F;
// How far a matched blob may sit from its cell, in cell pitches, for the
// homography to be believed. Looser than the square grid's 0.12 because the
// cells are placed by the quad's edges rather than fitted through the blobs
// themselves.
inline constexpr float kMaxQuadRms = 0.22F;

}  // namespace cube::vision
