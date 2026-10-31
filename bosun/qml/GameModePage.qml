// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
Flickable {
    id: page
    property bool manualLauncherHelpOpen: false
    objectName: "gameModePage"
    required property var widgets
    property bool advanced: false
    // The confirmed capture state also covers snapshots without the process flag.
    readonly property bool captureRunning: !!widgets.captureStatus.running || widgets.captureStatus.state === "capturing"
    readonly property bool captureReady: !!widgets.captureStatus.installed && widgets.captureStatus.state === "waiting" && !captureRunning && !widgets.captureStatus.starting
    Connections {
        target: page.widgets.fpsSetup
        function onCommonConfigured() {
            fpsSource.input.text = page.widgets.recommendedFpsFolder
            fpsProcess.input.text = ""
        }
    }
    FpsSetupDialog {
        id: fpsSetupDialog
        setup: page.widgets.fpsSetup
        transientParent: page.Window.window
    }
    contentWidth: width
    contentHeight: content.height
    clip: true
    component SetupText: Text {
        width: parent.width
        wrapMode: Text.Wrap
        color: "#b8bec6"
        font.pixelSize: 14
    }
    component SetupCode: TextEdit {
        width: parent.width
        readOnly: true
        selectByMouse: true
        wrapMode: TextEdit.Wrap
        color: "#80e5d1"
        font.pixelSize: 14
    }
    component SetupTitle: Text {
        color: "white"
        font.pixelSize: 18
        font.bold: true
    }
    component SetupDivider: Rectangle {
        width: parent.width
        height: 1
        color: "#44ffffff"
    }
    function applyEdits() {
        // Keep pending text before backend notifications refresh the bound fields.
        const source = fpsSource.input.text, process = fpsProcess.input.text
        const program = presentMon.input.text
        const key = gameProcess.input.text.trim().toLowerCase()
        const title = gameTitle.input.text, image = gameImage.input.text
        if (source !== (widgets.fpsConfig.source || "") || process !== (widgets.fpsConfig.process || "")) {
            if (!widgets.setFpsSource(source, process)) return
        }
        if (widgets.windowsCapture && program !== (widgets.gameConfig.presentMon || "")) {
            if (!widgets.setCaptureOptions(widgets.gameConfig.managedCapture !== false, program)) return
        }
        if (key !== "") {
            const existing = (widgets.gameConfig.profiles || {})[key] || {}
            if (title !== (existing.title || "") || image !== (existing.image || "")) {
                if (!widgets.setGameProfile(key, title, image)) return
            }
        }
        widgets.apply()
    }
    Column {
        id: content
        width: page.width
        spacing: 14
        Text { text: qsTr("Mode Jeu · 720 × 384"); color: "white"; font.pixelSize: 20; font.bold: true }
        Text {
            width: parent.width; wrapMode: Text.Wrap; color: "#c9ced6"
            text: page.widgets.windowsCapture
                ? qsTr("Start your Steam game: Galleon detects it and starts FPS capture automatically. Install the official PresentMon MSI once, with its shared service. Click Apply to save changes.")
                : qsTr("Automatic game artwork, FPS and CPU/GPU gauges. The keyboard screensaver stays off during an active capture. Click Apply to save changes.")
        }
        Row {
            spacing: 12
            Pill { objectName: "applyGameMode"; text: qsTr("Apply"); selected: page.widgets.pending; onClicked: page.applyEdits() }
            Pill {
                text: page.widgets.gameConfig.automatic !== false ? qsTr("Automatic: on") : qsTr("Automatic: off")
                onClicked: page.widgets.setGameOptions(page.widgets.gameConfig.automatic === false, !!page.widgets.gameConfig.localSensors)
            }
            Pill {
                text: page.widgets.gameConfig.localSensors ? qsTr("Local sensors: on") : qsTr("Local sensors: off")
                onClicked: page.widgets.setGameOptions(page.widgets.gameConfig.automatic !== false, !page.widgets.gameConfig.localSensors)
            }
            Text {
                height: 32; verticalAlignment: Text.AlignVCenter
                text: page.widgets.pending ? qsTr("Changes not applied") : qsTr("Settings applied")
                color: page.widgets.pending ? "#ffb74d" : "#9be7a0"
            }
        }
        Row {
            visible: page.widgets.windowsCapture
            spacing: 12
            Pill {
                objectName: "startPresentMon"
                text: page.captureRunning ? qsTr("Arrêter la capture FPS")
                    : page.widgets.captureStatus.starting ? qsTr("Démarrage de la capture FPS…")
                    : page.captureReady ? qsTr("Prêt — lancez un jeu")
                    : page.widgets.captureStatus.installed ? qsTr("Démarrer la capture FPS") : qsTr("Installer PresentMon")
                selected: page.captureRunning
                enabledLook: !page.captureReady && !page.widgets.captureStatus.starting && page.widgets.captureStatus.state !== "offline"
                onClicked: {
                    if (page.captureRunning) page.widgets.requestGameCapture(false)
                    else if (!page.widgets.captureStatus.installed) Qt.openUrlExternally("https://github.com/GameTechDev/PresentMon/releases/latest")
                    else page.widgets.requestGameCapture(true)
                }
            }
            Pill { text: page.advanced ? qsTr("Hide advanced settings") : qsTr("Advanced settings"); onClicked: page.advanced = !page.advanced }
        }
        Text {
            objectName: "gameCaptureStatus"
            visible: page.widgets.windowsCapture
            width: parent.width; wrapMode: Text.Wrap; color: "#aebac6"
            text: page.widgets.captureStatus.message ? qsTranslate("GameCapture", page.widgets.captureStatus.message) : qsTr("Waiting for the daemon…")
        }
        Text {
            width: parent.width; wrapMode: Text.Wrap; color: "#aebac6"
            text: page.widgets.gameState.active ? qsTr("Capture active: ") + page.widgets.gameState.title + " (" + page.widgets.gameState.process + ")"
                : qsTr("Waiting for a live FPS capture. Returns home after 10 seconds without fresh data.")
        }
        Text {
            width: parent.width; wrapMode: Text.Wrap; color: "#aebac6"
            visible: !!page.widgets.gameState.artworkStatus
            text: page.widgets.gameState.artworkStatus || ""
        }
        Rectangle {
            width: Math.min(720, page.width)
            height: width * 384 / 720
            color: "#101a25"; border.color: "#4b6574"
            Loader {
                anchors.fill: parent
                source: "qrc:/homewidgets/GameScreen.qml"
                onLoaded: item.stateData = Qt.binding(() => page.widgets.gameState)
            }
        }
        Row {
            visible: page.widgets.windowsCapture && page.advanced
            spacing: 12
            Pill {
                text: page.widgets.gameConfig.managedCapture !== false ? qsTr("Capture: automatic") : qsTr("Capture: manual CSV")
                onClicked: page.widgets.setCaptureOptions(page.widgets.gameConfig.managedCapture === false, presentMon.input.text)
            }
        }
        Field {
            id: presentMon; objectName: "presentMonPath"
            visible: false
            width: Math.min(720, page.width); maximumLength: 4096
            value: page.widgets.gameConfig.presentMon || ""
            placeholder: qsTr("PresentMon console path (optional; MSI detected automatically)")
            onEdited: text => page.widgets.setCaptureOptions(page.widgets.gameConfig.managedCapture !== false, text)
        }
        Text {
            visible: page.widgets.windowsCapture && page.advanced
            width: parent.width; wrapMode: Text.Wrap; color: "#aebac6"
            text: (page.widgets.captureStatus.program || "") + "\n" + (page.widgets.captureStatus.detail || "")
        }
        Text { visible: !page.widgets.windowsCapture || (page.advanced && page.widgets.gameConfig.managedCapture === false); text: qsTr("FPS capture source"); color: "white"; font.pixelSize: 18; font.bold: true }
        Row {
            visible: !page.widgets.windowsCapture || (page.advanced && page.widgets.gameConfig.managedCapture === false)
            spacing: 12
            Field {
                id: fpsSource; objectName: "gameFpsSource"
                width: Math.min(560, page.width * 0.5); maximumLength: 4096
                value: page.widgets.fpsConfig.source || ""
                placeholder: qsTr("FPS CSV file or log folder (absolute path)")
                onEdited: text => page.widgets.setFpsSource(text, fpsProcess.input.text)
            }
            Field {
                id: fpsProcess; maximumLength: 260
                value: page.widgets.fpsConfig.process || ""
                placeholder: qsTr("Game executable (optional)")
                onEdited: text => page.widgets.setFpsSource(fpsSource.input.text, text)
            }
        }
        Text {
            width: parent.width; wrapMode: Text.Wrap; color: "#aebac6"
            visible: !page.widgets.windowsCapture || page.advanced
            text: qsTr("MangoHud CSV on Linux, PresentMon --v1_metrics CSV on Windows. The source is shared with the home FPS widget. Local sensors optionally read LibreHardwareMonitor at 127.0.0.1:8085; missing measurements show a dash.")
        }
        Column {
            objectName: "linuxLauncherHelp"
            visible: !page.widgets.windowsCapture
            width: parent.width
            spacing: 14
            SetupTitle { text: qsTr("Linux · Préparer les FPS") }
            SetupText { text: qsTr("Faites la préparation commune une seule fois, puis suivez uniquement la section de votre lanceur. Galleon détectera ensuite le jeu à partir des mesures MangoHud.") }
            Column {
                objectName: "commonFpsHelp"
                width: parent.width; spacing: 8
                SetupTitle { text: qsTr("Commun · une seule fois") }
                SetupText { text: qsTr("1. Installez MangoHud avec le gestionnaire de logiciels de votre distribution. Pour un lanceur Flatpak, il faut aussi l’extension MangoHud correspondant à son runtime et les accès aux dossiers.") }
                SetupText { text: qsTr("2. Ouvrez Configuration unique ci-dessous. Cochez la préparation commune, les jeux ou réglages de lanceurs voulus et les permissions Flatpak nécessaires, puis Appliquer la sélection. Le compte rendu indique les modifications et les réglages personnels conservés.") }
                Pill {
                    objectName: "configureMangoHudButton"
                    text: qsTr("Configuration unique · jeux et lanceurs")
                    onClicked: fpsSetupDialog.open()
                }
                SetupText { text: qsTr("Configuration : ~/.config/MangoHud/MangoHud.conf (ou votre dossier XDG). Dossier des mesures : ") + page.widgets.recommendedFpsFolder }
                SetupText { text: qsTr("3. Activez Automatique en haut de cette page, puis Appliquer si vous avez changé ce réglage. Une fois votre lanceur configuré, relancez le jeu : les FPS et le mode Jeu doivent apparaître. Sans nouvelles mesures, le mode Jeu se ferme après environ 10 secondes.") }
                SetupText { text: qsTr("Une ancienne variable MANGOHUD_CONFIG ou une configuration propre au jeu peut remplacer le fichier commun. Vérifiez qu’elle utilise le même dossier de mesures. La boîte configure uniquement les éléments sélectionnés et affiche un compte rendu. Elle n’installe pas MangoHud.") }
            }
            Pill {
                objectName: "toggleManualLauncherHelp"
                activeFocusOnTab: true
                Keys.onSpacePressed: clicked()
                Keys.onReturnPressed: clicked()
                text: (manualLauncherHelp.visible ? "▼ " : "▶ ") + qsTr("Aides manuelles par lanceur")
                onClicked: page.manualLauncherHelpOpen = !page.manualLauncherHelpOpen
            }
            Column {
                id: manualLauncherHelp
                objectName: "manualLauncherHelp"
                visible: page.manualLauncherHelpOpen
                width: parent.width
                spacing: 14
                SetupDivider {}
                Column {
                    objectName: "flatpakFpsAccess"
                    width: parent.width; spacing: 8
                    SetupTitle { text: qsTr("Flatpak · autoriser les CSV FPS") }
                    SetupText { text: qsTr("Dans Configuration unique, cochez les lanceurs installés sous Permissions Flatpak pour autoriser la lecture de MangoHud.conf et l’écriture des CSV. L’extension MangoHud correspondant au runtime reste nécessaire. Quittez puis relancez les lanceurs après application.") }
                }
                SetupDivider {}
                Column {
                    objectName: "steamFpsHelp"
                    width: parent.width; spacing: 8
                    SetupTitle { text: "Steam" }
                    SetupText { text: qsTr("1. Bibliothèque → clic droit sur le jeu → Propriétés → Général → Options de lancement.") }
                    SetupText { text: qsTr("2. Collez cette ligne dans ce champ, sur une seule ligne (sélectionnez le texte puis Ctrl+C) :") }
                    SetupCode { objectName: "steamFpsLaunchCommand"; text: page.widgets.steamLaunchOptions }
                    SetupText { text: qsTr("3. Fermez les propriétés et lancez le jeu. Le réglage reste mémorisé pour les prochains lancements.") }
                    SetupText { text: qsTr("Cette commande appartient à Steam, jamais au terminal. Si des options existent déjà, conservez-les et ne dupliquez pas %command%. Steam Flatpak demande aussi l’extension MangoHud et les permissions décrites dans le guide Linux.") }
                }
                SetupDivider {}
                Column {
                    objectName: "heroicFpsHelp"
                    width: parent.width; spacing: 8
                    SetupTitle { text: "Heroic" }
                    SetupText { text: qsTr("1. Ouvrez les paramètres du jeu et activez MangoHud / Afficher les FPS dans les réglages Linux (l’emplacement dépend de la version).") }
                    SetupText { text: qsTr("2. Dans Configuration unique, sélectionnez les paramètres généraux pour les futurs jeux et chaque jeu déjà installé à configurer. Les réglages généraux seuls ne suffisent pas pour les jeux ayant leurs propres réglages. Ne mettez rien dans Arguments du jeu pour MangoHud.") }
                    SetupText { text: qsTr("Si vous devez définir les options par jeu : Paramètres du jeu → Avancé → Variables d’environnement. Nom : MANGOHUD_CONFIG. Valeur (sans guillemets supplémentaires) :") }
                    SetupCode { text: "autostart_log=1,log_interval=500,output_folder=" + page.widgets.recommendedFpsFolder + ",background_alpha=0,alpha=0" }
                    SetupText { text: qsTr("Heroic Flatpak : installez aussi l’extension MangoHud adaptée et autorisez le fichier de configuration et le dossier des mesures. La case seule ne donne pas ces accès ; voir le guide Linux.") }
                }
                SetupDivider {}
                Column {
                    objectName: "lutrisFpsHelp"
                    width: parent.width; spacing: 8
                    SetupTitle { text: "Lutris" }
                    SetupText { text: qsTr("1. Clic droit sur le jeu → Configurer → Options système → activez Compteur de FPS (MangoHud). Affichez les options avancées si nécessaire.") }
                    SetupText { text: qsTr("2. Avec la préparation commune, aucune commande ni variable par jeu n’est nécessaire. Enregistrez puis relancez le jeu. Ne mettez pas %command% dans Lutris.") }
                    SetupText { text: qsTr("Ancienne version sans cette case : ajoutez seulement mangohud dans Préfixe de commande. Pour des options par jeu : Options système → Variables d’environnement, nom MANGOHUD_CONFIG, valeur :") }
                    SetupCode { text: "autostart_log=1,log_interval=500,output_folder=" + page.widgets.recommendedFpsFolder + ",background_alpha=0,alpha=0" }
                }
                SetupDivider {}
                Column {
                    objectName: "polFpsHelp"
                    width: parent.width; spacing: 8
                    SetupTitle { text: "PlayOnLinux 4" }
                    SetupText { text: qsTr("1. Sélectionnez le raccourci du jeu → Configurer → Divers → Commande à exécuter avant de lancer le programme.") }
                    SetupText { text: qsTr("2. Ajoutez ces deux lignes dans ce champ, en conservant vos commandes existantes :") }
                    SetupCode {
                        objectName: "polFpsLaunchCommand"
                        text: "export " + page.widgets.steamLaunchOptions.replace(" mangohud %command%", "") + "\nexport BEFORE_WINE=\"mangohud${BEFORE_WINE:+ $BEFORE_WINE}\""
                    }
                    SetupText { text: qsTr("3. Fermez les réglages et relancez le jeu. Ce sont des lignes de pré-lancement, pas des arguments du jeu. Aucun %command% ici.") }
                }
                SetupDivider {}
                Column {
                    objectName: "bottlesFpsHelp"
                    width: parent.width; spacing: 8
                    SetupTitle { text: "Bottles" }
                    SetupText { text: qsTr("1. Ouvrez la bouteille du jeu → Préférences → Performances → activez Monitor Performance (MangoHud). Sur certaines versions, cette option est dans Affichage.") }
                    SetupText { text: qsTr("2. Avec la préparation commune, aucune commande de lancement n’est nécessaire. Relancez le jeu depuis la bouteille. N’ajoutez pas %command% à ses arguments.") }
                    SetupText { text: qsTr("Le bouton commun crée ces options dans MangoHud.conf ; elles sont affichées ici pour vérification, sans saisie supplémentaire nécessaire :") }
                    SetupCode { objectName: "bottlesFpsConfig"; text: "autostart_log=1\nlog_interval=500\noutput_folder=" + page.widgets.recommendedFpsFolder }
                    SetupText { text: qsTr("Bottles Flatpak : installez l'extension MangoHud correspondant au runtime, puis utilisez le bouton Autoriser les CSV dans la section Flatpak ci-dessus. Quittez et relancez Bottles après autorisation.") }
                }
                SetupDivider {}
                SetupText { text: qsTr("Besoin d’aide ? Le guide MODE_JEU_LINUX.fr.md détaille l’installation Flatpak, les autres activations Steam et les limites OpenGL / Vulkan / DXVK / VKD3D-Proton. Gamescope nécessite une configuration MangoApp distincte.") }
            }
        }
        SetupDivider {}
        Column {
            id: artworkGuide
            objectName: "gamingArtworkGuide"
            width: parent.width
            spacing: 10
            property int phase: 0
            readonly property bool playing: visible && (!Window.window || (Window.window.visible && Window.window.visibility !== Window.Minimized))
            readonly property int imageIndex: phase < 2 ? 0 : phase < 4 ? 1 : 2
            readonly property int secondsLeft: phase >= 6 && phase <= 9 ? 10 - phase : 0
            readonly property real dialAngle: phase < 2 ? 0 : phase < 4 ? 90 : 180
            readonly property string instruction: phase < 2 ? qsTr("Appuyez sur la molette de droite")
                : phase < 4 ? qsTr("Tournez la molette de droite pour changer d’image")
                : phase < 6 ? qsTr("Tournez encore pour afficher une troisième image")
                : phase < 10 ? qsTr("Attendez 4 secondes sans toucher à la molette")
                : qsTr("Image sauvée")
            onPlayingChanged: if (!playing) phase = 0
            Timer {
                objectName: "gamingArtworkGuideTimer"
                interval: 1000
                repeat: true
                running: artworkGuide.playing
                onTriggered: artworkGuide.phase = (artworkGuide.phase + 1) % 13
            }
            SetupTitle { text: qsTr("Choisir l’illustration en jeu"); width: parent.width; wrapMode: Text.Wrap }
            SetupText { text: qsTr("Démonstration · molette de droite du clavier") }
            Text {
                objectName: "gamingArtworkInstruction"
                width: parent.width
                text: artworkGuide.instruction
                wrapMode: Text.Wrap
                color: artworkGuide.phase >= 10 ? "#9be7a0" : "white"
                font.pixelSize: 16
                font.bold: true
            }
            Item {
                width: Math.min(480, parent.width)
                height: width * 0.54
                Item {
                    width: 480; height: 260
                    scale: parent.width / 480
                    transformOrigin: Item.TopLeft
                    Rectangle { x: 45; y: 4; width: 390; height: 250; radius: 18; color: "#242b35"; border.color: "#586473" }
                    Rectangle { x: 97; y: 23; width: 58; height: 58; radius: 29; color: "#475364"; border.color: "#91a1b4" }
                    Rectangle {
                        x: 310; y: 10; width: 84; height: 84; radius: 42
                        color: "transparent"; border.width: 4; border.color: "#df8ff4"
                        visible: artworkGuide.phase < 10
                    }
                    Rectangle {
                        x: 323; y: artworkGuide.phase === 1 ? 27 : 23
                        width: 58; height: 58; radius: 29
                        color: "#475364"; border.color: "#e5edf7"
                        rotation: artworkGuide.dialAngle
                        Behavior on rotation { NumberAnimation { duration: artworkGuide.phase === 0 ? 0 : 700; easing.type: Easing.InOutCubic } }
                        Rectangle { x: 27; y: 5; width: 4; height: 14; radius: 2; color: "#ffffff" }
                    }
                    Rectangle {
                        x: 63; y: 105; width: 354; height: 130; radius: 8
                        color: ["#205b87", "#683a85", "#a65a29"][artworkGuide.imageIndex]
                        clip: true
                        Canvas {
                            id: guideArt
                            anchors.fill: parent
                            property int imageIndex: artworkGuide.imageIndex
                            onImageIndexChanged: requestPaint()
                            onPaint: {
                                const ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                ctx.fillStyle = "#e8d5aa"
                                ctx.beginPath()
                                ctx.arc(imageIndex === 2 ? 100 : 285, 32, imageIndex === 2 ? 26 : 15, 0, Math.PI * 2)
                                ctx.fill()
                                ctx.fillStyle = ["#133c60", "#35204b", "#734225"][imageIndex]
                                ctx.beginPath()
                                if (imageIndex === 0) {
                                    ctx.moveTo(0, 130); ctx.lineTo(90, 32); ctx.lineTo(180, 108); ctx.lineTo(250, 55); ctx.lineTo(354, 130)
                                } else if (imageIndex === 1) {
                                    ctx.moveTo(0, 130)
                                    for (let i = 0; i < 8; ++i) {
                                        const top = 35 + (i % 3) * 18
                                        ctx.lineTo(i * 48, top); ctx.lineTo(i * 48 + 30, top); ctx.lineTo(i * 48 + 30, 130)
                                    }
                                } else {
                                    ctx.moveTo(0, 95); ctx.bezierCurveTo(100, 35, 210, 150, 354, 65); ctx.lineTo(354, 130)
                                }
                                ctx.closePath(); ctx.fill()
                            }
                        }
                        Text { x: 12; y: 8; text: qsTr("Illustration %1").arg(artworkGuide.imageIndex + 1); color: "white"; font.pixelSize: 16 }
                        Row {
                            x: 12; y: 76; spacing: 28
                            Text { text: "FPS  90"; color: "white"; font.pixelSize: 18; font.bold: true }
                            Text { text: "CPU  28 %"; color: "white"; font.pixelSize: 16 }
                            Text { text: "GPU  48 %"; color: "white"; font.pixelSize: 16 }
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 260; height: 46; radius: 8; color: "#e6242b35"
                            visible: artworkGuide.phase >= 6
                            Text {
                                anchors.centerIn: parent
                                text: artworkGuide.secondsLeft > 0 ? qsTr("Enregistrement dans %1 s").arg(artworkGuide.secondsLeft) : qsTr("Image sauvée")
                                color: artworkGuide.secondsLeft > 0 ? "white" : "#9be7a0"
                                font.pixelSize: 18; font.bold: true
                            }
                        }
                    }
                }
            }
            SetupText { text: qsTr("En jeu : appuyez sur la molette de droite, puis tournez pour parcourir les images SteamGridDB. Attendez 4 secondes sans la toucher pour enregistrer votre choix, ou appuyez de nouveau pour valider.") }
            SetupText { text: qsTr("L’image choisie remplace la précédente dans vos images personnalisées et sera réutilisée au prochain lancement du jeu. Une clé SteamGridDB configurée est nécessaire pour rechercher d’autres images.") }
        }
        SetupDivider {}
        Text { text: qsTr("SteamGridDB · horizontal game artwork (optional)"); color: "white"; font.pixelSize: 18; font.bold: true }
        Text {
            width: parent.width; wrapMode: Text.Wrap; color: "#aebac6"
            text: qsTr("Galleon first uses your manual artwork and local Steam images. Add your own SteamGridDB API key to download missing horizontal artwork; results are cached locally and work offline afterward. No key is bundled with Galleon.")
        }
        Row {
            spacing: 12
            Pill {
                text: qsTr("Get a personal API key")
                onClicked: Qt.openUrlExternally("https://www.steamgriddb.com/profile/preferences/api")
            }
            Text {
                color: page.widgets.steamGridDbConfigured ? "#9be7a0" : "#ffb74d"
                height: 32; verticalAlignment: Text.AlignVCenter
                text: page.widgets.steamGridDbConfigured ? qsTr("Key configured locally") : qsTr("No API key")
            }
        }
        Row {
            spacing: 12
            Field {
                id: steamGridDbKey
                objectName: "steamGridDbApiKey"
                width: Math.min(460, page.width * 0.58)
                maximumLength: 256
                password: true
                placeholder: qsTr("Your private SteamGridDB API key")
            }
            Pill {
                objectName: "saveSteamGridDbApiKey"
                text: qsTr("Save key")
                onClicked: {
                    if (page.widgets.setSteamGridDbApiKey(steamGridDbKey.input.text)) {
                        steamGridDbKey.input.text = ""
                        steamGridDbResult.text = qsTr("Key saved locally; new artwork is downloaded when a game is detected.")
                    } else steamGridDbResult.text = page.widgets.error
                }
            }
            Pill {
                objectName: "removeSteamGridDbApiKey"
                text: qsTr("Remove key")
                onClicked: {
                    if (page.widgets.setSteamGridDbApiKey("")) {
                        steamGridDbKey.input.text = ""
                        steamGridDbResult.text = qsTr("Key removed; cached images remain offline.")
                    } else steamGridDbResult.text = page.widgets.error
                }
            }
        }
        Row {
            spacing: 12
            Pill {
                objectName: "refreshSteamGridDbArtworks"
                text: qsTr("Clear cached artwork")
                onClicked: {
                    if (page.widgets.clearSteamGridDbArtworks())
                        steamGridDbResult.text = qsTr("Artworks removed from the cache; images can be fetched again during the next active game.")
                    else steamGridDbResult.text = page.widgets.error
                }
            }
        }
        Text { id: steamGridDbResult; width: parent.width; wrapMode: Text.Wrap; color: "#80e5d1" }
        Text {
            width: parent.width; wrapMode: Text.Wrap; color: "#aebac6"
            text: qsTr("The secret key is stored in a separate user-only configuration file, not in widgets.json. A refused key, rate limit, ambiguous game name or network error never prevents the bundled Gaming background from appearing. A newly saved key can immediately retry failed requests. Use Clear cached artwork to refresh downloaded images.")
        }
        Text { text: qsTr("Game artwork override (optional)"); color: "white"; font.pixelSize: 18; font.bold: true }
        Text {
            width: parent.width; wrapMode: Text.Wrap; color: "#aebac6"
            text: qsTr("Steam uses cached artwork when the process is identified. For non-Steam games on Windows, add the executable name here to enable detection. The title and image are optional. Click Apply.")
        }
        Row {
            spacing: 12
            Field { id: gameProcess; objectName: "gameProfileProcess"; placeholder: qsTr("Executable (e.g. cs2 or Game.exe)"); maximumLength: 260 }
            Field {
                id: gameTitle; objectName: "gameProfileTitle"; placeholder: qsTr("Game title"); maximumLength: 120
                value: ((page.widgets.gameConfig.profiles || {})[gameProcess.input.text.trim().toLowerCase()] || {}).title || ""
            }
        }
        Field {
            id: gameImage; objectName: "gameProfileImage"; width: Math.min(720, page.width); maximumLength: 4096
            placeholder: qsTr("Absolute local image path")
            value: ((page.widgets.gameConfig.profiles || {})[gameProcess.input.text.trim().toLowerCase()] || {}).image || ""
        }
        // Same Apply action as the button at the top of this page. Keep it
        // next to the artwork fields so saving a custom image is discoverable.
        Row {
            spacing: 12
            Pill {
                objectName: "applyGameArtwork"
                text: qsTr("Apply")
                selected: page.widgets.pending
                onClicked: page.applyEdits()
            }
            Text {
                height: 32; verticalAlignment: Text.AlignVCenter
                text: page.widgets.pending ? qsTr("Changes not applied") : qsTr("Settings applied")
                color: page.widgets.pending ? "#ffb74d" : "#9be7a0"
            }
        }
        Text { width: parent.width; wrapMode: Text.Wrap; text: page.widgets.error; color: "#ffb74d" }
    }
}
