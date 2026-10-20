// The solve screen for a cube that is not a 3x3: SolveScreen.qml loaded with the
// app's own controllers behind it, a bigger cube solved in them, and the screen
// stepped through the solution by the functions its buttons call, as
// solve_screen_test.cpp does for a 3x3.
//
// What is held here is that the screen draws the library's cube at every step,
// that the layers it turns are the layers the move names, that the stages of a
// reduction are worded, that a person who can reduce the cube may skip to the 3x3
// stage, that what only a 3x3 has (the camera) is not offered, and that the way back to
// a scan (the rescan) is, at the size of the cube being solved.
#include <catch2/catch_test_macros.hpp>

#include "camera_preview_support.hpp"
#include "controllers/direct_scan.hpp"
#include "cube_view_reading.hpp"
#include "drawn_cube.hpp"
#include "qml_test_app.hpp"
#include "scanned_nxn_support.hpp"

#include <cube/nxn/cube.hpp>
#include <cube/nxn/geometry.hpp>
#include <cube/nxn/outside_view.hpp>
#include <cube/nxn/scan_assembly.hpp>
#include <cube/nxn/stage.hpp>
#include <cube/solver/cube.hpp>
#include <cube/vision/direct_scan.hpp>

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QMetaObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <algorithm>
#include <memory>

using cube::nxn::NCube;
using cube::solver::Color;
using cube::test::wait_for;

