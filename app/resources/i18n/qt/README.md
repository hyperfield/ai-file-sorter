# Bundled Qt translations

The unmodified qtbase catalogs for da, de, es, fi, fr, it, ko, nl, sv, tr and
zh_CN come from Qt qttranslations v6.10.0:
https://github.com/qt/qttranslations/tree/v6.10.0/translations

These catalogs are compatible with the Qt 6.9 Windows runtime. Upstream module
licensing offers the GPL-3.0-only distribution option. Its license text, upstream
licensing metadata and the upstream Qt GPL exception text are retained here and
embedded with the catalogs; no additional exception is claimed for the catalogs.

Qt has no qtbase catalog for the supported Hindi, Icelandic or Norwegian Bokmål
interfaces at this revision. The application-owned hi, is and nb catalogs provide
an explicit fallback for all 18 QPlatformTheme standard buttons. Other standard
Qt text in these three languages falls back to English. English uses Qt source
strings and installs no translator.

CMake and Make compile and embed these catalogs, so deployments need no external
Qt translation installation. Exclude these vendor/fallback catalogs from app
lupdate; their source contexts belong to Qt.
