
/*
  ============================================================
  MINI GROOVEBOX - CODE COMPLET FINAL PROPRE
  Teensy 4.1 + Audio Shield SGTL5000 + OLED + SD + Encodeur
  + 3 potentiometres + 6 boutons + ampli / haut-parleur

  VERSION :
  - Carte SD connectee dans l'Audio Shield
  - Menu professionnel sur OLED
  - Encodeur pour naviguer
  - Sons WAV depuis SD si fichiers presents
  - Sons de synthese si fichiers WAV absents
  - Boutons physiques toujours actifs
  - Volume, BPM et Pitch par potentiometres
  - Micro affiche sur OLED

  ============================================================

  CONNEXIONS IMPORTANTES

  OLED I2C :
  GND -> GND
  VCC -> 3.3V
  SCL -> pin 19
  SDA -> pin 18

  Encodeur rotatif :
  CLK / A -> pin 28
  DT  / B -> pin 29
  SW      -> pin 30
  VCC     -> 3.3V
  GND     -> GND

  Boutons :
  Kick   -> pin 3  + GND
  Snare  -> pin 4  + GND
  Hi-Hat -> pin 5  + GND
  Clap   -> pin 6  + GND
  Play   -> pin 22 + GND
  Stop   -> pin 24 + GND

  Potentiometres :
  Volume -> A0
  BPM    -> A1
  Pitch  -> A2

  Audio Shield SD :
  CS   -> pin 10
  MOSI -> pin 11
  MISO -> pin 12
  SCK  -> pin 13

  Fichiers WAV a mettre a la racine de la carte SD :
  KICK.WAV
  SNARE.WAV
  HIHAT.WAV
  CLAP.WAV

  Format conseille :
  WAV PCM 16-bit, 44100 Hz, noms courts, majuscules, sans espace.

  Navigation menu :
  Tourner encodeur = defiler
  Clic court       = entrer / valider
  Clic long        = retour menu principal
*/

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <Encoder.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// CONSTANTES CLIC ENCODEUR
// ============================================================

#define CLICK_NONE  0
#define CLICK_SHORT 1
#define CLICK_LONG  2

// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledOK = false;

// ============================================================
// AUDIO
// ============================================================

// Lecture WAV
AudioPlaySdWav playWav;

// Son de secours genere par code
AudioSynthWaveform waveform1;
AudioEffectEnvelope envelope1;

// Mixage + sortie
AudioMixer4 mixer1;
AudioAmplifier masterAmp;
AudioOutputI2S audioOutput;

// Micro
AudioInputI2S audioInput;
AudioAnalyzePeak peakMic;

// Connexions audio
AudioConnection patchCord1(playWav, 0, mixer1, 0);
AudioConnection patchCord2(playWav, 1, mixer1, 1);
AudioConnection patchCord3(waveform1, envelope1);
AudioConnection patchCord4(envelope1, 0, mixer1, 2);
AudioConnection patchCord5(mixer1, 0, masterAmp, 0);
AudioConnection patchCord6(masterAmp, 0, audioOutput, 0);
AudioConnection patchCord7(masterAmp, 0, audioOutput, 1);
AudioConnection patchCordMic(audioInput, 0, peakMic, 0);

AudioControlSGTL5000 audioShield;

// ============================================================
// PINS
// ============================================================

const int BTN_KICK  = 3;
const int BTN_SNARE = 4;
const int BTN_HIHAT = 5;
const int BTN_CLAP  = 6;
const int BTN_PLAY  = 22;
const int BTN_STOP  = 24;

const int POT_VOLUME = A0;
const int POT_BPM    = A1;
const int POT_PITCH  = A2;

const int ENC_A  = 28;
const int ENC_B  = 29;
const int ENC_SW = 30;

Encoder encoder(ENC_A, ENC_B);

// ============================================================
// CARTE SD AUDIO SHIELD
// ============================================================

