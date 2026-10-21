// Which camera a phone's browser opens, and changing it. A phone has a front
// camera and a back one; a cube is scanned with the back one, and the app opens
// it unless it is told otherwise. The browser's side of this - what it lists,
// in what order, how much it hides before a site has been given access - is
// injected into the page (support/fake_camera.js, `cameras` in the app's
// options); the app is expected to cope with each of them and to give the
// person a button for the other camera.
import { test, expect } from '../support/fixtures.mjs';
import { PHONE } from '../support/app.mjs';

const FRONT = { deviceId: 'cam-front', label: 'Front Camera', facing: 'user' };
const BACK = { deviceId: 'cam-back', label: 'Back Camera', facing: 'environment' };

// How a browser lists them to a page that has not been given access: Safari
// shows one anonymous entry, Chrome one per camera, neither an id or a name.
const SAFARI_FIRST_VISIT = { withheld: true, collapse: true };
const CHROME_FIRST_VISIT = { withheld: true };

const opened = (app) => app.page.evaluate(() => window.__fakeCamera.chosen());
const requests = (app) => app.page.evaluate(() => window.__fakeCamera.requests());

async function startScan(app) {
  await app.click('launch.startScan');
  await app.waitForScreen('scan');
  await expect.poll(async () => (await app.singleton('DirectScan')).running,
    { timeout: 60_000 }).toBe(true);
}

test.describe('the camera a first scan opens, listed front first with nothing named', () => {
  // Safari: front first, and one anonymous entry until the site has access.
  test.use({ appOptions: { cameras: { list: [FRONT, BACK], options: SAFARI_FIRST_VISIT } } });

  test('is the back one', async ({ app }) => {
    await startScan(app);

    expect(await opened(app)).toBe('cam-back');
    // The page asked for a camera facing away, not for "a camera": asked for
    // that, this browser gives the front one.
    const [first] = await requests(app);
    expect(first.video.facingMode.ideal).toBe('environment');
  });

  test('can be changed to the front one and back, starting the scan again each time',
    async ({ app }) => {
      await startScan(app);
      expect(await opened(app)).toBe('cam-back');

      // One entry was all the browser would say before the first camera opened,
      // so the button is for the list the app reads once there is a camera.
      await app.waitFor('scan.switchCamera');
      const before = (await requests(app)).length;

      await app.click('scan.switchCamera');
      await expect.poll(() => opened(app)).toBe('cam-front');
      await expect.poll(async () => (await app.singleton('DirectScan')).running).toBe(true);
      expect((await requests(app)).length).toBeGreaterThan(before);
      // The picture is a selfie's again, the way a mirror shows it.
      expect((await app.singleton('CameraController')).previewMirrored).toBe(true);

      await app.click('scan.switchCamera');
      await expect.poll(() => opened(app)).toBe('cam-back');
      await expect.poll(async () => (await app.singleton('DirectScan')).running).toBe(true);
      // The back camera shows the scene, and a cube read in a mirror is not the
      // cube in the hand.
      expect((await app.singleton('CameraController')).previewMirrored).toBe(false);
      expect((await app.singleton('DirectScan')).cameraError).toBe('');
    });
});

test.describe('the camera a first scan opens, when the browser says how many but not which', () => {
  test.use({ appOptions: { cameras: { list: [FRONT, BACK], options: CHROME_FIRST_VISIT } } });

  test('is the back one, and the front one is a button away', async ({ app }) => {
    await startScan(app);
    expect(await opened(app)).toBe('cam-back');

    await app.waitFor('scan.switchCamera');
    await app.click('scan.switchCamera');
    await expect.poll(() => opened(app)).toBe('cam-front');
  });
});

test.describe('the camera a first scan opens, when the browser lists them by name', () => {
  test.use({ appOptions: { cameras: { list: [FRONT, BACK] } } });

  test('is the back one, though it is listed second', async ({ app }) => {
    await startScan(app);

    expect(await opened(app)).toBe('cam-back');
    // The list in Settings has it first, and says which is selected.
    expect((await app.singleton('CameraController')).selectedDeviceId).toBe('cam-back');
    expect((await app.singleton('CameraController')).previewMirrored).toBe(false);
  });

  test('can be changed, and the choice is the one the solve screen keeps', async ({ app }) => {
    await startScan(app);
    await app.waitFor('scan.switchCamera');

    await app.click('scan.switchCamera');
    await expect.poll(() => opened(app)).toBe('cam-front');
    expect((await app.singleton('CameraController')).selectedDeviceId).toBe('cam-front');
  });
});

test.describe('a laptop with one webcam', () => {
  test.use({
    appOptions: {
      cameras: {
        list: [{ deviceId: 'webcam', label: 'Integrated Camera' }],
        options: SAFARI_FIRST_VISIT,
      },
    },
  });

  test('opens it, mirrored, and has nothing to switch to', async ({ app }) => {
    await startScan(app);

    expect(await opened(app)).toBe('webcam');
    expect((await app.singleton('CameraController')).previewMirrored).toBe(true);
    expect(await app.find('scan.switchCamera')).toBeNull();
  });
});

test.describe('on a phone held upright', () => {
  test.use({
    appOptions: {
      viewport: PHONE,
      touch: true,
      cameras: { list: [FRONT, BACK] },
    },
  });

  test('the three buttons, Switch camera among them, fit across the screen', async ({ app }) => {
    await startScan(app);
    await app.waitFor('scan.switchCamera');
    await app.settle();

    expect(await app.cutOff()).toEqual([]);
    const button = await app.find('scan.switchCamera');
    expect(button.x).toBeGreaterThanOrEqual(0);
    expect(button.x + button.width).toBeLessThanOrEqual(PHONE.width);
    // Tapped, as a finger does.
    await app.tap('scan.switchCamera');
    await expect.poll(() => opened(app)).toBe('cam-front');
  });
});
