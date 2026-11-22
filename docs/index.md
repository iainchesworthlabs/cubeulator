---
template: marketing.html
title: Cubeulator
title_tag: "Cubeulator: a camera-driven Rubik's Cube solver"
description: "Hold each side of a scrambled Rubik's Cube up to the camera. Cubeulator reads the cube, solves it and shows you the turns, one at a time, on a 3D cube. Nothing is uploaded."
---
<section class="icl-hero icl-hero--app"><div class="icl-wrap"><div class="icl-hero__grid">
<div>
<img class="icl-hero__icon" src="assets/img/icon.svg" width="80" height="80" alt="The Cubeulator icon: a cube drawn in ink on pale paper, with its top layer turned part-way round.">
<h1>Cubeulator</h1>
<p class="icl-lead">Hold each side of a scrambled Rubik's Cube up to your camera. Cubeulator reads the whole cube, works out a solution and shows you the turns, one at a time, on a 3D cube.</p>
<div class="icl-actions" id="get">
<a class="icl-button" href="#download">Get Cubeulator</a>
<a class="icl-button icl-button--ghost" href="app/">Try it in your browser</a>
</div>
<ul class="cu-platforms" aria-label="Where Cubeulator runs">
<li><strong>Windows, macOS and Linux</strong><span>Download from GitHub, pre-release</span></li>
<li><strong>In a browser</strong><span>Nothing to install</span></li>
<li><strong>iPhone</strong><span>TestFlight, for invited testers only</span></li>
</ul>
<p class="icl-hero__meta">No account &middot; Nothing uploaded &middot; Works offline on a computer &middot; English, with eight more languages in draft</p>
</div>
<div class="cu-shots">
<figure class="cu-window"><picture><source srcset="guide/img/solve-middle-wide-dark.webp" media="(prefers-color-scheme: dark)"><img src="guide/img/solve-middle-wide-light.webp" width="900" height="640" alt="The solve screen on a computer: a 3D cube with the layer to turn nudged, the line Hold yellow on top, green facing you, the five stages of the Beginner method listed on the right and a Next turn button."></picture></figure>
<figure class="icl-device"><img src="guide/img/solve-middle-narrow-light.webp" width="390" height="844" alt="The same screen in the narrow layout that a phone gets: the 3D cube above, then the method buttons, the current stage and the Previous and Next turn buttons."></figure>
</div>
</div></div></section>

<section class="icl-section" id="how"><div class="icl-wrap">
<h2>Scan, solve, follow</h2>
<ol class="cu-steps">
<li><h3>Scan</h3><p>Show each side of the cube to the camera, in any order and either way up. Each side is recognised by the colour of its centre, which never moves. Six sides that make exactly one cube that can exist are a scan.</p></li>
<li><h3>Solve</h3><p>Pick how you want it solved: <em>Beginner</em> (layer by layer, about 100 turns), <em>Intermediate</em> (the method fast solvers use, about 50) or <em>Advanced</em> (the fewest turns, about 20).</p></li>
<li><h3>Follow</h3><p>The layer to turn nudges the way it has to go on a 3D cube, and the screen says it in words as well. Turn yours, and press Next. The camera stays on, so it can see your turn and step on by itself.</p></li>
</ol>
<p><a href="guide/">Read the user guide</a>, or see <a href="guide/how-it-works/">how it works</a> in a little more detail.</p>
</div></section>

<section class="icl-section icl-section--tint" id="features"><div class="icl-wrap">
<h2>What it does</h2>
<ul class="icl-points">
<li><h3>Reads a real cube</h3><p>It finds the nine stickers of each side and names them from what the camera sees. A sticker it read wrongly is put right by hand on the colours screen, without scanning again.</p></li>
<li><h3>Three ways to solve it</h3><p>Change method at any moment, even half-way. The cube is solved from where it stands now.</p></li>
<li><h3>A 2&times;2 up to a 5&times;5</h3><p>Scan it, or type the scramble or the colours in, and follow it on a 3D cube drawn at its size.</p></li>
<li><h3>Follows your turns</h3><p>Make the turn asked for in view of the camera and the solution steps on by itself. It never moves on while the cube and the solution disagree.</p></li>
<li><h3>Private by design</h3><p>The camera picture is read on your device and dropped. There is no account, no telemetry and no update check, and the app's own code makes no network calls.</p></li>
<li><h3>Free software</h3><p>The source is on GitHub under the GNU General Public License. Each release has signatures, build attestations and bills of materials.</p></li>
</ul>
<p><a href="features/">All the features</a></p>
</div></section>

