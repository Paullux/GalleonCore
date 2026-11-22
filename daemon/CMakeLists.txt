# SPDX-FileCopyrightText: 2026 Paul Woisard
# SPDX-License-Identifier: GPL-3.0-or-later

# The daemon's logic, as a library so that the tests can use it.
add_library(galleond_core STATIC
    RecordingIcons.h RecordingIcons.cpp
    RecordingController.h RecordingController.cpp
    GameCapture.h GameCapture.cpp
    GameMode.h GameMode.cpp
    GameInstallations.h GameInstallations.cpp
    GameArtworkResolver.h GameArtworkResolver.cpp
    FpsReader.h FpsReader.cpp
    HomeWidgets.h HomeWidgets.cpp
    FpsSetup.h FpsSetup.cpp
    LauncherCache.h LauncherCache.cpp
    AppsBackend.h AppsBackend.cpp
    CoverLoader.h CoverLoader.cpp
    KeyInjector.h PlatformBackends.h
    DesktopEntries.h
    WellKnownApps.h WellKnownApps.cpp
    PageOrder.h PageOrder.cpp
    StoredZip.h StoredZip.cpp
    ThemePackage.h ThemePackage.cpp
    ThemeInstallPlan.h ThemeInstallPlan.cpp
    ThemeInstaller.h ThemeInstaller.cpp
    ThemeExporter.h ThemeExporter.cpp
    PagesConfig.h PagesConfig.cpp
    InitialPages.h InitialPages.cpp
    PagesEditor.h PagesEditor.cpp
    PagesController.h PagesController.cpp
    ThemePreview.h ThemePreview.cpp
    WallpaperCatalog.h WallpaperCatalog.cpp
    ScreenRenderer.h ScreenRenderer.cpp
    TileRenderer.h TileRenderer.cpp
    ThemeIcon.h ThemeIcon.cpp
    VideoProbe.h VideoProbe.cpp
    VideoThumbnails.h VideoThumbnails.cpp
    ScreenController.h ScreenController.cpp
    SteamLibrary.h SteamLibrary.cpp
    UsageStats.h UsageStats.cpp
    Wiring.h Wiring.cpp
    ScreensaverSplitter.h ScreensaverSplitter.cpp
    ScreensaverController.h ScreensaverController.cpp
    MatrixTrigger.h MatrixTrigger.cpp
    MatrixRainController.h MatrixRainController.cpp
    BrightnessCurve.h BrightnessCurve.cpp
    KeyboardController.h KeyboardController.cpp
    AtomicFile.h AtomicFile.cpp
    KeyboardState.h KeyboardState.cpp
    ReplacedFileWatcher.h ReplacedFileWatcher.cpp
    BrightnessScheduler.h BrightnessScheduler.cpp
    LightingLevelScheduler.h LightingLevelScheduler.cpp
    LightingEnvironment.h
    PowerMessages.h PowerMessages.cpp
    TransportTrace.h TransportTrace.cpp
    LightingController.h LightingController.cpp LightingWorker.h
    ProcessWatchdog.h ProcessWatchdog.cpp
    KeyboardStatePublisher.h KeyboardStatePublisher.cpp
    AppearanceFile.h AppearanceFile.cpp
    LightingAppearanceApplier.h LightingAppearanceApplier.cpp
    RingSettings.h RingSettings.cpp
    LightingRestore.h LightingRestore.cpp
    NightMode.h NightMode.cpp
    NightController.h NightController.cpp
    MatrixRing.h MatrixRing.cpp
    RingSettingsApplier.h RingSettingsApplier.cpp
    LedPositions.h LedPositions.cpp
    WeatherPlace.h WeatherPlace.cpp
    SkySun.h SkySun.cpp
    InputActivity.h InputActivity.cpp
    LightingSkyDay.h LightingSkyDay.cpp
    StrictJson.h
    KeyMapFile.h KeyMapFile.cpp
    LightingState.h LightingState.cpp
    LightingStatePublisher.h LightingStatePublisher.cpp
)
target_include_directories(galleond_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(galleond_core PUBLIC galleon galleon_keyboard galleon_media galleon_keys galleon_platform Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::WebSockets PRIVATE galleon_warnings)
if(WIN32)
    target_sources(galleond_core PRIVATE WindowsDesktopEntries.cpp WindowsPowerEvents.h WindowsPowerEvents.cpp)
    target_link_libraries(galleond_core PUBLIC galleon_smtc PRIVATE shell32 ole32 uuid advapi32 user32 version)
    target_compile_definitions(galleond_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    target_sources(galleond_core PRIVATE QuitOnSignals.h QuitOnSignals.cpp LogindPowerEvents.h LogindPowerEvents.cpp LinuxKeyInjector.h LinuxKeyInjector.cpp LinuxPlatformBackends.cpp
        MprisController.h MprisController.cpp DesktopEntries.cpp)
    target_link_libraries(galleond_core PUBLIC Qt6::DBus)
endif()
set_target_properties(galleond_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The idle notification of a Wayland compositor (ext-idle-notify-v1): it ends the hidden green rain at the first action of the person on a session where
# the idle time is not given by D-Bus (KDE Plasma on Wayland). OPTIONAL: without wayland-client, wayland-scanner and the protocol file of the
# wayland-protocols package, galleond is built without it (the rain then ends by the keys and the dials of the module, by its time limit, or by the idle
# time that D-Bus gives on GNOME and on KDE with X11). The protocol is generated from the XML of the system at build time, never copied into the project.
set(GALLEON_WAYLAND_IDLE OFF CACHE INTERNAL "galleond is built with the Wayland idle notification")
if(NOT WIN32 AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
        pkg_check_modules(WAYLAND_PROTOCOLS QUIET wayland-protocols)
        find_program(GALLEON_WAYLAND_SCANNER wayland-scanner)
        if(WAYLAND_CLIENT_FOUND AND WAYLAND_PROTOCOLS_FOUND AND GALLEON_WAYLAND_SCANNER)
            pkg_get_variable(GALLEON_WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
            set(GALLEON_WAYLAND_IDLE_XML "${GALLEON_WAYLAND_PROTOCOLS_DIR}/staging/ext-idle-notify/ext-idle-notify-v1.xml" CACHE INTERNAL "")
            if(EXISTS "${GALLEON_WAYLAND_IDLE_XML}")
                set(GALLEON_WAYLAND_IDLE ON CACHE INTERNAL "galleond is built with the Wayland idle notification")
            endif()
        endif()
    endif()
endif()
if(GALLEON_WAYLAND_IDLE)
    enable_language(C)
    set(GALLEON_WAYLAND_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/wayland CACHE INTERNAL "")
    file(MAKE_DIRECTORY ${GALLEON_WAYLAND_GEN_DIR})
    set(_client_header ${GALLEON_WAYLAND_GEN_DIR}/ext-idle-notify-v1-client-protocol.h)
    set(_protocol_code ${GALLEON_WAYLAND_GEN_DIR}/ext-idle-notify-v1-protocol.c)
    add_custom_command(OUTPUT ${_client_header}
        COMMAND ${GALLEON_WAYLAND_SCANNER} client-header ${GALLEON_WAYLAND_IDLE_XML} ${_client_header}
        DEPENDS ${GALLEON_WAYLAND_IDLE_XML} VERBATIM)
    add_custom_command(OUTPUT ${_protocol_code}
        COMMAND ${GALLEON_WAYLAND_SCANNER} private-code ${GALLEON_WAYLAND_IDLE_XML} ${_protocol_code}
        DEPENDS ${GALLEON_WAYLAND_IDLE_XML} VERBATIM)
    # The generated code is C and is not ours: it is built apart from the warnings of the project.
    add_library(galleond_wayland_protocol STATIC ${_protocol_code} ${_client_header})
    target_include_directories(galleond_wayland_protocol SYSTEM PUBLIC ${GALLEON_WAYLAND_GEN_DIR})
    target_link_libraries(galleond_wayland_protocol PUBLIC PkgConfig::WAYLAND_CLIENT)
    set_target_properties(galleond_wayland_protocol PROPERTIES POSITION_INDEPENDENT_CODE ON)
    # The header must exist before the files of galleond_core are compiled, which linking the library alone does not ensure.
    add_custom_target(galleond_wayland_headers DEPENDS ${_client_header})
    add_dependencies(galleond_core galleond_wayland_headers)
    target_sources(galleond_core PRIVATE WaylandInputActivity.h WaylandInputActivity.cpp)
    target_link_libraries(galleond_core PUBLIC galleond_wayland_protocol)
    target_compile_definitions(galleond_core PUBLIC GALLEON_WAYLAND_IDLE=1)
    message(STATUS "galleond: the idle notification of Wayland (ext-idle-notify-v1) is built in")
else()
    message(STATUS "galleond: no idle notification of Wayland (wayland-client, wayland-scanner or wayland-protocols missing): the hidden rain ends without it")
endif()

# The positions of the LEDs (generated by tools/keyboard-map/build_map.py) go into the program: no install path to find on any system.
qt_add_resources(galleond_core "galleond_data"
    PREFIX "/galleond"
    BASE ${PROJECT_SOURCE_DIR}/data/devices
    FILES ${PROJECT_SOURCE_DIR}/data/devices/galleon-100-sd-led-positions.json
          ${PROJECT_SOURCE_DIR}/data/devices/galleon-100-sd-keymap.json
)

qt_add_resources(galleond_core "galleond_ui"
    PREFIX "/galleond"
    FILES qml/Preview.qml qml/Apps.qml
)

qt_add_executable(galleond main.cpp)
target_compile_definitions(galleond PRIVATE PROJECT_VERSION_STRING="${PROJECT_VERSION}")
if(GALLEON_SOURCE_TREE_PATHS)
    # Defaults for running from the build tree; an installed galleond looks in
    # <data dir>/galleoncore first.
    target_compile_definitions(galleond PRIVATE
        GALLEON_SOURCE_WIDGETS="${CMAKE_SOURCE_DIR}/widgets"
        GALLEON_SOURCE_WALLPAPERS="${CMAKE_SOURCE_DIR}/public/wallpaper")
endif()
target_link_libraries(galleond PRIVATE galleond_core galleon_warnings)
install(TARGETS galleond RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(WIN32)
    set_target_properties(galleond PROPERTIES WIN32_EXECUTABLE TRUE)
    configure_file(${PROJECT_SOURCE_DIR}/tools/packaging/windows/bosun-icon.rc.in ${CMAKE_CURRENT_BINARY_DIR}/bosun-icon.rc @ONLY)
    target_sources(galleond PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/bosun-icon.rc)
endif()

# Bundle the Gaming background in the daemon on Windows and Linux.
qt_add_resources(galleond_core "gaming_fallback"
    PREFIX "/galleon"
    BASE "${CMAKE_SOURCE_DIR}/public/wallpaper_gaming"
    FILES "${CMAKE_SOURCE_DIR}/public/wallpaper_gaming/galleoncore_gaming_fallback.png"
)

qt_add_resources(galleond_core "home_widgets"
    PREFIX "/homewidgets"
    BASE "${CMAKE_SOURCE_DIR}/widgets"
    FILES "${CMAKE_SOURCE_DIR}/widgets/HomeOverlay.qml" "${CMAKE_SOURCE_DIR}/widgets/GameScreen.qml" "${CMAKE_SOURCE_DIR}/widgets/CarGauge.qml"
)

# Driver calls are isolated so a stalled vendor library cannot block the display.
add_library(galleon_gpu_sensors STATIC GpuSensors.h GpuSensors.cpp)
target_include_directories(galleon_gpu_sensors PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(galleon_gpu_sensors PUBLIC Qt6::Core PRIVATE galleon_warnings)
if(WIN32)
    target_compile_definitions(galleon_gpu_sensors PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
qt_add_executable(galleon-gpu-probe GpuProbe.cpp)
target_link_libraries(galleon-gpu-probe PRIVATE galleon_gpu_sensors galleon_warnings)
install(TARGETS galleon-gpu-probe RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${PROJECT_SOURCE_DIR}/third_party/notices/AMD-SMI-MIT.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/galleoncore)

add_library(galleon_presentmon_api STATIC PresentMonService.h PresentMonService.cpp)
target_include_directories(galleon_presentmon_api PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(galleon_presentmon_api PUBLIC Qt6::Core PRIVATE galleon_warnings)
if(WIN32)
    target_compile_definitions(galleon_presentmon_api PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(galleon_presentmon_api PRIVATE advapi32)
endif()
target_link_libraries(galleond_core PUBLIC galleon_presentmon_api)
qt_add_executable(galleon-presentmon-probe PresentMonProbe.cpp)
target_link_libraries(galleon-presentmon-probe PRIVATE galleon_presentmon_api galleon_warnings)
if(WIN32)
    target_compile_definitions(galleon-presentmon-probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
install(TARGETS galleon-presentmon-probe RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${PROJECT_SOURCE_DIR}/third_party/notices/PresentMon-API-MIT.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/galleoncore)

qt_add_resources(galleond_core "fps_setup"
    PREFIX "/fpssetup"
    BASE "${PROJECT_SOURCE_DIR}"
    FILES "${PROJECT_SOURCE_DIR}/tools/fps_setup.py"
)
