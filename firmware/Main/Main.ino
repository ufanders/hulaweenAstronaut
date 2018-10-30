#include <Event.h>
#include <Timer.h>
#include <Debounce.h>
#include <FastLED.h>
#include "LedControl.h"

//======== FastLED stuff
#define CHIPSET     WS2813
#define DATA_PIN_1     15
#define DATA_PIN_2     18
#define COLOR_ORDER GRB
#define SPI_SPEED     5
#define NUM_LEDS    74
#define BRIGHTNESS  50
#define FRAMES_PER_SECOND 60

bool gReverseDirection = false;

CRGB leds1[NUM_LEDS], leds2[NUM_LEDS]; //two separate LED strip buses
byte coolingValue;
byte sparkingValue;
byte brightnessValue;
CRGBPalette16 gPal0;
CRGBPalette16 gPal1;
CRGBPalette16 gPal2;
CRGBPalette16 gPal3;
CRGBPalette16* gPalCurrent;

//======== UI stuff.
const byte buttons[4] = { 1, 2, 4, 6 };
Debounce Button0(buttons[0]);
Debounce Button1(buttons[1]);
Debounce Button2(buttons[2]);
Debounce Button3(buttons[3]);
bool buttonStates[4] = { false, false, false, false };
byte buttonReg;
const byte buttonLights[4] = { 0, 3, 5, 7 };

const byte toggles[4] = { 8, 9, 16, 14 };
Debounce Toggle0(toggles[0]);
Debounce Toggle1(toggles[1]);
Debounce Toggle2(toggles[2]);
Debounce Toggle3(toggles[3]);
bool toggleStates[4] = { false, false, false, false };
byte toggleReg;

void lampTest(void);
byte toggleValue, buttonValue, buttonLightsValue;
const byte potPin = 10;
short potState;

//======== timer stuff.
Timer t;
int flashEvent;
int tickEvent;
int animationEvents[3];

//======== control stuff
byte colorMode = 0;
byte smokeFlag = 0;
uint32_t timerTicksBase, timerTicks1, timerTicks2;
byte currentProgram;

//======== 7-segment stuff
LedControl lc = LedControl(19,20,21,1); //(MOSI, SCK, CS, Num devices)

//======== Ripple stuff.
int color;
int center = 0;
int step = -1;
int maxSteps = 16;
float fadeRate = 0.8;
int diff;
uint32_t currentBg = random(256);
uint32_t nextBg = currentBg;

//======== Noise stuff.
static uint16_t dist;         // A random number for our noise generator.
uint16_t scale = 30;          // Wouldn't recommend changing this on the fly, or the animation will be really blocky.
uint8_t maxChanges = 48;      // Value for blending between palettes.
 
CRGBPalette16 currentPalette(CRGB::Black);
CRGBPalette16 targetPalette(OceanColors_p);

//======== Lightning stuff.
uint8_t frequency = 50; // controls the interval between strikes
uint8_t flashes = 8; //the upper limit of flashes per strike
unsigned int dimmer = 1;
uint8_t ledstart; // Starting location of a flash
uint8_t ledlen; // Length of a flash

//======== Plasma stuff.
// Use qsuba for smooth pixel colouring and qsubd for non-smooth pixel colouring
#define qsubd(x, b)  ((x>b)?b:0) // Digital unsigned subtraction macro. if result <0, then => 0. Otherwise, take on fixed value.
#define qsuba(x, b)  ((x>b)?x-b:0) // Analog Unsigned subtraction macro. if result <0, then => 0
CRGBPalette16 currentPalette2; // Palette definitions
CRGBPalette16 targetPalette2;
TBlendType currentBlending = LINEARBLEND;

