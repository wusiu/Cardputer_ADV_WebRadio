# WebRadio_WuSiU - Cardputer Adv

An advanced internet radio for **M5Stack Cardputer Adv** with SD card station list support, Wi-Fi memory, and additional audio features.

![Cardputer ADV WebRadio – running](images/WebRadio.jpg)

---

## 📻 Radio Station List

The list of radio stations is stored in a text file: **station_list.txt**

The file must be placed in the **root directory of the SD card**.

### File format:

Example:
Radio 01 Name, http://radio-stream-link-01  
Radio 02 Name, http://radio-stream-link-02


- one station per line  
- station name and stream URL separated by a comma

---

## ⌨️ Controls (Cardputer Adv Keyboard)

- **Left / Right Arrow** – change radio station  
- **Up / Down Arrow** – adjust volume  
- **R** – reset the server connection (if the radio freezes or fails to start)
- **M** – toggle mute on / off
- **F** – toggle FFT audio visualization
- **B** – adjust screen brightness

---

## 📶 Wi-Fi

- Wi-Fi settings are **saved to memory**
- The device reconnects automatically on the next startup

### Reset Wi-Fi settings

To reset stored Wi-Fi credentials:
1. Start the program
2. Press **BtnG0** while the device is attempting to connect to the network

---

## 🧰 Requirements

- M5Stack Cardputer Adv
- SD card with `station_list.txt`
- Wi-Fi connection

---

## ⚙️ Arduino IDE Configuration

Before compiling and uploading the firmware, make sure the correct board and partition scheme are selected in Arduino IDE.

---

### 🧩 Select Board

Choose the correct board for Cardputer:

**M5Cardputer**

![Board_selection](images/board.jpg)

---

### 💾 Partition Scheme

Set the partition scheme to:

**Huge APP (3MB No OTA / 1MB SPIFFS)**

This is required to fit the web radio application in flash memory.

![Partition scheme](images/partition_scheme.jpg)

---

## 👤 Author

**WuSiU**

## 📄 License

MIT License

## Credits

Based on the original project:
https://github.com/cyberwisk/M5Cardputer_WebRadio


Original author: cyberwisk



