# Mini Groovebox Afrobeat

Projet pratique Teensy / Audio Shield pour la lecture de samples, le séquenceur 2x4 et l'enregistrement micro.

## Présentation

Ce dépôt contient le code source d'un prototype de **Mini Groovebox Afrobeat** basé sur une carte **Teensy 4.1** et un **Teensy Audio Shield SGTL5000**.
Le prototype permet de :

- lire des fichiers WAV depuis une carte micro-SD,
- déclencher quatre sons directs : Kick, Snare, Hihat, Clap,
- programmer une séquence 2x4 sur 8 pas,
- régler le volume, le tempo et le swing avec des potentiomètres,
- gérer les menus et l'affichage sur un écran OLED,
- enregistrer un signal micro au format WAV,
- lire et supprimer des enregistrements.

## Structure du dépôt

- `MiniGroovebox_CodeComplet_Final/` : dossier principal du projet Arduino.
  - `MiniGroovebox_Final_SEQ_ATTENTE_FIN_SON_POTS_OK.ino` : sketch Arduino principal.
  - `README.md` : documentation interne du module.
- `.gitignore` : règles Git pour ignorer les fichiers temporaires et les WAV.

## Installation et configuration

### Logiciels requis

- Arduino IDE
- Teensyduino
- Teensy Audio Library
- Adafruit SSD1306
- Adafruit GFX
- Encoder Library

### Configuration Arduino IDE

- Carte : **Teensy 4.1**
- USB Type : **Serial**
- CPU Speed : **600 MHz**

### Fichiers WAV

Sur la carte micro-SD, placez les quatre fichiers suivants à la racine :

```text
KICK.WAV
SNARE.WAV
HIHAT.WAV
CLAP.WAV
```

Les fichiers doivent être compatibles avec la Teensy Audio Library.

## Utilisation

1. Ouvrir `MiniGroovebox_CodeComplet_Final/MiniGroovebox_Final_SEQ_ATTENTE_FIN_SON_POTS_OK.ino` dans Arduino IDE.
2. Sélectionner la carte Teensy 4.1 et le port USB.
3. Téléverser le sketch sur la Teensy.
4. Insérer la carte micro-SD avec les fichiers WAV.
5. Démarrer le prototype et suivre les instructions à l'écran.

## Notes importantes

- Le dépôt ignore les fichiers `*.wav` par défaut pour éviter de versionner des fichiers audio volumineux.
- Si tu veux ajouter des samples de test, tu peux les stocker hors du dépôt ou les ajouter manuellement sur la carte SD.

## Sécurité GitHub

Ce dépôt est prêt pour GitHub avec le code source principal et un README racine clair.
Si tu veux, je peux aussi synchroniser le README interne à `MiniGroovebox_CodeComplet_Final/README.md` avec ce document.
