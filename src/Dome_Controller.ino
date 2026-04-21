//-----------------------------------------------------------------------------------------------
// Servo Expander "Dome Stealth Servo Expander" 
// 
// Jessica Janiuk (thePunderWoman) 1-2023
// Iterated from John Thompson (FrettedLogic)
// "Shamelessly paraphrased from Chris James' foundation"
//--------------------------------------------------------------

#include <Arduino.h>
#include <VarSpeedServo.h>

// Serial command port — hardware Serial (the only port on a Pro Mini)
#define COMMAND_SERIAL Serial
#define COMMAND_BAUD   9600

unsigned long randomHoloNextAt; // When to next call randomHoloMove() (millis)

typedef void (*ScheduledFn)();
ScheduledFn scheduledCallback = nullptr;
unsigned long scheduledCallAt  = 0;

void callAfterDuration(ScheduledFn fn, unsigned long seconds) {
  scheduledCallback = fn;
  scheduledCallAt   = millis() + seconds * 1000;
}

bool sequenceRunning = false;

// Dome panel names for our Arduino pins, USE THESE PHYSICAL PINS FOR SPECIFIC DOME SERVO
// Board Pins #2-#13. Panels marked based on Club Spec Panel numbering. PP=Pie Panels, P=Lower Panels, DT=Dome Topper
#define PP1_SERVO_PIN 2   //  PP1   Pie Panel         
#define PP2_SERVO_PIN 3   //  PP2   Pie Panel         
#define PP5_SERVO_PIN 4   //  PP5   Pie Panel         
#define PP6_SERVO_PIN 5   //  PP6   Pie Panel         
// #define DT_SERVO_PIN 6    //  DT    Dome Topper Panel  
#define P1_SERVO_PIN 7    //  P1    Low Panel        
#define P2_SERVO_PIN 8    //  P2    Low Panel        
#define P3_SERVO_PIN 9    //  P3    Low Panel        
#define P4_SERVO_PIN 10   //  P4    Low Panel        
#define P7_SERVO_PIN 11   //  P7    Low Panel         
#define P10_SERVO_PIN 12  //  P10   Low Panel        
#define P11_SERVO_PIN 6   //  P11   Low Panel
#define P13_SERVO_PIN 13  //  P13  Low Panel 

// Create an array of VarSpeedServo type, containing 5 elements/entries. 
// Note: Arrays are zero indexed. See http://arduino.cc/en/Reference/array

#define NBR_SERVOS 12  // Number of Servos (12)
VarSpeedServo Servos[NBR_SERVOS];

// Note: These reference internal numbering not physical pins for array use

#define PP1 0    //  PP1   Pie Panel         
#define PP2 1    //  PP2   Pie Panel         
#define PP5 2    //  PP5   Pie Panel        
#define PP6 3    //  PP6   Pie Panel         
// #define DT 4    // DT    Dome Topper Panel
#define P1 5    // P1    Low Panel        
#define P2 6    // P2    Low Panel        
#define P3 7    // P3    Low Panel        
#define P4 8    // P4    Low Panel        
#define P7 9    // P7    Low Panel        
#define P10 10  // P10   Low Panel        
#define P11 4   // P11   Low Panel
#define P13 11  // P13   Low Panel  
// #define P11_P13 11  // P11 & P13  Low Panels  

  //  Servos in open position = 2000ms ( 180 degrees )  
//  Servos in closed position = 1100ms ( 0 degrees ) 

#define FIRST_SERVO_PIN 2 //First Arduino Pin for Servos

// Dome Panel Open/Close values, can use servo value typically 1000-2000ms or position 0-180 degress. 
#define PANEL_OPEN 2000     //  (40)  0=open position, 2000 us                          
#define PANEL_CLOSE 1100   //   (170) 180=close position, 1100 us                        
#define PANEL_TINYOPEN 1800  // 90=midway point 1800ms, use for animations etc. 
#define PANEL_HALFWAY 1625  // 90=midway point 1800ms, use for animations etc. 
#define PANEL_PARTOPEN 1450  //Partially Open, approximately 1/4    

#define PANEL_MIN 1100
#define PANEL_MAX 2000
           
// Panel Speed Variables 0-255, higher number equals faster movement
#define SLOWSPEED 75
#define VLOWSPEED 125
#define LOWSPEED 160
#define SPEED 200            
#define FASTSPEED 255       
#define OPENSPEED 230   
#define CLOSESPEED 230

// Variables so program knows where things are
bool PiesOpen=false;
bool AllOpen=false;
bool LowOpen=false;
bool OpenClose=false;

void sendToBody(const char* cmd) {
  String full = String("BD:") + cmd;
  COMMAND_SERIAL.println(full);
}

// Mark a sequence as running. `seconds` is the body's auto-expire safety timeout —
// set it to the expected upper bound of the sequence duration. Body suppresses its
// auto animations while seqon is active.
void beginSequence(unsigned int seconds) {
  sequenceRunning = true;
  String full = String("dome=seqon,") + seconds;
  COMMAND_SERIAL.println(full);
}

void endSequence() {
  COMMAND_SERIAL.println("dome=seqoff");
  sequenceRunning = false;
}

