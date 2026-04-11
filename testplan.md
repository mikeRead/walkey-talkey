# Current Build Test Plan

## Purpose

This test plan is for the current flashed build of the mode-system refactor.

The goal is to confirm:

- the device boots into the new mode-driven UI
- `BOOT` now works as mode control instead of PTT
- the `boot_mode` overlay appears and mode cycling works
- each currently wired mode sends the expected touch-driven HID behavior
- unimplemented gestures stay inactive instead of doing unexpected work

Note:

- some filled-in `Result` notes below were captured during earlier debug builds
- treat the `Expected` sections as the source of truth for the current firmware and re-run the checks on the latest flashed build when validating

## Build Under Test

- Firmware target: `esp32s3`
- Build style: built-in mode config from `main/mode_config.c`
- Current default mode: `Cursor`

## Not In Scope For This Build

These are not expected to work yet:

- JSON-backed mode loading
- `double_tap`
- `long_press` as a distinct routed action
- consumer/media HID usages beyond the current keyboard-style mappings

## Test Environment

Please fill in before testing:

- Date:
- Tester:
- Host OS:
- Host app used to observe key events:
- USB connected directly or through hub:
- Board COM port seen by host:

## Expected Power-On State

After boot, the default idle screen should show:

- a large centered mode label showing `Cursor`
- status: `Ready`
- audio text: `Mic Ready` if the microphone path initialized correctly
- hint text: `Hold BOOT to change modes`
- no separate `Touch Controller` title text

Fill in:

- Boot screen displayed as expected: `PASS / FAIL`
- Notes:

## Test Matrix

Use this format for each case:

- Result: `PASS / FAIL / PARTIAL / NOT TESTED`
- Notes:

## Section 1: Basic UI And Idle State

### 1.1 Idle screen is stable

Steps:

1. Power the device or reset it after flashing.
2. Wait for the UI to settle.
3. Do not touch the screen for 10 seconds.

Expected:

- screen stays on the main status card
- mode remains `Cursor`
- no spontaneous mode changes
- no unexpected HID key output on the host

Result:

- Result: GOOD
- Notes:

### 1.2 Touch-down visual feedback appears

Steps:

1. Briefly press and hold the touchscreen without swiping.
2. Release before the hold action threshold if possible.

Expected:

- the card/background should show dark red touch-pending visual feedback while pressed
- releasing should return the UI to idle if no mode action was triggered

### 1.3 Swipe feedback arrow appears on normal mode swipes

Steps:

1. Stay in a normal mode such as `Presentation`, `Media`, or `Navigation`.
2. Perform a left, right, up, or down swipe on the touchscreen.

Expected:

- a bottom-of-screen arrow should briefly flash in the swipe direction
- the arrow should appear outside the main card
- the arrow should auto-hide after a short delay
- the arrow should not remain stuck on-screen
- in `Cursor`, up/down swipes may also trigger the mapped mode action while still showing swipe feedback

Result:

- Result: GOOD but unsure about if the key was working, it used to send f14 tap
- Notes:

## Section 2: BOOT Mode Control

### 2.1 BOOT enters boot mode

Steps:

1. Press and hold `BOOT`.

Expected:

- a simplified BOOT mode selector appears
- top line should read `Swipe to switch mode`
- the active mode name should remain visible in the center of the screen
- the BOOT-selection state should be indicated by the blue mode card visual
- bottom hint should read `Release BOOT = Confirm`
- host should not receive an F13/PTT key press just from pressing `BOOT`

Result:

- Result: the overlay appears but it donest look great, it needs to be simpliar, instead of the default box have the text
  in muplie lines:
  Swipe to swtich mode
  <- (preve mode name)
  CURRENT SELECTED MODE
  (preve next mode name)->

---

NOTHING HAPPENDS when I swipe

- Notes:

### 2.2 Releasing BOOT exits boot mode

Steps:

1. Press and hold `BOOT`.
2. Release `BOOT` without swiping.

Expected:

- overlay closes
- device returns to normal mode screen
- active mode should remain unchanged

Result:

- Result: WOrks as expected
- Notes:

### 2.3 BOOT + swipe right cycles to next mode

Steps:

1. Press and hold `BOOT`.
2. Swipe right on the touchscreen.
3. Release `BOOT`.

Expected mode order:

- `Cursor` -> `Presentation` -> `Media` -> `Navigation` -> back to `Cursor`

Expected:

- current mode changes to the next mode in the list
- overlay updates while BOOT is still held
- after release, the main screen shows the newly selected mode

Result:

- Result:FULLY FAILS NOTHING CHANGES ITs like i didn't do anything
- Notes:

### 2.4 BOOT + swipe left cycles to previous mode

Steps:

1. Press and hold `BOOT`.
2. Swipe left on the touchscreen.
3. Release `BOOT`.

Expected reverse order:

- `Cursor` <- `Presentation` <- `Media` <- `Navigation`

Expected:

- current mode changes to the previous mode in the list
- after release, the selected mode is preserved

Result:

- Result:FULLY FAILS NOTHING CHANGES ITs like i didn't do anything
- Notes:

### 2.5 BOOT + tap shows mode without changing it

Steps:

1. Press and hold `BOOT`.
2. Tap the touchscreen without swiping.
3. Release `BOOT`.

