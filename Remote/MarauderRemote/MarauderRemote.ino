#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= КЛАВИАТУРА =================
#define PCF8574_ADDR 0x27
#define KP_SDA 25
#define KP_SCL 26

char keyMap[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// ================= ДИСПЛЕЙ =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire1, OLED_RESET);

// ================= UART =================
#define MARAUDER_RX 16
#define MARAUDER_TX 17

// ================= ЭКРАНЫ =================
enum Screen { MAIN, WIFI, WIFI_ATTACK, BT, SNIFF, SETTINGS, SCR_STATUS, HELP, LIST, SAVED };
Screen currentScreen = MAIN;
int cursor = 0;

const char* mainMenu[]  = {"WiFi", "Bluetooth", "Sniffer", "Settings"};
const char* wifiMenu[]  = {"Scan WiFi", "Attack Sel", "Deauth Sel", "Beacon Sel", "Probe Sel", "Stop"};
const char* attackMenu[] = {
  "Deauth", "Beacon Spam", "Probe Flood", "Rickroll",
  "Random SSID", "Clone AP", "All Attacks", "Back"
};
const int attackCount = 8;
const char* btMenu[]    = {"BLE Scan", "BT Spam", "Stop"};
const char* sniffMenu[] = {"Beacon", "PMKID", "Raw", "Stop"};
const char* setMenu[]   = {"Help", "Saved", "Reboot", "Info", "Stop All", "Exit"};

const int mainCount  = 4;
const int wifiCount  = 6;
const int btCount    = 3;
const int sniffCount = 4;
const int setCount   = 6;

String statusLine1 = "";
String statusLine2 = "";
String lastCmd = "";

Screen returnScreen = MAIN;
int returnCursor = 0;
int returnScroll = 0;
String activeAttack = "";

const int MENU_VISIBLE = 5;
int menuScroll = 0;
int helpScroll = 0;
int listScroll = 0;

const char* helpLines[] = {
  "=== KEYS ===",
  "A - Up / Scroll+",
  "B - Down / Scroll-",
  "C - Select / Toggle",
  "D - Back / Exit",
  "# - STOP",
  "* - LIST (scan)",
  "9 - SAVED list",
  "",
  "=== Attack Types ===",
  "Deauth", "Beacon Spam", "Probe Flood",
  "Rickroll", "Random SSID", "Clone AP",
  "All Attacks"
};
const int helpLineCount = sizeof(helpLines) / sizeof(helpLines[0]);
const int HELP_VISIBLE  = 5;

// ================= БУФЕРЫ =================
#define LIST_MAX_LINES  40
#define LIST_LINE_LEN   26
#define LIST_VISIBLE    5

char listAPBuffer[LIST_MAX_LINES][LIST_LINE_LEN];
int  listAPCount = 0;
char listBLEBuffer[LIST_MAX_LINES][LIST_LINE_LEN];
int  listBLECount = 0;

char captureBuffer[LIST_MAX_LINES][LIST_LINE_LEN];
int  captureCount = 0;

int listType = 0;

bool capturing = false;
bool silentCapture = false;

int scanPhase = 0;
unsigned long scanPhaseStart = 0;
unsigned long lastDisplayUpdate = 0;

unsigned long lastByteTime = 0;
unsigned long lastRefresh = 0;         // ← для автообновления
unsigned long lastUserAction = 0;      // ← для паузы
const unsigned long CAPTURE_TIMEOUT = 1200;
const unsigned long REFRESH_INTERVAL = 8000;   // ← 8 сек между обновлениями
const unsigned long USER_PAUSE = 3000;         // ← пауза 3 сек после нажатия

const unsigned long SCAN_TIME = 30000;
const unsigned long STOP_WAIT = 500;
const unsigned long LIST_MIN_WAIT = 1500;
const unsigned long LIST_MAX_WAIT = 4000;

// ================= PCF8574 =================
void pcfWrite(uint8_t data) {
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(data);
  Wire.endTransmission();
}
uint8_t pcfRead() {
  Wire.requestFrom((uint8_t)PCF8574_ADDR, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0xFF;
}
char scanKeypad() {
  for (byte row = 0; row < 4; row++) {
    uint8_t out = 0xFF;
    out &= ~(1 << row);
    pcfWrite(out);
    delayMicroseconds(50);
    uint8_t in = pcfRead();
    for (byte col = 0; col < 4; col++) {
      if (!(in & (1 << (col + 4)))) {
        while (true) {
          pcfWrite(out);
          delayMicroseconds(50);
          uint8_t check = pcfRead();
          if (check & (1 << (col + 4))) break;
          delay(10);
        }
        delay(30);
        return keyMap[row][col];
      }
    }
  }
  return 0;
}

// ================= ФИЛЬТР =================
bool isNoiseLine(const char* line) {
  if (line[0] == '\0') return true;
  if (strstr(line, "ets Jul")) return true;
  if (strstr(line, "rst:0x")) return true;
  if (strstr(line, "load:0x")) return true;
  if (strstr(line, "entry 0x")) return true;
  if (strstr(line, "configsip:")) return true;
  if (strstr(line, "clk_drv:")) return true;
  if (strstr(line, "mode:DIO")) return true;
  if (strstr(line, "boot:0x")) return true;
  if (strstr(line, "SPIWP:")) return true;
  if (strstr(line, "ESP-IDF version")) return true;
  return false;
}

bool isListAPLine(const char* line) {
  if (line[0] != '[') return false;
  if (strstr(line, "][CH:") == nullptr) return false;
  return true;
}

bool isListBLELine(const char* line) {
  if (line[0] != '[') return false;
  if (strstr(line, "][RSSI:") == nullptr) return false;
  return true;
}

// ================= ЗАГОЛОВОК =================
void printTitle(const char* title) {
  display.print("[");
  display.print(title);
  if (activeAttack.length() > 0) display.print(" *");
  display.print("]");
}

// ================= ОТРИСОВКА =================
void drawMenu(const char* title, const char** items, int count) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  printTitle(title);
  display.println();
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  for (int i = 0; i < MENU_VISIBLE; i++) {
    int idx = menuScroll + i;
    if (idx >= count) break;
    display.setCursor(0, 14 + i * 10);
    display.print(idx == cursor ? "> " : "  ");
    display.println(items[idx]);
  }
  if (menuScroll > 0) { display.setCursor(122, 12); display.print("^"); }
  if (menuScroll + MENU_VISIBLE < count) { display.setCursor(122, 56); display.print("v"); }
  display.display();
}

void drawStatus() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("[STATUS]");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  display.setCursor(0, 16);
  display.println(statusLine1);
  display.setCursor(0, 28);
  display.println(statusLine2);
  display.drawLine(0, 42, 127, 42, SSD1306_WHITE);
  display.setCursor(0, 48);
  display.print("CMD: ");
  display.println(lastCmd);
  display.setCursor(0, 58);
  display.print("D=back  #=stop");
  display.display();
}