// Sine ease in-out (ported from ReelTwo ServoEasing.h): y = 0.5 * (1 - cos(pi*p))
// Steps a group of servos from `from` to `to` over `durationMs` for an organic feel.
void easeServosSineInOut(const uint8_t* indices, uint8_t count, int from, int to, unsigned int durationMs) {
  const uint8_t steps = 30;
  const unsigned int stepDelay = durationMs / steps;
  for (uint8_t s = 0; s <= steps; s++) {
    float p = (float)s / steps;
    float eased = 0.5 * (1 - cos(p * M_PI));
    int pos = from + (int)((to - from) * eased);
    for (uint8_t i = 0; i < count; i++) {
      Servos[indices[i]].write(pos);
    }
    delay(stepDelay);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  COMMAND_SERIAL.begin(COMMAND_BAUD);   // Start serial port for incoming commands

  COMMAND_SERIAL.println("Stealth Dome Servo Expander");
  sequenceRunning = false;

  COMMAND_SERIAL.println("Ready for serial commands (prefix: DM:)");

  // Schedule first random holo move after 2 seconds
  randomHoloNextAt = millis() + 2000;
}

//------------------------------------------------------
// Serial Command Functions

void readSerial() {
  static char buf[32];
  static uint8_t idx = 0;

  while (COMMAND_SERIAL.available()) {
    char c = COMMAND_SERIAL.read();
    if (c == '\n' || c == '\r') {
      if (idx > 0) {
        buf[idx] = '\0';
        if (strncmp(buf, "DM:", 3) == 0) {
          runCommand(buf + 3);
        }
        idx = 0;
      }
    } else if (idx < sizeof(buf) - 1) {
      buf[idx++] = c;
    }
  }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                                                                                //      
//                                                                         Command Sequences                                                                      //
//                                                                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// Open/Close Pie Panels////////////////////////////////////////////////////////////////////////////////////////

void OpenClosePies() {  // Note: This may seem backwards but the Close command ("if") is first and then the Open ("else")second, see Arduino reference guide

    beginSequence(12);
    //Open or close  All Pie Panels, each command will trigger an open or close command

    COMMAND_SERIAL.println("Pie Panels:");
    
    if (PiesOpen) { // Close the Pie Panels if PiesOpen is true
      COMMAND_SERIAL.println("Closing");
      PiesOpen=false;

      resetHolos();

      //.attach allows servos to operate
      Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX); 
      Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      // Servos[DT].attach(DT_SERVO_PIN,PANEL_MIN,PANEL_MAX);

      sendToBody("HAPPY"); // Play happy vocalization

      //closes panels, "true" statement allows servo to reach position before executing next .write
      Servos[PP1].write(PANEL_CLOSE,SPEED,true); // the "true" uses servo postion to ensure full position prior to next .write line. Good for smooth sequence verses delays
      Servos[PP2].write(PANEL_CLOSE,SPEED,true);
      Servos[PP5].write(PANEL_CLOSE,SPEED,true);
      Servos[PP6].write(PANEL_CLOSE,SPEED,true);
      // Servos[DT].write(PANEL_CLOSE,SPEED,true);            
      
      delay(800);
      // .detach disables servos      
      Servos[PP1].detach();
      Servos[PP2].detach();
      Servos[PP5].detach();
      Servos[PP6].detach();
      // Servos[DT].detach();
      
      COMMAND_SERIAL.println("Closed");
       
    } else { // Open Pie Panels
      COMMAND_SERIAL.println("Opening Pie Panels");
      PiesOpen=true;

      delay(100);

       //.attach allows servos to operate
      Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX); 
      Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      // Servos[DT].attach(DT_SERVO_PIN,PANEL_MIN,PANEL_MAX);

      sendToBody("HAPPY"); // Play happy vocalization

      for (int i=0; i<2; i++) { 
        // open pies in wave
       
        Servos[PP2].write(PANEL_OPEN,FASTSPEED,true);
        Servos[PP1].write(PANEL_OPEN,FASTSPEED,true);
        Servos[PP6].write(PANEL_OPEN,FASTSPEED,true);
        Servos[PP5].write(PANEL_OPEN,FASTSPEED,true);
  
        //  Servos[DT].write(PANEL_OPEN,FASTSPEED,true);
       
        // close pies in opposite wave
       
        Servos[PP5].write(PANEL_CLOSE,FASTSPEED,true);
        Servos[PP6].write(PANEL_CLOSE,FASTSPEED,true);
        Servos[PP1].write(PANEL_CLOSE,FASTSPEED,true);
        Servos[PP2].write(PANEL_CLOSE,FASTSPEED,true);
        //  Servos[DT].write(PANEL_CLOSE,FASTSPEED,true);

        // reopen pies
        Servos[PP2].write(PANEL_OPEN,FASTSPEED,true);
        Servos[PP1].write(PANEL_OPEN,FASTSPEED,true);
        Servos[PP6].write(PANEL_OPEN,FASTSPEED,true);
        Servos[PP5].write(PANEL_OPEN,FASTSPEED,true);
        //  Servos[DT].write(PANEL_OPEN,FASTSPEED,true);
      }

      delay(1000);
      // .detach disables servos      
      Servos[PP1].detach();
      Servos[PP2].detach();
      Servos[PP5].detach();
      Servos[PP6].detach();
      // Servos[DT].detach();

      COMMAND_SERIAL.println("Opened Pies");
    }
    // end "for" loop

    endSequence();
}


// Open/Close Low panels///////////////////////////////////////////////////////////////////////////////////

