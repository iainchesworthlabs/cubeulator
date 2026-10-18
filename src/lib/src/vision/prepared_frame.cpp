#include <cube/vision/prepared_frame.hpp>

#include <cube/profiling.hpp>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace cube::vision {

namespace {

// The readers work at this width: blob filters and the blur are sized for
// it, and a side read at a quarter of this still resolves (measured on
// recorded scans down to 320 wide).
constexpr int kWorkingWidth = 640;

// A cube of more stickers across the same span has less of the frame to a
// sticker, so the frame is worked at more width for it: the stickers of a 5x5
// held as a 3x3 is are three fifths the size, and brought down to 640 they are
// as small as a 3x3's would be at 380 (measured on drawn cubes, docs/nxn-app-plan.md
// gate S0: the nearest a cube may be to unreadable halves in a warm lamp's light
// at 1280). It costs the pixels it adds: a frame wider than the working width
// is read at the width asked for, and one narrower is read as it is.
[[nodiscard]] int working_width_for(int size) {
    if (size <= 3) {
        return kWorkingWidth;
    }
    return size <= 5 ? kWorkingWidth * 3 / 2 : kWorkingWidth * 2;
}

// A pixel is bright if it is within kThresholdOffset of the mean of a block
// this fraction of the frame's height across; see narrow_contours for why the
// second look's is narrower.
constexpr double kBlockFraction = 0.12;
constexpr double kNarrowBlockFraction = 0.06;
constexpr double kThresholdOffset = 8.0;

// How much smaller than a 3x3's the stickers of a cube of `size` are, when the
// cube spans as much of the frame: 3 / N for N above 3.
[[nodiscard]] double block_scale(int size) {
    return size > 3 ? 3.0 / static_cast<double>(size) : 1.0;
}

// The block, in pixels, that `fraction` of the frame comes to: odd, and never
// smaller than three.
[[nodiscard]] int block_pixels(int rows, int cols, double fraction) {
    return std::max(
        3, static_cast<int>(
               static_cast<unsigned>(static_cast<double>(std::min(rows, cols)) * fraction) | 1U));
}

// Threshold and trace. `mask`, when it is not empty, is a CV_8U image the same
// size as `brightness` whose non-zero pixels are taken out of the bright region
// before it is traced: that is how the glare look keeps a highlight from
// bridging two stickers.
[[nodiscard]] std::vector<std::vector<cv::Point>> traced(const cv::Mat& brightness, int block,
                                                         const cv::Mat& mask) {
    cv::Mat binary;
    cv::adaptiveThreshold(brightness, binary, 255.0, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY,
                          std::max(block, 3), kThresholdOffset);
    if (!mask.empty()) {
        cv::Mat kept;
        cv::bitwise_not(mask, kept);
        cv::bitwise_and(binary, kept, binary);
    }
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, cv::Mat::ones(3, 3, CV_8U));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    return contours;
}

[[nodiscard]] std::vector<std::vector<cv::Point>> outlines(const cv::Mat& brightness,
                                                           double block_fraction) {
    return traced(brightness, block_pixels(brightness.rows, brightness.cols, block_fraction),
                  cv::Mat{});
}

// The channels of `working`, split once per look that needs more than the
// brightest of them.
struct Channels {
    cv::Mat value;
    cv::Mat chroma;
};

[[nodiscard]] Channels channels_of(const cv::Mat& working) {
    std::array<cv::Mat, 3> split;
    cv::split(working, split.data());
    Channels channels;
    channels.value = cv::max(cv::max(split.at(0), split.at(1)), split.at(2));
    const cv::Mat lowest = cv::min(cv::min(split.at(0), split.at(1)), split.at(2));
    cv::subtract(channels.value, lowest, channels.chroma);
    return channels;
}

// Pixels that are clipped and nearly colourless: a specular highlight, not a
// sticker. 252 rather than 255 because a highlight that has not quite clipped
// still washes out the plastic beside it, and a chroma of 12 because a white
// sticker carries more than that even by a lamp (its b* is pushed by the cast)
// while a highlight carries almost none. Deliberately conservative: this look
// only ever adds a read, so a missed highlight costs what the reader already
// pays and a wrongly removed white sticker would add one it should not.
constexpr int kGlareValue = 252;
constexpr int kGlareChroma = 12;

