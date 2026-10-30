// The index.csv of a recording (cube::vision::recording::read_index and parse_index_line) on
// any bytes: a recording cut off mid-write, or a file that was never one.

#include <cube/vision/frame_recording.hpp>

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string text(reinterpret_cast<const char*>(data), size);
    std::istringstream stream(text);
    (void)cube::vision::recording::read_index(stream);
    (void)cube::vision::recording::parse_index_line(std::string_view(text));
    return 0;
}
