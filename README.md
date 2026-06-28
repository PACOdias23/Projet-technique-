# Projet-technique-
# Mini Groovebox Afrobeat
 ISEN2 — Groupe Drum Machine  
Période : 26 mai 2026 — 26 juin 2026

## 1. Présentation du projet

La **Mini Groovebox Afrobeat** est un instrument électronique compact inspiré des grooveboxes et des boîtes à rythmes. Le but du projet est de réaliser un prototype autonome capable de lire des sons de batterie, programmer une séquence rythmique, afficher les informations sur un écran OLED et enregistrer un son via un microphone.

Le projet repose sur une **Teensy 4.1** associée à un **Teensy Audio Shield SGTL5000**. L’ensemble est intégré dans un boîtier fabriqué au Fablab.

## 2. Membres du groupe

| Nom | Rôle principal |
|---|---|
| NKEMLA LIENOU Steve Frede | intégration et tests |
| Lamine | Chef de groupe/ Firmware et DSP |
| Sakeng Nzegang Aurélien Brayan | Rapport et documentation |
| Brayan Kengne | Fablab, CAO et boîtier |
| Mick Nathaniel | Validation et démonstration |

## 3. Encadrement

Enseignants / intervenants associés au projet :

- Amaury AUGUSTE
- Axel LECA
- Philippe MEGE
- Guillaume PEROCHEAU
- Eric SERRE

## 4. Objectifs

Les objectifs principaux du projet sont :

- lire des fichiers audio `.WAV` depuis une carte micro-SD ;
- déclencher quatre sons principaux : Kick, Snare, Hihat et Clap ;
- programmer une séquence sur 8 pas ;
- afficher les menus et les réglages sur un écran OLED ;
- régler le volume, le tempo et le swing avec des potentiomètres ;
- enregistrer un son via le micro ;
- relire ou supprimer les enregistrements ;
- intégrer le prototype dans un boîtier propre.

## 5. Fonctionnalités principales

### Sons directs

Quatre boutons permettent de déclencher directement les sons :

- KICK
- SNARE
- HIHAT
- CLAP

Les fichiers doivent être placés à la racine de la carte micro-SD avec les noms suivants :

```text
KICK.WAV
SNARE.WAV
HIHAT.WAV
CLAP.WAV
```

### Séquenceur 2x4

Le séquenceur est composé de 8 pas organisés en deux lignes de quatre boutons.

Chaque pas peut contenir un seul son à la fois :

```text
-1 = pas vide
0  = Kick
1  = Snare
2  = Hihat
3  = Clap
```

Le bouton **PLAY** lance la lecture de la séquence. Le séquenceur lit les pas dans l’ordre de 1 à 8.

### Écran OLED

L’écran OLED permet d’afficher :

- le menu principal ;
- le son sélectionné ;
- l’état du séquenceur ;
- les valeurs des potentiomètres ;
- le mode enregistrement ;
- les informations du projet.

### Enregistrement micro

Le prototype permet d’enregistrer le signal du micro dans un fichier WAV. Les fichiers générés sont enregistrés sous la forme :

```text
REC001.WAV
REC002.WAV
REC003.WAV
...
```

Un filtrage logiciel est appliqué pour réduire une partie du bruit parasite.

## 6. Matériel utilisé

| Composant | Rôle |
|---|---|
| Teensy 4.1 | Microcontrôleur principal |
| Teensy Audio Shield SGTL5000 | Lecture et traitement audio |
| Carte micro-SD | Stockage des samples et enregistrements |
| Écran OLED I2C 0,96 pouces | Affichage des menus |
| Boutons poussoirs | Commandes des sons, transport et séquenceur |
| LEDs | Retour visuel des pas du séquenceur |
| Résistances 220 Ω | Protection des LEDs |
| Potentiomètres | Réglage du volume, tempo et swing |
| Encodeur rotatif | Navigation dans les menus |
| Microphone | Enregistrement audio |
| Haut-parleur / amplificateur | Sortie sonore |
| Boîtier imprimé / fabriqué | Intégration finale |

## 7. Câblage principal

### Boutons directs

| Fonction | Broche Teensy |
|---|---|
| KICK | 3 |
| SNARE | 4 |
| HIHAT | 5 |
| CLAP | 6 |

### Boutons de transport

| Fonction | Broche Teensy |
|---|---|
| PLAY | 22 |
| STOP | 24 |
| REC | 25 |

### Écran OLED

| Signal | Broche Teensy |
|---|---|
| SDA | 18 |
| SCL | 19 |

### Potentiomètres

| Réglage | Broche Teensy |
|---|---|
| Volume | A0 |
| Tempo | A1 |
| Swing | A2 |

### Séquenceur

| Pas | Bouton | Broche bouton | LED | Broche LED |
|---|---|---|---|---|
| 1 | L1 C1 | 26 | LED1 | 34 |
| 2 | L1 C2 | 27 | LED2 | 35 |
| 3 | L1 C3 | 31 | LED3 | 36 |
| 4 | L1 C4 | 32 | LED4 | 37 |
| 5 | L2 C1 | 0 | LED5 | 39 |
| 6 | L2 C2 | 1 | LED6 | 40 |
| 7 | L2 C3 | 9 | LED7 | 41 |
| 8 | L2 C4 | 17 | LED8 | 2 |

## 8. Installation

### Prérequis logiciels

Installer :

