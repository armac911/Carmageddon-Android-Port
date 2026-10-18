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