bool sdOK = false;

const int SDCARD_CS_PIN = 10;
const int SDCARD_MOSI_PIN = 11;
const int SDCARD_SCK_PIN = 13;

// ============================================================
// DONNEES SONS
// ============================================================

const int NB_SONS = 4;

const char* nomsSons[NB_SONS] = {
  "KICK",
  "SNARE",
  "HI-HAT",
  "CLAP"
};

const char* fichiersSons[NB_SONS] = {
  "KICK.WAV",
  "SNARE.WAV",
  "HIHAT.WAV",
  "CLAP.WAV"
};

// Frequences adaptees au petit haut-parleur 8 ohms 0.3W
float freqSons[NB_SONS] = {
  180.0,
  350.0,
  1200.0,
  700.0
};

int dureeSons[NB_SONS] = {
  150,
  130,
  80,
  120
};

float ampSons[NB_SONS] = {
  0.85,
  0.78,
  0.65,
  0.78
};

// ============================================================
// ECRANS
// ============================================================

#define SCREEN_HOME         0
#define SCREEN_MAIN_MENU    1
#define SCREEN_SOUND_SELECT 2
#define SCREEN_VOLUME       3
#define SCREEN_BPM          4
#define SCREEN_PITCH        5
#define SCREEN_MIC          6
#define SCREEN_SD           7
#define SCREEN_ABOUT        8
#define SCREEN_STATUS       9

int screen = SCREEN_HOME;

const int NB_MENU = 8;
const char* menuItems[NB_MENU] = {
  "Selection sons",
  "Volume",
  "Tempo BPM",
  "Pitch",
  "Microphone",
  "Carte SD",
  "Statut systeme",
  "Infos projet"
};

int menuIndex = 0;
int selectedSound = 0;

long oldEncoderPos = 0;

bool playMode = false;
unsigned long lastStepTime = 0;
unsigned long lastButtonTime = 0;
unsigned long lastScreenRefresh = 0;
unsigned long lastSerialStatus = 0;

int volumePercent = 60;
int bpmValue = 120;
int pitchPercent = 100;

float volumeValue = 0.60;
float pitchFactor = 1.00;

bool forceRedraw = true;

// ============================================================
// OUTILS BOUTONS
// ============================================================

bool debounceGlobal() {
  if (millis() - lastButtonTime > 220) {
    lastButtonTime = millis();
    return true;
  }
  return false;
}

int readEncoderButton() {
  static bool lastState = HIGH;
  static unsigned long pressStart = 0;
  static bool longAlreadySent = false;

  bool current = digitalRead(ENC_SW);

  if (lastState == HIGH && current == LOW) {
    pressStart = millis();
    longAlreadySent = false;
  }

  if (current == LOW && !longAlreadySent && millis() - pressStart > 750) {
    longAlreadySent = true;
    lastState = current;
    return CLICK_LONG;
  }

  if (lastState == LOW && current == HIGH) {
    unsigned long duration = millis() - pressStart;
    lastState = current;

    if (duration < 750 && !longAlreadySent) {
      return CLICK_SHORT;
    }
  }

  lastState = current;
  return CLICK_NONE;
}

// ============================================================
// AFFICHAGE OLED
// ============================================================

void drawHeader(const char* title) {
  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print(title);

  display.setCursor(78, 2);
  display.print(sdOK ? "SD" : "--");

  if (playMode) {
    display.setCursor(100, 2);
    display.print("RUN");
  }

  display.setTextColor(SSD1306_WHITE);
}

void drawFooter(const char* text) {
  display.drawLine(0, 54, 127, 54, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(text);
}

void drawHome() {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 3);
  display.println("MINI");

  display.setCursor(0, 22);
  display.println("GROOVEBOX");

  display.drawLine(0, 44, 127, 44, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 48);
  display.print("Teensy 4.1 + SGTL5000");

  display.setCursor(0, 58);
  display.print("OLED | SD | MIC | AMP");

  display.display();
}

