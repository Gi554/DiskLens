# Dépendances

DiskLens utilise Qt Core, Qt Gui et Qt Widgets, liés dynamiquement. Le déploiement Windows ajoute Qt Network comme dépendance indirecte du plugin tactile Qt. Le code du projet n'inclut aucune copie des sources Qt.

- Qt : https://www.qt.io/
- Sources du kit vérifié (Qt Base 6.8.3) : https://code.qt.io/cgit/qt/qtbase.git/tree/?h=v6.8.3
- Licences et notices Qt : https://doc.qt.io/qt-6/licensing.html
- Licences des composants intégrés à Qt : https://doc.qt.io/qt-6/licenses-used-in-qt.html
- Déploiement des bibliothèques Windows : https://doc.qt.io/qt-6/windows-deployment.html

Les modules Qt Base utilisés ici sont disponibles sous LGPLv3 ou GPLv3, ou sous licence commerciale. Le binaire local utilise le SDK communautaire et le lien dynamique. Les DLL Qt demeurent remplaçables ; ne pas les fusionner avec l'exécutable lors de la préparation d'une distribution. Le dossier de sources Qt ci-dessus contient également les notices de ses composants tiers.

Le compilateur de la construction Windows est MinGW-w64 / GCC. Les bibliothèques d'exécution copiées lors du déploiement gardent leurs propres licences et exceptions : https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html

Le dossier `licenses/Qt` contient les textes officiels de Qt Base 6.8.3, les notices extraites du kit et son inventaire SPDX. `licenses/runtime` conserve les notices GCC, MinGW-w64 et winpthreads du compilateur utilisé. Cet inventaire Qt décrit tout Qt Base ; le paquet de l'application ne déploie qu'une partie de ses modules.

CMake et les outils servant à télécharger le SDK ne font pas partie de l'application. Les dépendances de développement et leurs caches doivent rester hors du dépôt.
