#include <Bounce2.h>

#define BUTTON_TIMEOUT 2000 // 2sec
#define BATTEY_LIGHT_TIMEOUT 5000 //5sec

#define MAX_BRIGHTNESS 128
#define MIN_BRIGHTNESS 0

#define FLASH_SPEED 1000 //For Mode1 and Mode2
#define FLASH_SPEED_MODE_3 500;
#define FLASH_SPEED_MODE_4 500;
#define FLASH_SPEED_MODE_5 200 

#define READ_CHARGE_INTERVAL 10000 //10 sec

// Pin assignments based on the schematic
#define WHITE_LED  23         //(should be 23) GPIO23 for constant white LED on Constant LED PCB
#define RED_LED  26           // in real device GPIO26 for red LED
#define GREEN_LED  27         // GPIO27 for green LED
#define BTN1_PIN  22          // Button 1 (S2) for constant LED control
#define BTN2_PIN  28          // Button 2 (S1) for flashing modes
#define BATTERY_PIN  29       // 29 in actual device
#define ILLUMINATION_LED 21   // GPIO21 for flashing white LED on Main PCB

uint8_t illuminationLEDValue = 0;
bool firstB2Hold = false;

const int batteryLevelPins[] = { 4, 3, 2, 1 };  // BAT_L1 to BAT_L4 GPIO for battery level LEDs
// Battery level thresholds (adjust based on actual readings)
const float batteryFull = 1.85;
const float battery75 = 1.80;
const float battery50 = 1.78;
const float battery25 = 1.75;
const float battery10 = 1.70;
uint64_t batteryStatStart = 0;
uint8_t batteryFlash = 0;

//Buttons
Bounce2::Button button1 = Bounce2::Button();
Bounce2::Button button2 = Bounce2::Button();
bool isPressing1 = false;
bool isPressing2 = false;

uint8_t flashMode = 0 ;
uint8_t flashPin = 0;
uint64_t flashStart = 0;
uint8_t flashState = MAX_BRIGHTNESS;
int flashCount = 1;

uint16_t prevCharge;
bool charging = false;
uint8_t toggleLed[4] = {0, 0, 0, 0};
int speed;
int i = 3; 

