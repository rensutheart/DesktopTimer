#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <Adafruit_NeoPixel.h>

// Buttons
#define BUTTON_1 10
#define BUTTON_2 3

#define ENCODER_PIN_A 0
#define ENCODER_PIN_B 1
#define ENCODER_BUTTON 2

volatile int lastEncoded = 0;
volatile long encoderValue = 0;
volatile bool encoderValueChanged = false;

long lastencoderValue = 0;

int lastMSB = 0;
int lastLSB = 0;

long readEncoderValue(void) {
  return encoderValue / 4;
}

// Neo Pixel
#define NEOPIXEL_PIN 6

Adafruit_NeoPixel strip = Adafruit_NeoPixel(48, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

int currentPixel = 0;

//https://learn.adafruit.com/led-tricks-gamma-correction/the-quick-fix
const uint8_t PROGMEM gamma8[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2,
  2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5,
  5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
  10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
  17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
  25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
  37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
  51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
  69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
  90, 92, 93, 95, 96, 98, 99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
  115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
  144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
  177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
  215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
};


#define RGB_BRIGHTNESS 32  // Change white brightness (max 255)

// I2C
#define I2C_SDA 4
#define I2C_SCL 5

// BME280
//if you need to read altitude,you need to know the sea level pressure
#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;

// OLED screen
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

#define OLED_RESET -1  // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// TIMER related
int timerDuration = 0;
int timerMillisAtStart = 0;
bool timerRunning = false;
int stopwatchMillisAtStart = 0;
int stopwatchMillisAtPause = 0;
bool stopwatchRunning = false;

// TIMER States
bool startState = 0;
bool pauseState = 0;
bool workState = 0;
bool breakState = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  Serial.println("DesktopTimer Running v0.1");

  /*
    BUTTONS
    */
  pinMode(BUTTON_1, INPUT);
  pinMode(BUTTON_2, INPUT);

  pinMode(ENCODER_PIN_A, INPUT);
  pinMode(ENCODER_PIN_B, INPUT);
  pinMode(ENCODER_BUTTON, INPUT);

  digitalWrite(ENCODER_PIN_A, HIGH);  //turn pullup resistor on
  digitalWrite(ENCODER_PIN_B, HIGH);  //turn pullup resistor on

  //call updateEncoder() when any high/low changed seen
  attachInterrupt(ENCODER_PIN_A, updateEncoder, CHANGE);
  attachInterrupt(ENCODER_PIN_B, updateEncoder, CHANGE);

  /*
    NEOPIXEL
    */
  strip.begin();
  strip.setBrightness(50);
  strip.show();  // Initialize all pixels to 'off'
  rainbow(0);

  /*
    DISPLAY
    */
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed. Could not start OLED."));
    // for(;;); // Don't proceed, loop forever
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);  // Draw 2X-scale text
  //display.setCursor(5, 5);            // Start at top-left corner
  display.println("Welcome\nto\nTimer\n\\_('-')_/");
  display.display();

  /*
    BME280
    */
  // default settings
  // (you can also pass in a Wire library object like &Wire2)

  if (!bme.begin(0x77, &Wire)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1)
      ;
  }
  else {
    Serial.println("Found BME280 sensor");
  }
}

void loop() {
  // printValues();
  // // delay(1000);
  // if(encoderValueChanged){
  //   encoderValueChanged = false;
  //   rainbow(readEncoderValue());
  // }

  if(digitalRead(BUTTON_1))
    neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS,RGB_BRIGHTNESS,0); // Yellow
  else if(digitalRead(BUTTON_2))
    neopixelWrite(RGB_BUILTIN,0,0,RGB_BRIGHTNESS); // Blue
  else if(isEncoderButtonPushDown())
    neopixelWrite(RGB_BUILTIN,RGB_BRIGHTNESS,0,RGB_BRIGHTNESS); // Magenta
  else
    neopixelWrite(RGB_BUILTIN,0,0,0);  // reset

  // CONTINUE HERE
  if (digitalRead(BUTTON_1)) {
    startStopwatchWithStartTime(stopwatchMillisAtPause);
  }


  if (timerRunning) {
    int timerRuntime = (millis() - timerMillisAtStart);

    if (timerRuntime > timerDuration) {
      timerRunning = false;
      show_time(0);
    }
    show_time(timerDuration - timerRuntime);
    neoPixelShowTime(timerDuration - timerRuntime);
  } else if (stopwatchRunning) {
    show_time(millis() - stopwatchMillisAtStart);
    neoPixelShowTime(millis() - stopwatchMillisAtStart);
  }

  delay(10);
}



void printValues() {
  // Serial.print("Temperature = ");
  // Serial.print(bme.readTemperature());
  // Serial.println(" *C");

  // // Convert temperature to Fahrenheit
  // /*Serial.print("Temperature = ");
  // Serial.print(1.8 * bme.readTemperature() + 32);
  // Serial.println(" *F");*/

  // Serial.print("Pressure = ");
  // Serial.print(bme.readPressure() / 100.0F);
  // Serial.println(" hPa");

  // Serial.print("Approx. Altitude = ");
  // Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
  // Serial.println(" m");

  // Serial.print("Humidity = ");
  // Serial.print(bme.readHumidity());
  // Serial.println(" %");

  display.clearDisplay();
  display.setCursor(0, 0);  // Start at top-left corner
  display.printf("T: %.2f*C\nP: %.2f\nH: %.2f\nEnc: %d", bme.readTemperature(), bme.readPressure() / 100.0F, bme.readHumidity(), readEncoderValue());
  display.display();
}