void OpenCloseLow() {

    beginSequence(15);

    //Open or close  Low Panels
    
    COMMAND_SERIAL.println("Low Panels: ");
    if (LowOpen) { // Close the Low Panels
      COMMAND_SERIAL.println("Closing");   
      LowOpen=false;

      resetHolos();

      // attach servos 
      Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX); // add servo min and max to limit travel if needed or expand range, normally 1000-2000
      Servos[P2].attach(P2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P3].attach(P3_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P4].attach(P4_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P7].attach(P7_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P10].attach(P10_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P11].attach(P11_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P13].attach(P13_SERVO_PIN,PANEL_MIN,PANEL_MAX);

      sendToBody("HAPPY"); // Play happy vocalization

      Servos[P4].write(PANEL_CLOSE,SPEED,true);
      Servos[P2].write(PANEL_CLOSE,SPEED,true);
      Servos[P1].write(PANEL_CLOSE,SPEED,true);
      Servos[P3].write(PANEL_CLOSE,SPEED,true);
      Servos[P11].write(PANEL_CLOSE,SPEED,true);
      Servos[P13].write(PANEL_CLOSE,SPEED,true);
      Servos[P7].write(PANEL_CLOSE,SPEED,true);
      Servos[P10].write(PANEL_CLOSE,SPEED,true);
     
      
      delay(1000);
      // Detach from the Low panels      

      Servos[P1].detach();
      Servos[P2].detach();
      Servos[P3].detach();
      Servos[P4].detach();
      Servos[P7].detach();
      Servos[P10].detach();
      Servos[P11].detach();
      Servos[P13].detach();
      
      COMMAND_SERIAL.println("Closed");
       
    } else { // Open Low Panels
      COMMAND_SERIAL.println("Lows Opening");
      LowOpen=true;

      COMMAND_SERIAL.println("*RD01"); // F102 (Front HP RC L/R) - no RC equivalent in AstroPixelsPlus, using random move

      // attach servos
      Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P2].attach(P2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P3].attach(P3_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P4].attach(P4_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P7].attach(P7_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P10].attach(P10_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P11].attach(P11_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P13].attach(P13_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      
      sendToBody("HAPPY"); // Play happy vocalization

       // Wave these panels in order specified below using "for" loop to repeat
      for (int i=0; i<2; i++) { 

        // P1,(P11,P13,P10) P2,P3,P4,P7
        // open
        Servos[P1].write(PANEL_OPEN,SPEED,true);
        Servos[P11].write(PANEL_OPEN,SPEED); // don't wait for this panel to open fully prior to next movement
        Servos[P13].write(PANEL_OPEN,SPEED); // don't wait for this panel to open fully prior to next movement
        Servos[P10].write(PANEL_OPEN,SPEED);     // don't wait for this panel to open fully prior to next movement
        Servos[P2].write(PANEL_OPEN,SPEED,true);
        Servos[P3].write(PANEL_OPEN,SPEED,true); 
        Servos[P4].write(PANEL_OPEN,SPEED,true); 
        Servos[P7].write(PANEL_OPEN,SPEED,true); 
      
        // P7,P4,P3,P2,P1,P11,P13,P10
        //close
        Servos[P7].write(PANEL_CLOSE,SPEED,true);
        Servos[P4].write(PANEL_CLOSE,SPEED,true);
        Servos[P3].write(PANEL_CLOSE,SPEED,true);
        Servos[P2].write(PANEL_CLOSE,SPEED,true);
        Servos[P1].write(PANEL_CLOSE,SPEED,true);
        delay(50); // used delay to time opposite dome side panel for better flow
        Servos[P11].write(PANEL_CLOSE,SPEED,true);
        Servos[P13].write(PANEL_CLOSE,SPEED,true);
        delay(50); // used delay to panel for better flow
        Servos[P10].write(PANEL_CLOSE,SPEED,true);
      }

      // write open
      Servos[P10].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P11].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P13].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P1].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P2].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P3].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P4].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P7].write(PANEL_OPEN,FASTSPEED,true);
     

      delay(1000);
      // Detach from the Low panels      

      Servos[P1].detach();
      Servos[P2].detach();
      Servos[P3].detach();
      Servos[P4].detach();
      Servos[P7].detach();
      Servos[P10].detach();
      Servos[P11].detach();
      Servos[P13].detach();
      
      COMMAND_SERIAL.println("Lows Opened");
    }
    endSequence();
}

//Open/Close All Panels////////////////////////////////////////////////////////////////////////////////////////////

