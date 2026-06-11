
MINI GROOVEBOX - CODE COMPLET FINAL PROPRE

Bibliotheques a installer dans Arduino IDE :
1. Adafruit GFX Library
2. Adafruit SSD1306
3. Encoder
4. Teensy Audio Library, deja fournie avec Teensyduino

Reglages Arduino IDE :
Board : Teensy 4.1
USB Type : Serial

Carte SD :
La carte SD est inseree dans l'Audio Shield.
Mettre les fichiers suivants a la racine :
KICK.WAV
SNARE.WAV
HIHAT.WAV
CLAP.WAV

Si les fichiers sont absents, le programme utilise des sons de synthese.

Navigation :
Tourner encodeur = defiler
Clic court = entrer / valider
Clic long = retour menu principal

Connexions encodeur :
CLK / A -> pin 28
DT / B  -> pin 29
SW      -> pin 30
VCC     -> 3.3V
GND     -> GND

Connexions OLED :
SDA -> pin 18
SCL -> pin 19
VCC -> 3.3V
GND -> GND

Si l'encodeur defile a l'envers :
Inverser CLK et DT, ou inverser les pins ENC_A et ENC_B dans le code.
