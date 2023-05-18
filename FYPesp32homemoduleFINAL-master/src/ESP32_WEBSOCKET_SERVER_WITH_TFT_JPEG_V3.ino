#include <SPI.h>
#include <ArduinoWebsockets.h>
#include <WiFi.h>
#include <TJpg_Decoder.h>
#include <TFT_eSPI.h>
#include <ezButton.h>

const char* ssid = "ibrar";
const char* password = "ibrarahmad";

using namespace websockets;
WebsocketsServer server;
WebsocketsClient client;

TFT_eSPI tft = TFT_eSPI(); // Invoke custom library

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  // Stop further decoding as the image is running off the bottom of the screen
  if (y >= tft.height()) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // Return 1 to decode the next block
  return 1;
}

ezButton button(13); // Pin for the button

bool buttonPressed = false;
unsigned long lastButtonPressTime = 0;
const unsigned long buttonCooldownTime = 5000; // Cooldown time in milliseconds

void setup() {
  Serial.begin(115200);
  delay(1000);

  tft.begin();
  tft.setRotation(3);
  tft.setTextColor(0xFFFF, 0x0000);
  tft.fillScreen(TFT_RED);
  tft.setSwapBytes(true); // We need to swap the colour bytes (endianess)

  // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(1);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);

  Serial.println();
  Serial.println("Setting AP...");
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP Address : ");
  Serial.println(IP);

  server.listen(8888);

  button.setDebounceTime(50); // Set debounce time to 50 milliseconds
  button.setCountMode(COUNT_FALLING);
}

void loop() {
  if (server.poll()) {
    client = server.accept();
  }

  if (client.available()) {
    client.poll();
    button.loop(); // Call the button's loop function to update its state

    if (button.isPressed() && !buttonPressed && (millis() - lastButtonPressTime >= buttonCooldownTime)) {
      buttonPressed = true;
      lastButtonPressTime = millis();

      // Send message to the client
      if (client.available()) {
        client.send("Button pressed!");
        Serial.println("Button pressed");

        // Display "Button pressed" on the TFT display
        tft.setCursor(0, 0);
        tft.println("Button pressed");
      }
    }

    if (buttonPressed && (millis() - lastButtonPressTime >= buttonCooldownTime)) {
      buttonPressed = false;
    }

    WebsocketsMessage msg = client.readBlocking();

    uint32_t t = millis();

    // Get the width and height in pixels of the JPEG if you wish
    uint16_t w = 0, h = 0;
    TJpgDec.getJpgSize(&w, &h, (const uint8_t*)msg.c_str(), msg.length());
    Serial.print("Width = ");
    Serial.print(w);
    Serial.print(", height = ");
    Serial.println(h);

    // Draw the image, top left at 0,0
    TJpgDec.drawJpg(0, 0, (const uint8_t*)msg.c_str(), msg.length());

    // How much time did rendering take (ESP8266 80MHz 271ms, 160MHz 157ms, ESP32 SPI 120ms, 8bit parallel 105ms
    t = millis() - t;
    Serial.print(t);
    Serial.println(" ms");
  }
}