void OpenCloseAll () {

    beginSequence(10);
    //Open or close  All Panels

    COMMAND_SERIAL.println("All Panels: ");
    
    if (AllOpen) { // Close all panels
      COMMAND_SERIAL.println("Closing");
      AllOpen=false;     

      sendToBody("HAPPY"); // Play happy vocalization
      //  Attach, write servo (min, max) range to ensure each panel opens and closes properly 

      Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      // Servos[DT].attach(DT_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P2].attach(P2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P3].attach(P3_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P4].attach(P4_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P7].attach(P7_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P10].attach(P10_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P11].attach(P11_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P13].attach(P13_SERVO_PIN,PANEL_MIN,PANEL_MAX);

      // Write pies to close

      // Servos[DT].write(PANEL_CLOSE,SPEED,true);
      Servos[PP2].write(PANEL_CLOSE,SPEED,true);
      Servos[PP6].write(PANEL_CLOSE,SPEED,true);
      Servos[PP5].write(PANEL_CLOSE,SPEED,true);
      Servos[PP1].write(PANEL_CLOSE,SPEED,true);
     
      //Write low panels to close
      Servos[P10].write(PANEL_CLOSE,SPEED,true);
      Servos[P7].write(PANEL_CLOSE,SPEED,true);
      Servos[P4].write(PANEL_CLOSE,SPEED,true);
      Servos[P3].write(PANEL_CLOSE,SPEED,true);
      Servos[P2].write(PANEL_CLOSE,SPEED,true);
      Servos[P1].write(PANEL_CLOSE,SPEED,true);
      Servos[P11].write(PANEL_CLOSE,SPEED,true);
      Servos[P13].write(PANEL_CLOSE,SPEED,true);
     

      delay(500);

      // Detach ALL after close     

      Servos[PP1].detach();
      Servos[PP2].detach();
      Servos[PP5].detach();
      Servos[PP6].detach();
      // Servos[DT].detach();
      Servos[P1].detach();
      Servos[P2].detach();
      Servos[P3].detach();
      Servos[P4].detach();
      Servos[P7].detach();
      Servos[P10].detach();
      Servos[P11].detach();
      Servos[P13].detach();

      COMMAND_SERIAL.println("Closed All Dome");
  
    } else { // Open all Panels
      COMMAND_SERIAL.println("Opening");
      AllOpen=true;

      //  Attach, write servo (min, max) range to ensure each panel opens and closes properly, adjust for your servos
      Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      // Servos[DT].attach(DT_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P2].attach(P2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P3].attach(P3_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P4].attach(P4_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P7].attach(P7_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P10].attach(P10_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P11].attach(P11_SERVO_PIN,PANEL_MIN,PANEL_MAX);
      Servos[P13].attach(P13_SERVO_PIN,PANEL_MIN,PANEL_MAX);

      sendToBody("HAPPY"); // Play happy vocalization

      //Write Pies open
      Servos[PP2].write(PANEL_OPEN,SPEED,true);
      Servos[PP5].write(PANEL_OPEN,SPEED,true);
      // Servos[DT].write(PANEL_OPEN,SPEED,true);
      Servos[PP1].write(PANEL_OPEN,SPEED,true);
      Servos[PP6].write(PANEL_OPEN,SPEED,true);
      
      //Write lows
      Servos[P10].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P11].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P13].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P1].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P2].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P3].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P4].write(PANEL_OPEN,FASTSPEED,true);
      Servos[P7].write(PANEL_OPEN,FASTSPEED,true);
     

      // Wave these panels back and forth like waving hello
      for (int i=0; i<2; i++) {   //Loop x number of times, 2 in this case

        // P1 & P2 twinkle up and down
        Servos[P1].write(PANEL_TINYOPEN,FASTSPEED,true);
        Servos[P1].write(PANEL_OPEN,FASTSPEED);
        delay(80);
        Servos[P2].write(PANEL_OPEN,FASTSPEED,true);
        Servos[P2].write(PANEL_TINYOPEN,FASTSPEED);
        delay(80);
        Servos[P2].write(PANEL_OPEN,FASTSPEED);
        
        
        delay(100);

        //PP1 & PP6 twinkle up and down
        Servos[PP2].write(PANEL_TINYOPEN,FASTSPEED,true);
        Servos[PP2].write(PANEL_OPEN,FASTSPEED,true);
        delay(80);
        Servos[PP5].write(PANEL_TINYOPEN,FASTSPEED,true);
        Servos[PP5].write(PANEL_OPEN,FASTSPEED,true);
      }   

      delay(800);

      // Detach when open     
      Servos[PP1].detach();
      Servos[PP2].detach();
      Servos[PP5].detach();
      Servos[PP6].detach();
      // Servos[DT].detach();
      Servos[P1].detach();
      Servos[P2].detach();
      Servos[P3].detach();
      Servos[P4].detach();
      Servos[P7].detach();
      Servos[P10].detach();
      Servos[P11].detach();
      Servos[P13].detach();

      
      COMMAND_SERIAL.println("Opened All Dome");
    }
    endSequence();
}

void randomHoloMove() {
  sequenceRunning = true;
  COMMAND_SERIAL.println("Randomly Moving Holos");

  COMMAND_SERIAL.println("*RD01");
  COMMAND_SERIAL.println("*RD02");
  COMMAND_SERIAL.println("*RD03");

  sequenceRunning = false;
}

void flutter() {
  beginSequence(10);
  COMMAND_SERIAL.println("Flutter Sequence: Start");

  Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  // Servos[DT].attach(DT_SERVO_PIN);
  Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P2].attach(P2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P3].attach(P3_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P4].attach(P4_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P7].attach(P7_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P10].attach(P10_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P11].attach(P11_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P13].attach(P13_SERVO_PIN,PANEL_MIN,PANEL_MAX);

  Servos[P1].write(PANEL_TINYOPEN,LOWSPEED, true);
  Servos[P2].write(PANEL_TINYOPEN,LOWSPEED, true);
  Servos[P3].write(PANEL_TINYOPEN,LOWSPEED, true);
  delay(50);
  Servos[P4].write(PANEL_TINYOPEN,LOWSPEED, true);
  delay(50);
  Servos[P7].write(PANEL_TINYOPEN,LOWSPEED, true);
  Servos[P10].write(PANEL_TINYOPEN,LOWSPEED, true);
  Servos[P11].write(PANEL_TINYOPEN,LOWSPEED, true);
  Servos[P13].write(PANEL_TINYOPEN,LOWSPEED, true);

  Servos[PP2].write(PANEL_TINYOPEN,LOWSPEED, true);
  Servos[PP1].write(PANEL_TINYOPEN,LOWSPEED, true);
  Servos[PP6].write(PANEL_TINYOPEN,LOWSPEED, true);
  delay(50);
  Servos[PP5].write(PANEL_TINYOPEN,LOWSPEED, true);

  Servos[P1].write(PANEL_CLOSE,LOWSPEED, true);
  Servos[P2].write(PANEL_CLOSE,LOWSPEED, true);
  Servos[P3].write(PANEL_CLOSE,LOWSPEED, true);
  Servos[P4].write(PANEL_CLOSE,LOWSPEED, true);
  Servos[P7].write(PANEL_CLOSE,LOWSPEED, true);
  Servos[P10].write(PANEL_CLOSE,LOWSPEED, true);
  Servos[P11].write(PANEL_CLOSE,LOWSPEED, true);
  Servos[P13].write(PANEL_CLOSE,LOWSPEED, true);

  Servos[PP2].write(PANEL_CLOSE,LOWSPEED, true);
  Servos[PP1].write(PANEL_CLOSE,LOWSPEED, true);
  Servos[PP6].write(PANEL_CLOSE,LOWSPEED, true);
  delay(50);
  Servos[PP5].write(PANEL_CLOSE,LOWSPEED, true);

  delay(500);

  // Detach when open
  Servos[PP1].detach();
  Servos[PP2].detach();
  Servos[PP5].detach();
  Servos[PP6].detach();
  // Servos[DT].detach();
  Servos[P1].detach();
  Servos[P2].detach();
  Servos[P3].detach();
  Servos[P4].detach();
  Servos[P7].detach();
  Servos[P10].detach();
  Servos[P11].detach();
  Servos[P13].detach();

  PiesOpen = false;
  AllOpen = false;
  LowOpen = false;

  COMMAND_SERIAL.println("Flutter Sequence: Complete");
  endSequence();
}

