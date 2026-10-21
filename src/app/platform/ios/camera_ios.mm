// The iPhone camera: AVFoundation capture behind interfaces/camera.hpp.
//
// A capture session feeds a video data output, and each frame its callback
// receives is turned into an upright BGR cv::Mat (platform/detail/
// camera_frame.hpp, which is tested with drawn frames on every platform) and put
// in a latest-wins mailbox that the app polls, like the desktop backends.
//
//  * Which camera. The back camera is listed first, so that it is the default
//    (a phone's rear lens is the one pointed at the cube); the front camera is
//    listed second, and the scan screen offers a switch when there are two
//    (platform_info_ios.cpp). DeviceInfo::facing says which is which, which is
//    what decides the preview's mirror: the mirror is display only and never
//    applied here, so the scan pipeline reads the scene as it is.
//  * Orientation. The sensor sits in landscape. AVCaptureDeviceRotationCoordinator
//    reports, for the way the phone is held now, the clockwise turn that makes a
//    frame level with the horizon; it is read for every frame and applied by the
//    adapter (a back camera held upright reports 90).
//  * Permission. Qt's QCameraPermission asks the user before the app gets here
//    (src/app/qt/CameraPermission.cpp); this file only refuses to open a camera
//    it is not allowed, and asks itself when called off the main thread with the
//    question still open.
//  * Photometry. The lock holds the camera's exposure and white balance where its
//    automatics settled (the interface explains why a scan needs that). It
//    reports what the device said, per control, and says plainly that it has no
//    brightness or saturation control.
//
// Nothing here has run on a phone yet: docs/ios-install.md has the checklist.

#include "interfaces/camera.hpp"

#include "platform/detail/camera_frame.hpp"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace cube::platform::camera {

namespace {

// Frames the automatics are given to settle after a lock is asked for, as on the
// desktop backends (a third of a second at 30 fps).
constexpr int kPhotometrySettleFrames = 20;

detail::FrameMailbox& mailbox() {
    static detail::FrameMailbox instance;
    return instance;
}

std::atomic<bool>& capturing_flag() {
    static std::atomic<bool> instance{false};
    return instance;
}

// The lock request and its outcome, handed between the app's thread and the
// capture queue. Held only around plain-data copies.
struct PhotometryState {
    std::mutex mutex;
    std::optional<PhotometryLockRequest> pending;
    bool restore_pending = false;
    int settle_frames_remaining = 0;
    std::optional<PhotometryLockReport> report;
    bool proven = false;
};

PhotometryState& photometry() {
    static PhotometryState instance;
    return instance;
}

Facing facing_of(AVCaptureDevicePosition position) {
    switch (position) {
        case AVCaptureDevicePositionFront:
            return Facing::front;
        case AVCaptureDevicePositionBack:
        case AVCaptureDevicePositionUnspecified:
            // The rear lens when the system does not say: it is the one that
            // looks at the cube, and unknown would get the selfie mirror.
            return Facing::rear;
    }
    return Facing::rear;
}

std::string to_std_string(NSString* text) {
    return text == nil ? std::string() : std::string([text UTF8String]);
}

// What one control became, judged by reading the device back.
ControlResult judge(PhotometryControl control, bool supported, bool held) {
    ControlResult result;
    result.control = control;
    if (!supported) {
        result.outcome = ControlOutcome::unsupported;
    } else {
        result.outcome = held ? ControlOutcome::applied : ControlOutcome::rejected;
        result.requested = 1.0;
        result.readback = held ? 1.0 : 0.0;
    }
    return result;
}

}  // namespace

}  // namespace cube::platform::camera

// The session, its delegate and the queue the frames arrive on.
@interface CubeCameraCapture : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property(nonatomic, strong) AVCaptureSession* session;
@property(nonatomic, strong) AVCaptureDevice* device;
@property(nonatomic, strong) AVCaptureDeviceRotationCoordinator* rotation;
@property(nonatomic, strong) dispatch_queue_t queue;
- (BOOL)startWithDevice:(AVCaptureDevice*)device;
- (void)stop;
- (void)restoreAutomatics;
@end