void setup() {
  
  int i = 0;
  
  delay(1000); // sanity delay

  timerTicksBase = 0;
  timerTicks1 = 0;
  timerTicks2 = 0;

  //pinMode(9, OUTPUT); //heartbeat indicator.
  for(i = 0; i < 4; i++)
  {
    pinMode(toggles[i], INPUT_PULLUP);
    pinMode(buttons[i], INPUT_PULLUP);
    pinMode(buttonLights[i], OUTPUT);
  }
  pinMode(potPin, INPUT);

  lc.shutdown(0,false); //wakeup MAX7219
  lc.setIntensity(0,8); //set to medium brightness
  lc.clearDisplay(0); //clear display
  lc.setDigit(0,0,0,false);
  lc.setDigit(0,1,1,false);
  lc.setDigit(0,2,2,false);
  lc.setDigit(0,3,3,false);

  FastLED.addLeds<CHIPSET, DATA_PIN_1, COLOR_ORDER>(leds1, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, DATA_PIN_2, COLOR_ORDER>(leds2, NUM_LEDS).setCorrection( TypicalLEDStrip );
  brightnessValue = BRIGHTNESS;
  FastLED.setBrightness(brightnessValue);

  // This first palette is the basic 'black body radiation' colors,
  // which run from black to red to bright yellow to white.
  //gPal = HeatColors_p;
  
  // These are other ways to set up the color palette for the 'fire'.
  // First, a gradient from black to red to yellow to white -- similar to HeatColors_p
  gPal1 = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Yellow, CRGB::White);
  
  // Second, this palette is like the heat colors, but blue/aqua instead of red/yellow
  gPal2 = CRGBPalette16( CRGB::Black, CRGB::Purple, CRGB::Aqua,  CRGB::White);
  
  // Third, here's a simpler, three-step gradient, from black to red to white
  //   gPal = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::White);

  gPal0 = CRGBPalette16( CRGB::Black, CRGB::White);

  coolingValue = 55;
  sparkingValue = 120;
  colorMode = 0;

  FastLED.clear();

  dist = random16(12345); // A semi-random number for our noise generator

  currentProgram = 0;

  // initialize timer1 
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  OCR1A = 1092; //compare match reg = 60Hz 
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();             // enable all interrupts

  Serial.begin(115200);
}


