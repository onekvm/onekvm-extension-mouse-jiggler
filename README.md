# OneKVM Mouse Jiggler

English | [简体中文](README.zh-CN.md)

> **Build note:** This repository contains extension sources only. The
> extension is built and packaged by the OpenEmbedded recipes in
> `onekvm-distro`; this repository does not provide a standalone release build.

A low-resource OneKVM extension that periodically sends a one-pixel relative
mouse movement and immediately moves back. The interval is configured on the
extension's own page. The process only runs while the extension is enabled.

The interval can be set from 5 seconds to 24 hours. The service uses OneKVM's
HID extension interface and does not access USB HID devices directly.