[[nodiscard]] cv::Mat glare_mask(const Channels& channels) {
    cv::Mat bright;
    cv::Mat grey;
    cv::compare(channels.value, kGlareValue, bright, cv::CMP_GE);
    cv::compare(channels.chroma, kGlareChroma, grey, cv::CMP_LT);
    cv::Mat mask;
    cv::bitwise_and(bright, grey, mask);
    cv::dilate(mask, mask, cv::Mat::ones(3, 3, CV_8U));
    return mask;
}

// The chroma the chroma look thresholds: colourfulness relative to the pixel's
// own brightness, so a dim sticker still reads as coloured, scaled to 0..255.
[[nodiscard]] cv::Mat chroma_image(const Channels& channels) {
    cv::Mat floor_value;
    cv::max(channels.value, cv::Scalar(1.0), floor_value);
    cv::Mat scaled;
    cv::divide(channels.chroma, floor_value, scaled, 255.0);
    return scaled;
}

}  // namespace

std::optional<PreparedFrame> prepare_frame(const cv::Mat& bgr, int size) {
    CUBE_ZONE_SCOPED_N("vision.prepare_frame");
    if (bgr.empty() || bgr.type() != CV_8UC3) {
        return std::nullopt;
    }

    PreparedFrame prepared;
    prepared.working = bgr;
    const int working_width = working_width_for(size);
    if (bgr.cols > working_width) {
        prepared.scale = static_cast<float>(bgr.cols) / static_cast<float>(working_width);
        const int height =
            static_cast<int>(std::lround(static_cast<float>(bgr.rows) / prepared.scale));
        cv::resize(bgr, prepared.working, cv::Size(working_width, height), 0.0, 0.0,
                   cv::INTER_AREA);
    }
    const cv::Mat& working = prepared.working;

    // Value (the brightest channel) is used so a saturated blue or red
    // sticker still stands clear of black plastic.
    std::array<cv::Mat, 3> channels;
    cv::split(working, channels.data());
    prepared.brightness = cv::max(cv::max(channels.at(0), channels.at(1)), channels.at(2));
    cv::GaussianBlur(prepared.brightness, prepared.brightness, cv::Size(5, 5), 0.0);

    prepared.contours = outlines(prepared.brightness, kBlockFraction * block_scale(size));
    return prepared;
}

std::vector<std::vector<cv::Point>> narrow_contours(const cv::Mat& brightness, int size) {
    CUBE_ZONE_SCOPED_N("vision.narrow_contours");
    if (brightness.empty() || brightness.type() != CV_8UC1) {
        return {};
    }
    return outlines(brightness, kNarrowBlockFraction * block_scale(size));
}

std::vector<std::vector<cv::Point>> contours_at_pitch(const cv::Mat& brightness, float pitch_px) {
    CUBE_ZONE_SCOPED_N("vision.contours_at_pitch");
    if (brightness.empty() || brightness.type() != CV_8UC1 || !(pitch_px > 0.0F)) {
        return {};
    }
    // A block about two pitches across: wide enough to carry a sticker and the
    // plastic around it, narrow enough not to reach the next one. Odd, as
    // adaptiveThreshold wants, and at least three.
    const int block =
        std::max(3, static_cast<int>(static_cast<unsigned>(std::lround(pitch_px * 2.0F)) | 1U));
    return traced(brightness, block, cv::Mat{});
}

std::vector<std::vector<cv::Point>> contours_without_glare(const cv::Mat& working, int size) {
    CUBE_ZONE_SCOPED_N("vision.contours_without_glare");
    if (working.empty() || working.type() != CV_8UC3) {
        return {};
    }
    const Channels channels = channels_of(working);
    cv::Mat brightness = channels.value;
    cv::GaussianBlur(brightness, brightness, cv::Size(5, 5), 0.0);
    return traced(
        brightness,
        block_pixels(brightness.rows, brightness.cols, kBlockFraction * block_scale(size)),
        glare_mask(channels));
}

std::vector<std::vector<cv::Point>> chroma_contours(const cv::Mat& working, int size) {
    CUBE_ZONE_SCOPED_N("vision.chroma_contours");
    if (working.empty() || working.type() != CV_8UC3) {
        return {};
    }
    const Channels channels = channels_of(working);
    cv::Mat chroma = chroma_image(channels);
    cv::GaussianBlur(chroma, chroma, cv::Size(5, 5), 0.0);
    return traced(chroma,
                  block_pixels(chroma.rows, chroma.cols, kBlockFraction * block_scale(size)),
                  cv::Mat{});
}

}  // namespace cube::vision
