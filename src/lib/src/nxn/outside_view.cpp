#include <cube/nxn/outside_view.hpp>

#include <cube/solver/outside_view.hpp>

#include <array>
#include <cstddef>

namespace cube::nxn {

std::vector<Color> outside_view(const NCube& cube, Face face) {
    const int n = cube.size();
    std::vector<Color> stickers;
    stickers.reserve(static_cast<std::size_t>(n) * static_cast<std::size_t>(n));
    for (int row = 0; row < n; ++row) {
        for (int col = 0; col < n; ++col) {
            switch (face) {
                case Face::U:
                case Face::F:
                case Face::L:
                    stickers.push_back(cube.at(face, row, col));
                    break;
                case Face::R:
                case Face::B:
                    stickers.push_back(cube.at(face, row, n - 1 - col));
                    break;
                case Face::D:
                    stickers.push_back(cube.at(face, n - 1 - row, col));
                    break;
            }
        }
    }
    return stickers;
}

namespace {

// A side's stickers turned a quarter turn anticlockwise: its right-hand column
// becomes its top row.
[[nodiscard]] std::vector<Color> turned_anticlockwise(const std::vector<Color>& side, int n) {
    std::vector<Color> turned(side.size());
    for (int row = 0; row < n; ++row) {
        for (int column = 0; column < n; ++column) {
            turned.at((static_cast<std::size_t>(row) * static_cast<std::size_t>(n)) +
                      static_cast<std::size_t>(column)) =
                side.at((static_cast<std::size_t>(column) * static_cast<std::size_t>(n)) +
                        static_cast<std::size_t>(n - 1 - row));
        }
    }
    return turned;
}

}  // namespace

std::vector<Color> camera_view(const NCube& cube, const solver::Hold& hold) {
    const Face side = solver::side_at(hold, solver::Place::Back);
    std::vector<Color> view = outside_view(cube, side);
    // As solver::camera_view: the neighbour that is the holder's top is turned up to
    // the top, one anticlockwise quarter turn for each place round it is.
    const std::array<Face, 4> clockwise = {
        solver::top_of(side),
        solver::right_of(side),
        solver::opposite(solver::top_of(side)),
        solver::opposite(solver::right_of(side)),
    };
    for (const Face neighbour : clockwise) {
        if (neighbour == hold.up) {
            break;
        }
        view = turned_anticlockwise(view, cube.size());
    }
    return view;
}

}  // namespace cube::nxn
