#pragma once

#include <QStringList>

#include <thread>

namespace cube::app::kit {

// The families Theme.qml names for the interface (Theme.fontHeading and
// Theme.fontMono): the ones the first text on screen asks Qt's font database
// for. A test holds this list to Theme.qml so that it cannot drift.
[[nodiscard]] QStringList themeFamilies();

// Asks Qt for `families` as the screens do - each in the weights they set it
// in - and measures a line of text in each, so that the font database has
// found them, or found that this machine has none, and made the engines.
// Blocks for as long as that takes; FontWarmup runs it on a thread.
void warmFonts(const QStringList& families);

// Does warmFonts() on a thread of its own while the rest of start-up goes on,
// and waits for it in its destructor.
//
// The first text the QML engine lays out used to wait for this, on the GUI
// thread: a family Qt does not have (Archivo and IBM Plex Mono are named and
// not installed on most machines) is looked for in every font the system has
// before Qt settles on its default, about a third of a second the first time.
// The lookup is the font database's, shared by every thread, so done once on
// another thread it is answered from memory on the GUI thread; and because it
// is the same lookup, the text is set in the same face it always was.
//
// Needs a QGuiApplication, and so must be destroyed before it.
class FontWarmup {
public:
    // `enabled` false (a platform with nothing to search: PlatformInfo) makes
    // this do nothing.
    FontWarmup(QStringList families, bool enabled);
    ~FontWarmup();

    FontWarmup(const FontWarmup&) = delete;
    FontWarmup& operator=(const FontWarmup&) = delete;
    FontWarmup(FontWarmup&&) = delete;
    FontWarmup& operator=(FontWarmup&&) = delete;

    // Blocks until the thread has finished. Safe to call more than once.
    void wait();

private:
    std::thread worker_;
};

}  // namespace cube::app::kit
