/*	jreim001_Random_Encounter.c - 7/20/2016 4:03:36 PM
 *	Name & E-mail:  - Joseph Reimbold, jreim001@ucr.edu
 *	CS Login: jreim001
 *	Lab Section: 21
 *	Assignment: Custom Lab Project - Random Encounter
 *	Exercise Description:
 *	Traverse a dungeon and keep going until you can go no further!
 *	
 *	
 *	I acknowledge all content contained herein, excluding template or example 
 *	code, is my own original work.
 */ 

#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include "scheduler.h"
#include "timer.h"
#include "io.c"
#include "5110.c"
#include "controller.c"
#include "screens.h"

// Player structure
struct Player {
	unsigned char hp;		// player's Health Points
	unsigned char att;	// player's Attack Damage
	unsigned char gold;	// player's gold acquired
	unsigned char currentRoom;	// player's current room progress	
};

// Monster structure
struct Monster {
	unsigned char monsterHP;	// monster's Health Points
	unsigned char monsterATT;	// monster's Attack Damage
	unsigned char monsterGold;	// amount of gold monster is holding
};

// shared variables
unsigned char combatScreen[504];	// modifiable combatScreen
unsigned char title, rest, room, combat, victory, gameOver, attack, dodge; // flags for game play status
unsigned char save, load;			// flag to command a save or load
struct Player p1;					// player
struct Monster monster;				// monster
enum Monster_Type {None, Chest, Skeleton, Goblin, Spider, Dragon} monsterType;

unsigned char isMonster;			// monster flag

unsigned char roomGold;				// amount of gold in room, if any

// default values for EEPROM player stats
unsigned char EEMEM currentRoomEE = 1;
unsigned char EEMEM playerHPEE = 20;
unsigned char EEMEM playerATTEE = 2;
unsigned char EEMEM playerGoldEE = 0;

// =====================================================
//  Functions
// =====================================================

// updates the Nokia LCD with screen passed in
void displayCurrentScreen(unsigned char *data)
{
	int pixel_cols = LCD_WIDTH * (LCD_HEIGHT / CHAR_HEIGHT); // <6> means 6 lines on LCD.
	lcd_goto_xy(0, 0);
	
	for(int i=0;i<pixel_cols;i++)
	lcd_col(*(data++));
}

// creates a seed for srand(), will allow each game played to be different
void initrand()
{
	uint32_t tmp;					// placeholder for EEPROM variable
	static uint32_t EEMEM rnd;		// create EEPROM variable

	tmp = eeprom_read_dword(&rnd);	// read EEPROM variable

	if (tmp == 0xffffffUL)			// if there is no value, create one
		tmp = 0xDEADBEEF;
	srand(tmp);
	eeprom_write_dword(&rnd, rand());
}

// determines content of room
void createRoom(){
	unsigned char rnd = rand() % 100 + 1;
	
	// 01-15, room is empty
	if (rnd <= 15){
		monsterType = None;
		roomGold = 0;
	}
		
	// 16-30, treasure chest, 1-10 gold
	if (rnd >= 16 && rnd <= 30){
		monsterType = Chest;
		roomGold = rand() % 10 + 1;
	}
	
	// 31-55, skeleton
	if (rnd >= 31 && rnd <= 55){
		monsterType = Skeleton;
		monster.monsterHP = 2;
		monster.monsterATT = 1;
		monster.monsterGold = rand() % 3;
	}
	
	// 56-75, goblin
	if (rnd >= 56 && rnd <= 75){
		monsterType = Goblin;
		monster.monsterHP = 4;
		monster.monsterATT = 2;
		monster.monsterGold = rand() % 5 + 1;
	}
	
	// 76-90, spider
	if (rnd >= 76 && rnd <= 90){
		monsterType = Spider;
		monster.monsterHP = 8;
		monster.monsterATT = 3;
		monster.monsterGold = rand() % 10 + 1;
	}
	
	// 91-100, dragon
	if (rnd >= 91 && rnd <= 100){
		monsterType = Dragon;
		monster.monsterHP = 16;
		monster.monsterATT = 4;
		monster.monsterGold = rand() % 26 + 25;
	}

}

// =====================================================
//  State Machines
// =====================================================

// Save/Load SM
enum SL_States{SL_Start, SL_Wait, SL_Save, SL_Load};