void bloom() {
  beginSequence(8);
  COMMAND_SERIAL.println("Bloom Sequence: Start");

  Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);

  static const uint8_t pies[] = { PP1, PP2, PP5, PP6 };

  // Open all 4 pies together with sine ease in-out
  easeServosSineInOut(pies, 4, PANEL_CLOSE, PANEL_OPEN, 1200);

  // Hold at top
  delay(2000);

  // Wiggle 3x around fully open
  for (uint8_t i = 0; i < 3; i++) {
    easeServosSineInOut(pies, 4, PANEL_OPEN, 1900, 180);
    easeServosSineInOut(pies, 4, 1900, PANEL_OPEN, 180);
  }

  // Settle
  delay(1000);

  // Snap closed (no ease)
  Servos[PP1].write(PANEL_CLOSE, SPEED, true);
  Servos[PP2].write(PANEL_CLOSE, SPEED, true);
  Servos[PP5].write(PANEL_CLOSE, SPEED, true);
  Servos[PP6].write(PANEL_CLOSE, SPEED, true);

  delay(500);

  Servos[PP1].detach();
  Servos[PP2].detach();
  Servos[PP5].detach();
  Servos[PP6].detach();

  PiesOpen = false;

  COMMAND_SERIAL.println("Bloom Sequence: Complete");
  endSequence();
}

void scream() {
  beginSequence(15);
  COMMAND_SERIAL.println("Scream Sequence: Start");

  COMMAND_SERIAL.println("*SC00"); // A007C - short circuit all HPs (front)
  COMMAND_SERIAL.println("*HW01"); // Wags the holoprojectors 5 times
  COMMAND_SERIAL.println("*HW02");
  COMMAND_SERIAL.println("*HW03");
  COMMAND_SERIAL.println("4T5");
  COMMAND_SERIAL.println("5T5");
  COMMAND_SERIAL.println("0T4");
  sendToBody("SCREAM");

  // TODO: Add dome servo animations
  COMMAND_SERIAL.println("Opening");
  AllOpen=true;

  //  Attach, write servo (min, max) range to ensure each panel opens and closes properly, adjust for your servos
  Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  // Servos[DT].attach(DT_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P2].attach(P2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P3].attach(P3_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P4].attach(P4_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P7].attach(P7_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P10].attach(P10_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P11].attach(P11_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P13].attach(P13_SERVO_PIN,PANEL_MIN,PANEL_MAX);

  //Write Pies open
  Servos[PP2].write(PANEL_OPEN,SPEED);
  Servos[PP5].write(PANEL_OPEN,SPEED);
  Servos[PP1].write(PANEL_OPEN,SPEED);
  Servos[PP6].write(PANEL_OPEN,SPEED);
  
  //Write lows
  Servos[P10].write(PANEL_OPEN,FASTSPEED);
  Servos[P11].write(PANEL_OPEN,FASTSPEED);
  Servos[P13].write(PANEL_OPEN,FASTSPEED);
  Servos[P1].write(PANEL_OPEN,FASTSPEED);
  Servos[P2].write(PANEL_OPEN,FASTSPEED);
  Servos[P3].write(PANEL_OPEN,FASTSPEED);
  Servos[P4].write(PANEL_OPEN,FASTSPEED);
  Servos[P7].write(PANEL_OPEN,FASTSPEED);
  

  // randomly flutter panels
  randomSeed(analogRead(0));
  for (int i=0; i<10; i++) {
    long randomPanel = random(12);
    Servos[randomPanel].write(PANEL_HALFWAY,FASTSPEED,true);
    Servos[randomPanel].write(PANEL_OPEN,FASTSPEED);
    delay(80);
    Servos[randomPanel].write(PANEL_HALFWAY,FASTSPEED,true);
    Servos[randomPanel].write(PANEL_OPEN,FASTSPEED);
    delay(100);
  }   

  delay(800);

  // Detach when open     
  Servos[PP1].detach();
  Servos[PP2].detach();
  Servos[PP5].detach();
  Servos[PP6].detach();
  // Servos[DT].detach();
  Servos[P1].detach();
  Servos[P2].detach();
  Servos[P3].detach();
  Servos[P4].detach();
  Servos[P7].detach();
  Servos[P10].detach();
  Servos[P11].detach();
  Servos[P13].detach();
  
  COMMAND_SERIAL.println("Opened All Dome");


  waitTime(2000); // wait 2 seconds before resetting

  COMMAND_SERIAL.println("Closing");
  AllOpen=false;     

  sendToBody("HAPPY"); // Play happy vocalization

  //  Attach, write servo (min, max) range to ensure each panel opens and closes properly 

  Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P2].attach(P2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P3].attach(P3_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P4].attach(P4_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P7].attach(P7_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P10].attach(P10_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P11].attach(P11_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P13].attach(P13_SERVO_PIN,PANEL_MIN,PANEL_MAX);

  // Write pies to close

  Servos[PP2].write(PANEL_CLOSE,SPEED);
  Servos[PP6].write(PANEL_CLOSE,SPEED);
  Servos[PP5].write(PANEL_CLOSE,SPEED);
  Servos[PP1].write(PANEL_CLOSE,SPEED);
  
  //Write low panels to close
  Servos[P10].write(PANEL_CLOSE,SPEED);
  Servos[P7].write(PANEL_CLOSE,SPEED);
  Servos[P4].write(PANEL_CLOSE,SPEED);
  Servos[P3].write(PANEL_CLOSE,SPEED);
  Servos[P2].write(PANEL_CLOSE,SPEED);
  Servos[P1].write(PANEL_CLOSE,SPEED);
  Servos[P11].write(PANEL_CLOSE,SPEED);
  Servos[P13].write(PANEL_CLOSE,SPEED);

  delay(500);

  // Detach ALL after close     

  Servos[PP1].detach();
  Servos[PP2].detach();
  Servos[PP5].detach();
  Servos[PP6].detach();
  // Servos[DT].detach();
  Servos[P1].detach();
  Servos[P2].detach();
  Servos[P3].detach();
  Servos[P4].detach();
  Servos[P7].detach();
  Servos[P10].detach();
  Servos[P11].detach();
  Servos[P13].detach();

  COMMAND_SERIAL.println("Closed All Dome");  

  resetHolos();
  resetLogics();
  resetPSIs();

  COMMAND_SERIAL.println("Scream Sequence: Complete");
  endSequence();
}