void drawMainMenu() {
  if (!oledOK) return;

  display.clearDisplay();
  drawHeader("MENU PRINCIPAL");

  int start = menuIndex - 2;

  if (start < 0) start = 0;
  if (start > NB_MENU - 5) start = NB_MENU - 5;
  if (start < 0) start = 0;

  for (int i = 0; i < 5 && (start + i) < NB_MENU; i++) {
    int idx = start + i;
    int y = 15 + i * 8;

    if (idx == menuIndex) {
      display.fillRect(0, y - 1, 128, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(3, y);
      display.print("> ");
      display.print(menuItems[idx]);
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.setCursor(3, y);
      display.print("  ");
      display.print(menuItems[idx]);
    }
  }

  drawFooter("Tourner=defiler Clic=OK");
  display.display();
}

void drawSoundSelect() {
  if (!oledOK) return;

  display.clearDisplay();
  drawHeader("SELECTION SONS");

  for (int i = 0; i < NB_SONS; i++) {
    int y = 15 + i * 10;

    if (i == selectedSound) {
      display.fillRect(0, y - 1, 128, 9, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(3, y);
      display.print("> ");
      display.print(nomsSons[i]);
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.setCursor(3, y);
      display.print("  ");
      display.print(nomsSons[i]);
    }

    display.setCursor(78, y);

    if (sdOK && SD.exists(fichiersSons[i])) {
      display.print("WAV");
    } else {
      display.print("SYN");
    }
  }

  drawFooter("Clic=jouer Long=retour");
  display.display();
}

void drawValueScreen(const char* title, int value, const char* suffix, int minVal, int maxVal, const char* footer) {
  if (!oledOK) return;

  display.clearDisplay();
  drawHeader(title);

  display.setTextSize(2);
  display.setCursor(0, 21);
  display.print(value);
  display.print(suffix);

  display.drawRect(0, 43, 128, 9, SSD1306_WHITE);

  int w = map(value, minVal, maxVal, 0, 124);
  if (w < 0) w = 0;
  if (w > 124) w = 124;

  display.fillRect(2, 45, w, 5, SSD1306_WHITE);

  drawFooter(footer);
  display.display();
}

void drawMicro() {
  if (!oledOK) return;

  float mic = 0.0;

  if (peakMic.available()) {
    mic = peakMic.read();
  }

  int level = (int)(mic * 100);

  if (level > 100) level = 100;
  if (level < 0) level = 0;

  display.clearDisplay();
  drawHeader("MICROPHONE");

  display.setTextSize(1);
  display.setCursor(0, 18);
  display.print("Niveau entree audio");

  display.setTextSize(2);
  display.setCursor(0, 30);
  display.print(level);
  display.print("%");

  display.drawRect(0, 48, 128, 8, SSD1306_WHITE);
  display.fillRect(2, 50, map(level, 0, 100, 0, 124), 4, SSD1306_WHITE);

  drawFooter("Long=retour menu");
  display.display();
}

void drawSD() {
  if (!oledOK) return;

  display.clearDisplay();
  drawHeader("CARTE SD");

  display.setTextSize(1);
  display.setCursor(0, 15);

  if (sdOK) {
    display.println("Statut: detectee");

    for (int i = 0; i < NB_SONS; i++) {
      display.print(fichiersSons[i]);
      display.print(": ");
      display.println(SD.exists(fichiersSons[i]) ? "OK" : "absent");
    }
  } else {
    display.println("Statut: non detectee");
    display.println("");
    display.println("Mode synthese actif");
  }

  drawFooter("Long=retour menu");
  display.display();
}

void drawStatus() {
  if (!oledOK) return;

  display.clearDisplay();
  drawHeader("STATUT SYSTEME");

  display.setTextSize(1);
  display.setCursor(0, 15);

  display.print("Audio: ");
  display.println("SGTL5000");

  display.print("SD: ");
  display.println(sdOK ? "OK" : "ABSENTE");

  display.print("Vol: ");
  display.print(volumePercent);
  display.println("%");

  display.print("BPM: ");
  display.println(bpmValue);

  display.print("Pitch: ");
  display.print(pitchPercent);
  display.println("%");

  drawFooter("Long=retour menu");
  display.display();
}

void drawAbout() {
  if (!oledOK) return;

  display.clearDisplay();
  drawHeader("INFOS PROJET");

  display.setTextSize(1);
  display.setCursor(0, 15);
  display.println("Mini Groovebox");
  display.println("Jalon 3 - ISEN");
  display.println("Audio + OLED + SD");
  display.println("Pots + Encodeur + AMP");

  drawFooter("Long=retour menu");
  display.display();
}

void drawSoundPlayed(const char* sound, bool fromSD) {
  if (!oledOK) return;

  display.clearDisplay();

  display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, 2);
  display.print("LECTURE");

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 22);
  display.print(sound);

  display.setTextSize(1);
  display.setCursor(0, 45);
  display.print("Source: ");
  display.print(fromSD ? "WAV SD" : "Synthese");

  display.setCursor(0, 56);
  display.print("Vol ");
  display.print(volumePercent);
  display.print("% BPM ");
  display.print(bpmValue);

  display.display();
}

