# Richtlinie zur Code-Signierung

**Entwurf. Nicht veröffentlicht.** Siehe `docs/signing/README.md`. Jedes `[TO CONFIRM: ...]` vor der Verwendung ersetzen.
**Maschineller Entwurf: vor der Veröffentlichung von einer Person mit Deutsch als Muttersprache lesen lassen.**

GalleonCore (der Daemon `galleond`, die Einstellungsanwendung Galleon Bosun, das Wächterprogramm `galleon-keyboard-watchdog`
und das Windows-Setup) ist freie Software unter den Lizenzen GPL-3.0-or-later und LGPL-3.0-or-later. Diese Seite beschreibt,
wie ihre Windows-Programme signiert werden.

## Namensnennung

[TO CONFIRM: den genauen Wortlaut und Link der aktuellen SignPath-Bedingungen übernehmen.]
Kostenlose Code-Signierung durch [SignPath.io](https://signpath.io), Zertifikat der [SignPath Foundation](https://signpath.org).

## Was signiert wird

Nur Programme, die aus dem Quellcode dieses Projekts im öffentlichen Repository `https://github.com/Paullux/GalleonCore` durch
dessen automatisierten Build (`.github/workflows/windows-core.yml`) erzeugt wurden: `galleond.exe`, `galleon-bosun.exe`,
`galleon-keyboard-watchdog.exe` und das Setup. Bibliotheken Dritter im Paket (zum Beispiel Qt) werden nicht mit dem Zertifikat
dieses Projekts signiert, es sei denn, sie werden hier aus dem Quellcode neu gebaut. Es wird nichts signiert, was das Projekt
nicht selbst gebaut hat.

## Rollen

- **Autoren und Committer:** Paul Woisard (Eigentümer des Repositorys). [TO CONFIRM: weitere Committer.]
- **Prüfende** für Änderungen von Personen, die keine Committer sind: [TO CONFIRM: Name oder „der Eigentümer“].
- **Freigebende** jeder Signieranfrage (jede Version wird vor dem Signieren von Hand freigegeben): [TO CONFIRM: Name].

Alle Personen mit Schreibzugriff auf das Repository oder den Signierdienst verwenden Multi-Faktor-Authentifizierung.

## Datenschutz

Dieses Programm überträgt keine Informationen an andere vernetzte Systeme, außer wenn die Nutzerin oder der Nutzer bzw. die
installierende oder betreibende Person dies ausdrücklich verlangt. Die folgenden Anfragen werden von den Programmen gestellt, und
nur wie beschrieben:

- **Update-Prüfung** (Galleon Bosun): fragt `api.github.com` nach der neuesten Version des Projekts, standardmäßig von selbst in
  Abständen und wenn die Nutzerin oder der Nutzer die Schaltfläche drückt. Sie sendet nichts über die Person oder den Rechner,
  außer dem, was jede Webanfrage enthält (die Adresse des Rechners). Sie lässt sich im Bereich „Updates“ von Bosun abschalten.
  [TO CONFIRM anhand des Codes: der Schalter existiert und ist bei paketverwalteten Installationen aus.]
- **Wetter-Widget** (galleond): nur nachdem ein Ort eingegeben wurde; es sendet Breiten- und Längengrad dieses Ortes an
  `api.open-meteo.com`.
- **Spiele-Artwork** (galleond): nur nachdem ein SteamGridDB-Schlüssel eingegeben wurde; es sendet den Titel oder die
  Steam-Kennung eines Spiels an `steamgriddb.com`.

Die Konfigurationsdateien bleiben im Ordner der Nutzerin oder des Nutzers. Das Projekt sendet nirgendwohin Nutzungsdaten,
Kennungen oder Protokolle.

## Änderungen am System

Die Programme schreiben nur in das eigene Profil. Das Setup kann Galleon Bosun und den Daemon bei der Anmeldung über den
Registrierungsschlüssel `Run` der Nutzerin oder des Nutzers starten; das wird im Setup angekündigt und vom Deinstallationsprogramm
entfernt. Unter Linux installieren die Pakete eine udev-Regel, die Tastatur und Stream-Deck-Modul der Nutzerin oder dem Nutzer
zugänglich macht. Die experimentelle Live-Beleuchtung ist standardmäßig aus, schreibt nie in den Speicher der Tastatur und wird
von einem Wächterprozess geschützt, der die Tasten zurückgibt, wenn der Daemon endet.

## Deinstallation

„Apps“ in den Windows-Einstellungen verwenden oder das Deinstallationsprogramm im Installationsordner starten. Eigene Dateien
(Seiten, Themen, Hintergründe, Kurven) bleiben erhalten, außer sie werden gelöscht.

## Ein Problem melden

[TO CONFIRM: Kontaktadresse oder Issue-Tracker `https://github.com/Paullux/GalleonCore/issues`.] Ein signiertes Programm, das
etwas tut, was diese Seite nicht nennt, ist ein Fehler und wird SignPath gemeldet.
