// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include "controllers/held_cube.hpp"

#include "controllers/diagnostics.hpp"
#include "controllers/direct_scan.hpp"
#include "solve_controller.hpp"

#include <cube/diagnostics/describe.hpp>
#include <cube/nxn/notation.hpp>
#include <cube/nxn/outside_view.hpp>
#include <cube/nxn/stepped_plan.hpp>
#include <cube/profiling.hpp>

#include <QMatrix3x3>

#include <algorithm>
#include <array>
#include <cstddef>

namespace cube::app {

namespace {

using cube::solver::Cube;
using cube::solver::Face;
using cube::solver::Hold;
using cube::solver::Place;
using cube::vision::HoldReading;

[[nodiscard]] QString side_letter(Face face) {
    switch (face) {
        case Face::U:
            return QStringLiteral("U");
        case Face::D:
            return QStringLiteral("D");
        case Face::L:
            return QStringLiteral("L");
        case Face::R:
            return QStringLiteral("R");
        case Face::F:
            return QStringLiteral("F");
        case Face::B:
            return QStringLiteral("B");
    }
    return QStringLiteral("U");
}

[[nodiscard]] QString place_name(Place place) {
    switch (place) {
        case Place::Top:
            return QStringLiteral("top");
        case Place::Bottom:
            return QStringLiteral("bottom");
        case Place::Left:
            return QStringLiteral("left");
        case Place::Right:
            return QStringLiteral("right");
        case Place::Front:
            return QStringLiteral("front");
        case Place::Back:
            return QStringLiteral("back");
    }
    return QStringLiteral("top");
}

[[nodiscard]] cube::diagnostics::Json hold_json(const Hold& hold) {
    cube::diagnostics::Json out = cube::diagnostics::Json::object();
    out.set("up", cube::diagnostics::letter_of(hold.up));
    out.set("front", cube::diagnostics::letter_of(hold.front));
    return out;
}

}  // namespace

HeldCube::HeldCube(DirectScan& scan, SolveController& solve, QObject* parent)
    : QObject(parent), scan_(scan), solve_(solve) {
    connect(&scan_, &DirectScan::frameWatched, this, &HeldCube::onFrameWatched);
    connect(&scan_, &DirectScan::runningChanged, this, [this]() {
        if (!scan_.watching()) {
            // Nobody is looking any more. The hold stands; what the camera
            // made of the last frame does not.
            forget();
            matched_this_move_ = false;
            setSight(Sight::None);
        }
    });
    connect(&solve_, &SolveController::stateChanged, this, &HeldCube::onSolveChanged);
    connect(&solve_, &SolveController::stateChanged, this, &HeldCube::updateMoveHidden);
    connect(&solve_, &SolveController::solutionChanged, this, [this]() {
        forget();
        matched_this_move_ = false;
        judged_index_ = -1;
        updateMoveHidden();
        if (solve_.solutionContinuesCube()) {
            // The same cube, still in the same hands: the method changed and
            // the way to solve it with it. How it is held has not, so the
            // hold stands. What was counted towards was about the move the
            // old solution expected, which is not this one.
            move_announced_ = false;
            setSight(Sight::None);
            return;
        }
        // A new cube to solve starts from the hold the screen asks for.
        seen_ = false;
        hold_ = cube::solver::kStatedHold;
        emit holdChanged();
        setSight(Sight::None);
    });
}

void HeldCube::setHold(const Hold& hold) {
    if (!cube::solver::is_valid(hold) || hold == hold_) {
        return;
    }
    hold_ = hold;
    emit holdChanged();
    updateMoveHidden();
}

QString HeldCube::up() const {
    return side_letter(hold_.up);
}
QString HeldCube::front() const {
    return side_letter(hold_.front);
}

QVariantMap HeldCube::places() const {
    QVariantMap places;
    for (const Face side : cube::solver::kAllFaces) {
        places.insert(side_letter(side), place_name(cube::solver::place_of(hold_, side)));
    }
    return places;
}

QQuaternion HeldCube::rotation() const {
    const std::array<int, 9> turn = cube::solver::rotation(hold_);
    std::array<float, 9> values{};
    for (std::size_t i = 0; i < values.size(); ++i) {
        values[i] = static_cast<float>(turn[i]);
    }
    return QQuaternion::fromRotationMatrix(QMatrix3x3(values.data()));
}

QString HeldCube::sight() const {
    switch (sight_) {
        case Sight::None:
            return QStringLiteral("none");
        case Sight::Matched:
            return QStringLiteral("matched");
        case Sight::Unsure:
            return QStringLiteral("unsure");
        case Sight::Wrong:
            return QStringLiteral("wrong");
        case Sight::Unknown:
            return QStringLiteral("unknown");
    }
    return QStringLiteral("none");
}

QString HeldCube::wrongSide() const {
    return sight_ == Sight::Wrong ? side_letter(wrong_side_) : QString();
}

void HeldCube::setSight(Sight sight, Face wrong_side) {
    if (sight == sight_ && (sight != Sight::Wrong || wrong_side == wrong_side_)) {
        return;
    }
    sight_ = sight;
    wrong_side_ = wrong_side;
    emit sightChanged();
}

std::optional<Face> HeldCube::side_turned_instead(const cube::vision::SideRead& read,
                                                  const cube::vision::SessionColours& colours,
                                                  float unmoved_distance) const {
    // Only with a hold that has been seen: if the cube might be held some
    // other way than shown, a turn and a different hold cannot be told apart.
    if (!seen_) {
        return std::nullopt;
    }
    const Cube& now = solve_.currentCube();
    const auto asked = solve_.currentMove();
    std::optional<Face> turned;
    for (const Face face : cube::solver::kAllFaces) {
        for (int turns = 1; turns <= 3; ++turns) {
            const cube::solver::Move move{face, turns};
            if (asked.has_value() && move == *asked) {
                continue;
            }
            const HoldReading after = cube::vision::read_hold(read, now.apply(move), colours);
            if (!after.shows(hold_) ||
                unmoved_distance < after.distance + cube::vision::kHoldClearDistance) {
                continue;
            }
            if (turned.has_value() && *turned != face) {
                return std::nullopt;  // turns of two different sides would both explain it
            }
            turned = face;
        }
    }
    return turned;
}

bool HeldCube::half_way_there(const cube::vision::SideRead& read,
                              const cube::vision::SessionColours& colours) const {
    const auto asked = solve_.currentMove();
    if (!asked.has_value() || asked->turns != 2) {
        return false;
    }
    for (const int turns : {1, 3}) {
        const Cube part_way = solve_.currentCube().apply(cube::solver::Move{asked->face, turns});
        if (cube::vision::read_hold(read, part_way, colours).shows(hold_)) {
            return true;
        }
    }
    return false;
}

void HeldCube::note(const cube::vision::SideRead& read, const cube::vision::SessionColours& colours,
                    const Verdict& verdict) const {
    if (diagnostics_ == nullptr || !diagnostics_->enabled()) {
        return;
    }
    using cube::diagnostics::Json;
    constexpr std::array<const char*, 6> kKinds = {"nothing", "held",  "moved",
                                                   "unsure",  "wrong", "unknown"};
    const Cube& now = solve_.currentCube();
    const auto asked = solve_.currentMove();
    const HoldReading here = cube::vision::read_hold(read, now, colours);

    Json data = Json::object();
    data.set("verdict", kKinds.at(static_cast<std::size_t>(verdict.kind)));
    data.set("move_index", solve_.moveIndex());
    data.set("asked", cube::diagnostics::move_json(asked));
    // The hold the screen is showing, and the one this frame says.
    data.set("shown_hold", hold_json(hold_));
    data.set("verdict_hold", hold_json(verdict.hold));
    data.set("seen", seen_);
    data.set("turned_instead", verdict.kind == Verdict::Kind::Wrong
                                   ? Json(cube::diagnostics::letter_of(verdict.side))
                                   : Json());

    // What was read: the nine colours and which stickers were found, as the
    // verdict's numbers came from them.
    data.set("stickers_found", read.stickers_found);
    Json found = Json::array();
    Json lab = Json::array();
    for (std::size_t i = 0; i < read.found.size(); ++i) {
        found.push(read.found.at(i));
        lab.push(cube::diagnostics::to_json(read.lab.at(i)));
    }
    data.set("found", std::move(found));
    data.set("lab", std::move(lab));

    // How far the side in view sits from the cube as it stood, and from the
    // cube after the move, and how many stickers disagree with each.
    Json as_it_stood = Json::object();
    as_it_stood.set("distance", here.distance);
    as_it_stood.set("stickers_off", cube::vision::stickers_off(
                                        read, cube::solver::camera_view(now, hold_), colours));
    data.set("as_it_stood", std::move(as_it_stood));
    if (asked.has_value()) {
        const Cube next = now.apply(*asked);
        Json after = Json::object();
        after.set("distance", cube::vision::read_hold(read, next, colours).distance);
        after.set("stickers_off", cube::vision::stickers_off(
                                      read, cube::solver::camera_view(next, hold_), colours));
        data.set("after_move", std::move(after));
    } else {
        data.set("after_move", Json());
    }
    diagnostics_->write("held", std::move(data));
}

cube::diagnostics::Json HeldCube::diagnosticState() const {
    using cube::diagnostics::Json;
    Json state = Json::object();
    state.set("hold", hold_json(hold_));
    state.set("seen", seen_);
    state.set("sight", sight().toStdString());
    state.set("wrong_side", wrongSide().toStdString());
    state.set("streak", streak_);
    state.set("move_announced", move_announced_);
    state.set("matched_this_move", matched_this_move_);
    return state;
}

void HeldCube::forget() {
    last_ = Verdict{};
    streak_ = 0;
    previous_centre_.reset();
}

void HeldCube::onSolveChanged() {
    if (solve_.size() == 3) {
        if (solve_.currentCube() == judged_cube_) {
            return;
        }
        judged_cube_ = solve_.currentCube();
    } else {
        // Another size has no 3x3 to compare: the cube expected is the one at this move.
        if (solve_.moveIndex() == judged_index_) {
            return;
        }
        judged_index_ = solve_.moveIndex();
    }
    // A different cube is expected now: whatever was being counted towards
    // was about the one before.
    move_announced_ = false;
    matched_this_move_ = false;
    last_ = Verdict{};
    streak_ = 0;
}

std::optional<Hold> HeldCube::choose(const std::vector<Hold>& holds) const {
    if (holds.size() == 1) {
        return holds.front();
    }
    for (const Hold& hold : holds) {
        if (hold == hold_) {
            return hold;
        }
    }
    // A side that looks the same turned does not say which way up it is.
    // Someone who turns a cube round to show another side mostly keeps the
    // same side on top, so that is the reading taken - if there is one.
    for (const Hold& hold : holds) {
        if (hold.up == hold_.up) {
            return hold;
        }
    }
    return std::nullopt;
}

HeldCube::Verdict HeldCube::judge(const cube::vision::SideRead& read,
                                  const cube::vision::SessionColours& colours) const {
    CUBE_ZONE_SCOPED_N("held_cube.judge");
    using Kind = Verdict::Kind;
    const Cube& now = solve_.currentCube();
    // Every comparison counts only the stickers the reader found: a cube in
    // the hand has fingers on it, and a sticker under a finger is no
    // evidence of anything.
    const HoldReading here = cube::vision::read_hold(read, now, colours);

    // Has the move been made? Asked first, and asked as a comparison: the
    // side in view is on the cube as it stands after the move, and the cube
    // as it stood explains it clearly worse. A move can change as little as
    // one sticker of a side, which the cube before it would still fit
    // loosely - but never as well.
    std::optional<Cube> next;
    std::optional<HoldReading> there;
    if (const auto move = solve_.currentMove(); move.has_value()) {
        next = now.apply(*move);
        there = cube::vision::read_hold(read, *next, colours);
        if (!there->holds.empty() &&
            here.distance >= there->distance + cube::vision::kHoldClearDistance) {
            if (const auto hold = choose(there->holds); hold.has_value()) {
                return say(Kind::Moved, *hold);
            }
            return say(Kind::Unsure);
        }
    }

    if (here.shows(hold_)) {
        return say(Kind::Held, hold_);
    }
    if (there.has_value() && there->shows(hold_)) {
        // It fits both ways about as well: the cube as it stood, held
        // differently, and the cube after the move, held as it was. That is
        // the side that was turned, facing the camera - a turned side and a
        // turned cube look the same from straight on.
        //
        // If this cube was seen held as shown while this very move was being
        // asked for, it is the move: that side was just there the other way
        // round, and turning it is what the screen asked for. Without that
        // sighting neither is assumed, and another side will settle it.
        if (matched_this_move_) {
            return say(Kind::Moved, hold_);
        }
        return say(Kind::Unsure);
    }

    if (!here.holds.empty()) {
        if (const auto hold = choose(here.holds); hold.has_value()) {
            return say(Kind::Held, *hold);
        }
        return say(Kind::Unsure);
    }

    // What is left is a side that is neither the cube as it stood nor the
    // cube after the move. Before calling that a mistake the evidence has to
    // be good, because being told a correct move was wrong is worse than
    // having to press Next:
    //  - the whole side was seen, all nine stickers found, and
    //  - at least two of them are plainly not the colour either cube has
    //    there. One sticker out is a glint, a shadow or a fingertip at least
    //    as often as it is a sticker in the wrong place.
    if (read.stickers_found < 9) {
        return say(Kind::Nothing);
    }
    const int off_as_it_stood =
        cube::vision::stickers_off(read, cube::solver::camera_view(now, hold_), colours);
    const int off_after_move =
        next.has_value()
            ? cube::vision::stickers_off(read, cube::solver::camera_view(*next, hold_), colours)
            : off_as_it_stood;
    if (off_as_it_stood < 2 || off_after_move < 2) {
        return say(Kind::Nothing);
    }
    // A half turn made a quarter at a time passes through a cube that is
    // neither: that is the move under way, not a wrong one.
    if (half_way_there(read, colours)) {
        return say(Kind::Nothing);
    }
    // A turn of some other side - or of the right side the wrong way - is
    // the likeliest thing to have happened, and worth naming if it is the
    // only one that fits.
    if (const auto side = side_turned_instead(read, colours, here.distance); side.has_value()) {
        return say(Kind::Wrong, hold_, *side);
    }
    return say(Kind::Unknown);
}

// ---- a cube of any size ------------------------------------------------------------------
//
// The same questions as judge(), asked of the plan's cube (cube::nxn) and a side of N x N
// cells; docs/nxn-camera-follow.md says why each rule is what it is.

namespace {

// The cells of the side in view that the move changes, from `hold`.
[[nodiscard]] std::vector<std::size_t> changed_cells(const cube::nxn::NCube& before,
                                                     const cube::nxn::NCube& after,
                                                     const Hold& hold) {
    const std::vector<cube::solver::Color> a = cube::nxn::camera_view(before, hold);
    const std::vector<cube::solver::Color> b = cube::nxn::camera_view(after, hold);
    std::vector<std::size_t> cells;
    for (std::size_t cell = 0; cell < a.size(); ++cell) {
        if (a.at(cell) != b.at(cell)) {
            cells.push_back(cell);
        }
    }
    return cells;
}

// Fewer cells than this changing and a move is not followed: one cell is as likely a glint
// as a turn.
constexpr std::size_t kLeastCellsChanged = 2;

}  // namespace

bool HeldCube::move_decided(const cube::vision::SideRead& read, const cube::nxn::NCube& before,
                            const cube::nxn::NCube& after, const Hold& hold,
                            const cube::vision::SessionColours& colours) const {
    const std::vector<std::size_t> cells = changed_cells(before, after, hold);
    if (cells.size() < kLeastCellsChanged) {
        return false;
    }
    const std::vector<cube::solver::Color> was = cube::nxn::camera_view(before, hold);
    const std::vector<cube::solver::Color> will_be = cube::nxn::camera_view(after, hold);
    std::size_t found = 0;
    std::size_t agree = 0;
    for (const std::size_t cell : cells) {
        if (!read.found.at(cell)) {
            continue;
        }
        ++found;
        const float to_was = cube::vision::colour_distance(
            read.lab.at(cell), colours.at(static_cast<std::size_t>(was.at(cell))));
        const float to_will_be = cube::vision::colour_distance(
            read.lab.at(cell), colours.at(static_cast<std::size_t>(will_be.at(cell))));
        agree += to_will_be < to_was ? 1U : 0U;
    }
    // Enough of the cells that decide it were seen (two, and half of them)...
    if (found < kLeastCellsChanged || 2 * found < cells.size()) {
        return false;
    }
    // ...and four fifths of those say it was made.
    return 5 * agree >= 4 * found;
}

HeldCube::Verdict HeldCube::judge_n(const cube::vision::SideRead& read,
                                    const cube::vision::SessionColours& colours) const {
    CUBE_ZONE_SCOPED_N("held_cube.judge_n");
    using Kind = Verdict::Kind;
    const cube::nxn::SteppedPlan& plan = solve_.plan();
    const cube::nxn::NCube& now = plan.current_cube();
    const int size = now.size();
    const float clear = cube::vision::hold_clear_distance(size);
    const HoldReading here = cube::vision::read_hold(read, now, colours);

    // Has the move been made? As for a 3x3: the side in view is the cube after the move, and
    // the cube as it stood explains it clearly worse - and, for a side of this many cells,
    // the cells the move changes agree (move_decided). A move that changes too few cells to
    // decide on is not decided, however well the mean fits.
    std::optional<cube::nxn::NCube> next;
    std::optional<HoldReading> there;
    if (!plan.at_end()) {
        next = plan.cube_at(plan.current_index() + 1);
        there = cube::vision::read_hold(read, *next, colours);
        if (!there->holds.empty() && here.distance >= there->distance + clear) {
            if (const auto hold = choose(there->holds); hold.has_value()) {
                if (move_decided(read, now, *next, *hold, colours)) {
                    return say(Kind::Moved, *hold);
                }
            } else {
                return say(Kind::Unsure);
            }
        }
    }

    if (here.shows(hold_)) {
        return say(Kind::Held, hold_);
    }
    if (there.has_value() && there->shows(hold_)) {
        // The side that was turned, facing the camera: a turned side and a rolled cube look
        // the same (see judge()).
        if (matched_this_move_) {
            return say(Kind::Moved, hold_);
        }
        return say(Kind::Unsure);
    }

    if (!here.holds.empty()) {
        if (const auto hold = choose(here.holds); hold.has_value()) {
            return say(Kind::Held, *hold);
        }
        return say(Kind::Unsure);
    }

    // Neither the cube as it stood nor the cube after the move. A mistake is said only of a
    // side seen whole, with plainly wrong cells in a share that grows with the side (two for
    // a 3x3, a twelfth of the cells above that).
    const auto cells = static_cast<int>(read.found.size());
    if (read.stickers_found < cells) {
        return say(Kind::Nothing);
    }
    const int needed = std::max(2, cells / 12);
    const int off_as_it_stood =
        cube::vision::stickers_off(read, cube::nxn::camera_view(now, hold_), colours);
    const int off_after_move =
        next.has_value()
            ? cube::vision::stickers_off(read, cube::nxn::camera_view(*next, hold_), colours)
            : off_as_it_stood;
    if (off_as_it_stood < needed || off_after_move < needed) {
        return say(Kind::Nothing);
    }
    if (half_way_there_n(read, colours)) {
        return say(Kind::Nothing);
    }
    if (const auto side = side_turned_instead_n(read, colours, here.distance); side.has_value()) {
        return say(Kind::Wrong, hold_, *side);
    }
    return say(Kind::Unknown);
}

std::optional<Face> HeldCube::side_turned_instead_n(const cube::vision::SideRead& read,
                                                    const cube::vision::SessionColours& colours,
                                                    float unmoved_distance) const {
    if (!seen_) {
        return std::nullopt;
    }
    const cube::nxn::SteppedPlan& plan = solve_.plan();
    const cube::nxn::NCube& now = plan.current_cube();
    const float clear = cube::vision::hold_clear_distance(now.size());
    // The layers the asked move turns, as turns of a slice: turning one of them the way it asks
    // is not "instead". A layer is spelt from either end of its axis, so a layer is tried once,
    // from the end it is nearer (the positive side's when it is in the middle).
    const int size = now.size();
    std::vector<cube::nxn::SliceTurn> asked;
    if (!plan.at_end()) {
        for (const cube::nxn::LayerMove& layer : cube::nxn::layers_of(plan.current_move())) {
            asked.push_back(cube::nxn::to_slice_turn(layer, size));
        }
    }
    std::optional<Face> turned;
    for (const Face face : cube::solver::kAllFaces) {
        for (int depth = 0; depth < size; ++depth) {
            const int other_end = size - 1 - depth;
            if (depth > other_end || (depth == other_end && !cube::nxn::is_positive(face))) {
                continue;
            }
            for (int turns = 1; turns <= 3; ++turns) {
                const cube::nxn::LayerMove move{.face = face, .depth = depth, .turns = turns};
                if (std::ranges::find(asked, cube::nxn::to_slice_turn(move, size)) != asked.end()) {
                    continue;
                }
                cube::nxn::NCube after = now;
                after.apply(move);
                const HoldReading reading = cube::vision::read_hold(read, after, colours);
                if (!reading.shows(hold_) || unmoved_distance < reading.distance + clear) {
                    continue;
                }
                if (turned.has_value() && *turned != face) {
                    return std::nullopt;  // turns of two different sides would both explain it
                }
                turned = face;
            }
        }
    }
    return turned;
}

bool HeldCube::half_way_there_n(const cube::vision::SideRead& read,
                                const cube::vision::SessionColours& colours) const {
    const cube::nxn::SteppedPlan& plan = solve_.plan();
    if (plan.at_end() || plan.current_move().turns != 2) {
        return false;
    }
    for (const int turns : {1, 3}) {
        cube::nxn::NCube part_way = plan.current_cube();
        for (const cube::nxn::LayerMove& layer : cube::nxn::layers_of(plan.current_move())) {
            part_way.apply(
                cube::nxn::LayerMove{.face = layer.face, .depth = layer.depth, .turns = turns});
        }
        if (cube::vision::read_hold(read, part_way, colours).shows(hold_)) {
            return true;
        }
    }
    return false;
}

void HeldCube::note_n(const cube::vision::SideRead& read, const Verdict& verdict) const {
    if (diagnostics_ == nullptr || !diagnostics_->enabled()) {
        return;
    }
    using cube::diagnostics::Json;
    constexpr std::array<const char*, 6> kKinds = {"nothing", "held",  "moved",
                                                   "unsure",  "wrong", "unknown"};
    Json data = Json::object();
    data.set("verdict", kKinds.at(static_cast<std::size_t>(verdict.kind)));
    data.set("size", solve_.size());
    data.set("move_index", solve_.moveIndex());
    data.set("shown_hold", hold_json(hold_));
    data.set("verdict_hold", hold_json(verdict.hold));
    data.set("seen", seen_);
    data.set("turned_instead", verdict.kind == Verdict::Kind::Wrong
                                   ? Json(cube::diagnostics::letter_of(verdict.side))
                                   : Json());
    data.set("stickers_found", read.stickers_found);
    diagnostics_->write("held", std::move(data));
}

void HeldCube::updateMoveHidden() {
    bool hidden = false;
    if (!solve_.isSolving() && !solve_.atEnd()) {
        if (solve_.size() == 3) {
            // The side facing the holder is the one side the camera never sees change.
            const auto move = solve_.currentMove();
            hidden = move.has_value() && cube::solver::place_of(hold_, move->face) == Place::Front;
        } else {
            const cube::nxn::SteppedPlan& plan = solve_.plan();
            hidden =
                changed_cells(plan.current_cube(), plan.cube_at(plan.current_index() + 1), hold_)
                    .size() < kLeastCellsChanged;
        }
    }
    if (hidden != move_hidden_) {
        move_hidden_ = hidden;
        emit moveHiddenChanged();
    }
}

void HeldCube::onFrameWatched() {
    CUBE_ZONE_SCOPED_N("held_cube.on_frame_watched");
    const std::optional<cube::vision::SideRead>& read = scan_.read();
    const std::optional<cube::vision::SessionColours> colours = scan_.colours();
    // What is read is a side of the size of the cube being solved: a read of any other size
    // (a scan left at another size) is a different cube's and means nothing here.
    const int size = solve_.size();
    if (!read.has_value() || !colours.has_value() || solve_.isSolving() || scan_.size() != size ||
        read->size != size) {
        forget();
        setSight(Sight::None);
        return;
    }

    // Held still since the frame before, as a read has to be for a scan.
    const bool steady =
        previous_centre_.has_value() && cube::vision::held_still(*read, *previous_centre_);
    previous_centre_ = cube::vision::steady_point(*read);
    if (!steady) {
        last_ = Verdict{};
        streak_ = 0;
        return;
    }

    onSolveChanged();
    const Verdict verdict = size == 3 ? judge(*read, *colours) : judge_n(*read, *colours);
    if (!(verdict == last_)) {
        if (size == 3) {
            note(*read, *colours, verdict);
        } else {
            note_n(*read, verdict);
        }
    }
    streak_ = verdict == last_ ? streak_ + 1 : 1;
    last_ = verdict;

    using Kind = Verdict::Kind;
    switch (verdict.kind) {
        case Kind::Nothing:
            // A side is in view and there is nothing to say about it, good
            // or bad. Whatever was said of an earlier one no longer stands.
            if (streak_ >= kFramesToTrust) {
                setSight(Sight::None);
            }
            break;
        case Kind::Held:
            if (streak_ >= kFramesToTrust) {
                const bool first_sight = !seen_;
                seen_ = true;
                if (verdict.hold != hold_) {
                    setHold(verdict.hold);
                } else if (first_sight) {
                    emit holdChanged();
                }
                matched_this_move_ = true;
                setSight(Sight::Matched);
            }
            break;
        case Kind::Moved:
            if (streak_ >= kFramesToTrust && !move_announced_) {
                move_announced_ = true;
                const bool first_sight = !seen_;
                seen_ = true;
                if (verdict.hold != hold_) {
                    setHold(verdict.hold);
                } else if (first_sight) {
                    emit holdChanged();
                }
                setSight(Sight::Matched);
                emit moveSeen();
            }
            break;
        case Kind::Unsure:
            if (streak_ >= kFramesToTrust) {
                setSight(Sight::Unsure);
            }
            break;
        case Kind::Wrong:
            if (streak_ >= kFramesToAccuse) {
                setSight(Sight::Wrong, verdict.side);
            }
            break;
        case Kind::Unknown:
            if (streak_ >= kFramesToDoubt) {
                setSight(Sight::Unknown);
            }
            break;
    }
}

}  // namespace cube::app
