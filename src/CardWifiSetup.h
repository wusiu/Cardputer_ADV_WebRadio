 /* 
 * @file CardWifiSetup.h
 * @author Aurélio Avanzi (Cyberwisk)
 *
 * @modified by WuSiU
 * @date 2026-02-01
 *
 * @Hardwares: M5Cardputer ADV
 * */


#include <WiFi.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <vector>

#define NVS_NAMESPACE "M5_settings"
#define NVS_SSID_KEY "wifi_ssid"
#define NVS_PASS_KEY "wifi_pass"
#define WIFI_TIMEOUT 20000
#define MAX_NETWORKS 10

String CFG_WIFI_SSID;
String CFG_WIFI_PASS;
Preferences preferences;

struct WiFiNetwork {
    String ssid;
    int32_t rssi;
    wifi_auth_mode_t encryption;
};
std::vector<WiFiNetwork> networks;

uint32_t calculateHash(const String& str) {
    uint32_t hash = 5381;
    for (char c : str) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

String inputText(const String& prompt, int x, int y, bool isPassword = false) {
    String data = "> ";
    String displayData = "> ";
    
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextScroll(true);
    M5Cardputer.Display.drawString(prompt, x, y);
    
    while (true) {
        M5Cardputer.update();

        if (M5Cardputer.Keyboard.isChange() &&
            M5Cardputer.Keyboard.isPressed()) {

            Keyboard_Class::KeysState status =
                M5Cardputer.Keyboard.keysState();

            for (auto i : status.word) {
                data += i;
                displayData += i;
            }

            if (status.del && data.length() > 2) {
                data.remove(data.length() - 1);
                displayData.remove(displayData.length() - 1);
            }

            if (status.enter) {
                data.remove(0, 2);
                return data;
            }

            M5Cardputer.Display.fillRect(0, y - 4,
                M5Cardputer.Display.width(), 25, BLACK);
            M5Cardputer.Display.drawString(displayData, 4, y);
        }

        delay(1);
    }

}

void displayWiFiInfo() {
    M5Cardputer.Display.fillRect(0, 20, 240, 135, BLACK);
    M5Cardputer.Display.setCursor(1, 1);
    M5Cardputer.Display.drawString("WiFi", 35, 1);
    M5Cardputer.Display.drawString("SSID: " + WiFi.SSID(), 1, 18);
    M5Cardputer.Display.drawString("IP: " + WiFi.localIP().toString(), 1, 33);
    int8_t rssi = WiFi.RSSI();
    M5Cardputer.Display.drawString("RSSI: " + String(rssi) + " dBm", 1, 48);
    unsigned long start = millis();
    while (millis() - start < 2000) {
    M5Cardputer.update();
    }
    M5Cardputer.Display.fillRect(0, 0, 240, 135, BLACK);
}


String scanAndDisplayNetworks() {

    int scrollOffset = 0;
    unsigned long lastScroll = 0;
    const unsigned long scrollDelay = 80;
    const unsigned long scrollStartDelay = 800;
    unsigned long lastScrollUpdate = 0;

    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);
    delay(100);

    int scanResult = WiFi.scanNetworks(false, true);

    M5Cardputer.Display.clear();
    M5Cardputer.Display.drawString("Scanning WiFi...", 1, 1);

    if (scanResult <= 0) {
        M5Cardputer.Display.clear();
        M5Cardputer.Display.drawString("No networks found", 1, 40);
        unsigned long start = millis();
        while (millis() - start < 2000) {
        M5Cardputer.update();
        }
        return "";
    }

    networks.clear();

    for (int i = 0; i < scanResult && networks.size() < MAX_NETWORKS; i++) {

        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;

        networks.push_back({
            ssid,
            WiFi.RSSI(i),
            WiFi.encryptionType(i)
        });
    }

    std::sort(networks.begin(), networks.end(),
        [](const WiFiNetwork& a, const WiFiNetwork& b) {
            return a.rssi > b.rssi;
        });

    int selectedNetwork = 0;

    bool needsRedraw = true;

    scrollOffset = 0;
    lastScroll = millis();

    while (true) {

    if (networks.empty()) {
        M5Cardputer.Display.fillScreen(BLACK);
        M5Cardputer.Display.drawString("No networks", 40, 60);
        unsigned long start = millis();
        while (millis() - start < 2000) {
            M5Cardputer.update();
        }
        return "";
    }

    if (needsRedraw) {
        M5Cardputer.Display.fillScreen(BLACK);
        M5Cardputer.Display.drawString("Available networks:", 1, 1);

        for (size_t i = 0; i < networks.size(); i++) {
            int y = 18 + i * 16;
            bool selected = (i == selectedNetwork);

            String prefix = selected ? "> " : "  ";
            String text = networks[i].ssid + " (" + String(networks[i].rssi) + "dBm)";

            int x = 1;

            int textWidth = M5Cardputer.Display.textWidth(text);

            String visibleText = text;

            if (selected && textWidth > 220) {
                if (scrollOffset < text.length()) {
                    visibleText = text.substring(scrollOffset);
                }
            }

            M5Cardputer.Display.drawString(
                prefix + visibleText,
                x, y
            );
        }

        M5Cardputer.Display.drawString("ENTER = select", 1, 120);
        needsRedraw = false;
    }

    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {

        if (M5Cardputer.Keyboard.isKeyPressed(';') && selectedNetwork > 0) {
            selectedNetwork--;
            scrollOffset = 0;
            lastScroll = millis();
            lastScrollUpdate = millis();
            needsRedraw = true;
        }
        else if (M5Cardputer.Keyboard.isKeyPressed('.') &&
                selectedNetwork < networks.size() - 1) {
            selectedNetwork++;
            scrollOffset = 0;
            lastScroll = millis();
            lastScrollUpdate = millis();
            needsRedraw = true;
        }
        else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
            return networks[selectedNetwork].ssid;
        }
    }
    String fullText =
        networks[selectedNetwork].ssid +
        " (" + String(networks[selectedNetwork].rssi) + "dBm)";

    int textWidth = M5Cardputer.Display.textWidth(fullText);

    if (textWidth > 220 &&
        millis() - lastScroll > scrollDelay &&
        millis() - lastScrollUpdate > scrollStartDelay) {

        lastScroll = millis();
        lastScrollUpdate = millis();
        scrollOffset++;

        if (scrollOffset > fullText.length())
            scrollOffset = 0;

        needsRedraw = true;
    }

    delay(1);
}

}