namespace {

[[nodiscard]] QString colour_name(Color colour) {
    switch (colour) {
        case Color::White:
            return QStringLiteral("White");
        case Color::Yellow:
            return QStringLiteral("Yellow");
        case Color::Green:
            return QStringLiteral("Green");
        case Color::Blue:
            return QStringLiteral("Blue");
        case Color::Orange:
            return QStringLiteral("Orange");
        case Color::Red:
            return QStringLiteral("Red");
    }
    return {};
}

// Solves `scrambled` in the app's own SolveController and waits for it.
SolveController& solve_in_app(const NCube& scrambled) {
    SolveController& solver = cube::test::app_controllers().solve;
    solver.solveCube(scrambled);
    REQUIRE(wait_for([&]() { return !solver.isSolving(); }, 60'000));
    REQUIRE(solver.totalMoves() > 0);
    REQUIRE(solver.size() == scrambled.size());
    return solver;
}

// Whether `made` is `truth` or the same cube held another way up: a scan puts a side where
// its centre colour says it goes, so the cube comes back in the orientation its centres name.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the cube made, then the cube it should be
[[nodiscard]] bool is_cube(const NCube& made, const NCube& truth) {
    const cube::nxn::Geometry& geometry = cube::nxn::Geometry::of(truth.size());
    for (std::size_t rotation = 0; rotation < geometry.rotations().size(); ++rotation) {
        NCube held = truth;
        held.apply(geometry.rotation_permutation(rotation));
        if (held == made) {
            return true;
        }
    }
    return false;
}

class Screen {
public:
    Screen() {
        QQmlComponent component(&cube::test::qml_engine());
        component.loadFromModule("Cubeulator", "SolveScreen");
        if (component.isError()) {
            FAIL(component.errorString().toStdString());
        }
        item_.reset(qobject_cast<QQuickItem*>(component.create()));
        if (item_ == nullptr) {
            FAIL(component.errorString().toStdString());
        }
        item_->setWidth(900);
        item_->setHeight(640);
        cube_ = item_->findChild<QQuickItem*>(QStringLiteral("solve.cube"));
        REQUIRE(cube_ != nullptr);
        // How long things take to move is not under test.
        REQUIRE(cube_->setProperty("turnDuration", 20));
        REQUIRE(cube_->setProperty("viewDuration", 0));
    }

    [[nodiscard]] QQuickItem* cube() const { return cube_; }
    [[nodiscard]] QQuickItem* item() const { return item_.get(); }
    [[nodiscard]] QObject* find(const char* name) const {
        return item_->findChild<QObject*>(QString::fromLatin1(name));
    }
    [[nodiscard]] QVariant get(const char* name) const { return item_->property(name); }
    void call(const char* method) { REQUIRE(QMetaObject::invokeMethod(item_.get(), method)); }

    // The colour the screen draws a sticker of `colour` in.
    [[nodiscard]] QColor paint(Color colour) const {
        QVariant result;
        REQUIRE(QMetaObject::invokeMethod(item_.get(), "colorHex", Q_RETURN_ARG(QVariant, result),
                                          Q_ARG(QVariant, colour_name(colour))));
        return result.value<QColor>();
    }

    [[nodiscard]] bool shows(const NCube& cube) const {
        return cube::test::shows(cube_, cube, [this](Color colour) { return paint(colour); });
    }

    // The words the screen has for a stage value.
    [[nodiscard]] QVariantMap copy_for(int stage) const {
        QVariant result;
        REQUIRE(QMetaObject::invokeMethod(item_.get(), "copyFor", Q_RETURN_ARG(QVariant, result),
                                          Q_ARG(QVariant, stage)));
        return result.toMap();
    }

    // Lets the screen catch up: the view finishes turning, the guide restarts.
    void settle() const {
        REQUIRE(wait_for([this]() {
            return !cube_->property("viewMoving").toBool() && !cube_->property("playing").toBool();
        }));
        QCoreApplication::processEvents();
        lay_out();
    }

    // The window's size, as a phone's. Positioners lay their children out when a window
    // polishes them, and there is no window here, so each is asked (settle() does it
    // again after every step).
    void resize(int width, int height) const {
        item_->setWidth(width);
        item_->setHeight(height);
        settle();
    }

    // The cube's box in the screen's own coordinates.
    [[nodiscard]] QRectF cube_box() const {
        return cube_->mapRectToItem(item_.get(), QRectF(0, 0, cube_->width(), cube_->height()));
    }

    // Presses Next and waits for the layer to have turned and the solution to have moved on.
    void step(SolveController& solver) {
        const int index = solver.moveIndex();
        call("next");
        REQUIRE(wait_for([&]() { return solver.moveIndex() == index + 1; }));
    }

private:
    static void lay_out(QQuickItem* item) {
        if (item->inherits("QQuickBasePositioner")) {
            QMetaObject::invokeMethod(item, "forceLayout");
        }
        for (QQuickItem* child : item->childItems()) {
            lay_out(child);
        }
    }
    void lay_out() const {
        for (int pass = 0; pass < 3; ++pass) {
            lay_out(item_.get());
            QCoreApplication::processEvents();
        }
    }

    std::unique_ptr<QQuickItem> item_;
    QQuickItem* cube_ = nullptr;
};

}  // namespace

TEST_CASE("the solve screen of a 5x5 steps through its solution on the library's cube",
          "[qml][solve-screen][nxn]") {
    const NCube scrambled = cube::nxn::random_scramble({.size = 5, .seed = 3, .moves = 80});
    SolveController& solver = solve_in_app(scrambled);

    Screen screen;
    screen.settle();
    // The view that draws a cube of any size, at the size the cube is.
    CHECK(screen.cube()->property("cubeSize").toInt() == 5);
    CHECK_FALSE(screen.get("threeByThree").toBool());
    REQUIRE(screen.shows(scrambled));

    // The first moves, one press each: the layers the move names are the ones that
    // are nudging, and the cube drawn is the cube the library has after each.
    for (int press = 0; press < 25 && !solver.atEnd(); ++press) {
        const QVariantMap move = solver.moves().at(solver.moveIndex()).toMap();
        INFO("move " << solver.moveIndex() << ": "
                     << move.value("notation").toString().toStdString());
        screen.settle();
        CHECK(screen.cube()->property("guideFace").toString() == move.value("face").toString());
        CHECK(screen.cube()->property("guideFirst").toInt() == move.value("first").toInt());
        CHECK(screen.cube()->property("guideLast").toInt() == move.value("last").toInt());
        CHECK(screen.cube()->property("guideTurns").toInt() == move.value("turns").toInt());

        screen.step(solver);
        REQUIRE(screen.shows(solver.plan().current_cube()));
    }

    // And back to the start, each step the last move the other way.
    while (!solver.atStart()) {
        const int index = solver.moveIndex();
        screen.call("previous");
        REQUIRE(wait_for([&]() { return solver.moveIndex() == index - 1; }));
        REQUIRE(screen.shows(solver.plan().current_cube()));
    }
    CHECK(solver.plan().current_cube() == scrambled);
}

TEST_CASE("the cube of a 5x5 is drawn in the shader style Settings has chosen",
          "[qml][solve-screen][nxn]") {
    // The style is the setting's, not the 3x3's alone: it reaches the cube of any size.
    const NCube scrambled = cube::nxn::random_scramble({.size = 5, .seed = 3, .moves = 80});
    solve_in_app(scrambled);
    Screen screen;
    screen.settle();

    QObject* settings =
        cube::test::qml_engine().singletonInstance<QObject*>("Cubeulator", "SettingsState");
    REQUIRE(settings != nullptr);
    const QVariant before = settings->property("shaderStyleIndex");
    CHECK(screen.cube()->property("shaderStyle").toInt() == before.toInt());
    for (const int style : {1, 2, 0}) {
        INFO("style " << style);
        REQUIRE(settings->setProperty("shaderStyleIndex", style));
        CHECK(screen.cube()->property("shaderStyle").toInt() == style);
        // Still the cube it was.
        CHECK(screen.shows(scrambled));
    }
    REQUIRE(settings->setProperty("shaderStyleIndex", before));
}
TEST_CASE("a 5x5 can be skipped to its 3x3 stage, and finished from there",
          "[qml][solve-screen][nxn]") {
    const NCube scrambled = cube::nxn::random_scramble({.size = 5, .seed = 4, .moves = 80});
    SolveController& solver = solve_in_app(scrambled);
    Screen screen;
    screen.settle();

    // The button is there at the start, and not before the solve has landed.
    QObject* skip = screen.find("solve.skip");
    REQUIRE(skip != nullptr);
    CHECK(skip->property("visible").toBool());
    QObject* button = screen.find("solve.skip.button");
    REQUIRE(button != nullptr);
    REQUIRE(QMetaObject::invokeMethod(button, "clicked"));

    CHECK(solver.moveIndex() == solver.coreStartIndex());
    CHECK(solver.plan().current_cube().is_reduced());
    screen.settle();
    CHECK(screen.shows(solver.plan().current_cube()));
    // It is not offered again from the 3x3's own stages.
    CHECK_FALSE(skip->property("visible").toBool());

    // The solution goes on from there, to a solved cube.
    while (!solver.atEnd()) {
        screen.step(solver);
    }
    CHECK(solver.plan().current_cube().is_solved());
    REQUIRE(screen.shows(solver.plan().current_cube()));
}

TEST_CASE("the cube of a 5x5 keeps its size on a phone from stage to stage and through the skip",
          "[qml][solve-screen][nxn]") {
    // What the panel under the cube holds is kept room for whether or not it is showing
    // (the way to skip to the 3x3 stage goes once the 3x3's stages begin), and whatever the
    // stage says: a cube that changed size as the stages went by would not be a cube
    // anyone could follow. And the panel is given what it needs before the cube is made
    // smaller than it can be followed at.
    constexpr double least_cube = 150;
    constexpr int phones[][2] = {{360, 640}, {390, 780}, {412, 915}};
    SolveController& solver =
        solve_in_app(cube::nxn::random_scramble({.size = 5, .seed = 9, .moves = 80}));
    for (const auto& phone : phones) {
        INFO(phone[0] << "x" << phone[1]);
        solver.goToMove(0);
        Screen screen;
        screen.resize(phone[0], phone[1]);
        const QRectF first = screen.cube_box();
        CHECK(first.width() >= least_cube - 0.5);
        CHECK(first.width() == first.height());
        CHECK(first.bottom() <= phone[1]);

        // Every stage's first move, the way to skip to the 3x3 stage, and the end.
        QList<int> where;
        for (const QVariant& range : solver.stageRanges()) {
            where.append(range.toMap().value("startIndex").toInt());
        }
        where.append(solver.coreStartIndex());
        where.append(solver.totalMoves());
        for (const int index : where) {
            INFO("move " << index);
            solver.goToMove(index);
            screen.settle();
            const QRectF now = screen.cube_box();
            CHECK(now.width() == first.width());
            CHECK(now.top() == first.top());
        }
        solver.goToMove(0);
        screen.settle();
        QObject* button = screen.find("solve.skip.button");
        REQUIRE(button != nullptr);
        REQUIRE(QMetaObject::invokeMethod(button, "clicked"));
        screen.settle();
        CHECK(screen.cube_box().width() == first.width());
        CHECK(screen.cube_box().top() == first.top());
    }
}

TEST_CASE("a 3x3 has no skip and still has its own view and its rescan",
          "[qml][solve-screen][nxn]") {
    SolveController& solver = cube::test::app_controllers().solve;
    solver.solveCube(NCube::from_cube3(cube::solver::random_scramble(7)));
    REQUIRE(wait_for([&]() { return !solver.isSolving(); }, 60'000));
    Screen screen;
    screen.settle();
    CHECK(screen.get("threeByThree").toBool());
    // Cube3DView has no cubeSize.
    CHECK_FALSE(screen.cube()->property("cubeSize").isValid());
    QObject* skip = screen.find("solve.skip");
    REQUIRE(skip != nullptr);
    CHECK_FALSE(skip->property("visible").toBool());
    QObject* rescan = screen.find("solve.rescan");
    REQUIRE(rescan != nullptr);
    CHECK(rescan->property("visible").toBool());
}

// The scan of a cube of `size` as the app does it (the camera reading six drawn sides, the
// solver handed the cube), left with the solve screen's camera watching and the screen asked
// for. Puts the app's state back when it goes out of scope: the 3x3 tests after these need the
// scan as it was found.
class ScannedAndSolving {
public:
    explicit ScannedAndSolving(int size)
        : app_(cube::test::app_controllers()),
          state_(cube::test::qml_engine().singletonInstance<QObject*>("Cubeulator", "AppState")),
          cube_(cube::test::unambiguous_cube(size, 11)) {
        REQUIRE(state_ != nullptr);
        app_.directScan.setWatching(false);
        app_.directScan.restart();
        app_.directScan.setSize(size);
        cube::test::show_all_sides(app_.directScan, cube_);
        REQUIRE(app_.directScan.complete());
        REQUIRE(wait_for([this]() { return !app_.solve.isSolving(); }, 120'000));
        REQUIRE(app_.solve.size() == size);
        REQUIRE(state_->setProperty("solveCamera", true));
        app_.directScan.setWatching(true);
    }
    ~ScannedAndSolving() {
        state_->setProperty("screen", QStringLiteral("launch"));
        state_->setProperty("solveCamera", true);
        app_.directScan.setWatching(false);
        app_.directScan.useDefaultSize();
        static_cast<void>(wait_for([this]() { return !app_.directScan.starting(); }));
    }
    ScannedAndSolving(const ScannedAndSolving&) = delete;
    ScannedAndSolving& operator=(const ScannedAndSolving&) = delete;

    [[nodiscard]] const NCube& cube() const { return cube_; }
    [[nodiscard]] QObject* state() const { return state_; }
    [[nodiscard]] cube::app::AppControllers& app() const { return app_; }

private:
    cube::app::AppControllers& app_;
    QObject* state_;
    NCube cube_;
};

TEST_CASE(
    "a cube that is not a 3x3 has the camera preview, marked at its size, and still its rescan",
    "[qml][solve-screen][nxn][camera-preview]") {
    for (const int size : {2, 4, 6}) {
        INFO("size " << size);
        const ScannedAndSolving scanned(size);
        Screen screen;
        screen.settle();
        CHECK(screen.cube()->property("cubeSize").toInt() == size);
        QObject* rescan = screen.find("solve.rescan");
        REQUIRE(rescan != nullptr);
        CHECK(rescan->property("visible").toBool());

        // The camera is on the cube in the hand, as it is for a 3x3.
        const auto* preview = qobject_cast<QQuickItem*>(screen.find("solve.camera"));
        REQUIRE(preview != nullptr);
        CHECK(preview->isVisible());

        // A side held up marks size x size stickers on the picture.
        const int stickers = size * size;
        const cube::test::Pose pose = cube::test::pose_for(size);
        for (int i = 0; i < 12; ++i) {
            scanned.app().directScan.processFrame(cube::test::draw_side(
                cube::nxn::outside_view(scanned.cube(), cube::solver::Face::F), pose));
        }
        REQUIRE(scanned.app().directScan.stickerPoints().size() == stickers);
        QCoreApplication::processEvents();
        int shown = 0;
        for (const QQuickItem* dot : cube::test::items_named(screen.item(), "solve.dot")) {
            shown += dot->isVisible() ? 1 : 0;
        }
        CHECK(shown == stickers);

        // What it says of a bigger cube is that it is shown and not followed, and the cube
        // is not matched against it: a side in view means nothing to the solution.
        const auto* words = qobject_cast<QQuickItem*>(screen.find("solve.words"));
        REQUIRE(words != nullptr);
        CHECK(words->isVisible());
        CHECK(words->property("text").toString() == screen.get("cameraWords").toString());
        CHECK(words->property("text").toString().contains("shownOnly"));
        CHECK(scanned.app().held.sight() == "none");
        CHECK_FALSE(scanned.app().held.seen());

        // Off for a solve that has no scan behind it: typed in, or carried on from history.
        REQUIRE(scanned.state()->setProperty("solveCamera", false));
        QCoreApplication::processEvents();
        CHECK_FALSE(preview->isVisible());
    }
}

TEST_CASE("the colours a camera read of a bigger cube can be looked over from the solve screen",
          "[qml][solve-screen][nxn][review]") {
    for (const int size : {2, 4}) {
        INFO("size " << size);
        const ScannedAndSolving scanned(size);
        REQUIRE(scanned.state()->setProperty("screen", QStringLiteral("solve")));
        Screen screen;
        screen.settle();
        const auto* check = qobject_cast<QQuickItem*>(screen.find("solve.checkColours"));
        REQUIRE(check != nullptr);
        CHECK(screen.get("canCheckColours").toBool());
        CHECK(check->isVisible());

        // To the colours screen at the cube's size...
        REQUIRE(QMetaObject::invokeMethod(const_cast<QQuickItem*>(check), "clicked"));
        CHECK(scanned.state()->property("screen").toString() == "colours");
        CHECK(scanned.app().colours.size() == size);
        CHECK(scanned.app().colours.fromScan());

        // ...and back is the solve; Scan again is a scan of a cube of this size, not of a 3x3.
        REQUIRE(QMetaObject::invokeMethod(scanned.state(), "closeColours"));
        CHECK(scanned.state()->property("screen").toString() == "solve");
        REQUIRE(QMetaObject::invokeMethod(scanned.state(), "reviewScanFromSolve"));
        REQUIRE(QMetaObject::invokeMethod(scanned.state(), "closeColours"));
        CHECK(scanned.state()->property("screen").toString() == "solve");
        REQUIRE(QMetaObject::invokeMethod(scanned.state(), "reviewScan"));
        REQUIRE(QMetaObject::invokeMethod(scanned.state(), "closeColours"));
        CHECK(scanned.state()->property("screen").toString() == "scan");
        CHECK(scanned.app().directScan.size() == size);
        CHECK_FALSE(scanned.app().directScan.autoSize());
    }
}

TEST_CASE("a bigger cube is rescanned part-way at its own size, and solved again from the scan",
          "[qml][solve-screen][nxn][rescan]") {
    cube::app::AppControllers& app = cube::test::app_controllers();
    SolveController& solver = app.solve;
    cube::app::DirectScan& camera = app.directScan;
    auto* state = cube::test::qml_engine().singletonInstance<QObject*>("Cubeulator", "AppState");
    REQUIRE(state != nullptr);

    // The controllers and the app state are the whole run's: the 3x3 tests after this one need the
    // scan as it was found (its size, no camera left opening on the scan screen).
    struct Restore {
        QObject* state;
        cube::app::DirectScan& scan;
        ~Restore() {
            state->setProperty("screen", QStringLiteral("launch"));
            state->setProperty("solveCamera", true);
            state->setProperty("elapsedSeconds", 0);
            scan.useDefaultSize();
            static_cast<void>(wait_for([this]() { return !scan.starting(); }));
        }
    } restore{state, camera};

    for (const int size : {4, 5}) {
        INFO("size " << size);
        const NCube first = cube::test::unambiguous_cube(size, 70);
        solve_in_app(first);
        // Something other than 3 was the last scan's size, and the scan's own is what a
        // person may have left it at: neither is the size of the cube being solved.
        camera.setSize(size == 4 ? 5 : 2);
        REQUIRE(state->setProperty("solveCamera", false));
        REQUIRE(state->setProperty("screen", QStringLiteral("solve")));
        REQUIRE(state->setProperty("elapsedSeconds", 42));

        Screen screen;
        screen.settle();
        screen.step(solver);
        screen.step(solver);

        QObject* rescan = screen.find("solve.rescan");
        REQUIRE(rescan != nullptr);
        REQUIRE(rescan->property("visible").toBool());
        REQUIRE(QMetaObject::invokeMethod(rescan, "clicked"));

        // The scan screen, at the size being solved and not found again; the camera is on it again
        // for the cube being solved; the clock is not started over.
        CHECK(state->property("screen").toString() == "scan");
        CHECK(camera.size() == size);
        CHECK_FALSE(camera.autoSize());
        CHECK(state->property("solveCamera").toBool());
        CHECK(state->property("elapsedSeconds").toInt() >= 42);
        CHECK(state->property("elapsedSeconds").toInt() < 45);

        // And a scan of the cube as it stands is solved from there: a new solution, from the
        // turn it starts at.
        const NCube standing = cube::test::unambiguous_cube(size, 200);
        camera.setWatching(false);
        camera.restart();
        cube::test::Pose pose;
        pose.pitch = 180.0F / static_cast<float>(size);
        int completed = 0;
        const auto counting = QObject::connect(&camera, &cube::app::DirectScan::scanCompleted,
                                               [&completed]() { ++completed; });
        for (const cube::solver::Face face : cube::solver::kAllFaces) {
            for (int i = 0; i < static_cast<int>(cube::vision::SideScan::kMinReads) + 4; ++i) {
                camera.processFrame(
                    cube::test::draw_side(cube::nxn::outside_view(standing, face), pose));
            }
            camera.processFrame(cube::test::room({640, 360}));
        }
        QObject::disconnect(counting);
        REQUIRE(completed == 1);
        REQUIRE(wait_for([&]() { return !solver.isSolving(); }, 120'000));
        REQUIRE(solver.size() == size);
        REQUIRE(solver.scannedNCube().has_value());
        // The cube that was scanned (held as its centres name), not the one that was being solved.
        CHECK(is_cube(*solver.scannedNCube(), standing));
        CHECK(*solver.scannedNCube() != first);
        CHECK(solver.scannedNCube()->size() == size);
        CHECK(solver.moveIndex() == 0);
        CHECK(solver.totalMoves() > 0);
        // Still the same clock.
        CHECK(state->property("elapsedSeconds").toInt() >= 42);
    }
}

TEST_CASE("every stage a bigger cube has is worded, and a layer inside it is named",
          "[qml][solve-screen][nxn]") {
    SolveController& solver =
        solve_in_app(cube::nxn::random_scramble({.size = 5, .seed = 6, .moves = 80}));
    Screen screen;
    screen.settle();

    // The stages of this solution, in the order they are done, are the reduction's
    // and then the 3x3's; each has a title and a sentence.
    bool reduction = false;
    for (const QVariant& range : solver.stageRanges()) {
        const int stage = range.toMap().value("stageIndex").toInt();
        INFO("stage " << stage);
        const QVariantMap copy = screen.copy_for(stage);
        CHECK_FALSE(copy.value("title").toString().isEmpty());
        CHECK_FALSE(copy.value("detail").toString().isEmpty());
        reduction = reduction || stage >= static_cast<int>(cube::nxn::StageId::Parity);
    }
    CHECK(reduction);

    // What is listed is what there is to do: a stage with no moves in this solution is
    // left out, and the one the solution ends in is the last of the list.
    const QVariantList order = screen.get("stageOrder").toList();
    CHECK_FALSE(order.isEmpty());
    for (const QVariant& stage : order) {
        QVariant range;
        REQUIRE(QMetaObject::invokeMethod(screen.item(), "rangeOf", Q_RETURN_ARG(QVariant, range),
                                          Q_ARG(QVariant, stage)));
        CHECK(range.toMap().value("endIndex").toInt() > range.toMap().value("startIndex").toInt());
    }
    solver.goToMove(solver.totalMoves());
    screen.settle();
    CHECK(screen.get("stagePosition").toInt() == static_cast<int>(order.size()) - 1);
    solver.goToMove(0);
    screen.settle();
    for (const cube::nxn::StageId id : {cube::nxn::StageId::Parity, cube::nxn::StageId::Centres,
                                        cube::nxn::StageId::Edges, cube::nxn::StageId::Corners}) {
        CHECK_FALSE(screen.copy_for(static_cast<int>(id)).value("title").toString().isEmpty());
    }

    // A move of a layer inside the cube is worded by one of two strings (their ids
    // here, since no language is installed under test): one layer, or several. An
    // outer one is worded as a 3x3's.
    bool inner = false;
    bool outer = false;
    for (int press = 0; press < solver.totalMoves() && !(inner && outer); ++press) {
        screen.settle();
        const QString words = screen.get("moveWords").toString();
        const bool several = solver.currentMoveFirst() != solver.currentMoveLast();
        if (solver.currentMoveFirst() > 0 || several) {
            inner = true;
            CHECK(words.contains(several ? "solve.move.layers" : "solve.move.layer"));
        } else {
            outer = true;
            CHECK_FALSE(words.contains("solve.move"));
        }
        screen.step(solver);
    }
    CHECK(inner);
    CHECK(outer);
}

TEST_CASE("a 2x2 has one way to be solved, so no choice of method", "[qml][solve-screen][nxn]") {
    for (const int size : {2, 4}) {
        INFO("size " << size);
        solve_in_app(cube::nxn::random_scramble({.size = size, .seed = 3, .moves = 60}));
        Screen screen;
        screen.settle();
        const auto* methods = qobject_cast<QQuickItem*>(screen.find("solve.method"));
        REQUIRE(methods != nullptr);
        CHECK(methods->isVisible() == (size != 2));
    }
}

TEST_CASE("a side is named by its colour at any size", "[qml][solve-screen][nxn]") {
    for (const int size : {2, 4, 5}) {
        INFO("size " << size);
        solve_in_app(cube::nxn::random_scramble({.size = size, .seed = 8, .moves = 60}));
        Screen screen;
        screen.settle();
        const QString words = screen.get("holdWords").toString();
        // "Hold <colour> on top, <colour> facing you".
        CHECK(words.contains("on top"));
        CHECK(words.contains("facing you"));
        CHECK_FALSE(words.contains("undefined"));
    }
}

// Pictures of the solve screen of a bigger cube, for looking at rather than for
// checking. Hidden ("[.gallery]") and inert unless CUBE_GALLERY_DIR names a
// directory: the real SolveScreen in a real window, parked off-screen, saved as
// it renders. One picture per run; CUBE_GALLERY_SHOT is "<size>-<where>", where
// "start" is the first move, "inner" the first move of a layer inside the cube
// and "core" the screen after skipping to the 3x3 stage.
TEST_CASE("solve screen of a bigger cube gallery", "[.gallery]") {
    const QString out = qEnvironmentVariable("CUBE_GALLERY_DIR");
    if (out.isEmpty()) {
        SKIP("set CUBE_GALLERY_DIR to a directory to write the pictures to");
    }
    REQUIRE(QDir().mkpath(out));
    const QString wanted = qEnvironmentVariable("CUBE_GALLERY_SHOT", "5-start");
    const QStringList parts = wanted.split(QLatin1Char('-'));
    REQUIRE(parts.size() == 2);
    const int size = parts.at(0).toInt();
    SolveController& solver = solve_in_app(
        cube::nxn::random_scramble({.size = size, .seed = 5, .moves = 40 + (8 * size)}));
    auto* app_state =
        cube::test::qml_engine().singletonInstance<QObject*>("Cubeulator", "AppState");
    REQUIRE(app_state != nullptr);
    REQUIRE(app_state->setProperty("solveCamera", false));

    QQuickWindow window;
    window.setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    window.setPosition(-4000, -4000);
    QQmlComponent component(&cube::test::qml_engine());
    component.loadFromModule("Cubeulator", "SolveScreen");
    std::unique_ptr<QQuickItem> screen(
        qobject_cast<QQuickItem*>(component.beginCreate(cube::test::qml_engine().rootContext())));
    if (screen == nullptr) {
        FAIL(component.errorString().toStdString());
    }
    screen->setParentItem(window.contentItem());
    component.completeCreate();
    const int width = qEnvironmentVariable("CUBE_GALLERY_WIDTH", "900").toInt();
    const int height = qEnvironmentVariable("CUBE_GALLERY_HEIGHT", "640").toInt();
    screen->setWidth(width);
    screen->setHeight(height);
    window.resize(width, height);

    if (parts.at(1) == QLatin1String("core")) {
        solver.skipToCore();
    } else if (parts.at(1) == QLatin1String("inner")) {
        while (!solver.atEnd() && solver.currentMoveFirst() == 0) {
            solver.nextMove();
        }
    }
    window.show();
    QElapsedTimer timer;
    timer.start();
    const int wait = qEnvironmentVariable("CUBE_GALLERY_WAIT_MS", "1500").toInt();
    while (timer.elapsed() < wait) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    const QImage image = window.grabWindow();
    REQUIRE_FALSE(image.isNull());
    REQUIRE(image.save(QDir(out).filePath("solve-" + wanted + ".png")));
}
