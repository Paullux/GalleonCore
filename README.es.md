<p align="center"><img src="site/logo.svg" alt="" width="96" height="96"></p>

# GalleonCore

Bosun incluye inglés, francés, alemán y español. El menú del globo permite seguir el idioma del sistema o cambiar al instante; la elección se recuerda. Consulta el [mantenimiento de las traducciones](docs/TRANSLATIONS.md).

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

🇬🇧 [Read in English](README.md) · 🇫🇷 [Lire en français](README.fr.md) · 🇩🇪 [Auf Deutsch lesen](README.de.md) · *En caso de diferencias, prevalece la versión en inglés.*

🌐 [Sitio web](https://galleoncore.xyz/)

Para seguir el estado de desarrollo del proyecto, consulta [ROADMAP.md](ROADMAP.md) (en francés).

**La plataforma abierta para el Corsair Galleon 100 SD en Linux.**

El Galleon 100 SD es un teclado mecánico de tamaño completo cuyo teclado numérico se sustituye por un módulo Elgato Stream Deck: 12 teclas LCD, una pantalla y dos diales. Corsair y Elgato solo ofrecen compatibilidad con el módulo Stream Deck en Windows y macOS. GalleonCore pretende que cada parte del teclado se pueda usar en Linux e integrarla con el escritorio.

> **Desarrollo activo.** Las teclas, la pantalla y los diales del Stream Deck funcionan en Linux, y Galleon Bosun sirve para configurar el módulo. Los widgets de Plasma siguen previstos. Hay disponible para pruebas interactivas una compilación de escritorio para Windows, con un instalador NSIS con la imagen de Bosun y un ZIP portátil ([detalles](docs/WINDOWS.md)); consulta la [hoja de ruta](ROADMAP.md) para ver el estado detallado de la validación.

> GalleonCore no está afiliado a Corsair ni a Elgato.

> **0.3.0:** Bosun en inglés, francés, alemán y español, cambio de idioma instantáneo, pestañas adaptables animadas y conservación de los cambios sin guardar en los accesos directos. [Notas de la versión](docs/RELEASE_0.3.0.md).

## Objetivos

- **Módulo Stream Deck**: dibujar en las 12 teclas y en la pantalla, leer las teclas y los diales.
- **Widgets compartidos**: los mismos widgets QML (reloj, monitor del sistema, lo que está sonando…) se renderizan en la pantalla del teclado *y* se muestran como widgets de KDE Plasma en el escritorio.
- **Iluminación RGB**: RGB por tecla, barra de iluminación ambiental y anillos de los diales, a través de [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB).
- **Complemento de OpenRGB**: configurar el deck desde OpenRGB y sincronizar los temas del deck con la iluminación.
- **Vista previa de escritorio para Windows**, con el mismo núcleo y backends nativos de multimedia, audio y teclado; la validación en hardware sigue pendiente.

Los widgets de la pantalla de inicio se pueden editar en la pestaña **Widgets** de Bosun (posición, tamaño y visibilidad) y guardar con **Aplicar**. La primera iteración incluye el tiempo, CPU/RAM, la temperatura de la CPU en Linux, la detección de descargas parciales y los FPS del juego mediante el CSV de MangoHud en Linux y el servicio/API de PresentMon en Windows, con tamaño de texto ajustable; consulta las [plataformas compatibles y los límites actuales](docs/HOME_WIDGETS.md).
Una pantalla de juego automática muestra ahora la ilustración local del juego, los FPS y las temperaturas de CPU/GPU mientras la captura está activa, y mantiene desactivado el salvapantallas del teclado hasta que termina.

## Estado

| Parte | Estado |
|---|---|
| Protocolo del Stream Deck (teclas, pantalla, diales) | Keep-alive, brillo, imágenes de teclas y de pantalla implementados a partir del propio descriptor del módulo y del trabajo de la comunidad; imagen de pantalla, imágenes de teclas y entrada de teclas y diales **confirmadas en el hardware** |
| Renderizado QML sin ventana hacia el deck | Fondo de pantalla con la hora (`WallpaperClock.qml`), renderizador por software |
| Daemon + interfaz D-Bus | Selector de fondos (`galleond --preview`) y `org.galleoncore.Deck` (`SetWallpaper`, `ClearWallpaper`, `Wallpapers`) |
| Páginas de teclas (JSON) y reproductor de música (MPRIS) | Implementado, pruebas en verde; **teclas, diales, la pantalla «reproduciendo ahora» (carátula real, título, artista, línea de tiempo) y una tecla de aplicación con su icono confirmados en el módulo** (la recarga en caliente, la doble pulsación, las miniaturas de vídeo y la ventana de aplicaciones aún no se han comunicado) |
| Widgets de KDE Plasma | Esqueleto |
| Iluminación RGB | **Experimental.** Se ha documentado una secuencia Corsair V2/Bragi no persistente de RGB por tecla en un teclado con Windows 11 (firmware del teclado 2.3.88). GalleonCore aún no incluye un controlador nativo de RGB por tecla; el funcionamiento en Linux y la compatibilidad con otros dispositivos y firmwares siguen sin verificarse. |
| Complemento de OpenRGB | Anillos de los diales implementados en el complemento aparte; el RGB del teclado sigue en investigación |
| Windows | Vista previa de escritorio; la CI nativa y la validación interactiva se siguen en [WINDOWS.md](docs/WINDOWS.md) |

### Capturas de Galleon Bosun

Las capturas actualizadas para la 0.2.0 muestran las cinco páginas de teclas junto a Fondos de pantalla, Widgets, Gaming y Salvapantallas. Las aplicaciones instaladas y sus iconos varían según el equipo; las lecturas en directo dependen del equipo donde se hizo la captura. [Abre la galería completa en el sitio web](https://galleoncore.xyz/#previews).

| Control | Multimedia |
|---|---|
| ![Página Control de Bosun con teclas multimedia](site/screenshots/control.png) | ![Página Multimedia de Bosun con navegadores y reproductores](site/screenshots/multimedia.png) |
| Juegos | Sistema |
| ![Página Juegos de Bosun con los juegos instalados](site/screenshots/games.png) | ![Página Sistema de Bosun con herramientas útiles](site/screenshots/system.png) |
| Teclado numérico | Elección de fondo de pantalla |
| ![Teclado numérico de Bosun](site/screenshots/numpad.png) | ![Colección de fondos de pantalla de Bosun](site/screenshots/wallpapers.png) |
| Widgets: diseño y tamaño del texto | Gaming: juego y grabación |
| ![Pestaña Widgets integrada en Bosun](site/screenshots/widgets.png) | ![Pestaña Gaming integrada en Bosun](site/screenshots/gaming.png) |
| Elección de salvapantallas | |
| ![Selector de salvapantallas de Bosun con los dos vídeos incluidos](site/screenshots/screensavers.png) | |


### Widgets y Gaming en Bosun

- **Widgets**: reloj y fecha locales, el tiempo de tu ciudad, CPU/RAM, temperatura de la CPU, descargas parciales y FPS. Edita la posición, las dimensiones, el tamaño del texto y la visibilidad en la vista previa, y guarda con **Aplicar**.
- **Gaming**: ilustración local del juego, FPS, indicadores de carga de CPU/GPU y temperaturas disponibles. El salvapantallas del módulo se suspende durante la captura. Los controles del pad solo aparecen mientras el juego está activo y desaparecen al cerrarlo, incluso tras cambiar de página de teclas.
- **Grabación**: una elección guardada entre Automático, OBS o Game Bar. «Automático» usa OBS en Linux y Game Bar en Windows. OBS usa WebSocket con estados confirmados; Game Bar usa atajos de teclado sin afirmar un estado no confirmado. Consulta la [guía de grabación](docs/GAMING_RECORDING.md).
- **FPS**: en Windows, detección del servicio PresentMon instalado y captura a través de su API, sin abrir su interfaz. En Linux, un botón de configuración de MangoHud e instrucciones para Steam, Heroic, Lutris, PlayOnLinux y Bottles; sigue siendo necesaria una configuración inicial del lanzador. Consulta la [guía de Linux (en francés)](docs/MODE_JEU_LINUX.fr.md).

Los sensores dependen del hardware y de los controladores: NVML para NVIDIA, AMD-SMI/hwmon en Linux; ADLX aún no está integrado en Windows. Las descargas se detectan contando archivos parciales, no con los porcentajes de progreso del navegador. Consulta las [limitaciones por plataforma](docs/HOME_WIDGETS.md).

[Ver la demostración de la 0.2.0 en YouTube](https://youtu.be/eFwru_OMNNI).

### Comprobación de actualizaciones (0.2.1) y notificaciones del sistema (0.2.3)

El [comprobador de actualizaciones](docs/UPDATES.md) de Bosun revisa las versiones estables de GitHub y abre
la descarga adecuada en tu navegador. Las comprobaciones automáticas se pueden desactivar.
Los usuarios de PPA/COPR no reciben un aviso duplicado cuando un repositorio configurado suministra
GalleonCore; las instalaciones DEB/RPM independientes conservan las notificaciones. El comprobador existe desde la 0.2.1; la 0.2.3 añade notificaciones del sistema en Linux
y Windows. Usa `--simulate-update` para previsualizar una notificación de actualización con etiqueta
sin publicar una versión. Consulta las [notas de la versión 0.2.3](docs/RELEASE_0.2.3.md).

### Windows: la entrada del Stream Deck no responde

En el Galleon 100 SD del propietario, las imágenes se actualizaban pero las teclas LCD y los diales no
respondían hasta que se reiniciaba el módulo. Actualizar el firmware del Stream Deck con la
aplicación oficial Elgato Stream Deck en **Windows** y desinstalarla después restableció
la entrada en la siguiente prueba del propietario. Esos dos cambios se hicieron a la vez, así que la
prueba no determina cuál de los dos solucionó el problema en otros dispositivos.

Si te encuentras con el mismo síntoma, anota primero la versión del firmware. Cierra
GalleonCore y las demás aplicaciones que controlen el módulo. En la aplicación Elgato Stream Deck
para Windows o macOS, abre **Preferencias → Dispositivos → Galleon 100 SD →
Actualizar firmware**. Mantén el teclado conectado hasta que termine la actualización.
Anota la nueva versión, cierra Stream Deck y reinicia GalleonCore en Windows
para una prueba de arranque en frío. Si usas Stream Deck solo para este teclado, puedes
desinstalarlo tras la actualización del firmware; déjalo instalado si otro dispositivo
lo necesita. La captura del propietario muestra
la versión `3.06.005`, que [Elgato incluye en las notas de la versión 7.4.1](https://edge.elgato.com/egc/sd_content/releasenotes/7.4.1.html).
El procedimiento de prueba en Windows y sus límites pendientes están en [WINDOWS.md](docs/WINDOWS.md).

El **firmware del teclado principal** es independiente: Corsair remite a
[Corsair Web Hub](https://www.corsair.com/web-hub) en un navegador compatible, en
**Ajustes del dispositivo**. No requiere instalar ni desinstalar ninguna aplicación. El
GIF de abajo muestra la actualización del *módulo Stream Deck* en la aplicación de Elgato, no esta
actualización del teclado.

![Preferencias del dispositivo en Elgato Stream Deck con el control de actualización del firmware del Galleon](site/firmware-update.gif)

## Arquitectura

```
lib/               libgalleon, biblioteca C++20/Qt 6 (LGPL-3.0-or-later)
  Protocol         funciones puras: bytes ↔ eventos e informes, sin E/S
  Transport        E/S HID en bruto (hidapi), sustituible por un simulado en las pruebas
  StreamDeck       keep-alive, lectura de entradas, señales de Qt
daemon/            galleond: controla el deck, expone una API D-Bus
widgets/           Widgets QML compartidos, sin dependencia de Plasma
plasmoid/          Widgets de KDE Plasma que reutilizan widgets/
tests/             Pruebas unitarias con Qt Test
fuzz/              Objetivos de libFuzzer para los analizadores del protocolo
openrgb-plugin/    Complemento de OpenRGB para los anillos de los diales
```

Al mantener el protocolo libre de E/S, cada formato de bytes se puede probar con pruebas unitarias y fuzzing sin el teclado.

El controlador RGB en sí no vive aquí: se desarrollará en un fork de OpenRGB y se propondrá como merge request al `CorsairPeripheralV2Controller`, para que todos los usuarios de OpenRGB se beneficien.

## Compilación

Opcional: `ffmpeg` (imágenes a partir de vídeos) y `gio` (iniciar aplicaciones).

Requisitos: CMake ≥ 3.21, un compilador C++20 (GCC ≥ 11 o Clang ≥ 14), Qt 6 (Core, Gui, Qml, Quick, DBus), hidapi (backend hidraw).

En Debian/Ubuntu:

```sh
sudo apt install cmake g++ qt6-base-dev qt6-declarative-dev qt6-websockets-dev libhidapi-dev pkg-config
cmake -B build
cmake --build build
ctest --test-dir build
```

Opciones de CMake:

| Opción | Valor por defecto | Efecto |
|---|---|---|
| `BUILD_TESTING` | `ON` | Compila las pruebas unitarias |
| `GALLEON_WERROR` | `OFF` | Trata las advertencias como errores |
| `GALLEON_SANITIZE` | `OFF` | AddressSanitizer + UndefinedBehaviorSanitizer |
| `GALLEON_FUZZ` | `OFF` | Objetivos de libFuzzer (solo Clang) |

Para que tu usuario pueda acceder al teclado sin root:

```sh
sudo cp tools/udev/70-galleon.rules /etc/udev/rules.d/
sudo udevadm control --reload && sudo udevadm trigger
```

## Calidad del código

Cada push y cada pull request ejecuta, en GitHub Actions:

- una compilación con advertencias estrictas tratadas como errores, con GCC y con Clang;
- las pruebas unitarias, también con AddressSanitizer y UndefinedBehaviorSanitizer;
- 60 segundos de fuzzing sobre el analizador de informes de entrada;
- clang-tidy, qmllint y `reuse lint` para las licencias.

Para reproducir localmente la ejecución con sanitizers:

```sh
CXX=clang++ cmake -B build-asan -DGALLEON_SANITIZE=ON -DGALLEON_FUZZ=ON
cmake --build build-asan
ctest --test-dir build-asan
./build-asan/fuzz/fuzz_input_report -max_total_time=60
```

## Cómo se hace este proyecto

La mayor parte del código se escribe con agentes de programación con IA (Claude Code y Codex), bajo
la dirección del mantenedor y siguiendo las instrucciones de
[CLAUDE.md](CLAUDE.md). El mantenedor posee el teclado, define las funciones,
prueba en el hardware real, examina los cambios propuestos y aprueba su
integración.

El código generado no se acepta sin verificación. Las reglas del proyecto exigen que los
datos del protocolo procedan de fuentes citadas o de capturas reales, que los datos del dispositivo se
validen antes de usarlos y que los cambios de comportamiento vayan acompañados de pruebas. Las tablas de estado
de este README y de [ROADMAP.md](ROADMAP.md) distinguen «probado» (pruebas
automáticas) de «confirmado en el módulo» (verificación humana en el hardware).

Los pull requests pasan por comprobaciones automáticas: compilación, pruebas y análisis
estático. Estas comprobaciones son públicas; detectan algunos errores y verifican que se cumplen las
reglas del proyecto. No garantizan la ausencia de errores, pero superarlas es
obligatorio antes de integrar en la rama protegida `main`.

Si algo falla, abre una incidencia; los informes con la versión del firmware, la
distribución y un registro son los más útiles.

## Fondo de pantalla y reloj en la pantalla

`galleond` dibuja una de las imágenes de [`public/wallpaper/`](public/wallpaper/) en la pantalla del Galleon (720 × 384) con la hora encima, y la redibuja cada minuto.

```sh
./build/daemon/galleond --preview                         # ventana para elegir un fondo de pantalla
./build/daemon/galleond --wallpaper manga-cyberpunk/manga-cyberpunk-01.jpg
./build/daemon/galleond --wallpaper cinematic/cinematic-02.jpg --render-to screen.png   # sin hardware
busctl --user call org.galleoncore.Deck /org/galleoncore/Deck org.galleoncore.Deck SetWallpaper s cinematic/cinematic-02.jpg
```

Solo se pueden elegir las imágenes que están en la carpeta de fondos, y cada una se comprueba (JPEG o PNG reales, tamaño, sin enlace simbólico). Se recuerda la última elección. La carpeta es `--wallpapers`, `$GALLEONCORE_WALLPAPERS`, `<directorio de datos>/galleoncore/wallpaper` o el árbol de código fuente.

## Páginas de teclas

Las 12 teclas y los 2 diales ejecutan las acciones de un archivo JSON, `~/.config/galleoncore/pages.json` (o `--pages archivo`). En un perfil nuevo de Linux o Windows, las cinco pestañas integradas son Contrôle, Multimédia, Jeu Vidéo, Système y Pavé Numérique. Multimedia da prioridad a navegadores y reproductores, Sistema sugiere herramientas de administración y Videojuegos empieza con los juegos de Steam instalados. Las sugerencias se eligen una sola vez, con un máximo de 12 teclas por página; los lanzadores sin relación o no disponibles dejan sus huecos vacíos. Quitar o cambiar una tecla en Bosun no la rellena ni la reordena. Las páginas personales existentes se dejan tal cual. El archivo se vuelve a leer cuando cambia; un archivo con un error se rechaza por completo y se conservan las páginas anteriores (los errores quedan en el registro).

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

- Las teclas se numeran de 0 a 11, fila por fila desde arriba a la izquierda. Una tecla sin `action` no hace nada.
- `icon`: un nombre del tema de iconos del escritorio, o un archivo junto al archivo de páginas (`.png`, `.jpg`, `.svg`; sin ruta). Puede ser un objeto con un nombre por estado del reproductor (`playing`, `paused`, `stopped`).
- Acciones: `media` (`play-pause`, `next`, `previous`, `stop`, `shuffle`, `repeat` (none, playlist, track), `mute` (el sonido del sistema, para todos los reproductores), `raise` (traer al frente la ventana del reproductor), `fullscreen` (un reproductor de vídeo), `restart` (volver al principio de la pista), `volume` con `step` en porcentaje, `seek` con `seconds`). Una tecla `shuffle` o `repeat` muestra su estado: añade `"state_labels": ["off", "on"]` (shuffle) o `["none", "list", "track"]` (repeat) para elegir las palabras; la tecla se ve más clara mientras está activada. Además `page` (abrir una página), `back`, `app` (iniciar una aplicación, véase más abajo), `none`. Un `turn` de dial admite `volume` o `seek`, aplicado una vez por cada paso de giro.
- El reproductor de música se encuentra a través de MPRIS: el que está sonando, o si no el último que sonó.
- Las acciones proceden únicamente de este archivo. La única forma de iniciar un programa es una tecla `app`, que nombra un archivo `.desktop`: no se puede escribir ningún comando en el archivo.
- `galleond --render-keys keys.png` dibuja las teclas de la primera página en una imagen, sin el hardware.

### Aplicaciones en las teclas

Una tecla puede iniciar una aplicación de tu escritorio sin que tengas que escribir ningún comando: nombra el archivo `.desktop` de la aplicación, y se usan su nombre y su icono para la tecla, salvo que indiques los tuyos.

```json
{ "key": 4, "action": { "type": "app", "desktop": "firefox.desktop" } }
```

`desktop` es un ID de archivo de escritorio (`firefox.desktop`, `org.kde.dolphin.desktop`) o la ruta absoluta de un archivo `.desktop`. Lo inicia el propio lanzador del escritorio (`gio launch`, si no `gtk-launch`, si no `kioclient`), de modo que `Exec`, los argumentos y `Terminal=true` se tratan como lo hace el escritorio. Una doble pulsación inicia la aplicación una sola vez. Por seguridad, el archivo de páginas debe ser tuyo y no ser escribible por otros; de lo contrario, las teclas de aplicación se desactivan.

Para esto nunca necesitas editar el JSON:

```sh
./build/daemon/galleond --apps                              # una ventana: elegir una aplicación, elegir una tecla
./build/daemon/galleond --list-apps --filter vlc            # las aplicaciones instaladas
./build/daemon/galleond --assign-app vlc.desktop --key 4    # ponerla en una tecla (--page NOMBRE para otra página)
./build/daemon/galleond --clear-key 4
```

**Galleon Bosun** es la aplicación de ajustes: permite editar las 12 teclas de cada tema, gestionar los temas, importar y recortar fondos de pantalla, configurar salvapantallas de vídeo y ajustar los widgets de inicio y el modo de juego. La pestaña Teclado muestra información del firmware y enlaces a las herramientas oficiales; no ofrece un controlador nativo de RGB por tecla. Bosun nunca abre el dispositivo HID: `galleond` se encarga del acceso al hardware.

```sh
./build/bosun/galleon-bosun
```

La edición se comprueba como cualquier archivo de páginas, la versión anterior se conserva como `pages.json.bak`, y un `galleond` en ejecución lee el cambio por sí solo.

### Temas

Una lista `themes` da a las páginas un nombre y un color. Una pulsación de dial con `{ "type": "theme" }` cambia ese dial a la elección de tema: al girarlo se recorren los temas, y una segunda pulsación vuelve al control del reproductor (tras 30 segundos sin girar, vuelve por sí solo). Las teclas muestran la página del tema, una línea del color del tema recorre la parte superior de la pantalla con su nombre, y el anillo alrededor del dial izquierdo toma ese color (el anillo derecho se queda blanco, para el control del reproductor). Mientras se elige el tema, la línea de la parte inferior de la pantalla también toma el color del tema. En una tecla, la misma acción pasa al tema siguiente.

```json
"themes": [
  { "name": "Control", "page": "main", "color": "#ffffff" },
  { "name": "Sound", "page": "sound", "color": "#e53935" }
],
"dials": { "0": { "press": { "type": "theme" } } }
```

Los colores de los anillos usan el informe de características por LED del protocolo del Stream Deck (`03 24 <led> <r> <g> <b>`, del PR 63 de rust-elgato-streamdeck). Los colores de los anillos se confirmaron en el módulo del propietario del proyecto el 19 de octubre de 2026; esto no valida el RGB por tecla del teclado principal. Como máximo 8 temas; el color es `#rrggbb`; la página de un tema debe existir.

### Elegir el fondo de pantalla con un dial

Los fondos se ordenan por estilo y después por nombre de archivo, siguiendo el orden numérico natural (`-2` antes que `-10`). Cada paso de giro aceptado del dial selecciona el fondo siguiente o anterior. Los giros posteriores se ignoran, sin ponerse en cola, hasta que se muestra ese fondo y transcurre un intervalo adicional de `screen.wallpaper_step_delay_ms` (150 ms por defecto; 0 desactiva esta limitación para contar cada paso de giro). La pantalla muestra el nombre y la posición del fondo seleccionado (`portrait-01   10/24`).

Una pulsación de dial con `{ "type": "wallpaper-mode" }` cambia ese dial del control del reproductor a la elección de fondo: al girarlo se recorren los fondos del catálogo (recordado, mostrado incluso mientras suena una pista), y una segunda pulsación valida y vuelve. Los giros se ignoran hasta que la pantalla muestra el nombre y la posición del fondo. Tras el último giro se muestra en un círculo una cuenta atrás de `screen.wallpaper_validate_seconds` (por defecto 4) mientras el dial sigue en la elección (anillo magenta; un giro la reinicia); cuando termina, la elección se valida: el dial vuelve a ser el control del reproductor (anillo y línea blancos) y el fondo elegido se queda en la pantalla durante `screen.wallpaper_hold_seconds` segundos antes de que una pista en reproducción lo recupere. Una línea fina en la parte inferior de la pantalla y el anillo del dial derecho indican el modo: blanco para el control del reproductor, magenta para la elección de fondo.

### Tema de teclado numérico, temas automáticos y teclas tintadas

Una acción `{ "type": "key", "key": "kp7" }` pulsa una tecla del teclado a través de un teclado numérico virtual (`/dev/uinput`), mantenida mientras se mantenga la tecla. Solo se acepta una lista fija de nombres (`kp0` a `kp9`, `kpdot`, `kpenter`, `kpplus`, `kpminus`, `kpasterisk`, `kpslash`, `numlock`, `backspace`, `f13` a `f24`), y el dispositivo virtual no recibe ninguna otra tecla. Bloq Num debe estar activado para que los dígitos del teclado numérico escriban dígitos. El dispositivo virtual necesita acceso de escritura a `/dev/uinput` (`tools/udev/71-galleon-uinput.rules`).

Un tema puede elegirse solo con `"when": "media"` (empieza un reproductor) o `"when": "no-media"` (no queda ninguno: detenido o ausente; un reproductor en pausa sigue contando como uno). Solo en esos momentos: tu propia elección con el dial se mantiene hasta el siguiente cambio. Como máximo un tema por disparador. Las teclas de la página de un tema se dibujan sobre un tono más oscuro del color del tema, más claro mientras se mantienen pulsadas.

La sección `screen` también tiene `wallpaper_hold_seconds` (de 1 a 600, por defecto 4): cuánto tiempo permanece en la pantalla un fondo elegido con el dial derecho, o mostrado al inicio, antes de que una pista en reproducción lo recupere.

### Aplicaciones más usadas y juegos de Steam

Una página puede pedir que sus teclas de aplicación se ordenen por uso con `"sort": "usage"`: los mismos lugares, con la aplicación más usada en el primero, marcada con una pequeña estrella dorada. Cada inicio desde el módulo cuenta 1 y se reduce a la mitad cada 30 días, de modo que un hábito reciente gana a uno antiguo. Los contadores se guardan en `~/.local/share/galleoncore/usage.json` (como máximo 200 aplicaciones, escrito de forma atómica).

```json
"jeux": {
  "sort": "usage",
  "steam": { "max": 6 },
  "keys": [ { "key": 0, "action": { "type": "app", "desktop": "steam.desktop" } } ]
}
```

`"menu": { "category": "Game", "max": N }` rellena las teclas libres de la página con hasta N aplicaciones de esa categoría del menú del escritorio (las `Categories` de freedesktop de cada archivo `.desktop`; `Game` para los juegos del menú de KDE, independientemente del lanzador utilizado: juegos de KDE, Lutris, Heroic, Wine, accesos directos de Steam…), las más usadas primero. Un acceso directo del menú que inicia un juego de Steam (`steam steam://rungameid/<n>`) se puntúa según el tiempo de juego y el último inicio de ese juego en los datos locales de Steam, aunque el juego no esté instalado aquí. Las herramientas (Proton, los runtimes de Steam) y todo lo que ya esté en la página se omiten, y un acceso directo de un juego de Steam y el mismo juego de la lista propia de Steam se muestran una sola vez.

`"steam": { "max": N }` rellena las teclas libres de la página con hasta N juegos de Steam instalados, los más jugados primero. Lee los propios archivos de Steam en este equipo, en solo lectura y sin red, cuenta ni clave: el tiempo de juego y la fecha del último inicio de cada juego (`userdata/<id>/config/localconfig.vdf`), y el nombre de cada juego instalado (`appmanifest_*.acf` en las carpetas de biblioteca). Las herramientas como Proton y los runtimes de Steam se omiten. Un juego de Steam puntúa por sus horas jugadas, atenuadas por el tiempo transcurrido desde su último inicio, más sus inicios desde el módulo. Un solo juego también puede ser una tecla propia: `{ "type": "steam", "appid": 730 }` lo inicia mediante `steam steam://rungameid/730`.

### Salvapantallas animado

Tras un tiempo sin uso, un vídeo vertical puede reproducirse sobre la pantalla y las 12 teclas a la vez, recortado como si la imagen se viera a través de ellas (los huecos entre teclas se omiten). Cualquier tecla o dial, o el inicio de una pista (salvo que `while_playing` sea true), devuelve la pantalla normal; la pulsación que despierta el módulo no hace nada más.

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

Las medidas son las del módulo, en milímetros, tomadas con una regla sobre el cristal visible (la pantalla, una tecla, los huecos entre teclas y la distancia desde la parte inferior de la pantalla hasta la parte superior de la primera fila de teclas). Nunca se adivinan: sin `geometry`, el salvapantallas permanece desactivado. Se considera que el módulo está centrado: la pantalla y la cuadrícula de teclas comparten un eje vertical. El vídeo se escala para cubrir la imagen (`720` píxeles de ancho, y de alto lo que corresponda en proporción al módulo) y se reproduce en bucle; dale esa forma, por ejemplo `720 x 1100`, para que no se recorte nada. Para un bucle sin saltos, termínalo en su primer fotograma. La ruta del vídeo es absoluta, `~/...` o relativa al archivo de páginas. `"video"` también puede ser una lista (hasta 8), cada entrada una ruta o `{ "file": "...", "title": "Nombre mostrado" }`: los vídeos se turnan, uno por cada inicio del salvapantallas, y el título se escribe en el registro. FFmpeg lo decodifica en su propio proceso (sin shell, prioridad mínima, sin audio, solo el protocolo `file`); el módulo admite unas 20 vueltas de pantalla más teclas por segundo (medido), así que `fps` llega hasta 20 y 15 es cómodo.

Los vídeos que pongas en tu propio teclado se quedan en tu equipo. Un vídeo añadido a este repositorio sigue [ASSETS.md](ASSETS.md).

### Reproduciendo ahora en la pantalla

Mientras un reproductor está sonando, la pantalla muestra la pista (carátula, título, artista, tiempo y una barra de progreso) en lugar del fondo, y vuelve al fondo cuando la música se detiene. Esto lo controla una sección opcional `screen` del archivo de páginas:

```json
"screen": { "now_playing": "playing", "remote_covers": true, "video_thumbnails": true }
```

- `now_playing`: `playing` (por defecto), `playing-or-paused` o `never`.
- `video_thumbnails`: `false` para no crear nunca una imagen de un vídeo con FFmpeg.
- `remote_covers`: `false` para cargar las carátulas solo desde archivos locales. Las carátulas indicadas como `https://` las descarga `galleond`; nunca descarga desde `http://`, desde una dirección IP ni desde un nombre local, limita el tamaño, el tiempo y las redirecciones, y solo decodifica JPEG, PNG y WebP.
- La carátula es, por este orden: la que da el reproductor, una imagen junto al archivo en reproducción (`cover.jpg`, `folder.png`, `front.jpg`, `albumart.jpg`, `album.jpg` o una con el nombre del archivo), una imagen del vídeo hecha con FFmpeg y guardada en `~/.cache/galleoncore/thumbnails` (como máximo 300 imágenes o 100 MB), el icono del reproductor (el cono de VLC) y, por último, una nota musical. Una pista sin título muestra el nombre de su archivo.
- La pantalla sigue al reproductor al instante: los cambios de pista y de estado llegan como notificaciones de D-Bus, y solo la barra de progreso se lee una vez por segundo.
- `galleond --render-now-playing screen.png --wallpaper cinematic/cinematic-02.jpg` dibuja una pantalla de ejemplo sin el hardware.

## Notas sobre el hardware

| Parte | ID USB |
|---|---|
| Teclado (RGB, teclas) | `1b1c:2b0c` |
| Módulo Stream Deck | `1b1c:2b18` |

Las notas del protocolo y las capturas van en [`docs/protocol/`](docs/protocol/). Las imágenes y los vídeos añadidos al repositorio siguen [`ASSETS.md`](ASSETS.md).

## Créditos

GalleonCore se apoya en la ingeniería inversa realizada por:

- [node-elgato-stream-deck](https://github.com/Julusian/node-elgato-stream-deck) (MIT), que añadió la compatibilidad con el Galleon en la v7.5.0;
- [galleon-deck](https://github.com/NLMP-DDHS/galleon-deck) (MIT), un servicio de Stream Deck en Python para el Galleon en Linux;
- [opendeck-galleon](https://github.com/shanelord01/opendeck-galleon), un complemento de OpenDeck para el Galleon.

## Licencia

- `lib/` (libgalleon): **LGPL-3.0-or-later**, para que otros proyectos puedan reutilizar la biblioteca del protocolo.
- Aplicación, widgets, herramientas y documentación: **GPL-3.0-or-later**.
- Fondos de pantalla y vídeos del repositorio: **CC-BY-SA-4.0**, con su procedencia (véase [ASSETS.md](ASSETS.md)).

Los textos completos están en [`LICENSES/`](LICENSES/). Los archivos llevan cabeceras SPDX, siguiendo la convención de [REUSE](https://reuse.software).

## Paquetes y comunidad

Elige tu sistema operativo en la [sección de descargas del sitio web](https://galleoncore.xyz/#downloads)
para obtener paquetes directos para Windows, Ubuntu, Fedora, Arch y AppImage. La
[versión v0.1.0](https://github.com/Paullux/GalleonCore/releases/tag/v0.1.0)
explica cómo añadir el repositorio PPA, COPR o AUR y qué contiene cada archivo.
Los detalles de compilación y empaquetado están en la [guía de distribución](tools/packaging/README.md).
La insignia de descargas de GitHub cuenta archivos de las versiones, no instalaciones de PPA/COPR/AUR.
El [sitio web](https://galleoncore.xyz/#downloads) muestra las descargas
directas acumuladas de paquetes por plataforma. Con el contenido externo opcional activado,
lee las cifras actuales de GitHub al abrir la página; si no, usa la última
instantánea diaria. Las cifras base diarias también permiten estimar los cambios de siete días.
Las descargas de PPA y COPR se recopilan a diario a las 03:23 UTC; la cifra combinada reciente
incluye GitHub, PPA y COPR en una ventana completa de siete días cuando está disponible, o en una ventana diaria completa si no.
El AUR publica votos y popularidad, no cifras de descargas. Estas cifras cuentan
transferencias (incluidas las actualizaciones), no usuarios únicos.

[Contribuir](CONTRIBUTING.md) · [Código de conducta](CODE_OF_CONDUCT.md) ·
[Seguridad](SECURITY.md) · [Licencias](LICENSE.md).