void drawHelp() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("[HELP ");
  display.print(helpScroll + 1);
  display.print("/");
  display.print(helpLineCount - HELP_VISIBLE + 1);
  display.println("]");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  for (int i = 0; i < HELP_VISIBLE; i++) {
    int idx = helpScroll + i;
    if (idx >= helpLineCount) break;
    display.setCursor(0, 14 + i * 10);
    display.println(helpLines[idx]);
  }
  if (helpScroll > 0) { display.setCursor(122, 12); display.print("^"); }
  if (helpScroll < helpLineCount - HELP_VISIBLE) { display.setCursor(122, 56); display.print("v"); }
  display.display();
}

void drawListScreen(const char* title) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  int count = (listType == 0) ? listAPCount : listBLECount;
  display.setCursor(0, 0);
  display.print("[");
  display.print(title);
  display.print(" ");
  display.print(count > 0 ? (listScroll + 1) : 0);
  display.print("/");
  display.print(count > LIST_VISIBLE ? (count - LIST_VISIBLE + 1) : 1);
  display.println("]");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  if (count == 0) {
    display.setCursor(0, 20);
    display.println("(empty)");
    display.setCursor(0, 40);
    display.println("press * to scan");
    display.display();
    return;
  }
  for (int i = 0; i < LIST_VISIBLE; i++) {
    int idx = listScroll + i;
    if (idx >= count) break;
    display.setCursor(0, 14 + i * 10);
    if (listType == 0) display.println(listAPBuffer[idx]);
    else               display.println(listBLEBuffer[idx]);
  }
  if (listScroll > 0) { display.setCursor(122, 12); display.print("^"); }
  if (listScroll + LIST_VISIBLE < count) { display.setCursor(122, 56); display.print("v"); }
  display.display();
}

