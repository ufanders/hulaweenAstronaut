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

void doSomething(void* context);

//======== debounce stuff
byte buttons[4] = { 1, 2, 4, 6 };
Debounce Button0(buttons[0]);
Debounce Button1(buttons[1]);
Debounce Button2(buttons[2]);
Debounce Button3(buttons[3]);
bool buttonStates[4] = { false, false, false, false };
byte buttonLights[4] = { 0, 3, 5, 7 };

byte toggles[4] = { 8, 9, 16, 14 };
Debounce Toggle0(toggles[0]);
Debounce Toggle1(toggles[1]);
Debounce Toggle2(toggles[2]);
Debounce Toggle3(toggles[3]);
bool toggleStates[4] = { false, false, false, false };

//======== timer stuff.
Timer t;
int smokeEvent;
int vapeEvent;
int tickEvent;

//======== control stuff
byte en_blower = 15;
byte en_vape = 14;
byte colorMode = 0;
byte smokeFlag = 0;
uint32_t timerTicksBase, timerTicks1, timerTicks2;

//======== 7-segment stuff
LedControl lc = LedControl(19,20,21,1); //(MOSI, SCK, CS, Num devices)

void lampTest(void);
byte toggleValue, buttonValue,buttonLightsValue;

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
  colorMode = 1;

  CRGB colorInit;
  //colorInit = ColorFromPalette( gPal0, 0);
  colorInit = CRGB::Black;
  for(i = 0; i < (NUM_LEDS); i++)
  {
    leds1[i] = colorInit;
  }
  FastLED.show();
  
  //tickEvent = t.every((1000/60), doSomething, (void*)2);


  // initialize timer1 
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1  = 0;

  OCR1A = 1092; //60Hz //31250; // compare match register 16MHz/256/2Hz
  TCCR1B |= (1 << WGM12);   // CTC mode
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt
  interrupts();             // enable all interrupts
  
}


ISR(TIMER1_COMPA_vect)
//void doSomething(void* context)
{
  //top half of ISR.
  
  timerTicksBase++;

  if(1) //timerTicksBase % 100)
  {
    timerTicks1++;
  }

  if(timerTicksBase == 100) timerTicksBase = 0; //reset timebase.
  
  //digitalWrite(9, digitalRead(9) ^ 1); //toggle heartbeat indicator.
}

 
void loop()
{

  //lampTest();

  t.update(); //update timer.
  
  //read pin states.
  if(!Button0.read() && buttonStates[0] == false)
  {
    //LED mode button pushed.
    buttonStates[0] = true;
    
    colorMode++;
    if(colorMode > 3) colorMode = 0;

    switch(colorMode)
    {
      case 0: //LEDs off.
      coolingValue = 55;
      sparkingValue = 192;
      brightnessValue = 0;
      FastLED.setBrightness(brightnessValue);
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
    }
  }

  if(Button0.read())
  {
    buttonStates[0] = false;
  }

  if(!Button1.read() && buttonStates[1] == false)
  {
    //smoke mode button pushed.
    buttonStates[1] = true;

    smokeFlag ^= 1; //toggle smoke flag.

    if(smokeFlag)
    {
      smokeEvent = t.oscillate(en_blower, 15000, HIGH);
      vapeEvent = t.oscillate(en_vape, 15000, HIGH);
    }
    else
    {
      t.stop(smokeEvent);
      t.stop(vapeEvent);

      digitalWrite(en_blower, 0);
      digitalWrite(en_vape, 0);
    }
  }

  if(Button1.read())
  {
    buttonStates[1] = false;
  }
  
  //bottom half of ISR.

  if((timerTicks1 >= 1)) //draw video frame.
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
    
    timerTicks1 = 0; //reset this timer.
  }

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

void lampTest(void)
{ 
int i = 0;
  
  //while(1)
  //{
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
    
  //}
}

