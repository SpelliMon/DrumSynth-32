#define MOZZI_AUDIO_MODE MOZZI_OUTPUT_INTERNAL_DAC
#define MOZZI_CONTROL_RATE 64
#include <Mozzi.h>
#include <Oscil.h>
#include <tables/triangle2048_int8.h>
#include <tables/square_no_alias_2048_int8.h>
#include <TFT_eSPI.h>

// ─── TFT ──────────────────────────────────────────────────────────────────────
TFT_eSPI tft = TFT_eSPI();
#define COL_BG       TFT_BLACK
#define COL_INACTIVE 0x2104
#define COL_PLAYING  TFT_WHITE
#define COL_CURSOR   TFT_YELLOW
#define COL_TEXT     TFT_WHITE
#define COL_KICK     0x07E0
#define COL_SNARE    0x001F
#define COL_HIHAT    0xF800

// ─── PINS ─────────────────────────────────────────────────────────────────────
#define PIN_PLAY      22
#define PIN_TOGGLE    14
#define PIN_UP        27
#define PIN_DOWN      26
#define PIN_LEFT      33
#define PIN_RIGHT     32
#define PIN_FREEZE    21
#define POT_TEMPO     34

// ─── OSCILLATORS ──────────────────────────────────────────────────────────────
Oscil<TRIANGLE2048_NUM_CELLS, MOZZI_AUDIO_RATE> kickOsc(TRIANGLE2048_DATA);
Oscil<SQUARE_NO_ALIAS_2048_NUM_CELLS, MOZZI_AUDIO_RATE> snareOsc(SQUARE_NO_ALIAS_2048_DATA);

// ─── SEQUENCER STATE ──────────────────────────────────────────────────────────
bool pattern[3][16] = {
  {1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0},
  {0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0},
  {1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0}
};

int bpm          = 120;
int previewBpm   = 120;
bool bpmEditMode = false;
int ticksPerStep;
int tickCount    = 0;
int currentStep  = 0;
bool playing     = false;
int editRow      = 0;
int editStep     = 0;

// ─── DISPLAY FLAGS ────────────────────────────────────────────────────────────
bool needStepRedraw   = false;
bool needHeaderRedraw = false;
bool needFullRedraw   = false;
bool needCursorRedraw = false;
bool needToggleRedraw = false;
int  redrawOldStep    = 0;
int  redrawNewStep    = 0;
int  cursorOldRow     = 0;
int  cursorOldStep    = 0;

// ─── DRUM STATE ───────────────────────────────────────────────────────────────
int kickAmp = 0, snareAmp = 0, hihatAmp = 0;
int kickTransient = 0, snareTonal = 0;
uint16_t lfsrSnare = 1, lfsrHihat = 1;
float smoothPreview = 120;

int8_t nextSnareNoise() {
  lfsrSnare = (lfsrSnare >> 1) ^ (-(lfsrSnare & 1u) & 0xB400u);
  return (int8_t)(lfsrSnare & 0xFF);
}
int8_t nextHihatNoise() {
  for (int i = 0; i < 4; i++)
    lfsrHihat = (lfsrHihat >> 1) ^ (-(lfsrHihat & 1u) & 0xB400u);
  return (int8_t)(lfsrHihat & 0xFF);
}

// ─── DISPLAY ──────────────────────────────────────────────────────────────────
#define STEP_W   14
#define STEP_H   40
#define STEP_GAP  2
#define GRID_X   10
#define GRID_Y   100

const char* rowNames[] = {"KICK", "SNARE", "HIHAT"};
uint16_t rowColors[]   = {COL_KICK, COL_SNARE, COL_HIHAT};

void drawStep(int row, int step) {
  int x = GRID_X + step * (STEP_W + STEP_GAP);
  int y = GRID_Y + row * (STEP_H + STEP_GAP);

  bool active = pattern[row][step];
  bool isEdit = (row == editRow && step == editStep);
  bool isPlay = (step == currentStep && playing);

  uint16_t color = active ? rowColors[row] : COL_INACTIVE;
  if (isPlay) color = COL_PLAYING;

  tft.fillRect(x, y, STEP_W, STEP_H, color);

  if (isEdit) {
    tft.drawRect(x,   y,   STEP_W,   STEP_H,   COL_CURSOR);
    tft.drawRect(x+1, y+1, STEP_W-2, STEP_H-2, COL_CURSOR);
  }
}

