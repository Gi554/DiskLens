# Vérification de la version 0.1.0

Vérifications effectuées le 9 octobre 2026 sur Windows x64.

## Construction

- CMake 4.4.4, GCC / MinGW-w64 13.1.0 et Qt 6.8.3 (kit mingw_64).
- Compilation C++17 en Release. Options d'avertissement activées : `-Wall -Wextra -Wpedantic`.
- Sources Qt et MinGW téléchargées sous forme de kits binaires portables dans le dossier de travail, hors du projet.
- Les outils de compilation et les données de test sont séparés des sources du projet.

## Tests

- Deux suites CTest passent : moteur et interface Qt offscreen.
- Le moteur vérifie les totaux, les relations de l'arbre, les dossiers vides, les chemins accentués, les contenus différents de même taille, les copies identiques, les modifications après analyse, l'annulation, les dossiers invalides et la géométrie du treemap.
- Le test des liens physiques passe hors du bac à sable Windows ; il est ignoré lorsque l'environnement refuse leur création.
- La création d'un lien symbolique avec ce kit MinGW renvoie « Function not implemented ». Ce cas est signalé comme ignoré, et non comme réussi. Le moteur Windows ignore explicitement les attributs de point de réanalyse, notamment les jonctions.
- L'interface vérifie les transitions asynchrones, les listes, la recherche, le filtre par extension, l'exploration d'un sous-dossier, le retour au parent, les groupes de doublons et la conservation du résultat après annulation d'une nouvelle analyse.

## Exécution et rendu

- Une analyse et une recherche des doublons ont été exécutées sur un dossier fictif de 14 fichiers / 7 dossiers, avec 41,3 Mio de données et une copie supplémentaire de 2 Mio.
- La capture de la fenêtre a été inspectée. La mise en page a été adaptée à la mise à l'échelle Windows pour éliminer un débordement de la carte sur sa légende.
- Le déploiement est effectué avec le script CMake de Qt et windeployqt. Les DLL et les plugins sont conservés séparément de l'exécutable.
- Le paquet est testé en retirant Qt et MinGW du PATH ; son mode de vérification effectue analyse, doublons et capture avant de quitter.
- L'outil de déploiement signale l'absence des traductions Qt et de bibliothèques DXC. Le rendu utilisé est celui de Qt Widgets / QPainter ; le lancement du paquet est vérifié indépendamment de ces avertissements. Les libellés DiskLens sont intégrés en français ; certains dialogues Qt peuvent rester en anglais.

## Limites de cette vérification

Les sélecteurs natifs de fichiers et l'ouverture de l'explorateur ne sont pas automatisés par ces tests. Les plateformes Linux et macOS ne sont pas compilées ici. L'analyse d'un volume de plusieurs millions de fichiers reste à mesurer ; le résultat est conservé en mémoire.