ISR(TIMER1_COMPA_vect)
{
  //top half of ISR.
  
  timerTicksBase++;

  timerTicks1++;

  if(timerTicksBase == 100) timerTicksBase = 0; //reset timebase.
  
  //digitalWrite(9, digitalRead(9) ^ 1); //toggle heartbeat indicator.
}

 
void loop()
{
  byte changedControls;
  short changedValue;
  bool programChange;
  bool variableChange;
  byte progNum;

  programChange = false;
  variableChange = false;

  t.update(); //update timer.

  changedControls = getControls(0b111);

  if(changedControls & 0b010) //engage selected variable for adjustment.
  {
    toggleReg = toggleStates[0] | (toggleStates[1] << 1) | (toggleStates[2] << 2) | (toggleStates[3] << 3);
    toggleReg = toggleReg & 0x0F;
    Serial.print("toggles=");
    Serial.println(toggleReg, HEX);
  }

  if(changedControls & 0b100) //adjust selected variable.
  {
    Serial.print("pot=");
    Serial.println(potState, HEX);

    switch(currentProgram)
    {
      case 0b0001: //Fire program.
        switch(toggleReg)
        {
          case 1: //LED brightness.
            brightnessValue = map(potState, 0, 1023, 0, 100); //0-100.
            changedValue = brightnessValue;
            FastLED.setBrightness(brightnessValue);
            //variableChange = true;
            break;
    
          case 2: //Fire-effect cooling value.
            coolingValue = map(potState, 0, 1023, 0, 255); //0-255.
            changedValue = coolingValue;
            variableChange = true;
            break;
          
          case 0:
          default:
            break;
        }
        break;

        case 0b0010: //Ripple program.
          switch(toggleReg)
          {
            case 1: //LED brightness.
              brightnessValue = map(potState, 0, 1023, 0, 100); //0-100.
              changedValue = brightnessValue;
              FastLED.setBrightness(brightnessValue);
              variableChange = true;
              break;
            
            case 0:
            default:
              break;
          }
          break;

        default:
          break;
    }

    if(toggleReg)
    {
      //update display with adjusted value.
      lc.setDigit(0,0,(changedValue%10),false);
      lc.setDigit(0,1,((changedValue/10)%10),false);
      lc.setDigit(0,2,((changedValue/100)%10),false);
      lc.setDigit(0,3,(changedValue/1000),false);
    }
  }

  if(changedControls & 0b001) //change program or cycle program mode.
  {
    buttonReg = buttonStates[0] | (buttonStates[1] << 1) | (buttonStates[2] << 2) | (buttonStates[3] << 3);
    buttonReg = ~buttonReg & 0x0F; //invert button logic.
    Serial.print("buttons=");
    Serial.println(buttonReg, HEX);

    //if we have pressed another button, change the program.
    if( (buttonReg != 0b0000) && (currentProgram != buttonReg) )
    {
      Serial.println("change loop");
      
      currentProgram = buttonReg;
      FastLED.clear();
      FastLED.show();
      
      //init program.
      switch(currentProgram)
      {
        case 0b0001:
          progNum = 1;
          OCR1A = 1092; //60Hz refresh.
          break;

        case 0b0010:
          progNum = 2;
          OCR1A = 3276; //20Hz refresh.
          break;

        case 0b0100:
          progNum = 3;
          break;

        case 0b1000:
          progNum = 4;
          break;
          
        default:
          progNum = 0;
          break;
      }

      //display program number.
      lc.setDigit(0,6,progNum,false);
      lc.setChar(0,7,'p',false);
    }
    
    programChange = true;
  }

  //read button states.
  if(programChange)
  {
    //process program changes.
    if(currentProgram == 0b0001 && buttonReg == 0b0001) //Fire program.
    {
      colorMode++;
      if(colorMode > 3) colorMode = 0;
  
      switch(colorMode)
      {
        case 0: //LEDs off.
          FastLED.clear();
          FastLED.show();
          break;
  
        case 1: //Fire.
          coolingValue = 55;
          sparkingValue = 100;
          brightnessValue = 96;
          FastLED.setBrightness(brightnessValue);
          break;
  
        case 2: //MDMA fire.
          coolingValue = 55;
          sparkingValue = 160;
          brightnessValue = 192;
          FastLED.setBrightness(brightnessValue);
          break;
  
        case 3: //FREAK OUT
          coolingValue = 55;
          sparkingValue = 192;
          brightnessValue = 255;
          FastLED.setBrightness(brightnessValue);
          break;

        default:
          break;
      }
    }

    if(currentProgram == 0b0010 && buttonReg == 0b0010) //Ripple program.
    {
      //cycle through different modes.
      
    }

    if(currentProgram == 0b0100 && buttonReg == 0b0100) //Noise program.
    {
      //cycle through different modes.
    }
    
    //change button illumination.
    t.stop(flashEvent);
    for(int i = 0; i < sizeof(buttonLights); i++)
    {
      digitalWrite(buttonLights[i], 0);
    }
    
    if(currentProgram == 0b1000)
    {
      flashEvent = t.oscillate(buttonLights[3], 1000, HIGH);
    }
    else
    {
      flashEvent = t.oscillate(buttonLights[(currentProgram/2)], 1000, HIGH);
    }
    
    programChange = false;
  }
  
  if((timerTicks1 >= 1)) //bottom half of ISR start.
  {
    if(currentProgram == 0b0001) //Fire program.
    {
      if(colorMode != 0)
      {
        random16_add_entropy(random());
    
        if(colorMode == 3) //rotate hue with each frame.
        {
          static uint8_t hue = 0;
          hue++;
          CRGB darkcolor  = CHSV(hue,255,192); // pure hue, three-quarters brightness
          CRGB lightcolor = CHSV(hue,128,255); // half 'whitened', full brightness
          gPal3 = CRGBPalette16( CRGB::Black, darkcolor, lightcolor, CRGB::White);
        }
      
        Fire2012WithPalette(); // run simulation frame, using palette colors
        
        FastLED.show(); // display this frame
      }
    }

    if(currentProgram == 0b0010) //Ripple program.
    {
      ripple();
    }

    if(currentProgram == 0b0100) //Noise program.
    {
      // Taken care of in main loop - uses system software timing.
    }
    
    timerTicks1 = 0; //reset this timer.
  } //bottom half of ISR end.

  //software timed loop start.
  if(currentProgram == 0b0100) //Noise program.
  {
    noise();
    //lightning();
    //plasma();
  } //software timed loop end.

  if(timerTicks2 == 10)
  {
    //TODO: update state of indicators?

    timerTicks2 = 0; //reset this timer.
  }

}

void Fire2012WithPalette()
{
  // Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
  for( int i = 0; i < NUM_LEDS; i++) 
  {
    heat[i] = qsub8( heat[i],  random8(0, ((coolingValue * 10) / NUM_LEDS) + 2));
  }

  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for( int k= NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
  }
  
  // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
  if( random8() < sparkingValue ) {
    int y = random8(7);
    heat[y] = qadd8( heat[y], random8(160,255) );
  }

  // Step 4.  Map from heat cells to LED colors
  for( int j = 0; j < NUM_LEDS; j++) 
  {
    // Scale the heat value from 0-255 down to 0-240
    // for best results with color palettes.
    byte colorindex = scale8( heat[j], 240);

    CRGB color;

    switch(colorMode)
    {
      case 0: //all black.
        color = ColorFromPalette( gPal0, 0);
      break;

      case 1: //fire.
        color = ColorFromPalette( gPal1, colorindex);
      break;

      case 2: //MDMA fire.
        color = ColorFromPalette( gPal2, colorindex);
      break;

      case 3: //FREAK OUT
        color = ColorFromPalette( gPal3, colorindex);
      break;
    }
    
    int pixelnumber;
    if( gReverseDirection ) {
      pixelnumber = (NUM_LEDS-1) - j;
    } else {
      pixelnumber = j;
    }
    
    leds1[pixelnumber] = color;
    leds2[pixelnumber] = color; //we hack in a copy of the first strip data into the second strip.
  }
}

