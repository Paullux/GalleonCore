# ROADMAP

Référence pour reprendre le développement à tout moment, y compris dans une nouvelle session Claude Code. À mettre à jour après chaque fonctionnalité terminée **et** testée (voir « Règle de mise à jour » en bas). Les notes de conception détaillées (protocole, RGB, OpenRGB, Windows) sont dans [docs/ROADMAP.md](docs/ROADMAP.md) ; les règles de code dans [CLAUDE.md](CLAUDE.md).

### État au 29 octobre 2026 — revue locale de `main`

**Point de reprise actuel : `22ba855` (PR #69).** Synchronisation effectuée avec `git pull --ff-only origin main` le 29 octobre : dépôt déjà à jour, sans modification locale avant cette revue. La version déclarée dans `CMakeLists.txt` reste **0.2.2** ; les compléments ci-dessous sont fusionnés pour préparer la suite, sans nouvelle publication effectuée pendant cette session.

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
Tests de non-régression sur configurations fictives ; aucun réglage de lanceur
réel modifié pendant le diagnostic. PR et fusion autorisées après validation
complète et CI par le propriétaire dans cette session.

#### Suite prioritaire

1. Valider l’arbre fusionné sous GCC, Clang + sanitizers, fuzzing et lint, et contrôler les CI Linux, paquets et Windows/NSIS avant une PR ou publication ultérieure.
2. Tester sur Linux et Windows les replis SteamGridDB, le choix à la molette (chargement, décompte, annulation, relance hors ligne) et le bouton Appliquer près de l’image personnelle ; documenter les jeux et résultats précis.
3. Valider la configuration MangoHud par sélection décrite ci-dessus sur les lanceurs réels, puis poursuivre séparément l’avertissement firmware Windows et les notifications système de mise à jour.
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

L'[issue #56](https://github.com/Paullux/GalleonCore/issues/56) reste **ouverte** pour suivre l'endurance de l'implémentation expérimentale C# d'Aaryush1 (tests jour après jour, plantages et watchdog, veille/réveil, reconnexions USB, coexistence Web Hub). Une collaboration pour un pilote **natif C++** est seulement une possibilité future, **sans engagement du contributeur**.

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
| **Galleon Bosun, écrans de veille : rognage et conversion** (interface). Une vidéo choisie (dépôt ou chemin) s'ajoute telle quelle ou « coupée à la taille du module » : fenêtre de rognage avec le cadre jaune aux proportions de l'image du module (déplacer à la souris, molette ou boutons pour zoomer, centrer) et, dedans, l'écran (cyan) et les 12 touches dessinés là où ils tomberont ; FFmpeg écrit une nouvelle vidéo 720×1258 aux images par seconde du réglage dans `~/.local/share/galleoncore/screensavers/`, barre de progression, bouton Stop ; le résultat est ajouté à la liste ; l'original n'est jamais touché. Une vidéo déjà dans la liste peut être recoupée ; liste pleine ou FFmpeg absent : dit pourquoi. Commande FFmpeg essayée pour de vrai sur la vidéo d'exemple (704×1280 → 720×1258, h264, 15 i/s, 1 s) | oui (`test_bosun`) ; FFmpeg réel essayé à la main | à essayer |
| **Galleon Bosun, onglet Fonds d'écran** : miniatures (par style), clic pour utiliser (appliqué aussitôt si galleond tourne, retenu pour le prochain démarrage dans `galleond.conf`), image en cours repérée (magenta), ajout d'une image personnelle (bouton « Browse… » en premier, dépôt ou chemin ; fenêtre de rognage au format de l'écran 720×384 ; enregistrée en JPEG 1440×768 dans `~/.local/share/galleoncore/wallpaper/mine/`, jamais par-dessus un fichier), suppression de ses propres images seulement (pas celles du projet, pas celle en cours). Côté démon : le catalogue lit plusieurs dossiers (le sien, le système, l'arbre source) au lieu du premier trouvé, se relit à `SetWallpaper` inconnu et à l'entrée du mode choix ; le dossier personnel est conservé même absent au démarrage pour découvrir le premier import sans redémarrer le démon. Fichiers vérifiés (taille, dimensions, contenu) avant décodage | oui (`test_wallpaperlibrary`, `test_wallpapercatalog`, `test_screencontroller`, `test_filenames`) | à essayer |
| **Galleon Bosun, onglet Clavier** : bouton qui ouvre le Web Hub de Corsair (`https://www.corsair.com/web-hub`, page officielle de l'aide Corsair) dans le navigateur de l'utilisateur, avec ce qu'il faut savoir (navigateur Chromium, rien n'est écrit dans la mémoire du clavier par cette application). Pas de webview : lien seul | sans objet (lien) | à essayer |
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

- **Mode Jeu automatique (23 octobre)** : écran dédié au-dessus de l'accueil/du média pendant une capture FPS fraîche, FPS, températures et charges CPU/GPU sur cadrans gradués de style automobile, illustration locale Steam ou association manuelle par exécutable, onglets Bosun Widgets et Mode Jeu séparés, chacun avec bouton Appliquer. Arrêt de la veille du module pendant le jeu, réveil immédiat et délai complet restauré à la sortie. Sources Linux hwmon/MangoHud, NVIDIA Linux/Windows et serveur local LibreHardwareMonitor optionnel. Détection fondée sur la capture active (retour sous dix secondes sans nouvelles données), disposition jeu fixe pour cette première version ; validation native Windows et sur le module à effectuer. [Détails](docs/HOME_WIDGETS.md).


- **Widgets de l’accueil (23 octobre)** : première itération dans Bosun, onglet dédié, aperçu QML partagé, visibilité/taille/position, nom de ville météo, taille de police par carte, compteur FPS via CSV MangoHud/PresentMon et bouton Appliquer avant sauvegarde atomique relue par le daemon. Horloge/date locale, météo Open-Meteo sur coordonnées explicites, CPU/RAM Linux/Windows, température CPU Linux et comptage des fichiers partiels de téléchargement. Tests Linux de persistance, géométrie, rendu et onglet ; validation native Windows et sur le module encore nécessaire. Restent les capteurs Windows/GPU, débits réseau/disque, graphes et progression exacte des téléchargements par navigateur. [Détails et limites](docs/HOME_WIDGETS.md).


- ~~Aperçu fluide du choix du fond~~ — réglé le 19 octobre (cause : le module n'affiche une image que si un second envoi la suit ; voir `docs/protocol/README.md`). Ancien texte : chaque cran renvoie un JPEG de l'écran. À mesurer sur le module : latence par cran, et si une rotation rapide s'accumule. Piste si trop lent : ne dessiner que le dernier fond demandé (regrouper les crans) et réduire la qualité JPEG pendant la rotation.
- **Écran de veille : composition** — avec la vraie géométrie (écran 62,6 × 33 mm, touches 13,7 mm, pas 19,1 mm, 5,4 mm entre écran et touches) l'écran n'occupe que **30 % de la hauteur** de l'image : le visage doit être dans les 25 % du haut. Ancien texte : deux vidéos reçues (704×1280, 24 i/s, 9 s) : « Hacker dans la métropole néon » (jaune, réaliste) et « Nuit gaming sous la lune rose » (rose, animée), listées dans `~/.config/galleoncore/pages-veille-test.json` avec une géométrie *estimée* (pas mesurée) ; à ne pas ajouter au dépôt sans vérifier `ASSETS.md` (marques visibles dans l'image, conditions du service de génération). Mesures à la règle (écran, touche, espaces, distance écran → touches) à mettre dans `screensaver.geometry`, puis une vidéo verticale (piste : image générée par ChatGPT puis animée par un outil IA, en boucle ; vérifier les conditions du service et `ASSETS.md`). À confirmer sur le module : fluidité à 15 i/s, continuité de l'image entre l'écran et les touches, réveil.
- **Pages remplies** : fait le 19 octobre (Multimédia et Plus, Création Musicale, Jeux vidéo, Système, Pavé numérique). Le contenu est dans le `pages.json` de l'utilisateur ; reste à le confirmer sur le module.

## 3. À vérifier ou corriger

- **Stabilité du démon** : exécution longue (heures), reprise après veille, débranchement/rebranchement du module (le redessin complet est déjà prévu sur `deckAvailableChanged`), fuites : caches bornés (images, icônes, applications) mais pas mesurés sur la durée.
- **Rechargement à chaud** : confirmé pour un déplacement de touche ; à vérifier : fichier remplacé par un éditeur (écriture en plusieurs étapes), fichier invalide pendant l'exécution (les anciennes pages doivent rester), changement de thèmes pendant qu'un mode est actif (le mode est remis à zéro, voulu).
- **Conflits entre instances** : deux `galleond` sont maintenant exclus (le second quitte). OpenRGB (plugin) et `galleond` écrivent tous deux les anneaux : à ne pas lancer ensemble sans règle (non résolu).
- **Isolation des tests** : les tests écrasaient `galleond.conf` (corrigé : `XDG_CONFIG_HOME` isolé, dans le code des tests et par ctest) et leurs faux lecteurs MPRIS apparaissaient sur le bus de l'utilisateur, ce qui perturbait un `galleond` en marche (corrigé : ctest lance chaque test sous `dbus-run-session`). Ne lancer les tests que par `ctest`. Vérifier qu'aucun autre test n'écrit dans le vrai `~/.config` ou `~/.cache`.
- Ligne fantôme sous les étiquettes des touches sur les photos : probablement optique, jamais confirmé.
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
6. **RVB du clavier** : reste côté fork d'OpenRGB, bloqué tant que le protocole d'écriture non persistant n'est pas trouvé.
7. **Piste future (non planifiée) : tests d'endurance automatisés de `galleond` sur GitHub Actions** — réutiliser les runners Ubuntu et Windows déjà employés, `FakeTransport` et l'API hidapi simulée pour exercer pendant 10 à 15 minutes les reconnexions HID, pertes de lecture, changements de pages et rafales d'événements, avec délais maximaux, surveillance des plantages, blocages et de la mémoire. Étudier un scénario nocturne plus long (~1 h) et une éventuelle intégration du test court comme condition de publication, **uniquement après validation de sa fiabilité**. L'injection d'un périphérique simulé dans le démon complet reste à concevoir. Cela ne remplace jamais les essais matériels USB/firmware ni les contrôles de coexistence OpenRGB. Proposition à étudier ultérieurement, sans lancement de travaux ni modification de la CI pour l'instant ; détails dans [docs/ROADMAP.md](docs/ROADMAP.md#tests-dendurance-ci--piste-future).

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
openrgb-plugin/   plugin OpenRGB (LampArray en libusb) ; le RVB clavier n'est pas développé ici
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