void overload() {
  beginSequence(12);
  COMMAND_SERIAL.println("Overload Sequence: Start");

  COMMAND_SERIAL.println("*SC00"); // short circuit all HPs
  COMMAND_SERIAL.println("4T4");   // PSI Pro
  COMMAND_SERIAL.println("5T4");   // PSO Pro
  COMMAND_SERIAL.println("0T4");   // astropixels
  sendToBody("OVERLOAD");

  Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P2].attach(P2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P3].attach(P3_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P4].attach(P4_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P7].attach(P7_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P10].attach(P10_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P11].attach(P11_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P13].attach(P13_SERVO_PIN,PANEL_MIN,PANEL_MAX);

  // Spasm burst: panels fling open in scrambled order with tiny stagger
  randomSeed(analogRead(0));
  static const uint8_t scramble[] = { P3,PP2,P10,PP6,P1,P7,PP1,P11,P2,PP5,P13,P4 };
  for (uint8_t i = 0; i < 12; i++) {
    Servos[scramble[i]].write(PANEL_OPEN, FASTSPEED);
    delay(random(10, 35));
  }

  // Chaos loop: ~8 seconds of random panels snapping to random positions,
  // with periodic dome twitches alternating +/- at a random arc up to 20 degrees.
  static const int positions[] = { PANEL_OPEN, PANEL_HALFWAY, PANEL_PARTOPEN, PANEL_CLOSE };
  unsigned long chaosEnd = millis() + 8000;
  unsigned long domeNextAt = millis() + random(600, 1400);
  bool domePositive = true;
  while (millis() < chaosEnd) {
    uint8_t panel = random(12);
    int pos = positions[random(4)];
    Servos[panel].write(pos, FASTSPEED);

    if (millis() >= domeNextAt) {
      int angle = random(1, 21);
      String direction = domePositive ? "dome=+" : "dome=-";
      String full = String(direction) + String(angle);
      COMMAND_SERIAL.println(full);
      domePositive = !domePositive;
      domeNextAt = millis() + random(600, 1400);
    }

    delay(random(40, 180));
  }

  // Slam everything closed
  Servos[PP1].write(PANEL_CLOSE, FASTSPEED);
  Servos[PP2].write(PANEL_CLOSE, FASTSPEED);
  Servos[PP5].write(PANEL_CLOSE, FASTSPEED);
  Servos[PP6].write(PANEL_CLOSE, FASTSPEED);
  Servos[P1].write(PANEL_CLOSE, FASTSPEED);
  Servos[P2].write(PANEL_CLOSE, FASTSPEED);
  Servos[P3].write(PANEL_CLOSE, FASTSPEED);
  Servos[P4].write(PANEL_CLOSE, FASTSPEED);
  Servos[P7].write(PANEL_CLOSE, FASTSPEED);
  Servos[P10].write(PANEL_CLOSE, FASTSPEED);
  Servos[P11].write(PANEL_CLOSE, FASTSPEED);
  Servos[P13].write(PANEL_CLOSE, FASTSPEED);
  delay(1500);

  Servos[PP1].detach();
  Servos[PP2].detach();
  Servos[PP5].detach();
  Servos[PP6].detach();
  Servos[P1].detach();
  Servos[P2].detach();
  Servos[P3].detach();
  Servos[P4].detach();
  Servos[P7].detach();
  Servos[P10].detach();
  Servos[P11].detach();
  Servos[P13].detach();

  resetHolos();
  resetLogics();
  resetPSIs();

  COMMAND_SERIAL.println("Overload Sequence: Complete");
  endSequence();
}

void heart() {
  beginSequence(10); // body auto-expires after 10s — matches resetHolos timer
  COMMAND_SERIAL.println("Heart: Start");

  COMMAND_SERIAL.println("*HPS601"); // rainbow all HPs (front)
  COMMAND_SERIAL.println("*HPS602"); // (rear)
  COMMAND_SERIAL.println("*HPS603"); // (top)
  COMMAND_SERIAL.println("4T7");
  COMMAND_SERIAL.println("@1MYou're");
  COMMAND_SERIAL.println("@2MWonderful");
  COMMAND_SERIAL.println("@3M");
  sendToBody("HEART");

  callAfterDuration(resetHolos, 10);

  COMMAND_SERIAL.println("Heart: Complete");
  endSequence();
}

void alarm() {
  beginSequence(10); // body auto-expires after 10s — matches resetHolos timer
  COMMAND_SERIAL.println("Alarm: Start");

  COMMAND_SERIAL.println("*HPF00312"); // pulse front HP
  COMMAND_SERIAL.println("*HPR00312"); // pulse rear HP
  COMMAND_SERIAL.println("*HPT00312"); // pulse top HP
  COMMAND_SERIAL.println("0T3"); // all logics to alarm
  COMMAND_SERIAL.println("4T3"); // PSI to alarm
  COMMAND_SERIAL.println("5T3"); // PSI to alarm
  sendToBody("ALARM");

  callAfterDuration(resetAll, 10);

  COMMAND_SERIAL.println("Alarm: Complete");
  endSequence();
}

