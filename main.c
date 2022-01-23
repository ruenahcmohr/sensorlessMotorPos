 /***********************************************************
  *   Program:    sensorless motor position control
  *   Created:    jan 2022 (yea, we are still alive)
  *   Author:     Ruediger Nahc Mohr
  *   Comments:   OOOOOOOOoooooooooooooooo....
  *   Update:    
  *
  *  Atmega 328 (arduino Uno, or pro mini, or not arduino, this is just C)
  *  Run this from the internal oscillator, 8Mhz, no /8
  *    Dont run it as 16Mhz or the motors inductive spike wont finish going away
  *    when the adc is trying to read the backEMF. do't go past stop, don't stop past go. 
  *  
  *
  * PC0    0-5V control signal 
  * PC1    "2.5V" reference from inverting EMF amplifier input
  * PC2    output of EMF amplifier
  *
  * PD5    PWM for cw motor drive
  * PD6    PWM for ccw motor drive
  *
  * PD2    really short pulse if the EMF reading was rejected due to commutation noise.
  *
  * * thats all! *
  *
  *  I used a TA7291AP motor driver. The motor driver must float both sides of the motor when not driving.
  *  The back EMF amplifier is a LMC6482 with a gain of about 35
  *    Ri = 5.1k  Rf = 180k
  *
  *  After a pwm drive pulse that is a maximum of 410us, there is 400us given for the motors 
  *   inductive flyback to settle out (determined by observation) 
  *  Timer 1 triggers the ADC to start reading backEMF values at 810us
  *  First, the reference channel is measured, then about 10 samples are taken of the backEMF.
  *  BackEMF samples are sorted into high and low values. If the difference of these is more than 20
  *  (from observation) then there was a commutator spike and the measurement is discarded.
  *  cause darned if I know how to filter that mess out.
  *  
  *  position is guessed by accumulating the velocity samples at intervals timed by the PWM.
  *  Testing shows my setup to creep in one direction, so I added a fudge factor to compensate for it. (-5)
  *
  *  A proportionate error is calculated to compare the control signal to the guessed position. 
  *  the result is applied to the velocity control.
  *
  *  A proportionate error is calculated to compare the control velocity to the backEMF reading
  *  the result is applied to the motor pwm driver.
  *
  *  This project was inspired by @sudamin on twitter, who did a much better job of it.
  *  Maybe I will be able to improve my version, maybe not.
  *
  *  Copyrights:
  *   Part of this code were copied from everywhere, I never actually typed the word 'for' 
  *     each time is was copied from a different stack exchange post, and some of it wasn't even CODE.
  *     You probably wrote more of this than you would like to take credit for.
  *
  *  Some things that could use improvement:
  *    HAHAHAHA all of it, someone find someone who knows what their doing!
  *
  *  I am @ruenahcmohr on twitter.
  *  If you like this content, please click this ascii subscribe button in your text editor now.
  *   It will not do anything, but you might feel better after.
  *
  *  [SUBSCRIBE] [LIKE] [UNSUBSCRIBE] [DISLIKE] [TAKE CREDIT AND RUN]
  *
  ************************************************************/
#include "main.h"
#include "avrcommon.h"      // I come with MACROS!
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>


#define INPUT 0
#define OUTPUT 1

// pin PD6 (OC0A) arduino 6  used for cw motor pwm
// pin PD5 (OC0B) arduino 5  used for ccw motor pwm
// pin PB1 (OC1A) arduino 9  used for adc go-ahead.
// pin PB2 (OC1B) arduino 10 used for nothing, this line should be deleted.

#define pwm0  OCR0A  
#define pwm1  OCR0B
#define pwm2  OCR1AL
#define pwm3  OCR1BL
 
volatile int      AdcCtrl, AdcRef, AdcHigh, AdcLow; // control input, reference input, high EMF reading and low EMF reading
volatile int      EMFVal;                           // result of EMF guess.
volatile uint8_t  ADCNext;                          // channel control
volatile uint8_t  cycleFlag;                        // cycle flag for calculations
volatile int      EMFPosition;                      // position guess.


void pwmInit    ( void );
void AnalogInit ( void );
 

int main(void) {

  int Verr, Vset, Pset; // velocity error, velocity setpoint, position setpoint

  DDRB = (INPUT << PB0 | INPUT << PB1 |INPUT << PB2 |INPUT << PB3 |INPUT << PB4 |INPUT << PB5 |INPUT << PB6 |INPUT << PB7);
  DDRC = (INPUT << PC0 | INPUT << PC1 |INPUT << PC2 |INPUT << PC3 |INPUT << PC4 |INPUT << PC5 |INPUT << PC6 );
  DDRD = (INPUT << PD0 | INPUT << PD1 |OUTPUT << PD2 |INPUT << PD3 |INPUT << PD4 |OUTPUT << PD5 |OUTPUT << PD6 |INPUT << PD7);  
     
  ADCNext   = 0;
  cycleFlag = 0;
  
  
  pwmInit();   
  AnalogInit();
  
  sei();                                   // start interrupts, they do most of the work here.
 
  pwm0 = 0;                                // forward pwm
  pwm1 = 0;                                // reverse pwm
  
  pwm2 = 101;                              // this is the adc start time, 810us, NO TOUCH!

        
  while(1){  
 
   while (!cycleFlag) NOP(); NOP(); NOP(); // wait for new sample (nops take less time to interupt out of than branches)
   cycleFlag = 0;                          // reset flag
   
   Pset = AdcCtrl-512;                     // offset control reading
   Pset *= 32;                             // scale for position use.
   
   Vset = Pset - EMFPosition;              // calc position error
   Vset /= 6;                              // gain of 1/6
   Vset = limit(Vset, -511, 511);          // limit to +-512
   
   // Vset = AdcCtrl-512;                  // for playing with velocity control
   
   
   Verr = Vset - EMFVal ;                  // calculate velocity error
   Verr *= 4;                              // how much gain is the right amount of gain? I do not know
   
   Verr = limit(Verr, -51, 51);            // limit to 51, this is 410us
   
   if (Verr > 0) {                         // apply drive signal to correct side of motor driver.
     pwm0 = Verr;
     pwm1 = 0;
   } else {
     pwm0 = 0;
     pwm1 = -Verr;
   }
  
  };

}




