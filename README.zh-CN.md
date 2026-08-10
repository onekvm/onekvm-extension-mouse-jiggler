# OneKVM 鼠标抖动扩展

[English](README.md) | 简体中文

定时让 OneKVM 所连接计算机的鼠标移动一个像素并立即移回，避免计算机因
长时间无输入而休眠。扩展体积小，并且只在启用期间运行。

> **构建说明：** 本仓库只包含扩展源码。正式软件包由 `onekvm-distro` 中的
> OpenEmbedded（OE）配方统一构建；本仓库不提供独立的正式构建流程。

## 功能

- 活动间隔可设为 5 秒到 24 小时
- 鼠标往返移动一个像素，最终回到原来的位置
- CPU 和内存限制较低，适合嵌入式设备
- 通过 OneKVM 已鉴权的 HID 扩展接口发送输入
- 不直接访问 USB HID 设备

## 安装与启用

软件包名称是 `onekvm-extension-mouse-jiggler`。请通过 OneKVM 发行版的软件包
或镜像安装，然后使用插件管理器控制扩展：

```sh
onekvm-plugin-manager enable mouse-jiggler
onekvm-plugin-manager disable mouse-jiggler
```

## 配置

打开 **鼠标抖动** 扩展页面并设置间隔秒数。默认值为 60 秒，有效范围是
5 到 86,400 秒；保存新值后，正在运行的服务会使用新配置。

这种活动可以阻止许多操作系统因空闲而休眠，但无法覆盖忽略鼠标移动或按
其他条件强制休眠的主机策略。

## 实现说明

C++20 服务会请求 OneKVM Core 发送一次相对鼠标移动及其反向移动，不会
自行打开物理 HID 设备。停用扩展后服务随即停止，不再产生后台鼠标活动。

## 许可证

本项目使用 GNU General Public License v3.0，完整条款见
[`LICENSE`](LICENSE)。
