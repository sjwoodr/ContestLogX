# Third-Party Attributions

This file lists the third-party libraries distributed with ContestLogX and their license terms.

---

## Qt Framework

**Modules used:** Qt6::Core, Qt6::Widgets, Qt6::Network, Qt6::Xml, Qt6::Multimedia, Qt6::SerialPort

**Copyright:** The Qt Company Ltd. and individual contributors

**License:** GNU Lesser General Public License v3.0 (LGPL v3)

Qt is used as a dynamically linked library. In accordance with the LGPL v3, users
may re-link this application against a modified version of Qt. The Qt shared
libraries (`.so` / `.dylib` / `.dll`) bundled in this distribution are unmodified.

- License text: https://www.gnu.org/licenses/lgpl-3.0.html
- Source code:  https://code.qt.io / https://www.qt.io/download

Qt is a trademark of The Qt Company Ltd.

---

## FFmpeg

**Copyright:** The FFmpeg developers

**License:** GNU Lesser General Public License v2.1 or later (LGPL v2.1+)

The FFmpeg shared libraries (libavcodec, libavformat, libavutil, libswresample,
libswscale) are the LGPL build that ships with Qt Multimedia, bundled unmodified
and dynamically linked.

- License text: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
- Source code:  https://ffmpeg.org/download.html

---

## CTY.DAT (DXCC Country Database)

**Author:** Jim Reisert AD1C

**License:** Freely redistributable for amateur radio use. See https://www.country-files.com/ for terms.

The `cty.dat` file provides DXCC entity, prefix, and continent data used for
country lookups and multiplier scoring.

---

*ContestLogX itself is released under the MIT License.
See the LICENSE file for details.*
