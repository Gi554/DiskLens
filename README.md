# DiskLens

DiskLens est une application de bureau en C++17 et Qt Widgets qui permet de comprendre ce qui occupe un dossier. Une carte proportionnelle montre son contenu ; les listes permettent de retrouver les gros fichiers et les copies identiques.

## Fonctionnalités

- Analyse en arrière-plan avec compteur de fichiers, taille parcourue et annulation.
- Carte interactive : sélection, info-bulles, navigation par double-clic et retour au parent.
- Classement des 500 plus gros fichiers, recherche par chemin et filtre par extension.
- Liste du dossier courant, limitée aux 2 000 premiers éléments par taille pour garder l'interface réactive.
- Recherche des doublons : candidats de même taille, comparaison SHA-256 complète, exclusion des liens physiques et des fichiers modifiés depuis l'analyse.
- Export CSV de tous les fichiers du résultat, en UTF-8, avec leur taille exacte en octets.
- Ouverture d'un emplacement dans l'explorateur du système.

L'application n'efface ni ne déplace les fichiers. L'export CSV écrit uniquement le rapport à l'emplacement choisi.

## Version Windows

Le paquet autonome est disponible dans les [versions publiées](https://github.com/Gi554/DiskLens/releases/latest). Décompresser entièrement `DiskLens-Windows.zip`, puis lancer `bin/DiskLens.exe`. Les DLL et les plugins doivent rester dans leurs dossiers.

## Utilisation

Choisir un dossier lance son analyse. Les cartes de statistiques portent sur le dossier racine ; la carte interactive montre les éléments du dossier courant. La surface représente la taille logique en octets, pas l'espace alloué par le système de fichiers.

Le filtre par extension s'applique aux listes, et la recherche atténue également les éléments de la carte qui ne correspondent pas. La recherche des doublons concerne l'intégralité du résultat, indépendamment des filtres. Un groupe conserve tous ses chemins visibles si un de ses fichiers correspond au filtre. L'export CSV contient tous les fichiers, sans appliquer les filtres de l'interface.

Raccourcis : **Ctrl+O** choisir un dossier, **Ctrl+R** actualiser, **Ctrl+F** rechercher, **Échap** annuler le travail en cours. Une analyse annulée conserve le résultat précédent.

## Compilation

Prérequis : un compilateur C++17, CMake 3.24 ou plus récent, Qt 6.5 ou plus récent avec Widgets. Utiliser un kit Qt et un compilateur compatibles ; par exemple Qt 6.8.3 pour MinGW 13.1 sur Windows.

Windows avec MinGW dans le PATH :

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/mingw_64"
cmake --build build -j 4
ctest --test-dir build --output-on-failure
.\build\DiskLens.exe
```

Sur Linux avec les paquets de développement Qt Widgets installés :

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/DiskLens
```

Le SDK Qt doit être accessible au lancement (sur Windows, placer son dossier `bin` dans le PATH). Pour un dossier autonome, la règle d'installation CMake utilise l'outil de déploiement Qt :

```powershell
cmake --install build --prefix "$PWD/dist"
```

L'application installée se trouve dans `dist/bin`. Qt est lié dynamiquement. Aucun script Python, JavaScript ou QML n'est nécessaire à l'application ni à sa compilation avec un SDK déjà installé. CMake décrit la construction ; le style de l'interface est intégré au C++.

Pour vérifier le rendu sur un dossier de test :

```powershell
.\build\DiskLens.exe C:\dossier-test --smoke --screenshot apercu.png
```

Ce mode effectue une analyse, une recherche des doublons et un aller-retour du filtre de recherche, puis capture la fenêtre et quitte.

## Architecture

```text
src/
  core/   parcours des fichiers, recherche des doublons, calcul des rectangles
  ui/     fenêtre Qt Widgets et carte dessinée avec QPainter
  main.cpp
tests/    tests du moteur sur un dossier temporaire
```

Le parcours est itératif. L'arbre stocke les relations parent/enfants et agrège les tailles en ordre inverse. Le découpage de la carte est un treemap binaire pondéré. Les opérations longues tournent dans un thread ; l'interface lit des compteurs atomiques et récupère le résultat après la fin du travail.

Les tests du moteur couvrent les totaux, les chemins Unicode, les doublons, les changements de fichiers, l'annulation et les surfaces de la carte. Les tests de l'interface utilisent la plateforme Qt offscreen pour vérifier la fin des tâches asynchrones, la recherche, les filtres, la navigation et la conservation d'un résultat après annulation.

## Limites actuelles

- Liens symboliques et points de réanalyse Windows (jonctions, certains fichiers à la demande) ignorés ; les liens physiques sont comptés par chemin dans l'analyse, puis exclus des copies supplémentaires lors de la recherche des doublons.
- Les fichiers vides restent dans les listes mais ne produisent pas de rectangle ni de groupe de doublons.
- Les permissions insuffisantes sont signalées avec un relevé limité aux 100 premiers incidents.
- Le disque peut changer pendant le parcours. Les résultats ne sont pas un instantané transactionnel ; actualiser avant de prendre une décision.
- SHA-256 identifie les contenus avec la garantie pratique d'un hash cryptographique ; aucune suppression automatique n'utilise ce résultat.
- Les copies supplémentaires expriment des octets logiques. Compression, fichiers creux, déduplication du volume et partage des blocs peuvent changer l'espace réellement récupérable.
- Une opération système bloquée, notamment sur un partage réseau, peut retarder l'annulation ou la fermeture. La liste et l'arbre restent en mémoire.

## Licence

Le code DiskLens est sous licence MIT. Qt et ses dépendances conservent leurs licences propres, détaillées dans [THIRD_PARTY.md](THIRD_PARTY.md).
