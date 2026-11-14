# Politique de signature de code

**Brouillon. Non publié.** Voir `docs/signing/README.md`. Remplacer chaque `[TO CONFIRM: ...]` avant tout usage.

GalleonCore (le démon `galleond`, l'application de réglage Galleon Bosun, le programme de surveillance
`galleon-keyboard-watchdog` et le Setup Windows) est un logiciel libre sous licences GPL-3.0-or-later et LGPL-3.0-or-later.
Cette page explique comment ses binaires Windows sont signés.

## Attribution

[TO CONFIRM: reprendre la formule exacte et le lien exigés par les conditions actuelles de SignPath.]
Signature de code gratuite fournie par [SignPath.io](https://signpath.io), certificat de la [SignPath Foundation](https://signpath.org).

## Ce qui est signé

Seulement des binaires construits à partir du code source de ce projet, dans le dépôt public
`https://github.com/Paullux/GalleonCore`, par sa compilation automatique (`.github/workflows/windows-core.yml`) :
`galleond.exe`, `galleon-bosun.exe`, `galleon-keyboard-watchdog.exe` et le Setup. Les bibliothèques tierces livrées dans le
paquet (par exemple Qt) ne sont pas signées par le certificat de ce projet, sauf si elles sont recompilées ici. Rien n'est signé
qui n'ait été construit par le projet.

## Rôles

- **Auteurs et committers :** Paul Woisard (propriétaire du dépôt). [TO CONFIRM: autres committers.]
- **Relecteurs** des changements faits par des personnes qui ne sont pas committers : [TO CONFIRM: nom ou « le propriétaire »].
- **Approbateurs** de chaque demande de signature (chaque version est approuvée à la main avant d'être signée) : [TO CONFIRM: nom].

Toute personne ayant un accès en écriture au dépôt ou au service de signature utilise l'authentification à plusieurs facteurs.

## Politique de confidentialité

Ce programme ne transfère aucune information vers d'autres systèmes en réseau, sauf si l'utilisateur ou la personne qui l'installe
ou l'exploite le demande expressément. Les requêtes suivantes sont faites par les programmes, et seulement comme décrit :

- **Vérification des mises à jour** (Galleon Bosun) : interroge `api.github.com` pour la dernière version du projet, d'elle-même à
  intervalles par défaut, et quand l'utilisateur appuie sur le bouton. Elle n'envoie rien sur l'utilisateur ou l'ordinateur au-delà de
  ce que porte toute requête web (l'adresse de l'ordinateur). Elle se désactive dans le panneau Mises à jour de Bosun. [TO CONFIRM
  d'après le code : l'interrupteur existe et est désactivé pour les installations gérées par un gestionnaire de paquets.]
- **Widget météo** (galleond) : seulement après que l'utilisateur a saisi un lieu ; il envoie la latitude et la longitude de ce lieu à
  `api.open-meteo.com`.
- **Illustrations de jeux** (galleond) : seulement après que l'utilisateur a saisi une clé SteamGridDB ; il envoie le titre ou
  l'identifiant Steam d'un jeu à `steamgriddb.com`.

Les fichiers de configuration restent dans le dossier de l'utilisateur. Le projet n'envoie nulle part ni donnée d'usage, ni
identifiant, ni journal.

## Changements du système

Les programmes n'écrivent que dans le profil de l'utilisateur. Le Setup peut lancer Galleon Bosun et le démon à l'ouverture de
session par la clé de registre `Run` de l'utilisateur ; cela est annoncé dans le Setup et retiré par le désinstalleur. Sous Linux,
une règle udev qui donne le clavier et le module Stream Deck à l'utilisateur est installée par les paquets. L'éclairage en direct,
expérimental, est désactivé par défaut, n'écrit jamais dans la mémoire du clavier et est protégé par un processus de surveillance
qui rend les touches si le démon s'arrête.

## Désinstallation

Utiliser « Applications » dans les paramètres de Windows, ou lancer le désinstalleur du dossier d'installation. Les fichiers de
l'utilisateur (pages, thèmes, fonds d'écran, courbes) sont conservés, sauf s'il les supprime.

## Signaler un problème

[TO CONFIRM: adresse de contact ou suivi des tickets `https://github.com/Paullux/GalleonCore/issues`.] Un binaire signé qui fait
autre chose que ce que dit cette page est un défaut, et sera signalé à SignPath.
