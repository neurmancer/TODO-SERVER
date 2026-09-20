# Third-Party Licenses

This project contains and uses third-party software distributed under licenses
separate from the project's GNU GPL license.

## MD4C

MD4C is third-party software used for Markdown processing.

Upstream project: 
[MD4C on GitHub](https://github.com/mity/md4c)

License: 
MIT LICENSE

Copyright:
Copyright (c) 2016-2024 Martin Mitas

The original license text is preserved with the vendored MD4C source files in
[Server/vendor/md4c/LICENSE.md](Server/vendor/md4c/LICENSE.md).

---

## Gradle Wrapper

The scripts and wrapper JAR in `Client/` come from Gradle 8.13.0.

Upstream: [Gradle](https://github.com/gradle/gradle/tree/v8.13.0).
License: Apache License 2.0. The wrapper JAR's license text is preserved in
[Client/gradle/LICENSE](Client/gradle/LICENSE).

---

## FFmpeg / ffprobe

The music importer runs FFmpeg's `ffprobe` command to validate downloaded MP3
files. `Server/build.sh install` installs the distribution's `ffmpeg` package.
FFmpeg is an external command-line dependency; its source and binaries are not
bundled in this repository or the Android APK.

Upstream: [FFmpeg](https://ffmpeg.org/).

License: LGPL version 2.1 or later by default. Enabling optional GPL components
makes the FFmpeg build subject to the GPL; the exact license depends on the
build configuration and enabled dependencies. See the
[upstream licensing information](https://ffmpeg.org/legal.html) and the license
notices shipped with your distribution's package. Run `ffprobe -L` to display
the installed executable's license.

Third-party components remain subject to their original licenses and
copyright notices.