@implementation CubeCameraCapture

- (BOOL)startWithDevice:(AVCaptureDevice*)device {
    NSError* error = nil;
    AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
    if (input == nil) {
        return NO;
    }

    AVCaptureSession* session = [[AVCaptureSession alloc] init];
    if ([session canSetSessionPreset:AVCaptureSessionPreset1280x720]) {
        session.sessionPreset = AVCaptureSessionPreset1280x720;
    }
    if (![session canAddInput:input]) {
        return NO;
    }
    [session addInput:input];

    AVCaptureVideoDataOutput* output = [[AVCaptureVideoDataOutput alloc] init];
    id const pixel_format_key = CFBridgingRelease(CFRetain(kCVPixelBufferPixelFormatTypeKey));
    output.videoSettings = @{pixel_format_key : @(kCVPixelFormatType_32BGRA)};
    // A frame the app has not taken by the time the next arrives is dropped: the
    // scan wants the newest, not a queue.
    output.alwaysDiscardsLateVideoFrames = YES;
    self.queue = dispatch_queue_create("com.iainchesworthlabs.cubeulator.camera", DISPATCH_QUEUE_SERIAL);
    [output setSampleBufferDelegate:self queue:self.queue];
    if (![session canAddOutput:output]) {
        return NO;
    }
    [session addOutput:output];

    self.device = device;
    self.rotation = [[AVCaptureDeviceRotationCoordinator alloc] initWithDevice:device previewLayer:nil];
    self.session = session;
    // Blocks until the session runs, so start() is called off the main thread.
    [session startRunning];
    return session.isRunning;
}

- (void)stop {
    [self.session stopRunning];
    self.session = nil;
    self.rotation = nil;
    self.device = nil;
}

- (void)captureOutput:(AVCaptureOutput*)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection {
    (void)output;
    (void)connection;
    CVImageBufferRef pixels = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (pixels == nullptr) {
        return;
    }

    cv::Mat bgr;
    if (CVPixelBufferLockBaseAddress(pixels, kCVPixelBufferLock_ReadOnly) == kCVReturnSuccess) {
        const cube::platform::camera::detail::BgraBuffer buffer{
            .data = static_cast<const std::uint8_t*>(CVPixelBufferGetBaseAddress(pixels)),
            .width = static_cast<int>(CVPixelBufferGetWidth(pixels)),
            .height = static_cast<int>(CVPixelBufferGetHeight(pixels)),
            .stride_bytes = CVPixelBufferGetBytesPerRow(pixels),
        };
        const double clockwise = self.rotation != nil ? self.rotation.videoRotationAngleForHorizonLevelCapture : 90.0;
        bgr = cube::platform::camera::detail::bgr_from_bgra(buffer, clockwise);
        CVPixelBufferUnlockBaseAddress(pixels, kCVPixelBufferLock_ReadOnly);
    }
    if (!bgr.empty()) {
        cube::platform::camera::mailbox().publish(std::move(bgr));
    }
    [self servicePhotometry];
}