void redraw() {
  switch (currentScreen) {
    case MAIN:        drawMenu("MAIN",  mainMenu,  mainCount);   break;
    case WIFI:        drawMenu("WiFi",  wifiMenu,  wifiCount);   break;
    case WIFI_ATTACK: drawMenu("Attack", attackMenu, attackCount); break;
    case BT:          drawMenu("BT",    btMenu,    btCount);     break;
    case SNIFF:       drawMenu("Sniff", sniffMenu, sniffCount);  break;
    case SETTINGS:    drawMenu("Set",   setMenu,   setCount);    break;
    case SCR_STATUS:  drawStatus();                              break;
    case HELP:        drawHelp();                                break;
    case LIST:        drawListScreen(listType == 0 ? "AP LIST" : "BLE LIST"); break;
    case SAVED:       drawListScreen(listType == 0 ? "SAVED AP" : "SAVED BLE"); break;
  }
}

// ================= ОТПРАВКА =================
void sendCmd(const char* cmd) {
  Serial2.println(cmd);
  Serial.print("TX: ");
  Serial.println(cmd);
}

void showStatus(String l1, String l2, const char* cmd) {
  statusLine1 = l1;
  statusLine2 = l2;
  lastCmd = cmd;
  returnScreen = currentScreen;
  returnCursor = cursor;
  returnScroll = menuScroll;
  currentScreen = SCR_STATUS;
  cursor = 0;
  menuScroll = 0;
  drawStatus();
}

// ================= СТАРТ =================
void startScanWiFi() {
  listType = 0;
  captureCount = 0;
  capturing = true;
  silentCapture = false;
  scanPhase = 1;
  scanPhaseStart = millis();
  lastDisplayUpdate = 0;

  statusLine1 = "Scanning WiFi...";
  statusLine2 = "30 sec remaining";
  lastCmd = "scanall";
  returnScreen = currentScreen;
  returnCursor = cursor;
  returnScroll = menuScroll;
  currentScreen = SCR_STATUS;
  drawStatus();

  sendCmd("stopscan");
  delay(300);
  sendCmd("scanall");
}

void startScanBLE() {
  listType = 1;
  captureCount = 0;
  capturing = true;
  silentCapture = false;
  scanPhase = 1;
  scanPhaseStart = millis();
  lastDisplayUpdate = 0;

  statusLine1 = "Scanning BLE...";
  statusLine2 = "30 sec remaining";
  lastCmd = "sniffbt";
  returnScreen = currentScreen;
  returnCursor = cursor;
  returnScroll = menuScroll;
  currentScreen = SCR_STATUS;
  drawStatus();

  sendCmd("stopscan");
  delay(300);
  sendCmd("sniffbt");
}

void startListCapture(int type, bool silent) {
  listType = type;
  captureCount = 0;
  capturing = true;
  silentCapture = silent;
  scanPhase = 3;
  scanPhaseStart = millis();
  lastByteTime = millis();
  lastDisplayUpdate = 0;

  if (!silent) {
    statusLine1 = (type == 0) ? "Loading APs..." : "Loading BLE...";
    statusLine2 = "waiting...";
    lastCmd = (type == 0) ? "list -a" : "list -b";
    returnScreen = currentScreen;
    returnCursor = cursor;
    returnScroll = menuScroll;
    currentScreen = SCR_STATUS;
    drawStatus();
  }

  sendCmd(type == 0 ? "list -a" : "list -b");
}