void helloThere() {
  beginSequence(4);
  COMMAND_SERIAL.println("Hello There: Start");
  COMMAND_SERIAL.println("@1MHello");
  COMMAND_SERIAL.println("@2MThere");
  COMMAND_SERIAL.println("@3MGeneral Kenobi");
  sendToBody("HELLO");

  Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P1].write(PANEL_OPEN,SLOWSPEED, true);
  delay(10);
  Servos[P1].write(PANEL_HALFWAY,SLOWSPEED, true);
  delay(10);
  Servos[P1].write(PANEL_OPEN,SLOWSPEED, true);
  delay(10);
  Servos[P1].write(PANEL_HALFWAY,SLOWSPEED, true);
  delay(10);
  Servos[P1].write(PANEL_OPEN,SLOWSPEED, true);
  delay(10);
  Servos[P1].write(PANEL_CLOSE,SPEED, true);
  Servos[P1].detach();

  COMMAND_SERIAL.println("Hello There: Complete");
  endSequence();
}

// Trigger the Leia sequence
void leiaMode() {
  beginSequence(36); // body auto-expires after 36s — matches |36 duration on HP/PSI/logic commands
  COMMAND_SERIAL.println("Leia Sequence: Start");

  COMMAND_SERIAL.println("@HPS101|36"); // S1 - front HP Leia LED sequence
  COMMAND_SERIAL.println("*OF02|36");   // S1 - rear HP off
  COMMAND_SERIAL.println("*OF03|36");   // S1 - top HP off
  COMMAND_SERIAL.println("4T6|36"); // PSI Pro
  COMMAND_SERIAL.println("5T6|36"); // PSI Pro
  COMMAND_SERIAL.println("0T6|36");    // Astropixels Pro
  sendToBody("LEIA");
  delay(500);

  COMMAND_SERIAL.println("Leia Sequence: Complete");
  endSequence();
}

void resetHolos() {
  COMMAND_SERIAL.println("*RL00");
  COMMAND_SERIAL.println("Holo Projectors reset");
}

void resetLogics() {
  COMMAND_SERIAL.println("0T1");
  COMMAND_SERIAL.println("Astropixels reset");
}

void resetPSIs() {
  COMMAND_SERIAL.println("4T1");
  COMMAND_SERIAL.println("5T1");
  COMMAND_SERIAL.println("PSI Pros reset");
}

void resetBody() {
  sendToBody("RESET");
  COMMAND_SERIAL.println("Body controller reset");
}

//RESET SERVOS, HOLOS, MAGIC PANEL  ================================================================================================

void resetAll() {

  beginSequence(4);

  COMMAND_SERIAL.println("Reset Dome Panels/Holos");

  // Attach
  Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  // Servos[DT].attach(DT_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P2].attach(P2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P3].attach(P3_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P4].attach(P4_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P7].attach(P7_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P10].attach(P10_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P11].attach(P11_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P13].attach(P13_SERVO_PIN,PANEL_MIN,PANEL_MAX);

  // Close
  Servos[PP1].write(PANEL_CLOSE,SPEED);
  Servos[PP2].write(PANEL_CLOSE,SPEED);
  Servos[PP5].write(PANEL_CLOSE,SPEED);
  Servos[PP6].write(PANEL_CLOSE,SPEED);
  // Servos[DT].write(PANEL_CLOSE,SPEED);
  
  
  // Write low panels 
  Servos[P1].write(PANEL_CLOSE,SPEED);
  Servos[P2].write(PANEL_CLOSE,SPEED);
  Servos[P3].write(PANEL_CLOSE,SPEED);
  Servos[P4].write(PANEL_CLOSE,SPEED);
  Servos[P7].write(PANEL_CLOSE,SPEED);
  Servos[P10].write(PANEL_CLOSE,SPEED);
  Servos[P11].write(PANEL_CLOSE,SPEED);
  Servos[P13].write(PANEL_CLOSE,SPEED);


  delay(1000); // wait for servos to close
  // Detach servos

  Servos[PP1].detach();
  Servos[PP2].detach();
  Servos[PP5].detach();
  Servos[PP6].detach();
  // Servos[DT].detach();
  Servos[P1].detach();
  Servos[P2].detach();
  Servos[P3].detach();
  Servos[P4].detach();
  Servos[P7].detach();
  Servos[P10].detach();
  Servos[P11].detach();
  Servos[P13].detach();

  COMMAND_SERIAL.println("Dome Panels Closed,Reset");

  resetHolos();
  resetLogics();
  resetPSIs();
  resetBody();

  endSequence();
}