void setup() {
  // put your setup code here, to run once:
  Serial1.begin(115200);
  Serial1.println("Hello, Raspberry Pi Pico!");

  pinMode(WHITE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(ILLUMINATION_LED, OUTPUT);

  //set low initiallay
  analogWrite(WHITE_LED, LOW);
  analogWrite(RED_LED, LOW);
  analogWrite(GREEN_LED, LOW);

  analogWrite(ILLUMINATION_LED, illuminationLEDValue);

  button1.attach( BTN1_PIN, INPUT_PULLUP );
  button2.attach( BTN2_PIN, INPUT_PULLUP );

  button1.interval(5);
  button2.interval(5);

  button1.setPressedState(LOW);
  button2.setPressedState(LOW);

  for (int i = 0; i < 4; i++) {
    pinMode(batteryLevelPins[i], OUTPUT);
    digitalWrite(batteryLevelPins[i], LOW);
  }

  prevCharge = analogRead(BATTERY_PIN);
}

void loop() {
  // put your main code here, to run repeatedly:
  button1.update();
  button2.update();

  //Handle Button 1
  if ( button1.read() == LOW && button1.currentDuration() > BUTTON_TIMEOUT && !isPressing1 )
  {
    isPressing1 = true;
    button1LongPress(); // Call long press function
  }
  else if (button1.rose() && !isPressing1)
  {
    isPressing1 = true;
    button1ShortPress();
  }
  if (button1.released())
    isPressing1 = false;

  //Handle Button 2
  if ( button2.read() == LOW && button2.currentDuration() > BUTTON_TIMEOUT && !isPressing2 )
  {
    isPressing2 = true;
    button2LongPress(); // Call long press function
  }
  else if (button2.rose() && !isPressing2)
  {
    isPressing2 = true;
    button2ShortPress();
  }
  if (button2.released())
    isPressing2 = false;

  detectCharging();
  batteryReqHandle();
  handleFlashMode();
  delay(1); // this speeds up the simulation
}


void button1ShortPress()
{
  Serial1.println("Button-1 Short Press Triggred");
  if (illuminationLEDValue == 0)
  {
    //Show battery level for 5 sec
    Serial1.println("Show Battery for 5 sec!");
    batteryStatStart = millis();
    showBatterylevel();
  }
  else
  {
    if (illuminationLEDValue == 255)
      illuminationLEDValue = 255 / 3;
    else
      illuminationLEDValue = illuminationLEDValue + (255 / 3);
    Serial1.println(illuminationLEDValue);
    analogWrite(ILLUMINATION_LED, illuminationLEDValue);
  }
}

void button1LongPress()
{
  Serial1.println("Button-1 Long Press Triggred");
  if (illuminationLEDValue == 0)
  {
    illuminationLEDValue = 255 / 3;
    analogWrite(ILLUMINATION_LED, illuminationLEDValue);
  }
  else
  {
    illuminationLEDValue = 0;
    analogWrite(ILLUMINATION_LED, illuminationLEDValue);
  }
}

void button2ShortPress()
{
  if (!firstB2Hold)
  {
    return;
  }

  initaliseLed();
  flashMode = (flashMode + 1) % 6;
  if (flashMode == 0)
    flashMode = 1;
  Serial1.println("Flash Mode:" + String(flashMode));
  flashStart = millis();
  switch (flashMode)
  {
    case 1:
      flashPin = WHITE_LED;
      analogWrite(flashPin, flashState);
      break;
    case 2:
      flashPin = RED_LED;
      analogWrite(flashPin, flashState);
      break;
    case 3:
      flashPin = GREEN_LED;
      analogWrite(flashPin, flashState);
      break;
    case 4:
      flashPin = WHITE_LED;
      analogWrite(flashPin, flashState);
      break;
    case 5:
      flashPin = WHITE_LED;
      analogWrite(flashPin, flashState);
      break;
  }
}

void button2LongPress()
{
  if (!firstB2Hold)
  {
    firstB2Hold = true;
    button2ShortPress();
    return;
  }
  firstB2Hold = false;
  Serial1.println("Stopping all the blink process!");
  flashMode = 0 ;
  flashPin = 0;
  flashStart = 0;
  initaliseLed();
}

void handleFlashMode()
{
  if (flashMode == 0)
  {
    return;
  }
  else if (flashMode == 3 )
  {
    speed = FLASH_SPEED_MODE_3;
  }
  else if (flashMode == 4 )
  {
    speed = FLASH_SPEED_MODE_4;
  }
  else if (flashMode == 5)
  {
    speed = FLASH_SPEED_MODE_5;
  }
  else
  {
    speed = FLASH_SPEED;
  }

  if (millis() - flashStart >= speed )
  {
    flashCount ++;
    if (flashCount > 0)
      flashState = flashState == MAX_BRIGHTNESS ? MIN_BRIGHTNESS : MAX_BRIGHTNESS;
    analogWrite(flashPin, flashState);
    if (flashMode == 3 && flashCount == 4 )
    {
      flashPin = flashPin == GREEN_LED ? RED_LED : GREEN_LED;
      flashCount = -1;
      flashState = 0;
    }
    else if (flashMode == 4 && flashCount == 4 )
    {
      if (flashPin == WHITE_LED)
        flashPin = RED_LED;
      else if (flashPin == RED_LED)
        flashPin = GREEN_LED;
      else
        flashPin = WHITE_LED;
      flashCount = -2;
      flashState = 0;
    }
    else if (flashMode == 5 && flashCount == 2 )
    {
      if (flashPin == WHITE_LED)
        flashPin = RED_LED;
      else if (flashPin == RED_LED)
        flashPin = GREEN_LED;
      else
        flashPin = WHITE_LED;
      flashCount = 0;
      flashState = 0;
    }
    flashStart = millis();
  }
}

void showBatterylevel()
{
  float batteryVoltage = analogRead(BATTERY_PIN) * (3.3 / 1023.0);
  Serial1.println("Battery Voltage: " + String(batteryVoltage));
  if (batteryVoltage >= battery75)
  {
    batteryStatStart = millis();
    charging = false;
    for (int i = 0; i < 4; i++) {
      digitalWrite(batteryLevelPins[i], HIGH);
    }
    toggleLed[0] = 0;
    toggleLed[1] = 0;
    toggleLed[2] = 0;
    toggleLed[3] = 0;

  }
  else if (batteryVoltage >= battery50)
  {
    digitalWrite(batteryLevelPins[0], LOW);
    digitalWrite(batteryLevelPins[1], HIGH);
    digitalWrite(batteryLevelPins[2], HIGH);
    digitalWrite(batteryLevelPins[3], HIGH);
    toggleLed[0] = batteryLevelPins[0];
    toggleLed[1] = 0;
    toggleLed[2] = 0;
    toggleLed[3] = 0;
  }
  else if (batteryVoltage >= battery25)
  {
    digitalWrite(batteryLevelPins[0], LOW);
    digitalWrite(batteryLevelPins[1], LOW);
    digitalWrite(batteryLevelPins[2], HIGH);
    digitalWrite(batteryLevelPins[3], HIGH);
    toggleLed[0] = batteryLevelPins[0];
    toggleLed[1] = batteryLevelPins[1];
    toggleLed[2] = 0;
    toggleLed[3] = 0;
  }
  else if (batteryVoltage >= battery10)
  {
    digitalWrite(batteryLevelPins[0], LOW);
    digitalWrite(batteryLevelPins[1], LOW);
    digitalWrite(batteryLevelPins[2], LOW);
    digitalWrite(batteryLevelPins[3], HIGH);
    toggleLed[0] = batteryLevelPins[0];
    toggleLed[1] = batteryLevelPins[1];
    toggleLed[2] = batteryLevelPins[2];
    toggleLed[3] = 0;
  }
  else if (batteryVoltage <= battery10)
  {
    digitalWrite(batteryLevelPins[0], LOW);
    digitalWrite(batteryLevelPins[1], LOW);
    digitalWrite(batteryLevelPins[2], LOW);
    digitalWrite(batteryLevelPins[3], HIGH);
    batteryFlash = 1;
    toggleLed[0] = batteryLevelPins[0];
    toggleLed[1] = batteryLevelPins[1];
    toggleLed[2] = batteryLevelPins[2];
    toggleLed[3] = batteryLevelPins[3];
  }
  else
  {
    // Low battery, all LEDs off
    for (int i = 0; i < 4; i++) {
      digitalWrite(batteryLevelPins[i], LOW);
    }
  }
}

void initaliseLed()
{
  analogWrite(WHITE_LED, 0);
  analogWrite(RED_LED, 0);
  analogWrite(GREEN_LED, 0);
  flashCount = 1;
  flashState = MAX_BRIGHTNESS;
}

void batteryReqHandle()
{
  if (batteryStatStart == 0 )
    return;

  if (batteryFlash > 0 && (millis() % FLASH_SPEED == 1))
  {
    batteryFlash ++;
    if (batteryFlash % 2 == 0)
      digitalWrite(batteryLevelPins[3], LOW);
    else
      digitalWrite(batteryLevelPins[3], HIGH);
  }
  if  (millis() - batteryStatStart > BATTEY_LIGHT_TIMEOUT)
  {
    Serial1.println("Turning off the battery Display");
    for (int i = 0; i < 4; i++)
    {
      batteryFlash = 0;
      digitalWrite(batteryLevelPins[i], LOW);
      batteryStatStart = 0;
    }
  }
}

void detectCharging()
{
  if (millis() % READ_CHARGE_INTERVAL == 0 )
  {
    uint16_t currCharge = analogRead(BATTERY_PIN);
    Serial1.println(currCharge);
    if (currCharge > prevCharge)
    {
      Serial1.println("Charging Detected");
      charging = true;
      showBatterylevel();
    }
    else if (currCharge < prevCharge && charging)
    {
      Serial1.println("Charger Removed!");
      charging = false;
      for (int i = 0; i < 4; i++)
      {
        pinMode(batteryLevelPins[i], OUTPUT);
        digitalWrite(batteryLevelPins[i], LOW);
      }
    }
    prevCharge = currCharge;
  }

  if (charging && (millis() % 500 == 0) )
  {
    if (toggleLed[i] != 0) { // Check if the position is not empty
      digitalWrite(toggleLed[i], HIGH);
    }

    i--; // Move to the next LED

    if (i < -1) { // Reset if we've reached the end
      i = 3;

      // Toggle all LEDs off
      for (int j = 0; j < 4; j++) {
        if (toggleLed[j] != 0) { // Check if the position is not empty
          digitalWrite(toggleLed[j], LOW);
        }
      }
    }
  }
}