// ================= ПРИЁМ =================
void pushCaptureLine(const char* line) {
  Serial.print("RAW: ");
  Serial.println(line);
  if (captureCount >= LIST_MAX_LINES) return;
  if (isNoiseLine(line)) return;

  if (listType == 0) {
    if (!isListAPLine(line)) return;
  } else {
    if (!isListBLELine(line)) return;
  }

  strncpy(captureBuffer[captureCount], line, LIST_LINE_LEN - 1);
  captureBuffer[captureCount][LIST_LINE_LEN - 1] = '\0';
  captureCount++;

  Serial.print("CAP[");
  Serial.print(captureCount);
  Serial.print("]: ");
  Serial.println(line);
}

void processCapture(char c) {
  if ((unsigned char)c > 127) return;
  lastByteTime = millis();
  static char lineBuf[80];
  static int linePos = 0;
  if (c == '\n' || c == '\r') {
    if (linePos > 0) {
      lineBuf[linePos] = '\0';
      pushCaptureLine(lineBuf);
      linePos = 0;
    }
  } else {
    if (linePos < 79) lineBuf[linePos++] = c;
  }
}

void finishCapture() {
  capturing = false;
  scanPhase = 0;
  if (listType == 0) {
    listAPCount = captureCount;
    for (int i = 0; i < captureCount; i++) {
      strncpy(listAPBuffer[i], captureBuffer[i], LIST_LINE_LEN);
      listAPBuffer[i][LIST_LINE_LEN - 1] = '\0';
    }
  } else {
    listBLECount = captureCount;
    for (int i = 0; i < captureCount; i++) {
      strncpy(listBLEBuffer[i], captureBuffer[i], LIST_LINE_LEN);
      listBLEBuffer[i][LIST_LINE_LEN - 1] = '\0';
    }
  }
  int count = (listType == 0) ? listAPCount : listBLECount;
  if (listScroll > count - 1) listScroll = (count > 0) ? count - 1 : 0;
  if (listScroll < 0) listScroll = 0;
  if (!silentCapture) {
    currentScreen = LIST;
    drawListScreen(listType == 0 ? "AP LIST" : "BLE LIST");
  } else if (currentScreen == LIST) {
    drawListScreen(listType == 0 ? "AP LIST" : "BLE LIST");
  }
  silentCapture = false;
}

// ================= ВЫБОР ЦЕЛИ =================
void selectTarget(int listIndex) {
  if (listIndex < 0 || listIndex >= captureCount) return;
  const char* line = (listType == 0) ? listAPBuffer[listIndex] : listBLEBuffer[listIndex];
  int idx = -1;
  if (sscanf(line, "[%d]", &idx) != 1 || idx < 0) {
    Serial.println("Cannot parse index");
    return;
  }
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "select -a %d", idx);
  sendCmd(cmd);
  char msg[32];
  snprintf(msg, sizeof(msg), "Selected #%d", idx);
  showStatus(msg, "target set", cmd);
}