void cantina() {
  beginSequence(17); // 15s song + close buffer
  COMMAND_SERIAL.println("Cantina: Start");
  sendToBody("CANTINA");

  COMMAND_SERIAL.println("1T13"); // Disco Ball PSI pattern
  COMMAND_SERIAL.println("2T13"); // Disco Ball PSI pattern
  COMMAND_SERIAL.println("3T13"); // Disco Ball PSI pattern
  COMMAND_SERIAL.println("4T13"); // Disco Ball PSI pattern
  COMMAND_SERIAL.println("5T13"); // Disco Ball PSI pattern
  COMMAND_SERIAL.println("*HPS601"); // rainbow all HPs (front)
  COMMAND_SERIAL.println("*HPS602"); // (rear)
  COMMAND_SERIAL.println("*HPS603"); // (top)

  Servos[PP1].attach(PP1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP2].attach(PP2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP5].attach(PP5_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[PP6].attach(PP6_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P1].attach(P1_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P2].attach(P2_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P3].attach(P3_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P4].attach(P4_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P7].attach(P7_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P10].attach(P10_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P11].attach(P11_SERVO_PIN,PANEL_MIN,PANEL_MAX);
  Servos[P13].attach(P13_SERVO_PIN,PANEL_MIN,PANEL_MAX);

  // 130 BPM = ~923ms per 2 beats — matches body controller timing exactly.
  // Pie group A (PP1+PP5) alternates with group B (PP2+PP6).
  // Low group A (P1+P3+P7+P11) alternates with group B (P2+P4+P10+P13).
  //
  // Sync: dome waits 100ms, body waits 88ms. BD:CANTINA takes ~12ms to
  // transmit at 9600 baud, so both loops start at T=100ms from dome's clock.
  const unsigned long BEAT_MS = 923;
  const unsigned long DURATION = 15000;
  waitTime(100);
  bool evenOpen = true;
  unsigned long endTime = millis() + DURATION;

  while (millis() < endTime) {
    if (evenOpen) {
      Servos[PP1].write(PANEL_OPEN,  SPEED);
      Servos[PP5].write(PANEL_OPEN,  SPEED);
      Servos[PP2].write(PANEL_CLOSE, SPEED);
      Servos[PP6].write(PANEL_CLOSE, SPEED);
      Servos[P1].write(PANEL_OPEN,   SPEED);
      Servos[P3].write(PANEL_OPEN,   SPEED);
      Servos[P7].write(PANEL_OPEN,   SPEED);
      Servos[P11].write(PANEL_OPEN,  SPEED);
      Servos[P2].write(PANEL_CLOSE,  SPEED);
      Servos[P4].write(PANEL_CLOSE,  SPEED);
      Servos[P10].write(PANEL_CLOSE, SPEED);
      Servos[P13].write(PANEL_CLOSE, SPEED);
    } else {
      Servos[PP1].write(PANEL_CLOSE, SPEED);
      Servos[PP5].write(PANEL_CLOSE, SPEED);
      Servos[PP2].write(PANEL_OPEN,  SPEED);
      Servos[PP6].write(PANEL_OPEN,  SPEED);
      Servos[P1].write(PANEL_CLOSE,  SPEED);
      Servos[P3].write(PANEL_CLOSE,  SPEED);
      Servos[P7].write(PANEL_CLOSE,  SPEED);
      Servos[P11].write(PANEL_CLOSE, SPEED);
      Servos[P2].write(PANEL_OPEN,   SPEED);
      Servos[P4].write(PANEL_OPEN,   SPEED);
      Servos[P10].write(PANEL_OPEN,  SPEED);
      Servos[P13].write(PANEL_OPEN,  SPEED);
    }
    evenOpen = !evenOpen;
    waitTime(BEAT_MS);
  }

  Servos[PP1].write(PANEL_CLOSE, SPEED, true);
  Servos[PP2].write(PANEL_CLOSE, SPEED, true);
  Servos[PP5].write(PANEL_CLOSE, SPEED, true);
  Servos[PP6].write(PANEL_CLOSE, SPEED, true);
  Servos[P1].write(PANEL_CLOSE,  SPEED, true);
  Servos[P2].write(PANEL_CLOSE,  SPEED, true);
  Servos[P3].write(PANEL_CLOSE,  SPEED, true);
  Servos[P4].write(PANEL_CLOSE,  SPEED, true);
  Servos[P7].write(PANEL_CLOSE,  SPEED, true);
  Servos[P10].write(PANEL_CLOSE, SPEED, true);
  Servos[P11].write(PANEL_CLOSE, SPEED, true);
  Servos[P13].write(PANEL_CLOSE, SPEED, true);

  delay(500);

  Servos[PP1].detach();
  Servos[PP2].detach();
  Servos[PP5].detach();
  Servos[PP6].detach();
  Servos[P1].detach();
  Servos[P2].detach();
  Servos[P3].detach();
  Servos[P4].detach();
  Servos[P7].detach();
  Servos[P10].detach();
  Servos[P11].detach();
  Servos[P13].detach();

  PiesOpen = false;
  AllOpen = false;
  LowOpen = false;
  resetPSIs();
  resetLogics();
  resetHolos();

  COMMAND_SERIAL.println("Cantina: Complete");
  endSequence();
}

//----------------------------------------------------------------------------
//  Delay function
//----------------------------------------------------------------------------
void waitTime(unsigned long waitTime)
{
  unsigned long endTime = millis() + waitTime;
  while (millis() < endTime)
  {}// do nothing
}


void runCommand(const char* cmd)
{
  COMMAND_SERIAL.println("Serial command: ");
  COMMAND_SERIAL.println(cmd);

  if (strcmp(cmd, "RESET") == 0) {
    resetAll();
  } else if (strcmp(cmd, "PIES") == 0) {
    OpenClosePies();
  } else if (strcmp(cmd, "LOW") == 0) {
    OpenCloseLow();
  } else if (strcmp(cmd, "OPENALL") == 0) {
    OpenCloseAll();
  } else if (strcmp(cmd, "LEIA") == 0) {
    leiaMode();
  } else if (strcmp(cmd, "HEART") == 0) {
    heart();
  } else if (strcmp(cmd, "HELLO") == 0) {
    helloThere();
  } else if (strcmp(cmd, "SCREAM") == 0) {
    scream();
  } else if (strcmp(cmd, "FLUTTER") == 0) {
    flutter();
  } else if (strcmp(cmd, "OVERLOAD") == 0) {
    overload();
  } else if (strcmp(cmd, "BLOOM") == 0) {
    bloom();
  } else if (strcmp(cmd, "CANTINA") == 0) {
    cantina();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////Main Loop////////////

void loop() {
  readSerial();

  // Fire any scheduled callback
  if (scheduledCallback != nullptr && millis() >= scheduledCallAt) {
    ScheduledFn fn = scheduledCallback;
    scheduledCallback = nullptr;
    fn();
  }

// Periodically call randomHoloMove() only when no sequence is running
  if (millis() >= randomHoloNextAt && !sequenceRunning) {
    randomHoloMove();
    randomHoloNextAt = millis() + random(30000, 120001);
  }
}
