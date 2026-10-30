// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import Cubeulator

// GPU-rendered Rubik's cube of any size from 2 to 9, for the solve and the
// solution screens when the cube is not a 3x3 (Cube3DView.qml is the 3x3's, and
// stays what it is until this beats it).
//
// It is drawn the way the 3x3 cannot be scaled to: not a node per cubie and a
// binding per sticker, which at 7x7 is hundreds of nodes, but as three scene
// objects whatever the size. The cube at rest is one mesh. While layers turn it
// is two: the layers, hung on a pivot, and everything else, with solid plastic
// where they part (CubeNGeometry, cube::nxn::build_mesh). A move is "lift the
// layers, turn them, put them down, and the colours move with them", and costs
// the same at every size (docs/performance.md, "The 3D view").
//
// The surface is the 3x3 view's, where it applies, so a screen can use either:
// faceletColors (a new object is a new cube), guideFace/guideTurns with the
// layers the move turns (guideFirst, guideLast), playMove(), movePlayed(), the
// hold and the viewpoint, the shader styles, and the drag that looks round the
// back. What a cube that is not a 3x3 does not have is the camera's hold and
// pose tracking, and the scan rings.
//
// The shader styles cost the single mesh nothing. The mesh's vertices carry the
// colour of the sticker they belong to, and the styles' materials read it from
// there (shaders/toon_mesh.frag, cel_mesh.frag) instead of from a uniform per
// sticker; Borderlands' ink outline is the whole mesh again, a hair larger and
// drawn inside out, where the 3x3 draws one per cubie.
Item {
    id: root

    // The side length of a cube nothing paints; a painting brings its own.
    property int size: 3
    // 0 = Physically-Based Realism (default), 1 = Instructional Glow,
    // 2 = Borderlands Cel-Shading, as for Cube3DView.
    property int shaderStyle: 0
    property color topColor: Theme.faceU
    property color bottomColor: Theme.faceD
    property color frontColor: Theme.faceF
    property color backColor: Theme.faceB
    property color leftColor: Theme.faceL
    property color rightColor: Theme.faceR

    property bool interactive: true
    property bool autoRotate: true
    property real autoRotateSpeed: 0.35 // degrees per animation tick while idle
    property bool showStats: false
    // Lights the cube alike from every side and takes the gloss off the
    // stickers (see Cube3DView.evenLight).
    property bool evenLight: false

    // Per-facelet sticker painting: null keeps the uniform face colours above;
    // otherwise a JS object mapping side letter -> N x N CSS colours, each side as
    // seen from outside, top-left first, row by row (cube::nxn::outside_view).
    //
    // A new painting is a new cube: every layer goes back where it started, in
    // the same pass that recolours the stickers. After playMove() has turned
    // layers, painting the cube as it now stands therefore changes nothing on
    // screen. Hand it a new object each time, as SolveScreen.painted() does.
    property var faceletColors: null
    onFaceletColorsChanged: resetLayers()

    // ---- Move guidance ----------------------------------------------------
    // The side whose layers the next move turns: "U", "D", "L", "R", "F", "B",
    // or "" for none. guideFirst and guideLast are the depth of the nearest and
    // the farthest layer it turns, counted from that side (0 is its outer
    // layer; a wide move has guideLast > guideFirst). While set, those layers
    // nudge a little way in the direction they have to go and back, over and
    // over. guideTurns is quarter turns clockwise as seen from outside that
    // side - 1, 2 or 3. A half turn nudges twice before it returns.
    property string guideFace: ""
    property int guideFirst: 0
    property int guideLast: 0
    property int guideTurns: 1
    onGuideFaceChanged: scheduleGuide()
    onGuideFirstChanged: scheduleGuide()
    onGuideLastChanged: scheduleGuide()
    onGuideTurnsChanged: scheduleGuide()

    // How far one nudge swings the layers, in degrees.
    property real nudgeAngle: 30
    // How long a quarter turn takes when playMove() turns layers for real.
    property int turnDuration: 380
    // How much the stickers outside the layers in hand darken, 0..1.
    property real guideDim: 0

    // True while playMove() is turning layers.
    property bool playing: false
    readonly property bool nudging: nudgeOnce.running || nudgeTwice.running
    // playMove()'s turn has finished and the layers are set down.
    signal movePlayed()

    // ---- How the cube is shown --------------------------------------------
    // The cube's own orientation: identity is U up, F towards the viewer and R
    // to the viewer's right.
    property quaternion holdRotation: Qt.quaternion(1, 0, 0, 0)
    // Where the cube is looked at from, in degrees: viewYaw turns it about the
    // vertical like a turntable, then viewPitch tips the top towards the viewer.
    property real viewPitch: 0
    property real viewYaw: 0
    property int viewDuration: 450
    property real shownPitch: viewPitch
    property real shownYaw: viewYaw
    Behavior on shownPitch { NumberAnimation { id: pitchSwing; duration: root.viewDuration; easing.type: Easing.InOutCubic } }
    Behavior on shownYaw { NumberAnimation { id: yawSwing; duration: root.viewDuration; easing.type: Easing.InOutCubic } }
    // True while the cube is on its way to a new viewpoint or orientation. The
    // nudge waits for it.
    readonly property bool viewMoving: pitchSwing.running || yawSwing.running || holdSwing.running
    onViewMovingChanged: scheduleGuide()

    // Dragging can turn the cube two ways. Left alone it spins it, as on the
    // launch screen. With dragTurnsView it moves the viewpoint instead, added
    // to viewYaw and viewPitch, and letting go brings the view back.
    property bool dragTurnsView: false
    property real dragYaw: 0
    property real dragPitch: 0
    function releaseView() {
        dragYaw = ((dragYaw + 180) % 360 + 360) % 360 - 180;
        viewSpring.restart();
    }

    readonly property bool dragging: dragHandler.active
    readonly property bool turning: playing
    property vector3d homeRotation: Qt.vector3d(-24, -32, 0)

    // The cube in view: its stickers, and the layers lifted, if any.
    readonly property int cubeSize: cubeModel.size
    property var held: null // { axis: 0|1|2, first, last } (layers from the negative end) | null
    property real turnAngle: 0
    property real playTarget: 0
    property real nudgeSign: 1

    // What an unpainted cube looks like: each side its own colour.
    function uniformPainting() {
        var n = root.size * root.size;
        var colours = { U: root.topColor, D: root.bottomColor, F: root.frontColor,
                        B: root.backColor, L: root.leftColor, R: root.rightColor };
        var out = {};
        for (var side in colours) {
            var list = [];
            for (var i = 0; i < n; i++)
                list.push(colours[side]);
            out[side] = list;
        }
        return out;
    }

    CubeNModel {
        id: cubeModel
        defaultSize: root.size
        faceletColors: root.faceletColors !== null ? root.faceletColors : root.uniformPainting()
    }

    // ---- Turns ------------------------------------------------------------
    readonly property var sideAxis: ({ U: 1, D: 1, R: 0, L: 0, F: 2, B: 2 })
    readonly property var sidePositive: ({ U: true, R: true, F: true, D: false, L: false, B: false })

    // The turn a move makes: which layers, about which axis, and how many
    // degrees about the positive direction of that axis (right-handed, so
    // positive is anticlockwise seen from the positive end). A move is quarter
    // turns clockwise seen from outside its side, and D, L and B are looked at
    // from the negative end of their axis - so for them clockwise is positive.
    // `first` and `last` count from the side. Null for anything not a move.
    function moveTurn(face, first, last, turns) {
        if (!(face in sideAxis) || turns < 1 || turns > 3)
            return null;
        var n = root.cubeSize;
        if (first < 0 || last < first || last >= n)
            return null;
        var positive = sidePositive[face];
        var quarters = turns === 3 ? -1 : turns; // three clockwise is one anticlockwise
        return { axis: sideAxis[face],
                 first: positive ? n - 1 - last : first,
                 last: positive ? n - 1 - first : last,
                 degrees: (positive ? -90 : 90) * quarters };
    }

    // Turns the layers `first` to `last` of `face` (counted from it, 0 its outer
    // layer) the whole way - `turns` quarter turns clockwise seen from outside it
    // - then sets them down and emits movePlayed. A turn already under way is
    // finished on the spot first.
    function playMove(face, first, last, turns) {
        var turn = moveTurn(face, first, last, turns);
        if (!turn)
            return false;
        playTurn(turn);
        return true;
    }

    function playTurn(turn) {
        finishPlaying();
        stopNudge();
        if (!held || held.axis !== turn.axis || held.first !== turn.first || held.last !== turn.last) {
            setDown(0);
            pickUp(turn);
        }
        playTarget = turn.degrees;
        playing = true;
        playAnim.from = turnAngle;
        playAnim.to = playTarget;
        playAnim.duration = turnDuration * (Math.abs(playTarget) > 90 ? 1.5 : 1);
        playAnim.start();
    }

    function finishPlaying() {
        if (!playing)
            return;
        playAnim.stop();
        endPlay();
    }

    function endPlay() {
        if (!playing)
            return;
        setDown(playTarget);
        playing = false;
        scheduleGuide();
        movePlayed();
    }

    function pickUp(turn) {
        turnAngle = 0;
        pivot.axis = Qt.vector3d(turn.axis === 0 ? 1 : 0, turn.axis === 1 ? 1 : 0, turn.axis === 2 ? 1 : 0);
        held = { axis: turn.axis, first: turn.first, last: turn.last };
    }

    // Puts the layers in hand back on the cube, `degrees` (a multiple of 90)
    // round from where they were picked up, and the colours with them.
    function setDown(degrees) {
        if (!held)
            return;
        var lifted = held;
        // Clockwise seen from the positive end is the negative direction.
        var quarters = (((Math.round(-degrees / 90)) % 4) + 4) % 4;
        if (quarters !== 0)
            cubeModel.turn(lifted.axis, lifted.first, lifted.last, quarters);
        turnAngle = 0;
        held = null;
    }

    // Nothing lifted, nothing turning.
    function resetLayers() {
        playAnim.stop();
        playing = false;
        stopNudge();
        turnAngle = 0;
        held = null;
        scheduleGuide();
    }

    function stopNudge() {
        nudgeOnce.stop();
        nudgeTwice.stop();
    }

    // The guide restarts once things have settled: a step of the solve changes
    // the painting, the side and the turn count together, and the layers should
    // be picked up once, from the cube as repainted.
    // A Timer of the view's own, not Qt.callLater: a callLater scheduled as the
    // screen is torn down (a step of the solve, then the screen leaves) runs
    // after the view's context has gone, and the call into its own functions raises
    // "... is not a function (exception occurred during delayed function
    // evaluation)". That warning failed the macOS system
    // walk (history_test.cpp, "an unfinished solve is carried on") on every
    // run for days. A Timer is destroyed with the view and never fires after.
    // Restarting it is the same coalescing callLater gave.
    Timer {
        id: guideSoon
        interval: 0
        onTriggered: root.applyGuide()
    }

    function scheduleGuide() { guideSoon.restart(); }

    function applyGuide() {
        if (playing)
            return;
        stopNudge();
        setDown(0);
        var turn = moveTurn(guideFace, guideFirst, guideLast, guideTurns);
        if (!turn)
            return;
        pickUp(turn);
        if (viewMoving)
            return; // once the cube has stopped turning, this runs again
        nudgeSign = turn.degrees > 0 ? 1 : -1;
        if (Math.abs(turn.degrees) > 90)
            nudgeTwice.restart();
        else
            nudgeOnce.restart();
    }

    // Every sticker as it sits, in the cube's own frame and in cubie spacings:
    //   { color: "#rrggbb", cubie: [x, y, z], facing: [x, y, z] }
    // x runs L to R, y D to U, z B to F, and a cubie's coordinates run from
    // -(N-1)/2 to (N-1)/2. This is what the tests compare with cube::nxn::NCube.
    function stickerPlacements() {
        return cubeModel.stickerPlacements();
    }

    // Which way each side faces in the scene, where the viewer looks along -z
    // with +y up and +x to the right: { U: [x, y, z], ... }.
    function sideDirections() {
        var out = {};
        var axes = { U: [0, 1, 0], D: [0, -1, 0], F: [0, 0, 1], B: [0, 0, -1], L: [-1, 0, 0], R: [1, 0, 0] };
        for (var letter in axes) {
            var a = axes[letter];
            var d = holdNode.mapDirectionToScene(Qt.vector3d(a[0], a[1], a[2]));
            out[letter] = [d.x, d.y, d.z];
        }
        return out;
    }

    View3D {
        id: view3d
        anchors.fill: parent
        camera: camera

        // The same prefiltered sky as the 3x3's, as a light probe
        // (docs/performance.md, "The light probe").
        Texture {
            id: sky
            source: "lightprobe/sky.ktx"
        }

        // Borderlands Cel-Shading's vignette and halftone post pass come from
        // ExtendedSceneEnvironment, as on the 3x3. It is only the environment
        // while that style is: drawing with it costs a frame about 0.05 ms of
        // render time and shifts the edges' anti-aliasing even with nothing
        // switched on, which the other two looks have no use for.
        environment: root.shaderStyle === 2 ? celEnvironment : plainEnvironment

        SceneEnvironment {
            id: plainEnvironment
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
            lightProbe: sky
            probeExposure: root.evenLight ? 0.35 : 1.0
        }
        ExtendedSceneEnvironment {
            id: celEnvironment
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
            lightProbe: sky
            probeExposure: root.evenLight ? 0.35 : 1.0
            vignetteEnabled: true
            vignetteStrength: 10
            effects: [celShadePost]
        }
        Effect {
            id: celShadePost
            passes: Pass {
                shaders: Shader {
                    stage: Shader.Fragment
                    shader: "shaders/celshade_post.frag"
                }
            }
        }

        // Where the 3x3's 430 puts a cube of 3 x 36 at the same share of the view.
        PerspectiveCamera {
            id: camera
            position: Qt.vector3d(0, 0, root.cubeSize * 4.0)
            fieldOfView: 26
            clipNear: 1
            clipFar: root.cubeSize * 20
        }

        // Qt Quick 3D draws four directional lights at most (see Cube3DView).
        DirectionalLight {
            visible: !root.evenLight
            eulerRotation: Qt.vector3d(-35, -35, 0)
            brightness: 1.1
        }
        DirectionalLight {
            eulerRotation: Qt.vector3d(40, 150, 0)
            brightness: 0.35
        }
        DirectionalLight {
            visible: root.shaderStyle === 0 && !root.evenLight
            eulerRotation: Qt.vector3d(-60, 20, 0)
            brightness: 1.6
        }
        DirectionalLight {
            visible: root.evenLight
            eulerRotation: Qt.vector3d(0, 0, 0)
            brightness: 0.75
            ambientColor: Qt.rgba(0.22, 0.22, 0.22, 1)
        }
        DirectionalLight {
            visible: root.evenLight
            eulerRotation: Qt.vector3d(55, 25, 0)
            brightness: 0.5
        }
        DirectionalLight {
            visible: root.evenLight
            eulerRotation: Qt.vector3d(-50, 40, 0)
            brightness: 0.45
        }

        PrincipledMaterial {
            id: plastic
            vertexColorsEnabled: true
            baseColor: "white"
            metalness: 0.0
            roughness: root.evenLight ? 0.6 : 0.38
            specularAmount: root.evenLight ? 0.25 : 0.6
            specularTint: 0.1
            clearcoatAmount: root.evenLight ? 0.08 : 0.3
            clearcoatRoughnessAmount: 0.12
        }
        // What the layers in hand leave behind, when it is asked to sit back.
        PrincipledMaterial {
            id: dimmed
            vertexColorsEnabled: true
            baseColor: Qt.rgba(1 - root.guideDim, 1 - root.guideDim, 1 - root.guideDim, 1)
            metalness: 0.0
            roughness: root.evenLight ? 0.6 : 0.38
            specularAmount: root.evenLight ? 0.25 : 0.6
            specularTint: 0.1
        }

        // Style 1: Instructional Glow (toon_mesh.frag, as toon.frag on the 3x3).
        // One for the cube and what the layers leave, which sits back when
        // guideDim asks; one for the layers in hand, which glow.
        CustomMaterial {
            id: glowRest
            shadingMode: CustomMaterial.Shaded
            fragmentShader: "shaders/toon_mesh.frag"
            property color uBodyColor: cubeModel.body
            property color uRimColor: Theme.accent
            property real uRimPower: 2.2
            property real uGlowStrength: 0.0
            property real uTint: root.held !== null ? 1 - root.guideDim : 1
        }
        CustomMaterial {
            id: glowLifted
            shadingMode: CustomMaterial.Shaded
            fragmentShader: "shaders/toon_mesh.frag"
            property color uBodyColor: cubeModel.body
            property color uRimColor: Theme.accent
            property real uRimPower: 2.2
            property real uGlowStrength: 1.0
            property real uTint: 1.0
        }
        // Style 2: Borderlands Cel-Shading (cel_mesh.frag, as cel.frag), and the
        // ink round it: the mesh again, a hair larger and drawn inside out, so
        // that only the rim of its back shows past the real one.
        CustomMaterial {
            id: celRest
            shadingMode: CustomMaterial.Shaded
            fragmentShader: "shaders/cel_mesh.frag"
            property real uTint: root.held !== null ? 1 - root.guideDim : 1
        }
        CustomMaterial {
            id: celLifted
            shadingMode: CustomMaterial.Shaded
            fragmentShader: "shaders/cel_mesh.frag"
            property real uTint: 1.0
        }
        PrincipledMaterial {
            id: ink
            baseColor: "black"
            lighting: PrincipledMaterial.NoLighting
            cullMode: Material.FrontFaceCulling
        }
        // The 3x3's outline stands half a unit of its 36 off each cubie; here it
        // is the same distance off the whole cube.
        readonly property real inkScale: 1 + (0.5 / 36) / (root.cubeSize / 2)

        // The materials for the cube at rest and for what the layers leave, and
        // for the layers in hand, in the style in use.
        readonly property Material restMaterial: root.shaderStyle === 1 ? glowRest
            : root.shaderStyle === 2 ? celRest : plastic
        readonly property Material partMaterial: root.shaderStyle === 1 ? glowRest
            : root.shaderStyle === 2 ? celRest : (root.guideDim > 0 ? dimmed : plastic)
        readonly property Material liftedMaterial: root.shaderStyle === 1 ? glowLifted
            : root.shaderStyle === 2 ? celLifted : plastic

        // The three meshes, each drawn by the model that shows it and, in
        // Borderlands, by its ink as well.
        CubeNGeometry {
            id: wholeMesh
            model: cubeModel
            part: 0
            active: root.held === null
        }
        CubeNGeometry {
            id: restMesh
            model: cubeModel
            part: 1
            axis: root.held ? root.held.axis : 0
            first: root.held ? root.held.first : 0
            last: root.held ? root.held.last : 0
            active: root.held !== null
        }
        CubeNGeometry {
            id: liftedMesh
            model: cubeModel
            part: 2
            axis: root.held ? root.held.axis : 0
            first: root.held ? root.held.first : 0
            last: root.held ? root.held.last : 0
            active: root.held !== null
        }

        // The viewpoint: the tip towards the viewer outside the turntable spin,
        // so the cube's uprights stay upright on screen however far round it is.
        Node {
            id: tiltNode
            eulerRotation: Qt.vector3d(root.shownPitch + root.dragPitch, 0, 0)

            Node {
                id: spinNode
                eulerRotation: Qt.vector3d(0, root.shownYaw + root.dragYaw, 0)

                Node {
                    id: cubeRoot
                    eulerRotation: root.homeRotation

                    // How the cube itself is turned.
                    Node {
                        id: holdNode
                        rotation: root.holdRotation
                        Behavior on rotation {
                            QuaternionAnimation { id: holdSwing; duration: root.viewDuration; type: QuaternionAnimation.Slerp }
                        }

                        // At rest: the whole cube, one mesh.
                        Model {
                            visible: root.held === null
                            materials: view3d.restMaterial
                            geometry: wholeMesh
                        }
                        Loader3D {
                            active: root.shaderStyle === 2 && root.held === null
                            sourceComponent: Model {
                                scale: Qt.vector3d(view3d.inkScale, view3d.inkScale, view3d.inkScale)
                                materials: ink
                                geometry: wholeMesh
                            }
                        }
                        // In hand: everything the layers leave...
                        Model {
                            visible: root.held !== null
                            materials: view3d.partMaterial
                            geometry: restMesh
                        }
                        Loader3D {
                            active: root.shaderStyle === 2 && root.held !== null
                            sourceComponent: Model {
                                scale: Qt.vector3d(view3d.inkScale, view3d.inkScale, view3d.inkScale)
                                materials: ink
                                geometry: restMesh
                            }
                        }
                        // ...and the layers, on the pivot they turn about.
                        Node {
                            id: pivot
                            property vector3d axis: Qt.vector3d(0, 1, 0)
                            visible: root.held !== null
                            rotation: Quaternion.fromAxisAndAngle(axis, root.turnAngle)

                            Model {
                                visible: root.held !== null
                                materials: view3d.liftedMaterial
                                geometry: liftedMesh
                            }
                            Loader3D {
                                active: root.shaderStyle === 2 && root.held !== null
                                sourceComponent: Model {
                                    scale: Qt.vector3d(view3d.inkScale, view3d.inkScale, view3d.inkScale)
                                    materials: ink
                                    geometry: liftedMesh
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    StatsOverlay { active: root.showStats }

    NumberAnimation {
        id: playAnim
        target: root
        property: "turnAngle"
        easing.type: Easing.InOutQuad
        onFinished: root.endPlay()
    }

    // The nudge for a quarter turn: a breath, out with a little kick, a beat held
    // there, back, and a rest before the next.
    SequentialAnimation {
        id: nudgeOnce
        loops: Animation.Infinite
        PauseAnimation { duration: 300 }
        NumberAnimation {
            target: root; property: "turnAngle"
            to: root.nudgeSign * root.nudgeAngle
            duration: 170; easing.type: Easing.OutBack; easing.overshoot: 1.6
        }
        PauseAnimation { duration: 190 }
        NumberAnimation { target: root; property: "turnAngle"; to: 0; duration: 260; easing.type: Easing.InOutQuad }
        PauseAnimation { duration: 160 }
    }

    // The nudge for a half turn: two kicks, one after the other, then back.
    SequentialAnimation {
        id: nudgeTwice
        loops: Animation.Infinite
        PauseAnimation { duration: 300 }
        NumberAnimation {
            target: root; property: "turnAngle"
            to: root.nudgeSign * root.nudgeAngle
            duration: 160; easing.type: Easing.OutBack; easing.overshoot: 1.6
        }
        PauseAnimation { duration: 120 }
        NumberAnimation {
            target: root; property: "turnAngle"
            to: root.nudgeSign * root.nudgeAngle * 2
            duration: 160; easing.type: Easing.OutBack; easing.overshoot: 1.6
        }
        PauseAnimation { duration: 220 }
        NumberAnimation { target: root; property: "turnAngle"; to: 0; duration: 340; easing.type: Easing.InOutQuad }
        PauseAnimation { duration: 160 }
    }

    DragHandler {
        id: dragHandler
        target: null
        enabled: root.interactive
        property real lastX: 0
        property real lastY: 0
        onActiveChanged: {
            if (active) {
                springAnim.stop();
                viewSpring.stop();
                lastX = centroid.position.x;
                lastY = centroid.position.y;
            } else if (root.dragTurnsView) {
                root.releaseView();
            } else {
                springAnim.start();
            }
        }
        onCentroidChanged: {
            if (!active)
                return;
            var dx = centroid.position.x - lastX;
            var dy = centroid.position.y - lastY;
            if (root.dragTurnsView) {
                // The cube follows the hand: dragged right it turns to the right,
                // dragged down its top comes towards the viewer. It stops short of
                // being seen from straight above or below.
                root.dragYaw = root.dragYaw + dx * 0.5;
                root.dragPitch = Math.max(-85 - root.shownPitch,
                                          Math.min(85 - root.shownPitch, root.dragPitch + dy * 0.5));
            } else {
                cubeRoot.eulerRotation.y = cubeRoot.eulerRotation.y + dx * 0.4;
                cubeRoot.eulerRotation.x = Math.max(-85, Math.min(85, cubeRoot.eulerRotation.x - dy * 0.4));
            }
            lastX = centroid.position.x;
            lastY = centroid.position.y;
        }
    }

    // A dragged view let go: back to the viewpoint the screen asked for.
    ParallelAnimation {
        id: viewSpring
        NumberAnimation { target: root; property: "dragYaw"; to: 0; duration: 550; easing.type: Easing.OutBack }
        NumberAnimation { target: root; property: "dragPitch"; to: 0; duration: 550; easing.type: Easing.OutBack }
    }

    // Eases back to the presented orientation once the user lets go.
    ParallelAnimation {
        id: springAnim
        NumberAnimation { target: cubeRoot; property: "eulerRotation.x"; to: root.homeRotation.x; duration: 550; easing.type: Easing.OutBack }
        NumberAnimation { target: cubeRoot; property: "eulerRotation.y"; to: root.homeRotation.y; duration: 550; easing.type: Easing.OutBack }
    }

    Timer {
        interval: 16
        repeat: true
        running: root.autoRotate && !dragHandler.active && !springAnim.running
        onTriggered: cubeRoot.eulerRotation.y = cubeRoot.eulerRotation.y + root.autoRotateSpeed
    }

    Component.onCompleted: scheduleGuide()
}