boolean isEncoderButtonPushDown(void) {
  if (!digitalRead(ENCODER_BUTTON)) {
    delay(10);  // Debounce
    if (!digitalRead(ENCODER_BUTTON))
      return true;
  }
  return false;
}

void updateEncoder() {
  int MSB = digitalRead(ENCODER_PIN_A);  //MSB = most significant bit
  int LSB = digitalRead(ENCODER_PIN_B);  //LSB = least significant bit
  // Serial.printf("%d %d", MSB, LSB);
  int encoded = (MSB << 1) | LSB;          //converting the 2 pin value to single number
  int sum = (lastEncoded << 2) | encoded;  //adding it to the previous encoded value

  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue--;

  lastEncoded = encoded;  //store this value for next time

  encoderValueChanged = true;
}

// Slightly different, this makes the rainbow equally distributed throughout
void rainbow(int offset) {
  uint16_t i;

  for (i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + offset * 10) & 255));
  }
  strip.show();
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// TIMER related
void show_time(int milliseconds) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (milliseconds < 10) {
    display.setTextSize(4);    // Draw 2X-scale text
    display.setCursor(5, 20);  // Start at top-left corner
    display.println(" 0.00");
    display.display();
    return;
  }

  int hours = (milliseconds / 1000) / 60 / 60;
  milliseconds -= hours * 60 * 60 * 1000;
  int minutes = (milliseconds / 1000) / 60;
  milliseconds -= minutes * 60 * 1000;
  int seconds = (milliseconds / 1000);
  milliseconds -= seconds * 1000;

  String hoursText = String(hours);
  String hoursTextBlank = String(hours);
  if (hours < 10) {
    hoursTextBlank = " " + hoursTextBlank;
    hoursText = "0" + hoursText;
  }

  String minutesText = String(minutes);
  String minutesTextBlank = String(minutes);
  if (minutes < 10) {
    minutesTextBlank = " " + minutesTextBlank;
    minutesText = "0" + minutesText;
  }

  String secondsText = String(seconds);
  String secondsTextBlank = String(seconds);
  if (seconds < 10) {
    secondsTextBlank = " " + secondsTextBlank;
    secondsText = "0" + secondsText;
  }

  milliseconds = milliseconds / 10;
  String millisecondsText = String(milliseconds);
  if (milliseconds < 10) {
    millisecondsText = "0" + millisecondsText;
  }

  if (hours > 0) {
    display.setTextSize(2.5);   // Draw 2X-scale text
    display.setCursor(10, 30);  // Start at top-left corner
    display.println(hoursTextBlank + ":" + minutesText + ":" + secondsText);
  } else if (minutes > 0) {
    display.setTextSize(4);    // Draw 2X-scale text
    display.setCursor(5, 20);  // Start at top-left corner
    display.println(minutesTextBlank + ":" + secondsText);
  } else {
    display.setTextSize(4);    // Draw 2X-scale text
    display.setCursor(5, 20);  // Start at top-left corner
    display.println(secondsTextBlank + "." + millisecondsText);
  }

  display.display();
}

void neoPixelShowTime(int milliseconds) {
  int days = (milliseconds / 1000) / 60 / 60 / 24;
  milliseconds -= days * 24 * 60 * 60 * 1000;
  int hours = (milliseconds / 1000) / 60 / 60;
  milliseconds -= hours * 60 * 60 * 1000;
  int minutes = (milliseconds / 1000) / 60;
  milliseconds -= minutes * 60 * 1000;
  int seconds = (milliseconds / 1000);
  milliseconds -= seconds * 1000;

  float minSecFraction = 24.0 / 60.0;
  float hourFraction = 1.0;

  for (int pixel = 0; pixel < 48; pixel++) {
    float fR = 0.0;
    float fG = 0.0;
    float fB = 0.0;

    if (pixel < 24)  // inside ring (0-23)
    {
      if (pixel < minSecFraction * (seconds + milliseconds / 1000.0)) {
        fR += 0.3;
        fG += 0.4;
        fB += 0.8;
      }
    } else {  // outside ring (24-47)
      if ((pixel-24) < minSecFraction * minutes) {
        fR += 0.4;
        fG += 0.0;
        fB += 0.8;
      }

      if ((pixel-24) < hourFraction * (hours%24)) {
        fR += 0.6;
        fG += 0.0;
        fB += 0.2;
      }
    }
    if (fR > 1)
      fR = 1;
    if (fG > 1)
      fG = 1;
    if (fB > 1)
      fB = 1;

    //    int R = fR * 255;
    //    int G = fG * 255;
    //    int B = fB * 255;

    int R = pow(fR, 2.2) * 255;
    int G = pow(fG, 2.2) * 255;
    int B = pow(fB, 2.2) * 255;
    //Serial.println(String(pixel) + ": " + String(R));
    //strip.setPixelColor(pixel, strip.Color(pgm_read_byte(&gamma8[R]), pgm_read_byte(&gamma8[G]), pgm_read_byte(&gamma8[B])));
    strip.setPixelColor(pixel, strip.Color(R, G, B));
  }

  strip.show();
}

// This is a countdown timer that will run from the start time to 0
void startTimer(int timerDurationIn) {
  timerDuration = timerDurationIn;
  timerMillisAtStart = millis();
  timerRunning = true;
  stopwatchRunning = false;
  stopwatchMillisAtPause = 0;
}

void startStopwatch() {
  stopwatchMillisAtStart = millis();
  timerRunning = false;
  stopwatchRunning = true;
}

void startStopwatchWithStartTime(int startTimeMS) {
  stopwatchMillisAtStart = millis() - startTimeMS;
  timerRunning = false;
  stopwatchRunning = true;
}