// Between frames, on the capture queue: the lock waits for the automatics to
// settle (counted in delivered frames), then holds them.
- (void)servicePhotometry {
    using namespace cube::platform::camera;
    PhotometryState& state = photometry();
    std::optional<PhotometryLockRequest> request;
    bool restore = false;
    {
        const std::scoped_lock lock(state.mutex);
        if (state.restore_pending) {
            state.restore_pending = false;
            restore = true;
        } else if (state.pending.has_value()) {
            if (state.settle_frames_remaining > 0) {
                --state.settle_frames_remaining;
                return;
            }
            request = std::move(state.pending);
            state.pending.reset();
        }
    }
    if (restore) {
        [self restoreAutomatics];
        return;
    }
    if (!request.has_value() || self.device == nil) {
        return;
    }

    AVCaptureDevice* device = self.device;
    const bool exposure_supported = [device isExposureModeSupported:AVCaptureExposureModeLocked];
    const bool white_balance_supported = [device isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeLocked];
    NSError* error = nil;
    if ([device lockForConfiguration:&error]) {
        if (request->lock_exposure && exposure_supported) {
            device.exposureMode = AVCaptureExposureModeLocked;
        }
        if (request->lock_auto_white_balance && white_balance_supported) {
            device.whiteBalanceMode = AVCaptureWhiteBalanceModeLocked;
        }
        [device unlockForConfiguration];
    }

    PhotometryLockReport report;
    report.platform_supported = true;
    const auto set = [&report](const ControlResult& result) {
        report.controls.at(static_cast<std::size_t>(result.control)) = result;
    };
    set(request->lock_auto_white_balance
            ? judge(PhotometryControl::auto_white_balance, white_balance_supported,
                    device.whiteBalanceMode == AVCaptureWhiteBalanceModeLocked)
            : ControlResult{.control = PhotometryControl::auto_white_balance});
    set(request->lock_exposure
            ? judge(PhotometryControl::exposure, exposure_supported,
                    device.exposureMode == AVCaptureExposureModeLocked)
            : ControlResult{.control = PhotometryControl::exposure});
    // The camera has no brightness or saturation control to offer: said so,
    // rather than reported as locked.
    set(request->lock_brightness ? judge(PhotometryControl::brightness, false, false)
                                 : ControlResult{.control = PhotometryControl::brightness});
    set(request->lock_saturation ? judge(PhotometryControl::saturation, false, false)
                                 : ControlResult{.control = PhotometryControl::saturation});

    if (device.whiteBalanceMode == AVCaptureWhiteBalanceModeLocked) {
        const AVCaptureWhiteBalanceTemperatureAndTintValues values =
            [device temperatureAndTintValuesForDeviceWhiteBalanceGains:device.deviceWhiteBalanceGains];
        report.white_balance_kelvin = static_cast<double>(values.temperature);
    }

    const std::scoped_lock lock(state.mutex);
    state.proven = state.proven || report.exposure_locked();
    state.report = report;
}

- (void)restoreAutomatics {
    AVCaptureDevice* device = self.device;
    if (device == nil) {
        return;
    }
    NSError* error = nil;
    if ([device lockForConfiguration:&error]) {
        if ([device isExposureModeSupported:AVCaptureExposureModeContinuousAutoExposure]) {
            device.exposureMode = AVCaptureExposureModeContinuousAutoExposure;
        }
        if ([device isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance]) {
            device.whiteBalanceMode = AVCaptureWhiteBalanceModeContinuousAutoWhiteBalance;
        }
        [device unlockForConfiguration];
    }
}

@end

namespace cube::platform::camera {

namespace {

CubeCameraCapture* current_capture = nil;
std::mutex capture_mutex;

// Asks the system for the camera when the question is still open and the caller
// can wait for the answer without stalling the interface. Qt's permission has
// normally answered it already.
bool camera_allowed() {
    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo]) {
        case AVAuthorizationStatusAuthorized:
            return true;
        case AVAuthorizationStatusNotDetermined: {
            if ([NSThread isMainThread]) {
                return false;
            }
            dispatch_semaphore_t answered = dispatch_semaphore_create(0);
            __block bool granted = false;
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                                     completionHandler:^(BOOL ok) {
                                       granted = ok;
                                       dispatch_semaphore_signal(answered);
                                     }];
            dispatch_semaphore_wait(answered, DISPATCH_TIME_FOREVER);
            return granted;
        }
        case AVAuthorizationStatusDenied:
        case AVAuthorizationStatusRestricted:
            return false;
    }
    return false;
}

}  // namespace

