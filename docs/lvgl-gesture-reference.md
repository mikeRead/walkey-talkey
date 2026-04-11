# LVGL Gesture And Event Reference

## Scope

This document lists the LVGL event families available in the installed LVGL version used by this project, with extra focus on touch, click, and gesture handling.

For this project, the most important group is the input-device event family.

## Input Device Events

These are the main events you use for touch, pointer, encoder, keypad, click streaks, and gesture handling:

- `LV_EVENT_PRESSED`: widget has been pressed
- `LV_EVENT_PRESSING`: widget is being pressed continuously
- `LV_EVENT_PRESS_LOST`: pointer/finger is still down but moved off the widget
- `LV_EVENT_SHORT_CLICKED`: short press and release, not sent if scrolled
- `LV_EVENT_SINGLE_CLICKED`: first short click in a click streak
- `LV_EVENT_DOUBLE_CLICKED`: second short click in a click streak
- `LV_EVENT_TRIPLE_CLICKED`: third short click in a click streak
- `LV_EVENT_LONG_PRESSED`: press held at least `long_press_time`
- `LV_EVENT_LONG_PRESSED_REPEAT`: repeated callback after long press
- `LV_EVENT_CLICKED`: release without scroll, regardless of long press
- `LV_EVENT_RELEASED`: widget was released in all cases
- `LV_EVENT_SCROLL_BEGIN`: scrolling begins
- `LV_EVENT_SCROLL_THROW_BEGIN`: throw or inertial scroll begins
- `LV_EVENT_SCROLL_END`: scrolling ends
- `LV_EVENT_SCROLL`: scrolling in progress
- `LV_EVENT_GESTURE`: gesture detected
- `LV_EVENT_KEY`: key sent to widget
- `LV_EVENT_ROTARY`: encoder or wheel rotation sent to widget
- `LV_EVENT_FOCUSED`: widget received focus
- `LV_EVENT_DEFOCUSED`: widget lost focus
- `LV_EVENT_LEAVE`: widget lost focus but is still selected
- `LV_EVENT_HIT_TEST`: advanced hit testing
- `LV_EVENT_INDEV_RESET`: input device reset
- `LV_EVENT_HOVER_OVER`: hover entered widget
- `LV_EVENT_HOVER_LEAVE`: hover left widget

## Built-In Gesture Directions

When LVGL raises `LV_EVENT_GESTURE`, the gesture direction can be read from the active input device and is typically one of:

- left
- right
- up
- down

Directional gestures are native. Rich multi-touch behaviors are not the default pattern.

## Drawing Events

These are emitted around rendering and custom draw flows:

- `LV_EVENT_COVER_CHECK`
- `LV_EVENT_REFR_EXT_DRAW_SIZE`
- `LV_EVENT_DRAW_MAIN_BEGIN`
- `LV_EVENT_DRAW_MAIN`
- `LV_EVENT_DRAW_MAIN_END`
- `LV_EVENT_DRAW_POST_BEGIN`
- `LV_EVENT_DRAW_POST`
- `LV_EVENT_DRAW_POST_END`
- `LV_EVENT_DRAW_TASK_ADDED`

## Special Events

These are common widget or process state events:

- `LV_EVENT_VALUE_CHANGED`
- `LV_EVENT_INSERT`
- `LV_EVENT_REFRESH`
- `LV_EVENT_READY`
- `LV_EVENT_CANCEL`

## Object And Screen Lifecycle Events

These cover object creation, deletion, screen changes, layout, and style changes:

- `LV_EVENT_CREATE`
- `LV_EVENT_DELETE`
- `LV_EVENT_CHILD_CHANGED`
- `LV_EVENT_CHILD_CREATED`
- `LV_EVENT_CHILD_DELETED`
- `LV_EVENT_SCREEN_UNLOAD_START`
- `LV_EVENT_SCREEN_LOAD_START`
- `LV_EVENT_SCREEN_LOADED`
- `LV_EVENT_SCREEN_UNLOADED`
- `LV_EVENT_SIZE_CHANGED`
- `LV_EVENT_STYLE_CHANGED`
- `LV_EVENT_LAYOUT_CHANGED`
- `LV_EVENT_GET_SELF_SIZE`

## Display And Rendering Pipeline Events

These are optional or display-pipeline-oriented events:

- `LV_EVENT_INVALIDATE_AREA`
- `LV_EVENT_RESOLUTION_CHANGED`
- `LV_EVENT_COLOR_FORMAT_CHANGED`
- `LV_EVENT_REFR_REQUEST`
- `LV_EVENT_REFR_START`
- `LV_EVENT_REFR_READY`
- `LV_EVENT_RENDER_START`
- `LV_EVENT_RENDER_READY`
- `LV_EVENT_FLUSH_START`
- `LV_EVENT_FLUSH_FINISH`
- `LV_EVENT_FLUSH_WAIT_START`
- `LV_EVENT_FLUSH_WAIT_FINISH`
- `LV_EVENT_VSYNC`
- `LV_EVENT_VSYNC_REQUEST`

If translation support is enabled in LVGL, there is also:

- `LV_EVENT_TRANSLATION_LANGUAGE_CHANGED`

## What LVGL Does Not Hand You Automatically

LVGL does not usually give you a full, generic high-level mobile gesture engine out of the box. These are typically built in app logic:

- pinch zoom
- rotate
- two-finger gestures
- custom drawn gesture shapes
- advanced multi-step gesture combos

## Recommended Pattern For This Project

For this ESP32 touch UI, the cleanest LVGL-native approach is:

- `LV_EVENT_PRESSED` for touch-down feedback
- `LV_EVENT_SHORT_CLICKED` if you want a real tap action
- `LV_EVENT_DOUBLE_CLICKED` only if you truly want double-tap behavior
- `LV_EVENT_LONG_PRESSED` for hold start
- `LV_EVENT_RELEASED` for hold end
- `LV_EVENT_GESTURE` only if you want directional swipe handling

## Practical Note For Current Firmware

For the current PTT firmware:

- short tap: no action
- long press: same PTT behavior as the BOOT button
- release after long press: end PTT

That keeps the logic simple and leans on LVGL's native long-press event flow instead of a custom gesture engine.
