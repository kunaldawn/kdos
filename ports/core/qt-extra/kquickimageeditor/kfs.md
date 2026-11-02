## About

KQuickImageEditor is a QML library of image-editing components: cropping, rotating, mirroring and
resizing, colour adjustment through a shader, and an annotation layer of arrows, shapes, text,
highlighting, blur and pixelation with undo. Spectacle draws its annotation editor with it, and
Koko, NeoChat and Skanpage use its crop and adjust tools. The QML module is
`org.kde.kquickimageeditor`; C++ code finds the library with `find_package(KQuickImageEditor)`.

## Notes

The blur is written against Google's Highway, the portable SIMD library, from
[highway](#port-highway), which the configure requires at 1.3.0 or newer; Highway chooses the widest vector instructions the running processor has, so the package stays at the
tree's baseline instruction set and still uses AVX2 or AVX-512 where they exist. The colour
adjustment's vertex and fragment shaders are compiled at build time by `qsb` from
[qt6-qtshadertools](#port-qt6-qtshadertools). The annotation settings are a KConfig schema, which
is why [kconfig](#port-kconfig) is in `depends`.

The QML imports `org.kde.kirigami` and Qt Quick's Effects, Shapes and Dialogs modules at run
time; [kirigami](#port-kirigami) and [qt6-qtdeclarative](#port-qt6-qtdeclarative) provide them,
and a missing one fails only when a window loads the editor.

The source tarball is 3.3 MB because it carries `koko-animation.mp4`, the demonstration video of
the README, which nothing builds or installs.
