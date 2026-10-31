# Bosun languages

Bosun includes English, French, German and Spanish in every desktop package,
including DEB/PPA, RPM/COPR, AppImage and Windows. No language package or network
request is needed. The globe menu in the upper right is always available;
language names are shown as English, Français, Deutsch and Español so users can
recover from an accidental selection. Changes apply immediately to the window
and notification-area menu, without restarting Bosun or galleond.

The default is **System language**: the first supported entry in Qt's system UI
language preferences wins, including regional variants such as `fr_CA` and
`es-MX`. English is the fallback. An explicit selection is saved in
`galleoncore/bosun.conf`, under `interface/language`, in the user's configuration
folder. Screenshot and simulation modes do not read or write that preference.
Names and labels from personal profiles and installed applications are preserved.
The selected tab keeps its full title; other tabs share the width reduction,
with a readable minimum and horizontal scrolling when necessary. Width changes
animate over one second; screenshot mode keeps a static layout. This does
not change the language picker’s position, size or behavior.
OS-owned dialogs, external program output and technical configuration diagnostics
may follow the system or originating program's language. Documentation linked from
an explanation keeps its original language.

## Catalog maintenance

`bosun/translations/{en,fr,de,es}.json` are UTF-8 source-text catalogs. Each key
is the original complete source text; the value is its translation. Existing
French source texts also have English translations. These small, embedded
catalogs are read by a `QTranslator` adapter, so both `tr()` and QML `qsTr()` use
the same selection without requiring Linguist tools on builders. Contexts share
a translation for identical source text; use distinct source wording when the
meaning differs. Preserve `%1`, `%2`, `%n`, file-filter patterns, command syntax,
paths, product names and all safety conditions. Never translate configuration
identifiers or user-entered content.

For new UI text, update all four catalogs. `tools/test_translations.py` checks
static Bosun text coverage, duplicate keys, completeness and substitution
arguments. `test_languages` covers regional language matching, fallback,
persistence, rejected preferences and immediate QML/tray translation. Bosun's
window test also checks that a language change keeps an unfinished key edit and
personal theme names intact. The German and Spanish wording was prepared with
AI assistance; native-speaker review is welcome.

The FPS helper keeps its operational data and comparisons in their original
form. Bosun translates fixed presentation text and formats helper-generated
account/bottle suffixes separately from personal game and bottle names.