/*
void lampTest(void)
{ 
    int i = 0;
    toggleValue = 0;
    
    for(i = 0; i < 4; i++)
    {
      toggleValue |= digitalRead(toggles[i]);
      toggleValue <<= 1;
    }

    buttonValue = 0;

    for(i = 0; i < 4; i++)
    {
      buttonValue |= digitalRead(buttons[i]);
      buttonValue <<= 1;
    }
  
    //toggleValue = (Toggle0.read() & 0x01) + (Toggle1.read() << 1) + (Toggle2.read() << 2) + (Toggle3.read() << 3);
    //buttonValue = Button0.read() + (Button1.read() << 1) + (Button2.read() << 2) + (Button3.read() << 3);
    buttonLightsValue = 0xFF; //buttonValue;
    
    //lc.setDigit(0,0,toggleValue,false);
    //lc.setDigit(0,1,buttonValue,false);

    for(i = 0; i < 4; i++)
    {
      digitalWrite(buttonLights[i], (buttonLightsValue & 0x01));
      buttonLightsValue >> 1;
    }
}
*/

byte getControls(byte controlsMask)
{
  #define HYSTERESIS_STATIC 25 //crude bump filter.
  #define HYSTERESIS_DYNAMIC 10 //crude noise filter.
  
  byte changed = 0;
  bool currentStates[sizeof(buttonStates)];
  short currentValue;

  if(controlsMask & 0b001) //poll buttons.
  {
      currentStates[0] = Button0.read();
      currentStates[1] = Button1.read();
      currentStates[2] = Button2.read();
      currentStates[3] = Button3.read();

    for(int i=0; i < sizeof(buttonStates); i++)
    {
      if(currentStates[i] != buttonStates[i])
      {
        buttonStates[i] = currentStates[i];
        changed |= 0b001;
      }
    }
  }

  if(controlsMask & 0b010) //poll toggles.
  {
      currentStates[0] = Toggle0.read();
      currentStates[1] = Toggle1.read();
      currentStates[2] = Toggle2.read();
      currentStates[3] = Toggle3.read();

    for(int i=0; i < sizeof(buttonStates); i++)
    {
      if(currentStates[i] != toggleStates[i])
      {
        toggleStates[i] = currentStates[i];
        changed |= 0b010;
      }
    }
  }

  if(controlsMask & 0b100) //poll pot.
  {
    currentValue = analogRead(potPin);
    
    if( (currentValue < potState - HYSTERESIS_DYNAMIC) || (currentValue > potState + HYSTERESIS_DYNAMIC) )
    {
      potState = currentValue;
      changed |= 0b100;
    }
  }
  
  return changed;
}

void ripple() {
 
    if (currentBg == nextBg) {
      nextBg = random(256);
    }
    else if (nextBg > currentBg) {
      currentBg++;
    } else {
      currentBg--;
    }
    for(uint16_t l = 0; l < NUM_LEDS; l++) {
      leds1[l] = CHSV(currentBg, 255, 50);
      leds2[l] = leds1[l];
    }
 
  if (step == -1) {
    center = random(NUM_LEDS);
    color = random(256);
    step = 0;
  }
 
  if (step == 0) {
    leds1[center] = CHSV(color, 255, 255);
    leds2[center] = leds1[center];
    step ++;
  }
  else {
    if (step < maxSteps) {
      //Serial.println(pow(fadeRate,step));
 
      leds1[wrap(center + step)] = CHSV(color, 255, pow(fadeRate, step)*255);
      leds2[wrap(center + step)] = leds1[wrap(center + step)];
      leds1[wrap(center - step)] = CHSV(color, 255, pow(fadeRate, step)*255);
      leds2[wrap(center - step)] = leds1[wrap(center - step)];
      if (step > 3) {
        leds1[wrap(center + step - 3)] = CHSV(color, 255, pow(fadeRate, step - 2)*255);
        leds2[wrap(center + step - 3)] = leds1[wrap(center + step - 3)];
        leds1[wrap(center - step + 3)] = CHSV(color, 255, pow(fadeRate, step - 2)*255);
        leds2[wrap(center - step + 3)] = leds1[wrap(center - step + 3)];
      }
      step ++;
    }
    else {
      step = -1;
    }
  }
 
  LEDS.show();
}

int wrap(int step) {
  if(step < 0) return NUM_LEDS + step;
  if(step > NUM_LEDS - 1) return step - NUM_LEDS;
  return step;
}
 