int SL_Tick(int state){
	switch(state){				//transitions
		case SL_Start:
			state = SL_Wait;
			//load = 0;
			
			break;
		
		case SL_Wait:			
			if (!save && !load)
				state = SL_Wait;
			else if (save && !load)
				state = SL_Save;
				
			else if (load && !save)
				state = SL_Load;
				
			break;
	}
	
	switch(state){				//actions
		case SL_Start:
			break;
		
		case SL_Wait:
			break;
			
		case SL_Save:
			// update EEPROM values if they have changed
			eeprom_update_byte((uint8_t*) &currentRoomEE, p1.currentRoom);
			eeprom_update_byte((uint8_t*) &playerHPEE, p1.hp);
			eeprom_update_byte((uint8_t*) &playerATTEE, p1.att);
			eeprom_update_byte((uint8_t*) &playerGoldEE, p1.gold);
			
			// done saving, reset flag
			save = 0;
			
			break;	
			
		case SL_Load:
			// load EEPROM values to play with
			p1.currentRoom = eeprom_read_byte(&currentRoomEE);
			p1.hp = eeprom_read_byte(&playerHPEE);
			p1.att = eeprom_read_byte(&playerATTEE);
			p1.gold = eeprom_read_byte(&playerGoldEE);
			
			// done loading, reset flag
			load = 0;
			
			break;
	}

	return state;
}

// Nokia SM
enum Nokia_States{Nokia_Start, Nokia_Display};

int Nokia_Tick(int state){
	static unsigned char restCount;
		
	switch(state){				//transitions
		case Nokia_Start:
			state = Nokia_Display;
			restCount = 0;
			break;
		
		case Nokia_Display:
			state = Nokia_Display;
			break;
	}
	
	switch(state){				//actions
		case Nokia_Start:
			break;
		
		case Nokia_Display:
			// following conditionals controls display on Nokia LCD
			if (title)
				displayCurrentScreen(titleScreen);
			else if (rest){
				// controls fire animation on rest screen
				if (restCount == 0)
					displayCurrentScreen(restScreen);
				
				else if (restCount == 1)
					displayCurrentScreen(restScreen2);
					
				else if (restCount == 2)
					displayCurrentScreen(restScreen3);
				
				else if (restCount == 3)
					displayCurrentScreen(restScreen4);
					
				restCount++;
				if (restCount == 3)
					restCount = 0;				
				
			}
			else if (victory)
				displayCurrentScreen(victoryScreen);
			else if (gameOver)
				displayCurrentScreen(gameOverScreen);
			else if (combat){
				
				// controls which monster screen to display
				if (monsterType == Skeleton)
					displayCurrentScreen(skeletonScreen);
				else if (monsterType == Goblin)
					displayCurrentScreen(goblinScreen);
				else if (monsterType == Spider)
					displayCurrentScreen(spiderScreen);
				else if (monsterType == Dragon)
					displayCurrentScreen(dragonScreen);
			}
			
			break;
	}

	return state;
}

// LCD SM
enum LCD_States{LCD_Start, LCD_Title, LCD_Wait, LCD_Room, LCD_Victory, LCD_Rest, LCD_Combat, LCD_GameOver};