void refreshScreen() {
  if (!forceRedraw && millis() - lastScreenRefresh < 250) {
    return;
  }

  lastScreenRefresh = millis();

  if (!forceRedraw && screen != SCREEN_MIC) {
    return;
  }

  forceRedraw = false;

  switch (screen) {
    case SCREEN_HOME:
      drawHome();
      break;

    case SCREEN_MAIN_MENU:
      drawMainMenu();
      break;

    case SCREEN_SOUND_SELECT:
      drawSoundSelect();
      break;

    case SCREEN_VOLUME:
      drawValueScreen("VOLUME", volumePercent, "%", 0, 100, "Pot A0 | Long=retour");
      break;

    case SCREEN_BPM:
      drawValueScreen("TEMPO", bpmValue, " BPM", 60, 180, "Pot A1 | Long=retour");
      break;

    case SCREEN_PITCH:
      drawValueScreen("PITCH", pitchPercent, "%", 80, 130, "Pot A2 | Long=retour");
      break;

    case SCREEN_MIC:
      drawMicro();
      break;

    case SCREEN_SD:
      drawSD();
      break;

    case SCREEN_STATUS:
      drawStatus();
      break;

    case SCREEN_ABOUT:
      drawAbout();
      break;
  }
}

// ============================================================
// LECTURE POTENTIOMETRES
// ============================================================

void readPots() {
  static unsigned long lastRead = 0;

  if (millis() - lastRead < 80) {
    return;
  }

  lastRead = millis();

  int rawVol = analogRead(POT_VOLUME);
  int rawBpm = analogRead(POT_BPM);
  int rawPitch = analogRead(POT_PITCH);

  int newVol = map(rawVol, 0, 1023, 0, 90);
  int newBpm = map(rawBpm, 0, 1023, 60, 180);
  int newPitch = map(rawPitch, 0, 1023, 80, 130);

  bool changed = false;

  if (abs(newVol - volumePercent) >= 2) {
    volumePercent = newVol;
    volumeValue = volumePercent / 100.0;

    audioShield.volume(volumeValue);
    masterAmp.gain(max(0.05f, volumeValue * 1.6f));

    changed = true;
  }

  if (abs(newBpm - bpmValue) >= 2) {
    bpmValue = newBpm;
    changed = true;
  }

  if (abs(newPitch - pitchPercent) >= 2) {
    pitchPercent = newPitch;
    pitchFactor = pitchPercent / 100.0;
    changed = true;
  }

  if (changed) {
    if (screen == SCREEN_VOLUME || screen == SCREEN_BPM || screen == SCREEN_PITCH || screen == SCREEN_STATUS) {
      forceRedraw = true;
    }
  }
}

