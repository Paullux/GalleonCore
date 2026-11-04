<p align="center"><img src="site/logo.svg" alt="" width="96" height="96"></p>

# GalleonCore

Bosun enthält Englisch, Französisch, Deutsch und Spanisch. Über das Globus-Menü folgst du der Systemsprache oder wechselst sofort; die Auswahl wird gespeichert. Siehe [Pflege der Übersetzungen](docs/TRANSLATIONS.md).

[![Website](https://img.shields.io/badge/website-GalleonCore-46505E)](https://galleoncore.xyz/)
[![CI](https://github.com/Paullux/GalleonCore/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Paullux/GalleonCore/actions/workflows/ci.yml)
[![Linux packages](https://github.com/Paullux/GalleonCore/actions/workflows/linux-packages.yml/badge.svg?branch=main)](https://github.com/Paullux/GalleonCore/actions/workflows/linux-packages.yml)
[![Windows desktop](https://github.com/Paullux/GalleonCore/actions/workflows/windows-core.yml/badge.svg?branch=main&event=push)](https://github.com/Paullux/GalleonCore/actions/workflows/windows-core.yml)
[![Release](https://img.shields.io/github/v/release/Paullux/GalleonCore)](https://github.com/Paullux/GalleonCore/releases)
[![GitHub downloads (latest release)](https://img.shields.io/github/downloads/Paullux/GalleonCore/latest/total?label=GitHub%20downloads%20%28latest%29)](https://github.com/Paullux/GalleonCore/releases)
[![Ubuntu PPA](https://img.shields.io/badge/Ubuntu-PPA-E95420?logo=ubuntu&logoColor=white)](https://launchpad.net/~paulwoisard/+archive/ubuntu/galleoncore)
[![Fedora COPR](https://img.shields.io/badge/Fedora-COPR-51A2DA?logo=fedora&logoColor=white)](https://copr.fedorainfracloud.org/coprs/paullux/GalleonCore/)
[![Arch AUR](https://img.shields.io/badge/Arch-AUR-1793D1?logo=archlinux&logoColor=white)](https://aur.archlinux.org/packages/galleoncore)
[![Linux AppImage](https://img.shields.io/badge/Linux-AppImage-8E6BBF?logo=linux&logoColor=white)](https://github.com/Paullux/GalleonCore/releases/download/v0.3.0/GalleonCore-0.3.0-x86_64.AppImage)
[![Licenses](https://img.shields.io/badge/license-GPL%20%2F%20LGPL%20%2F%20CC--BY--SA-blue)](LICENSE.md)
[![Security policy](https://img.shields.io/badge/security-report%20privately-46505E)](SECURITY.md)

🇬🇧 [Read in English](README.md) · 🇫🇷 [Lire en français](README.fr.md) · 🇪🇸 [Leer en español](README.es.md) · *Bei Abweichungen ist die englische Fassung maßgeblich.*

🌐 [Website](https://galleoncore.xyz/)

Den Entwicklungsstand des Projekts findest du in [ROADMAP.md](ROADMAP.md) (auf Französisch).

**Die offene Plattform für das Corsair Galleon 100 SD unter Linux.**

Das Galleon 100 SD ist eine mechanische Tastatur in voller Größe, deren Ziffernblock durch ein Elgato-Stream-Deck-Modul ersetzt ist: 12 LCD-Tasten, ein Bildschirm und zwei Drehregler. Corsair und Elgato unterstützen das Stream-Deck-Modul nur unter Windows und macOS. GalleonCore will jeden Teil der Tastatur unter Linux nutzbar machen und in den Desktop integrieren.

> **Aktive Entwicklung.** Die Tasten, der Bildschirm und die Drehregler des Stream Decks funktionieren unter Linux, Galleon Bosun dient zur Konfiguration des Moduls. Plasma-Widgets sind weiterhin geplant. Ein Windows-Desktop-Build mit NSIS-Setup im Bosun-Design und portablem ZIP steht für interaktive Tests bereit ([Details](docs/WINDOWS.md)); den ausführlichen Validierungsstand zeigt die [Roadmap](ROADMAP.md).

> GalleonCore steht in keiner Verbindung zu Corsair oder Elgato.

> **0.3.0:** Bosun auf Englisch, Französisch, Deutsch und Spanisch, sofortiger Sprachwechsel, animierte Tabs mit anpassbarer Breite und erhaltene Bearbeitungsentwürfe für Tastenbelegungen. [Versionshinweise](docs/RELEASE_0.3.0.md).

## Ziele

- **Stream-Deck-Modul**: auf den 12 Tasten und dem Bildschirm zeichnen, Tasten und Drehregler lesen.
- **Gemeinsame Widgets**: dieselben QML-Widgets (Uhr, Systemmonitor, aktuell laufender Titel …) werden auf dem Tastaturbildschirm gerendert *und* als KDE-Plasma-Widgets auf dem Desktop angezeigt.
- **RGB-Beleuchtung**: RGB pro Taste, RGB-Lichtleiste und Drehregler-Ringe über [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB).
- **OpenRGB-Plugin**: das Deck aus OpenRGB konfigurieren und Deck-Themes mit der Beleuchtung synchronisieren.
- **Windows-Desktop-Vorschau** auf demselben Kern, mit nativen Medien-, Audio- und Tastatur-Backends; die Hardwarevalidierung steht noch aus.

Startbildschirm-Widgets lassen sich im Tab **Widgets** von Bosun bearbeiten (Position, Größe und Sichtbarkeit) und mit **Anwenden** speichern. Die erste Ausbaustufe umfasst Wetter, CPU/RAM, die CPU-Temperatur unter Linux, die Erkennung unvollständiger Downloads und Spiel-FPS über die MangoHud-CSV unter Linux bzw. den PresentMon-Dienst/die API unter Windows, mit einstellbarer Textgröße; siehe [unterstützte Plattformen und aktuelle Grenzen](docs/HOME_WIDGETS.md).
Ein automatischer Spielbildschirm zeigt jetzt lokale Spielgrafiken, FPS und CPU/GPU-Temperaturen, solange die Erfassung läuft, und verhindert, dass der Bildschirmschoner der Tastatur während der Erfassung startet.

## Status

| Teil | Status |
|---|---|
| Stream-Deck-Protokoll (Tasten, Bildschirm, Drehregler) | Keep-alive, Helligkeit, Tasten- und Bildschirmbilder, umgesetzt anhand des eigenen Deskriptors des Moduls und der Arbeit der Community; Bildschirmbild, Tastenbilder sowie Tasten- und Drehregler-Eingabe **auf der Hardware bestätigt** |
| QML-Rendering ohne Fenster auf das Deck | Hintergrundbild mit Uhrzeit (`WallpaperClock.qml`), Software-Renderer |
| Daemon + D-Bus-Schnittstelle | Hintergrundauswahl (`galleond --preview`) und `org.galleoncore.Deck` (`SetWallpaper`, `ClearWallpaper`, `Wallpapers`) |
| Tastenseiten (JSON) und Musikplayer (MPRIS) | Implementiert, Tests grün; **Tasten, Drehregler, der Bildschirm „Läuft gerade“ (echtes Cover, Titel, Interpret, Zeitleiste) und eine Anwendungstaste mit ihrem Symbol auf dem Modul bestätigt** (Hot-Reload, Doppeldruck, Video-Vorschaubilder und das Anwendungsfenster sind noch nicht zurückgemeldet) |
| KDE-Plasma-Widgets | Grundgerüst |
| RGB-Beleuchtung | **Experimentell.** Eine nicht persistente Corsair-V2/Bragi-Sequenz für RGB pro Taste ist für eine Tastatur unter Windows 11 dokumentiert (Tastatur-Firmware 2.3.88). GalleonCore enthält noch keinen nativen Treiber für RGB pro Taste; Linux-Unterstützung und weitere Geräte- und Firmwarevarianten sind noch nicht bestätigt. |
| OpenRGB-Plugin | Drehregler-Ringe im separaten Plugin umgesetzt; RGB der Tastatur bleibt in Untersuchung |
| Windows | Desktop-Vorschau; native CI und interaktive Validierung werden in [WINDOWS.md](docs/WINDOWS.md) verfolgt |

### Screenshots von Galleon Bosun

Die für 0.2.0 aktualisierten Aufnahmen zeigen die fünf Tastenseiten neben Hintergrundbildern, Widgets, Gaming und Bildschirmschonern. Installierte Anwendungen und ihre Symbole unterscheiden sich je nach Rechner; Live-Werte hängen vom Aufnahmerechner ab. [Die vollständige Galerie auf der Website öffnen](https://galleoncore.xyz/#previews).

| Steuerung | Multimedia |
|---|---|
| ![Bosun-Seite Steuerung mit Medientasten](site/screenshots/control.png) | ![Bosun-Seite Multimedia mit Browsern und Mediaplayern](site/screenshots/multimedia.png) |
| Spiele | System |
| ![Bosun-Seite Spiele mit installierten Spielen](site/screenshots/games.png) | ![Bosun-Seite System mit nützlichen Werkzeugen](site/screenshots/system.png) |
| Ziffernblock | Hintergrundbild-Auswahl |
| ![Bosun-Ziffernblock](site/screenshots/numpad.png) | ![Bosun-Sammlung von Hintergrundbildern](site/screenshots/wallpapers.png) |
| Widgets: Layout und Textgröße | Gaming: Spiel und Aufnahme |
| ![Integrierter Tab Widgets in Bosun](site/screenshots/widgets.png) | ![Integrierter Tab Gaming in Bosun](site/screenshots/gaming.png) |
| Bildschirmschoner-Auswahl | |
| ![Bosun-Auswahl des Bildschirmschoners mit den beiden mitgelieferten Videos](site/screenshots/screensavers.png) | |


### Widgets und Gaming in Bosun

- **Widgets**: lokale Uhr und Datum, Wetter deiner Stadt, CPU/RAM, CPU-Temperatur, unvollständige Downloads und FPS. Bearbeite Position, Abmessungen, Textgröße und Sichtbarkeit in der Vorschau und speichere mit **Anwenden**.
- **Gaming**: lokale Spielgrafik, FPS, Auslastungsanzeigen für CPU/GPU und verfügbare Temperaturen. Der Bildschirmschoner des Moduls wird während der Erfassung ausgesetzt. Die Pad-Steuerung erscheint nur, solange das Spiel aktiv ist, und verschwindet beim Beenden, auch nach einem Wechsel der Tastenseite.
- **Aufnahme**: eine gespeicherte Wahl zwischen Automatisch, OBS oder Game Bar. „Automatisch“ nutzt OBS unter Linux und Game Bar unter Windows. OBS arbeitet über WebSocket mit bestätigten Zuständen; Game Bar nutzt Tastenkombinationen und behauptet keinen unbestätigten Zustand. Siehe die [Aufnahme-Anleitung](docs/GAMING_RECORDING.md).
- **FPS**: Unter Windows wird der installierte PresentMon-Dienst erkannt und über seine API erfasst, ohne seine Oberfläche zu öffnen. Unter Linux gibt es eine Schaltfläche für die MangoHud-Konfiguration und Anleitungen für Steam, Heroic, Lutris, PlayOnLinux und Bottles; eine erste Einrichtung des Launchers bleibt nötig. Siehe die [Linux-Anleitung (auf Französisch)](docs/MODE_JEU_LINUX.fr.md).

Die Sensoren hängen von Hardware und Treibern ab: NVML für NVIDIA, AMD-SMI/hwmon unter Linux; ADLX ist unter Windows noch nicht integriert. Downloads werden über die Zahl unvollständiger Dateien erfasst, nicht über die Fortschrittsanzeige des Browsers. Siehe die [Plattformgrenzen](docs/HOME_WIDGETS.md).

[Die Demonstration zu 0.2.0 auf YouTube ansehen](https://youtu.be/eFwru_OMNNI).

### Update-Prüfung (0.2.1) und Systembenachrichtigungen (0.2.3)

Die [Update-Prüfung](docs/UPDATES.md) von Bosun prüft stabile GitHub-Releases und öffnet
den passenden Download in deinem Browser. Automatische Prüfungen lassen sich abschalten.
Nutzer von PPA/COPR erhalten keine doppelte Meldung, wenn ein eingerichtetes Repository
GalleonCore bereitstellt; eigenständig installierte DEB/RPM-Pakete behalten die Benachrichtigungen. Die Prüfung gibt es seit 0.2.1; 0.2.3 ergänzt Systembenachrichtigungen unter Linux
und Windows. Mit `--simulate-update` siehst du eine gekennzeichnete Update-Benachrichtigung vorab,
ohne eine Release zu veröffentlichen. Siehe die [Versionshinweise 0.2.3](docs/RELEASE_0.2.3.md).

### Windows: Stream-Deck-Eingabe reagiert nicht

Beim Galleon 100 SD des Eigentümers wurden die Bilder aktualisiert, doch die LCD-Tasten und Drehregler
reagierten nicht, bis das Modul zurückgesetzt wurde. Ein Update der Stream-Deck-Firmware mit der
offiziellen Elgato-Stream-Deck-App unter **Windows** und die anschließende Deinstallation dieser App stellten
die Eingabe im nächsten Test des Eigentümers wieder her. Beide Änderungen wurden gemeinsam vorgenommen, daher
lässt sich nicht sagen, welche davon das Problem bei anderen Geräten behebt.

Wenn du dasselbe Symptom siehst, notiere zuerst die Firmware-Version. Schließe
GalleonCore und andere Apps, die das Modul steuern. Öffne in der Elgato-Stream-Deck-
App unter Windows oder macOS **Einstellungen → Geräte → Galleon 100 SD →
Firmware aktualisieren**. Lasse die Tastatur angeschlossen, bis das Update abgeschlossen ist.
Notiere die neue Version, schließe Stream Deck und starte GalleonCore unter Windows
nach einem Kaltstart von Windows erneut. Wenn du Stream Deck nur für diese Tastatur nutzt, kannst du es
nach dem Firmware-Update deinstallieren; lasse es installiert, wenn ein anderes Gerät
es braucht. Der Screenshot des Eigentümers zeigt
Version `3.06.005`, die [Elgato in den Versionshinweisen zu 7.4.1 aufführt](https://edge.elgato.com/egc/sd_content/releasenotes/7.4.1.html).
Das Testverfahren unter Windows und seine verbleibenden Grenzen stehen in [WINDOWS.md](docs/WINDOWS.md).

Die **Firmware der Haupttastatur** ist davon getrennt: Corsair verweist auf das
[Corsair Web Hub](https://www.corsair.com/web-hub) in einem kompatiblen Browser unter
**Geräteeinstellungen**. Es muss keine App installiert oder deinstalliert werden. Das
GIF unten zeigt das Update des *Stream-Deck-Moduls* in der Elgato-App, nicht dieses
Update der Tastatur.

![Geräteeinstellungen der Elgato-Stream-Deck-App mit der Steuerung für das Galleon-Firmware-Update](site/firmware-update.gif)

## Architektur

```
lib/               libgalleon, C++20/Qt-6-Bibliothek (LGPL-3.0-or-later)
  Protocol         reine Funktionen: Bytes ↔ Ereignisse und Berichte, keine E/A
  Transport        rohe HID-E/A (hidapi), in Tests durch eine Attrappe ersetzbar
  StreamDeck       Keep-alive, Eingabeabfrage, Qt-Signale
daemon/            galleond: steuert das Deck, stellt eine D-Bus-API bereit
widgets/           Gemeinsame QML-Widgets, ohne Plasma-Abhängigkeit
plasmoid/          KDE-Plasma-Widgets, die widgets/ wiederverwenden
tests/             Qt-Test-Unit-Tests
fuzz/              libFuzzer-Ziele für die Protokoll-Parser
openrgb-plugin/    OpenRGB-Plugin für die Drehregler-Ringe
```

Weil das Protokoll frei von E/A ist, lässt sich jedes Byteformat ohne die Tastatur per Unit-Test prüfen und fuzzen.

Der RGB-Controller selbst liegt nicht hier: Er wird in einem Fork von OpenRGB entwickelt und als Merge Request für den `CorsairPeripheralV2Controller` vorgeschlagen, damit jeder OpenRGB-Nutzer davon profitiert.

## Bauen

Optional: `ffmpeg` (Bilder aus Videos) und `gio` (Anwendungen starten).

Voraussetzungen: CMake ≥ 3.21, ein C++20-Compiler (GCC ≥ 11 oder Clang ≥ 14), Qt 6 (Core, Gui, Qml, Quick, DBus), hidapi (hidraw-Backend).

Unter Debian/Ubuntu:

```sh
sudo apt install cmake g++ qt6-base-dev qt6-declarative-dev qt6-websockets-dev libhidapi-dev pkg-config
cmake -B build
cmake --build build
ctest --test-dir build
```

CMake-Optionen:

| Option | Standard | Wirkung |
|---|---|---|
| `BUILD_TESTING` | `ON` | Die Unit-Tests bauen |
| `GALLEON_WERROR` | `OFF` | Warnungen als Fehler behandeln |
| `GALLEON_SANITIZE` | `OFF` | AddressSanitizer + UndefinedBehaviorSanitizer |
| `GALLEON_FUZZ` | `OFF` | libFuzzer-Ziele (nur Clang) |

Damit dein Benutzer ohne Root auf die Tastatur zugreifen kann:

```sh
sudo cp tools/udev/70-galleon.rules /etc/udev/rules.d/
sudo udevadm control --reload && sudo udevadm trigger
```

## Codequalität

Jeder Push und jeder Pull Request führt in GitHub Actions Folgendes aus:

- einen Build mit strengen Warnungen als Fehler, mit GCC und mit Clang;
- die Unit-Tests, auch unter AddressSanitizer und UndefinedBehaviorSanitizer;
- 60 Sekunden Fuzzing des Parsers für Eingabeberichte;
- clang-tidy, qmllint und `reuse lint` für die Lizenzierung.

So lässt sich der Sanitizer-Lauf lokal nachstellen:

```sh
CXX=clang++ cmake -B build-asan -DGALLEON_SANITIZE=ON -DGALLEON_FUZZ=ON
cmake --build build-asan
ctest --test-dir build-asan
./build-asan/fuzz/fuzz_input_report -max_total_time=60
```

## Wie dieses Projekt entsteht

Der größte Teil des Codes wird mit KI-Coding-Agenten (Claude Code und Codex) geschrieben, unter
Anleitung des Maintainers und nach den Vorgaben in
[CLAUDE.md](CLAUDE.md). Der Maintainer besitzt die Tastatur, legt die Funktionen fest,
testet auf der echten Hardware, prüft die vorgeschlagenen Änderungen und genehmigt deren
Übernahme.

Generierter Code wird nicht ohne Überprüfung akzeptiert. Die Projektregeln verlangen, dass
Protokollfakten aus zitierten Quellen oder echten Mitschnitten stammen, Gerätedaten vor der Verwendung
validiert werden und Verhaltensänderungen mit Tests einhergehen. Die Statustabellen
in dieser README und in [ROADMAP.md](ROADMAP.md) unterscheiden „getestet“ (automatische
Tests) von „am Modul bestätigt“ (manuelle Prüfung auf der Hardware).

Pull Requests durchlaufen automatisierte Prüfungen: Kompilierung, Tests und statische
Analyse. Diese Prüfungen sind öffentlich; sie fangen einige Fehler ab und stellen sicher, dass die
Projektregeln eingehalten werden. Sie garantieren keine Fehlerfreiheit, aber ihr Bestehen ist
Voraussetzung für die Übernahme in den geschützten Branch `main`.

Wenn etwas nicht stimmt, eröffne ein Issue; am hilfreichsten sind Meldungen mit Firmware-Version,
Distribution und Log.

## Hintergrundbild und Uhr auf dem Bildschirm

`galleond` zeichnet eines der Bilder aus [`public/wallpaper/`](public/wallpaper/) mit der Uhrzeit darüber auf den Bildschirm des Galleon (720 × 384) und zeichnet jede Minute neu.

```sh
./build/daemon/galleond --preview                         # Fenster zur Auswahl eines Hintergrundbilds
./build/daemon/galleond --wallpaper manga-cyberpunk/manga-cyberpunk-01.jpg
./build/daemon/galleond --wallpaper cinematic/cinematic-02.jpg --render-to screen.png   # ohne Hardware
busctl --user call org.galleoncore.Deck /org/galleoncore/Deck org.galleoncore.Deck SetWallpaper s cinematic/cinematic-02.jpg
```

Es können nur Bilder gewählt werden, die im Hintergrundbild-Ordner liegen, und jedes wird geprüft (echtes JPEG oder PNG, Größe, kein symbolischer Link). Die letzte Auswahl wird gespeichert. Der Ordner ist `--wallpapers`, `$GALLEONCORE_WALLPAPERS`, `<Datenverzeichnis>/galleoncore/wallpaper` oder der Quellbaum.

## Tastenseiten

Die 12 Tasten und die 2 Drehregler führen die Aktionen einer JSON-Datei aus, `~/.config/galleoncore/pages.json` (oder `--pages Datei`). Bei einem neuen Linux- oder Windows-Profil heißen die fünf eingebauten Tabs Contrôle, Multimédia, Jeu Vidéo, Système und Pavé Numérique. Multimedia bevorzugt Browser und Player, System schlägt Administrationswerkzeuge vor, und Videospiele beginnt mit installierten Steam-Spielen. Die Vorschläge werden einmal ausgewählt, mit höchstens 12 Tasten pro Seite; unpassende oder nicht verfügbare Launcher lassen ihre Plätze leer. Das Entfernen oder Ändern einer Taste in Bosun füllt oder ordnet sie nicht neu. Bestehende persönliche Seiten bleiben unverändert. Die Datei wird neu gelesen, wenn sie sich ändert; eine fehlerhafte Datei wird als Ganzes abgelehnt und die bisherigen Seiten bleiben erhalten (die Fehler stehen im Log).

```json
{
  "version": 1,
  "home": "main",
  "pages": {
    "main": { "keys": [
      { "key": 0, "label": "Previous", "icon": "media-skip-backward", "action": { "type": "media", "command": "previous" } },
      { "key": 1, "label": "Play", "icon": { "playing": "media-playback-pause", "paused": "media-playback-start" },
        "action": { "type": "media", "command": "play-pause" } },
      { "key": 11, "label": "More", "action": { "type": "page", "page": "other" } }
    ] },
    "other": { "keys": [ { "key": 0, "label": "Back", "action": { "type": "back" } } ] }
  },
  "dials": { "0": { "turn": { "type": "media", "command": "volume", "step": 2 }, "press": { "type": "media", "command": "play-pause" } } }
}
```

- Die Tasten sind von 0 bis 11 nummeriert, Zeile für Zeile von oben links. Eine Taste ohne `action` tut nichts.
- `icon`: ein Name aus dem Symbolthema des Desktops oder eine Datei neben der Seitendatei (`.png`, `.jpg`, `.svg`; kein Pfad). Es kann ein Objekt mit einem Namen je Playerzustand sein (`playing`, `paused`, `stopped`).
- Aktionen: `media` (`play-pause`, `next`, `previous`, `stop`, `shuffle`, `repeat` (none, playlist, track), `mute` (der Systemton, für jeden Player), `raise` (das Fenster des Players nach vorn holen), `fullscreen` (ein Videoplayer), `restart` (zurück zum Anfang des Titels), `volume` mit `step` in Prozent, `seek` mit `seconds`). Eine Taste `shuffle` oder `repeat` zeigt ihren Zustand: Füge `"state_labels": ["off", "on"]` (shuffle) oder `["none", "list", "track"]` (repeat) hinzu, um die Wörter zu wählen; die Taste ist heller, solange die Funktion aktiv ist. Außerdem `page` (eine Seite öffnen), `back`, `app` (eine Anwendung starten, siehe unten), `none`. Ein Drehregler-`turn` akzeptiert `volume` oder `seek`, pro Raststufe einmal angewandt.
- Der Musikplayer wird über MPRIS gefunden: der gerade spielende, sonst der zuletzt gespielte.
- Aktionen stammen ausschließlich aus dieser Datei. Ein Programm lässt sich nur über eine `app`-Taste starten, die eine `.desktop`-Datei benennt: In die Datei kann kein Befehl geschrieben werden.
- `galleond --render-keys keys.png` zeichnet die Tasten der ersten Seite ohne die Hardware in ein Bild.

### Anwendungen auf den Tasten

Eine Taste kann eine Anwendung deines Desktops starten, ohne dass du einen Befehl eintippen musst: Sie benennt die `.desktop`-Datei der Anwendung, und deren Name und Symbol werden für die Taste verwendet, sofern du keine eigenen angibst.

```json
{ "key": 4, "action": { "type": "app", "desktop": "firefox.desktop" } }
```

`desktop` ist eine Desktop-Datei-ID (`firefox.desktop`, `org.kde.dolphin.desktop`) oder der absolute Pfad einer `.desktop`-Datei. Der Starter des Desktops selbst startet sie (`gio launch`, sonst `gtk-launch`, sonst `kioclient`), sodass `Exec`, Argumente und `Terminal=true` so behandelt werden, wie es der Desktop tut. Ein Doppeldruck startet die Anwendung einmal. Aus Sicherheitsgründen muss die Seitendatei dir gehören und darf nicht für andere beschreibbar sein, sonst werden Anwendungstasten deaktiviert.

Dafür musst du das JSON nie bearbeiten:

```sh
./build/daemon/galleond --apps                              # ein Fenster: Anwendung wählen, Taste wählen
./build/daemon/galleond --list-apps --filter vlc            # die installierten Anwendungen
./build/daemon/galleond --assign-app vlc.desktop --key 4    # eine auf eine Taste legen (--page NAME für eine andere Seite)
./build/daemon/galleond --clear-key 4
```

**Galleon Bosun** ist die Einstellungsanwendung: Du kannst die 12 Tasten jedes Themes bearbeiten, Themes verwalten, Hintergrundbilder importieren und zuschneiden, Video-Bildschirmschoner einrichten sowie Startbildschirm-Widgets und Spieleinstellungen anpassen. Der Tab „Tastatur“ zeigt Firmware-Informationen und Links zu den offiziellen Werkzeugen; einen nativen Treiber für RGB pro Taste bietet er nicht. Bosun öffnet das HID-Gerät nie selbst: Den Hardwarezugriff übernimmt `galleond`.

```sh
./build/bosun/galleon-bosun
```

Die Änderung wird wie jede Seitendatei geprüft, die vorherige Version bleibt als `pages.json.bak` erhalten, und ein laufender `galleond` übernimmt die Änderung selbstständig.

### Themes

Eine Liste `themes` gibt Seiten einen Namen und eine Farbe. Ein Druck auf den Drehregler mit `{ "type": "theme" }` schaltet diesen Drehregler auf die Theme-Auswahl: Drehen blättert durch die Themes, und ein zweiter Druck kehrt zur Player-Steuerung zurück (nach 30 Sekunden ohne Drehung geschieht das von selbst). Die Tasten zeigen die Seite des Themes, eine Linie in der Farbe des Themes verläuft mit seinem Namen am oberen Bildschirmrand, und der Ring um den linken Drehregler nimmt die Farbe an (der rechte Ring bleibt weiß, für die Player-Steuerung). Während das Theme gewählt wird, nimmt auch die Linie am unteren Bildschirmrand die Farbe des Themes an. Auf einer Taste schaltet dieselbe Aktion zum nächsten Theme weiter.

```json
"themes": [
  { "name": "Control", "page": "main", "color": "#ffffff" },
  { "name": "Sound", "page": "sound", "color": "#e53935" }
],
"dials": { "0": { "press": { "type": "theme" } } }
```

Die Ringfarben verwenden den Feature-Report des Stream-Deck-Protokolls pro LED (`03 24 <led> <r> <g> <b>`, aus rust-elgato-streamdeck PR 63). Die Farben der Drehregler-Ringe wurden am 19. Oktober 2026 am Modul des Projekteigentümers bestätigt; das bestätigt nicht die RGB-Steuerung einzelner Tasten der Haupttastatur. Höchstens 8 Themes; die Farbe hat die Form `#rrggbb`; die Seite eines Themes muss existieren.

### Hintergrundbild mit einem Drehregler wählen

Die Hintergrundbilder sind nach Stil und dann nach Dateiname in natürlicher Zahlenfolge geordnet (`-2` vor `-10`). Jede angenommene Raststufe des Drehreglers wählt das nächste oder vorherige Hintergrundbild. Weitere Drehungen werden verworfen und nicht vorgemerkt, bis dieses Bild angezeigt wird und anschließend weitere `screen.wallpaper_step_delay_ms` verstrichen sind (Standard: 150 ms; 0 deaktiviert diese Begrenzung, sodass jede Raststufe zählt). Der Bildschirm zeigt den Namen und die Position des ausgewählten Hintergrundbilds (`portrait-01   10/24`).

Ein Druck auf den Drehregler mit `{ "type": "wallpaper-mode" }` schaltet diesen Drehregler von der Player-Steuerung auf die Hintergrundauswahl: Drehen blättert durch die Hintergrundbilder des Katalogs (gespeichert, auch während eines laufenden Titels angezeigt), und ein zweiter Druck bestätigt und kehrt zurück. Drehungen werden ignoriert, bis der Bildschirm Namen und Platz des Hintergrundbilds zeigt. Nach der letzten Drehung wird ein Countdown von `screen.wallpaper_validate_seconds` (Standard 4) in einem Kreis angezeigt, während der Drehregler in der Auswahl bleibt (magentafarbener Ring, eine Drehung startet ihn neu); wenn er endet, wird die Auswahl bestätigt: Der Drehregler ist wieder die Player-Steuerung (weißer Ring und weiße Linie), und das gewählte Hintergrundbild bleibt `screen.wallpaper_hold_seconds` Sekunden auf dem Bildschirm, bevor ein laufender Titel ihn zurückerobert. Eine dünne Linie am unteren Bildschirmrand und der Ring des rechten Drehreglers zeigen den Modus: weiß für die Player-Steuerung, magenta für die Hintergrundauswahl.

### Keypad-Theme, automatische Themes und getönte Tasten

Eine Aktion `{ "type": "key", "key": "kp7" }` drückt eine Tastaturtaste über ein virtuelles Keypad (`/dev/uinput`), gehalten, solange die Taste gehalten wird. Es wird nur eine feste Liste von Namen akzeptiert (`kp0` bis `kp9`, `kpdot`, `kpenter`, `kpplus`, `kpminus`, `kpasterisk`, `kpslash`, `numlock`, `backspace`), und das virtuelle Gerät erhält keine anderen Tasten. Num Lock muss aktiv sein, damit die Ziffern des Keypads Ziffern tippen. Das virtuelle Gerät braucht Schreibzugriff auf `/dev/uinput` (`tools/udev/71-galleon-uinput.rules`).

Ein Theme kann sich mit `"when": "media"` (ein Player startet) oder `"when": "no-media"` (keiner ist mehr übrig: gestoppt oder nicht vorhanden; ein pausierter Player zählt weiterhin als einer) selbst wählen. Nur in diesen Momenten: Deine eigene Wahl mit dem Drehregler bleibt bis zur nächsten Änderung bestehen. Höchstens ein Theme pro Auslöser. Die Tasten der Seite eines Themes werden auf einem dunkleren Farbton der Themefarbe gezeichnet, heller, solange sie gehalten werden.

Der Abschnitt `screen` enthält außerdem `wallpaper_hold_seconds` (1 bis 600, Standard 4): wie lange ein mit dem rechten Drehregler gewähltes oder beim Start angezeigtes Hintergrundbild auf dem Bildschirm bleibt, bevor ein laufender Titel ihn zurückerobert.

### Meistgenutzte Anwendungen und Steam-Spiele

Eine Seite kann mit `"sort": "usage"` verlangen, dass ihre Anwendungstasten nach Nutzung geordnet werden: dieselben Plätze, die meistgenutzte Anwendung auf dem ersten, markiert durch einen kleinen goldenen Stern. Jeder Start vom Modul aus zählt 1 und halbiert sich alle 30 Tage, sodass eine aktuelle Gewohnheit eine alte schlägt. Die Zähler liegen in `~/.local/share/galleoncore/usage.json` (höchstens 200 Anwendungen, atomar geschrieben).

```json
"jeux": {
  "sort": "usage",
  "steam": { "max": 6 },
  "keys": [ { "key": 0, "action": { "type": "app", "desktop": "steam.desktop" } } ]
}
```

`"menu": { "category": "Game", "max": N }` füllt die freien Tasten der Seite mit bis zu N Anwendungen dieser Kategorie des Desktop-Menüs (die freedesktop-`Categories` jeder `.desktop`-Datei; `Game` für die Spiele des KDE-Menüs, egal womit gestartet: KDE-Spiele, Lutris, Heroic, Wine, Steam-Verknüpfungen …), die meistgenutzten zuerst. Eine Menüverknüpfung, die ein Steam-Spiel startet (`steam steam://rungameid/<n>`), wird anhand der Spielzeit und des letzten Starts dieses Spiels aus den lokalen Steam-Daten bewertet, auch wenn das Spiel hier nicht installiert ist. Werkzeuge (Proton, die Steam-Runtimes) und alles, was schon auf der Seite steht, werden ausgelassen, und eine Steam-Spielverknüpfung und dasselbe Spiel aus Steams eigener Liste werden nur einmal angezeigt.

`"steam": { "max": N }` füllt die freien Tasten der Seite mit bis zu N installierten Steam-Spielen, die meistgespielten zuerst. Es liest die eigenen Dateien von Steam auf diesem Rechner, nur lesend und ohne Netzwerk, Konto oder Schlüssel: die Spielzeit und das Datum des letzten Starts jedes Spiels (`userdata/<id>/config/localconfig.vdf`) sowie den Namen jedes installierten Spiels (`appmanifest_*.acf` in den Bibliotheksordnern). Werkzeuge wie Proton und die Steam-Runtimes werden ausgelassen. Ein Steam-Spiel erhält als Wertung seine gespielten Stunden, abgeschwächt durch die Zeit seit dem letzten Start, plus seine Starts vom Modul aus. Ein einzelnes Spiel kann auch eine eigene Taste sein: `{ "type": "steam", "appid": 730 }` startet es über `steam steam://rungameid/730`.

### Animierter Bildschirmschoner

Nach einer Zeit ohne Nutzung kann ein Hochformat-Video gemeinsam über den Bildschirm und die 12 Tasten laufen, zugeschnitten, als sähe man das Bild durch sie hindurch (die Lücken zwischen den Tasten bleiben ausgespart). Jede Taste oder jeder Drehregler oder ein startender Titel (außer `while_playing` ist true) bringt den normalen Bildschirm zurück; der Tastendruck, der das Modul weckt, bewirkt sonst nichts.

```json
"screensaver": {
  "video": "screensaver.mp4",
  "idle_minutes": 5,
  "fps": 15,
  "while_playing": false,
  "geometry": {
    "screen_mm": [W, H],
    "key_mm": [W, H],
    "gap_mm": [X, Y],
    "screen_to_keys_mm": D
  }
}
```

Die Maße sind die des Moduls, in Millimetern, mit einem Lineal auf dem sichtbaren Glas gemessen (der Bildschirm, eine Taste, die Lücken zwischen den Tasten und der Abstand vom unteren Rand des Bildschirms zur Oberkante der ersten Tastenreihe). Sie werden nie geschätzt: Ohne `geometry` bleibt der Bildschirmschoner aus. Das Modul gilt als zentriert: Bildschirm und Tastenraster teilen sich eine senkrechte Achse. Das Video wird so skaliert, dass es das Bild abdeckt (`720` Pixel breit, im Verhältnis so hoch wie das Modul) und läuft in einer Schleife; wähle dieses Format, zum Beispiel `720 x 1100`, damit nichts abgeschnitten wird. Für eine nahtlose Schleife soll es auf seinem ersten Bild enden. Der Videopfad ist absolut, `~/...` oder relativ zur Seitendatei. `"video"` kann auch eine Liste sein (bis zu 8), jeder Eintrag ein Pfad oder `{ "file": "...", "title": "Anzeigename" }`: Die Videos wechseln sich ab, eines pro Start des Bildschirmschoners, und der Titel wird ins Log geschrieben. FFmpeg dekodiert es in einem eigenen Prozess (keine Shell, niedrigste Priorität, kein Ton, nur das `file`-Protokoll); das Modul schafft etwa 20 Durchgänge aus Bildschirm plus Tasten pro Sekunde (gemessen), daher geht `fps` bis 20, und 15 ist komfortabel.

Videos, die du auf deine eigene Tastatur legst, bleiben auf deinem Rechner. Ein Video, das diesem Repository hinzugefügt wird, folgt [ASSETS.md](ASSETS.md).

### „Läuft gerade“ auf dem Bildschirm

Während ein Player spielt, zeigt der Bildschirm anstelle des Hintergrundbilds den Titel (Cover, Titel, Interpret, Zeit und einen Fortschrittsbalken) und kehrt zum Hintergrundbild zurück, wenn die Musik endet. Das steuert ein optionaler Abschnitt `screen` der Seitendatei:

```json
"screen": { "now_playing": "playing", "remote_covers": true, "video_thumbnails": true }
```

- `now_playing`: `playing` (Standard), `playing-or-paused` oder `never`.
- `video_thumbnails`: `false`, um nie mit FFmpeg ein Bild aus einem Video zu erzeugen.
- `remote_covers`: `false`, um Cover nur aus lokalen Dateien zu laden. Als `https://` angegebene Cover lädt `galleond` herunter; er lädt nie von `http://`, von einer IP-Adresse oder von einem lokalen Namen herunter, begrenzt Größe, Zeit und Weiterleitungen und dekodiert nur JPEG, PNG und WebP.
- Das Cover ist, in dieser Reihenfolge: das vom Player gelieferte, ein Bild neben der gespielten Datei (`cover.jpg`, `folder.png`, `front.jpg`, `albumart.jpg`, `album.jpg` oder eines, das wie die Datei benannt ist), ein mit FFmpeg aus dem Video erzeugtes Bild, aufbewahrt in `~/.cache/galleoncore/thumbnails` (höchstens 300 Bilder oder 100 MB), das Symbol des Players (der VLC-Kegel), dann eine Note. Ein Titel ohne Titelangabe zeigt seinen Dateinamen.
- Der Bildschirm folgt dem Player sofort: Titel- und Statusänderungen kommen als D-Bus-Benachrichtigungen an, und nur der Fortschrittsbalken wird einmal pro Sekunde gelesen.
- `galleond --render-now-playing screen.png --wallpaper cinematic/cinematic-02.jpg` zeichnet einen Beispielbildschirm ohne die Hardware.

## Hinweise zur Hardware

| Teil | USB-ID |
|---|---|
| Tastatur (RGB, Tasten) | `1b1c:2b0c` |
| Stream-Deck-Modul | `1b1c:2b18` |

Protokollnotizen und Mitschnitte gehören nach [`docs/protocol/`](docs/protocol/). Bilder und Videos, die dem Repository hinzugefügt werden, folgen [`ASSETS.md`](ASSETS.md).

## Dank

GalleonCore baut auf dem Reverse Engineering auf, das geleistet wurde von:

- [node-elgato-stream-deck](https://github.com/Julusian/node-elgato-stream-deck) (MIT), das in v7.5.0 die Galleon-Unterstützung hinzugefügt hat;
- [galleon-deck](https://github.com/NLMP-DDHS/galleon-deck) (MIT), einem Python-Stream-Deck-Dienst für das Galleon unter Linux;
- [opendeck-galleon](https://github.com/shanelord01/opendeck-galleon), einem OpenDeck-Plugin für das Galleon.

## Lizenz

- `lib/` (libgalleon): **LGPL-3.0-or-later**, damit andere Projekte die Protokollbibliothek wiederverwenden können.
- Anwendung, Widgets, Werkzeuge und Dokumentation: **GPL-3.0-or-later**.
- Hintergrundbilder und Videos des Repositorys: **CC-BY-SA-4.0**, mit Herkunftsnachweis (siehe [ASSETS.md](ASSETS.md)).

Die vollständigen Texte liegen in [`LICENSES/`](LICENSES/). Die Dateien tragen SPDX-Header nach der [REUSE](https://reuse.software)-Konvention.

## Pakete und Community

Wähle dein Betriebssystem im [Download-Bereich der Website](https://galleoncore.xyz/#downloads)
für direkte Pakete für Windows, Ubuntu, Fedora, Arch und als AppImage. Die
[Release v0.1.0](https://github.com/Paullux/GalleonCore/releases/tag/v0.1.0)
erklärt, wie du das PPA, COPR oder das AUR-Repository hinzufügst und was jede Datei enthält.
Details zu Build und Paketierung stehen in der [Distributionsanleitung](tools/packaging/README.md).
Das GitHub-Download-Badge zählt Release-Dateien, keine Installationen über PPA/COPR/AUR.
Die [Website](https://galleoncore.xyz/#downloads) zeigt kumulierte
direkte Paket-Downloads je Plattform. Mit aktiviertem optionalem externem Inhalt
liest sie beim Öffnen der Seite aktuelle Zahlen von GitHub; andernfalls verwendet sie den letzten
täglichen Schnappschuss. Tägliche Basiswerte liefern außerdem ungefähre Änderungen über sieben Tage.
PPA- und COPR-Downloads werden täglich um 03:23 UTC erfasst; die aktuelle Gesamtzahl
umfasst GitHub, PPA und COPR über ein vollständiges Siebentagefenster, sofern verfügbar, andernfalls über ein vollständiges Tagesfenster.
Der AUR veröffentlicht Stimmen und Popularität, keine Download-Zahlen. Diese Zahlen zählen
Übertragungen (einschließlich Updates), keine eindeutigen Nutzer.

[Mitwirken](CONTRIBUTING.md) · [Verhaltenskodex](CODE_OF_CONDUCT.md) ·
[Sicherheit](SECURITY.md) · [Lizenzen](LICENSE.md).