Result<std::vector<DeviceInfo>> enumerate_devices() {
    @autoreleasepool {
        AVCaptureDeviceDiscoverySession* discovery = [AVCaptureDeviceDiscoverySession
            discoverySessionWithDeviceTypes:@[ AVCaptureDeviceTypeBuiltInWideAngleCamera ]
                                  mediaType:AVMediaTypeVideo
                                   position:AVCaptureDevicePositionUnspecified];

        std::vector<DeviceInfo> devices;
        for (AVCaptureDevice* device in discovery.devices) {
            devices.push_back(DeviceInfo{
                .identifier = to_std_string(device.uniqueID),
                .display_name = to_std_string(device.localizedName),
                .facing = facing_of(device.position),
            });
        }
        // The rear camera first: the controller selects the first one listed, and
        // the one that looks away from the person is the one for a cube.
        std::stable_sort(devices.begin(), devices.end(), [](const DeviceInfo& a, const DeviceInfo& b) {
            return a.facing == Facing::rear && b.facing != Facing::rear;
        });
        return devices;
    }
}

Result<void> start(const DeviceInfo& device) {
    @autoreleasepool {
        const std::scoped_lock lock(capture_mutex);
        if (current_capture != nil) {
            [current_capture stop];
            current_capture = nil;
            capturing_flag() = false;
        }
        if (!camera_allowed()) {
            return std::unexpected(CubeError::camera_start_failed);
        }
        AVCaptureDevice* chosen = [AVCaptureDevice
            deviceWithUniqueID:[NSString stringWithUTF8String:device.identifier.c_str()]];
        if (chosen == nil) {
            return std::unexpected(CubeError::camera_not_found);
        }

        mailbox().clear();
        CubeCameraCapture* capture = [[CubeCameraCapture alloc] init];
        if (![capture startWithDevice:chosen]) {
            [capture stop];
            return std::unexpected(CubeError::camera_start_failed);
        }
        current_capture = capture;
        capturing_flag() = true;
        return {};
    }
}

void stop() {
    @autoreleasepool {
        const std::scoped_lock lock(capture_mutex);
        if (current_capture != nil) {
            [current_capture stop];
            current_capture = nil;
        }
        capturing_flag() = false;
        mailbox().clear();
        PhotometryState& state = photometry();
        const std::scoped_lock photometry_lock(state.mutex);
        state.pending.reset();
        state.restore_pending = false;
    }
}

// The settings page of this app, where a camera that was refused is turned back
// on: the question cannot be asked twice.
void open_permission_settings() {
    dispatch_async(dispatch_get_main_queue(), ^{
      NSURL* url = [NSURL URLWithString:UIApplicationOpenSettingsURLString];
      if (url != nil) {
          [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
      }
    });
}

std::optional<TimedFrame> try_pop_latest_frame() {
    return mailbox().try_pop();
}

bool is_capturing() noexcept {
    return capturing_flag().load();
}

// True once a lock has been read back as holding exposure on this camera, as in
// the browser: the answer is the open camera's, not the platform's.
bool photometry_control_supported() noexcept {
    PhotometryState& state = photometry();
    const std::scoped_lock lock(state.mutex);
    return state.proven;
}

Result<void> request_photometry_lock(const PhotometryLockRequest& request) {
    if (!is_capturing()) {
        return std::unexpected(CubeError::camera_control_unavailable);
    }
    PhotometryState& state = photometry();
    const std::scoped_lock lock(state.mutex);
    state.pending = request;
    state.restore_pending = false;
    state.settle_frames_remaining = kPhotometrySettleFrames;
    return {};
}

Result<void> request_photometry_restore() {
    if (!is_capturing()) {
        return std::unexpected(CubeError::camera_control_unavailable);
    }
    PhotometryState& state = photometry();
    const std::scoped_lock lock(state.mutex);
    state.pending.reset();
    state.restore_pending = true;
    return {};
}

std::optional<PhotometryLockReport> take_photometry_report() {
    PhotometryState& state = photometry();
    const std::scoped_lock lock(state.mutex);
    return std::exchange(state.report, std::nullopt);
}

}  // namespace cube::platform::camera
