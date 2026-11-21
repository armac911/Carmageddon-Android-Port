# Third-party notices

The initial repository snapshot contains inherited code. Its initial commit does
not establish authorship of those files. Preserve original source notices and
licenses when modifying or redistributing it.

| Component | Source / credit | License material |
| --- | --- | --- |
| Dethrace and BRender-compatible engine code | dethrace-labs/dethrace and contributors | Root LICENSE (GPL v3); original source notices |
| Inherited Android port | mgrz18/dethrace-android; attribution from inherited README | docs/README-inherited-android-fork.md; source notices |
| SDL 2.30.10 | libsdl.org and contributors; downloaded for build | docs/licenses/SDL2-LICENSE.txt; downloaded SDL source includes further component notices |
| miniaudio | David Reid and contributors | lib/miniaudio/LICENSE; header includes additional embedded-code notices |
| libsmacker 1.1.1 | Greg Kennedy and contributors | lib/libsmacker/COPYING (LGPL 2.1); smacker.h grants 2.1 or later |
| glad 0.1.36 generated loader | David Herberth and contributors | lib/glad/LICENSE; retained generator/API notices |
| Khronos API/platform definitions | The Khronos Group | lib/glad/LICENSE, lib/glad/include/KHR/khrplatform.h; docs/licenses/Apache-2.0.txt |
| Unity C test framework | Mike Karlesky, Mark VanderVoord, Greg Williams and contributors | test/framework/LICENSE.txt; source headers retain original dates |

Source provenance: the inherited Android README identifies the Dethrace opengles
branch as its basis. The restored snapshot does not record an exact upstream base
commit or complete historical commit graph. Do not replace attribution with an
AndroGaming-only or AI-only authorship claim.

Build/test tools include Android SDK/NDK, CMake, Gradle, Java, Python, C compilers
and Mesa EGL/GLES. GitHub Actions workflows reference separately maintained
community actions. Their own notices/terms apply; these tools are not presented
as AndroGaming-authored engine components.

Carmageddon game data, sound/music, trademarks and original artwork are separate
from the engine code. Original game development is credited to Stainless Games.
The project includes a maintainer-supplied game-art app icon and AndroGaming
launcher logo; it makes no claim to ownership of the original game artwork.
No original DATA/ or MUSIC/ directory or private signing key belongs in Git.

## 0.9.1 additions

- innoextract by Daniel Scharrer/contributors: zlib-style license retained in
  docs/licenses/innoextract-LICENSE.txt. Its extraction implementation is
  unchanged; only CMake integration is patched in the build directory.
- Boost 1.85.0: Boost Software License 1.0, docs/licenses/Boost-LICENSE.txt.
- XZ 5.4.6 liblzma: public domain; complete package licensing summary retained
  in docs/licenses/XZ-COPYING.txt. Only liblzma is linked into the helper.
- bzip2 1.0.8: license retained in docs/licenses/bzip2-LICENSE.txt.
- stb_vorbis: Sean Barrett and contributors, dual MIT/public domain; complete
  license remains in lib/miniaudio/stb_vorbis.c, with source commit recorded in
  STB-SOURCE.txt. The original miniaudio source is unchanged; a wrapper enables
  its documented Vorbis backend.

Pinned installer source URLs/checksums and build modifications are in
android/installer/CMakeLists.txt and docs/IMPORTER-BUILD.md.

## AMD FidelityFX Super Resolution 1.0

- src/harness/resources/fsr_easu_frag.glsl and fsr_rcas_frag.glsl are GLSL
  ports of the EASU and RCAS passes in AMD's ffx_fsr1.h
  (https://github.com/GPUOpen-Effects/FidelityFX-FSR).
  Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved.
  Licensed under the MIT License: Permission is hereby granted, free of charge,
  to any person obtaining a copy of this software and associated documentation
  files (the "Software"), to deal in the Software without restriction,
  including without limitation the rights to use, copy, modify, merge,
  publish, distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to the
  following conditions: The above copyright notice and this permission notice
  shall be included in all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

## Snapdragon Game Super Resolution 1

- src/harness/resources/gsr_frag.glsl is a GLSL ES 3.0 port of
  sgsr1_shader_mobile_edge_direction.frag from
  https://github.com/SnapdragonStudios/snapdragon-gsr.
  Copyright (c) 2023-2025, Qualcomm Innovation Center, Inc. All rights reserved.
  Licensed under the BSD 3-Clause License: Redistribution and use in source and
  binary forms, with or without modification, are permitted provided that the
  following conditions are met: (1) Redistributions of source code must retain
  the above copyright notice, this list of conditions and the following
  disclaimer. (2) Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  (3) Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from this
  software without specific prior written permission.
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