/*

initialize pwm channels

@16Mhz
/64 = ~1 kHz @ 8 bit
/256 = ~240 hz
/1024 = ~61 Hz


This project we use /64 for about 488Hz
*/
void pwmInit() {

  OCR0A  = 0;                                // clear pwm levels
  OCR0B  = 0;
  
  GTCCR = (1<<TSM)|(0<<PSRASY)|(1<<PSRSYNC); // stop timers


  // set up WGM, clock, and mode for timer 0
  TCCR0A = 1 << COM0A1 |                     /* ** normal polarity */
           0 << COM0A0 |                     /*   this bit 1 for interted, 0 for normal  */
           1 << COM0B1 |                     /* ** normal polarity */
           0 << COM0B0 |                     /*   this bit 1 for interted, 0 for normal  */
	   1 << WGM00 |                      /* fast pwm */
           1 << WGM01  ;
	  
  TCCR0B = 0 << CS02  |                      /* CLKio /64 */
           1 << CS01  |
           1 << CS00  ;
  
  // set up WGM, clock, and mode for timer 1
  TCCR1A = 1 << WGM10 |                      /* fast pwm, 8 bit, no outputs */
           0 << WGM11 ;
           
  TCCR1B = 0 << CS12  |                      /* CLKio /64 */
           1 << CS11  |
           1 << CS10  | 
           1 << WGM12  ;          

  TIMSK1 = (1<<OCIE1A) |(1<<TOIE1);          // enable compare interrupt for channel 1A


  // set all timers to the same value
  TCNT0  = 0; // set timer0 to 0
  TCNT1H = 0; // set timer1 to 0
  TCNT1L = 0; // 

  GTCCR  = 0; // start timers
  
 }
 



ISR( TIMER1_COMPA_vect ) {                   // switch to reading ref and EMF
  ADCNext = 1; 
  
  EMFPosition += (EMFVal - 5) ;              // 5 is a fudge-factor used to correct the zero-velocity 
    
}


ISR(TIMER1_OVF_vect) {                       // switch to reading control signal

  int t;
   
  ADCNext = 0; 
  
  if ((AdcHigh - AdcLow) > 20) {              
   pulsePin(2, PORTD);  
   return;                                   // throw out sample, commutation  (excessive noise) 
  }
  
  t =  AdcHigh;                              // ((ADCH+ADCL)/2) - Ref
  t += AdcLow;
  t >>= 1;
  t -= AdcRef;
  EMFVal = t;
  
  cycleFlag = 1;                             // allow main loop to recalculate
      
}




void AnalogInit (void) {  

  // Activate ADC with Prescaler 
  ADCSRA =  1 << ADEN  |
            1 << ADSC  |                      /* start a converstion, irq will take from here */
            0 << ADATE |
            0 << ADIF  |
            1 << ADIE  |                      /* enable interrupt */
            1 << ADPS2 |
            1 << ADPS1 |
            0 << ADPS0 ;
                        
  ADMUX = (1<<REFS0);                         // channel 0    
   
  AdcCtrl = 0;
  AdcRef  = 0;
  AdcHigh = 0;
  AdcLow  = 0;
}



ISR(ADC_vect) { 
  int t;

  t = ADC;
    
  switch(ADMUX & 0x0F) {                       // find out what channel was read
   case 0:                                     // if channel 0, save value as control
     AdcCtrl = t;  
     if (ADCNext != 0) {                       // if timer triggered, switch to EMF readings next
       ADMUX = 1|(1<<REFS0);
     } 
     
   break;
   
   case 1:                                     // if channel 1, save value as reference 
     AdcRef = t; 
     ADMUX = 2|(1<<REFS0);                     // next read EMF amplifier
     AdcLow = 1023;                            // reset limit detectors
     AdcHigh = 0;
     
   break;    

   case 2:                                     // if 2, save EMF readings (high or low peak)
     if (0) {
     } else if (t < AdcLow) {
       AdcLow = t;
     } else if (t > AdcHigh) {
       AdcHigh = t;
     }
     
     if (ADCNext == 0)  ADMUX = 0|(1<<REFS0);  // if time expires, return to measuring control signal
         
   break;
   
   /*
   case 3:   
   break; 
   
   case 4:   
   break;     

   case 5:   
   break; 
    */ 
  }
  
   ADCSRA |= _BV(ADSC);                        // start next conversion
  
  return;
}