int LCD_Tick(int state){
	switch(state){				// transitions
		case LCD_Start:
			state = LCD_Title;
			break;
		
		case LCD_Title:
			state = LCD_Wait;
			break;
			
		case LCD_Wait:
			if (rest)
				state = LCD_Rest;
			else if (gameOver)
				state = LCD_GameOver;
			else if (title)
				state = LCD_Title;
			else if (victory)
				state = LCD_Victory;
			else if (room)
				state = LCD_Room;
			else if (combat)
				state = LCD_Combat;
			break;
			
		case LCD_Rest:
				state = LCD_Wait;
			break;
			
		case LCD_Room:
			state = LCD_Wait;
			break;
			
		case LCD_Combat:
			state = LCD_Wait;
			break;
			
		case LCD_Victory:
			state = LCD_Wait;
			break;
			
		case LCD_GameOver:
			state = LCD_Wait;
			break;
			
	}
	
	switch(state){				// actions
		case LCD_Start:
			break;
		
		case LCD_Title:
			//LCD_DisplayString(1, "Start for new   Select to load");
			LCD_DisplayString(1, "Start for new");
			LCD_Cursor(0);
			break;
			
		case LCD_Wait:
			break;
			
		case LCD_Rest:
			LCD_DisplayString(1, "HP     Gold     ATT    Room    ");
			
			// display player HP
			int i = 4;
			char hp[3];
			itoa(p1.hp, hp, 10);
			LCD_Cursor(i);
			char *p;
			p = hp;
			
			while (*p){
				LCD_WriteData(*p);
				p++;
				i++;
				LCD_Cursor(i);
			}
			
			// display player Gold
			i = 13;
			char gold[4];
			itoa(p1.gold, gold, 10);
			LCD_Cursor(i);
			p = gold;

			while (*p){
				LCD_WriteData(*p);
				p++;
				i++;
				LCD_Cursor(i);
			}
			
			// display player ATT
			i = 21;
			char att[3];
			itoa(p1.att, att, 10);
			LCD_Cursor(i);
			p = att;

			while (*p){
				LCD_WriteData(*p);
				p++;
				i++;
				LCD_Cursor(i);
			}
			
			// display Room
			i = 29;
			char map[3];
			itoa(p1.currentRoom, map, 10);
			LCD_Cursor(i);
			p = map;

			while (*p){
				LCD_WriteData(*p);
				p++;
				i++;
				LCD_Cursor(i);
			}
			
			LCD_Cursor(0);
			
			break;
			
		case LCD_Combat:
			LCD_DisplayString(1, "You HP    ATT   NME HP    ATT   ");

			// display player HP
			i = 8;
			itoa(p1.hp, hp, 10);
			LCD_Cursor(i);
			p = hp;
			
			while (*p){
				LCD_WriteData(*p);
				p++;
				i++;
				LCD_Cursor(i);
			}
			
			// display player ATT
			i = 15;
			itoa(p1.att, att, 10);
			LCD_Cursor(i);
			p = att;

			while (*p){
				LCD_WriteData(*p);
				p++;
				i++;
				LCD_Cursor(i);
			}

			// display monster HP
			i = 24;
			char monHP[3];
			itoa(monster.monsterHP, monHP, 10);
			LCD_Cursor(i);
			p = monHP;

			while (*p){
				LCD_WriteData(*p);
				p++;
				i++;
				LCD_Cursor(i);
			}
			
			// display monster ATT
			i = 31;
			char monATT[3];
			itoa(monster.monsterATT, monATT, 10);
			LCD_Cursor(i);
			p = monATT;

			while (*p){
				LCD_WriteData(*p);
				p++;
				i++;
				LCD_Cursor(i);
			}
			
			LCD_Cursor(0);
			
			break;
			
		case LCD_Room:
				// if no monster, display proper empty room or prompt for combat
				if (monsterType == None || monsterType == Chest)
					LCD_DisplayString(1, "Press B to rest");
				else {
					LCD_DisplayString(1, "Press B to entercombat!");
				}
				
				LCD_Cursor(0);
			break;
			
		case LCD_Victory:
			// display victory LCD screen
			LCD_DisplayString(1, "VICTORY!!! PressB to rest");
			LCD_Cursor(0);
			
			break;
			
		case LCD_GameOver:
			// display game over LCD screen
			LCD_DisplayString(1, "YOU ARE DEAD....Start for new...");
			LCD_Cursor(0);
			
			break;
	}

	return state;
}

// Gameplay SM
enum Gameplay_States{Gameplay_Start, Gameplay_Title, Gameplay_Rest, Gameplay_Room,
					 Gameplay_Victory, Gameplay_Combat, Gameplay_Gameover};

int Gameplay_Tick(int state){
	switch(state){				//transitions
		case Gameplay_Start:
			state = Gameplay_Title;
			title = 1;
			
			break;
		
		case Gameplay_Title:
			if (title)
				state = Gameplay_Title;
			else if (rest){
				state = Gameplay_Rest;
			}
				
			break;
			
		case Gameplay_Rest:
			if (rest)
				state = Gameplay_Rest;
			else if (room){
				state = Gameplay_Room;
				createRoom();
				lcd_clear();
			}
						
			break;
			
		case Gameplay_Room:
			if (room)
				state = Gameplay_Room;
			else if (combat)
				state = Gameplay_Combat;
			else if (rest){
				state = Gameplay_Rest;
				p1.currentRoom++;
			}
			
			break;
			
		case Gameplay_Combat:
			if (combat)
				state = Gameplay_Combat;
			else if (victory)
				state = Gameplay_Victory;
			else if (gameOver)
				state = Gameplay_Gameover;
			
			break;
			
		case Gameplay_Victory:
			if (victory)
				state = Gameplay_Victory;
			else if (rest){
				state = Gameplay_Rest;
				
			}
			
			break;
		
		case Gameplay_Gameover:
			if (gameOver)
				state = Gameplay_Gameover;
			else if (title)
				state = Gameplay_Title;
				
			break;
	}
	
	switch(state){				//actions
		case Gameplay_Start:
			break;
		
		case Gameplay_Title:
			getControllerData();		// check controller input

			// if "Start" button is pressed, begin new game with default stats
			if (buttons[4]){
				p1.currentRoom = 1;
				p1.hp = 20;
				p1.att = 2;
				p1.gold = 0;
				
				// erase save file if starting fresh save
				//save = 1;
				
				title = 0;
				rest = 1;
			}

			// if "Select" button is pressed, load previous saved stats
			else if (buttons[3]){
				//load = 1;
				title = 0;
				rest = 1;
			}
			
			break;
			
		case Gameplay_Rest:
			getControllerData();		// check controller input
			
			// if "Up" is pressed, move to room state which determines the room room contents
			if (buttons[5]){
				rest = 0;
				room = 1;
			}
			
			break;
			
		case Gameplay_Room:
			getControllerData();		// check controller input
			
			// The following conditionals controls display based on what is found in the room
			if (monsterType == None){
				lcd_goto_xy(0,0);
				lcd_str("You found an  empty room.   You rest and  regain some   lost HP.");
			}
			
			else if (monsterType == Chest){
				lcd_goto_xy(0,0);
				lcd_str("You found a room with a treasure chest! You open it and regain some lost HP!");
			}
			
			else if (monsterType == Skeleton){
				lcd_goto_xy(0,0);
				lcd_str("You encounter a skeleton!");
			}
			
			else if (monsterType == Goblin){
				lcd_goto_xy(0,0);
				lcd_str("You encounter a goblin!");
			}
			
			else if (monsterType == Spider){
				lcd_goto_xy(0,0);
				lcd_str("You encounter a giant spider!");
			}
			
			else if (monsterType == Dragon){
				lcd_goto_xy(0,0);
				lcd_str("You encounter a dragon!");
			}
			
			// if "B" is pressed, move to rest if no monster, combat if monster
			if (buttons[1]){
				
				if (monsterType == None || monsterType == Chest){
					// regain 5 hp for finding a room with no monster
					if (p1.hp < 95)
						p1.hp += 5;
					else
						p1.hp = 99;
						
					// add gold from chest, will be 0 if empty room
					p1.gold += roomGold;
					
					room = 0;
					rest = 1;
					
					//if (p1.currentRoom % 5 == 0)
						//save = 1;
				}
				
				else{
					room = 0;
					combat = 1;
				}
			}
			
			break;
		
		case Gameplay_Combat:
			getControllerData();		// check controller input
			
			
			// if "A" button is pressed, attack monster, monster attacks back
			if (buttons[9]){
				
				// player attack
				unsigned char pAttack = rand() % 10;
				
				// critical attack if 8 or 9
				if (pAttack >= 8){
					monster.monsterHP -= 2 * p1.att;
				}
				
				// regular attack if 1-7
				else if (pAttack >= 1 && pAttack <= 7){
					monster.monsterHP -= p1.att;
				}
				
				// miss attack if 0
				else{
					// left intentionally blank as reminder of miss
				}
				
				// monster attack
				unsigned char mAttack = rand() % 10;
				
				// critical attack if 9
				if (mAttack >= 9){
					p1.hp -= 2 * monster.monsterATT;
				}
				
				// regular attack if 2-8
				else if (mAttack >= 2 && pAttack <= 8){
					p1.hp -= monster.monsterATT;
				}
				
				// miss attack if 0 or 1
				else{
					// left intentionally blank as reminder of miss
				}
			}
			
			// if player dies, game over
			if (p1.hp == 0 || p1.hp > 99){
				combat = 0;
				gameOver = 1;
			}

			// if monster dies,  victory
			else if (monster.monsterHP == 0 || monster.monsterHP > 99){
				p1.gold += monster.monsterGold;
				combat = 0;
				victory = 1;
			}
			
			break;
			
		case Gameplay_Victory:
			getControllerData();		// check controller input

			// if "B" button is pressed, move to rest
			if (buttons[1]){
				victory = 0;
				rest = 1;
				p1.currentRoom++;
				
				//if (p1.currentRoom % 5 == 0)
					//save = 1;
			}
			
			break;
		
		case Gameplay_Gameover:
			getControllerData();		// check controller input

			// if "Select" button is pressed, move to title
			if (buttons[4]){
				gameOver = 0;
				title = 1;
			}

			// reset EEPROM values on game over
			eeprom_update_byte((uint8_t*) &currentRoomEE, 1);
			eeprom_update_byte((uint8_t*) &playerHPEE, 20);
			eeprom_update_byte((uint8_t*) &playerATTEE, 2);
			eeprom_update_byte((uint8_t*) &playerGoldEE, 0);
			
			break;
	}

	return state;
}