// ================= ВЫПОЛНЕНИЕ =================
void executeCurrent() {
  if (currentScreen == MAIN) {
    if (cursor == 0) { currentScreen = WIFI;        cursor = 0; menuScroll = 0; }
    if (cursor == 1) { currentScreen = BT;          cursor = 0; menuScroll = 0; }
    if (cursor == 2) { currentScreen = SNIFF;       cursor = 0; menuScroll = 0; }
    if (cursor == 3) { currentScreen = SETTINGS;    cursor = 0; menuScroll = 0; }
  }
  else if (currentScreen == WIFI) {
    if (cursor == 0) { startScanWiFi(); return; }
    if (cursor == 1) { currentScreen = WIFI_ATTACK; cursor = 0; menuScroll = 0; redraw(); return; }
    if (cursor == 2) { sendCmd("attack -t deauth");    activeAttack = "Deauth";   showStatus("Deauth Sel", "Attacking selected...", "attack -t deauth"); }
    if (cursor == 3) { sendCmd("attack -t beacon -a"); activeAttack = "Beacon";   showStatus("Beacon Sel", "Cloning selected APs...", "attack -t beacon -a"); }
    if (cursor == 4) { sendCmd("attack -t probe");     activeAttack = "Probe";    showStatus("Probe Sel", "Probing selected...", "attack -t probe"); }
    if (cursor == 5) { sendCmd("stopscan"); activeAttack = ""; showStatus("Stopped", "WiFi halted.", "stopscan"); }
  }
  else if (currentScreen == WIFI_ATTACK) {
    if (cursor == 0) { sendCmd("attack -t deauth"); activeAttack = "Deauth"; showStatus("Attack: Deauth", "Kicking selected...", "attack -t deauth"); }
    if (cursor == 1) { sendCmd("attack -t beacon -a"); activeAttack = "Beacon"; showStatus("Attack: Beacon", "Cloning selected APs...", "attack -t beacon -a"); }
    if (cursor == 2) { sendCmd("attack -t probe"); activeAttack = "Probe"; showStatus("Attack: Probe", "Probing selected...", "attack -t probe"); }
    if (cursor == 3) { sendCmd("attack -t rickroll"); activeAttack = "Rickroll"; showStatus("Attack: Rickroll", "Sending rickroll...", "attack -t rickroll"); }
    if (cursor == 4) { sendCmd("attack -t beacon -r"); activeAttack = "Random SSID"; showStatus("Random SSID", "Spamming random names...", "attack -t beacon -r"); }
    if (cursor == 5) { sendCmd("attack -t beacon -a"); activeAttack = "Clone AP"; showStatus("Clone AP", "Copying selected APs...", "attack -t beacon -a"); }
    if (cursor == 6) {
      sendCmd("attack -t deauth");    delay(100);
      sendCmd("attack -t beacon -a"); delay(100);
      sendCmd("attack -t probe");     delay(100);
      sendCmd("attack -t rickroll");  delay(100);
      activeAttack = "All";
      showStatus("All Attacks", "all types", "ALL");
    }
    if (cursor == 7) { currentScreen = WIFI; cursor = 0; menuScroll = 0; redraw(); return; }
  }
  else if (currentScreen == BT) {
    if (cursor == 0) { startScanBLE(); return; }
    if (cursor == 1) { sendCmd("blespam -t all");   showStatus("BT Spamming",  "Sending packets...", "blespam -t all"); }
    if (cursor == 2) { sendCmd("stopscan"); activeAttack = ""; showStatus("Stopped", "BT halted.", "stopscan"); }
  }
  else if (currentScreen == SNIFF) {
    if (cursor == 0) { sendCmd("sniffbeacon"); showStatus("Beacon Sniff", "Capturing beacons...", "sniffbeacon"); }
    if (cursor == 1) { sendCmd("sniffpmkid");  showStatus("PMKID Sniff",  "Capturing PMKID...",   "sniffpmkid"); }
    if (cursor == 2) { sendCmd("sniffraw");    showStatus("Raw Sniff",    "Raw 802.11 frames...", "sniffraw"); }
    if (cursor == 3) { sendCmd("stopscan"); activeAttack = ""; showStatus("Stopped", "Sniffer halted.", "stopscan"); }
  }
  else if (currentScreen == SETTINGS) {
    if (cursor == 0) { currentScreen = HELP; cursor = 0; helpScroll = 0; drawHelp(); return; }
    if (cursor == 1) { listType = 0; listScroll = 0; currentScreen = SAVED; drawListScreen("SAVED AP"); return; }
    if (cursor == 2) { sendCmd("reboot");   showStatus("Rebooting", "Marauder restarting","reboot"); }
    if (cursor == 3) { sendCmd("info");     showStatus("Info",      "Dumping info...",     "info"); }
    if (cursor == 4) { sendCmd("stopscan"); activeAttack = ""; showStatus("Stop All", "Everything halted.", "stopscan"); }
    if (cursor == 5) { currentScreen = MAIN; cursor = 0; menuScroll = 0; redraw(); return; }
  }
}

