// defines for setting and clearing register bits
#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))  //
#endif                                              //this code for adjusting prescale taken from http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1208715493/11 - thanks jmknapp


int notes=14;          // set up number of notes

//define pins

int buttonsOn=0;       //1 is tx for midi
int lasersOn=2;        //pin 2 drives lasers via transistor
int harpReset=3;       //not used
int buttonNoteDown=4;  //buttons are connected to these pins
int buttonNoteUp=5;    //  
int buttonOctDown=6;   //
int buttonOctUp=7;     //
int buttonScaleDown=8; //
int buttonScaleUp=9;   //
int multS0=10;         //multiplexer control pin 0
int multS1=11;         //multiplexer control pin 1
int multS2=12;         //multiplexer control pin 2
int multS3=13;         //multiplexer control pin 3     

int Uthreshold[14];    //array of upper threshold values
int Lthreshold[14];    //array of lower threshold values
int noteTimer[14];     //timer for note triggering

boolean notePlaying[14];   //note states array
int buttonActive[14];      //array for button checks - stores the state of each button: trigger (2), pressed(1) or unpressed(0). trigger is an instantaneous value.

int scales[14][14]={                            //setup scales to switch between (values are semitones relative to the fundamental)
  {0,1,2,3,4,5,6,7,8,9,10,11,12,13},            //chromatic
  {0,2,4,5,7,9,11,12,14,16,17,19,21,23},        //major 
  {0,2,3,5,7,9,10,12,14,15,17,19,21,22},        //minor
  {0,2,5,7,9,12,14,17,19,21,24,26,29,31},       //pentatonic
  {0,7,12,19,24,31,36,43,48,55,60,67,72,79},    //5th Arpeggio
  {0,4,7,12,16,19,24,28,31,36,40,43,48,52},     //major 3rd, 5th arpeggio
  {0,3,7,12,15,19,24,27,31,36,39,43,48,51},     //minor 3rd, 5th arpeggio
  {0,4,7,10,12,16,19,22,24,28,31,34,36,40},     //major 3rd,5th,7th arpeggio
  {0,3,7,10,12,15,19,22,24,27,31,34,36,39},     //minor 3rd,5th,7th arpeggio
  {0,3,5,7,8,10,12,15,17,19,20,22,24,27},       //some kinda 6 note scale
  {0,4,7,11,12,16,19,23,24,28,31,35,36,40},     //major 3rd,5th,major 7th arpeggio
  {0,3,7,11,12,15,19,23,24,27,31,35,36,39},     //minor 3rd,5th,major 7th arpeggio
  {0,3,5,6,7,10,12,15,17,18,19,22,24,27},       //blues!
  {0,10,20,30,39,49,59,69,79,89,98,108,118,127} //CC values
};
boolean isCC[14]={0,0,0,0,0,0,0,0,0,0,0,0,0,1};  //set up array for defining scales as Control Change messages
int scale=0;                                     //starting scale value (default chromatic)
int bnote=0x3C;                                  //starting note value (default middle C)


void setup(){
  // set prescale to 16
  sbi(ADCSRA,ADPS2) ;    //
  cbi(ADCSRA,ADPS1) ;    //
  cbi(ADCSRA,ADPS0) ;    //this code for adjusting prescale taken from http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1208715493/11 - thanks jmknapp  
  
  analogReference(INTERNAL);     //set analog reference to internal (scaled against 1.1V instead of 5V)
  
  //define pin modes
  
  pinMode(buttonsOn, OUTPUT);
  pinMode(lasersOn, OUTPUT);     
  //pinMode(harpReset, OUTPUT);   //needs to be connected through a resistor - not in use
  pinMode(buttonNoteDown, INPUT); 
  pinMode(buttonNoteUp, INPUT); 
  pinMode(buttonOctDown, INPUT); 
  pinMode(buttonOctUp, INPUT); 
  pinMode(buttonScaleDown, INPUT); 
  pinMode(buttonScaleUp, INPUT); 
  pinMode(multS0, OUTPUT); 
  pinMode(multS1, OUTPUT); 
  pinMode(multS2, OUTPUT); 
  pinMode(multS3, OUTPUT);         
  
  digitalWrite(buttonsOn,true);   //power supply for buttons
  calibrate(notes,lasersOn);      //calibrate thresholds
  Serial.begin(31250);            //midi baud rate = 31250
}