void one_color_allHSV(int ahue, int abright) {                // SET ALL LEDS TO ONE COLOR (HSV)
  for (int i = 0 ; i < NUM_LEDS; i++ ) {
    leds1[i] = CHSV(ahue, 255, abright);
  }
}

void fillnoise8() {
  
  for(int i = 0; i < NUM_LEDS; i++) {                                      // Just ONE loop to fill up the LED array as all of the pixels change.
    uint8_t index = inoise8(i*scale, dist+i*scale) % 255;                  // Get a value from the noise function. I'm using both x and y axis.
    leds1[i] = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
    leds2[i] = leds1[i];
  }
  dist += beatsin8(10,1, 4);                                               // Moving along the distance (that random number we started out with). Vary it a bit with a sine wave.
                                                                           // In some sketches, I've used millis() instead of an incremented counter. Works a treat.
} // fillnoise8()

void lightning(void)
{
  ledstart = random8(NUM_LEDS);                               // Determine starting location of flash
  ledlen = random8(NUM_LEDS-ledstart);                        // Determine length of flash (not to go beyond NUM_LEDS-1)
  
  for (int flashCounter = 0; flashCounter < random8(3,flashes); flashCounter++)
  {
    if(flashCounter == 0) dimmer = 5;                         // the brightness of the leader is scaled down by a factor of 5
    else dimmer = random8(1,3);                               // return strokes are brighter than the leader
    
    fill_solid(leds1+ledstart,ledlen,CHSV(255, 0, 255/dimmer));
    fill_solid(leds2+ledstart,ledlen,CHSV(255, 0, 255/dimmer));
    FastLED.show(); // Show a section of LEDs
    
    delay(random8(4,10));                                     // each flash only lasts 4-10 milliseconds
    fill_solid(leds1+ledstart,ledlen,CHSV(255,0,0));          // Clear the section of LED's
    fill_solid(leds2+ledstart,ledlen,CHSV(255,0,0)); 
    FastLED.show();
    
    if (flashCounter == 0) delay (150);                       // longer delay until next flash after the leader
    
    delay(50+random8(100));                                   // shorter delay between strokes  
  } // for()
  
  delay(random8(frequency)*100);                              // delay between strikes
}

void plasma() {

  EVERY_N_MILLISECONDS(50) {                                  // FastLED based non-blocking delay to update/display the sequence.
    plasmaInner();
  }

  EVERY_N_MILLISECONDS(100) {
    uint8_t maxChanges = 24; 
    nblendPaletteTowardPalette(currentPalette2, targetPalette2, maxChanges);   // AWESOME palette blending capability.
  }

  EVERY_N_SECONDS(5) {                                 // Change the target palette to a random one every 5 seconds.
    uint8_t baseC = random8();                         // You can use this as a baseline colour if you want similar hues in the next line.
    targetPalette2 = CRGBPalette16(CHSV(baseC+random8(32), 192, random8(128,255)), CHSV(baseC+random8(32), 255, random8(128,255)), CHSV(baseC+random8(32), 192, random8(128,255)), CHSV(baseC+random8(32), 255, random8(128,255)));
  }
}

void plasmaInner() // This is the heart of this program. Sure is short. . . and fast.
{
  int thisPhase = beatsin8(6,-64,64); // Setting phase change for a couple of waves.
  int thatPhase = beatsin8(7,-64,64);

  for (int k=0; k<NUM_LEDS; k++) // For each of the LEDs in the strand, set a brightness based on a wave as follows:
  {
    int colorIndex = cubicwave8((k*23)+thisPhase)/2 + cos8((k*15)+thatPhase)/2;           // Create a wave and add a phase change and add another wave with its own phase change.. Hey, you can even change the frequencies if you wish.
    int thisBright = qsuba(colorIndex, beatsin8(7,0,96));                                 // qsub gives it a bit of 'black' dead space by setting sets a minimum value. If colorIndex < current value of beatsin8(), then bright = 0. Otherwise, bright = colorIndex..

    leds1[k] = ColorFromPalette(currentPalette2, colorIndex, thisBright, currentBlending);  // Let's now add the foreground colour.
    leds2[k] = leds2[k];
  }

  FastLED.show();
}

void noise(void)
{
    EVERY_N_MILLISECONDS(10) 
    {
      nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);  // Blend towards the target palette
      fillnoise8(); // Update the LED array with noise at the new location
    }

    EVERY_N_SECONDS(5)
    {  // Change the target palette to a random one every 5 seconds.
      targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
    }

    LEDS.show();
}

