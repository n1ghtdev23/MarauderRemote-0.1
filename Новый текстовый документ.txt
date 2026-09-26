# ESP32 Marauder Remote (v0.1)

An autonomous hardware control panel for **ESP32 Marauder** using a 4x4 matrix keypad and an OLED SSD1306 display. 

This project allows you to control Marauder entirely offline and in the field, without needing a smartphone, PC, or WebUI. 

> 💰 **Estimated build cost:** ~\$18–\$22 (1600–1800 RUB).
> 🛠 **Author:** [@n1ghtdev23](https://github.com)
> 💡 Based on the original ESP32 Marauder by @justcallmekoko.

---

## 📦 Hardware Requirements

*   **2×** ESP32 DevKit V1 boards
*   **1×** 4x4 Matrix Keypad + PCF8574 I2C Expander (Default address: `0x27`)
*   **1×** 0.96" OLED SSD1306 Display (Default address: `0x3C`)
*   **2×** Power Banks (2A+ output recommended)
*   Dupont jumper wires

---

## 🔌 Wiring Diagram

### 1. Remote ESP32 ↔ Marauder ESP32 (UART2 Cross-Connection)
*   Remote **GPIO 16 (RX)** ←→ Marauder **GPIO 17 (TX)**
*   Remote **GPIO 17 (TX)** ←→ Marauder **GPIO 16 (RX)**
*   **GND** ←→ **GND** (⚠️ *Mandatory common ground!*)
*   *Baudrate:* `115200`, 8N1

### 2. Remote ESP32 ↔ Keypad (PCF8574, 0x27)
*   **SDA** → GPIO 25
*   **SCL** → GPIO 26
*   **VCC** → 3.3V
*   **GND** → GND
*   *Note:* Matrix 4x4 connects directly to PCF8574. Pin mapping (Rows P0–P3, Columns P4–P7) may vary depending on your specific hardware module.

### 3. Remote ESP32 ↔ OLED Display (SSD1306, 0x3C)
*   **SDA** → GPIO 18
*   **SCL** → GPIO 19
*   **VCC** → 3.3V
*   **GND** → GND

---

## 🚀 Installation & Flashing

### 1. Install Required Libraries
*   **For Marauder:** All dependencies are already bundled inside the modified source folder. No extra installation needed.
*   **For the Remote:** Open Arduino IDE → *Tools* → *Manage Libraries* and install:
    *   `Adafruit SSD1306` (by Adafruit)
    *   `Adafruit GFX Library` (by Adafruit)
    *   `AdvKeyPad` (by Rob Tillaart)
    *   *Do not change or rename these libraries.*

### 2. Flash the Marauder Board
1. Navigate to the `Marauder_modified/` directory.
2. Open `esp32_marauder.ino` in Arduino IDE.
3. Select Board: **ESP32 Dev Module**.
4. Select Partition Scheme: **Minimal SPIFFS**.
5. Upload the sketch to your first ESP32.

### 3. Flash the Remote Board
1. Navigate to `Remote/MarauderRemote/`.
2. Open `MarauderRemote.ino` in Arduino IDE.
3. Select Board: **ESP32 Dev Module**.
4. Upload the sketch to your second ESP32.

---

## 🎮 Controls & Keymap

| Key | Action |
| :---: | --- |
| **A / B** | Move Selection Up / Down |
| **C then B** | Select / Enter (Sequential press) |
| **D** | Back / Return |
| **#** | STOP current attack/scan |
| *** ** | Start Scan |
| **9** | Open SAVED menu |

### 🔄 UART Communication Protocol
The Remote sends plain text commands over serial, and the Marauder unit executes them. Default commands include:
`UP`, `DOWN`, `SELECT`, `BACK`, `STOP`, `SCAN`, `SAVED`.

---

## ⚠️ Known Issues & Limitations

1. **Bluetooth Spam "All Attack" Crash:** 
   Running the `BT Spam All` attack on ESP32 DevKit V1 with Marauder v1.16.0 triggers a core panic error:
   `Core 0 panic'ed (InstrFetchProhibited) PC: 0x00000000` causing a reboot loop.
   * **Workaround:** Avoid "All Attack" mode; use single target BT Spam sub-modes instead. Press the Physical **Reset/EN** button to interrupt a frozen spam sequence.
2. **Keypad Freeze after `#`:** 
   After pressing the `#` (STOP) key, you must immediately press the `D` (BACK) key. Failing to do so might cause the keypad interface to freeze, requiring a hardware reboot.

---

## 🛠 Troubleshooting

*   **Blank Display:** Double-check your I2C address (`0x3C` or `0x3D`) and verify the VCC/GND pins.
*   **Unresponsive Keypad:** Verify the PCF8574 I2C address (`0x27`), check SDA/SCL lines, and ensure it receives stable power.
*   **No Serial Data/Connection:** Ensure **TX** connects to **RX** (and vice versa) and that the **GND** pins of both ESP32 units are tied together.
*   **Garbage output in Serial Monitor:** Ensure your baud rate is strictly set to `115200`.

---

## 📁 Repository Structure

*   `Marauder_modified/` — Pre-configured Marauder source files.
*   `Remote/MarauderRemote/` — Source code for the controller.
*   `README.md` — Documentation.
*   `LICENSE` — Legal information.

---

## 📄 License & Disclaimer

### License
This project is licensed under the **MIT License**.
* Original Marauder code base by **justcallmekoko**.
* Remote hardware controller firmware by **n1ghtdev23**.

### Disclaimer
This tool is developed for educational purposes and authorized penetration testing only. Do not use this project against devices without explicit permission from the owner. The author accepts no liability for any misuse, damage, or legal consequences caused by this software.
