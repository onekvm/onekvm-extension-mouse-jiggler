# OneKVM Mouse Jiggler Extension

English | [简体中文](README.zh-CN.md)

Keep the computer connected to OneKVM awake by periodically moving its pointer
one pixel and immediately moving it back. The extension is intentionally small
and only runs while enabled.

> **Build note:** This repository contains extension sources only. Release
> packages are built by the OpenEmbedded recipes in `onekvm-distro`; this
> repository does not provide a standalone release build.

## Features

- Configurable activity interval from 5 seconds to 24 hours
- One-pixel round-trip movement that leaves the pointer in its original place
- Low CPU and memory limits suitable for embedded devices
- Input sent through OneKVM's authenticated HID extension interface
- No direct access to USB HID devices

## Install and enable

The package name is `onekvm-extension-mouse-jiggler`. Install it through your
OneKVM distribution package or image, then manage it with the plugin manager:

```sh
onekvm-plugin-manager enable mouse-jiggler
onekvm-plugin-manager disable mouse-jiggler
```

## Configuration

Open the **Mouse Jiggler** extension page and set the interval in seconds. The
default is 60 seconds; accepted values range from 5 to 86,400 seconds. Saving a
new value updates the running service.

The activity prevents idle-based sleep in many operating systems, but it does
not override host policies that ignore pointer movement or enforce sleep by
other criteria.

## How it works

The C++20 service asks OneKVM Core to send a relative mouse movement and its
inverse. It never opens the physical HID device itself. Disabling the extension
stops the service, so no background movement is generated afterward.

## License

This project is licensed under the GNU General Public License v3.0. See
[`LICENSE`](LICENSE).