void drawHeader() {
  tft.fillRect(0, 40, 320, 55, COL_BG);

  tft.setTextSize(2);
  tft.setCursor(10, 45);

  if (bpmEditMode) {
    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.print("NEW: ");
    tft.print(previewBpm);
    tft.print("   ");
    tft.setTextSize(1);
    tft.setCursor(10, 68);
    tft.setTextColor(TFT_YELLOW, COL_BG);
    tft.print("press freeze to confirm");
    tft.setTextColor(COL_TEXT, COL_BG);
  } else {
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.print("BPM: ");
    tft.print(bpm);
    tft.print("   ");
    tft.fillRect(0, 65, 320, 12, COL_BG);
  }

  tft.setTextSize(2);
  tft.setCursor(230, 45);
  if (playing) {
    tft.setTextColor(TFT_GREEN, COL_BG);
    tft.print("PLAY");
  } else {
    tft.setTextColor(TFT_RED, COL_BG);
    tft.print("STOP");
  }
  tft.setTextColor(COL_TEXT, COL_BG);
}

void fullRedraw() {
  tft.fillScreen(COL_BG);
  tft.setTextColor(TFT_CYAN, COL_BG);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("DrumSynth 32");

  drawHeader();

  for (int r = 0; r < 3; r++)
    for (int s = 0; s < 16; s++)
      drawStep(r, s);
}

// ─── BUTTON STATE ─────────────────────────────────────────────────────────────
bool prevPlay   = false;
bool prevToggle = false;
bool prevUp     = false;
bool prevDown   = false;
bool prevLeft   = false;
bool prevRight  = false;
bool prevFreeze = false;

// ─── SETUP ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // play is now a regular push button — INPUT_PULLUP, LOW when pressed
  pinMode(PIN_PLAY,   INPUT_PULLUP);
  pinMode(PIN_TOGGLE, INPUT_PULLUP);
  pinMode(PIN_UP,     INPUT_PULLUP);
  pinMode(PIN_DOWN,   INPUT_PULLUP);
  pinMode(PIN_LEFT,   INPUT_PULLUP);
  pinMode(PIN_RIGHT,  INPUT_PULLUP);
  pinMode(PIN_FREEZE, INPUT_PULLUP);

  tft.init();
  tft.setRotation(3);
  tft.invertDisplay(false);
  tft.fillScreen(COL_BG);
  fullRedraw();

  ticksPerStep = (MOZZI_CONTROL_RATE * 60) / (bpm * 4);
  startMozzi(MOZZI_CONTROL_RATE);
}