// ============================================================
// INITIALISATION SD
// ============================================================

void initSDCard() {
  sdOK = false;

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);

  Serial.println("Initialisation carte SD Audio Shield...");

  if (SD.begin(SDCARD_CS_PIN)) {
    sdOK = true;
    Serial.println("SD Audio Shield OK.");
    return;
  }

#ifdef BUILTIN_SDCARD
  if (SD.begin(BUILTIN_SDCARD)) {
    sdOK = true;
    Serial.println("SD Teensy OK.");
    return;
  }
#endif

  Serial.println("SD absente ou non lisible. Mode synthese actif.");
}

// ============================================================
// AUDIO
// ============================================================

void playSynthSound(int index) {
  float f = freqSons[index] * pitchFactor;

  waveform1.frequency(f);
  waveform1.amplitude(ampSons[index]);

  envelope1.noteOn();
  delay(dureeSons[index]);
  envelope1.noteOff();
}

void playSound(int index) {
  if (index < 0 || index >= NB_SONS) {
    return;
  }

  selectedSound = index;

  bool fromSD = false;

  Serial.print("Lecture: ");
  Serial.println(nomsSons[index]);

  if (sdOK && SD.exists(fichiersSons[index])) {
    playWav.stop();
    delay(5);

    playWav.play(fichiersSons[index]);
    fromSD = true;

    Serial.print("Source SD: ");
    Serial.println(fichiersSons[index]);
  } else {
    playSynthSound(index);
    fromSD = false;

    Serial.println("Source synthese.");
  }

  drawSoundPlayed(nomsSons[index], fromSD);

  delay(120);
  forceRedraw = true;
}

// ============================================================
// MENU
// ============================================================

void enterMenuItem() {
  switch (menuIndex) {
    case 0:
      screen = SCREEN_SOUND_SELECT;
      break;

    case 1:
      screen = SCREEN_VOLUME;
      break;

    case 2:
      screen = SCREEN_BPM;
      break;

    case 3:
      screen = SCREEN_PITCH;
      break;

    case 4:
      screen = SCREEN_MIC;
      break;

    case 5:
      screen = SCREEN_SD;
      break;

    case 6:
      screen = SCREEN_STATUS;
      break;

    case 7:
      screen = SCREEN_ABOUT;
      break;
  }

  forceRedraw = true;
}

void goBackToMenu() {
  screen = SCREEN_MAIN_MENU;
  forceRedraw = true;
}