Expected:

- no mode change
- overlay remains active while BOOT is held
- current mode remains the same after release

Result:

- Result: FULLY FAILS NOTHING CHANGES ITs like i didn't do anything
- Notes:

## Section 3: Cursor Mode

Precondition:

- active mode is `Cursor`

### 3.1 Touch hold starts dictation/PTT behavior

Steps:

1. Ensure the mode label reads `Cursor`.
2. Press and hold the touchscreen long enough to trigger hold.

Expected:

- host receives `F13` key down
- mic gate is enabled
- UI status changes to `Dictation Active`
- audio text changes to `Streaming`
- hint text should reflect dictation activity

Result:

- Result: GOOD
- Notes:

### 3.2 Releasing hold stops dictation/PTT behavior

Steps:

1. While still in `Cursor` mode, trigger the hold behavior.
2. Release the touchscreen.

Expected:

- host receives `F13` key up
- mic gate is disabled
- UI returns to idle/ready state
- mode remains `Cursor`

Result:

- Result:GOOD
- Notes:

### 3.3 Cursor tap does not send a normal typing key

Steps:

1. In `Cursor` mode, do a short tap.

Expected:

- host receives `F14`
- mode should remain `Cursor`
- tap should not send slide-navigation or arrow keys

Result:

- Result:GOOD
- Notes:

### 3.4 Cursor swipe up clears the field

Steps:

1. In `Cursor` mode, focus an app with editable text.
2. Swipe up on the touchscreen.

Expected:

- host receives `Ctrl+A` followed by `Backspace`
- hint text may show `Clear field`
- mode remains `Cursor`

Result:

- Result:
- Notes:

### 3.5 Cursor swipe down toggles Cursor text mode

Steps:

1. In `Cursor` mode, keep the Cursor text input or composer focused on the host.
2. Swipe down on the touchscreen.

Expected:

- host receives `Ctrl+.`
- Cursor toggles text mode on the host
- hint text may show `Toggle text mode`
- mode remains `Cursor`
- the shortcut should fire once for the swipe and must not repeat rapidly while the finger is still moving

Result:

- Result:
- Notes:

### 3.6 Cursor swipe right sends Enter

Steps:

1. In `Cursor` mode, focus an app where `Enter` is meaningful.
2. Swipe right on the touchscreen.

Expected:

- host receives `ENTER`
- hint text may show `Enter`
- mode remains `Cursor`

Result:

- Result:
- Notes:

## Section 4: Presentation Mode

Precondition:

- use BOOT mode control to select `Presentation`

### 4.1 Swipe left sends Page Down

Expected:

- host receives `PAGE_DOWN`
- hint text may show `Next slide`

Result:

- Result:
- Notes:

### 4.2 Swipe right sends Page Up

Expected:

- host receives `PAGE_UP`
- hint text may show `Previous slide`

Result:

- Result:
- Notes:

### 4.3 Tap sends Space

Expected:

- host receives `SPACE`

Result:

- Result:
- Notes:

## Section 5: Media Mode

Precondition:

- use BOOT mode control to select `Media`

Note:

- this mode currently uses placeholder keyboard-safe mappings, not true media-consumer HID usages

### 5.1 Tap sends Space

Expected:

- host receives `SPACE`

Result:

- Result:
- Notes:

### 5.2 Swipe left sends Left Arrow

Expected:

- host receives `LEFT_ARROW`

Result:

- Result:
- Notes:

### 5.3 Swipe right sends Right Arrow

Expected:

- host receives `RIGHT_ARROW`

Result:

- Result:
- Notes:

## Section 6: Navigation Mode

Precondition:

- use BOOT mode control to select `Navigation`

### 6.1 Swipe left sends Left Arrow

Expected:

- host receives `LEFT_ARROW`

Result:

- Result:
- Notes:

### 6.2 Swipe right sends Right Arrow

Expected:

- host receives `RIGHT_ARROW`

Result:

- Result:
- Notes:

### 6.3 Tap sends Escape

Expected:

- host receives `ESCAPE`

Result:

- Result:
- Notes:

## Section 7: Negative And Regression Checks

### 7.1 BOOT alone no longer acts as PTT

Steps:

1. Press and hold `BOOT`.
2. Do not touch the screen.

Expected:

- no `F13` should be sent
- no mic gate streaming state should begin
- only the mode-control overlay should appear

Result:

- Result:
- Notes:

### 7.2 Unsupported gestures do nothing harmful

Steps:

1. Try `double_tap`.
2. Try `swipe_up`.
3. Try `swipe_down`.

Expected:

- no crash
- no unwanted mode changes
- no clearly incorrect host key output
- repeated swipe callbacks should not cause the same macro to fire more than once per gesture

Result:

- Result:
- Notes:

### 7.3 Mode changes clear prior active outputs

Steps:

1. In `Cursor`, start a touch hold so dictation/PTT is active.
2. Release touch.
3. Enter boot mode and switch to another mode.
4. Use the new mode.

Expected:

- previous active key state should not remain stuck
- no lingering F13 state after mode switch
- new mode behavior should work normally

Result:

- Result:
- Notes:

## Section 8: Overall Summary

- Overall result:
- Most important failures:
- Suspected repro steps for failures:
- Any host apps used for verification:
- Photos or videos captured:
