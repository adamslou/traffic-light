// ***** 0. Documentation Section *****
// TableTrafficLight.c for Lab 10
// Runs on LM4F120/TM4C123
// Index implementation of a Moore finite state machine to operate a traffic light.  
// Daniel Valvano, Jonathan Valvano
// January 15, 2016

// east/west red light connected to PB5
// east/west yellow light connected to PB4
// east/west green light connected to PB3
// north/south facing red light connected to PB2
// north/south facing yellow light connected to PB1
// north/south facing green light connected to PB0
// pedestrian detector connected to PE2 (1=pedestrian present)
// north/south car detector connected to PE1 (1=car present)
// east/west car detector connected to PE0 (1=car present)
// "walk" light connected to PF3 (built-in green LED)
// "don't walk" light connected to PF1 (built-in red LED)

// ***** 1. Pre-processor Directives Section *****
#include "TExaS.h"
#include "tm4c123gh6pm.h"

// ***** 2. Global Declarations Section *****
#define SENSOR  (*((volatile unsigned long *)0x400243FC))
#define LIGHT   (*((volatile unsigned long *)0x400053FC))
#define WALK_LIGHT	(*((volatile unsigned long *)0x400253FC))
	
struct State {
  unsigned long ledOutput;	// PB5 - PB0
	unsigned long walkOutput; // PF3, PF1
  unsigned long time; 			// delay in 10ms units
  unsigned long next[8];
}; // next state for inputs 0-7

typedef const struct State StateType;
#define goW   0
#define waitW 1
#define goS   2
#define waitS 3
#define walk  4
#define dontWalk1 5
#define dontWalk2 6
#define dontWalk3 7
#define dontWalk4 8
#define dontWalk5 9
#define dontWalk6 10

StateType FSM[11]={
 {0x0C,0x02,100,{waitW,goW,waitW,waitW,waitW,waitW,waitW,waitW}},
 {0x14,0x02,50,{goS,goW,goS,goS,walk,walk,goS,goS}},
 {0x21,0x02,100,{waitS,waitS,goS,waitS,waitS,waitS,waitS,waitS}},
 {0x22,0x02,50,{walk,goW,goS,goW,walk,walk,walk,walk}},
 {0x24,0x08,100,{dontWalk1,dontWalk1,dontWalk1,dontWalk1,walk,dontWalk1,dontWalk1,dontWalk1}},
 {0x24,0x00,20,{dontWalk2,dontWalk2,dontWalk2,dontWalk2,dontWalk2,dontWalk2,dontWalk2,dontWalk2}},
 {0x24,0x02,20,{dontWalk3,dontWalk3,dontWalk3,dontWalk3,dontWalk3,dontWalk3,dontWalk3,dontWalk3}},
 {0x24,0x00,20,{dontWalk4,dontWalk4,dontWalk4,dontWalk4,dontWalk4,dontWalk4,dontWalk4,dontWalk4}},
 {0x24,0x02,20,{dontWalk5,dontWalk5,dontWalk5,dontWalk5,dontWalk5,dontWalk5,dontWalk5,dontWalk5}},
 {0x24,0x00,20,{dontWalk6,dontWalk6,dontWalk6,dontWalk6,dontWalk6,dontWalk6,dontWalk6,dontWalk6}},
 {0x24,0x02,20,{goW,goW,goS,goW,walk,goW,goS,goW}}
};
unsigned long stateIdx;  	// the current state (0-10)
unsigned long input;			// input from the sensors

// FUNCTION PROTOTYPES: Each subroutine defined
void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts

// ***** 3. Subroutines Section *****
void SysTick_Init(void){
  NVIC_ST_CTRL_R = 0;               // disable SysTick during setup
  NVIC_ST_CTRL_R = 0x00000005;      // enable SysTick with core clock
}
// The delay parameter is in units of the 80 MHz core clock. (12.5 ns)
void SysTick_Wait(unsigned long delay){
  NVIC_ST_RELOAD_R = delay-1;  // number of counts to wait
  NVIC_ST_CURRENT_R = 0;       // any value written to CURRENT clears
  while((NVIC_ST_CTRL_R&0x00010000)==0){ // wait for count flag
  }
}
// 800000*12.5ns equals 10ms
void SysTick_Wait10ms(unsigned long delay){
  unsigned long i;
  for(i=0; i<delay; i++){
    SysTick_Wait(800000);  // wait 10ms
  }
}

int main(void){ 
	volatile unsigned long delay;
  TExaS_Init(SW_PIN_PE210, LED_PIN_PB543210,ScopeOff); // activate grader and set system clock to 80 MHz
  SysTick_Init();
  SYSCTL_RCGC2_R |= 0x32;      	// 1) B E F
  delay = SYSCTL_RCGC2_R;      	// 2) no need to unlock
  GPIO_PORTE_AMSEL_R &= ~0x07; 	// 3) disable analog function on PE2-0
  GPIO_PORTE_PCTL_R &= ~0x000000FF; // 4) enable regular GPIO
  GPIO_PORTE_DIR_R &= ~0x07;   	// 5) inputs on PE2-0
  GPIO_PORTE_AFSEL_R &= ~0x07; 	// 6) regular function on PE2-0
  GPIO_PORTE_DEN_R |= 0x07;    	// 7) enable digital on PE2-0
  GPIO_PORTB_AMSEL_R &= ~0x3F; 	// 3) disable analog function on PB5-0
  GPIO_PORTB_PCTL_R &= ~0x00FFFFFF;	// 4) enable regular GPIO
  GPIO_PORTB_DIR_R |= 0x3F;    	// 5) outputs on PB5-0
  GPIO_PORTB_AFSEL_R &= ~0x3F; 	// 6) regular function on PB5-0
  GPIO_PORTB_DEN_R |= 0x3F;    	// 7) enable digital on PB5-0
	GPIO_PORTF_AMSEL_R &= ~0x0A;	// 8) disable analog function on PF3, PF1
	GPIO_PORTF_PCTL_R &= ~0x0000F0F0;	// 9) enable regular GPIO
	GPIO_PORTF_DIR_R |= 0x0A;			// 10) outputs on PF3, PF1
	GPIO_PORTF_AFSEL_R &= ~0x0A;	// 11) regular function on PF3, PF1
	GPIO_PORTF_DEN_R |= 0x0A;			// 12) enable digital on PF3, PF1
//	LIGHT = 0x00;
//	WALK_LIGHT = 0x00;
//	SENSOR = 0x00;
	EnableInterrupts();
  stateIdx = goW;  							// initialize state to a green light for traffic going west
  while(1){
    LIGHT = FSM[stateIdx].ledOutput;  			// set car lights
		WALK_LIGHT = FSM[stateIdx].walkOutput;  // set pedestrian lights
    SysTick_Wait10ms(FSM[stateIdx].time);
    input = SENSOR;     // read sensors
    stateIdx = FSM[stateIdx].next[input];  
  }
}