void connectToWiFi() {
    WiFi.mode(WIFI_STA);
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    
    preferences.begin(NVS_NAMESPACE, true);
    CFG_WIFI_SSID = preferences.getString(NVS_SSID_KEY, "");
    CFG_WIFI_PASS = preferences.getString(NVS_PASS_KEY, "");
    uint32_t stored_ssid_hash = preferences.getUInt("ssid_hash", 0);
    uint32_t stored_pass_hash = preferences.getUInt("pass_hash", 0);
    preferences.end();
    
    bool validCredentials = !CFG_WIFI_SSID.isEmpty() && 
                          (calculateHash(CFG_WIFI_SSID) == stored_ssid_hash) &&
                          (calculateHash(CFG_WIFI_PASS) == stored_pass_hash);
    
    if (validCredentials) {
        WiFi.begin(CFG_WIFI_SSID.c_str(), CFG_WIFI_PASS.c_str());
        
        unsigned long startTime = millis();
        M5Cardputer.Display.print("Network connection");
        
        while (millis() - startTime < WIFI_TIMEOUT) {
            M5Cardputer.update();
            
            if (M5Cardputer.BtnA.isPressed()) {
                preferences.begin(NVS_NAMESPACE, false);
                preferences.clear();
                preferences.end();
                M5Cardputer.Display.clear();
                M5Cardputer.Display.drawString("Memory cleared.", 1, 60);
                delay(1000);
                ESP.restart();
                return;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                displayWiFiInfo();
                return;
            }
            
            M5Cardputer.Display.print(".");
            delay(50);
        }
    }
    
    M5Cardputer.Display.clear();
    M5Cardputer.Display.drawString("Wi-Fi setup", 1, 1);
    
    CFG_WIFI_SSID = scanAndDisplayNetworks();
    if (CFG_WIFI_SSID.isEmpty()) {
        return;
    }
    
    M5Cardputer.Display.clear();
    M5Cardputer.Display.drawString("SSID: " + CFG_WIFI_SSID, 1, 20);
    M5Cardputer.Display.drawString("Enter your password:", 1, 38);
    CFG_WIFI_PASS = inputText("> ", 4, M5Cardputer.Display.height() - 24, true);
    
    preferences.begin(NVS_NAMESPACE, false);
    preferences.putString(NVS_SSID_KEY, CFG_WIFI_SSID);
    preferences.putString(NVS_PASS_KEY, CFG_WIFI_PASS);
    preferences.putUInt("ssid_hash", calculateHash(CFG_WIFI_SSID));
    preferences.putUInt("pass_hash", calculateHash(CFG_WIFI_PASS));
    preferences.end();
    
    M5Cardputer.Display.clear();
    M5Cardputer.Display.drawString("Data saved.", 1, 60);
    
    WiFi.begin(CFG_WIFI_SSID.c_str(), CFG_WIFI_PASS.c_str());
    delay(300);
    displayWiFiInfo();
}
