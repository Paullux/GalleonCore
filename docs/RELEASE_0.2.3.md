# GalleonCore 0.2.3

## Nouveautés / What's new

- **Ajout des fonds d'écran personnalisés en jouant en options de fond d'écran en mode repos**. Les images de jeu validées sont conservées dans la bibliothèque personnelle ; elles peuvent aussi être choisies comme fond normal, sans changer automatiquement le fond au repos.
- Choix des illustrations Gaming à la molette droite, aperçu et validation après quatre secondes ; conservation du choix par jeu et utilisation hors ligne des images déjà disponibles.
- Reconnaissance étendue des jeux locaux et des lanceurs, amélioration des correspondances SteamGridDB et second bouton Appliquer près des images personnalisées.
- Configuration FPS Linux par sélection : préparation MangoHud, jeux Steam, Heroic général et jeux existants, Lutris, CrossOver Vulkan/DXVK, Bottles, PlayOnLinux et permissions Flatpak détectées. Compte rendu, sauvegardes et restauration ; aides manuelles repliables.
- Corrections des grandes valeurs VDF Steam, commandes MangoHud déjà correctes, doublons de métadonnées Heroic et anciennes entrées Lutris dont l’exécutable manque.
- Version du firmware Stream Deck visible dans Bosun et sa barre d’état ; conseil de mise à jour sous Windows pour les versions antérieures à 3.06.006. Simulation explicite avec `galleon-bosun --old-firmware` ; lien vers le logiciel officiel Elgato, séparé du Web Hub Corsair.
- Notifications système de mise à jour, en conservant la détection des installations gérées par dépôt.
- Construction AppImage corrigée : appimagetool 1.9.1 fixe et SHA-256 vérifié, au lieu d’un binaire `continuous` remplacé entre deux CI.

## English summary

Persistent game artwork can also be selected as an idle-screen wallpaper from the personal library. This release adds right-dial artwork selection, improved local-game discovery and SteamGridDB matching, selection-based Linux FPS setup with backups, Stream Deck firmware readback and an explicit old-firmware preview, system update notifications, and clearer Bosun controls and help. AppImage packaging now uses SHA-256-verified appimagetool 1.9.1.

## Installation and compatibility

Use the Windows per-user NSIS Setup or portable ZIP, Ubuntu DEB/PPA, Fedora RPM/COPR, Arch package/AUR, or the AppImage with separate host-integration scripts. Python 3 is required for Linux launcher configuration. MangoHud and its matching Flatpak extension remain external dependencies. Existing custom launch options are preserved; CrossOver OpenGL injection remains manual.

For a Stream Deck firmware update, fully stop **Bosun and galleond**, use the official Elgato Stream Deck app on Windows, keep the keyboard connected, then close Elgato and disable its automatic startup before restarting GalleonCore. The main Corsair keyboard firmware is separate. No firmware flashing is performed by GalleonCore.

Automated CI does not replace physical Windows/Linux startup, reconnect, idle and gameplay checks. The detailed owner observations and remaining limits are recorded in [ROADMAP.md](../ROADMAP.md), [Windows instructions](WINDOWS.md), [Gaming widgets](HOME_WIDGETS.md) and [Linux launcher setup](MODE_JEU_LINUX.fr.md).

[Full changelog](https://github.com/Paullux/GalleonCore/compare/v0.2.2...v0.2.3)
