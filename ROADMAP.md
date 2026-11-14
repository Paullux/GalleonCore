# ROADMAP

Référence pour reprendre le développement à tout moment, y compris dans une nouvelle session Claude Code. À mettre à jour après chaque fonctionnalité terminée **et** testée (voir « Règle de mise à jour » en bas). Les notes de conception détaillées (protocole, RGB, OpenRGB, Windows) sont dans [docs/ROADMAP.md](docs/ROADMAP.md) ; les règles de code dans [AGENTS.md](AGENTS.md).

### Règles de séparation bibliothèque/démon du clavier — 10 novembre 2026

Décision d'architecture avant tout code sur la luminosité (propriété `0xEA`) : une bibliothèque `lib/keyboard/` indépendante de `lib/` (Stream Deck), un composant `KeyboardController` séparé dans `galleond`, aucune dépendance croisée entre les deux parties USB du produit (clavier `1b1c:2b0c`, module Stream Deck `1b1c:2b18`). C'est `galleond` qui écrira la propriété, jamais un processus séparé ni le plugin OpenRGB — qui reste, lui, réservé au RVB par touche. Détails dans [docs/ROADMAP.md, section 3](docs/ROADMAP.md#3-contrôleur-rvb-pour-openrgb). **Règle d'architecture ; seul l'outil manuel `tools/keyboard-brightness` existe (voir « Implémenté et testé »), rien n'est branché dans `galleond`.**

### Relecture allemande et espagnole — 4 novembre 2026

Corrections de sens et de formulation dans les README et le site : rotation des
molettes, veille pendant la capture, prérequis du paquet `.deb` et accès exclusif
au périphérique. Les quatre README décrivent désormais les fonctions existantes
de Bosun et distinguent les anneaux confirmés le 19 octobre du RGB par touche
encore expérimental. Les titres, descriptions et libellés accessibles suivent
la langue choisie ; les compteurs traduisent aussi le relevé quotidien et les
dates périmées. Les commandes, exemples JSON et conditions de sécurité restent
inchangés. Cette relecture assistée ne constitue pas une validation native ni
un nouvel essai matériel. Validation complète de l’arbre courant à consulter
avec `python3 tools/check.py status` ; les 14 contrôles CI doivent réussir avant
la fusion de la PR demandée par le propriétaire.

### Site et README en quatre langues — 3 novembre 2026

Ajout de l’allemand et de l’espagnol au site (accueil, confidentialité, CGU,
bandeau de consentement, compteurs) et aux README (`README.de.md`,
`README.es.md`). Le sélecteur de langue du site propose les quatre langues et
choisit la langue du navigateur au premier passage. Contrôlé par script sur le
rendu (chaque chaîne existe dans les quatre langues) ; les traductions DE/ES
sont à relire par un locuteur natif avant publication. Aucun changement de code
ni de protocole.

### Diagnostic du Setup Windows de la PR #80 — 31 octobre 2026

La compilation, les tests Qt Windows et les aperçus du ZIP installé ont passé.
Le test NSIS échoue au contrôle « Bosun démarre seul exactement un démon ».
Le propriétaire rappelle que le démarrage Windows est habituellement lent.
Remplacement des 40 tentatives (~12 s) par un budget monotone de 60 s, avec
attente des endpoints réels, détection immédiate d’un processus sorti et contrôle
inchangé d’un unique démon installé. Journaux stdout/stderr conservés même en
échec ; tests PowerShell simulant une disponibilité après plus de 40 sondages,
un timeout et un processus arrêté. Les vérifications de mise à niveau,
désinstallation et conservation des données restent obligatoires. Résultat du
prochain essai natif à confirmer ; aucune correction HID déduite de ce test.

### Préparation v0.3.0 — 31 octobre 2026

Le propriétaire autorise la création d’une PR et propose une release 0.3.0.
Préparation cohérente de la version CMake, des notes de version, README anglais
et français, site et changelogs DEB/RPM. Les quatre langues sont intégrées au
paquet principal sur toutes les plateformes. La PR inclut le correctif local
d’édition des raccourcis, les onglets adaptatifs et leur animation d’une seconde.
La validation complète Docker a passé avant ce changement de version ; elle est
rejouée sur l’arbre final avant ouverture de la PR. CI Linux/Windows et paquets
à contrôler ensuite. Le site conserve sa barrière de publication jusqu’à la
présence des téléchargements de la nouvelle release. Aucun tag, aucune fusion,
aucune publication de release ou de paquets n’est autorisée par cette seule PR.
Aucune nouvelle validation matérielle Windows n’est déduite de cette préparation.

### Panneau de mise à jour et règles communes — 30 octobre 2026

Modification préparée : ouverture du panneau Updates après validation d'une
nouvelle version, sous Linux et Windows (AUR compris), une fois par version
entre sessions. Les installations DEB/RPM gérées par dépôt (PPA/COPR) restent
exclues. Simulation sans persistance et captures silencieuses conservées.
Règles communes dans `AGENTS.md`, importées par `CLAUDE.md`, avec consignes
sur les responsabilités, dépendances, frontières asynchrones et tests portables.
Validation automatisée et essai interactif à consigner avant de déclarer testé.

### Publication v0.2.3 et suivi du PPA — 29 octobre 2026

Les PR #75 et #76 sont fusionnées et le tag `v0.2.3` pointe sur `bcc0dd2`.
Release GitHub, site public (Cloudflare et GitHub Pages), COPR Fedora
43/44/45/rawhide et AUR publiés ; versions et liens publics vérifiés.
Le PPA révision 1 a échoué sur deux tests : Launchpad impose
`HOME=/sbuild-nonexistent` et le mode de test Qt ignore `XDG_DATA_HOME`.
Correction de packaging en cours : HOME temporaire accessible pour les tests,
nettoyé même en cas d’échec ; tous les tests restent obligatoires.
Le workflow de réparation prépare `0.2.3-2~noble` et `0.2.3-2~resolute`
depuis l’archive originale publiée, sans déplacer le tag ni remplacer
les autres paquets. Publication APT à confirmer après validation et upload.

### Préparation v0.2.3 — historique de la PR #75

La PR #74 a été fusionnée dans `main` (`e4133da`). La préparation 0.2.3
regroupe les notes de version, README anglais/français, site, version CMake,
changelogs DEB et spec/changelog RPM. Les notes `docs/RELEASE_0.2.3.md` seront
utilisées pour le corps de la release GitHub. Les fonctionnalités et limites
sont détaillées dans ces notes et dans les guides existants ; aucune nouvelle
validation matérielle Windows n’est déduite de la seule fusion.

Échec AppImage post-fusion identifié : remplacement upstream du binaire
`appimagetool` continuous le 29 octobre, entre la CI de PR et celle de main.
Passage à la version officielle fixe 1.9.1, dont le SHA-256 téléchargé correspond
à l’empreinte publiée par GitHub. Les contrôles SHA ne sont pas désactivés.
Publication et tag v0.2.3 autorisés par le propriétaire une fois la PR, les CI
et la validation complète de main fusionné réussies. Pas de tag publié par
cette seule mise à jour documentaire.

### Notifications de mise à jour — PR #73 fusionnée, 29 octobre

Implémentées dans `main` : une notification système par version nouvellement
validée pendant la session, sans notification depuis le cache seul ou une erreur.
Linux utilise freedesktop D-Bus ; Windows utilise l’icône Qt et ouvre Updates au
clic. Les installations gérées par dépôt conservent leurs règles de notification.
La simulation est explicitement identifiée et les captures d’écran sont silencieuses.
Les tests automatisés et les CI de la PR ont passé ; cela ne vaut pas nouvelle
validation physique de l’affichage sur le poste Windows du propriétaire.

### Historique de la revue locale du 29 octobre 2026 — avant la PR #70

**Référence de cette revue historique : `22ba855` (PR #69).** Synchronisation effectuée avec `git pull --ff-only origin main` le 29 octobre : dépôt déjà à jour, sans modification locale avant cette revue. La version déclarée dans `CMakeLists.txt` était **0.2.2** ; les compléments ci-dessous sont fusionnés pour préparer la suite, sans nouvelle publication effectuée pendant cette session.

#### Fonctionnalités présentes dans les sources

- **Socle Linux/Windows** : bibliothèque C++20/Qt 6 et transport HID, démon `galleond`, configuration QML dans Bosun, pages de 12 touches, médias, fonds personnels et veille vidéo. Les widgets d’accueil et le mode Jeu utilisent les composants QML partagés ; FPS via MangoHud sous Linux et PresentMon sous Windows, capteurs selon les backends disponibles. Les plasmoids restent un chantier distinct.
- **[PR #66](https://github.com/Paullux/GalleonCore/pull/66) fusionnée le 28 octobre** : catalogue local des jeux Epic/EA/GOG sous Windows et Heroic/Legendary/Lutris/Bottles sous Linux, raccourcis Steam hors boutique ; rapprochement des chemins Wine et des exécutables Unreal. Choix d’illustration Gaming à la molette droite, validation par appui ou après quatre secondes, sauvegarde personnelle par jeu et démonstration animée dans Bosun. Ces travaux ne sont plus uniquement locaux ou sur une branche de test. Les limites de découverte et les retours matériels consignés plus bas restent applicables.
- **[PR #68](https://github.com/Paullux/GalleonCore/pull/68) fusionnée le 28 octobre** : recherche SteamGridDB avec repli après retrait des articles français `Le/La/Les`, variantes de numéros de suite romains/arabes et correspondances d’éditions. Si les formats Steam habituels ne donnent rien, recherche parmi les autres grilles statiques en conservant les critères d’image horizontale. Le titre affiché et les identifiants des images sauvegardées restent conservés.
- **[PR #69](https://github.com/Paullux/GalleonCore/pull/69) fusionnée le 29 octobre** : plusieurs candidats SteamGridDB essayés dans un ordre borné (titre exact, variante avec `The`, nom précédé d’un auteur, édition plus longue ; huit identifiants au plus). Un candidat sans illustration permet d’essayer le suivant ; les correspondances arbitraires restent refusées. Second bouton **Appliquer / Apply** ajouté près du formulaire d’image personnalisée, tout en bas de l’onglet Gaming. Le bouton initial, uniquement en haut, sortait du champ visible pendant l’édition du formulaire : il fallait remonter pour valider. Le second bouton corrige ce défaut d’accessibilité de l’action au moment où elle est nécessaire ; il utilise le même enregistrement du brouillon que celui du haut de page.
- **[PR #67](https://github.com/Paullux/GalleonCore/pull/67) fusionnée le 28 octobre** : lien vers cette roadmap mis en évidence dans les deux README.

#### Vérification de cette revue et limites

Revue des sources, des changements fusionnés et des tests associés (`test_homewidgets`, `test_gamemode`, `test_launchercache`, `test_modes_flow`, `test_bosun`). **Aucune compilation, suite de tests ou vérification de CI distante relancée pendant cette revue documentaire.** `tools/check.py status` indique que la dernière validation complète locale enregistrée concerne un **autre arbre** (`2d2218923dee3f8cc869c091939e5adc17a3589f`) : elle ne valide pas le `main` actuel. La présence de tests de régression dans les sources ne vaut pas résultat d’exécution.

Les retours positifs Ghostrunner/Linux (identification et image) et Ghostrunner/Windows (changement d’image à la molette) sont ceux déjà consignés le 28 octobre. Aucun nouveau résultat matériel n’a été reçu pour #68/#69. Rejouer notamment RDR2, jeux GOG/EA et jeux ajoutés à Steam ; distinguer **détection/capture FPS**, **identification du titre**, **recherche d’image** et **persistance du choix**.

#### Retour matériel du propriétaire — 29 octobre (22 photos)

**Bilan confirmé par le propriétaire le 29 octobre : tous les jeux de ses essais retrouvent désormais une pochette automatiquement, sauf NeuroHell, jeu personnel dont l’image a été ajoutée manuellement.** La recherche d’illustrations est opérationnelle sur cet ensemble ; aucun défaut de pochette restant n’est rapporté. NeuroHell utilise une image personnalisée. Ce bilan ne généralise pas la couverture à tous les jeux existants et ne change pas les conditions de lancement documentées ci-dessous.

- Mode Gaming, titre, FPS et jauges visibles sur le module : Red Dead Redemption 2, Grand Theft Auto III, Crysis, Far Cry, Genshin Impact, Medal of Honor, S.T.A.L.K.E.R. Shadow of Chernobyl (dont Enhanced Edition), Duke Nukem 3D, SuperTuxKart, Sansar et Jericho, avec illustration affichée. Les photos ne permettent pas à elles seules de déterminer le lanceur, le système ou la provenance de chaque image. Les valeurs FPS élevées sont des valeurs affichées, pas une mesure indépendante de leur exactitude ; les boutons d’enregistrement visibles ne prouvent pas leur exécution.
- **Précision du propriétaire sur les jeux hors boutique / installés depuis un CD** : GTA III et Far Cry fonctionnent en mode Gaming après leur ajout à **Steam**. Pour **Jericho**, un premier lancement autonome n’activait pas le mode Gaming ; après ajout à un lanceur, il est reconnu comme jeu. Le propriétaire cite Steam et/ou Epic comme contournement pour ces installations. Le lanceur fait donc partie des conditions de réussite rapportées ; ne pas présenter les photos comme une validation du lancement autonome de ces jeux. Ce constat concerne les essais du propriétaire, sans attribuer à PresentMon une règle universelle ni modifier la recherche d’illustrations.
- **Les Sims 4, Beowulf et 0 A.D.** : photos avant avec fond de secours, puis après avec illustration. Deux illustrations différentes sont visibles pour Les Sims 4. Le propriétaire décrit les difficultés de correspondance entre noms localisés, titres courts et ponctuation ; la méthode de sélection de chaque image et la version du programme ne sont pas précisées. Ces retours valident le rendu sur matériel, sans démontrer seuls quel repli réseau a abouti ni la persistance après redémarrage.
- **NeuroHell** : jeu personnel développé par le propriétaire avec Godot, modèles majoritairement créés avec RodinAI et terrains procéduraux avec une extension Blender. Photos du mode Gaming avec fond de secours, puis image personnalisée. Le propriétaire confirme sous Linux un essai via Heroic avec une AppImage et via Steam avec l’exécutable exporté de Godot. Ce jeu sert de cas concret pour la détection d’un jeu personnel et l’illustration locale, sans dépendance à sa présence sur SteamGridDB.
- **Aide Bosun Linux/Windows en suivi local séparé** : conseil pour les jeux personnels/indépendants non détectés (lanceur acceptant les jeux locaux, source FPS adaptée, image personnelle). Ce changement d’aide et son complément dans `docs/HOME_WIDGETS.md` ne font pas partie de la PR documentaire #70 ; ils restent locaux pour une validation distincte.

**Recherche d’illustrations confirmée opérationnelle par le propriétaire** après ces retours : conserver le comportement actuel. La stratégie décrite (titre exact, replis de correspondance, articles français et titres courts) explique les essais ; elle ne constitue pas une demande de modification du résolveur. Aucun changement de recherche, de normalisation ni de clé persistante effectué pendant cette session. Les cas Les Sims 4, Beowulf et 0 A.D. ne sont plus à présenter comme des défauts ouverts de recherche.

#### Configuration FPS Linux par sélection — PR #71, 29 octobre

À la demande du propriétaire : **Configuration unique** ouvre un dialogue modal
Bosun avec sélection dynamique par groupe/jeu, tout sélectionner/désélectionner,
bouton d’application fixe et compte rendu par élément. Préparation MangoHud commune,
Steam natif/Flatpak (jeux installés et raccourcis hors boutique, options vides
uniquement), Heroic général pour futurs jeux **et** réglages individuels des jeux
existants, Lutris YAML reconnu, Bottles **par bouteille**, pré-lancement
PlayOnLinux vide, permissions des quatre lanceurs Flatpak détectés. Sauvegardes
avant écriture, restauration de fichier avec contrôle de modification, refus des
lanceurs actifs et fichiers périmés ; formats non pris en charge explicités.
L’aide aux jeux personnels et le guide jusque-là locaux sont inclus dans ce
chantier. Aucun changement de recherche de pochettes ni de capture Windows.

Tests sur configurations fictives et dossier personnel isolé : options Steam
préservées, raccourcis binaires, Heroic global/individuel et variables héritées,
Lutris/Bottles/PlayOnLinux, permissions Flatpak limitées, réapplication,
restauration et conflits ; test QML du regroupement/sélection et bouton visible,
aperçu du dialogue contrôlé. Validation complète Docker réussie sur une première version ; le contrôle est
rejoué pour le contenu final avant fusion. Résultats et CI Linux/Windows dans la
[PR #71](https://github.com/Paullux/GalleonCore/pull/71). Le propriétaire a
explicitement autorisé la PR et sa fusion après validation.
**Aucun nouvel essai interactif des lanceurs réels n’est revendiqué.**
Voir [le parcours et les limites](docs/MODE_JEU_LINUX.fr.md).

#### Retour sur le dialogue FPS — 29 octobre

Le propriétaire signale des jeux Steam en double, Bottles et PlayOnLinux absents
de la machine, et CrossOver installé. Lecture seule sur le poste : le dossier
Steam `userdata/0` doublonnait le jeu du compte réel ; anciennes configurations
Bottles Flatpak et PlayOnLinux présentes sans application installée ; CrossOver
confirmé dans `/opt/cxoffice`. Correctif : ignorer le compte technique 0 et les
alias/identifiants dupliqués, vérifier l’installation native/Flatpak avant
proposition, ajouter CrossOver natif par bouteille standard pour Vulkan/DXVK
(variables MangoHud seulement ; OpenGL et chemins personnalisés restent manuels).
Les captures du propriétaire confirment aussi deux raccourcis Steam indirects :
Medal of Honor cible Lutris avec `lutris:rungameid/2` ; AlterEgo cible un `.lnk`
dans sa bouteille CrossOver. Ces entrées étaient initialement visibles mais non sélectionnables. Au retour UX
du propriétaire, les raccourcis indirects sont désormais omis de la sélection
Steam ; aucun préfixe ajouté à leurs options.
Tests de non-régression sur configurations fictives ; aucun réglage de lanceur
réel modifié pendant le diagnostic. PR et fusion autorisées après validation
complète et CI par le propriétaire dans cette session.

#### Retours sur les métadonnées réelles — 29 octobre

Le propriétaire confirme Ghostrunner unique dans Heroic : deux enregistrements
Legendary/importé visent le même exécutable et le même préfixe. Regrouper cette
installation dans Bosun et configurer l’entrée importée explicite sans supprimer
l’ancien profil ni les dossiers. CS2 : la commande Galleon complète est correcte ;
le lecteur VDF bloquait sur des champs Steam de 5 à 20 Kio, sans rapport avec les
options du jeu. Lecture bornée élargie pour les valeurs, clés toujours bornées,
et reconnaissance de la commande complète comme déjà configurée, sans écriture.
CapCut est désinstallé selon le propriétaire : l’entrée en erreur vient de Lutris,
qui conserve une déclaration installé=1 et un ancien chemin d’installateur absent.
Les configurations à exécutable absolu manquant sont masquées ; les chemins YAML
échappés sont reconnus sans réécrire les chemins ni exécuter de contenu.
Tests sur fixtures et contrôles en mémoire ; aucun fichier réel de lanceur modifié.

#### Détection du firmware Stream Deck — 29 octobre, en validation

Lecture HID en lecture seule du rapport Gen2 0x05, une fois par connexion réussie,
avec validation de l’identifiant, de la longueur et du numéro de version. Bosun
observe la version du démon dans un état local borné et daté ; aucune ouverture
HID depuis Bosun, aucune mise à jour automatique. L’onglet Clavier affiche le
firmware du **module Stream Deck**, distinct du firmware clavier Corsair.
Le mode `galleon-bosun --old-firmware` ouvre l’onglet Clavier et simule
explicitement 3.06.005, sans écriture matérielle ni sauvegarde de cette version.
Le bouton firmware ouvre le MSI officiel Elgato Stream Deck 7.6.0 ; Web Hub
est présenté séparément pour le clavier Corsair.
Sous Windows uniquement, une version réelle antérieure à 3.06.006 affiche le conseil de
mise à jour et la procédure d’arrêt complet Bosun/galleond avant Elgato, puis
fermeture/désactivation d’Elgato avant reprise. Une lecture indisponible ou un
état périmé affiche une version inconnue, sans prétendre que le firmware est à jour.
Les cinq suites ciblées passent : protocole, transport HID simulé, Stream Deck,
état partagé et Bosun. La version courante est également affichée en petit dans
la barre du bas, avec « inconnu » si indisponible. Cas couverts : réponse malformée, comparaison numérique,
lecture unique, reconnexion, état périmé et affichage. Résultat de validation complète suivi par `tools/check.py status` ;
lecture sur clavier réel et résultat Windows restent à confirmer.

L’aide aux jeux personnels est déplacée sous l’explication de l’apparition des
boutons Gaming, avec la même police et taille, en vert pâle. Les aides manuelles
par lanceur sont regroupées sous un volet ▶/▼ replié par défaut ; le bouton de
configuration générale reste visible. Rendu vérifié dans
une capture Bosun et test d’interface réussi. Lutris reste installé à la demande
du propriétaire ; son retrait n’est plus prévu.

#### Suite prioritaire

1. Valider l’arbre fusionné sous GCC, Clang + sanitizers, fuzzing et lint, et contrôler les CI Linux, paquets et Windows/NSIS avant une PR ou publication ultérieure.
2. Tester sur Linux et Windows les replis SteamGridDB, le choix à la molette (chargement, décompte, annulation, relance hors ligne) et le bouton Appliquer près de l’image personnelle ; documenter les jeux et résultats précis.
3. Valider la configuration MangoHud par sélection décrite ci-dessus sur les lanceurs réels, et confirmer sur matériel l’avertissement firmware Windows et les notifications déjà implémentés dans les PR #73/#74.
4. Conserver les essais de stabilité longue, démarrage à froid, veille et reconnexion USB ; le RGB par touche reste expérimental et séparé de la préparation v0.2.3.

### Historique — du 21 au 28 octobre 2026

Les mentions « local », « branche de test » ou « avant PR » dans les notes ci-dessous décrivent leur état à la date du retour. Pour l’état de fusion et les validations restant à faire, utiliser le point de reprise du 29 octobre ci-dessus.

État au 21 octobre 2026 : socle Linux/Windows, Setup NSIS, pages initiales à 12 touches et aperçus Bosun fusionnés dans `main` jusqu'à la PR #35. La validation complète de l'arbre fusionné et les CI de `main` ont réussi. Le propriétaire a confirmé le fonctionnement du Setup après mise à jour du firmware Stream Deck depuis l'application officielle Elgato, puis désinstallation de cette application. Le tag [`v0.1.0`](https://github.com/Paullux/GalleonCore/tree/v0.1.0) pointe sur `8634920` et a déclenché la [CI de publication](https://github.com/Paullux/GalleonCore/actions/runs/39674680765) : consulter cette exécution et les dépôts pour leur état effectif. COPR a réussi après retrait d'un dépôt externe invalide. Les premières sources PPA ont compilé et passé CTest, mais Launchpad a refusé les binaires à cause de 46 fichiers datés de 1970 ; une révision Debian `-2` corrige les horodatages sans changer l'archive source publiée. Les essais prolongés et le démarrage à froid sous Windows restent à confirmer. La [PR #31](https://github.com/Paullux/GalleonCore/pull/31), ancien contournement HID expérimental non validé sur le matériel, est fermée sans fusion.

### État au 27 octobre 2026 — v0.2.2 publiée, prochaine v0.2.3

Cette section est un **point de reprise historique au 27 octobre** ; le bilan du 29 octobre ci-dessus la remplace pour l’état courant.

- **v0.2.2 publiée le 26 octobre** : la [PR #59](https://github.com/Paullux/GalleonCore/pull/59) apporte la préparation des permissions MangoHud pour les lanceurs Flatpak détectés dans Bosun (notamment Heroic, Steam, Lutris et Bottles lorsqu'ils sont installés). Il reste à configurer les paramètres **dans** les lanceurs eux-mêmes.
- **[PR #60](https://github.com/Paullux/GalleonCore/pull/60) fusionnée** : au premier démarrage de Bosun, vérification des releases sans attendre l'échéance du cache de 24 h ; la limitation des vérifications périodiques et le délai de reprise après erreur restent applicables.
- **[PR #64](https://github.com/Paullux/GalleonCore/pull/64) fusionnée le 27 octobre** : ajout d'un halo sombre en plusieurs couches derrière le compteur FPS sur les quatre côtés. Rendu de l’ombre confirmé sur le clavier réel sous Linux et Windows par le propriétaire.
- **[PR #65](https://github.com/Paullux/GalleonCore/pull/65) fusionnée le 27 octobre** : intégration facultative SteamGridDB pour les illustrations Gaming horizontales manquantes, clé API saisie dans Bosun et stockée séparément de `widgets.json`, téléchargements asynchrones bornés, cache local persistant avec effacement volontaire depuis Bosun, nouvelle tentative après changement de clé, refus et absence de réseau sans blocage du mode Jeu. Retours matériels partiels Linux/Windows ci-dessous ; améliorations en suivi local.
- **[PR #63](https://github.com/Paullux/GalleonCore/pull/63) fusionnée le 27 octobre** : Matomo activé uniquement avec consentement, fond Gaming de secours embarqué dans le programme, CI Linux/Windows validées et premier rendu confirmé sur le matériel sous CS2. Le contraste du compteur FPS sur les jaquettes très claires est repris dans une PR distincte.
- **[PR #61](https://github.com/Paullux/GalleonCore/pull/61) fusionnée le 27 octobre** : documentation du flux RGB non persistant par touche, constaté par Aaryush1 sur **un clavier sous Windows 11, firmware clavier 2.3.88**. **Ce n'est pas un pilote RGB intégré à GalleonCore.** Voir [l'issue #56](https://github.com/Paullux/GalleonCore/issues/56) et [les notes protocolaires](docs/protocol/README.md#live-per-key-lighting-windows-2026-10-25).

#### Retours matériels et suivi de la PR #65 — 27 octobre

- **#64 : ombre FPS confirmée par le propriétaire sous Linux et Windows**, avec photos du module réel.
- **#65 fusionnée** : Windows, GTA V Enhanced affiche son illustration, Ghostrunner fonctionne depuis Epic et CS2 conserve son fonctionnement. RDR2 déclenche le mode Jeu mais reste sur le fond de secours avec le titre `RedDeadRedemption2`. Les jeux ajoutés à Steam, Les Sims 4, les jeux EA et GOG testés ne déclenchent pas le mode Jeu. Genshin Impact et Fortnite non testés.
- **Linux #65** : CS2 conserve son image Steam ; les jeux Heroic testés restent sur le fond de secours. Ne pas considérer ce retour comme une validation de SteamGridDB sous Linux.
- **Suivi local en développement** : catalogue partagé des installations, indépendant des lettres de lecteurs et des dossiers personnels : manifestes Epic et registre EA/GOG sous Windows ; métadonnées Heroic/Legendary, configurations Lutris et programmes déclarés Bottles sous Linux (natif/Flatpak). Rapprochement CSV MangoHud avec/sans `.exe`, `argv[0]` Wine et titres concaténés. Lectures bornées, cache de trente secondes ; sous-ensemble YAML scalaire uniquement. Les raccourcis Bottles `.lnk`, formats non reconnus et chemins Wine non mappés demandent encore un profil manuel. Validation complète et nouveaux essais matériels requis avant PR ; aucun nouveau résultat matériel revendiqué.
- **Retour Linux du 28 octobre** : Ghostrunner déclenche le mode Jeu mais affiche `Ghostrunner Win 64 Shipping` avec le fond de secours. Le catalogue contient bien le titre Ghostrunner. Correctif local : traduction des chemins Wine via Z: ou WINEPREFIX/dosdevices, puis repli sur un titre/exécutable unique avec suppression des suffixes Unreal connus ; les noms génériques/ambigus restent refusés. Test ciblé réussi sur le poste, validation ciblée Clang/sanitizers/lint et nouvel essai matériel à confirmer.

#### Branche de test — choix d’illustration Gaming à la molette (28 octobre)

Demande du propriétaire : pousser une branche parallèle, **sans PR**, et lancer la CI Windows pour obtenir un Setup de test. Le suivi local regroupe la découverte générique des installations et le sélecteur d’illustrations : appui sur la molette droite en jeu, rotation pour parcourir les images horizontales, second appui ou quatre secondes d’inactivité avec décompte pour valider. La validation attend le chargement ; fermeture/changement de jeu = annulation. Un fichier par jeu dans les images personnelles remplace le choix précédent, reste prioritaire hors ligne et ne change pas le fond normal. Le premier résultat est proposé sans choix sauvegardé ; les titres normalisés exacts restent prioritaires, avec repli sur un titre d’édition commençant par le titre complet du jeu. Tests de persistance/remplacement et parcours des molettes avec faux clavier ; validation matérielle de ce sélecteur encore requise sous Linux et Windows. Le titre et l’illustration de Ghostrunner depuis Heroic/Wine ont été confirmés en photo par le propriétaire après le correctif d’identification ; cela ne valide pas encore le sélecteur.

#### Candidats v0.2.3 — PR indépendantes, ordre de travail proposé

Retour matériel Windows du 28 octobre sur la branche de test : changement d’illustration de Ghostrunner confirmé par le propriétaire. RDR2 déclenche le mode Jeu mais ne trouve aucune image avec le titre « Red Dead Redemption 2 », alors que SteamGridDB utilise « II ». Les jeux GOG testés, les installations DVD enregistrées chez EA et GTA III ajouté à Steam ne déclenchent pas la capture. Correctifs locaux en validation : réponse du catalogue rejetée par le filtre `name/icon` de LauncherCache, reconnaissance des numéros de suite finaux I–XX/1–20 (sans changer les identifiants persistants des images), lecture bornée des exécutables des jeux hors boutique ajoutés à Steam dans `shortcuts.vdf`. Nouveau test Windows requis ; ne pas considérer ces défauts comme résolus sur le matériel.

Aide Bosun du 28 octobre : démonstration animée dans la page Mode Jeu, avec les consignes, cercle sur la molette droite, deux rotations et trois illustrations distinctes, décompte de quatre secondes puis « Image sauvée ». La séquence boucle sur la page visible et s’arrête lorsque la page ou la fenêtre est masquée/minimisée. Compilation locale, qmllint et tests ciblés GCC dans l’environnement CI réussis, dont le parcours, le rebouclage et l’arrêt de la démonstration masquée. Il s’agit d’une aide visuelle, sans action sur le clavier ni téléchargement d’illustrations ; retour Linux positif du propriétaire, essai Windows encore requis. Le propriétaire autorise désormais le push de la branche parallèle et l’ouverture d’une PR après validation complète, pour poursuivre les essais sans fusion ni publication de release.

1. **Fond Gaming de secours (PR #63 fusionnée)** : visuel intégré dans `public/wallpaper_gaming/galleoncore_gaming_fallback.png` et embarqué comme ressource Qt pour Windows/Linux ; utilisé sans connexion ni fond personnalisé lorsque la jaquette du jeu manque. CI Linux/Windows réussies ; aperçu et écran réel sous CS2 confirmés par le propriétaire.
2. **Identification des jeux et SteamGridDB (PR #65 fusionnée, suivi en cours)** : conservation des identifiants Steam et des profils manuels ; reconnaissance initiale des chemins de jeux Epic/GOG/Heroic/Lutris/Bottles et des exécutables Windows dans les emplacements connus. Sur un nom de jeu identifiable, consultation facultative de SteamGridDB avec la clé API personnelle saisie dans Bosun. Recherche stricte, images **horizontales**, requêtes bornées et cache persistant dans les données utilisateur. Ordre : image manuelle > image Steam locale > cache SteamGridDB > téléchargement réseau > fond embarqué. Une clé refusée, l'absence de match, de quota ou de connexion n'empêche pas l'affichage du mode Jeu. **Limites à valider** : lanceurs utilisant des chemins non standard, jeux Wine à exécutable générique, choix manuel du titre/profil et nouveau téléchargement après modification de la clé. Validation des compléments et nouveaux essais matériels indispensables.
3. **Bouton unique « Configurer MangoHud » dans Bosun (demande du 27 octobre)** : détecter Steam/Heroic/Lutris/Bottles et leurs jeux installés, en natif et Flatpak ; préparer la configuration FPS commune et appliquer les réglages nécessaires via les formats propres à chaque lanceur. Objectif : une action simple comparable à la préparation PresentMon sous Windows. Préserver les options existantes, sauvegarder avant modification, rendre l’action répétable et afficher un bilan par lanceur/jeu (configuré, absent, non pris en charge ou action manuelle nécessaire). Conserver le dossier FPS utilisateur et `background_alpha=0,alpha=0`. Ne pas annoncer tous les jeux configurés sans vérification ; les permissions Flatpak seules ne règlent pas l’activation de MangoHud dans les lanceurs. Réutiliser le catalogue de jeux découvert pour les illustrations et ne jamais dépendre des chemins personnels du propriétaire.
4. **Avertissement firmware sur Windows uniquement** : un module Stream Deck en **3.06.005** a provoqué un blocage de `galleond` sous Windows ; la mise à jour en **3.06.006 ou ultérieure** est la voie recommandée. Étudier une détection non destructive (HID si possible ; registre Windows en second recours), sans alerte sous Linux où ce comportement n'a pas été reproduit. Guide impératif : **arrêter intégralement Bosun et `galleond` avant** d'utiliser le logiciel officiel Elgato sous Windows pour mettre à jour le firmware ; une fois terminé, **fermer Elgato, idéalement le désinstaller et désactiver son lancement automatique** afin d'éviter des contrôleurs concurrents, puis relancer GalleonCore.
5. **Notifications système des mises à jour** : si une vérification manuelle ou automatique découvre une release stable plus récente, proposer une notification système Windows/Linux et un lien vers la release ; respecter la préférence utilisateur, limiter les répétitions et conserver les règles existantes de détection des installations via PPA/COPR.

Chaque chantier doit rester une **PR de périmètre limité** avec ses tests de régression et, quand nécessaire, ses essais sur matériel. **v0.2.3 n'est ni publiée ni garantie à une date donnée** : la préparer progressivement, idéalement durant la semaine, et différer les chantiers non prêts. Avant un tag : GCC, Clang + sanitizers, fuzzing, lint/REUSE, paquets Linux, tests Windows/NSIS et validation matérielle pertinente.

#### Polissage visuel Gaming — PR indépendante

- **Halo sombre adouci derrière le compteur FPS (PR #64 fusionnée)** : halo sur les quatre côtés de la zone FPS, sans dépendance graphique supplémentaire ni changement des jauges CPU/GPU. Test de contraste automatisé ; validation visuelle sur matériel Linux et Windows confirmée par le propriétaire. Ce correctif est indépendant du chantier SteamGridDB.

#### RGB du clavier — chantier séparé de v0.2.3

L'[issue #56](https://github.com/Paullux/GalleonCore/issues/56) reste **ouverte** pour suivre l'endurance de l'implémentation expérimentale C# d'Aaryush1. **Premier retour d'usage quotidien (11 novembre 2026, rapporté par lui, non reproduit ici)** : environ 10 jours de fonctionnement sur 6 démarrages, la plus longue session de 8 jours 16 heures sans écriture ratée, gel ni reconnexion, et les 6 arrêts ont relâché les touches. Restent non exercés en usage : veille/réveil, rebranchements USB, plantage réel, coexistence Web Hub (voir [docs/WINDOWS_LIVE_LIGHTING_CSHARP.md](docs/WINDOWS_LIVE_LIGHTING_CSHARP.md#daily-use)). Une collaboration pour un pilote **natif C++** est seulement une possibilité future, **sans engagement du contributeur**.

L'architecture et les protections de cette solution C# sont décrites (en anglais) dans [docs/WINDOWS_LIVE_LIGHTING_CSHARP.md](docs/WINDOWS_LIVE_LIGHTING_CSHARP.md) ; ce document ne change pas l'état du chantier.

Ne pas intégrer le flux d'écriture en production avant une validation dédiée : format strict des **440 octets** (`12 00` + 146 triplets RGB), commandes et modes autorisés (**propriété 3 : 1 ou 2 uniquement ; jamais 3/5, modes bootloader**), restitution du contrôle en sortie/erreur/veille, watchdog indépendant, cession de contrôle si Web Hub ou autre pilote intervient. Les résultats actuels ne démontrent ni le fonctionnement Linux ni la compatibilité avec tous les firmwares. Fermer #56 seulement lorsqu'un **pilote RGB complet et stable est intégré et validé dans GalleonCore**.

### Candidat 0.2.0 — 24 octobre 2026

Retour matériel Windows du propriétaire sur le setup 0.2.0 : mesure des FPS et mode Widgets fonctionnels. Anomalie ouverte : le bouton PresentMon reste sur « Démarrer la capture FPS » alors que les FPS sont affichés ; le message de statut n’a pas été relevé. La logique actuelle et son test natif couvrent déjà la transition `running=true` → « Arrêter » ; le défaut réel n’est donc pas encore reproduit ni corrigé. Ne pas confondre la capture fonctionnelle avec la validation de son bouton.

Correctif du bouton Windows : instantané `c1e3a0b`, [CI Windows réussie](https://github.com/Paullux/GalleonCore/actions/runs/40084070829). Le bouton traite aussi l’état confirmé `capturing` lorsque `running` manque ou vaut faux ; même décision pour le libellé et la commande stop, étape de démarrage visible et désactivée. Tests ciblés Linux et 14 tests natifs Windows réussis, dont la régression sur le bouton et la demande stop. Setup, mise à niveau et conservation des données vérifiés automatiquement. Essai du propriétaire encore nécessaire pour confirmer la disparition du défaut réel, non reproduit avec ses données de statut. Les blocages globaux 0.2.0 ci-dessous restent ouverts.


Le propriétaire confirme le mode Jeu CS2/Steam sous Linux après alignement du dossier CSV MangoHud. Le setup Windows candidat `22f1cd6` a passé la [CI Windows](https://github.com/Paullux/GalleonCore/actions/runs/40081354924), y compris déploiement, installation/mise à niveau/désinstallation avec applications ouvertes et conservation des données. Les nouveaux essais matériels Windows restent à effectuer. La [CI Linux](https://github.com/Paullux/GalleonCore/actions/runs/40081356379) passe sous GCC ; Clang + sanitizers échoue dans `test_gpusensors` sur le type de pointeur de fonction NVML et l’analyse statique comporte des diagnostics à corriger. **La 0.2.0 n’est pas validée pour publication. Aucun tag ni publication avant le retour des essais Windows et la résolution des contrôles bloquants.**

### Préparation de v0.2.0

Périmètre : widgets d’accueil, mode Jeu automatique, capteurs GPU, contrôle Game Bar/OBS et aide Linux par lanceur. Le propriétaire a confirmé les widgets et les FPS sous Windows ainsi que CS2/Steam sous Linux, et a autorisé la publication après réussite des tests. Le libellé Windows au repos indique désormais « Prêt — lancez un jeu ».

Les contrôles de sortie obligatoires couvrent GCC, Clang + ASan/UBSan, fuzzing, clang-tidy, QML, tests des outils, REUSE, Windows natif et cycle du Setup, puis les paquets Linux. Le type NVML est partagé entre le collecteur et sa bibliothèque de test ; les notices AMD/Intel sont conservées et le texte MIT canonique est fourni. Les résultats des anciens candidats ci-dessus et ci-dessous sont historiques et ne constituent pas le bilan du contenu final de v0.2.0. Les notes de version sont dans [docs/RELEASE_0.2.0.md](docs/RELEASE_0.2.0.md). Le site sera repris séparément après publication.

### v0.2.1 — vérification des mises à jour Bosun

Publication de la v0.2.1 autorisée par le propriétaire le 24 octobre 2026, après
confirmation du mode de simulation sur Kubuntu Resolute et Windows. Les anciens
projets de regroupement dans une version ultérieure sont remplacés par cette demande.
Implémentation sur `codex/bosun-update-check` :
comparaison numérique avec la dernière release stable GitHub, panneau et lien
navigateur vers le fichier adapté, préférence automatique persistante, cache et
limites réseau. Les DEB/RPM interrogent les métadonnées locales APT/DNF : pas de
notification si un dépôt fournit GalleonCore (PPA/COPR), notifications conservées
pour les installations indépendantes. Détection indéterminée : vérification
manuelle disponible, automatique suspendue pour la session. Les 47 suites Qt passent sous GCC et Clang avec ASan/UBSan ; le nouveau
contrôleur est testé hors réseau avec les cas de téléchargement et d’erreur.
Simulation confirmée par le propriétaire sur Windows et Kubuntu Resolute.
La détection PPA/COPR est couverte par les tests ; aucun retour interactif spécifique
sur les dépôts n’a été reçu.
Le mode `--simulate-update` teste la vraie release avec une version de comparaison fictive `0.0.0`, sans modifier les préférences ni publier de release ; panneau identifié comme simulation, lien de téléchargement réel. Tests ciblés GCC/Clang avec sanitizers.
Voir [le fonctionnement et les limites](docs/UPDATES.md).

## 1. Implémenté et testé

### Thèmes partageables `.gctheme` — 11 novembre 2026

[PR #93](https://github.com/Paullux/GalleonCore/pull/93) fusionnée le 11 novembre 2026, après une validation complète (tous les tests sous GCC et sous Clang + ASan/UBSan, fuzzing, clang-tidy, qmllint, REUSE). Un fichier `.gctheme` porte un onglet du module (ses 12 touches, sa couleur, son fond d'écran et, sur demande, les réglages des molettes) : archive zip « stored » (`StoredZip`), validateur (`ThemePackage`, mêmes règles que `pages.json` et liste blanche d'actions), installation (`ThemeInstaller`, noms uniques, application installée choisie parmi les variantes deb, Flatpak et Snap), export (`ThemeExporter`, JPEG qualité 80 minimum, images de touche jusqu'à 160 × 160), boutons « Installer un fichier de thème » et « Exporter » dans Bosun, basculement vers le nouvel onglet dans Bosun et sur le Stream Deck (`ShowTheme`), fond d'écran par onglet, touches F13 à F24, thème d'exemple Voice chat, spécification ([docs/THEME_FORMAT.md](docs/THEME_FORMAT.md)) et pages `themes.html` et `theme-format.html` du site en quatre langues.

- **Confirmé sur le module par le propriétaire (Linux, KDE Plasma, Wayland)** : installation de Voice chat depuis Bosun, affichage de l'onglet sur le module, lancement de Discord, touches média, fond d'écran de l'onglet, raccourcis F14 à F18 reconnus par KDE (enregistrés sous « Lancer (5) » à « Lancer (9) »).
- **À savoir** : sous Linux, F13 ouvre les paramètres système de KDE, F20 coupe le micro, F19 et F24 n'ont pas de nom et ne peuvent pas servir de raccourci ; un thème doit se limiter à F14 à F18. KDE enregistre ces touches sous le nom « Lancer (n) » : le raccourci doit être enregistré dans les paramètres de KDE en appuyant sur la touche du module.
- **Multiplateforme (12 novembre 2026)** : un même fichier `.gctheme` s'installe sous Linux et sous Windows. Confirmé par le propriétaire sur Windows (voir l'entrée du 12 novembre plus bas) et sur Linux, où un thème exporté depuis Windows (`voice-chat-Windows.gctheme`, touche Discord sous la forme portable `discord`, F14 à F18, fond d'écran) a été importé ensuite. Les deux systèmes lisent le même format ; seule l'association des raccourcis F14 à F18 dans l'application diffère (directe sous Windows, via « Lancer (n) » sous KDE). Le côté Linux de l'identifiant portable demande un Bosun compilé après le commit `6a5f10d` : un ancien build refuse la touche (`desktop: must be a desktop file name…`). Aucun thème publié sur le store autre que l'exemple.
- **Idée (à garder pour le week-end)** : quand quelqu'un propose un thème, l'image de présentation sur le site peut être **le fond d'écran du thème lui-même** s'il en définit un (entrée `wallpapers/` du `.gctheme`, déjà validée par `ThemePackage`) : pas besoin de la redemander, on la récupère dans le fichier. Seulement si le thème n'a pas de fond, lui demander une image au format **15:8** (le rapport de l'écran du module, 720 × 384), pour qu'elle ne soit ni rognée ni déformée ; ce même format 15:8 est aussi celui à demander pour le fond d'écran d'un thème. À ajouter aux règles de contenu de `site/themes.html` (quatre langues) et à `docs/THEME_FORMAT.md`, et éventuellement à faire contrôler par `ThemePackage` (avertissement ou refus d'un autre rapport) : à décider avec le propriétaire.
- **Restent ouverts** : la capture KDE de la page des thèmes ; le prompt de l'image du thème cite Discord alors que `ASSETS.md` demande de ne pas nommer une œuvre existante (voir `themes/voice-chat/PROVENANCE.md`) ; boîte `themes@` dédiée (le bouton de soumission utilise `contact@`) ; mention dans les README.

- **Identifiant d'application portable (PR #95, 11 novembre 2026)** : une touche `app` ne pouvait nommer une application que par son identifiant `.desktop` Linux, jamais par un chemin Windows ; installer le thème Voice chat (touche Discord) échouait donc entièrement sous Windows. Ajout de `daemon/WellKnownApps.h` : une petite table intégrée (pour l'instant `discord` seul) associant un identifiant portable court (jamais suffixé `.desktop`) à ses identifiants `.desktop` Linux connus et, pour Windows, au nom de l'exécutable visé par le raccourci et à son `ProductName`/`CompanyName` (informations de version du fichier, pas le nom affiché du raccourci — insensible à la langue ou à un raccourci renommé). `DesktopEntries::isValidReference`/`lookup` (Linux et Windows) et `ThemePackage` acceptent cet identifiant ; `ThemeInstallPlan` le résout, au choix du thème, vers ce que la machine possède réellement (jamais stocké tel quel dans `pages.json`) ; une application introuvable rend la touche inactive avec un avertissement, sans jamais faire échouer tout le thème. `ThemeExporter` fait l'inverse : une touche `app` reconnue est réexportée sous sa forme portable, pour qu'un même fichier s'installe des deux côtés. `themes/voice-chat/theme.json` (et les deux `.gctheme` associés) utilisent maintenant `"desktop": "discord"`. **Testé et confirmé sur machine réelle (Windows, ce PC, Discord installé via le raccourci menu Démarrer)** : résolution à l'installation simulée (`ThemeInstallPlan`) vers le vrai raccourci installé, sans avertissement ; `test_windowsdesktop` et un nouveau `test_wellknownapps` passent, y compris un test anti-faux-positif (un `notepad.exe` renommé `Discord.exe` n'est pas reconnu, car ses informations de version ne correspondent pas). **Non confirmé** : le côté Linux (tests `test_desktopentries`, `test_pagesconfig`, `test_themepackage`, `test_themeinstall` mis à jour mais jamais exécutés depuis cette session Windows) et la validation complète (`tools/check.py full`, Docker) — python3 n'est pas installé sur ce poste. CSS du mode sombre des pages `themes.html`/`theme-format.html` et des pages légales (`site/legal.css`, qui n'avait aucun support du mode sombre) corrigé séparément sur `fix/legal-pages-dark-mode`.

### Outil manuel de luminosité du clavier — 11 novembre 2026

`tools/keyboard-brightness` (option CMake `GALLEON_BUILD_KEYBOARD_BRIGHTNESS_TOOL`, hors build par défaut) lit ou écrit la luminosité globale du clavier (propriété `0xEA`, 0 à 1000) via `lib/keyboard/` (`galleon_keyboard`) et `galleon::HidTransport`. Sans argument : lecture seule ; avec une valeur : écriture puis relecture de confirmation. Tests : `tests/test_keyboardlink.cpp` (faux transport). **Sur le module** : confirmé par le propriétaire sous **Windows** (10 novembre) et sous **Linux** (11 novembre : lecture, écriture 100 puis 1000, relecture identique), firmware clavier 2.3.88, une seule unité. Pas encore de mémorisation/restauration ni de cession de place au Web Hub : rien n'est branché dans `galleond`.

### Branche `keyboard` — lecture de la luminosité actuelle — 13 novembre 2026

Première étape de la branche `origin/keyboard` (fonctions du clavier développées élément par élément, fusion dans `main` seulement quand l'ensemble est stable). `KeyboardController` sait lire la valeur que le clavier tient (`ReadBrightness`, signal `brightnessRead`) sans rien écrire ; une écriture en attente passe toujours avant une lecture. Bosun (Linux) la demande quand `galleond` apparaît et place le curseur sur le pas de 100 le plus proche (une valeur posée par le Web Hub reste lisible). Décision du propriétaire : le clavier garde la valeur choisie, pas de restauration à la sortie.

- **Validation** : `tools/check.py full` réussi (GCC, Clang + ASan/UBSan 58/58, fuzzing, clang-tidy, qmllint, outils, REUSE), environnement hôte Ubuntu 24.04 comme la CI. Tests : trois cas de contrôleur et la règle du pas le plus proche (`test_keyboardcontroller`, `test_daemonstatus`).
- **Non confirmé sur le module** : le curseur qui se place sur la vraie valeur au démarrage de `galleond` (à essayer après un changement dans le Web Hub).
- **Windows** : le point d'accès local n'a pas de canal de réponse, le curseur y reste inconnu. Reste à concevoir (fichier d'état ou canal bidirectionnel).
- **Reste dans cette branche** : détecter un contrôleur étranger pour lui céder la place ; puis la couche de sécurité RVB (étape 2) ; idée du propriétaire, plus tard : une courbe de luminosité sur 24 h (heure en X, 0 à 1000 en Y), qui demande un minuteur dans `galleond` avec garde-fou sur le nombre d'écritures.

### Branche `keyboard` — couche de sécurité du RVB par touche (étape 2, première tranche) — 13 novembre 2026

Premières briques **pures** de la couche de sécurité, dans `lib/keyboard/` (aucune écriture possible : rien n'est envoyé, rien n'est branché dans `galleond`) :

- `LiveCommandGuard` : liste d'autorisation vérifiée **avant** tout transport. Seuls passent : ouverture de session, lecture de propriété, écriture de la propriété 3 à **1 ou 2** (jamais 3, 4, 5 ; jamais la luminosité `0xEA`, qui est stockée), ping sans charge, ouverture du fichier **0x0022** dans le tampon 0, `unbind` tel que dans la capture, et l'écriture d'**une** trame de 440 octets (`06` annonçant 440, puis `07` ; pas de `07` sans `06`, pas d'octet de remplissage non nul). Le garde ne change d'état que sur ce que le clavier a **confirmé** (statut 0).
- `LiveFrame` : trame `12 00` + 146 triplets RVB et découpe en quatre rapports de 129 octets ; l'exemple détaillé de `docs/protocol/README.md` (F1 vert, octet 177) sert de vecteur de test. Un indice de LED hors 0..145 refuse toute la trame ; session 0 refusée.
- `LivePolicy` : `RateLimiter` (20 images/s en moyenne, rafale de 2, une image trop rapide est abandonnée, temps passé en argument) et `FreezePolicy` (deux échecs de suite déclenchent le gel, levé seulement par un rebranchement).
- Sources : `docs/protocol/README.md` (captures d'Aaryush1, firmware 2.3.88) et `docs/WINDOWS_LIVE_LIGHTING_CSHARP.md` (qu'il autorise à réutiliser ou réécrire, issue #56). Écrit à neuf, d'après les notes.
- Tests : `test_keyboardlive` (30 cas) et la cible de fuzzing `fuzz_live_guard` (5 millions d'exécutions sans erreur). **La cible de fuzzing n'est pas encore dans la CI** (`ci.yml` et `tools/check.py` ne lancent que `fuzz_input_report`) : à décider avec le propriétaire, car cela touche la CI.
- **Cession au Web Hub (deuxième tranche, `LiveYield`)** : `ReplyTracker` classe chaque réponse (la nôtre, tardive après notre propre expiration, ou **étrangère** = un autre hôte interroge le clavier ; liste des expirées bornée à 8), `YieldPolicy` applique les règles mesurées par Aaryush1 : rien n'est envoyé pendant 3 s après la connexion ; mode relu environ chaque seconde ; un mode autre que 2 ou une réponse étrangère met en cession (plus rien n'est envoyé, **pas même un ping, et rien n'est libéré**) ; après 60 s sans trafic étranger, une lecture du mode : 1 ou 2 → libération puis reprise, tout autre mode (3, 4, 5…) → on attend 60 s de plus ; le trafic étranger pendant la sonde l'annule. Deux décisions pures : au démarrage, libérer sauf si le mode est un **1 confirmé** (une lecture échouée n'en est pas un), et ne rien envoyer si un autre hôte parlait déjà ; le processus de surveillance ne libère que si le clavier est encore en mode 2. Test : `test_keyboardyield` (26 cas, dont le scénario mesuré du Web Hub ouvert en cours d'exécution). Mesuré par Aaryush1 sous Windows seulement ; rien n'a été essayé sous Linux.
- **Processus de surveillance (troisième tranche)** : `LiveRequests` (constructeurs des requêtes, octets de la capture, tous acceptés par le garde), `LiveRelease` (`releaseIfHostMode` : session neuve, lecture du mode, et **seulement si le clavier est encore en mode 2** : `unbind`, mode 1, relecture ; chaque étape tentée même si la précédente échoue ; trois tentatives au plus, jamais de boucle sans fin ; tout envoi passe d'abord par `LiveCommandGuard` ; un mode illisible n'est jamais traité), l'échange requête/réponse partagé avec la luminosité (`Exchange`, comportement inchangé) et `openVendorInterface` (l'interface constructeur et sa vérification de collection, désormais à un seul endroit, utilisée par `KeyboardController` et l'outil). L'outil `tools/keyboard-watchdog` (option `GALLEON_BUILD_KEYBOARD_WATCHDOG`, hors build par défaut) prend `--pid N`, attend la fin du processus (un `pidfd` sous Linux, un handle de processus sous Windows : un identifiant réutilisé n'est jamais pris pour l'écrivain ; impossible à surveiller → il n'envoie rien), puis libère. Tests : `test_keyboardrelease` (14 cas, clavier simulé), `test_keyboardwatchdog` (arguments, attente d'un vrai processus sous Linux). **Non vérifié** : la version Windows de l'attente (écrite, non compilée ici), et tout essai sur le clavier réel ; **rien ne lance encore ce processus** (il n'y a pas d'écrivain de trames), l'outil ne s'essaie donc qu'à la main, par le propriétaire.
- **Reste pour l'étape 2** : la réponse d'Aaryush1 à l'issue de relecture ; aucune écriture de trame avant.

### Branche `keyboard` — fichier d'état de la luminosité (retour de la valeur sous Windows) — 13 novembre 2026

Décision du propriétaire : `galleond` écrit, après chaque lecture ou écriture **confirmée** de la luminosité, `keyboard-state.json` à côté de `pages.json` (`{"version":1,"brightness":700,"time":1783938123}`) ; Bosun le surveille. Le canal local de Windows n'a pas de réponse, c'était la seule voie de retour.

- `KeyboardState` (format **strict** : exactement ces trois clés, nombres entiers dans la plage, 4 Kio au plus ; écriture atomique) ; `KeyboardStatePublisher` (écoute `KeyboardController`, qui ne le connaît pas ; un échec de lecture ou d'écriture ne publie rien et ne remplace jamais la dernière valeur réellement vue).
- Nouvelle action du canal local, `keyboard-brightness-read` (sans autre champ), qui fait lire le clavier par `galleond` ; Bosun la demande dès que le démon apparaît.
- `KeyboardStateWatcher` (Bosun) : surveille le dossier et refait la surveillance du fichier après chaque remplacement par renommage ; ignore tout fichier invalide ; **ignore une valeur datée d'avant le démarrage de la fenêtre** (elle décrit une session précédente et un autre programme a pu changer le clavier) ; réessaie toutes les 3 s tant que le dossier n'existe pas.
- Sous Linux le retour reste le signal D-Bus déjà en place ; le fichier est écrit aussi (même code), mais seul Windows le lit pour l'instant.
- Tests : `test_keyboardstate` (format, écriture atomique, éditeur), `test_keyboardstatewatcher`, `test_localcontrol`. **Non vérifié** : le côté Windows de Bosun (`WindowsDaemonStatus.cpp`, quelques lignes écrites sans pouvoir les compiler ici : la CI Windows dira) et tout essai sur le clavier réel.

### Branche `keyboard` — courbe de luminosité sur 24 h, partie démon — 13 novembre 2026

Choix du propriétaire : **un point par heure**, chacun un **pas de 100** (0 à 1000) dans le fichier ; **échelle affichée de 0 à 100 %** ; dans l'interface, une **courbe continue et lissée**, enregistrée **point par point au plus proche**. Deux mécanismes au choix : **curseur** ou **courbe** (boutons radio dans Bosun, à faire).

- Fichier `brightness-curve.json` à côté de `pages.json` : `{"version":1,"mode":"curve"|"slider","hours":[24 nombres]}`, **strict** (exactement ces trois clés, 24 entiers multiples de 100 entre 0 et 1000, 4 Kio au plus ; un fichier invalide est refusé en entier). `BrightnessCurve` : lecture et écriture atomique, `brightnessAt` (le point le plus proche de l'heure : 07:29 prend 07:00, 07:30 prend 08:00 ; 23:30 et après prennent 00:00), `smoothBrightness` (spline de Catmull-Rom fermée sur 24 h, passe par chaque point, bornée à 0..1000) et `snapBrightness` (arrondi au pas de 100) pour l'éditeur à venir. Défaut : mode curseur, forme jour/nuit.
- `BrightnessScheduler` (démon) : ne fait **rien** en mode curseur ; en mode courbe il **lit d'abord le clavier** et n'écrit que si la valeur diffère de la cible, et seulement quand la cible **change** (au plus 24 écritures par jour, plus une au démarrage ou au passage en mode courbe). Il ne se bat pas contre un changement fait entre-temps (curseur de Bosun, Web Hub) avant son point suivant. Un fichier absent ou abîmé laisse la courbe en cours. L'heure est lue toutes les 30 s (veille, changement d'heure). Il ne parle qu'au `KeyboardController`.
- `ReplacedFileWatcher` : surveillance partagée d'un fichier remplacé par renommage (dossier surveillé, nouvelle surveillance du fichier, nouvel essai tant que le dossier manque) ; Bosun (`KeyboardStateWatcher`) l'utilise aussi, au lieu d'une copie.
- Tests : `test_brightnesscurve` (22), `test_brightnessscheduler` (11, clavier simulé : curseur, fichier absent, lecture avant écriture, pas de réécriture, point suivant, clavier illisible, retour au curseur, fichier abîmé, fichier enregistré après le démarrage).
- **Éditeur dans Bosun (onglet Clavier)** : deux boutons radio, « Curseur manuel » ou « Courbe sur 24 heures » (le curseur se grise en mode courbe) ; une courbe lissée fermée sur 24 h, échelle 0 à 100 %, 24 poignées à tirer verticalement (chacune ramenée au pas de 10 % pendant le glissement, la courbe suit le point tiré avec la **même** fonction de lissage que le démon), repère de l'heure actuelle et du point que le démon appliquera ; au clavier, flèches haut/bas pour la valeur (validée après 450 ms de repos) et gauche/droite pour changer d'heure. Rien n'est envoyé pendant le glissement ; chaque point lâché est enregistré (`BrightnessCurveModel`, fichier à côté de `pages.json`). Un fichier abîmé n'est pas utilisé, un avertissement le dit et la prochaine modification le remplace. Textes en quatre langues. Tests : `test_brightnesscurvemodel` (17), `test_bosun` (éditeur et radios).
- **Confirmé sur le module par le propriétaire (Linux, 13 novembre 2026)** : l'outil de choix de la luminosité fonctionne **en mode courbe comme en mode curseur**, avec le build de la branche `keyboard` compilé chez lui (Qt 6.10). C'est tout ce qui a été rapporté : le détail du suivi à l'heure choisie, du retour au curseur et de la valeur réelle placée au démarrage n'a pas été précisé.
- **Confirmé aussi sous Windows par le propriétaire (13 novembre 2026)** : « Windows vérifié également », avec le setup produit par la CI de la branche (run Windows desktop vert au commit `616dbfa`). Le propriétaire n'a pas détaillé les points un par un ; ne rien en déduire de plus que « le réglage de luminosité marche sous Windows ».
- **Défaut relevé par le propriétaire, Linux et Windows (corrigé)** : l'onglet Clavier, devenu plus haut que la fenêtre avec l'éditeur de courbe, n'avait aucun moyen de défiler (c'était une simple colonne, pas une zone défilante). Il est maintenant dans un `Flickable` avec une barre de défilement visible (`ScrollRail`, en QML pur car QtQuick.Controls n'est pas une dépendance) ; molette, glissement de la barre et clic sur la piste. Demande suivante du propriétaire, faite : **une barre horizontale aussi, en bas**, parce qu'un clavier dessiné (réglages touche par touche, démonstrations de thèmes statiques ou dynamiques) demandera de la largeur. `ScrollRail` gère les deux orientations ; l'onglet Clavier calcule sa largeur de contenu et affiche la barre dès que le contenu dépasse la fenêtre. **Avec le contenu actuel elle ne s'affiche pas** (la fenêtre fait au moins 1000 px de large) : elle est prête, testée sur un contenu plus large (`aScrollRailFollowsItsFlickableInBothDirectionsAndOnlyShowsWhenSomethingIsOutOfView`), et n'a pas pu être vue à l'œuvre dans l'onglet réel. Test vertical : `theKeyboardTabScrollsWhenItIsTallerThanTheWindowSoNothingIsOutOfReach`. Les autres onglets (Widgets, Gaming, Fonds d'écran, Économiseurs) défilent à la molette mais **n'ont pas de barre visible** : même défaut possible, non traité.
- **Reste** : les essais du propriétaire à poursuivre (suivi de la courbe à chaque heure, valeur lue au démarrage de `galleond`), à préciser pour Windows (fichier d'état, attente de processus du watchdog). Une compilation de la branche chez le propriétaire avec Qt 6.10 a révélé un en-tête manquant dans un test (corrigé, `616dbfa`) : la CI en Qt 6.4 ne pouvait pas le voir.

### Branche `keyboard` — dessin du clavier et du module, avec transparence — 13 novembre 2026

Demande du propriétaire : une image du clavier avec canal alpha dans l'onglet Clavier, des « mini-cases » de couleur sous les touches, et le module Stream Deck affiché. **Décision : aucune image de Corsair dans le dépôt** (marque et logo visibles, rendu du produit protégé ; une version « stylisée » en resterait dérivée) ; le dessin est **généré par notre propre script à partir de nos mesures**.

- `tools/keyboard-map/build_map.py` lit `docs/protocol/lamps-2b0c.csv` (108 lampes mesurées), `lamps-2b18.csv` et `data/devices/galleon-100-sd.json`, et écrit `bosun/qml/KeyboardMapData.js` (ce que Bosun dessine), `bosun/icons/galleon-100-sd-overlay.svg` (**le calque sombre à trous réels, alpha 0** : légendes, contour de chaque touche, bande lumineuse, écran et 12 touches du module, anneaux des molettes) et `galleon-100-sd-map.svg` (le même sur une lumière blanche, pour le regarder). Rendu vérifié : posé sur un arc-en-ciel et sur des images, la couleur passe par les lettres, les contours et les fenêtres du module, comme sur les vraies touches ; un PNG exporté garde l'alpha (extérieur, écran, touches du module et anneaux à alpha 0, plaque sombre à 255).
- **Mesuré / décoratif / estimé** : les 94 lampes sont à leur position mesurée et chacune est rattachée à la touche la plus proche ; le **pas des touches est celui des lampes (20,0 mm), pas les 19,05 mm du standard** (une première version avec 19,05 dérivait d'une touche sur la rangée) ; les capuchons (disposition française ISO, légendes) sont de la décoration ; la position du module par rapport au clavier, la hauteur des molettes, le contour et la bande lumineuse sont **estimés** (à mesurer à la règle). Les lampes 94 à 107 ne sont pas dessinées (Y = 0 peut être un repère, rien ne s'est allumé). Le clavier ne donne pas de noms de touches : la correspondance lampe/touche vaut ce que valent leurs positions (deux lampes sous le « B », par exemple, c'est dans les mesures).
- Dans Bosun (onglet Clavier, « Clavier et module · aperçu ») : le dessin, avec la lumière sous chaque touche vue par transparence ; clic sur une touche + palette pour la colorer ; « Donner cette couleur à toutes les touches », « Arc-en-ciel », « Effacer » ; le module montre les 12 touches (images réelles de `TileRenderer`) et le nom de l'onglet de thème regardé en dernier. **Aperçu seulement : rien n'est envoyé au clavier** (le RVB par touche reste bloqué). Les couleurs ne sont pas enregistrées.
- **Limites connues** : l'Enter ISO est dessiné en deux rectangles, pas en L ; l'écran du module n'affiche que le nom du thème (le vrai écran est rendu par `galleond` : un miroir en direct, y compris l'écran de veille, demanderait que `galleond` publie ses images, avec un cache borné) ; le rendu n'a été vu que par captures hors écran sous Linux.
- Tests : `tools/test_keyboard_map.py` (15 : fichiers à jour, fidélité aux mesures, aucune touche qui se recouvre, tout dans le contour, 12 touches au pas mesuré, vrais trous dans le calque) ; `test_bosun` (clic, couleur, arc-en-ciel, effacement).

### Branche `keyboard` — le lien d'éclairage (point 1 du contrôleur de LED) — 13 novembre 2026

Le propriétaire trouve l'affichage du clavier « clean » (13 novembre) ; il reste à développer le contrôleur des LED, le processus de surveillance et ce que préconise Aaryush1. **Aaryush1 n'a pas encore répondu à l'issue #102** (aucun commentaire au 13 novembre) : tant que sa relecture n'est pas là, **aucune trame ne part vers un clavier réel**, et ce qui suit est désactivé et testé seulement avec un faux clavier.

- `LiveLink` (`lib/keyboard/`) assemble toutes les briques de sécurité au-dessus d'un `Transport` : le garde (tout envoi y passe), le limiteur (20 images/s, les autres abandonnées), le gel (deux échecs de suite), la cession (3 s d'écoute sans rien envoyer, mode relu chaque seconde, 60 s de calme avant de reprendre, rien n'est libéré pendant la cession), la libération au démarrage (sauf un 1 **confirmé** : un mode illisible n'en est pas un) et à la sortie. L'heure est un argument ; l'appelant appelle `tick()` toutes les 50 à 100 ms, `show()` pour une image, `release()` pour finir. `generation()` monte à chaque (re)prise des clés : qui garde une scène la renvoie alors.
- Détails traités : un rapport que personne n'a demandé (ni une réponse tardive à l'une de nos commandes) = autre hôte ; une session expirée (octet 1 = 15) fait reprendre les clés dans une nouvelle session ; une trame interrompue est abandonnée (`LiveCommandGuard::abortFrame`, **que le clavier accepte une nouvelle trame par-dessus une trame abandonnée est une hypothèse, pas une mesure**) ; un clavier qui disparaît arrête tout. Le tampon d'échange (`Exchange`) et l'envoi vérifié (`CheckedLink`) sont partagés avec la luminosité et la libération du processus de surveillance.
- Tests : `test_keyboardlivelink` (25 cas, faux clavier : mode, session, fichier ouvert, trames reçues, Web Hub qui parle ou qui reprend le mode, panne, session expirée, débranchement) ; cible de fuzzing `fuzz_live_link` (le clavier « répond » n'importe quoi : seuls des octets autorisés peuvent sortir ; 10 860 exécutions sans erreur) ajoutée à la CI.
- **Pas encore fait** : le composant `galleond` qui l'emploie (hors de la boucle du module : un fil à part, comme `KeyboardController`), son activation (désactivée par défaut, choix explicite, marquée expérimentale), le lancement du processus de surveillance avec son propre identifiant avant de prendre le clavier (et le **refus de prendre le clavier si ce garde n'a pas démarré**), la libération à la veille, à la fermeture de session et à l'arrêt, les effets (fond selon l'heure, frappe réactive), le sélecteur de modes dans Bosun.
- **Leçon d'Aaryush1 à appliquer** : son Guard plantait au démarrage parce qu'il écrivait son journal dans le dossier de l'application, en lecture seule, et rien ne l'a signalé pendant deux versions. Notre `tools/keyboard-watchdog` n'écrit aucun fichier (il n'écrit que sur la sortie standard) ; il doit **être testé dans sa forme installée**, pas seulement depuis le build de développement.

### Branche `keyboard` — le contrôleur d'éclairage dans `galleond` (désactivé) — 13 novembre 2026

Suite du point 1 : le composant `galleond` qui emploie `LiveLink`. **Expérimental et éteint** : rien ne démarre seul, rien n'est mémorisé d'un démarrage à l'autre, et **aucune trame n'est partie vers un clavier réel** (testé seulement avec un faux clavier). **Aaryush1 n'a toujours pas répondu à l'issue #102.**

- `LightingController` (`daemon/`, côté clavier, séparé du côté Stream Deck) : un fil de travail à part (les échanges bloquent : jamais dans la boucle qui envoie le keep-alive du module), interface D-Bus `org.galleoncore.Lighting` (`Enable(bool)`, `SetColor(r,g,b)`, `State()`, signaux `stateChanged`, `activeChanged`). Pas d'activation sous Windows pour l'instant (voir plus bas).
- **`Enable(true)` est REFUSÉ, sans toucher au clavier**, si (1) le système ne prévient pas de la veille à temps, ou (2) le processus de surveillance ne tourne pas. La surveillance est lancée **avant** l'ouverture du clavier, pour l'identifiant de `galleond` ; si elle s'arrête pendant que l'éclairage est allumé, l'éclairage est coupé et les touches rendues.
- `ProcessWatchdog` : lance `galleon-keyboard-watchdog --pid N` comme processus fils (il survit à un plantage de `galleond`, c'est le but), et n'y croit que s'il existe, démarre **et tourne encore 300 ms après** (un garde qui s'arrête aussitôt ne protège rien). Il n'écrit aucun fichier (leçon d'Aaryush1 : son Guard plantait en écrivant son journal dans le dossier en lecture seule de l'application, sans que rien ne le signale pendant deux versions). L'outil n'est **pas construit par défaut** (`GALLEON_BUILD_KEYBOARD_WATCHDOG`) et **pas encore installé avec les paquets** : tant qu'il manque, l'éclairage reste refusé.
- **Rendre les touches** : à `Enable(false)`, à la sortie du démon, **avant la veille** (`LogindPowerEvents` : verrou « delay » de logind, qui retarde la veille le temps de rendre les touches ; reprise après le réveil avec les 3 s d'écoute), et quand le lien s'arrête seul (deux échecs de suite, clavier disparu) : il reste alors éteint jusqu'à nouvelle demande. Le verrou logind n'a **pas été essayé sur une vraie machine** (seulement testé avec une fausse veille).
- `QuitOnSignals` : `SIGTERM` (`systemctl stop`), `SIGINT` et `SIGHUP` quittent `galleond` par la boucle d'événements, donc ses destructeurs rendent les touches. **Un test a trouvé un vrai défaut** : le tube de communication était bloquant, et `galleond` serait resté suspendu sur le signal censé l'arrêter ; corrigé (tube non bloquant).
- Pendant que l'éclairage est allumé, `KeyboardController` met la luminosité **en pause** (`setPaused`), avec un message qui le dit : les deux liraient les mêmes réponses et se prendraient pour un autre hôte. Câblé dans `main.cpp`, les deux composants ne se connaissent pas.
- **Windows** : `UnavailablePowerEvents` refuse l'éclairage (les notifications de veille ne sont pas câblées) plutôt que de tourner sans rendre les touches avant la veille. À faire : les notifications de puissance de Windows et un canal de commande (l'endpoint local n'a pas de réponse).
- Tests : `test_lightingcontroller` (18 : éteint par défaut, refus sans veille ni surveillance, surveillance avant ouverture, 3 s d'écoute, couleur, arrêt, destruction, mort de la surveillance, veille puis réveil, clavier illisible, cession, gel), `test_processwatchdog` (9, avec de petits scripts), `test_quitonsignals` (5), `test_keyboardcontroller` (pause, 14).
- **Pas encore fait** : (activation par Bosun faite, voir plus bas ; avant, seulement par D-Bus : `busctl --user call org.galleoncore.Deck /org/galleoncore/Lighting org.galleoncore.Lighting Enable b true`), installation de l'outil de surveillance avec les paquets et son test **dans sa forme installée**, les effets (fond selon l'heure, frappe réactive), le sélecteur de modes de Bosun.

### Éclairage en direct — testé sur le clavier du propriétaire (Linux) — 13 novembre 2026
- Retour du propriétaire, avec deux photos : l'activation depuis Bosun allume bien les touches, et la couleur choisie dans les pastilles (vert) est appliquée à tout le clavier. **Le contrôle des LED fonctionne sur le matériel, sous Linux.**
- Testé : activation, couleur unique. **Pas encore testé** : arrêt (rendu des touches), cession à un autre programme (Web Hub), veille/réveil, plantage de `galleond` avec le programme de surveillance, Windows. À faire avant de parler de fonction stable ; l'issue #102 (Aaryush1) doit en être informée.

### Onglet d'un thème dans Bosun : touches toujours visibles et miniature du fond d'écran — 14 novembre 2026
- Demandes du propriétaire après son essai sous Windows : (1) la bande d'état cachait la dernière ligne de touches quand la fenêtre est basse ; (2) voir d'un coup d'œil si le thème a un fond d'écran.
- (1) La taille des touches s'adapte à la hauteur disponible (de 140 px à 84 px au plus bas) pour que les quatre lignes finissent au-dessus de la bande ; la colonne de gauche garde la largeur de trois touches pleines (la note du dessus ne dépend ainsi pas de la taille des touches, ce qui ferait une boucle) et la colonne de droite ne bouge pas.
- (2) Dans « Fond d'écran de cet onglet », la miniature du fond de l'onglet (même fournisseur d'images que la page des fonds d'écran), rechargée quand le fichier des pages change ; rien d'affiché s'il n'y en a pas.
- **Défaut trouvé à l'essai sous Windows** : la miniature restait vide après l'installation du thème. Cause : pour recharger l'image, j'affectais `source` par script, ce qui détruit sa liaison avec l'onglet affiché ; la lecture du fichier des pages qui suit une installation le déclenchait pendant qu'un onglet sans fond était à l'écran, et l'image restait vide pour les onglets suivants. Maintenant la révision des pages est dans l'adresse (`?r=…`, ignorée par le fournisseur) et la liaison n'est plus jamais cassée ; un message remplace le cadre vide si l'image n'est pas dans la liste des fonds d'écran. Test qui le reproduit : `theThumbnailFollowsTheTabEvenWhenThePagesWereReadAgainWhileAnotherTabWasShown`.
- Tests : `theTwelveKeysEndAboveTheStatusBarInALowWindowAndKeepTheirSizeInATallOne`, `aTabWithItsOwnWallpaperShowsItsThumbnailAndOneWithoutShowsNone`. L'aspect lui-même est à juger à l'écran.

### Éclairage en direct sous Windows : premier essai sur le clavier du propriétaire — 14 novembre 2026
- Version essayée : archive de la CI Windows de `5bf08c7`. Les **chemins de sécurité marchent sous Windows** (retour du propriétaire) : (1) éteindre l'éclairage depuis Bosun rend les touches au dernier réglage du Web Hub ; (2) lancer le Web Hub rend les touches au dernier réglage du Web Hub (cession) ; (3) `taskkill /F /IM galleond.exe` pendant que l'éclairage est allumé rend aussi les touches au dernier réglage (le chien de garde fait son travail).
- **Ce qui ne marche pas** : la couleur choisie ne s'affiche pas. `lighting-state.json` dit `"holding"` (allumé), le chien de garde tourne, aucune panne, donc `galleond` croit avoir pris les touches et que les trames sont acceptées. Les mêmes étapes allument les touches sous Linux. Cause **inconnue** ; aucune hypothèse n'est vérifiée.
- Pour la trouver : journal de diagnostic (`daemon/TransportTrace.*`) activé par `GALLEON_LIGHTING_LOG=1` dans l'environnement de `galleond`, qui note l'heure, le sens, la longueur et les 16 premiers octets de chaque rapport échangé avec le clavier et les changements d'état, dans `%LOCALAPPDATA%\GalleonCore\logs\lighting-*.log` (256 Kio au plus, ni numéro de série ni chemin). Rien n'est envoyé de plus.
- Retours de l'interface : (1) avec l'éclairage allumé, le curseur et la courbe de luminosité sont suspendus (voulu : les deux écrivent en même temps sur le clavier) mais rien ne le disait : une note l'explique maintenant ; (2) « Donner cette couleur à toutes les touches » de l'aperçu envoie maintenant la couleur au clavier quand l'éclairage est allumé (une seule couleur pour toutes les touches : une couleur par touche demande la correspondance entre les touches et les lampes du protocole, pas encore établie). **Pas encore compris** : le bouton radio de choix du mode qui « ne change plus » avec l'éclairage allumé : le code ne le désactive pas, il faut le message de la barre d'état.

### Onglet Clavier : la luminosité est relue à chaque ouverture — 14 novembre 2026
- Demande du propriétaire : à l'ouverture de l'onglet Clavier, le curseur doit montrer ce que le clavier tient maintenant, pas ce qui a été vu au démarrage de Bosun (la courbe du jour ou un autre programme a pu la changer).
- Bosun demande à `galleond` de lire (le même outil de lecture que le reste ; Bosun n'ouvre toujours jamais le clavier) à chaque ouverture de l'onglet ; la réponse arrive par le signal D-Bus sous Linux et par `keyboard-state.json` sous Windows, comme déjà. Avec l'éclairage en direct allumé, la lecture est suspendue par le démon : le curseur garde sa dernière valeur. Si le Web Hub est ouvert, sa lecture peut masquer la nôtre (déjà dit dans l'onglet).
- Test : `openingTheKeyboardTabAsksGalleondToReadTheBrightnessAgain` (faux `galleond` sur sa propre connexion) ; il échoue sans le changement. Un piège rencontré : à l'exécution du gestionnaire de changement d'onglet, la propriété `tab` n'a pas encore suivi le nouvel indice ; on lit donc l'onglet dans la liste.

### Éclairage en direct sous Windows : la veille et le réveil sont branchés — 14 novembre 2026
- Ce qui bloquait (`UnavailablePowerEvents`, qui refusait tout) est remplacé par `WindowsPowerEvents` : une fenêtre jamais affichée (de premier niveau : les diffusions de Windows ne vont qu'à ces fenêtres, pas aux fenêtres « message seul »), aussi enregistrée avec `RegisterSuspendResumeNotification`. Elle donne les touches **à l'intérieur** de la notification de mise en veille et de fin de session (Windows n'attend qu'un court délai et on ne peut pas lui demander d'attendre : `holdSleep` ne fait rien). La décision « que veut dire ce message » est pure et testée ici (`PowerMessages`, 4 tests) ; l'écoute native ne s'exécute que sous Windows.
- Bosun : `lightingSupported` est vrai sous Windows, les boutons apparaissent ; un refus du démon (pas de chien de garde, pas de fenêtre) revient par `lighting-state.json`.
- **Rien n'a été essayé sur le clavier sous Windows.** À essayer par le propriétaire, un geste à la fois : allumer, une couleur, éteindre ; veille puis réveil ; `taskkill /F /IM galleond.exe` allumé (le chien de garde doit rendre les touches) ; fermeture de session. Le retour sera noté ici.
- Limites connues : la mise en veille « moderne » (S0) n'envoie peut-être pas ces messages ; une libération lente (clavier qui ne répond plus) peut dépasser le délai de Windows.

### Thèmes : applications Linux sous Windows et six applications portables — branche `feat/theme-apps`, 14 novembre 2026
- **Défaut trouvé par le propriétaire** en installant sous Windows un thème « son » exporté sous Linux : toutes les applications y sont nommées par leur fichier de bureau (`ardour.desktop`…), Windows les rejetait toutes et refusait **le thème entier**, contrairement à ce que dit `docs/THEME_FORMAT.md`. Maintenant un nom `xxx.desktop` simple est lu, jamais résolu ni lancé sous Windows (`isPlainDesktopId` ; `lookup` ne trouve rien, `launch` refuse) : la touche est gardée, inerte, avec un avertissement.
- **Thèmes déjà exportés** : l'essai du propriétaire sous Windows (14 novembre) a montré qu'aucune application n'était trouvée à l'installation d'un thème exporté avant l'existence des identifiants portables, puisque ses touches portent le nom Linux (`ardour.desktop`) que Windows ne résout jamais. À l'installation seulement, ce nom est maintenant rapproché de l'application de la table dont il est un des noms Linux (`WellKnownApps::findByLinuxDesktopId`) et cherché par le même exécutable ; le chemin trouvé est enregistré. Rien n'est cherché à l'exécution ni à l'affichage de Bosun (ce serait un parcours du menu Démarrer à chaque redessin).
- **Six applications portables** ajoutées à la table, avec ce que le propriétaire a lu sur son PC : Audacity (3 et 4, deux paires de version distinctes qui doivent correspondre en entier), Ardour, Kdenlive, LMMS, MuseScore (`MuseScore4.exe` : une autre version demandera une ligne de plus), TuxGuitar. Plusieurs n'ont aucune information de version : seul le nom de l'exécutable les identifie. OpenShot et Avidemux sont **volontairement absents** : rien n'a été mesuré sous Windows.
- Décision du propriétaire : pas de fenêtre de remplacement ; un symbole « non installé » dans Bosun, et la touche se change dans l'éditeur habituel. **Fait** : un badge orange « ! » sur la touche d'une application absente de l'ordinateur (`BosunBackend::keys` renvoie `appMissing`), et une phrase dans l'éditeur de la touche. Sous Windows, un nom `xxx.desktop` compte toujours comme absent ; un chemin absolu, selon que le fichier existe ; un identifiant portable, par une recherche dans le menu Démarrer faite une fois par identifiant et par session (réponses gardées jusqu'à la relecture du fichier des pages, 256 au plus) : à surveiller si Bosun ralentit sous Windows. La liste des applications sur la page des thèmes du site viendra plus tard.
- **Écriture du fichier des pages sous Windows** : en changeant une touche du thème installé, Bosun a affiché « Impossible d'écrire …\galleoncore\pag… » (le fichier des pages). Même cause que pour la courbe : le renommage qui remplace le fichier est refusé quand un autre programme l'a ouvert un instant. `PagesEditor` écrit maintenant par `writeFileAtomically` (réessais bornés, droits du fichier conservés, raison de l'échec donnée). Pas de test qui reproduise le refus : il ne se produit que sous Windows, à vérifier à l'essai.
- **Isolation des tests** : `test_screencontroller`, `test_modes_flow` et `test_nowplaying_flow` partageaient un dossier de réglages fixe et ctest les lance en parallèle : un fond d'écran sauvegardé par l'un apparaissait dans le `restore()` d'un autre (échec sur Fedora 45 seulement, au hasard de l'ordonnancement). Chacun a son dossier temporaire.
- Tests : le tri des signatures (`test_wellknownapps`), l'export Linux vers l'identifiant portable (`test_desktopentries`), et deux tests Windows (`test_windowsdesktop`) que seule la CI Windows exécute.

### Éclairage en direct — trou 5 de la relecture d'Aaryush1 (suivi des réponses) — 14 novembre 2026
- **Traité** : tout envoi échoué était enregistré comme « délai dépassé », même quand le clavier avait répondu avec un statut non nul ou la fin de session ; une réponse d'un autre hôte arrivant ensuite pour la même commande était alors avalée comme « en retard, la nôtre » (`Stale`). Maintenant seul un envoi réellement parti et resté sans réponse dans le délai laisse une réponse tardive possible (`answerStateAfter`) ; une réponse reçue avec une erreur, ou un envoi jamais parti, ne laisse rien à venir, donc une réponse suivante est étrangère et le lien cède.
- **Non traité, volontairement** : une lecture (`get`) étrangère peut passer pour notre lecture du mode, car ces réponses ne portent pas d'étiquette de propriété. Son indice (l'octet 2, l'écho de session, vaut 00 chez les hôtes étrangers et porte notre identifiant chez nous) vient d'un seul hôte et d'un seul échantillon, sous Windows : **à mesurer sous Linux avant de s'en servir** pour filtrer. Sans danger pendant qu'on tient les touches ; pendant le sondage après cession, cela pourrait déclencher une reprise, et dans le chien de garde une valeur 2 déclencherait une libération.
- Tests : le suivi (`test_keyboardyield`, 2 nouveaux) et un scénario complet avec un faux clavier qui refuse une lecture puis un autre hôte qui parle (`test_keyboardlivelink`). Le test du scénario échoue sans la correction (vérifié).

### Éclairage en direct — trou 2 de la relecture d'Aaryush1 (prise des touches) — 14 novembre 2026
- Avant : `takeKeys()` lisait le mode, libérait peut-être, passait en mode 2 et ouvrait le fichier sans revérifier si un autre hôte (le Web Hub qui se connecte en plein milieu) avait parlé entre-temps : il aurait été écrasé une fois. Maintenant : le trafic étranger est classé avant chaque étape qui écrit (libération, passage en mode 2, ouverture du fichier) ; s'il y en a, la prise s'arrête, le lien passe à « cédé » (`Yielded`), rien n'est libéré par-dessus le trafic de l'autre, et le compteur de pannes n'est pas touché (ce n'est pas une panne). La reprise se fait par la règle habituelle après une minute de calme.
- Deux tests avec un faux clavier où l'autre hôte parle à un instant précis (`FakeDevice::afterCommand`) : juste après la lecture du mode, et juste après le passage en mode 2. Ils échouent sans la correction (vérifié).
- Reste de ce trou : `show()` ne vide le trafic non sollicité qu'avant les quatre rapports d'une trame, pas entre eux (jugé mineur par Aaryush1, non traité).

### Éclairage en direct — trou 3 de la relecture d'Aaryush1 (réveil) — 14 novembre 2026
- Après la veille, le clavier peut ne pas être revenu sur le bus quand le démon veut le rouvrir. Avant : un seul échec d'ouverture éteignait l'éclairage pour de bon. Maintenant : l'ouverture est retentée toutes les secondes, 30 fois au plus (`LightingController::ResumeRetryMs` / `ResumeAttempts`), l'état reste « starting » avec une explication ; au-delà, l'éclairage s'éteint comme avant.
- Deux tests : le clavier qui revient après quelques essais, et celui qui ne revient jamais. Ils échouent sans la correction (vérifié).
- Sous Linux seulement pour l'instant : la veille n'est pas encore branchée sous Windows.

### Luminosité du clavier : essai sous Windows concluant — 14 novembre 2026
- Retour du propriétaire sur la version Windows (archive de la CI, commit `d901a59`) : **le bouton radio de choix du mode fonctionne, le curseur de luminosité fonctionne, la courbe fonctionne.** Il a aussi utilisé le clavier toute la soirée et constaté que la courbe évolue bien au fil des heures.
- Avant le correctif, la sauvegarde de la courbe échouait par moments sous Windows (renommage refusé quand un lecteur a le fichier ouvert un instant) : les trois petits fichiers partagés (courbe, `keyboard-state.json`, `lighting-state.json`) passent maintenant par `AtomicFile`, qui réessaie quelques fois à quelques millisecondes d'écart.
- Toujours **non essayé sous Windows** : l'éclairage en direct (refusé : la veille n'est pas branchée), le chien de garde installé.

### Barres de défilement dans tous les onglets de Bosun — 13 novembre 2026
- Les onglets thème, Fonds d'écran, Widgets, Gaming et Économiseurs ont leur barre verticale (la même `ScrollRail` que l'onglet Clavier : elle n'apparaît que si quelque chose dépasse). La barre d'un onglet masqué n'est jamais affichée.
- Test : `everyTabHasItsScrollBarAndOnlyTheShownTabShowsOne`. Une barre placée *dans* la page du thème provoquait une boucle de mise en page : elle est posée à côté, comme les autres. À regarder sur ton écran : l'onglet « + » et les fenêtres très petites.

### Branche `keyboard` — activation de l'éclairage en direct dans Bosun — 13 novembre 2026
- Onglet Clavier de Bosun : section « éclairage en direct (expérimental) » avec un bouton allumer/éteindre, l'état en clair (éteint, démarrage, attend que le clavier soit libre, allumé, rendu à un autre programme, en pause pendant la veille), la raison d'un refus de `galleond`, et une couleur unique choisie dans les pastilles. Désactivé par défaut ; le curseur de luminosité est grisé tant que l'éclairage est actif.
- `DaemonStatus` ne fait que **demander** à `galleond` (D-Bus `org.galleoncore.Lighting`) : Bosun n'ouvre jamais le clavier. Sous Windows : indisponible, message explicite.
- Tests : cinq cas D-Bus avec un faux `galleond` sur sa propre connexion (une connexion ne reçoit pas ses propres signaux, et Qt garde en cache l'ancien propriétaire d'un nom). Les tests de `DaemonStatus` utilisent maintenant tous un faux démon séparé.
- **Pas testé sur le matériel** : aucune écriture réelle sur le clavier avant la relecture d'Aaryush1 (issue #102) et l'accord du propriétaire. Reste : sélecteur de modes à la place du lien Web Hub, effets.

### Outil de surveillance livré avec le logiciel — 13 novembre 2026

Premier des trois chantiers qui suivent la fusion de `keyboard` (PR #104). `galleon-keyboard-watchdog` est maintenant **construit avec le bureau** (`GALLEON_BUILD_KEYBOARD_WATCHDOG`, activé par défaut avec `GALLEON_BUILD_DESKTOP`) et **installé dans `bin/`, à côté de `galleond`**, qui le cherche là : l'éclairage expérimental reste refusé sans lui, donc chaque paquet doit le porter. Deb, AUR et AppImage l'ont par `cmake --install` (la construction de l'AppImage échoue s'il manque), le RPM le liste dans `%files`, le Setup Windows installe le `bin/` préparé.

- **Défaut trouvé pendant ce travail, qui aurait rendu la surveillance inutile sous systemd** : le service utilisateur de `galleond` n'avait pas de `KillMode`, donc la valeur par défaut `control-group` : à l'arrêt ou au plantage de `galleond`, systemd tue **tous les processus du service**, la surveillance comprise, au moment précis où elle sert. Les trois sources de service (`tools/systemd/galleond.service`, la version de développement, et l'unité écrite pour l'AppImage) disent maintenant `KillMode=process`, avec l'explication ; un test d'emballage vérifie les trois. **Non vérifié sur un vrai systemd** (ce conteneur n'en a pas) : à essayer par le propriétaire, de préférence sans éclairage d'abord (`systemctl --user kill --signal=SIGKILL --kill-whom=main galleond` doit laisser `galleon-keyboard-watchdog` vivant s'il était lancé).
- **Leçon d'Aaryush1 appliquée** (son Guard plantait au démarrage en écrivant son journal dans le dossier en lecture seule de l'application, sans que rien ne le signale pendant deux versions) : l'outil n'écrit **aucun fichier** et se teste **dans sa forme installée**. Nouvelle option `--dry-run` : il attend le processus comme d'habitude, puis annonce ce qu'il ferait **sans ouvrir le clavier** (donc sans jamais toucher un vrai clavier, y compris sur la machine du propriétaire). `tests/test_keyboardwatchdogtool.cpp` le lance depuis **une copie dans un dossier en lecture seule**, avec un `HOME` et des dossiers XDG inutilisables : il doit rester en attente tant que le processus vit, finir avec 0 ensuite, et ne rien écrire à côté de lui (un utilisateur root peut écrire partout : la vérification a toute sa force pour les autres, dont la CI). Une installation réelle dans un dossier temporaire a montré le programme à côté de `galleond` et exécutable ; il ne trouve `libgalleon.so` que si le dossier est dans le chemin des bibliothèques, **comme `galleond` et Bosun** (installation système attendue, pas d'`RPATH`).
- **Windows** (à vérifier par le job Windows de la CI, rien n'a pu y être exécuté d'ici) : le test natif du Setup contrôle, après installation et après mise à niveau, que le programme est à côté de `galleond.exe`, qu'il sort avec 1 sans argument et avec 0 en `--dry-run` sur un processus déjà terminé ; la désinstallation doit le retirer. Le job de la CI qui teste l'installation préparée fait la même chose avec un vrai processus qu'il attend.
- **Installation hors des dossiers système (défaut relevé par le propriétaire, 13 novembre 2026)** : après `cmake --install --prefix ~/.local`, `galleon-bosun` répondait « libgalleon.so.0: cannot open shared object file ». Les programmes installés n'avaient aucun chemin vers leur bibliothèque (seule une installation sous `/usr` marchait). Le conseil donné au propriétaire d'installer dans `~/.local` était donc incomplet. Corrigé : hors de `/usr`, le `RUNPATH` installé est `$ORIGIN/../<libdir>`, **relatif au programme** (il ne dépend pas du préfixe) ; les paquets, qui installent sous `/usr`, n'en reçoivent pas (règle Debian) ; vérifié sur une installation réelle des trois programmes lancés sans `LD_LIBRARY_PATH`, et sur une installation de type paquet (aucun `RUNPATH`). Test d'emballage : `InstalledProgramsFindTheirLibrary`.
- **Non vérifié** : un vrai systemd (voir plus haut) ; l'AppImage en mode « extraction » (le dossier extrait est supprimé à la fin de l'application : la surveillance, déjà chargée en mémoire, devrait continuer, non essayé) ; tout essai de l'éclairage sur un clavier réel.

### Décisions du propriétaire — 13 novembre 2026

- **Retour de la valeur sous Windows** : `galleond` écrit, après chaque lecture ou écriture de la luminosité, un petit fichier d'état (`keyboard-state.json`) que Bosun surveille. Fait (voir plus haut), côté Windows à confirmer.
- **Courbe de luminosité sur 24 h** (heure en X, 0 à 100 % en Y) : la courbe vit dans un **fichier JSON à part** ; deux mécanismes au choix, curseur ou courbe (voir plus haut).

### Correctifs issus d'un audit de `main` (57eeb11) — PR #96, 12 novembre 2026

Cinq défauts relevés par un audit externe, vérifiés dans le code avant correction, chacun avec son test et son commit :

- **Veille vidéo** : `ScreensaverController::setConfig` laissait `m_active` à vrai après l'arrêt du décodeur, donc la nouvelle veille ne démarrait pas sans interaction (`test_screensaver`).
- **Démarrage de FFmpeg** : `waitForStarted(3000)` bloquait la boucle qui envoie aussi le keep-alive de 500 ms. Le démarrage est maintenant asynchrone (`started`, erreur de démarrage, minuteur de 3 s, annulation par une interaction). **Pas de test dédié** : sous Linux le lancement est confirmé presque aussitôt, aucun test fiable n'échoue sans le correctif.
- **Molette à l'import d'un thème** : les actions `page` de `turn`/`press` suivent le nouveau nom de page, comme celles des touches (`test_themeinstall`).
- **Import sur volume insensible à la casse (Windows)** : noms d'icônes et de dossiers de fonds comparés sans tenir compte de la casse, et un fichier existant n'est jamais remplacé, donc un échec d'écriture de `pages.json` ne supprime plus un fichier de l'utilisateur (`test_themeinstall` pour le choix des noms ; pas de test d'écriture réelle sur volume Windows).
- **Outil de luminosité** : une file jamais vidée pendant la purge (`StaleReports`, rien n'est envoyé) et une réponse hors 0..1000 (`InvalidReply`, au lieu d'une troncature à 16 bits : 65 578 se lisait 42) sont refusées (`test_keyboardlink`).

**Suites de la relecture (PR #98 fusionnée, PR #99 en brouillon)** :

- Relance de FFmpeg : `start()` indique si un décodeur a été lancé et la relance a son propre drapeau (`m_restarting`), donc le résultat ne dépend plus de l'ordre de `started()` (Qt sous Windows peut l'émettre avant le retour de `start()`). Une relance qui échoue, expire ou est annulée par une touche annonce maintenant la fin de la veille (`activeChanged(false)`). Le test de relance passait déjà avant le correctif : l'ordre des signaux sous Windows n'a pas pu être reproduit sous Linux, et aucun test ne couvre une relance qui échoue en asynchrone.
- Création des images d'un thème avec `QIODevice::NewOnly` (plus de trou entre la vérification et l'écriture ; un lien symbolique pendant n'est pas suivi). Test : `aPathTakenByAnEntryThePlanCannotSeeIsNotReplaced`.
- PR #99 : une piste qui démarre pendant le lancement du décodeur n'allume plus la veille quand `whilePlaying` est faux (`onStarted()` relit l'état du média ; nettoyage commun `abandonStart()`). Test avec un faux lecteur, vu en échec sans le correctif.

Validation : CI GitHub verte et `tools/check.py full` réussi (Docker, Ubuntu 24.04) sur l'arbre fusionné. L'import de thème en général est maintenant confirmé sur le module sous Windows (voir l'entrée suivante). **Non vérifié** : le cas précis d'un nom de fichier en casse différente dans un thème (la correction d'insensibilité à la casse elle-même), la relance de FFmpeg sous Windows, et sur le module / le clavier réel (outil de luminosité) ; à confirmer par le propriétaire avant de classer ces points « sur le module ».

### Thème « voice-chat » et intégration Discord confirmés sur Windows — 12 novembre 2026

Après l'échec attendu de l'import d'un ancien thème entièrement Linux (une touche référence une appli par un identifiant `.desktop` littéral, portable seulement via la table `WellKnownApps` — voir [docs/protocol/README.md](docs/protocol/README.md), ce n'est pas un plantage : `galleon-bosun.exe` reste actif, le rejet est le comportement voulu), le thème [`voice-chat.gctheme`](site/themes.html) (déjà publié, construit avec des touches clavier F14-F18 portables) a été importé et utilisé en conditions réelles sous Windows, avec le build complet déployé par la CI de la PR #99 (`galleon-presentmon-probe.exe` et `galleon-gpu-probe.exe` inclus, contrairement à un build de développement partiel compilé à la main plus tôt dans la journée, qui en manquait et affichait « Assistant FPS absent »).

Constaté par le propriétaire :
- Les cinq touches Discord (micro, sourdine, parler, caméra, partage d'écran) fonctionnent avec les raccourcis F14-F18 configurés directement dans les paramètres de Discord. **Sous Windows, aucun réglage supplémentaire dans les paramètres système n'est nécessaire** (Discord y reconnaît F14-F18 nativement), à la différence de Linux/KDE Plasma où l'injection arrive comme « Lancer (5)-(9) » et doit être réassociée dans les raccourcis clavier de KDE — voir les étapes mises à jour sur [site/themes.html](site/themes.html).
- Le widget Accueil (FPS/CPU/GPU) a affiché Counter Strike 2 à 136 FPS sur l'écran du module pendant cet essai.
- Le Mode Jeu de Windows, qui ne fonctionnait plus sur CS2 et RDR2 après un essai précédent avec le build de développement partiel ci-dessus (sans les deux outils FPS/GPU, donc sans l'assistant que le widget Accueil lance), fonctionne normalement avec ce build complet — confirmé par le propriétaire sur les deux jeux. Aucune preuve (journal d'événements Windows, session ETW restée active) n'a été trouvée reliant le souci précédent au code de ce dépôt ; le plus probable reste le build de développement incomplet, sans certitude absolue faute de journal de plantage de `galleond` à ce moment-là.

Un thème de raccourcis clavier peut donc être combiné avec le widget FPS/CPU/GPU pour un usage de streaming : les commandes d'appel côté Discord, les performances en direct sur l'écran du module — mention ajoutée sur [site/themes.html](site/themes.html).

### Onglets Bosun à largeur adaptative — 31 octobre 2026

Le titre actif conserve sa largeur complète, mesurée avec la police affichée.
La réduction se répartit entre les autres onglets, jusqu’à un minimum lisible ;
la barre reste défilante si nécessaire et garde l’onglet sélectionné visible.
Une transition QML d’une seconde, avec accélération et décélération douces,
montre l’onglet actif grandir pendant que les autres rétrécissent ; le défilement
suit la géométrie pendant l’animation. Les changements rapides repartent de la
largeur courante. Les captures statiques désactivent cette animation.
Le sélecteur de langue conserve sa position, sa taille et son fonctionnement.
Calcul pur dans `TabWidths.js`, six tests de géométrie et test QML avec huit
thèmes longs en allemand, largeur intermédiaire pendant l’animation puis titre
entier à l’arrivée ; contrôles ciblés et aperçu vérifiés. La validation
complète de référence est indiquée par `tools/check.py status`. Le propriétaire
autorise la création de la PR de préparation 0.3.0 le 31 octobre.

### Langues de Bosun — 31 octobre 2026

Interface et menu de notification : choix immédiat de la langue du système,
anglais, français, allemand ou espagnol, mémorisé séparément des profils.
Catalogues intégrés aux mêmes paquets Linux/Windows, sans sous-paquets de langue.
Les libellés personnels et le brouillon d’une touche sont conservés. Les messages
techniques provenant du système et les guides externes gardent leur langue propre.
Les messages fixes de capture reçus du démon sont traduits à l’affichage dans
Bosun, sans modifier les données IPC.
Tests de sélection, persistance, repli, traduction QML/menu et intégrité des
catalogues ajoutés. Les contrôles ciblés locaux et les 86 tests Python ont passé.
La validation complète de référence et l’arbre vérifié sont consultables avec
`tools/check.py status` (commande : `tools/check.py full`). Traductions allemandes
et espagnoles préparées avec assistance IA, relecture native bienvenue.
Inclure aussi le correctif local d’édition des raccourcis dans la prochaine PR,
à la demande explicite du propriétaire. Aucun nouvel essai matériel revendiqué.

### Édition des raccourcis Bosun — 30 octobre 2026

Le brouillon d’une touche reste ouvert lors des rafraîchissements du backend
si la page et le raccourci enregistré sont inchangés. Changer de touche recharge
les champs ; les champs texte et icône réagissent uniquement à la saisie utilisateur.
Contrôles ciblés Docker réussis : 10 tests sur 47 sous GCC, clang-tidy,
qmllint, tests de l’outil et REUSE ; sans sanitizers.
Binaire local recompilé ; le propriétaire confirme après relance de Bosun que
l’édition ne se ferme plus. Aucune nouvelle validation sur le module ni
validation complète revendiquée.

Le 20 octobre, le propriétaire confirme le fonctionnement du correctif des fonds personnels et du bouton « Parcourir » (PR #24).

« Testé » = tests Qt automatiques dans `tests/`, sous `-Werror`. « Sur le module » = confirmé par l'utilisateur sur le matériel (photo ou retour).

| Fonction | Tests | Sur le module |
|---|---|---|
| **Gaming : logiciel d’enregistrement** (24 octobre) : préférence Automatique/Game Bar/OBS, défaut Windows Game Bar et Linux OBS, logiciels absents grisés ; onglet Gaming unique (réglages Mode Jeu et enregistrement), thème runtime Gaming du pad avec quatre actions adaptées et icônes intégrées, page de touches présente uniquement pendant le jeu et retirée à sa fermeture même après un changement de mode, aide CSV MangoHud dans le dossier utilisateur, commandes OBS WebSocket 5 authentifiées et états confirmés, Game Bar par raccourcis sans état supposé ; réglages locaux persistants | oui, ciblés Linux (`test_recording`, `test_pagescontroller`, `test_bosun`) et rendu QML ; exécution native Windows à faire | à confirmer avec OBS et Game Bar réels ; [configuration](docs/GAMING_RECORDING.md) |
| Fond d'écran + heure/date (locale du système) sur l'écran 720×384, redessiné chaque minute | oui | oui |
| Fond retenu (`galleond.conf`) et affiché dès le démarrage ; avertissement si le fond sauvegardé n'existe plus | oui | oui |
| Images des 12 touches (JPEG 160×160), dessin partiel (seule la touche changée est renvoyée) | oui | oui |
| Entrées : 12 touches, 2 molettes (rotation + appui), décodage validé et fuzzé | oui + fuzz | oui |
| Pages de boutons en JSON (`~/.config/galleoncore/pages.json`), validation stricte (champ inconnu = fichier refusé), rechargement à chaud | oui | oui (déplacement d'une touche pendant l'exécution) |
| Contrôle MPRIS : lecture/pause, suivant, précédent, stop, volume, avance/recul ; volume par pas cumulés | oui (faux lecteur sur le bus) | oui (VLC, Clementine) |
| Écran « en cours de lecture » : titre, artiste, chronologie, pochette ; mise à jour quasi instantanée au changement de piste (`PropertiesChanged`) | oui | oui (pochette réelle vue) |
| Pochettes : fichier local, https seulement (pas d'IP ni de nom local), vignette vidéo FFmpeg en cache, icône du lecteur en dernier recours | oui | partiel (pochette et icône vues ; vignette vidéo non confirmée) |
| Touches « application » : lancement d'un `.desktop` (`gio launch`, sans shell), icône et nom pris du `.desktop`, anti-rebond ; fichier de pages réservé à son propriétaire | oui | oui (Firefox, Dolphin) |
| Sélecteur d'applications (`--apps`, `--list-apps`, `--assign-app`, `--clear-key`) | oui | non confirmé |
| Thèmes : pages colorées (Contrôle jaune, Multimédia cyan, Son rouge, Jeux bleu marine, Système vert, Pavé numérique blanc) ; clic molette gauche = mode choix du thème, rotation = thème suivant/précédent, second clic ou 30 s = retour au contrôle | oui | oui |
| Ligne de couleur et nom du thème en haut de l'écran ; bandeau `◀ Nom ▶` pendant le choix | oui | oui |
| Anneaux des molettes indépendants : gauche = couleur du thème, droite = blanc (contrôle) ou magenta (choix du fond) ; rapport `03 24` par LED | oui | oui (couleurs vues) |
| Choix du fond par la molette droite : clic = mode fond (anneau et barre du bas magenta), rotation = fond suivant/précédent, second clic ou 30 s = retour ; le fond s'affiche même si une piste joue | oui | oui (validé par l'utilisateur) |
| Un seul `galleond` à la fois : le nom D-Bus est pris avant d'ouvrir le module, un second démarrage s'arrête (code 1) sans toucher au module | manuel | à confirmer |
| Service systemd utilisateur (`tools/systemd/galleond.service`, variante `galleond-dev.service` pour l'arbre de compilation) | `systemd-analyze verify` | démarre et pilote le module ; **démarrage à l'ouverture de session confirmé après un redémarrage de la machine** (19 octobre, 14:45 : module ouvert, pages chargées, fond affiché sans commande) ; **reprise après veille de l'ordinateur confirmée côté module** (19 octobre : même processus avant/après, le module a continué à répondre et la veille animée a démarré ; le message « main thread busy 107180 ms » est le temps gelé pendant la veille). Le problème de réveil de la carte graphique (NVIDIA RTX 5060 Ti, `PreserveVideoMemoryAllocations: 0`) est hors du projet |
| Écran de veille animé : vidéo verticale (FFmpeg → images brutes) découpée pour l'écran 720×384 et les 12 touches 160×160, boucle, délai d'inactivité, réveil par touche/molette/début de piste, suspension des autres contrôleurs ; section `screensaver` du JSON (géométrie en mm exigée) | oui (découpage, FFmpeg réel, réveil, médias) | oui : essai du 19 octobre avec les deux vidéos et une géométrie *estimée* (« un vrai succès » selon l'utilisateur) ; géométrie désormais **mesurée par photo** (±0,5 mm, voir `data/devices/galleon-100-sd.json`) ; l'utilisateur a refait les images avec l'action en haut ; la veille est dans son vrai `pages.json` (5 min, 2 vidéos de `public/Screensavers/`, sans logo, avec `PROVENANCE.md` complet (prompts de ChatGPT Images et de `p-video`) et licence CC-BY-SA-4.0) |
| Débit du module mesuré (`--measure-throughput`) : écran 44,7 i/s, touches 35 tours/s, écran + touches 20 tours/s | — | oui (mesuré le 19 octobre) |
| Thème « Pavé numérique » (touches = clavier virtuel uinput, liste fixe de touches, touche maintenue tant qu'on appuie) choisi tout seul quand aucun média ne joue, Contrôle quand un lecteur joue (`"when"`) ; touches teintées d'une version sombre de la couleur du thème | oui | oui (confirmé par l'utilisateur le 19 octobre) (retour de l'utilisateur) |
| Test de bout en bout des molettes (`test_modes_flow` : vrais widgets, faux module, même câblage que `galleond` via `connectPagesToScreen`) : thème sans média, rotation, volume rendu à la sortie du mode, choix du fond validé | oui | — |
| Molette gauche sans lecteur : règle le volume du système (`wpctl`, PipeWire) ; le fond choisi reste 4 s à l'écran avant que la lecture reprenne ; la veille laisse au moins la moitié du temps du démon libre | oui | oui (confirmé par l'utilisateur le 19 octobre) |
| Clic de molette : le cran parasite juste avant ou après l'appui est ignoré (fenêtre de 350 ms avant l'appui, 250 ms après l'appui ou le relâchement), pour que le fond ou le thème choisi ne bouge pas à la validation | oui (`test_modes_flow`) | oui (confirmé par l'utilisateur le 19 octobre) |
| Choix du fond validé tout seul après 4 s sans toucher la molette (`screen.wallpaper_validate_seconds`) ; le fond choisi reste affiché avec un décompte (`wallpaper_hold_seconds`) avant le retour de la lecture | oui | oui (confirmé par l'utilisateur le 19 octobre) |
| Choix du fond précis : la molette compte chaque cran, le premier cran est dessiné tout de suite (≈45 ms mesurées : rendu 20 ms, encodage 1 ms, envoi ≈ 25 ms) et la rafale suivante est regroupée, le réglage n'est écrit qu'à la validation, ordre naturel stable du catalogue (styles par nom, `-2` avant `-10`), nom et position (`portrait-01   10/24`) affichés pendant le choix | oui | oui (confirmé par l'utilisateur le 19 octobre) |
| Lecture des molettes : tous les rapports en attente sont lus à chaque tour (avant : un seul toutes les 10 ms, une file pouvait retarder un cran de plusieurs secondes) ; témoins de blocage du démon dans le journal | oui (`test_streamdeck`) | oui (confirmé par l'utilisateur le 19 octobre) (retard d'environ 2 s mesuré sur la vidéo du 19 octobre) |
| Choix du fond : après un cran accepté, les rotations sont ignorées (pas mises en file) jusqu'à ce que l'écran ait réellement affiché le fond du cran, puis pendant un repos `screen.wallpaper_step_delay_ms` (150 par défaut, 0 = aucune règle) ; délai de secours 1,5 s ; images allégées pendant le choix | oui (`test_modes_flow`, `test_pagescontroller`) | oui (confirmé par l'utilisateur le 19 octobre) |
| Choix du fond : au clic, les rotations sont ignorées jusqu'à l'affichage du nom et de la place (`NN/24`) ; le décompte de validation (`wallpaper_validate_seconds`, 4 s) s'affiche pendant que le choix reste possible (anneau magenta), l'anneau repasse en blanc à la fin ; puis le fond reste `wallpaper_hold_seconds` secondes avant la lecture | oui | oui (confirmé par l'utilisateur le 19 octobre) |
| Affichage immédiat : le module n'affiche une image que si un second envoi la suit ; chaque image (écran et touches) est envoyée deux fois de suite, sauf les images de la vidéo de veille (`flush = false`) | oui | écran : oui (vidéo du 19 octobre, variante B) ; touches : oui (confirmé par l'utilisateur le 19 octobre) |
| Applications et jeux les plus utilisés : `sort: usage` (score décroissant 30 jours par lancement, fichier borné), étoile sur le n°1 ; `steam: {max}` remplit les touches libres avec les jeux Steam installés (temps de jeu et dernier lancement lus localement, sans réseau ni clé) ; action `steam` ; option `menu: {category, max}` : applications du menu KDE d'une catégorie (`Game`), classées par usage et, pour un raccourci Steam, par le temps de jeu local de son jeu | oui (`test_usage`, `test_pageorder`, `test_desktopentries`, `test_pagescontroller`, `test_pagesconfig`) | à confirmer |
| Commandes média en plus : `shuffle`, `repeat`, `mute` (son du système par `wpctl`), `raise`, `fullscreen`, `restart` (MPRIS) ; les touches aléatoire et répéter affichent leur état (mots réglables par `state_labels`, fond plus clair quand actif ; MPRIS ne distingue pas album et liste) | oui (`test_mprisController`, `test_pagescontroller`, `test_pagesconfig`) | à confirmer |
| **Galleon Bosun, maquette** (`bosun/`, `galleon-bosun`) : fenêtre QML avec un onglet par thème, la grille des 12 touches dessinée par `TileRenderer` (comme le module), le détail d'une touche (lecture seule), rechargement quand `pages.json` change ; onglets Clavier, Fonds d'écran et Écrans de veille annoncés mais vides. Options `--pages`, `--tab`, `--key`, `--screenshot` | oui (`test_bosun`) | sans objet (fenêtre de bureau : à essayer par l'utilisateur) |
| **Galleon Bosun, onglet Écrans de veille** (tranche 1, lecture seule) : réglages (actif ou non, délai, fps, pendant la lecture, taille de l'image découpée), liste des vidéos avec vignette (FFmpeg, cache partagé avec galleond), taille, fréquence, durée et codec par `ffprobe` (`VideoProbe`, sans shell, délai, protocole `file` seul, sortie bornée), remarques (partie coupée, vidéo plus petite que l'écran, fichier absent, `ffprobe` absent) | oui (`test_videoprobe`, `test_bosun`) | sans objet (fenêtre de bureau : à essayer) |
| **Galleon Bosun, écrans de veille, tranche 2** : réglages modifiables (délai de 1 à 720 min avec raccourcis, images par seconde 1 à 20, pendant la lecture), titre d'une vidéo, retrait d'une vidéo (confirmation par second clic ; le fichier vidéo n'est jamais touché ; la dernière retirée éteint l'écran de veille sans effacer les réglages). Écriture par `PagesEditor` : validée comme tout fichier de pages, atomique, copie `.bak`, fichier déjà invalide laissé intact | oui (`test_pageseditor`, `test_bosun`) | sans objet (fenêtre de bureau : à essayer) |
| **Galleon Bosun, écrans de veille, tranche 3a** : ajouter une vidéo telle quelle. Le premier bouton « Browse… » ouvre le sélecteur natif ; on peut aussi déposer un fichier sur la fenêtre ou écrire son chemin ; il est seulement regardé (vignette, taille, fréquence, durée, codec par `ffprobe`, remarques : partie coupée, plus petit que l'écran, moins d'images par seconde que le réglage) jusqu'à l'ajout ; refus si ce n'est pas une vidéo lisible, si ffprobe ne la lit pas, si la liste est pleine (8) ou si elle y est déjà (même fichier écrit autrement). Le fichier n'est ni copié ni converti. Le texte des remarques est une fonction pure (`VideoAdvice`) | oui (`test_videoadvice`, `test_pageseditor`, `test_bosun`) | sans objet (fenêtre de bureau : à essayer) |
| **Galleon Bosun, écrans de veille, tranche 3b-1 : le moteur du rognage et de la conversion** (sans interface encore). `CropMath` : rectangle de rognage aux proportions de l'image du module, toujours dans la vidéo (centrer, ajuster, déplacer, zoomer autour d'un point). `ScreensaverSplitter::regions` : où tombent l'écran et les 12 touches dans l'image. `VideoConverter` : FFmpeg sans shell, priorité minimale, protocole `file` seul, sans audio, 120 s au plus, un à la fois, écrit sous un nom temporaire puis renomme, jamais par-dessus un fichier, noms sûrs, progression, annulation, délai de 30 min. Résultat dans `~/.local/share/galleoncore/screensavers/` puis ajouté à la liste ; l'original n'est jamais touché. Encodeur libx264, sinon mpeg4. Côté `BosunBackend` : `startConversion`, `cancelConversion`, état `conversion` | oui (`test_cropmath`, `test_videoconverter`, `test_screensaver`, `test_bosun`) | sans objet (pas d'interface encore) |
| **Galleon Bosun, onglets de thème éditables** : une touche fait une application (liste du menu KDE avec icônes), une commande du lecteur (volume et déplacement avec leur pas), l'ouverture d'une page, un retour ou une touche du pavé ; texte et icône réglables ; vider une touche. Le thème (onglet) : nom, couleur (palette ou #rrggbb), ordre, retrait (jamais le dernier ni celui de la page d'accueil ; la page part avec lui sauf si une touche y mène) ; onglet « + » pour en créer un (page vide de 12 touches, nom de page tiré du nom). Les sous-pages (ex. `multimedia-plus`) s'éditent depuis l'onglet dont les touches y mènent (de proche en proche), et seulement celui-là. Chaque onglet peut avoir sa page « plus » (`main-plus`, `jeux-plus`… : nom tiré de celui de la page) : bouton dans l'éditeur de l'onglet, ou case à cocher à la création ; la dernière touche libre y mène, une touche Retour y revient ; elle part avec son onglet quand rien d'autre n'y mène. Tout s'écrit dans `pages.json` (copie `.bak`) que galleond relit seul : le changement atteint le module aussitôt et reste après redémarrage. Barre d'état : galleond tourne-t-il (nom D-Bus), bouton pour le démarrer (`systemctl --user start galleond`) | oui (`test_pageseditor`, `test_pagename`, `test_daemonstatus`, `test_bosun`) | à essayer |
| **Easter egg caché** : petit effet visuel caché sur l'écran et les touches, déclenché par une combinaison volontairement non décrite ici (ni la combinaison ni l'effet, pour ne pas gâcher la découverte) ; borné dans le temps, inhibé en jeu et pendant l'écran de veille, décision pure testée séparément du rendu. **Aucune écriture sur le clavier**. Détails dans le code (`daemon/MatrixTrigger.*`, `daemon/MatrixRainController.*`, `widgets/MatrixRain.qml`) | oui (`test_matrixrain`) | confirmé le 11 novembre (écran et les 12 touches, build Windows natif) |
| **Galleon Bosun, écrans de veille : rognage et conversion** (interface). Une vidéo choisie (dépôt ou chemin) s'ajoute telle quelle ou « coupée à la taille du module » : fenêtre de rognage avec le cadre jaune aux proportions de l'image du module (déplacer à la souris, molette ou boutons pour zoomer, centrer) et, dedans, l'écran (cyan) et les 12 touches dessinés là où ils tomberont ; FFmpeg écrit une nouvelle vidéo 720×1258 aux images par seconde du réglage dans `~/.local/share/galleoncore/screensavers/`, barre de progression, bouton Stop ; le résultat est ajouté à la liste ; l'original n'est jamais touché. Une vidéo déjà dans la liste peut être recoupée ; liste pleine ou FFmpeg absent : dit pourquoi. Commande FFmpeg essayée pour de vrai sur la vidéo d'exemple (704×1280 → 720×1258, h264, 15 i/s, 1 s) | oui (`test_bosun`) ; FFmpeg réel essayé à la main | à essayer |
| **Galleon Bosun, onglet Fonds d'écran** : miniatures (par style), clic pour utiliser (appliqué aussitôt si galleond tourne, retenu pour le prochain démarrage dans `galleond.conf`), image en cours repérée (magenta), ajout d'une image personnelle (bouton « Browse… » en premier, dépôt ou chemin ; fenêtre de rognage au format de l'écran 720×384 ; enregistrée en JPEG 1440×768 dans `~/.local/share/galleoncore/wallpaper/mine/`, jamais par-dessus un fichier), suppression de ses propres images seulement (pas celles du projet, pas celle en cours). Côté démon : le catalogue lit plusieurs dossiers (le sien, le système, l'arbre source) au lieu du premier trouvé, se relit à `SetWallpaper` inconnu et à l'entrée du mode choix ; le dossier personnel est conservé même absent au démarrage pour découvrir le premier import sans redémarrer le démon. Fichiers vérifiés (taille, dimensions, contenu) avant décodage | oui (`test_wallpaperlibrary`, `test_wallpapercatalog`, `test_screencontroller`, `test_filenames`) | à essayer |
| **Galleon Bosun, onglet Clavier** : bouton qui ouvre le Web Hub de Corsair (`https://www.corsair.com/web-hub`, page officielle de l'aide Corsair) dans le navigateur de l'utilisateur, avec ce qu'il faut savoir (navigateur Chromium, rien n'est écrit dans la mémoire du clavier par cette application). Pas de webview : lien seul | sans objet (lien) | à essayer |
| **Galleon Bosun, onglet Clavier : curseur de luminosité** (12 novembre 2026) : onze crans de 0 à 100 % par pas de 10 % (valeurs 0, 100 … 1000 de la propriété `0xEA`), valeur et graduations affichées, clavier (flèches) et souris, rien n'est envoyé pendant le glissement, une seule demande au relâchement. Bosun n'ouvre jamais le HID : il demande à `galleond`, par D-Bus (`org.galleoncore.Keyboard`, objet `/org/galleoncore/Keyboard`) sous Linux et par une action du point d'accès local (`keyboard-brightness`) sous Windows. Nouveau composant `KeyboardController` du démon, séparé de l'écran et des pages (câblé dans `main.cpp` seulement) : rien n'est ouvert au démarrage ni gardé, l'interface fabricant (`MI_01`) n'est ouverte que le temps d'une demande puis fermée (donc le Web Hub n'est jamais tenu à l'écart), seuls les pas de 100 sont acceptés, une demande à la fois (la plus récente remplace celle en attente), au moins 300 ms entre deux écritures (la valeur est conservée par le clavier et peut atteindre sa mémoire), écriture sur un fil à part pour ne pas bloquer le keep-alive du module. La valeur reste dans le clavier : pas de restauration à la sortie, c'est le choix de l'utilisateur. **Limites** : le curseur ne lit pas la valeur actuelle (il ne montre que ce qui a été réglé d'ici) ; les onze crans ont été essayés par le propriétaire sur une unité (firmware 2.3.88) ; ni mémorisation ni cession de place au Web Hub (fermer le Web Hub d'abord). Lien « Trouver des thèmes sur le site » ajouté au-dessus de l'installation d'un thème (onglet +) | oui (`test_keyboardcontroller` avec un faux clavier, `test_localcontrol`, `test_bosun`) | oui : curseur essayé par le propriétaire sur son clavier le 12 novembre 2026, les onze crans (0 à 100 %) ont été essayés progressivement, tous fonctionnent ; le lien « Trouver des thèmes sur le site » fonctionne aussi |
| **Galleon Bosun, icône dans la zone de notification** : fermer la fenêtre la masque (si la zone existe), clic gauche affiche ou masque, clic droit : Afficher/Masquer, « Quitter et arrêter galleond » (le module repasse en pavé numérique) et « Quitter » (galleond continue), ou « Démarrer galleond » quand il ne tourne pas ; menu refait selon l'état. Sans zone de notification, fenêtre ordinaire. Le contenu du menu est une fonction pure | oui (`test_traymenu`, fermeture = masquage dans `test_bosun`) ; l'icône elle-même ne se teste pas hors bureau | à essayer |
| Plugin OpenRGB (LampArray en libusb) : anneaux des molettes pilotés depuis OpenRGB | oui | oui (les anneaux ; le clavier non, voir §4) |
| **Bureau Windows** : SMTC, Core Audio, SendInput, IPC/verrou par utilisateur, Chrome/App Paths/PWA distinctes, icônes natives, Setup NSIS UTF-8, démarrage de galleond et Bosun minimisé par HKCU Run ; Bosun lance le démon absent | oui (tests natifs, déploiement, installation/mise à niveau/désinstallation avec applications ouvertes et données conservées) | partiel : fonds et pages présents ; stabilité et ouverture de session à revalider |
| **Pages initiales fixes Linux/Windows** : cinq onglets, au plus 12 touches par page, navigateurs/lecteurs et outils système pertinents prioritaires, jeux Steam en premier ; détection unique ; retraits et positions conservés ; `.url` Steam Windows bornés et validés | oui (`test_initialpages`, `test_pagescontroller`, `test_usage`, `test_windowsdesktop`) | nouvelles pages Linux à confirmer ; Windows à revalider après ce correctif |
| **Réactivité Windows** : recherche d’icônes/jaquettes dans un enfant limité, annulation des requêtes périmées, dessin échelonné des 12 touches ; keep-alive connu servi entre rapports JPEG longs | oui (`test_launchercache`, `test_streamdeck`, `test_windowsdesktop`) | retour au logo Corsair et longues sessions à revalider |
| **Réveil sans attente du décodeur** : arrêt asynchrone FFmpeg et nettoyage borné, première action consommée | oui (`test_screensaver`, `test_windowswake`, `test_modes_flow`) | réveil court Linux observé avant ce correctif ; réveil prolongé Linux/Windows à confirmer |
| **Compteurs de diffusion du site** : totaux directs GitHub lus à l'ouverture après consentement (sinon dernier relevé quotidien), écarts directs sur sept jours ; PPA/COPR vérifiés chaque semaine et intervalle indiqué, source indisponible affichée comme telle, votes AUR séparés des téléchargements | oui (`test_download_stats`, `test_download_stats_js`) ; affichage en production à vérifier après fusion | sans objet (statistiques de distribution) |
| **Confidentialité du site** : pages Confidentialité et CGU en français/anglais ; polices et badges tiers désactivés jusqu'au choix explicite ; refus aussi simple que l'acceptation, choix modifiable depuis le pied de page | oui (vérification du site et du consentement) ; à confirmer après déploiement Pages | sans objet (site web) |
| **Découverte du site** : descriptions des trois pages, Open Graph et Twitter Card sur l’accueil, `robots.txt`, sitemap XML, favicon SVG/ICO/PNG et icônes Apple/Android dérivées du logo Bosun | oui (validation locale des références, formats et dimensions) ; déploiement Pages à vérifier | sans objet (site web) |

## 2. En cours

### Icône des fichiers `.gctheme` — branche `feat/theme-file-icon`, 13 novembre 2026

Pas encore fusionnée ni en pull request. Les fichiers de thème doivent avoir une icône propre dans les gestionnaires de fichiers ; l'icône est **livrée par le logiciel**, jamais dans le fichier de thème (le format reste inchangé).

- **Fait** : type MIME `application/x-galleoncore-theme` (`bosun/mime/galleoncore-theme.xml`, glob `*.gctheme`, sous-classe de `application/zip`, commentaire en fr/de/es) ; icône `application-x-galleoncore-theme` en PNG de 16 à 512 px dans hicolor (`mimetypes`), installée par `bosun/CMakeLists.txt` (donc par les paquets deb/PPA, AUR et AppImage) et listée dans le `%files` du paquet RPM/COPR ; sous Windows, Setup (`galleoncore.nsi`) enregistre `.gctheme` pour l'utilisateur courant seulement (HKCU), ProgID `GalleonCore.Theme`, libellé « Thème GalleonCore », `DefaultIcon` vers `gctheme.ico` (installé dans le dossier d'installation), `SHChangeNotify` ; la désinstallation retire exactement ces clés (et l'association `.gctheme` seulement si elle est toujours à nous), la mise à niveau les réécrit à l'identique. Pas de verbe d'ouverture, pas de HKLM.
- **Pas de `MimeType=`** dans `galleon-bosun.desktop` : Bosun n'ouvre aucun fichier passé en argument (aucune action d'ouverture inventée). Pas de recette Flatpak ni Snap dans le dépôt.
- **Icône** : le SVG fourni par le propriétaire (barre de navire en dégradé arc-en-ciel, trois images et un pinceau) remplace l'icône provisoire ; lisible de 16 à 256 px à l'aperçu. Pour la changer à nouveau : `tools/theme-icon/source/galleoncore-theme.png` puis `python3 tools/theme-icon/build_icons.py` (voir `tools/theme-icon/README.md`). Provenance (ChatGPT, prompt du propriétaire) consignée dans `ASSETS.md` ; la date y est celle de la session, à corriger si besoin.
- **Testé** : `tools/test_theme_icon.py` (XML MIME valide et accepté par `update-mime-database`, glob, sous-classe, nom d'icône identique à celui des paquets, tailles PNG et `.ico`, `%files` RPM, clés NSIS HKCU seulement et symétrie installation/désinstallation, icônes livrées = sortie du script) ; le script NSIS compile avec `-WX` ; `reuse lint`. `test-setup.ps1` vérifie maintenant les clés après installation, après mise à niveau et après désinstallation (CI Windows).
- **Validation complète (`tools/check.py full`) : non faite** — la session de développement n'avait pas Docker utilisable (Docker Hub répondait 429). Obligatoire avant toute fusion ou pull request ; aucun C++ n'a changé, seules les lignes `install` de `bosun/CMakeLists.txt` le concernent.
- **Non vérifié** : Setup réel sous Windows (les assertions de `test-setup.ps1` n'ont pas encore tourné), affichage de l'icône dans l'Explorateur, dans Dolphin ou un autre gestionnaire de fichiers Linux. À confirmer par le propriétaire avant de classer cette entrée « sur le module/le bureau ».

## 3. À vérifier ou corriger

- **Stabilité du démon** : exécution longue (heures), reprise après veille, débranchement/rebranchement du module (le redessin complet est déjà prévu sur `deckAvailableChanged`), fuites : caches bornés (images, icônes, applications) mais pas mesurés sur la durée.
- **Rechargement à chaud** : confirmé pour un déplacement de touche ; à vérifier : fichier remplacé par un éditeur (écriture en plusieurs étapes), fichier invalide pendant l'exécution (les anciennes pages doivent rester), changement de thèmes pendant qu'un mode est actif (le mode est remis à zéro, voulu).
- **Conflits entre instances** : deux `galleond` sont maintenant exclus (le second quitte). OpenRGB (plugin) et `galleond` écrivent tous deux les anneaux : à ne pas lancer ensemble sans règle (non résolu).
- **Isolation des tests** : les tests écrasaient `galleond.conf` (corrigé : `XDG_CONFIG_HOME` isolé, dans le code des tests et par ctest) et leurs faux lecteurs MPRIS apparaissaient sur le bus de l'utilisateur, ce qui perturbait un `galleond` en marche (corrigé : ctest lance chaque test sous `dbus-run-session`). Ne lancer les tests que par `ctest`. Vérifier qu'aucun autre test n'écrit dans le vrai `~/.config` ou `~/.cache`.
- Ligne fantôme sous les étiquettes des touches sur les photos : probablement optique, jamais confirmé.
- Correctifs de la PR #96 : démarrage asynchrone de FFmpeg et import de thème (collision de casse) à essayer sous Windows ; outil de luminosité à ré-essayer sur le clavier (lecture et écriture 100 puis 1000).
- Vignette vidéo FFmpeg, anti-rebond et lancement Kdenlive/Steam : construits et testés, pas rapportés sur le module.
- Fuzzing sur le matériel réel (le sandbox de libFuzzer bloque ~90 s ici).
- Clavier : RVB par touche **bloqué** (voir `docs/protocol/README.md`). Ne jamais rejouer les écritures du Web Hub (profil en flash) ; les valeurs 3 et 5 de la propriété 3 sont des bootloaders ; la commande 0x0f est une remise à zéro d'usine.

## 4. À développer

1. **Galleon Bosun** (interface graphique de réglage ; nom choisi le 19 octobre). Décision : **QML** (Kirigami) + un petit module C++ qui réutilise `parsePagesConfig`, `PagesEditor` et le rendu des touches (pas de seconde validation à maintenir) ; application séparée `galleon-bosun`. Elle ne parle au démon que par D-Bus ou par le fichier JSON validé, jamais au matériel ; écriture atomique avec sauvegarde datée avant chaque enregistrement. Onglets prévus (à valider avec l'utilisateur au fur et à mesure) :
   - **Clavier / LED** : lien vers le Corsair Web Hub, ouvert par `xdg-open` (navigateur ou PWA de l'utilisateur). Pas de webview intégrée tant que les conditions d'utilisation de Corsair n'ont pas été lues et jugées compatibles ; explication honnête de ce qui marche sous Linux (OpenRGB).
   - **Un onglet par thème** : touches par défaut mais modifiables (ajout, retrait, icône), couleur de l'onglet, ajout et suppression d'onglet (garde-fous : jamais le dernier thème ni la page de repli ; une cible de `Retour` / `Plus` ne peut pas disparaître).
   - **Fonds d'écran** : miniatures avec nom, ajout d'image personnalisée, rognage au format de l'écran (720×384). Les images de l'utilisateur restent dans son dossier, hors dépôt.
   - **Économiseurs d'écran** : choix, ajout de vidéo avec résolution et fps affichés (plafond mesuré : environ 20 i/s écran et touches ensemble), rognage graphique sur la vraie géométrie, conversion FFmpeg en arrière-plan (arguments fixes, pas de shell, entrée traitée comme non fiable).
   - **Réinitialiser** : par onglet et global, avec confirmation et sauvegarde préalable.
   À trancher : aperçu en direct sur le module pendant l'édition (demande une commande de plus au démon).
2. **Profils** : plusieurs fichiers de pages et bascule (changement de profil selon l'application active ou au choix) ; le fichier JSON unique actuel est le profil de base.
3. **Sous-menus** (pages « Applications » avec touche Retour : `page` et `back` existent déjà dans le format), Discord, autres raccourcis clavier (l'action `key` existe, liste de touches limitée au pavé numérique : à étendre avec validation) ; opérateurs du pavé (+ − × ÷) : pas de place sur 12 touches, à placer sur une seconde page ou les molettes.
4. **Windows : validation matérielle** — les backends et le bureau sont implémentés et testés automatiquement ; la suite porte sur les essais du propriétaire (stabilité, pages à 12 lanceurs, retraits/modifications, réveil prolongé, ouverture de session, lancement réel de jeux/PWA et coexistence RGB). Voir `docs/WINDOWS.md`.
5. **Plasmoids KDE** réutilisant `widgets/` (aperçu de l'écran du clavier sur le bureau).
6. **RVB (anneaux des molettes et clavier) : uniquement notre propre code** (décision du 12 novembre 2026). Plus de fork d'OpenRGB ni de code d'OpenRGB repris : on écrit dans `galleond` et `lib/`, d'après la spécification publique HID LampArray, les captures du dépôt et le protocole validé par Aaryush1. Raisons : une licence claire (LGPL-3.0-or-later pour `lib/`, l'accord d'Aaryush1), aucune contrainte héritée d'un autre projet, un protocole déjà validé sur le même modèle et le même firmware, et un testeur disponible. Les règles de sécurité de `AGENTS.md` s'appliquent à toute écriture RVB. Deux chantiers, dans cet ordre : **(a) les anneaux des molettes**, deux lampes LampArray de la partie Stream Deck (MI_03, page d'usage `0x59`) — composant `galleond` séparé de l'écran et du clavier, transport libusb sous Linux (le noyau ne lie pas l'interface, pas de `hidraw`, règle udev à prévoir) et rapports de fonctionnalité HID sous Windows, rapports construits par une fonction pure d'après le descripteur et testés avec un faux transport, réglages dans Bosun ; **(b) le RVB par touche du clavier**, bloqué tant que le protocole d'écriture non persistant n'est pas validé indépendamment (voir l'accord d'Aaryush1 ci-dessous). **Méthode** : les issues ne sont pas toutes ouvertes d'avance. À chaque implémentation (les anneaux, puis la couche de sécurité, puis chaque effet du clavier), on ouvre une issue au moment de publier le code, et on y mentionne @Aaryush1 pour lui demander son avis, comme testeur et relecteur (il a accepté ce rôle sur l'issue #56 ; il ne co-écrit pas le C++). Sa relecture de la couche de sécurité vient avant celle des effets. Restent à documenter avant le code : la concurrence de l'Éclairage dynamique Windows et d'OpenRGB sur les anneaux (ne pas inventer de commande de propriété), le comportement de reprise après plantage, et la coexistence avec le Web Hub. **Accord d'Aaryush1 (12 novembre 2026, [issue #56](https://github.com/Paullux/GalleonCore/issues/56#issuecomment-6827820538))** : il autorise la réutilisation de tout le code de `lab/` (liste d'autorisation, token bucket, cession au Web Hub, Guard) sous LGPL-3.0-or-later ou autre licence au choix du projet, ou une écriture à neuf d'après les notes de protocole ; pas besoin de redemander. Il ne veut pas co-écrire le C++ mais propose d'être testeur (Galleon 100 SD, firmware 2.3.88, Windows 11) et relecteur de la conception, y compris des règles de sécurité (crash avec watchdog, veille/réveil, rebranchement, coexistence avec le Web Hub). Pistes envisagées, dans l'ordre : bibliothèque de protocole du clavier et couche de sécurité (une issue par étape, lui comme testeur), puis frappe réactive, fond ambiant selon l'heure, et dans Bosun un sélecteur de mode d'éclairage (modes statiques échangeables et dynamiques) à la place du lien vers le Web Hub ; Easter egg de pluie Matrix sur le module liée à la frappe. Ce choix change la règle « le RVB va dans le fork d'OpenRGB » de `AGENTS.md` : à décider et à mettre à jour avant la première ligne de code. Aucune ligne n'est écrite à ce jour.
7. **À la fin, une fois le RVB du clavier implémenté : l'aperçu de Bosun montre ce qui se passe vraiment** (demande du propriétaire, 13 novembre 2026). L'aperçu « Clavier et module » de l'onglet Clavier (`KeyboardMap`) devra appliquer **les effets des touches** (le fond, la frappe réactive, les autres effets) et afficher **le thème courant du Stream Deck, y compris les économiseurs d'écran**, au lieu des couleurs choisies à la main et des seules touches du dernier onglet de thème regardé (l'écran du module n'affiche aujourd'hui que le nom du thème). À prévoir : (a) le RVB par touche livré et validé d'abord (issue #102, relecture d'Aaryush1, puis ses essais), (b) l'état des effets en cours exposé par le contrôleur d'éclairage (`LightingController`) à Bosun, (c) un **miroir en direct de ce que le module affiche** (touches, écran, économiseur en cours), publié par `galleond` avec un cache **borné** : Bosun n'ouvre jamais le HID, donc c'est `galleond` qui doit fournir les images, à un rythme raisonnable (les économiseurs vidéo sont lourds), (d) sous Windows, le canal de retour qu'il faut encore concevoir (l'endpoint local n'a pas de réponse ; voir le fichier d'état de la luminosité). C'est la dernière étape du chantier clavier, pas une étape intermédiaire.
8. **Piste future (non planifiée) : tests d'endurance automatisés de `galleond` sur GitHub Actions** — réutiliser les runners Ubuntu et Windows déjà employés, `FakeTransport` et l'API hidapi simulée pour exercer pendant 10 à 15 minutes les reconnexions HID, pertes de lecture, changements de pages et rafales d'événements, avec délais maximaux, surveillance des plantages, blocages et de la mémoire. Étudier un scénario nocturne plus long (~1 h) et une éventuelle intégration du test court comme condition de publication, **uniquement après validation de sa fiabilité**. L'injection d'un périphérique simulé dans le démon complet reste à concevoir. Cela ne remplace jamais les essais matériels USB/firmware ni les contrôles de coexistence OpenRGB. Proposition à étudier ultérieurement, sans lancement de travaux ni modification de la CI pour l'instant ; détails dans [docs/ROADMAP.md](docs/ROADMAP.md#tests-dendurance-ci--piste-future).

## 5. Architecture

```
lib/        libgalleon (C++20/Qt6, LGPL-3.0-or-later)
  Protocol      fonctions pures : octets <-> événements/rapports (aucune E/S)
  Transport     interface ; HidTransport (hidapi) = E/S brutes
  StreamDeck    logique : keep-alive 500 ms, lecture, signaux, setKeyImage / setScreenImage / setBrightness / setDialRing
  Layout        géométrie pure
daemon/     galleond (GPL-3.0-or-later), bibliothèque statique galleond_core + main.cpp
  PagesConfig        format JSON, validation stricte, thèmes, actions (media, page, back, app, theme, wallpaper-mode)
  PagesController    touches, molettes, thèmes, modes (thème / fond), anneaux, rechargement à chaud
  MprisController    MediaController sur D-Bus (MPRIS) ; MediaController.h = interface (futur SMTC)
  ScreenController   écran : fond+heure ou piste, ligne du haut, barre du bas, D-Bus org.galleoncore.Deck
  ScreenRenderer     rendu hors écran de widgets/*.qml (Qt Quick, backend logiciel)
  TileRenderer       images des touches ; CoverLoader, VideoThumbnails, DesktopEntries, WallpaperCatalog
  PagesEditor / AppsBackend / qml/Apps.qml   sélecteur d'applications
widgets/    QML pur partagé (WallpaperClock.qml, NowPlaying.qml) : pas de PlasmaCore/Kirigami
openrgb-plugin/   plugin OpenRGB existant (LampArray en libusb), à part ; le RVB de GalleonCore s'écrit dans galleond et lib/ (décision du 12 novembre 2026)
tests/ fuzz/ tools/ docs/protocol/ data/devices/ public/wallpaper/ plasmoid/
```

Décisions à ne pas perdre :
- **Toute donnée externe est non fiable** (rapports HID, images, vidéos, JSON) : validation de la longueur, des valeurs, puis de la séquence ; une entrée invalide est ignorée et journalisée. Un fichier de pages avec une seule erreur est refusé en entier.
- **Aucun octet de protocole inventé** : chaque fait de protocole cite sa source (node-elgato-stream-deck, galleon-deck, rust-elgato-streamdeck PR 63, captures de `docs/protocol/`). Le rapport des anneaux `03 24 <led> <r> <g> <b>` (led = (1 − molette) × 4 + segment) vient de la PR 63, confirmé sur le module le 19 octobre 2026.
- **Jamais d'écriture en flash** du clavier ; les écritures matérielles de test sont lancées par l'utilisateur (commandes données une par bloc).
- **Aucune action ne lance un programme sans être dans le fichier de pages** : `gio launch <fichier>` à un seul argument, sans shell ; le fichier doit appartenir à l'utilisateur et ne pas être inscriptible par d'autres.
- **État de longue durée borné** (caches d'images, d'icônes, d'applications, pile de pages) pour que LeakSanitizer ne soit pas le seul garde-fou.
- **Fichiers** : `~/.config/galleoncore/pages.json` (pages, thèmes, molettes, écran), `galleond.conf` (fond retenu), `~/.cache/galleoncore/thumbnails` (vignettes vidéo). Les tests isolent `XDG_CONFIG_HOME`.
- **Modes de molette** : un appui sur une molette configurée `theme` ou `wallpaper-mode` bascule cette molette entre contrôle multimédia et choix (thème / fond) ; sortie par second appui ou après 30 s sans rotation.
- Fonctionnalités gardées volontairement : système de profils et configuration JSON (demande explicite de l'utilisateur).
- **CI** (GitHub Actions, Ubuntu 24.04) : Qt **6.4**, plus ancien que celui du poste de développement (6.10) ; GCC, Clang + ASan/UBSan + fuzz, lint (clang-tidy 18, qmllint, reuse). Pièges rencontrés : `Locale.toString` n'existe pas en 6.4 (le widget utilise `Date.toLocaleTimeString`), un `QLocale` C++ n'y est pas enveloppé en `Locale` (la langue est passée par son nom), `QTimeZone::UTC` demande Qt 6.5, Qt Quick 6.4 laisse un cycle d'images à la sortie (suppression ciblée dans `tests/lsan.supp`). Pour reproduire : un conteneur `ubuntu:24.04` avec `qt6-base-dev qt6-declarative-dev` et les modules `qml6-module-*`. `main` est protégée : tout passe par une pull request aux trois vérifications vertes.
- **Validation** : `tools/check.py quick|batch|full|status` (voir CLAUDE.md, « Validation method »). Ciblé pendant le développement, complet (conteneur Ubuntu 24.04 de `tools/ci/`, comme la CI) à la fin d'un lot et avant toute fusion ; le rapport dit `TARGETED` ou `FULL VALIDATION PASSED`, lié à l'arbre exact (`status`).
- Build : `cmake -B build -DGALLEON_WERROR=ON && cmake --build build && ctest --test-dir build` ; aussi `-DGALLEON_SANITIZE=ON`, clang-tidy, `qmllint widgets/*.qml`, `reuse lint` (non installé sur cette machine : à lancer ailleurs avant une publication).

## 6. Prochaines étapes prioritaires

### Ce que veut dire la v1.0.0 (décision du propriétaire, 14 novembre 2026)

La v1.0.0 sortira quand les deux contrôles seront complets :

- **Contrôle total du Stream Deck : OK** (état déclaré par le propriétaire).
- **Contrôle total du clavier RVB : en cours.** Une fois terminé : OK. Aujourd'hui : luminosité (curseur, courbe de 24 h) et éclairage en direct d'une seule couleur, essayé sur le matériel sous Linux ; restent la veille/réveil, la cession au Web Hub et le plantage vérifiés sur le matériel, le portage Windows, les effets, et l'aperçu fidèle dans Bosun (point 7 de « À développer »).

### Signature de code Windows : SignPath Foundation (à suivre)

- Le propriétaire a décidé de demander un certificat de signature de code à [SignPath Foundation](https://signpath.org/) (programme gratuit pour les projets libres). **Statut de la demande : à confirmer par le propriétaire.**
- Ce que le programme exige (page des conditions de SignPath, lue le 14 novembre 2026) : licence libre approuvée par l'OSI et sans double licence commerciale (GPL/LGPL : conforme) ; projet maintenu, déjà publié dans la forme à signer, avec une page de téléchargement qui décrit ses fonctions ; binaires construits depuis nos sources par une compilation automatique et vérifiable (la CI Windows en est une) ; **métadonnées imposées : le nom du produit identique à celui du projet et la version identique d'une compilation à l'autre** ; approbation manuelle de chaque version ; double authentification pour tous ceux qui signent ; **une « politique de signature » publiée sur la page d'accueil et les pages de téléchargement** (rôles, attribution SignPath, politique de confidentialité) ; désinstallation possible ; changements de configuration du système annoncés (démarrage automatique, règles udev). L'acceptation reste à la discrétion de SignPath, sans appel.
- **À préparer de notre côté** : vérifier les métadonnées des exécutables Windows (aujourd'hui seul le Setup porte une description) ; rédiger la politique de signature et de confidentialité dans les quatre langues du site (le contrôle de mise à jour et le widget météo contactent des services externes) ; définir les rôles (auteur, relecteur, approbateur) ; vérifier l'authentification à deux facteurs sur GitHub.
- Une signature n'efface pas tout de suite l'avertissement de Windows SmartScreen : la réputation d'un éditeur se construit avec les téléchargements.

La liste courante est celle du **point de reprise du 29 octobre**, en tête du fichier : validation de l’arbre fusionné, essais Gaming Linux/Windows, configuration MangoHud, firmware/notifications puis endurance. Les fonctionnalités Bosun déjà présentes (édition des touches, fonds, veille, démarrage avec la session) ne sont plus à traiter comme des développements initiaux à entreprendre. Les détails anciens des sections 2 et 4 restent à réconcilier progressivement avec les validations ; ils ne remplacent pas les priorités courantes.

## Règle de mise à jour

Avant de la déplacer, les tests ciblés suffisent pour avancer, mais la case « Tests : oui » ne vaut qu'avec une validation complète (`tools/check.py full`) de l'arbre fusionné. Après chaque fonctionnalité terminée et testée : la déplacer vers « Implémenté et testé » (avec ses tests et, si confirmé, « sur le module »), retirer ses lignes de « En cours » ou « À développer », et mettre à jour « Prochaines étapes » et la date. Ne pas marquer « sur le module » sans retour de l'utilisateur. Garder ce fichier court : le détail de conception va dans `docs/ROADMAP.md`.

## 7. État du portage Windows et prochain essai matériel

Le code commun reste dans `main`, avec des backends Linux/Windows séparés.
La [PR #28](https://github.com/Paullux/GalleonCore/pull/28) regroupe les derniers
correctifs ; les [CI et artefacts courants](https://github.com/Paullux/GalleonCore/actions)
font foi pour le commit à essayer. Les détails et limites sont dans
[docs/WINDOWS.md](docs/WINDOWS.md), le packaging dans
[docs/PACKAGING.md](docs/PACKAGING.md).

Le quatrième retour matériel signale un retour au logo Corsair sur les pages
chargées, puis une reprise après environ une minute, sans changement du PID.
Les retraits/modifications pouvaient être annulés par le remplissage automatique.
La tentative de pagination à 22 est donc remplacée par **12 lanceurs initiaux
fixes**, sans découverte sur rechargement/édition. Le même plafond et les cinq
onglets initiaux sont appliqués aux profils Linux neufs ; les fichiers personnels
existants ne sont pas migrés. Les anciens liens Plus générés
sont retirés ; les pages supplémentaires restent dans le fichier pour récupération.
Les raccourcis `.url` Steam sont désormais acceptés avec validation stricte.

Les recherches Shell/Steam sont isolées dans un enfant limité (2,5 s, 64 Kio,
12 requêtes en attente, 64 résultats), le dessin cède la main entre touches et les
transferts longs servent le keep-alive existant. Les tests simulent des recherches
et écritures lentes ; ils ne prouvent pas à eux seuls la résolution matérielle.
La découverte initiale ne charge pas les icônes/jaquettes avant l’ouverture HID.

La capture du propriétaire identifie les anneaux comme `Dynamic Light` dans
OpenRGB officiel : Corsair LampArray VID 1B1C/PID 2B18, MI_03. L’Éclairage dynamique
Windows et OpenRGB peuvent concurrencer galleond ; aucun arbitrage automatique
ni nouvelle commande RGB persistante n’est ajouté. Tester d’abord sans ces autres
écrivains, puis évaluer leur coexistence séparément.

Le diagnostic Linux précédent a montré une déconnexion USB spontanée, puis un
réveil court réussi après réénumération. L’ancien patch expérimental avait été
retiré : ne pas le présenter comme une correction matériellement confirmée.
Le réveil prolongé et le nouveau transfert avec keep-alive restent à vérifier.

Le propriétaire a demandé explicitement le tag `v0.1.0` et le lancement de la
publication après son nouvel essai du Setup. Ce tag a été poussé depuis `main` ;
la réussite de la CI de publication ne vaut pas confirmation des essais Windows
prolongés, ni preuve que les dépôts PPA/COPR/AUR ont effectivement publié les
paquets.

### Retour matériel et suivi HID du 21 octobre

Le propriétaire confirme la pagination manuelle et le comportement attendu une
fois le contrôle acquis, même avec OpenRGB ; le démarrage reste intermittent.
Le suivi `codex/windows-hid-startup` corrige les défauts identifiés dans le cycle
HID : ouverture Windows après initialisation Qt, contrôle du mode non bloquant,
lecture à délai nul, premier keep-alive obligatoire et transitions de reconnexion
visibles avant nouvelle tentative périodique. Des régressions simulées couvrent
ces chemins ; la cause matérielle et le démarrage à froid restent à confirmer.
Les nouveaux liens de pagination Bosun sont `NEXT >` et `< BACK`, sans modifier les
libellés personnels. Voir `docs/WINDOWS.md`. Validation complète exigée avant PR ;
aucune publication avant acceptation matérielle du propriétaire.

### Nouvel essai matériel Windows du 21 octobre

Le changement de fond dans Bosun réveille désormais la veille animée avant
d'afficher l'image choisie. Le test de régression avec faux périphérique valide
la transition veille → fond sous Qt ; l'effet sur le module Windows reste à
confirmer lors du prochain essai matériel.

Le propriétaire confirme que la PR #29 ne résout pas la première prise de
contrôle : images et Bosun fonctionnent, mais les touches et molettes restent
inactives même après dix minutes. Un lancement manuel après démarrage complet
de Windows reproduit le défaut. Le PID de `galleond.exe` reste identique pendant
le retour temporaire au logo Corsair, puis les commandes fonctionnent. Le PID
stable ne permet pas de conclure sur la fermeture du handle HID. Une
instrumentation bornée, sans contenu des rapports ni nouvelle commande HID,
est préparée pour distinguer absence de lecture, rejet du décodage et vraie
reconnexion ; voir `docs/WINDOWS.md`. La cause et la correction matérielles
restent à établir après comparaison des journaux d'une première ouverture en
échec et de la récupération.

Le 21 octobre, le propriétaire a ensuite mis à jour le firmware Stream Deck
depuis l'application officielle Elgato sous Windows, puis désinstallé cette
application. Lors du nouvel essai, touches LCD et molettes ont répondu sans la
panne initiale ; sa capture montre `3.06.005`. Comme les deux changements ont
été faits ensemble, leur effet individuel reste inconnu. Le propriétaire a
ensuite jugé le Setup satisfaisant et demandé le premier tag. Confirmer encore
la stabilité après démarrage à froid et longue veille ; ne pas attribuer la
résolution du défaut HID à un correctif logiciel non vérifié.

### Clarification du bouton PresentMon (24 octobre)

Le propriétaire confirme que le bouton devient vert « Arrêter » dès que le jeu est lancé : le défaut observé était le libellé au repos. Correction UI : état `waiting` installé → « Prêt — lancez un jeu » désactivé, démarrage automatique au lancement du jeu ; arrêt/reprise et erreurs conservent leurs actions. Tests QML et scénario natif Windows étendus ; validation native de ce dernier libellé à refaire dans le prochain setup.

### Aide Linux Gaming organisée par lanceur (24 octobre)

Aide de l’interface et guide Linux réorganisés : Commun → Steam → Heroic → Lutris → PlayOnLinux → Bottles. Une préparation commune unique configure les fichiers et applique le dossier FPS ; chaque lanceur dispose de son chemin de menus et de ses propres commandes éventuelles. Les alternatives Steam, les permissions Flatpak et les limites restent dans des sections avancées du guide. Le bouton de dossier seul, redondant avec la configuration commune, a été retiré de cette aide. Compilation, qmllint, test_bosun (rendu QML) et syntaxe des blocs shell du guide réussis. Capture de l’aide contrôlée visuellement.