// ─── CONTROL UPDATE ───────────────────────────────────────────────────────────
void updateControl() {

  // ── Tempo ──────────────────────────────────────────────────────────────────
  bool freezeBtn = digitalRead(PIN_FREEZE) == LOW;
  if (freezeBtn && !prevFreeze) {
    if (!bpmEditMode) {
      smoothPreview = map(analogRead(POT_TEMPO), 0, 4095, 60, 200);
      previewBpm    = (int)smoothPreview;
      bpmEditMode   = true;
      needHeaderRedraw = true;
    } else {
      bpm = previewBpm;
      ticksPerStep = (MOZZI_CONTROL_RATE * 60) / (bpm * 4);
      if (ticksPerStep < 1) ticksPerStep = 1;
      bpmEditMode = false;
      needHeaderRedraw = true;
    }
  }
  prevFreeze = freezeBtn;

  if (bpmEditMode) {
    float raw = (float)map(analogRead(POT_TEMPO), 0, 4095, 60, 200);
    smoothPreview += (raw - smoothPreview) * 0.05f;
    int newPreview = (int)smoothPreview;
    if (newPreview != previewBpm) {
      previewBpm = newPreview;
      needHeaderRedraw = true;
    }
  }

  // ── Play/stop — now push button, LOW when pressed ─────────────────────────
  bool playBtn = digitalRead(PIN_PLAY) == LOW;
  if (playBtn && !prevPlay) {
    playing = !playing;
    if (!playing) {
      redrawOldStep  = currentStep;
      redrawNewStep  = 0;
      currentStep    = 0;
      needStepRedraw = true;
    }
    needHeaderRedraw = true;
  }
  prevPlay = playBtn;

  // ── Navigation ─────────────────────────────────────────────────────────────
  bool upBtn    = digitalRead(PIN_UP)    == LOW;
  bool downBtn  = digitalRead(PIN_DOWN)  == LOW;
  bool leftBtn  = digitalRead(PIN_LEFT)  == LOW;
  bool rightBtn = digitalRead(PIN_RIGHT) == LOW;

  if (upBtn && !prevUp) {
    cursorOldRow  = editRow;
    cursorOldStep = editStep;
    editRow = (editRow - 1 + 3) % 3;
    needCursorRedraw = true;
  }
  if (downBtn && !prevDown) {
    cursorOldRow  = editRow;
    cursorOldStep = editStep;
    editRow = (editRow + 1) % 3;
    needCursorRedraw = true;
  }
  if (leftBtn && !prevLeft) {
    cursorOldRow  = editRow;
    cursorOldStep = editStep;
    editStep = (editStep - 1 + 16) % 16;
    needCursorRedraw = true;
  }
  if (rightBtn && !prevRight) {
    cursorOldRow  = editRow;
    cursorOldStep = editStep;
    editStep = (editStep + 1) % 16;
    needCursorRedraw = true;
  }

  prevUp    = upBtn;
  prevDown  = downBtn;
  prevLeft  = leftBtn;
  prevRight = rightBtn;

  // ── Toggle ─────────────────────────────────────────────────────────────────
  bool toggleBtn = digitalRead(PIN_TOGGLE) == LOW;
  if (toggleBtn && !prevToggle) {
    pattern[editRow][editStep] = !pattern[editRow][editStep];
    needToggleRedraw = true;
  }
  prevToggle = toggleBtn;

  // ── Sequencer ──────────────────────────────────────────────────────────────
  if (playing) {
    tickCount++;
    if (tickCount >= ticksPerStep) {
      tickCount = 0;

      int stepBeforeAdvance = currentStep;

      if (pattern[0][currentStep]) {
        kickOsc.setFreq(92.67f);   // updated frequency
        kickAmp = 255;
        kickTransient = 3;
      }
      if (pattern[1][currentStep]) {
        snareAmp   = 255;
        snareTonal = 5;
        snareOsc.setFreq(200.f);
      }
      if (pattern[2][currentStep]) {
        hihatAmp = 180;
      }

      currentStep    = (currentStep + 1) % 16;
      redrawOldStep  = stepBeforeAdvance;
      redrawNewStep  = currentStep;
      needStepRedraw = true;
    }
  }

  // ── Decay ──────────────────────────────────────────────────────────────────
  if (kickTransient > 0) kickTransient--;
  if (snareTonal    > 0) snareTonal--;
  if (kickAmp  > 0) { kickAmp  -= 20; if (kickAmp  < 0) kickAmp  = 0; }
  if (snareAmp > 0) { snareAmp -= 11; if (snareAmp < 0) snareAmp = 0; } // updated decay
  if (hihatAmp > 0) { hihatAmp -= 40; if (hihatAmp < 0) hihatAmp = 0; }
}

// ─── AUDIO ────────────────────────────────────────────────────────────────────
AudioOutput updateAudio() {
  int kick = 0, snare = 0, hihat = 0;

  if (kickAmp > 0) {
    kick = (kickOsc.next() * kickAmp) >> 6;
    if (kickTransient > 0)
      kick += ((rand() % 256 - 128) * 200) >> 6;
  }
  if (snareAmp > 0) {
    if (snareTonal > 0) snare = (snareOsc.next() * snareAmp) >> 7;
    else                snare = (nextSnareNoise() * snareAmp) >> 8;
  }
  if (hihatAmp > 0)
    hihat = (nextHihatNoise() * hihatAmp) >> 9;

  int mix = kick + snare + hihat;
  return MonoOutput::from8Bit(constrain(mix, -128, 127));
}

// ─── LOOP — all display work here ─────────────────────────────────────────────
void loop() {
  audioHook();

  if (needFullRedraw) {
    fullRedraw();
    needFullRedraw   = false;
    needHeaderRedraw = false;
    needStepRedraw   = false;
    needCursorRedraw = false;
    needToggleRedraw = false;
    return;
  }

  if (needHeaderRedraw) {
    drawHeader();
    needHeaderRedraw = false;
  }

  if (needStepRedraw) {
    for (int r = 0; r < 3; r++) {
      drawStep(r, redrawOldStep);
      drawStep(r, redrawNewStep);
    }
    needStepRedraw = false;
  }

  if (needCursorRedraw) {
    drawStep(cursorOldRow,  cursorOldStep);
    drawStep(editRow, editStep);
    needCursorRedraw = false;
  }

  if (needToggleRedraw) {
    drawStep(editRow, editStep);
    needToggleRedraw = false;
  }
}