void loop() { 
  for(int i=0;i<notes;i++){          
    
    if((multRead(i)<Uthreshold[i])&&(notePlaying[i]==0)){              //check upper threshold on all photodiodes on notes that aren't already playing
      if(noteTimer[i]<7){                                              
        noteTimer[i]++;                                                //if Uthreshold is broken and note is set to off, add to noteTimer, up to 7 cycles
      }
    }
    
    if((multRead(i)>Uthreshold[i])&&(notePlaying[i]==0)){
      noteTimer[i]=-1;                                                //if Uthreshold is de-broken and note is still set to off, reset noteTimer (did not break lower threshold)
    }
    
    if((multRead(i)<Lthreshold[i])&&(notePlaying[i]==0)){        //note on & CC messages
      if (isCC[scale]==false){                                   //differentiating between CC scale and note scale
        int note=bnote+scales[scale][i];                        
        int vel=127-15*noteTimer[i];                             //trial and error value for velocity sensitivity
        midiMessage(0x90, note, vel);                          
      } else {                                                   //CC messages
        int note=scales[scale][i];
        midiMessage(0xB0, 0x73, note);
      }
      //Serial.println(127-20*noteTimer[i]);                          //velocity debug line
      noteTimer[i]=-1;                                                //reset noteTimer
      notePlaying[i]=1;                                               //software-side note values
    }
    
    if((multRead(i)>Lthreshold[i])&&(notePlaying[i]==1)){        //note off messages
      if (isCC[scale]==false){  
        int note=bnote+scales[scale][i];
        midiMessage(0x80, note, 0x45);                    
      }
      notePlaying[i]=0;                                          //software-side note values
    }
  }
  
  if ((buttonquery(buttonNoteDown)==true)&&(bnote>0x00)){midiReset(notes);bnote--;}          //check all buttons and, if triggered carry out necessary actions
  if ((buttonquery(buttonNoteUp)==true)&&(bnote<0x7F)){midiReset(notes);bnote++;}
  if ((buttonquery(buttonOctDown)==true)&&(bnote>0x00)){midiReset(notes);bnote-=12;}
  if ((buttonquery(buttonOctUp)==true)&&(bnote<0x7F)){midiReset(notes);bnote+=12;}
  if ((buttonquery(buttonScaleDown)==true)&&(scale>0)){midiReset(notes);scale--;}
  if ((buttonquery(buttonScaleUp)==true)&&(scale<13)){midiReset(notes);scale++;}
  if ((buttonActive[4]==1)&&(buttonActive[5]==1)&&(buttonActive[8]==1)){bnote=0x3C;scale=0;calibrate(notes,lasersOn);fullMidiReset(notes);} //reset condition buttons 1,2 and 5
}



void midiMessage(int cmd, int pitch, int velocity) {
  Serial.write(cmd); 
  Serial.write(pitch);    
  Serial.write(velocity);

//  Serial.print(cmd);Serial.print("  ");             //for debug
//  Serial.print(pitch);Serial.print("  ");  
//  Serial.println(velocity);
}



void calibrate(int i,int lasers){
  for(int j=0;j<i;j++){  
    int bleh=multRead(i);                      //reads all the PDs once as changing the reference voltage messes up the first few analogReads
  }
  
  for(int j=0;j<i;j++){                          //this function cycles through each note, taking the low and high values of each photodiode (switches the lasers on and off many times too, to demonstrate a reset)
    digitalWrite(lasers,true);                 //do not break the beams while the harp is resetting or calibration won't work for those notes
    delay(40);
    int highval=multRead(j);
    digitalWrite(lasers,false);
    delay(40);
    int lowval=multRead(j);
    Lthreshold[j]=lowval+((highval-lowval)*.25);
    Uthreshold[j]=lowval+((highval-lowval)*.75);
  }
  digitalWrite(lasers,true);    //set lasers on for good
}



//multiplexer reading function
int multRead(int noteVal){      
  PORTB = byte(noteVal*4);
  int result=analogRead(0);
  return result;
}


//function for reading button states - no debouncing, but is easily implemented
boolean buttonquery(int buttonpin){
  int pressed=digitalRead(buttonpin);
  if ((pressed==true)&&(buttonActive[buttonpin]==0)){
    buttonActive[buttonpin]=2;
  }
  if ((pressed==false)&&(buttonActive[buttonpin]==1)){
    buttonActive[buttonpin]=0;
  }
  if ((pressed==true)&&(buttonActive[buttonpin]==2)){
    buttonActive[buttonpin]=1;
    return true;
  } else {
    return false;
  }
}



void midiReset(int j){
  for(int i=0;i<j;i++){
    if(notePlaying[i]==1){       
      int note=bnote+scales[scale][i];
      midiMessage(0x80, note, 0x45);
      notePlaying[i]=0;                                          //this function sends midi off messages for all notes that are playing
    }
  }
}



void fullMidiReset(int j){
  for(int i=0;i<127;i++){      
    midiMessage(0x80, i, 0x45);                    //this function send midi note off messages for all midi notes (on channel 1)
  }
  for(int i=0;i<j;i++){      
    notePlaying[i]=0;   
  }
}
