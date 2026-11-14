# Política de firma de código

**Borrador. No publicado.** Véase `docs/signing/README.md`. Sustituir cada `[TO CONFIRM: ...]` antes de usarlo.
**Borrador automático: que lo lea una persona hispanohablante nativa antes de publicarlo.**

GalleonCore (el demonio `galleond`, la aplicación de ajustes Galleon Bosun, el programa vigilante `galleon-keyboard-watchdog` y
el instalador de Windows) es software libre bajo las licencias GPL-3.0-or-later y LGPL-3.0-or-later. Esta página explica cómo se
firman sus binarios de Windows.

## Atribución

[TO CONFIRM: usar la redacción exacta y el enlace que exijan las condiciones actuales de SignPath.]
Firma de código gratuita proporcionada por [SignPath.io](https://signpath.io), certificado de la [SignPath Foundation](https://signpath.org).

## Qué se firma

Solo binarios construidos a partir del código fuente de este proyecto, en el repositorio público
`https://github.com/Paullux/GalleonCore`, mediante su compilación automática (`.github/workflows/windows-core.yml`):
`galleond.exe`, `galleon-bosun.exe`, `galleon-keyboard-watchdog.exe` y el instalador. Las bibliotecas de terceros incluidas en el
paquete (por ejemplo Qt) no se firman con el certificado de este proyecto, salvo que se recompilen aquí. No se firma nada que el
proyecto no haya construido.

## Funciones

- **Autores y committers:** Paul Woisard (propietario del repositorio). [TO CONFIRM: otros committers.]
- **Revisores** de los cambios de personas que no son committers: [TO CONFIRM: nombre o «el propietario»].
- **Aprobadores** de cada solicitud de firma (cada versión se aprueba a mano antes de firmarse): [TO CONFIRM: nombre].

Toda persona con acceso de escritura al repositorio o al servicio de firma usa autenticación de varios factores.

## Política de privacidad

Este programa no transferirá información a otros sistemas en red salvo que el usuario o la persona que lo instala o lo opera lo
solicite expresamente. Los programas hacen las siguientes peticiones, y solo como se describe:

- **Comprobación de actualizaciones** (Galleon Bosun): consulta `api.github.com` para saber cuál es la última versión del proyecto,
  por sí sola a intervalos por defecto y cuando el usuario pulsa el botón. No envía nada sobre el usuario o el equipo más allá de lo
  que lleva cualquier petición web (la dirección del equipo). Se desactiva en el panel de Actualizaciones de Bosun. [TO CONFIRM
  según el código: el interruptor existe y está desactivado en las instalaciones gestionadas por un gestor de paquetes.]
- **Widget del tiempo** (galleond): solo después de que el usuario introduzca un lugar; envía la latitud y la longitud de ese lugar
  a `api.open-meteo.com`.
- **Ilustraciones de juegos** (galleond): solo después de que el usuario introduzca una clave de SteamGridDB; envía el título o el
  identificador de Steam de un juego a `steamgriddb.com`.

Los archivos de configuración permanecen en la carpeta del usuario. El proyecto no envía a ningún sitio datos de uso,
identificadores ni registros.

## Cambios en el sistema

Los programas solo escriben en el perfil del usuario. El instalador puede iniciar Galleon Bosun y el demonio al iniciar sesión
mediante la clave de registro `Run` del usuario; se anuncia en el instalador y lo elimina el desinstalador. En Linux, los paquetes
instalan una regla udev que da al usuario el teclado y el módulo Stream Deck. La iluminación en vivo, experimental, está
desactivada por defecto, nunca escribe en la memoria del teclado y está protegida por un proceso vigilante que devuelve las teclas
si el demonio termina.

## Desinstalación

Usar «Aplicaciones» en los ajustes de Windows, o ejecutar el desinstalador de la carpeta de instalación. Los archivos del usuario
(páginas, temas, fondos, curvas) se conservan salvo que los borre.

## Informar de un problema

[TO CONFIRM: dirección de contacto o el seguimiento de incidencias `https://github.com/Paullux/GalleonCore/issues`.] Un binario
firmado que haga algo que esta página no dice es un defecto y se comunicará a SignPath.
