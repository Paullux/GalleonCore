# GalleonCore 0.3.0

## What's new

- **Bosun in four languages:** English, French, German and Spanish are bundled in every Linux and Windows package. Bosun detects the system UI language and falls back to English when it is unsupported.
- **Instant language switching:** use the globe menu to change the window and notification-area menu without restarting. The choice is remembered across sessions; personal page names and key labels remain intact.
- **Adaptive, animated tabs:** the selected tab keeps its complete title. Other tabs share the width reduction down to a readable minimum, then the bar scrolls when necessary. A smooth one-second transition shows the selected tab growing and the others shrinking, with the selection kept in view. The language picker stays in place.
- **Shortcut drafts preserved:** backend refreshes and language changes no longer discard unfinished key edits. Switching to another key still loads its saved configuration.
- **Clearer updates:** a freshly validated newer version opens the existing Updates panel once per version across sessions, including AUR installations. Repository-managed DEB/RPM installations retain package-manager updates; simulations and screenshots do not persist presentation state.
- **Packaging follow-up:** retain the writable disposable test home for Launchpad and the Qt WebSockets dependency in the repair workflow. Package tests remain mandatory.

## Installation and compatibility

The languages are included in the existing Windows Setup/ZIP, DEB/PPA, RPM/COPR, Arch/AUR and AppImage distributions. No separate language package, download or additional translation build tool is required. Existing profiles are not migrated or overwritten.

Bosun language selection applies to Bosun, including its tray menu and notifications. It does not rename personal content or change the daemon's language. OS-owned dialogs, external program output and technical configuration diagnostics may keep their originating language. Linked documentation keeps its original language. German and Spanish wording was prepared with AI assistance; native-speaker review is welcome.

The supported Qt baseline remains 6.4. This release does not introduce a new HID ownership or RGB protocol policy and does not establish new physical Windows hardware validation. Existing hardware observations and limitations remain in [the roadmap](https://github.com/Paullux/GalleonCore/blob/v0.3.0/ROADMAP.md).

## Résumé en français

Bosun inclut désormais le français, l’anglais, l’allemand et l’espagnol. Le menu globe permet de suivre la langue du système ou de changer immédiatement toute l’interface et le menu de notification ; le choix est mémorisé. Les onglets adaptent leur largeur pour garder le titre actif entier, avec une animation fluide d’une seconde. Les brouillons des raccourcis sont conservés lors des rafraîchissements et changements de langue. Les profils et libellés personnels restent inchangés, sans paquet de langue supplémentaire.

[Translation maintenance](https://github.com/Paullux/GalleonCore/blob/v0.3.0/docs/TRANSLATIONS.md) · [Full changelog](https://github.com/Paullux/GalleonCore/compare/v0.2.3...v0.3.0)
