/*
  ============================================================
  MINI GROOVEBOX AFROBEAT - VERSION FINALE STABLE
  Teensy 4.1 + Audio Shield + OLED + SD

  CORRECTIONS IMPORTANTES :
  - A0 = Volume physique.
  - A1 = Tempo physique.
  - A2 = Swing physique.
  - Les valeurs s'affichent directement sur OLED quand on tourne les potentiometres.
  - Le menu Auto est retire : bouton PLAY = lancer/arreter le sequenceur.
  - Le sequenceur respecte l'ordre 1 -> 8.
  - Le sequenceur attend la fin du son avant de passer au pas suivant.
  - Tempo/Swing agissent sur l'espacement des pas du sequenceur.
  - Les fichiers WAV gardent leur vitesse normale avec AudioPlaySdWav.
  - Le changement de vitesse/hauteur du WAV lui-meme demande un moteur audio avance.
  - LED3 et LED4 sont desactivees dans le code pour eviter les courts-circuits
    sur les anciennes lignes 36/37. Les autres LEDs restent actives.
  - Calibration micro au demarrage supprimee.
  - Filtre micro plus leger pour que l'enregistrement sorte encore.

  CABLAGE SEQUENCEUR :

  L1 :
  R1 -> pin 33
  B1 -> pin 26
  B2 -> pin 27
  B3 -> pin 31
  B4 -> pin 32

  L2 :
  R2 -> pin 38
  B5 -> pin 0
  B6 -> pin 1
  B7 -> pin 9
  B8 -> pin 17

  LEDs :
  LED1 -> pin 34
  LED2 -> pin 35
  LED3 -> non utilisee / laissee non fonctionnelle
  LED4 -> non utilisee / laissee non fonctionnelle
  LED5 -> pin 39
  LED6 -> pin 40
  LED7 -> pin 41
  LED8 -> pin 2

  BOUTONS DIRECTS :
  KICK  -> pin 3
  SNARE -> pin 4
  HIHAT -> pin 5
  CLAP  -> pin 6

  TRANSPORT :
  PLAY -> pin 22
  STOP -> pin 24
  REC  -> pin 25

  POTENTIOMETRES :
  A0 -> Volume
  A1 -> Tempo
  A2 -> Swing

  FICHIERS WAV SUR LA SD :
  KICK.WAV
  SNARE.WAV
  HIHAT.WAV
  CLAP.WAV
  ============================================================
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Encoder.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <string.h>
#include <stdlib.h>

// Ce programme pilote une mini groovebox sur Teensy 4.1.
// Il gère :
// - lecture de sons WAV depuis une carte SD,
// - séquenceur 2x4 avec avance pas à pas et LEDs,
// - enregistrement micro en WAV,
// - affichage OLED et navigation par encodeur rotatif,
// - boutons directs pour déclencher les sons et contrôler le transport.
// Les sections suivantes sont organisées par fonctionnalité :
// matériel, audio, séquenceur, écran, SD et logique principale.

// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledOK = false; // True si l'écran OLED est initialisé correctement

// ============================================================
// CLIC ENCODEUR
// ============================================================

#define CLICK_NONE  0
#define CLICK_SHORT 1
#define CLICK_LONG  2

// ============================================================
// BOUTONS DIRECTS
// ============================================================

const int BTN_KICK  = 3;
const int BTN_SNARE = 4;
const int BTN_HIHAT = 5;
const int BTN_CLAP  = 6;

const int BTN_PLAY  = 22;
const int BTN_STOP  = 24;
const int BTN_REC   = 25;

// ============================================================
// SEQUENCEUR 2x4
// ============================================================

const int STEP_COUNT = 8;
const int SOUND_COUNT = 4;

const int ROW_L1 = 33;
const int ROW_L2 = 38;

const int stepPins[STEP_COUNT] = {
  26, 27, 31, 32,
  0,  1,  9,  17
};

// -1 = LED desactivee. Cela evite de forcer les pins 36/37 si elles sont en court-circuit.
const int ledPins[STEP_COUNT] = {
  34, 35, -1, -1,
  39, 40, 41, 2
};

bool lastStepState[STEP_COUNT] = {
  false, false, false, false,
  false, false, false, false
};

unsigned long lastStepTimeButton[STEP_COUNT] = {
  0, 0, 0, 0,
  0, 0, 0, 0
};

const unsigned long STEP_DEBOUNCE = 130;

// -1 = pas vide ; 0 = KICK ; 1 = SNARE ; 2 = HIHAT ; 3 = CLAP
int sequenceStep[STEP_COUNT] = {
  -1, -1, -1, -1,
  -1, -1, -1, -1
};
// Chaque pas contient une valeur : -1 pour vide, 0=KICK, 1=SNARE, 2=HIHAT, 3=CLAP.

int selectedSeqSound = 0;
int selectedSound = 0;

// ============================================================
// PATTERNS SAUVEGARDES
// ============================================================

const int MAX_PATTERNS = 8;
int selectedPattern = 1;

int patternPreview[STEP_COUNT] = {
  -1, -1, -1, -1,
  -1, -1, -1, -1
};

bool patternPreviewOK = false;
bool patternDirty = false;
unsigned long patternSaveAt = 0;

// ============================================================
// ENCODEUR
// ============================================================

const int ENC_A  = 28;
const int ENC_B  = 29;
const int ENC_SW = 30;

Encoder encoder(ENC_A, ENC_B);
long lastEncPos = 0;

// ============================================================
// POTENTIOMETRES
// ============================================================

const int POT_VOLUME = A0;
const int POT_TEMPO  = A1;
const int POT_SWING  = A2;

int volumePercent = 70;
float volumeValue = 0.70;

int tempoPercent = 55;
int bpm = 140;
int swingPercent = 10;

unsigned long lastPotRead = 0;

// ============================================================
// POPUP REGLAGES
// ============================================================

int popupType = 0;
// 0 = aucun, 1 = volume, 2 = tempo, 3 = swing, 4 = save, 5 = load, 6 = delete
unsigned long popupUntil = 0;

// ============================================================
// SD AUDIO SHIELD
// ============================================================

const int SDCARD_CS_PIN   = 10;
const int SDCARD_MOSI_PIN = 11;
const int SDCARD_SCK_PIN  = 13;

// ============================================================
// AUDIO
// ============================================================

AudioPlaySdWav wavKick;
AudioPlaySdWav wavSnare;
AudioPlaySdWav wavHihat;
AudioPlaySdWav wavClap;
AudioPlaySdWav wavRec;

AudioInputI2S audioInput;
AudioAnalyzePeak peakMic;
AudioRecordQueue recQueue;

AudioMixer4 mixerA;
AudioMixer4 mixerB;
AudioMixer4 mixerL;
AudioMixer4 mixerR;

AudioAmplifier ampL;
AudioAmplifier ampR;

AudioOutputI2S audioOutput;
AudioControlSGTL5000 audioShield;

AudioConnection patch1(wavKick,  0, mixerA, 0);
AudioConnection patch2(wavSnare, 0, mixerA, 1);
AudioConnection patch3(wavHihat, 0, mixerA, 2);
AudioConnection patch4(wavClap,  0, mixerA, 3);

AudioConnection patch5(wavRec, 0, mixerB, 0);

AudioConnection patch6(audioInput, 0, peakMic, 0);
AudioConnection patch7(audioInput, 0, recQueue, 0);

AudioConnection patch8(mixerA, 0, mixerL, 0);
AudioConnection patch9(mixerB, 0, mixerL, 1);

AudioConnection patch10(mixerA, 0, mixerR, 0);
AudioConnection patch11(mixerB, 0, mixerR, 1);

AudioConnection patch12(mixerL, 0, ampL, 0);
AudioConnection patch13(mixerR, 0, ampR, 0);

AudioConnection patch14(ampL, 0, audioOutput, 0);
AudioConnection patch15(ampR, 0, audioOutput, 1);
// Connexions audio : mixage stéréo final vers la sortie I2S.

// ============================================================
// MICRO
// ============================================================

int micGainValue = 32;

float micRawLast = 0.0;
float micNoiseFloor = 0.0012;
float micLevelSmooth = 0.0;
int micLevelPercent = 0;
unsigned long lastMicUpdate = 0;

const float MIC_DEAD_ZONE = 0.0010;
const float MIC_DISPLAY_SCALE = 12000.0;

// Reglages volontairement doux pour ne pas couper la voix.
const int MIC_RECORD_GAIN_PERCENT = 220;
const int MIC_RECORD_LIMIT = 30000;
const int MIC_NOISE_GATE_RAW = 20;
const int MIC_NOISE_GATE_AFTER_GAIN = 60;

int32_t micHpPrevIn = 0;
int32_t micHpPrevOut = 0;
int32_t micLpPrevOut = 0;

const int REC_SKIP_BLOCKS_AT_START = 4;
int recSkipBlocks = 0;

// ============================================================
// SONS
// ============================================================

const char* soundNames[SOUND_COUNT] = {
  "KICK",
  "SNARE",
  "HIHAT",
  "CLAP"
};

const char* soundFiles[SOUND_COUNT] = {
  "KICK.WAV",
  "SNARE.WAV",
  "HIHAT.WAV",
  "CLAP.WAV"
};

bool soundExists[SOUND_COUNT] = {
  false, false, false, false
};

// ============================================================
// SD / REC AUDIO
// ============================================================

bool sdOK = false;
bool sdIsTeensy = false;
bool sdIsAudioShield = false;

File recFile;
bool isRecording = false;

const int MAX_REC_SECONDS = 20;
const int MAX_REC_FILES = 20;

unsigned long recStartedAt = 0;
unsigned long recByteCount = 0;

int selectedRecIndex = 1;
int currentRecIndex = 1;

bool recPlaybackActive = false;
char recPlaybackName[13] = "REC001.WAV";
int cachedRecCount = 0;

// ============================================================
// SEQUENCE PLAY
// ============================================================

bool seqRunning = false;
int currentStep = 0;
int playingStep = -1;
bool seqStepActive = false;
bool seqStepHasSound = false;
int seqStepSound = -1;
unsigned long seqStepMinUntil = 0;

// ============================================================
// ECRANS
// ============================================================

enum Screen {
  SCREEN_MENU,
  SCREEN_SOUNDS,
  SCREEN_SEQ,
  SCREEN_PATTERNS,
  SCREEN_RECORD,
  SCREEN_PLAY_REC,
  SCREEN_DELETE_REC,
  SCREEN_MIC,
  SCREEN_CONTROLS,
  SCREEN_ABOUT,
  SCREEN_SLEEP
};

Screen screen = SCREEN_MENU;

const int MENU_COUNT = 10;

const char* menuItems[MENU_COUNT] = {
  "Sons",
  "Seq 2x4",
  "Patterns",
  "Enregistrer",
  "Lire REC",
  "Suppr REC",
  "Micro",
  "Reglages",
  "Infos",
  "Veille"
};

int menuIndex = 0;
bool sleepMode = false;

// ============================================================
// TEMPS
// ============================================================

unsigned long lastButtonTime = 0;
unsigned long lastScreenRefresh = 0;
unsigned long holdScreenUntil = 0;
bool forceRedraw = true;

// ============================================================
// PROTOTYPES UTILES
// ============================================================

void drawPlayScreen(const char* name);
void updateLeds();
void stopSequencer();
void startSequencer();
void refreshScreen(bool force = false);
bool savePatternSlot(int index);
bool loadPatternSlot(int index);
bool deletePatternSlot(int index);
void showPopup(int type);

// ============================================================
// OUTILS
// ============================================================

// Ignore les rebonds d'un bouton pour éviter les doubles appuis non désirés.
bool debounceButton() {
  if (millis() - lastButtonTime > 180) {
    lastButtonTime = millis();
    return true;
  }
  return false;
}

const char* getSeqSoundName(int value) {
  if (value == -1) return "VIDE";
  if (value == 0) return "KICK";
  if (value == 1) return "SNARE";
  if (value == 2) return "HIHAT";
  if (value == 3) return "CLAP";
  return "ERR";
}

char getSeqLetter(int value) {
  if (value == -1) return '-';
  if (value == 0) return 'K';
  if (value == 1) return 'S';
  if (value == 2) return 'H';
  if (value == 3) return 'C';
  return '?';
}

void makeRecFilename(int index, char* buffer) {
  sprintf(buffer, "REC%03d.WAV", index);
}

void makePatternFilename(int index, char* buffer) {
  sprintf(buffer, "PAT%02d.TXT", index);
}

// ============================================================
// OLED
// ============================================================

// Dessine une barre de progression simple sur l'écran OLED.
void drawBar(int x, int y, int w, int h, int value, int maxValue) {
  display.drawRect(x, y, w, h, SSD1306_WHITE);
  if (maxValue <= 0) maxValue = 1;
  int fillW = map(value, 0, maxValue, 0, w - 4);
  if (fillW < 0) fillW = 0;
  if (fillW > w - 4) fillW = w - 4;
  display.fillRect(x + 2, y + 2, fillW, h - 4, SSD1306_WHITE);
}

void drawHeader(const char* title) {
  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print(title);

  display.setCursor(70, 2);
  display.print(sdOK ? "SD" : "NO");

  if (seqRunning) {
    display.setCursor(92, 2);
    display.print("PLAY");
  }

  if (isRecording) {
    display.setCursor(112, 2);
    display.print("REC");
  }

  display.setTextColor(SSD1306_WHITE);
}

void drawFooter(const char* txt) {
  display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(txt);
}

// Affiche l'écran principal d'action.
// Ce message apparaît quand un son est joué ou quand une alerte est nécessaire.
void drawPlayScreen(const char* name) {
  if (!oledOK) return;
  display.clearDisplay();
  drawHeader("ACTION");
  display.setTextSize(2);
  display.setCursor(0, 17);
  display.print(name);
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("VOL ");
  display.print(volumePercent);
  display.print("% T ");
  display.print(tempoPercent);
  display.print("% S ");
  display.print(swingPercent);
  display.print("%");
  display.display();
}

// Affiche un message temporaire sur l'écran OLED, puis revient à l'écran normal.
void showPopup(int type) {
  // Affiche un message temporaire sur l'écran OLED. Le message reste visible
  // pendant environ 900 ms avant de revenir à l'écran précédent.
  popupType = type;
  popupUntil = millis() + 900;
  forceRedraw = true;
}

void drawPopupScreen() {
  display.clearDisplay();

  if (popupType == 1) {
    drawHeader("VOLUME");
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print(volumePercent);
    display.print("%");
    display.setTextSize(1);
    drawBar(0, 42, 128, 10, volumePercent, 100);
    display.setCursor(0, 56);
    display.print("A0 volume");
  }
  else if (popupType == 2) {
    drawHeader("TEMPO");
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print(tempoPercent);
    display.print("%");
    display.setTextSize(1);
    display.setCursor(0, 36);
    display.print(bpm);
    display.print(" BPM");
    drawBar(0, 46, 128, 8, tempoPercent, 100);
  }
  else if (popupType == 3) {
    drawHeader("SWING");
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print(swingPercent);
    display.print("%");
    display.setTextSize(1);
    drawBar(0, 42, 128, 10, swingPercent, 100);
    display.setCursor(0, 56);
    display.print("A2 groove");
  }
  else if (popupType == 4) {
    drawHeader("PATTERN");
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("SAVE OK");
    display.setTextSize(1);
    display.setCursor(0, 46);
    display.print("Pattern sauvegarde");
  }
  else if (popupType == 5) {
    drawHeader("PATTERN");
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("LOAD OK");
    display.setTextSize(1);
    display.setCursor(0, 46);
    display.print("Pattern charge");
  }
  else if (popupType == 6) {
    drawHeader("PATTERN");
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("DELETE");
    display.setTextSize(1);
    display.setCursor(0, 46);
    display.print("Pattern supprime");
  }

  display.display();
}

// ============================================================
// AUDIO SETTINGS
// ============================================================

void muteAudioOutput() {
  audioShield.volume(0.0);
  ampL.gain(0.0);
  ampR.gain(0.0);
}

// Applique le volume choisi au mixeur et à la sortie audio.
void applyVolume() {
  float v = volumeValue;
  if (v < 0.0) v = 0.0;
  if (v > 0.85) v = 0.85;
  audioShield.volume(v);
  audioShield.lineOutLevel(20);
  ampL.gain(v * 1.05);
  ampR.gain(v * 1.05);
}

void fadeInAudio() {
  for (int i = 0; i <= volumePercent; i += 5) {
    float v = i / 100.0;
    if (v > volumeValue) v = volumeValue;
    if (v > 0.85) v = 0.85;
    audioShield.volume(v);
    ampL.gain(v * 1.05);
    ampR.gain(v * 1.05);
    delay(15);
  }
  applyVolume();
}

void applySoundGains() {
  mixerA.gain(0, 0.90);
  mixerA.gain(1, 0.78);
  mixerA.gain(2, 0.60);
  mixerA.gain(3, 0.78);

  mixerB.gain(0, 0.92);
  mixerB.gain(1, 0.00);
  mixerB.gain(2, 0.00);
  mixerB.gain(3, 0.00);

  mixerL.gain(0, 0.88);
  mixerL.gain(1, 0.95);
  mixerL.gain(2, 0.00);
  mixerL.gain(3, 0.00);

  mixerR.gain(0, 0.88);
  mixerR.gain(1, 0.95);
  mixerR.gain(2, 0.00);
  mixerR.gain(3, 0.00);
}

// ============================================================
// MICRO
// ============================================================

// Analyse le niveau du micro et met à jour l'affichage micro.
void updateMicLevel() {
  if (peakMic.available()) {
    micRawLast = peakMic.read();

    float corrected = micRawLast - micNoiseFloor;
    if (corrected < MIC_DEAD_ZONE) corrected = 0.0;

    float instant = corrected * MIC_DISPLAY_SCALE;
    if (instant < 0.0) instant = 0.0;
    if (instant > 100.0) instant = 100.0;

    if (instant > micLevelSmooth) {
      micLevelSmooth = (micLevelSmooth * 0.45) + (instant * 0.55);
    } else {
      micLevelSmooth = (micLevelSmooth * 0.90) + (instant * 0.10);
    }

    if (micLevelSmooth < 1.5) micLevelSmooth = 0.0;

    micLevelPercent = (int)micLevelSmooth;
    if (micLevelPercent < 0) micLevelPercent = 0;
    if (micLevelPercent > 100) micLevelPercent = 100;

    lastMicUpdate = millis();
  }

  if (millis() - lastMicUpdate > 400) {
    micLevelSmooth *= 0.85;
    if (micLevelSmooth < 1.5) micLevelSmooth = 0.0;
    micLevelPercent = (int)micLevelSmooth;
  }
}

void resetMicFilter() {
  micHpPrevIn = 0;
  micHpPrevOut = 0;
  micLpPrevOut = 0;
}

int16_t processMicSample(int16_t input) {
  int32_t raw = input;

  // Passe-haut doux : retire un peu de ronflement grave sans tuer la voix.
  int32_t hp = raw - micHpPrevIn + ((micHpPrevOut * 995) / 1000);
  micHpPrevIn = raw;
  micHpPrevOut = hp;

  if (hp > -MIC_NOISE_GATE_RAW && hp < MIC_NOISE_GATE_RAW) {
    hp = 0;
  }

  int32_t sample = (hp * MIC_RECORD_GAIN_PERCENT) / 100;

  if (sample > -MIC_NOISE_GATE_AFTER_GAIN && sample < MIC_NOISE_GATE_AFTER_GAIN) {
    sample = 0;
  }

  // Passe-bas leger : reduit les aigus parasites, mais garde le son audible.
  int32_t lp = ((micLpPrevOut * 2) + sample) / 3;
  micLpPrevOut = lp;

  if (lp > MIC_RECORD_LIMIT) lp = MIC_RECORD_LIMIT;
  if (lp < -MIC_RECORD_LIMIT) lp = -MIC_RECORD_LIMIT;

  return (int16_t)lp;
}

// ============================================================
// SD
// ============================================================

// Démarre la carte SD. Supporte Audio Shield ou Teensy SD intégré.
void initSDCard() {
  // Démarre la carte SD et détecte si elle est sur la carte audio ou sur le Teensy.
  sdOK = false;
  sdIsTeensy = false;
  sdIsAudioShield = false;

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  if (SD.begin(SDCARD_CS_PIN)) {
    sdOK = true;
    sdIsAudioShield = true;
    Serial.println("SD Audio Shield OK");
    return;
  }

#ifdef BUILTIN_SDCARD
  if (SD.begin(BUILTIN_SDCARD)) {
    sdOK = true;
    sdIsTeensy = true;
    Serial.println("SD Teensy OK");
    return;
  }
#endif

  Serial.println("NO SD");
}

void checkSoundFiles() {
  if (!sdOK) {
    for (int i = 0; i < SOUND_COUNT; i++) soundExists[i] = false;
    return;
  }

  for (int i = 0; i < SOUND_COUNT; i++) {
    soundExists[i] = SD.exists(soundFiles[i]);
    Serial.print(soundFiles[i]);
    Serial.print(" : ");
    Serial.println(soundExists[i] ? "OK" : "ABSENT");
  }
}

// ============================================================
// REC / PATTERN UTILS
// ============================================================

bool recExists(int index) {
  // Vérifie si un fichier d'enregistrement existe sur la carte SD.
  if (!sdOK) return false;
  char filename[13];
  makeRecFilename(index, filename);
  return SD.exists(filename);
}

bool patternExists(int index) {
  if (!sdOK) return false;
  char filename[13];
  makePatternFilename(index, filename);
  return SD.exists(filename);
}

int findFirstRecording() {
  for (int i = 1; i <= MAX_REC_FILES; i++) if (recExists(i)) return i;
  return 0;
}

int findNextFreeRecording() {
  for (int i = 1; i <= MAX_REC_FILES; i++) if (!recExists(i)) return i;
  return 0;
}

int countRecordings() {
  int count = 0;
  for (int i = 1; i <= MAX_REC_FILES; i++) if (recExists(i)) count++;
  return count;
}

void refreshRecordingCache() {
  if (!sdOK) {
    cachedRecCount = 0;
    return;
  }
  if (wavRec.isPlaying()) return;

  cachedRecCount = countRecordings();
  int first = findFirstRecording();
  if (first > 0) selectedRecIndex = first;
}

void selectNextRecording(int direction) {
  if (wavRec.isPlaying()) return;

  int start = selectedRecIndex;
  for (int i = 0; i < MAX_REC_FILES; i++) {
    selectedRecIndex += direction;
    if (selectedRecIndex < 1) selectedRecIndex = MAX_REC_FILES;
    if (selectedRecIndex > MAX_REC_FILES) selectedRecIndex = 1;
    if (recExists(selectedRecIndex)) return;
  }
  selectedRecIndex = start;
}

void selectNextPattern(int direction) {
  selectedPattern += direction;
  if (selectedPattern < 1) selectedPattern = MAX_PATTERNS;
  if (selectedPattern > MAX_PATTERNS) selectedPattern = 1;
  forceRedraw = true;
}

// ============================================================
// PATTERN : LIRE / SAUVER / SUPPRIMER
// ============================================================

bool readPatternIntoArray(int index, int target[STEP_COUNT]) {
  if (!sdOK) return false;

  char filename[13];
  makePatternFilename(index, filename);

  if (!SD.exists(filename)) {
    for (int i = 0; i < STEP_COUNT; i++) target[i] = -1;
    return false;
  }

  File file = SD.open(filename);
  if (!file) {
    for (int i = 0; i < STEP_COUNT; i++) target[i] = -1;
    return false;
  }

  char buffer[80];
  int len = 0;
  while (file.available() && len < 79) {
    buffer[len++] = file.read();
  }
  buffer[len] = '\0';
  file.close();

  char* token = strtok(buffer, ",; \r\n");
  int pos = 0;

  while (token != NULL && pos < STEP_COUNT) {
    int value = atoi(token);
    if (value < -1 || value > 3) value = -1;
    target[pos++] = value;
    token = strtok(NULL, ",; \r\n");
  }

  while (pos < STEP_COUNT) target[pos++] = -1;
  return true;
}

// Charge un pattern depuis la SD et active le séquenceur.
bool loadPatternSlot(int index) {
  bool ok = readPatternIntoArray(index, sequenceStep);
  if (ok) {
    selectedPattern = index;
    playingStep = -1;
    currentStep = 0;
    seqStepActive = false;
    updateLeds();
    Serial.print("PATTERN charge : PAT");
    Serial.println(index);
  }
  return ok;
}

bool savePatternSlot(int index) {
  if (!sdOK) return false;

  char filename[13];
  makePatternFilename(index, filename);

  if (SD.exists(filename)) SD.remove(filename);

  File file = SD.open(filename, FILE_WRITE);
  if (!file) return false;

  for (int i = 0; i < STEP_COUNT; i++) {
    file.print(sequenceStep[i]);
    if (i < STEP_COUNT - 1) file.print(",");
  }
  file.println();
  file.close();

  patternDirty = false;
  Serial.print("PATTERN sauvegarde : ");
  Serial.println(filename);
  return true;
}

bool deletePatternSlot(int index) {
  if (!sdOK) return false;

  char filename[13];
  makePatternFilename(index, filename);
  if (!SD.exists(filename)) return false;

  SD.remove(filename);
  for (int i = 0; i < STEP_COUNT; i++) patternPreview[i] = -1;
  patternPreviewOK = false;

  Serial.print("PATTERN supprime : ");
  Serial.println(filename);
  return true;
}

void requestPatternSave() {
  if (!sdOK) return;
  patternDirty = true;
  patternSaveAt = millis() + 800;
}

void servicePatternSave() {
  if (!patternDirty) return;
  if (!sdOK) return;
  if (seqRunning) return;
  if (isRecording) return;
  if (wavRec.isPlaying()) return;

  if ((long)(millis() - patternSaveAt) >= 0) {
    savePatternSlot(selectedPattern);
  }
}

void updatePatternPreview() {
  patternPreviewOK = readPatternIntoArray(selectedPattern, patternPreview);
}

int findFirstPattern() {
  for (int i = 1; i <= MAX_PATTERNS; i++) if (patternExists(i)) return i;
  return 0;
}

void loadFirstPatternAtStartup() {
  int first = findFirstPattern();
  if (first > 0) {
    selectedPattern = first;
    loadPatternSlot(first);
  }
}

// ============================================================
// WAV HEADER REC AUDIO
// ============================================================

void writeLE16(File &file, uint16_t value) {
  file.write(value & 0xFF);
  file.write((value >> 8) & 0xFF);
}

void writeLE32(File &file, uint32_t value) {
  file.write(value & 0xFF);
  file.write((value >> 8) & 0xFF);
  file.write((value >> 16) & 0xFF);
  file.write((value >> 24) & 0xFF);
}

void writeWavHeader(File &file, uint32_t dataSize) {
  uint32_t sampleRate = 44100;
  uint16_t bitsPerSample = 16;
  uint16_t channels = 1;
  uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
  uint16_t blockAlign = channels * bitsPerSample / 8;

  file.write((const uint8_t*)"RIFF", 4);
  writeLE32(file, 36 + dataSize);
  file.write((const uint8_t*)"WAVE", 4);
  file.write((const uint8_t*)"fmt ", 4);
  writeLE32(file, 16);
  writeLE16(file, 1);
  writeLE16(file, channels);
  writeLE32(file, sampleRate);
  writeLE32(file, byteRate);
  writeLE16(file, blockAlign);
  writeLE16(file, bitsPerSample);
  file.write((const uint8_t*)"data", 4);
  writeLE32(file, dataSize);
}

// ============================================================
// AUDIO PLAYBACK
// ============================================================

void stopAllDrumSounds() {
  wavKick.stop();
  wavSnare.stop();
  wavHihat.stop();
  wavClap.stop();
}

bool isDrumSoundPlaying(int index) {
  if (index == 0) return wavKick.isPlaying();
  if (index == 1) return wavSnare.isPlaying();
  if (index == 2) return wavHihat.isPlaying();
  if (index == 3) return wavClap.isPlaying();
  return false;
}

void stopAllSounds() {
  stopAllDrumSounds();
  wavRec.stop();
  recPlaybackActive = false;
}

// Joue un son depuis la SD en fonction de l'index de sample.
bool playSoundFile(int index) {
  if (index < 0 || index >= SOUND_COUNT) return false;
  if (!sdOK) return false;
  if (!soundExists[index]) return false;

  stopAllDrumSounds();
  delay(4);

  if (index == 0) return wavKick.play(soundFiles[index]);
  if (index == 1) return wavSnare.play(soundFiles[index]);
  if (index == 2) return wavHihat.play(soundFiles[index]);
  if (index == 3) return wavClap.play(soundFiles[index]);
  return false;
}

// Déclenche un son direct ou affiche un message si la SD ou le son manque.
void triggerSound(int index, bool showScreen = true) {
  if (isRecording) return;
  if (recPlaybackActive || wavRec.isPlaying()) return;

  if (index < 0 || index >= SOUND_COUNT) return;

  selectedSeqSound = index;
  selectedSound = index;

  if (seqRunning) {
    forceRedraw = true;
    return;
  }

  if (!sdOK) {
    if (showScreen) {
      drawPlayScreen("NO SD");
      holdScreenUntil = millis() + 800;
    }
    return;
  }

  if (!soundExists[index]) {
    if (showScreen) {
      drawPlayScreen("ABSENT");
      holdScreenUntil = millis() + 800;
    }
    return;
  }

  bool ok = playSoundFile(index);
  if (showScreen) {
    drawPlayScreen(ok ? soundNames[index] : "ERREUR");
    holdScreenUntil = millis() + 800;
  }
}

// ============================================================
// REC AUDIO
// ============================================================

void serviceRecording() {
  if (!isRecording) return;

  while (recQueue.available() > 0) {
    int16_t* buffer = recQueue.readBuffer();

    if (recSkipBlocks > 0) {
      recSkipBlocks--;
      recQueue.freeBuffer();
      continue;
    }

    if (recFile) {
      static int16_t processed[128];
      for (int i = 0; i < 128; i++) {
        processed[i] = processMicSample(buffer[i]);
      }
      recFile.write((byte*)processed, 256);
      recByteCount += 256;
    }

    recQueue.freeBuffer();
  }
}

// Commence un enregistrement sur SD en créant un fichier WAV.
void startRecording() {
  if (!sdOK) {
    drawPlayScreen("NO SD");
    holdScreenUntil = millis() + 800;
    return;
  }

  if (isRecording) return;
  if (recPlaybackActive || wavRec.isPlaying()) return;

  currentRecIndex = findNextFreeRecording();
  if (currentRecIndex == 0) {
    drawPlayScreen("REC FULL");
    holdScreenUntil = millis() + 1000;
    return;
  }

  char filename[13];
  makeRecFilename(currentRecIndex, filename);

  stopSequencer();
  stopAllSounds();

  recFile = SD.open(filename, FILE_WRITE);
  if (!recFile) {
    drawPlayScreen("REC ERR");
    holdScreenUntil = millis() + 1000;
    return;
  }

  writeWavHeader(recFile, 0);

  recByteCount = 0;
  recStartedAt = millis();
  recSkipBlocks = REC_SKIP_BLOCKS_AT_START;
  resetMicFilter();

  recQueue.begin();
  while (recQueue.available() > 0) {
    recQueue.readBuffer();
    recQueue.freeBuffer();
  }

  isRecording = true;
  screen = SCREEN_RECORD;
  forceRedraw = true;

  Serial.print("REC START : ");
  Serial.println(filename);
}

// Arrête l'enregistrement et écrit l'en-tête WAV final.
void stopRecording() {
  if (!isRecording) return;

  serviceRecording();
  recQueue.end();
  serviceRecording();

  if (recFile) {
    recFile.seek(0);
    writeWavHeader(recFile, recByteCount);
    recFile.close();
  }

  isRecording = false;
  selectedRecIndex = currentRecIndex;
  cachedRecCount = countRecordings();

  drawPlayScreen("REC OK");
  holdScreenUntil = millis() + 1000;
  forceRedraw = true;

  Serial.println("REC STOP");
}

void toggleRecordingButton() {
  if (recPlaybackActive || wavRec.isPlaying()) return;
  if (seqRunning) stopSequencer();
  if (isRecording) stopRecording();
  else startRecording();
}

void playSelectedRecording() {
  if (isRecording) return;
  if (seqRunning) return;
  if (recPlaybackActive || wavRec.isPlaying()) return;

  if (!sdOK) {
    drawPlayScreen("NO SD");
    holdScreenUntil = millis() + 800;
    return;
  }

  if (!recExists(selectedRecIndex)) {
    drawPlayScreen("AUCUN REC");
    holdScreenUntil = millis() + 800;
    return;
  }

  makeRecFilename(selectedRecIndex, recPlaybackName);
  stopAllSounds();
  delay(30);

  bool ok = wavRec.play(recPlaybackName);
  if (ok) {
    recPlaybackActive = true;
    screen = SCREEN_PLAY_REC;
    forceRedraw = true;
    Serial.print("PLAY REC : ");
    Serial.println(recPlaybackName);
  } else {
    recPlaybackActive = false;
    drawPlayScreen("ERR REC");
    holdScreenUntil = millis() + 900;
  }
}

void servicePlaybackEnd() {
  if (recPlaybackActive && !wavRec.isPlaying()) {
    recPlaybackActive = false;
    if (screen == SCREEN_PLAY_REC) forceRedraw = true;
    Serial.println("REC PLAY FIN");
  }
}

void deleteSelectedRecording() {
  if (!sdOK) return;
  if (wavRec.isPlaying()) return;
  if (!recExists(selectedRecIndex)) return;

  char filename[13];
  makeRecFilename(selectedRecIndex, filename);

  wavRec.stop();
  recPlaybackActive = false;
  SD.remove(filename);

  cachedRecCount = countRecordings();
  int first = findFirstRecording();
  selectedRecIndex = first > 0 ? first : 1;

  drawPlayScreen("SUPPRIME");
  holdScreenUntil = millis() + 900;
  forceRedraw = true;
}

// ============================================================
// SEQUENCEUR BOUTONS
// ============================================================

void setupStepButtons() {
  pinMode(ROW_L1, OUTPUT);
  pinMode(ROW_L2, OUTPUT);
  digitalWrite(ROW_L1, LOW);
  digitalWrite(ROW_L2, LOW);

  for (int i = 0; i < STEP_COUNT; i++) {
    pinMode(stepPins[i], INPUT_PULLUP);
    lastStepState[i] = false;
    lastStepTimeButton[i] = 0;
  }
}

// Gère les boutons tactiles du séquenceur 2x4.
// Chaque bouton active ou désactive le son actuellement sélectionné pour ce pas.
void handleStepButtons() {
  if (isRecording || recPlaybackActive || wavRec.isPlaying()) return;

  for (int i = 0; i < STEP_COUNT; i++) {
    bool pressed = digitalRead(stepPins[i]) == LOW;

    if (pressed && !lastStepState[i] && millis() - lastStepTimeButton[i] > STEP_DEBOUNCE) {
      lastStepState[i] = true;
      lastStepTimeButton[i] = millis();

      if (sequenceStep[i] == selectedSeqSound) {
        sequenceStep[i] = -1;
      } else {
        sequenceStep[i] = selectedSeqSound;
      }

      requestPatternSave();

      if (screen != SCREEN_SEQ && screen != SCREEN_PATTERNS) screen = SCREEN_SEQ;
      forceRedraw = true;

      Serial.print("PAS ");
      Serial.print(i + 1);
      Serial.print(" = ");
      Serial.println(getSeqSoundName(sequenceStep[i]));
    }

    if (!pressed) lastStepState[i] = false;
  }
}

// ============================================================
// LEDS
// ============================================================

void setupLeds() {
  for (int i = 0; i < STEP_COUNT; i++) {
    if (ledPins[i] >= 0) {
      pinMode(ledPins[i], OUTPUT);
      digitalWrite(ledPins[i], LOW);
    }
  }
}

// Allume ou éteint les LEDs :
// - si le séquenceur est arrêté, les LEDs montrent les pas actifs,
// - si le séquenceur tourne, la LED du pas en cours clignote.
void updateLeds() {
  for (int i = 0; i < STEP_COUNT; i++) {
    if (ledPins[i] < 0) continue;

    bool ledOn = false;

    if (!seqRunning) {
      ledOn = (sequenceStep[i] != -1);
    } else {
      ledOn = (i == playingStep && sequenceStep[i] != -1);
    }

    digitalWrite(ledPins[i], ledOn ? HIGH : LOW);
  }
}

void clearSequence() {
  for (int i = 0; i < STEP_COUNT; i++) sequenceStep[i] = -1;
  playingStep = -1;
  currentStep = 0;
  seqStepActive = false;
  requestPatternSave();
  updateLeds();
  forceRedraw = true;
}

// ============================================================
// PLAY SEQUENCE : TEMPO + SWING STRICTS
// ============================================================

// Calcule la durée de base d'un pas du séquenceur à partir du BPM.
// Le séquenceur fait 8 pas et le tempo est appliqué en millisecondes.
unsigned long getBaseStepDuration() {
  // Tempo du sequenceur : plus le BPM est haut, plus les pas vont vite.
  // Attention : le WAV garde sa vitesse normale ; on ne time-stretch pas le fichier.
  unsigned long baseStep = (60000UL / (unsigned long)bpm) / 2;

  if (baseStep < 90) baseStep = 90;
  if (baseStep > 900) baseStep = 900;

  return baseStep;
}

unsigned long getStepDuration(int stepIndex) {
  unsigned long baseStep = getBaseStepDuration();

// Swing = décalage des contretemps pour donner du groove.
// L'effet est limité pour garder une lecture stable.
  unsigned long swingOffset = (baseStep * (unsigned long)swingPercent * 45UL) / 10000UL;

  if (stepIndex % 2 == 0) {
    return baseStep + swingOffset;
  } else {
    if (baseStep > swingOffset + 60) return baseStep - swingOffset;
    return 60;
  }
}

void startSequencer() {
  if (isRecording) return;
  if (recPlaybackActive || wavRec.isPlaying()) return;

  if (!sdOK) {
    drawPlayScreen("NO SD");
    holdScreenUntil = millis() + 800;
    return;
  }

  // On coupe tous les sons de batterie avant de démarrer le séquenceur.
  stopAllDrumSounds();

  seqRunning = true;
  currentStep = 0;
  playingStep = -1;
  seqStepActive = false;
  seqStepHasSound = false;
  seqStepSound = -1;
  seqStepMinUntil = 0;

  screen = SCREEN_SEQ;
  forceRedraw = true;
}

void stopSequencer() {
  seqRunning = false;
  seqStepActive = false;
  seqStepHasSound = false;
  seqStepSound = -1;
  playingStep = -1;

  stopAllDrumSounds();
  updateLeds();
  forceRedraw = true;
}

void startSeqStep() {
  // Ordre strict : 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 7 -> 8
  playingStep = currentStep;

  int soundToPlay = sequenceStep[playingStep];
  seqStepSound = soundToPlay;
  seqStepMinUntil = millis() + getStepDuration(playingStep);
  seqStepActive = true;

  if (soundToPlay != -1) {
    bool ok = playSoundFile(soundToPlay);
    seqStepHasSound = ok;
  } else {
    stopAllDrumSounds();
    seqStepHasSound = false;
  }

  updateLeds();
  forceRedraw = true;
}

void finishSeqStep() {
  currentStep++;
  if (currentStep >= STEP_COUNT) currentStep = 0;

  playingStep = -1;
  seqStepActive = false;
  seqStepHasSound = false;
  seqStepSound = -1;

  updateLeds();
  forceRedraw = true;
}

// Contrôle le déroulement du séquenceur pas à pas.
void handleSequencer() {
  if (!seqRunning) return;
  if (isRecording) return;
  if (recPlaybackActive || wavRec.isPlaying()) return;

  unsigned long now = millis();

  if (!seqStepActive) {
    startSeqStep();
    return;
  }

  bool minTimeReached = ((long)(now - seqStepMinUntil) >= 0);

  // Pas vide : on respecte uniquement la duree tempo/swing.
  if (!seqStepHasSound) {
    if (minTimeReached) {
      finishSeqStep();
    }
    return;
  }

  // Pas avec son : on ne passe jamais au suivant tant que le son n'est pas fini.
  bool soundFinished = !isDrumSoundPlaying(seqStepSound);

  if (minTimeReached && soundFinished) {
    finishSeqStep();
  }
}

// ============================================================
// AFFICHAGE
// ============================================================

void drawMenu() {
  display.clearDisplay();
  drawHeader("MENU");

  int start = menuIndex - 2;
  if (start < 0) start = 0;
  if (start > MENU_COUNT - 5) start = MENU_COUNT - 5;

  for (int i = 0; i < 5; i++) {
    int idx = start + i;
    if (idx >= MENU_COUNT) break;

    int y = 15 + i * 8;

    if (idx == menuIndex) {
      display.fillRect(0, y - 1, 128, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(2, y);
      display.print("> ");
      display.print(menuItems[idx]);
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.setCursor(2, y);
      display.print("  ");
      display.print(menuItems[idx]);
    }
  }

  drawFooter("Clic=OK Long=Menu");
  display.display();
}

void drawSounds() {
  display.clearDisplay();
  drawHeader("SONS");

  for (int i = 0; i < SOUND_COUNT; i++) {
    int y = 16 + i * 9;

    if (i == selectedSound) {
      display.fillRect(0, y - 1, 128, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }

    display.setCursor(2, y);
    display.print(i == selectedSound ? "> " : "  ");
    display.print(soundNames[i]);
    display.setCursor(84, y);
    display.print(soundExists[i] ? "OK" : "--");
    display.setTextColor(SSD1306_WHITE);
  }

  drawFooter("Clic=jouer");
  display.display();
}

void drawSeq() {
  display.clearDisplay();
  drawHeader("SEQ 2x4");

  display.setCursor(0, 14);
  display.print("Son: ");
  display.print(getSeqSoundName(selectedSeqSound));

  display.setCursor(72, 14);
  display.print(tempoPercent);
  display.print("% ");
  display.print(swingPercent);
  display.print("%");

  display.setCursor(0, 27);
  display.print("L1: ");
  for (int i = 0; i < 4; i++) {
    if (i == playingStep && seqRunning) {
      display.print("[");
      display.print(getSeqLetter(sequenceStep[i]));
      display.print("]");
    } else {
      display.print(getSeqLetter(sequenceStep[i]));
      display.print(" ");
    }
  }

  display.setCursor(0, 39);
  display.print("L1: ");
  for (int i = 4; i < 8; i++) {
    if (i == playingStep && seqRunning) {
      display.print("[");
      display.print(getSeqLetter(sequenceStep[i]));
      display.print("]");
    } else {
      display.print(getSeqLetter(sequenceStep[i]));
      display.print(" ");
    }
  }

  drawFooter("PLAY=seq  REC=save pat");
  display.display();
}

void drawPatterns() {
  updatePatternPreview();

  display.clearDisplay();
  drawHeader("PATTERNS");

  char filename[13];
  makePatternFilename(selectedPattern, filename);

  display.setCursor(0, 14);
  display.print(filename);
  display.setCursor(74, 14);
  display.print(selectedPattern);
  display.print("/");
  display.print(MAX_PATTERNS);

  display.setCursor(0, 25);
  display.print("Etat: ");
  display.print(patternPreviewOK ? "EXISTE" : "VIDE");

  display.setCursor(0, 36);
  display.print("L1: ");
  for (int i = 0; i < 4; i++) {
    display.print(getSeqLetter(patternPreview[i]));
    display.print(" ");
  }

  display.setCursor(0, 46);
  display.print("L1: ");
  for (int i = 4; i < 8; i++) {
    display.print(getSeqLetter(patternPreview[i]));
    display.print(" ");
  }

  drawFooter("Clic=L REC=S STOP=D");
  display.display();
}

void drawRecord() {
  display.clearDisplay();
  drawHeader("ENREG");

  if (isRecording) {
    unsigned long sec = (millis() - recStartedAt) / 1000;
    display.setTextSize(2);
    display.setCursor(0, 15);
    display.print("REC ");
    display.print(sec);
    display.print("s");

    display.setTextSize(1);
    display.setCursor(0, 38);
    display.print("Mic ");
    display.print(micLevelPercent);
    display.print("%");
    drawBar(58, 37, 66, 10, micLevelPercent, 100);

    drawFooter("REC/STOP = fin");
  } else {
    display.setCursor(0, 18);
    display.print("REC = demarrer");
    display.setCursor(0, 32);
    display.print("Max ");
    display.print(MAX_REC_SECONDS);
    display.print(" secondes");
    display.setCursor(0, 44);
    display.print("Mic ");
    display.print(micLevelPercent);
    display.print("%");
    drawFooter("Long=Menu");
  }

  display.display();
}

void drawPlayRec() {
  display.clearDisplay();
  drawHeader("LIRE REC");

  if (recPlaybackActive || wavRec.isPlaying()) {
    display.setTextSize(2);
    display.setCursor(0, 15);
    display.print("PLAY");
    display.setTextSize(1);
    display.setCursor(0, 40);
    display.print(recPlaybackName);
    drawFooter("Lecture... STOP");
    display.display();
    return;
  }

  if (cachedRecCount <= 0) {
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("AUCUN");
    drawFooter("Aucun REC");
  } else {
    char filename[13];
    makeRecFilename(selectedRecIndex, filename);
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print("REC ");
    display.print(selectedRecIndex);
    display.setTextSize(1);
    display.setCursor(0, 42);
    display.print(filename);
    display.setCursor(80, 42);
    display.print("T:");
    display.print(cachedRecCount);
    drawFooter("Clic=lire Enc=changer");
  }

  display.display();
}

void drawDeleteRec() {
  display.clearDisplay();
  drawHeader("SUPPR REC");

  if (cachedRecCount <= 0) {
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("AUCUN");
    drawFooter("Rien a supprimer");
  } else {
    char filename[13];
    makeRecFilename(selectedRecIndex, filename);
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print("DEL ");
    display.print(selectedRecIndex);
    display.setTextSize(1);
    display.setCursor(0, 42);
    display.print(filename);
    drawFooter("Clic=supprimer");
  }

  display.display();
}

void drawMic() {
  display.clearDisplay();
  drawHeader("MIC");

  display.setCursor(0, 14);
  display.print("Niveau micro");
  display.setTextSize(2);
  display.setCursor(0, 25);
  display.print(micLevelPercent);
  display.print("%");
  display.setTextSize(1);
  drawBar(58, 29, 66, 10, micLevelPercent, 100);
  display.setCursor(0, 45);
  display.print("Gain:");
  display.print(micGainValue);
  display.print(" Filtre doux");
  drawFooter("REC pour tester");
  display.display();
}

void drawControls() {
  display.clearDisplay();
  drawHeader("REGLAGES");

  display.setCursor(0, 15);
  display.print("VOL ");
  display.print(volumePercent);
  display.print("%");
  drawBar(52, 14, 72, 8, volumePercent, 100);

  display.setCursor(0, 30);
  display.print("TEMPO ");
  display.print(tempoPercent);
  display.print("% ");
  display.print(bpm);
  display.print("BPM");

  display.setCursor(0, 43);
  display.print("SWING ");
  display.print(swingPercent);
  display.print("%");

  drawFooter("A0 VOL A1 TEMP A2 SW");
  display.display();
}

void drawAbout() {
  display.clearDisplay();
  drawHeader("INFOS");

  display.setTextSize(1);
  display.setCursor(0, 14);
  display.println("Mini Groovebox");
  display.println("Teensy 4.1 + Audio");
  display.println("SD WAV + REC + Seq");
  display.println("PLAY = sequenceur");
  display.println("A1 tempo / A2 swing");

  drawFooter("Menu Veille a la fin");
  display.display();
}

void drawSleep() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.print("MODE VEILLE");
  display.setCursor(0, 36);
  display.print("Appuie bouton");
  display.display();
}

void refreshScreen(bool force) {
  if (!oledOK) return;
  if (sleepMode) return;

  if (!force && millis() - lastScreenRefresh < 120) return;
  lastScreenRefresh = millis();

  if (popupType != 0 && millis() < popupUntil) {
    drawPopupScreen();
    return;
  }

  if (popupType != 0 && millis() >= popupUntil) {
    popupType = 0;
    forceRedraw = true;
  }

  if (!force && millis() < holdScreenUntil) return;

  if (!force && !forceRedraw &&
      screen != SCREEN_MIC &&
      screen != SCREEN_RECORD &&
      screen != SCREEN_PLAY_REC &&
      screen != SCREEN_CONTROLS &&
      screen != SCREEN_SEQ &&
      screen != SCREEN_PATTERNS) {
    return;
  }

  forceRedraw = false;

  if (screen == SCREEN_MENU) drawMenu();
  else if (screen == SCREEN_SOUNDS) drawSounds();
  else if (screen == SCREEN_SEQ) drawSeq();
  else if (screen == SCREEN_PATTERNS) drawPatterns();
  else if (screen == SCREEN_RECORD) drawRecord();
  else if (screen == SCREEN_PLAY_REC) drawPlayRec();
  else if (screen == SCREEN_DELETE_REC) drawDeleteRec();
  else if (screen == SCREEN_MIC) drawMic();
  else if (screen == SCREEN_CONTROLS) drawControls();
  else if (screen == SCREEN_ABOUT) drawAbout();
  else if (screen == SCREEN_SLEEP) drawSleep();
}

// ============================================================
// POTENTIOMETRES
// ============================================================

// Lit les potentiomètres et met à jour le volume, tempo et swing.
void readPots() {
  if (millis() - lastPotRead < 60) return;
  lastPotRead = millis();

  int rawV = analogRead(POT_VOLUME);
  int rawT = analogRead(POT_TEMPO);
  int rawS = analogRead(POT_SWING);

  int newVolume = map(rawV, 0, 1023, 0, 100);
  int newTempo  = map(rawT, 0, 1023, 0, 100);
  int newSwing  = map(rawS, 0, 1023, 0, 100);

  if (abs(newVolume - volumePercent) >= 2) {
    volumePercent = newVolume;
    volumeValue = volumePercent / 100.0;
    applyVolume();
    showPopup(1);
  }

  if (abs(newTempo - tempoPercent) >= 2) {
    tempoPercent = newTempo;
    bpm = map(tempoPercent, 0, 100, 40, 220);
    if (bpm < 40) bpm = 40;
    if (bpm > 220) bpm = 220;
    showPopup(2);
  }

  if (abs(newSwing - swingPercent) >= 2) {
    swingPercent = newSwing;
    showPopup(3);
  }
}

// ============================================================
// VEILLE
// ============================================================

void goToSleep() {
  stopSequencer();
  stopAllSounds();
  for (int i = 0; i < STEP_COUNT; i++) {
    if (ledPins[i] >= 0) digitalWrite(ledPins[i], LOW);
  }
  sleepMode = true;
  screen = SCREEN_SLEEP;
  if (oledOK) drawSleep();
}

bool anyButtonPressed() {
  if (digitalRead(BTN_KICK) == LOW) return true;
  if (digitalRead(BTN_SNARE) == LOW) return true;
  if (digitalRead(BTN_HIHAT) == LOW) return true;
  if (digitalRead(BTN_CLAP) == LOW) return true;
  if (digitalRead(BTN_PLAY) == LOW) return true;
  if (digitalRead(BTN_STOP) == LOW) return true;
  if (digitalRead(BTN_REC) == LOW) return true;
  if (digitalRead(ENC_SW) == LOW) return true;
  for (int i = 0; i < STEP_COUNT; i++) {
    if (digitalRead(stepPins[i]) == LOW) return true;
  }
  return false;
}

void handleSleep() {
  if (!sleepMode) return;
  if (anyButtonPressed()) {
    sleepMode = false;
    screen = SCREEN_MENU;
    forceRedraw = true;
    delay(250);
  }
}

// ============================================================
// BOUTONS DIRECTS
// ============================================================

// Gère les boutons de son direct et les boutons transport (play/stop/rec).
void handleDirectButtons() {
  if (digitalRead(BTN_KICK) == LOW && debounceButton()) {
    selectedSeqSound = 0;
    selectedSound = 0;
    triggerSound(0);
    forceRedraw = true;
  }

  if (digitalRead(BTN_SNARE) == LOW && debounceButton()) {
    selectedSeqSound = 1;
    selectedSound = 1;
    triggerSound(1);
    forceRedraw = true;
  }

  if (digitalRead(BTN_HIHAT) == LOW && debounceButton()) {
    selectedSeqSound = 2;
    selectedSound = 2;
    triggerSound(2);
    forceRedraw = true;
  }

  if (digitalRead(BTN_CLAP) == LOW && debounceButton()) {
    selectedSeqSound = 3;
    selectedSound = 3;
    triggerSound(3);
    forceRedraw = true;
  }

  if (digitalRead(BTN_REC) == LOW && debounceButton()) {
    if (screen == SCREEN_PATTERNS) {
      bool ok = savePatternSlot(selectedPattern);
      if (ok) showPopup(4);
      else {
        drawPlayScreen("SAVE ERR");
        holdScreenUntil = millis() + 800;
      }
      forceRedraw = true;
      return;
    }
    toggleRecordingButton();
  }

  if (digitalRead(BTN_PLAY) == LOW && debounceButton()) {
    if (isRecording) return;
    if (recPlaybackActive || wavRec.isPlaying()) return;

    if (screen == SCREEN_PATTERNS) {
      bool ok = loadPatternSlot(selectedPattern);
      if (ok) {
        showPopup(5);
        startSequencer();
      } else {
        drawPlayScreen("PAT VIDE");
        holdScreenUntil = millis() + 800;
      }
      forceRedraw = true;
      return;
    }

    if (seqRunning) stopSequencer();
    else startSequencer();
  }

  if (digitalRead(BTN_STOP) == LOW && debounceButton()) {
    if (screen == SCREEN_PATTERNS) {
      bool ok = deletePatternSlot(selectedPattern);
      if (ok) showPopup(6);
      else {
        drawPlayScreen("DEJA VIDE");
        holdScreenUntil = millis() + 800;
      }
      forceRedraw = true;
      return;
    }

    if (isRecording) {
      stopRecording();
      return;
    }

    if (recPlaybackActive || wavRec.isPlaying()) {
      wavRec.stop();
      recPlaybackActive = false;
      drawPlayScreen("STOP REC");
      holdScreenUntil = millis() + 700;
      forceRedraw = true;
      return;
    }

    if (seqRunning) {
      stopSequencer();
      drawPlayScreen("STOP");
      holdScreenUntil = millis() + 500;
      return;
    }

    stopAllSounds();
    drawPlayScreen("STOP");
    holdScreenUntil = millis() + 500;
  }
}

// ============================================================
// ENCODEUR
// ============================================================

int readEncoderClick() {
  static bool lastState = HIGH;
  static unsigned long pressStart = 0;
  static bool longSent = false;

  bool state = digitalRead(ENC_SW);

  if (lastState == HIGH && state == LOW) {
    pressStart = millis();
    longSent = false;
  }

  if (state == LOW && !longSent && millis() - pressStart > 800) {
    longSent = true;
    lastState = state;
    return CLICK_LONG;
  }

  if (lastState == LOW && state == HIGH) {
    unsigned long duration = millis() - pressStart;
    lastState = state;
    if (duration < 800 && !longSent) return CLICK_SHORT;
  }

  lastState = state;
  return CLICK_NONE;
}

void enterMenuItem() {
  if (menuIndex == 0) screen = SCREEN_SOUNDS;
  else if (menuIndex == 1) screen = SCREEN_SEQ;
  else if (menuIndex == 2) screen = SCREEN_PATTERNS;
  else if (menuIndex == 3) screen = SCREEN_RECORD;
  else if (menuIndex == 4) {
    if (!wavRec.isPlaying()) refreshRecordingCache();
    screen = SCREEN_PLAY_REC;
  }
  else if (menuIndex == 5) {
    if (!wavRec.isPlaying()) refreshRecordingCache();
    screen = SCREEN_DELETE_REC;
  }
  else if (menuIndex == 6) screen = SCREEN_MIC;
  else if (menuIndex == 7) screen = SCREEN_CONTROLS;
  else if (menuIndex == 8) screen = SCREEN_ABOUT;
  else if (menuIndex == 9) {
    goToSleep();
    return;
  }

  forceRedraw = true;
}

void handleEncoder() {
  if (recPlaybackActive || wavRec.isPlaying()) return;

  long pos = encoder.read() / 4;

  if (pos != lastEncPos) {
    if (pos > lastEncPos) {
      if (screen == SCREEN_MENU) {
        menuIndex++;
        if (menuIndex >= MENU_COUNT) menuIndex = 0;
      } else if (screen == SCREEN_SOUNDS) {
        selectedSound++;
        if (selectedSound >= SOUND_COUNT) selectedSound = 0;
        selectedSeqSound = selectedSound;
      } else if (screen == SCREEN_SEQ) {
        selectedSeqSound++;
        if (selectedSeqSound >= SOUND_COUNT) selectedSeqSound = 0;
        selectedSound = selectedSeqSound;
      } else if (screen == SCREEN_PATTERNS) {
        selectNextPattern(1);
      } else if (screen == SCREEN_PLAY_REC || screen == SCREEN_DELETE_REC) {
        selectNextRecording(1);
      }
    } else {
      if (screen == SCREEN_MENU) {
        menuIndex--;
        if (menuIndex < 0) menuIndex = MENU_COUNT - 1;
      } else if (screen == SCREEN_SOUNDS) {
        selectedSound--;
        if (selectedSound < 0) selectedSound = SOUND_COUNT - 1;
        selectedSeqSound = selectedSound;
      } else if (screen == SCREEN_SEQ) {
        selectedSeqSound--;
        if (selectedSeqSound < 0) selectedSeqSound = SOUND_COUNT - 1;
        selectedSound = selectedSeqSound;
      } else if (screen == SCREEN_PATTERNS) {
        selectNextPattern(-1);
      } else if (screen == SCREEN_PLAY_REC || screen == SCREEN_DELETE_REC) {
        selectNextRecording(-1);
      }
    }

    lastEncPos = pos;
    forceRedraw = true;
  }

  int click = readEncoderClick();

  if (click == CLICK_LONG) {
    if (!isRecording) {
      screen = SCREEN_MENU;
      forceRedraw = true;
    }
    return;
  }

  if (click == CLICK_SHORT) {
    if (screen == SCREEN_MENU) {
      enterMenuItem();
    }
    else if (screen == SCREEN_SOUNDS) {
      selectedSeqSound = selectedSound;
      triggerSound(selectedSound);
    }
    else if (screen == SCREEN_SEQ) {
      bool ok = savePatternSlot(selectedPattern);
      if (ok) showPopup(4);
      else {
        drawPlayScreen("SAVE ERR");
        holdScreenUntil = millis() + 800;
      }
      forceRedraw = true;
    }
    else if (screen == SCREEN_PATTERNS) {
      bool ok = loadPatternSlot(selectedPattern);
      if (ok) showPopup(5);
      else {
        drawPlayScreen("PAT VIDE");
        holdScreenUntil = millis() + 800;
      }
      forceRedraw = true;
    }
    else if (screen == SCREEN_RECORD) {
      toggleRecordingButton();
    }
    else if (screen == SCREEN_PLAY_REC) {
      playSelectedRecording();
    }
    else if (screen == SCREEN_DELETE_REC) {
      deleteSelectedRecording();
    }
    else if (screen == SCREEN_ABOUT) {
      screen = SCREEN_MENU;
      forceRedraw = true;
    }
    else {
      screen = SCREEN_MENU;
      forceRedraw = true;
    }
  }
}

// ============================================================
// DEMARRAGE
// ============================================================

void clearOled() {
  if (!oledOK) return;
  display.clearDisplay();
  display.display();
  delay(80);
}

void startupScreen() {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, 8);
  display.println("AFRO");
  display.setCursor(0, 28);
  display.println("BEATS");
  display.setTextSize(1);
  display.setCursor(70, 16);
  display.print("MINI");
  display.setCursor(70, 28);
  display.print("GROOVE");
  display.setCursor(70, 40);
  display.print("BOX");
  display.display();
  delay(900);
}

// ============================================================
// SETUP
// ============================================================
// La fonction setup() initialise tout le matériel et l'état du système.
// Elle prépare les boutons, le séquenceur, l'audio, l'écran OLED et la carte SD.
void setup() {
  Serial.begin(9600);
  delay(500);

  analogReadResolution(10);

  pinMode(BTN_KICK, INPUT_PULLUP);
  pinMode(BTN_SNARE, INPUT_PULLUP);
  pinMode(BTN_HIHAT, INPUT_PULLUP);
  pinMode(BTN_CLAP, INPUT_PULLUP);
  pinMode(BTN_PLAY, INPUT_PULLUP);
  pinMode(BTN_STOP, INPUT_PULLUP);
  pinMode(BTN_REC, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  setupStepButtons();
  setupLeds();

  for (int i = 0; i < STEP_COUNT; i++) {
    sequenceStep[i] = -1;
    patternPreview[i] = -1;
    if (ledPins[i] >= 0) digitalWrite(ledPins[i], LOW);
  }

  AudioMemory(240);

  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  clearOled();

  audioShield.enable();
  muteAudioOutput();
  audioShield.inputSelect(AUDIO_INPUT_MIC);
  audioShield.micGain(micGainValue);
  audioShield.lineOutLevel(20);

  applySoundGains();
  startupScreen();

  // Pas de calibration micro au demarrage.
  initSDCard();
  checkSoundFiles();
  refreshRecordingCache();
  loadFirstPatternAtStartup();

  // Lecture initiale des potentiometres pour partir avec les bonnes valeurs.
  int rawV = analogRead(POT_VOLUME);
  int rawT = analogRead(POT_TEMPO);
  int rawS = analogRead(POT_SWING);
  volumePercent = map(rawV, 0, 1023, 0, 100);
  tempoPercent = map(rawT, 0, 1023, 0, 100);
  swingPercent = map(rawS, 0, 1023, 0, 100);
  bpm = map(tempoPercent, 0, 100, 40, 220);
  volumeValue = volumePercent / 100.0;

  fadeInAudio();

  lastEncPos = encoder.read() / 4;
  screen = SCREEN_MENU;
  forceRedraw = true;
  updateLeds();

  Serial.println("===== MINI GROOVEBOX - VERSION SEQ AUDIO + POTS OK =====");
  Serial.println("PLAY = sequenceur ordre 1-8 + attente fin son.");
  Serial.println("A0 volume, A1 tempo, A2 swing.");
  Serial.println("LED3/LED4 desactivees pour eviter courts-circuits.");
  Serial.println("Tempo/Swing = placement des pas, pas pitch WAV.");

  refreshScreen(true);
}

// ============================================================
// LOOP
// ============================================================
// Boucle principale : elle exécute toutes les tâches en temps réel.
// On y gère le mode veille, l'audio, le séquenceur, les boutons et l'affichage.
void loop() {
  // Si le système est en veille, ne pas exécuter le reste des actions.
  if (sleepMode) {
    handleSleep();
    return;
  }

  // Met à jour le niveau micro et gère l'écriture des blocs d'enregistrement.
  updateMicLevel();
  serviceRecording();

  // Si l'enregistrement est actif, arrêter automatiquement au bout du temps maximum.
  if (isRecording && millis() - recStartedAt > (unsigned long)MAX_REC_SECONDS * 1000UL) {
    stopRecording();
  }

  servicePlaybackEnd();
  servicePatternSave();

  // Lecture des potentiomètres pour mettre à jour le volume, le tempo et le swing.
  readPots();

  // Gestion des entrées utilisateur : encodeur, boutons directs, pas et séquenceur.
  handleEncoder();
  handleDirectButtons();
  handleStepButtons();
  handleSequencer();

  updateLeds();
  refreshScreen(false);
}
