# ESP32 Marauder Remote

Автономный пульт для ESP32 Marauder — клавиатура 4x4 + OLED SSD1306.

## 📦 Что нужно

- 2× ESP32 DevKit V1
- 1× клавиатура 4x4 + PCF8574 (адрес 0x27)
- 1× OLED SSD1306 0.96" (адрес 0x3C)
- 2× повербанк 2A+
- Провода Dupont

## 🔌 Схема подключения

### Пульт ↔ Marauder (UART2)
- Пульт GPIO 16 (RX) ←→ Marauder GPIO 17 (TX)
- Пульт GPIO 17 (TX) ←→ Marauder GPIO 16 (RX)
- GND ↔ GND — обязательно
- Скорость: 115200, 8N1

### Пульт ↔ Клавиатура (PCF8574, 0x27)
- SDA → GPIO 25
- SCL → GPIO 26
- VCC → 3.3V
- GND → GND
- Матрица 4x4 к PCF8574: Rows → P0–P3, Columns → P4–P7  
  (или наоборот — зависит от модуля, проверьте по своему)

### Пульт ↔ Дисплей (SSD1306, 0x3C)
- SDA → GPIO 18
- SCL → GPIO 19
- VCC → 3.3V
- GND → GND

## 🚀 Установка

### 1. Библиотеки

- **Для Marauder:** библиотеки уже встроены в исходники Marauder и устанавливаются при открытии кода Marauder. Отдельно ничего ставить не нужно.
- **Для пульта:** Arduino IDE → Инструменты → Управлять библиотеками:
  - Adafruit SSD1306 (автор Adafruit)
  - Adafruit GFX Library (автор Adafruit)
  - AdvKeyPad (автор Rob Tillaart)

Названия библиотек оставлены как есть — не меняйте их.

### 2. Прошей Marauder

Готовые исходники в папке `Marauder_modified/`:
- Board: ESP32 Dev Module
- Partition: Minimal SPIFFS
- Открой `esp32_marauder.ino`
- Залей

### 3. Прошей Пульт

- Открой `Remote/MarauderRemote/MarauderRemote.ino`
- Board: ESP32 Dev Module
- Залей на вторую ESP32

## 🎮 Управление

| Клавиша | Действие |
| :--- | :--- |
| A / B | Вверх / Вниз |
| C и B Последовательно | Выбрать |
| D | Назад |
| # | STOP |
| * | Сканировать |
| 9 | SAVED |

## 🔄 Протокол UART

Пульт шлёт текстовые команды, Marauder их выполняет. Пример команд:

- `UP`
- `DOWN`
- `SELECT`
- `BACK`
- `STOP`
- `SCAN`
- `SAVED`

Если у тебя в коде другие команды — замени этот список на свои.

## ⚠️ Известные проблемы

- Режим **BT Spam All attack** на ESP32 DevKit V1 с Marauder v1.16.0 вызывает краш:
  - `Guru Meditation Error: Core 0 panic'ed (InstrFetchProhibited)`
  - `PC: 0x00000000`
  - После чего ESP32 перезагружается.
- Workaround: не использовать `All attack`; использовать одиночные режимы BT Spam. Для остановки зависшего спама нажать Reset/EN.
  -После нажатия клавиши # , сразу надо нажать кнопку D , если этого не сделать, прийдеться перезагружать плату.

## 🛠 Troubleshooting

- Дисплей не работает → проверь адрес I2C (0x3C/0x3D) и питание.
- Клавиатура не работает → проверь адрес PCF8574 (0x27), подключение SDA/SCL, питание.
- UART не работает → проверь перекрёстное соединение TX/RX и общий GND.
- Мусор в Serial → проверь скорость 115200.

## 📁 Структура репозитория

- `Marauder_modified/` — изменённые исходники Marauder
- `Remote/MarauderRemote/` — код пульта
- `README.md`
- `LICENSE`

## 📄 Лицензия

MIT. Оригинал Marauder — justcallmekoko.  
Код пульта — n1ghtdev23  
Полный текст лицензии в файле `LICENSE`.

## ⚠️ Дисклеймер

Проект предназначен только для тестирования собственных устройств и обучения.  
Использование против чужих устройств без разрешения может нарушать закон.  
Автор не несёт ответственности за любой ущерб.

## 🙏 Credits

- justcallmekoko — ESP32 Marauder
- Adafruit — Adafruit SSD1306, Adafruit GFX Library
- Rob Tillaart — AdvKeyPad