void handleEncoder() {
  long pos = encoder.read() / 4;

  if (pos != oldEncoderPos) {
    long diff = pos - oldEncoderPos;
    oldEncoderPos = pos;

    if (diff > 0) {
      if (screen == SCREEN_MAIN_MENU) {
        menuIndex++;

        if (menuIndex >= NB_MENU) {
          menuIndex = 0;
        }
      } else if (screen == SCREEN_SOUND_SELECT) {
        selectedSound++;

        if (selectedSound >= NB_SONS) {
          selectedSound = 0;
        }
      }
    } else {
      if (screen == SCREEN_MAIN_MENU) {
        menuIndex--;

        if (menuIndex < 0) {
          menuIndex = NB_MENU - 1;
        }
      } else if (screen == SCREEN_SOUND_SELECT) {
        selectedSound--;

        if (selectedSound < 0) {
          selectedSound = NB_SONS - 1;
        }
      }
    }

    forceRedraw = true;
  }

  int click = readEncoderButton();

  if (click == CLICK_LONG) {
    if (screen != SCREEN_MAIN_MENU) {
      goBackToMenu();
    }

    return;
  }

  if (click == CLICK_SHORT) {
    if (screen == SCREEN_HOME) {
      screen = SCREEN_MAIN_MENU;
      forceRedraw = true;
    } else if (screen == SCREEN_MAIN_MENU) {
      enterMenuItem();
    } else if (screen == SCREEN_SOUND_SELECT) {
      playSound(selectedSound);
    } else {
      goBackToMenu();
    }
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(9600);
  delay(800);

  analogReadResolution(10);

  pinMode(BTN_KICK, INPUT_PULLUP);
  pinMode(BTN_SNARE, INPUT_PULLUP);
  pinMode(BTN_HIHAT, INPUT_PULLUP);
  pinMode(BTN_CLAP, INPUT_PULLUP);
  pinMode(BTN_PLAY, INPUT_PULLUP);
  pinMode(BTN_STOP, INPUT_PULLUP);

  pinMode(ENC_SW, INPUT_PULLUP);

  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    oledOK = true;
    Serial.println("OLED OK.");
  } else {
    oledOK = false;
    Serial.println("OLED non detecte.");
  }

  AudioMemory(70);

  if (audioShield.enable()) {
    Serial.println("Audio Shield SGTL5000 OK.");
  } else {
    Serial.println("Audio Shield non detecte.");
  }

  audioShield.inputSelect(AUDIO_INPUT_MIC);
  audioShield.micGain(30);

  audioShield.volume(0.60);
  audioShield.lineOutLevel(13);

  mixer1.gain(0, 0.8);
  mixer1.gain(1, 0.8);
  mixer1.gain(2, 0.9);

  masterAmp.gain(0.9);

  waveform1.begin(WAVEFORM_SINE);
  waveform1.frequency(100);
  waveform1.amplitude(0.7);

  envelope1.attack(3);
  envelope1.hold(35);
  envelope1.decay(80);
  envelope1.sustain(0.2);
  envelope1.release(100);

  initSDCard();

  oldEncoderPos = encoder.read() / 4;

  screen = SCREEN_HOME;
  forceRedraw = true;
  refreshScreen();

  delay(1700);

  screen = SCREEN_MAIN_MENU;
  forceRedraw = true;

  Serial.println("Mini Groovebox pret.");
  Serial.println("Tourner encodeur = defiler.");
  Serial.println("Clic court = entrer / valider.");
  Serial.println("Clic long = retour menu.");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  readPots();
  handleEncoder();

  if (digitalRead(BTN_KICK) == LOW && debounceGlobal()) {
    playSound(0);
  }

  if (digitalRead(BTN_SNARE) == LOW && debounceGlobal()) {
    playSound(1);
  }

  if (digitalRead(BTN_HIHAT) == LOW && debounceGlobal()) {
    playSound(2);
  }

  if (digitalRead(BTN_CLAP) == LOW && debounceGlobal()) {
    playSound(3);
  }

  if (digitalRead(BTN_PLAY) == LOW && debounceGlobal()) {
    playMode = true;
    Serial.println("PLAY actif.");
    forceRedraw = true;
  }

  if (digitalRead(BTN_STOP) == LOW && debounceGlobal()) {
    playMode = false;
    playWav.stop();
    envelope1.noteOff();

    Serial.println("STOP.");
    forceRedraw = true;
  }

  if (playMode) {
    unsigned long intervalMs = 60000UL / bpmValue;

    if (millis() - lastStepTime >= intervalMs) {
      lastStepTime = millis();
      playSound(selectedSound);
    }
  }

  if (millis() - lastSerialStatus > 3000) {
    lastSerialStatus = millis();

    Serial.print("Etat | SD=");
    Serial.print(sdOK ? "OK" : "NO");
    Serial.print(" | Son=");
    Serial.print(nomsSons[selectedSound]);
    Serial.print(" | Vol=");
    Serial.print(volumePercent);
    Serial.print("% | BPM=");
    Serial.print(bpmValue);
    Serial.print(" | Pitch=");
    Serial.print(pitchPercent);
    Serial.println("%");
  }

  refreshScreen();
}