// =====================================================
//  Main
// =====================================================

int main(void)
{
	DDRB = 0x03; PORTB = 0xFC;	// input on B2-B7, output on B0-B1 for the SNES controller
	DDRC = 0xFF; PORTC = 0x00;  // Initialize C0-C7 as output for the LCD display
	DDRD = 0xFF; PORTD = 0x00;  // Initialize D6-D7 as output for LCD display
	
	// Initialize the NOKIA LCD
	// define the 5 Nokia LCD Data pins: SCE, RST, D/C, DN, SCLK
	Nlcd_init(&PORTD, PD4, &PORTD, PD3, &PORTD, PD2, &PORTD, PD1, &PORTD, PD0);
	
	// initialize 16x2 LCD		
	LCD_init();
	LCD_ClearScreen();
	LCD_Cursor(1);
	
	// seed random number generator
	initrand();
	
	//task periods
	unsigned long int SL_calc = 50;
	unsigned long int Nokia_calc = 50;
	unsigned long int LCD_calc = 50;
	unsigned long int Gameplay_calc = 100;
	
	//Calculating GCD	unsigned long int tmpGCD = 1;
	tmpGCD = findGCD(SL_calc, Nokia_calc);
	tmpGCD = findGCD(tmpGCD, LCD_calc);
	tmpGCD = findGCD(tmpGCD, Gameplay_calc);
	
	//Greatest common divisor for all tasks or smallest time unit for tasks.
	unsigned long int GCD = tmpGCD;
	
	//Recalculate GCD periods for scheduler
	unsigned long int SL_period = SL_calc/GCD;
	unsigned long int Nokia_period = Nokia_calc/GCD;
	unsigned long int LCD_period = LCD_calc/GCD;
	unsigned long int Gameplay_period = Gameplay_calc/GCD;
		
	//Declare an array of tasks
	static task SL_task, Nokia_task, LCD_task, Gameplay_task;
	task *tasks[] = { &SL_task, &Nokia_task, &LCD_task, &Gameplay_task };
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);
	
	//SL task
	SL_task.state = SL_Start;
	SL_task.period = SL_period;
	SL_task.elapsedTime = SL_period;
	SL_task.TickFct = &SL_Tick;
	
	//Nokia task
	Nokia_task.state = Nokia_Start;
	Nokia_task.period = Nokia_period;
	Nokia_task.elapsedTime = Nokia_period;
	Nokia_task.TickFct = &Nokia_Tick;
	
	//LCD task
	LCD_task.state = LCD_Start;
	LCD_task.period = LCD_period;
	LCD_task.elapsedTime = LCD_period;
	LCD_task.TickFct = &LCD_Tick;
	
	//Gameplay task
	Gameplay_task.state = Gameplay_Start;
	Gameplay_task.period = Gameplay_period;
	Gameplay_task.elapsedTime = Gameplay_period;
	Gameplay_task.TickFct = &Gameplay_Tick;
	
	TimerSet(GCD);
	TimerOn();
	
	unsigned short i;
	
	while(1) {
		// Scheduler code
		for ( i = 0; i < numTasks; i++ ) {
			// Task is ready to tick
			if ( tasks[i]->elapsedTime == tasks[i]->period ) {
				// Setting room state for task
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				// Reset the elapsed time for room tick.
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += 1;
		}
		while(!TimerFlag);
		TimerFlag = 0;
	}
	
	return 0;
}