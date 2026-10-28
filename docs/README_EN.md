# Stream Stats

System load, streaming and recording in one OBS panel. The layout works vertically and horizontally.

![Stream Stats](screenshots/vertical.png)

## Features

- Computer and OBS CPU, computer GPU, memory, FPS and frame loss.
- Total streaming bitrate, data sent and active output count.
- Recording speed, file size, duration and free disk space.
- Charts covering 1, 5 or 15 minutes, with values under the pointer.
- Optional frame loss and low disk warnings, plus a session summary that can be saved as CSV.
- A collapsed “Our plugins” section showing Stream Chat and Stream Notify traffic and local connection status.

## Install

1. Close OBS. Download **StreamStats-1.0.4-OBS33-Windows-x64** from the [latest successful build](https://github.com/DIMI0007/stream-stats/actions/workflows/build-windows.yml) and extract the ZIP.
2. Open `C:\ProgramData\obs-studio\plugins\`. Replace the old `stream-stats` folder with the complete folder from the ZIP.
3. Start OBS and open **Docks → Stream Stats**.

For OBS Studio 33 on Windows x64. The build is unsigned. GPU usage covers all applications, and local OBS stream activity does not confirm that a platform is live for viewers.

## Support the project

If these plugins help your streams, please consider a donation. Your support helps improve the plugins, fix issues and fund future development.

[Support via DonationAlerts](https://www.donationalerts.com/r/otmopozok) · [Support via DonatePay](https://donatepay.ru/don/OTMOPOZOK)

[Русский](../README.md) · [GPL v2 or later](../LICENSE_NOTICE.md)