int currentCount() {
  switch (currentScreen) {
    case MAIN:        return mainCount;
    case WIFI:        return wifiCount;
    case WIFI_ATTACK: return attackCount;
    case BT:          return btCount;
    case SNIFF:       return sniffCount;
    case SETTINGS:    return setCount;
    default:          return 0;
  }
}

void updateScrollForCursor() {
  if (cursor < menuScroll) menuScroll = cursor;
  if (cursor >= menuScroll + MENU_VISIBLE) menuScroll = cursor - MENU_VISIBLE + 1;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Remote booting...");

  Wire.begin(KP_SDA, KP_SCL);
  Wire.setClock(100000);

  Wire1.begin(18, 19);
  Wire1.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 not found!");
    while (1);
  }
  Serial.println("Display OK");

  Serial2.begin(115200, SERIAL_8N1, MARAUDER_RX, MARAUDER_TX);
  Serial.println("UART2 ready");

  cursor = 0;
  menuScroll = 0;
  helpScroll = 0;
  listScroll = 0;
  listAPCount = 0;
  listBLECount = 0;
  scanPhase = 0;
  currentScreen = MAIN;
  redraw();
}

// ================= LOOP =================
void loop() {
  while (Serial2.available()) {
    char c = Serial2.read();
    Serial.write(c);
    if (capturing) processCapture(c);
  }

  if (capturing && scanPhase == 1) {
    unsigned long elapsed = millis() - scanPhaseStart;
    if (millis() - lastDisplayUpdate > 1000) {
      lastDisplayUpdate = millis();
      long remaining = ((long)SCAN_TIME - (long)elapsed) / 1000;
      if (remaining < 0) remaining = 0;
      char buf[24];
      snprintf(buf, sizeof(buf), "%ld sec remaining", remaining);
      statusLine1 = (listType == 0) ? "Scanning WiFi..." : "Scanning BLE...";
      statusLine2 = buf;
      drawStatus();
      Serial.print("Countdown: ");
      Serial.println(remaining);
    }
    if (elapsed > SCAN_TIME) {
      scanPhase = 2;
      scanPhaseStart = millis();
      statusLine1 = "Stopping...";
      statusLine2 = "please wait";
      drawStatus();
      sendCmd("stopscan");
    }
  }
  else if (capturing && scanPhase == 2 && (millis() - scanPhaseStart) > STOP_WAIT) {
    captureCount = 0;
    sendCmd(listType == 0 ? "list -a" : "list -b");
    scanPhase = 3;
    scanPhaseStart = millis();
    lastByteTime = millis();
    statusLine1 = "Getting list...";
    statusLine2 = "waiting for data";
    drawStatus();
  }
  else if (capturing && scanPhase == 3) {
    unsigned long elapsed = millis() - scanPhaseStart;
    bool minPassed = elapsed > LIST_MIN_WAIT;
    bool maxPassed = elapsed > LIST_MAX_WAIT;
    bool quiet = (millis() - lastByteTime) > 700;
    if (maxPassed || (minPassed && quiet)) {
      finishCapture();
    }
  }

  // === АВТООБНОВЛЕНИЕ LIST (возвращено) ===
  if (!capturing && currentScreen == LIST &&
      (millis() - lastRefresh > REFRESH_INTERVAL) &&
      (millis() - lastUserAction > USER_PAUSE)) {
    startListCapture(listType, true);
    lastRefresh = millis();
  }

  char key = scanKeypad();
  if (key == 0) return;
  lastUserAction = millis();   // ← сброс паузы при нажатии

  if (capturing) {
    if (key == 'D') {
      if (silentCapture) {
        capturing = false;
        scanPhase = 0;
        drawListScreen(listType == 0 ? "AP LIST" : "BLE LIST");
      } else {
        if (scanPhase == 1 || scanPhase == 2) sendCmd("stopscan");
        capturing = false;
        scanPhase = 0;
        if (captureCount > 0) {
          finishCapture();
        } else {
          currentScreen = returnScreen;
          cursor = returnCursor;
          menuScroll = returnScroll;
          redraw();
        }
      }
    }
    return;
  }

  if (currentScreen == SAVED) {
    if (key == 'A') { if (listScroll > 0) listScroll--; drawListScreen(listType == 0 ? "SAVED AP" : "SAVED BLE"); }
    else if (key == 'B') {
      int count = (listType == 0) ? listAPCount : listBLECount;
      if (listScroll < count - LIST_VISIBLE) listScroll++;
      drawListScreen(listType == 0 ? "SAVED AP" : "SAVED BLE");
    }
    else if (key == 'C' || key == '9') {
      listType = 1 - listType;
      listScroll = 0;
      drawListScreen(listType == 0 ? "SAVED AP" : "SAVED BLE");
    }
    else if (key == 'D') { currentScreen = MAIN; cursor = 0; menuScroll = 0; redraw(); }
    return;
  }

  if (key == '9') {
    listScroll = 0;
    currentScreen = SAVED;
    drawListScreen(listType == 0 ? "SAVED AP" : "SAVED BLE");
    return;
  }

  if (currentScreen == LIST) {
    if (key == 'A') { if (listScroll > 0) listScroll--; drawListScreen(listType == 0 ? "AP LIST" : "BLE LIST"); }
    else if (key == 'B') {
      int count = (listType == 0) ? listAPCount : listBLECount;
      if (listScroll < count - LIST_VISIBLE) listScroll++;
      drawListScreen(listType == 0 ? "AP LIST" : "BLE LIST");
    }
    else if (key == 'C') {
      int count = (listType == 0) ? listAPCount : listBLECount;
      if (count > 0 && listScroll < count) {
        selectTarget(listScroll);
      }
    }
    else if (key == 'D') {
      currentScreen = WIFI;
      cursor = 0;
      menuScroll = 0;
      redraw();
    }
    else if (key == '*') {
      if (listType == 0) startScanWiFi(); else startScanBLE();
    }
    return;
  }

  if (key == '*') {
    if (currentScreen == BT) startScanBLE();
    else startScanWiFi();
    return;
  }

  if (key == '#') {
    sendCmd("stopscan");
    activeAttack = "";
    showStatus("STOP", "All halted.", "stopscan");
    return;
  }

  if (currentScreen == HELP) {
    if (key == 'A') { if (helpScroll > 0) helpScroll--; drawHelp(); }
    else if (key == 'B') { if (helpScroll < helpLineCount - HELP_VISIBLE) helpScroll++; drawHelp(); }
    else if (key == 'D') { currentScreen = SETTINGS; cursor = 0; menuScroll = 0; redraw(); }
    return;
  }

  if (currentScreen == SCR_STATUS) {
    if (key == 'D') {
      currentScreen = returnScreen;
      cursor = returnCursor;
      menuScroll = returnScroll;
      redraw();
    }
    return;
  }

  if (key == 'A') {
    cursor--;
    if (cursor < 0) cursor = currentCount() - 1;
    updateScrollForCursor();
    redraw();
  }
  else if (key == 'B') {
    cursor++;
    if (cursor >= currentCount()) cursor = 0;
    updateScrollForCursor();
    redraw();
  }
  else if (key == 'C') {
    executeCurrent();
  }
  else if (key == 'D') {
    if (currentScreen == WIFI_ATTACK) { currentScreen = WIFI; cursor = 0; menuScroll = 0; redraw(); return; }
    if (currentScreen != MAIN) { currentScreen = MAIN; cursor = 0; menuScroll = 0; redraw(); }
  }
}
