extern "C" {
  #include "user_interface.h"
}

#include <ESP8266WiFi.h>

#define N 10

volatile int BPM;
volatile int Signal;
volatile int IBI = 600;
volatile boolean Pulse = false;
volatile boolean QS = false;

volatile int Rate[N];
volatile unsigned long CurrBeatTime = 0;
volatile unsigned long LastBeatTime = 0;
volatile int P = 500;
volatile int T = 500;
volatile int Threshold = 512;
volatile int Amplifier = 100;

int PulseSensorPin = 17;
int FadePin = 4;
int FadeRate = 0;

void timer0_ISR (void) {
  noInterrupts();
  Signal = system_adc_read();
  CurrBeatTime = getCurrentTime(); // msec
  unsigned long interval = CurrBeatTime - LastBeatTime;
  
  // hold bottom
  if ((Signal < Threshold) && (interval > (IBI*3) / 5)) {
    if (Signal < T) {
      T = Signal;
      //Serial.println("T:" + String(T));
    }
  }
   
  // hold peak
  if (Signal > Threshold && Signal > P) {
    P = Signal;
    //Serial.println("P:" + String(P));
  }
  
  if (interval > 250 /* ms */) {
    
    // check if Signal is over Threshold
    if ((Signal > Threshold) && !Pulse && (interval > (IBI*3) / 5)) {
      Pulse = true;
      IBI = interval;
      
      if (Rate[0] < 0) { // first time
        Rate[0] = 0;
        LastBeatTime = getCurrentTime();
        setupTimer(10);
        noInterrupts();
        return;
      } else if (Rate[0] == 0) {  // second time
        for (int i = 0; i < N; ++i) {
          Rate[i] = IBI;
        }
      }
      
      word running_total = 0;     
      for (int i = 0; i < N-1; ++i) {
        Rate[i] = Rate[i+1];
        running_total += Rate[i];
      }
      
      Rate[N-1] = IBI;
      running_total += IBI;
      running_total /= N;
      BPM = 60000 / running_total;
      QS = true;
      LastBeatTime = getCurrentTime();
    }
  }
  
  // check if Signal is under Threshold
  if ((Signal < Threshold) && Pulse) {
    Pulse = false;
    Amplifier = P - T;
    Threshold = Amplifier / 2 + T; // revise Threshold
    P = Threshold;
    T = Threshold;
  }
  
  // check if no Signal is over 2.5 sec
  if (interval > 2500 /* ms */) {
    Threshold = 512;
    P = 500;
    T = 500;
    LastBeatTime = getCurrentTime();
    for (int i = 0; i < N; ++i) {
      Rate[i] = -1;
    }
  }
  setupTimer(10);
  interrupts();
}

void setupTimer(int m /* msec */) {
  timer0_isr_init();
  timer0_attachInterrupt(timer0_ISR);
  timer0_write(ESP.getCycleCount() + 80000L * m); // 80MHz/1000 == 1msec
}

unsigned long getCurrentTime() {
  return ESP.getCycleCount() / 80000L;
}

void setup() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  
  pinMode(FadePin, OUTPUT);
  analogWriteRange(255);
  Serial.begin(115200); 
  noInterrupts();
  setupTimer(10);
  interrupts();
  LastBeatTime = getCurrentTime(); // msec
}

void loop() {
  if (QS) {
    FadeRate = 255; 
    Serial.print("BPM: ");
    Serial.println(BPM);
    QS = false;
  }
  
  FadeRate -= 15;
  FadeRate = constrain(FadeRate, 0, 255);
  analogWrite(FadePin, FadeRate);
  delay(20);
}