- Arduino IDE ;
- Teensyduino ;
- Teensy Audio Library ;
- Adafruit SSD1306 ;
- Adafruit GFX ;
- Encoder Library.

### Configuration Arduino IDE

Dans Arduino IDE :

```text
Type de carte : Teensy 4.1
USB Type      : Serial
CPU Speed     : 600 MHz
```

### Carte micro-SD

La carte micro-SD doit contenir les fichiers :

```text
KICK.WAV
SNARE.WAV
HIHAT.WAV
CLAP.WAV
```

Les fichiers doivent être au format WAV compatible avec la Teensy Audio Library.

## 9. Utilisation

### Lancer le prototype

1. Brancher la Teensy en USB.
2. Attendre l’affichage du menu sur l’OLED.
3. Vérifier que la carte SD est détectée.
4. Tester les quatre boutons de sons.

### Programmer une séquence

1. Aller dans le menu du séquenceur.
2. Choisir un son : Kick, Snare, Hihat ou Clap.
3. Appuyer sur les boutons du séquenceur pour placer le son.
4. Appuyer à nouveau sur un pas pour l’effacer.
5. Appuyer sur PLAY pour lancer la lecture.
6. Appuyer sur STOP pour arrêter.

### Enregistrer le micro

1. Appuyer sur REC.
2. Parler ou produire un son près du micro.
3. Appuyer sur STOP ou REC pour arrêter.
4. Aller dans le menu de lecture des enregistrements.
5. Lire le fichier enregistré.

## 10. Tests réalisés

Les tests ont été réalisés module par module :

- test de l’écran OLED ;
- test des boutons directs ;
- test des boutons du séquenceur ;
- test des LEDs ;
- test de la lecture des fichiers WAV ;
- test de la carte micro-SD ;
- test de l’enregistrement micro ;
- test du mode séquenceur ;
- test du boîtier et de l’intégration finale.

## 11. Problèmes rencontrés et corrections

### Mauvaise correspondance entre boutons et LEDs

Le mapping entre les boutons, les pas et les LEDs n’était pas toujours correct. Nous avons corrigé le tableau de correspondance dans le code et vérifié chaque entrée une par une.

### Confusion avec les boutons poussoirs 4 broches

Les boutons 4 broches peuvent créer des erreurs si les pattes utilisées ne sont pas les bonnes. Nous avons testé les contacts au multimètre afin d’identifier les bonnes paires de broches.

### Problèmes avec certaines broches LED

Certaines broches étaient en contact avec le GND à cause du câblage déjà collé. Nous avons privilégié la stabilité du son et du séquenceur plutôt que de forcer toutes les LEDs à fonctionner.

### Perte de sortie audio

Certaines broches ne devaient pas être utilisées pour les LEDs, car elles sont réservées à l’Audio Shield. Les broches audio ont été évitées afin de conserver la sortie sonore.

### Bruit du micro

Le micro captait beaucoup de parasites au début. Nous avons ajouté un filtrage logiciel et ajusté le gain pour obtenir un signal plus exploitable.

## 12. Structure du dépôt

Organisation recommandée du dépôt Git :

```text
Mini-Groovebox-Afrobeat/
│
├── README.md
├── code/
│   └── MiniGroovebox_Final.ino
│
├── docs/
│   ├── Rapport_Mini_Groovebox.pdf
│   └── Presentation.pdf
│
├── hardware/
│   ├── schema_cablage.png
│   └── pin_mapping.pdf
│
├── samples/
│   ├── KICK.WAV
│   ├── SNARE.WAV
│   ├── HIHAT.WAV
│   └── CLAP.WAV
│
├── fablab/
│   ├── boitier.stl
│   └── plans_boitier.pdf
│
└── media/
    └── video_demo.txt
```

## 13. Limites du prototype

Le prototype est fonctionnel, mais certaines limites restent présentes :

- le câblage reste sensible aux faux contacts ;
- le micro dépend de la qualité du câblage et de l’environnement sonore ;
- le changement de tempo agit sur le déclenchement des pas, pas sur la vitesse interne des fichiers WAV ;
- le prototype gagnerait en fiabilité avec un PCB ;
- le boîtier peut encore être amélioré pour faciliter l’accès aux connexions.

## 14. Améliorations possibles

Pour une version future, il serait possible de :

- réaliser un PCB propre ;
- améliorer le préampli micro ;
- ajouter une sauvegarde plus avancée des patterns ;
- ajouter une vraie gestion MIDI ;
- améliorer l’interface OLED ;
- ajouter plus de sons ;
- créer une meilleure coque finale ;
- ajouter une batterie pour rendre l’instrument autonome.

## 15. Démonstration

Scénario de démonstration conseillé :

1. Présenter rapidement le concept.
2. Lancer un son direct avec KICK, SNARE, HIHAT et CLAP.
3. Programmer un pattern simple.
4. Lancer la séquence avec PLAY.
5. Modifier le volume, le tempo ou le swing.
6. Faire un enregistrement micro.
7. Relire l’enregistrement.
8. Conclure sur les améliorations possibles.

## 16. Conclusion

La Mini Groovebox Afrobeat est un prototype pédagogique qui montre l’intégration de plusieurs compétences : électronique, programmation embarquée, traitement du signal, interface utilisateur et fabrication mécanique.

Le projet nous a permis de comprendre l’importance du câblage, du test par module et de la documentation. Même si le prototype reste perfectible, il répond à l’objectif principal : créer un instrument compact, jouable et démontrable.