<section class="icl-section" id="screens"><div class="icl-wrap">
<h2>A look at it</h2>
<p class="cu-note">These are pictures of the app itself, taken by its own test suite. The camera picture in them is a drawn frame, so nobody is in them.</p>
<div class="cu-gallery cu-gallery--wide">
<figure><div class="cu-window"><img src="guide/img/scan-reading-wide-light.webp" width="900" height="640" loading="lazy" alt="The scan screen with one side in view: a dot on each of its nine stickers, and the first of six chips along the bottom filling."></div><figcaption>Scanning. A dot sits on each sticker it found, and a chip fills as a side is read.</figcaption></figure>
<figure><div class="cu-window"><picture><source srcset="guide/img/solution-middle-wide-dark.webp" media="(prefers-color-scheme: dark)"><img src="guide/img/solution-middle-wide-light.webp" width="900" height="640" loading="lazy" alt="The solution screen: the stages and every turn in a list, grouped by stage, beside a 3D cube with Previous, Play and Next buttons."></picture></div><figcaption>The whole solution, grouped by stage, to look back over.</figcaption></figure>
<figure><div class="cu-window"><picture><source srcset="guide/img/solve-big-middle-wide-dark.webp" media="(prefers-color-scheme: dark)"><img src="guide/img/solve-big-middle-wide-light.webp" width="900" height="640" loading="lazy" alt="The solve screen for a 5x5 cube: a layer inside the cube is nudged, and the move line names it as layer 2, quarter turn."></picture></div><figcaption>A 5&times;5. A layer inside the cube is named from the side it is counted from.</figcaption></figure>
<figure><div class="cu-window"><img src="guide/img/colours-wide-light.webp" width="900" height="640" loading="lazy" alt="The colours screen: the top and front sides of a 4x4 painted, the cube drawn beside them and a count of the stickers still to paint."></div><figcaption>Putting right a sticker the camera misread, or typing a cube in.</figcaption></figure>
</div>
<p>The start screen, <a href="guide/scan/">scanning</a> and <a href="guide/settings/">settings</a> are in the <a href="guide/">user guide</a>, in light and dark.</p>
</div></section>

<section class="icl-section icl-section--tint" id="download"><div class="icl-wrap">
<h2>Get Cubeulator</h2>
<ul class="cu-platform-cards">
<li>
<p class="cu-state">Pre-release</p>
<h3>Windows, macOS and Linux</h3>
<p>An installer for each, on GitHub: Windows 10 or later (64-bit), macOS 14.4 or later on Apple silicon, and Debian, Ubuntu or Fedora. They are not signed with a Windows or Apple certificate yet, so each system asks before it runs one. Every file can be <a href="guide/verify/">checked</a>.</p>
<a class="icl-button" href="https://github.com/iainchesworthlabs/cubeulator/releases">Packages on GitHub</a>
</li>
<li>
<p class="cu-state">Nothing to install</p>
<h3>In a browser</h3>
<p>The same app, as WebAssembly, in a current Chrome, Edge, Firefox or Safari. It needs a camera and a secure page, which this is. It runs on your device; nothing is sent anywhere.</p>
<a class="icl-button" href="app/">Open Cubeulator</a>
</li>
<li>
<p class="cu-state">Invited testers</p>
<h3>iPhone</h3>
<p>The iPhone app goes to TestFlight for testers the author has invited. It is not in the App Store, and there is no public test link. It needs iOS 18 or later. Everyone else can use the browser version on a phone.</p>
<a class="icl-button icl-button--ghost" href="guide/iphone/">About the iPhone build</a>
</li>
</ul>
<p>Android: the app builds for Android, but its camera is not written yet, so it cannot scan and is not offered.</p>
<p>It is pre-release software. The scan has been checked on recorded sessions and on a webcam, not yet across many cameras, rooms and cubes. <a href="whats-new/">What's new</a> says what each release changed and what is not yet proven.</p>
</div></section>
