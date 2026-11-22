# How it works

A short account of what Cubeulator does between your camera and the 3D cube, for anyone who is curious. The code is open,
and the detail is in the [developer documentation](https://github.com/iainchesworthlabs/cubeulator/tree/develop/docs) on
GitHub.

## Reading the cube

Cubeulator reads one side at a time, from whichever side is held square to the camera. It finds the nine stickers by the dark
plastic around them, measures the colour of each, and groups the reads into sides.

It does not need to know which side is which by where it is. The centre sticker of a side never moves on a real cube, so six
different centres name six different sides, and the colours around each centre say how they sit. Once six sides are in,
Cubeulator tries each of them in every way up (four to the sixth power ways) and keeps only the arrangements that are a cube
that can exist. Usually exactly one is left.

That check is also what tells red from orange. Under a warm bulb their hue is only a few degrees apart, so a name read from
the colour alone is a guess; the cube settles it, because only one naming makes a cube that can exist.

About one scrambled cube in eighty looks the same from its six flat sides in two different ways. For those, Cubeulator asks you
to turn the cube so the camera sees two or three sides at once, as when a cube is held on a corner. Seeing a corner settles which
cube yours is. See [Scanning a cube](scan.md).

The camera picture is shrunk to 640 pixels across, read in memory and dropped. Nothing is saved or sent anywhere
([Privacy](../privacy.md)).

## Three methods

The same cube can be solved three ways. They are three separate engines, not one engine with a setting.

| Method | What it is | Turns, on average |
| --- | --- | --- |
| **Beginner** | Layer by layer: the cross, the first-layer corners, the middle edges, then the last layer in two looks | about 100 |
| **Intermediate** | CFOP, the method fast solvers use: the cross, the first two layers in pairs, then the last layer in one look each for orientation and permutation | about 50 |
| **Advanced** | Kociemba's two-phase search, which looks for the fewest turns | about 20 |

The figures are averages over many random scrambles on a desktop. A solution is worked out in about a second or less. Beginner
is the default, and you can change method at any moment: the cube is solved from where it stands now.

## Following your turns

On the solve screen the camera stays on. If the side it sees matches the cube *after* the turn that was asked for, the solution
steps on by itself. It never steps on while the cube it sees and the solution disagree, and a turn it cannot see (a layer
behind the front one, or a turn that changes almost nothing on the visible side) is a turn you confirm with **Next turn**.
See [Following the solution](solve.md#the-camera-follows-you).

## Bigger cubes

A cube from 2×2 to 5×5 is first made to look like a 3×3: its centres are gathered into blocks, then its edges are paired up,
and then what is left is solved as a 3×3 with the method you chose. See [Another size of cube](other-sizes.md).

## Where to read more

On GitHub: the [vision pipeline](https://github.com/iainchesworthlabs/cubeulator/blob/develop/docs/vision-pipeline.md),
[solve methods](https://github.com/iainchesworthlabs/cubeulator/blob/develop/docs/solver-methods.md), the
[solver for bigger cubes](https://github.com/iainchesworthlabs/cubeulator/blob/develop/docs/nxn-solver.md) and the
[architecture](https://github.com/iainchesworthlabs/cubeulator/blob/develop/docs/architecture.md).
