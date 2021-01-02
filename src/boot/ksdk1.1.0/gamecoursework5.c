#include <stdlib.h>
#include <time.h> 

#include "fsl_misc_utilities.h"
#include "fsl_device_registers.h"
#include "fsl_i2c_master_driver.h"
#include "fsl_spi_master_driver.h"
#include "fsl_rtc_driver.h"
#include "fsl_clock_manager.h"
#include "fsl_power_manager.h"
#include "fsl_mcglite_hal.h"
#include "fsl_port_hal.h"

#include "gpio_pins.h"
#include "SEGGER_RTT.h"
#include "warp.h"

#include "devMMA8451Q.h"
#include "devSSD1331.h"
#include "coursework5.h"

//Initialisation of Variables
extern volatile WarpI2CDeviceState      deviceMMA8451QState;
bool randominit = false;
uint16_t        readSensorRegisterValueLSB;
uint16_t        readSensorRegisterValueMSB;
int16_t         readSensorRegisterValueCombined;
WarpStatus      i2cReadStatus;
uint16_t        menuI2cPullupValue = 32768;
int 		oppmove = 0;
int 		health = 100;
int		opphealth = 100;
int		damagechance = 0;
int 		delay = 2000;

//FUNCTION FOR GETTING RANDOM VALUE FROM ACCELEROMETER TO START
/*For the virtual opponent's random moves, the accelerometer readings are manipulated to provide the random element. However, the first prompt does not have any readings as the user has not made a move yet; therefore, this function simply takes a reading of the accelerometer to give the user a prompt to go for one of three moves (jab, hook, block) */
void oppmoveinit()
{
	enableI2Cpins(menuI2cPullupValue);
	i2cReadStatus = readSensorRegisterMMA8451Q(kWarpSensorOutputRegisterMMA8451QOUT_Y_MSB, 2 /* numberOfBytes */);
	readSensorRegisterValueMSB = deviceMMA8451QState.i2cBuffer[0];
	readSensorRegisterValueLSB = deviceMMA8451QState.i2cBuffer[1];
	readSensorRegisterValueCombined = ((readSensorRegisterValueMSB & 0xFF) << 6) | (readSensorRegisterValueLSB >> 2);
	readSensorRegisterValueCombined = (readSensorRegisterValueCombined ^ (1 << 13)) - (1 << 13);
	int number = readSensorRegisterValueCombined;
	oppmove = number % 3;
	if (oppmove < 0)
	{
		oppmove = oppmove * -1;
	}
	disableI2Cpins();
}

//MAIN FUNCTION LOOP FOR GAME MODE EXECUTION
/*The function begins by making sure both players are on the same health points of 100. Then the prompt is given to the user (gained by using the function above initially), via the screen flashing a specific colour. As soon as the screen goes blank, 40 accelerometer readings are taken every 20 milliseconds and saved to x, y and z data arrays. Then the analysis is conducted whereby these data raays are sifted through to see if they trigger specific checkpoints. This will determine whether the user has jabbed or punched. The jab and punches also require to be a specific strength and fluid motion for them to be recognised, encouraging maximum engagement and technique by the user. The block is evaluated slightly differently whereby the user has to stay within certain bounds with minimal errors. According to which set of flags have been triggered, the appropriate speech/animation is displayed by the virtual opponent. Here is a mapping of the responses (note the 50/50 chances below are again determined using readings from the current accelerometer readings):

- White Flash Screen Prompt 
- Successful User Jab = 'POW' displayed, screen flashes green, user deals 10 damage to opponent. 
- Failed User Jab = 50/50 chance => A) Damage - screen flashes red, user takes 10 damage; B) No Damage - no flash and no damage to either player  

- Blue Flash Screen Prompt
- Successful User Hook = 'OWW' is displayed, screen flashes green twice, user deals 20 damage to opponent.
- Failed User Hook = 50/50 chance => A) Damage - screen flashes red twice user takes 20 damage; B) No Damage - no flash and no damage to either player

- Pink Flash Screen Prompt
- Successful User Block = 'NOOO' is displayed, screen flashes green, user protects health.
- Failed User Block = 50/50 chance => A) Small Damage - screen flashes red, user takes 10 damage; B) Big Damage -  screen flashes red twice, user takes 20 damage;

Then a new value is provided from the accelerometer readings to decide what prompt the user will get next. And before it is displayed, the user will be updated on the health of both players (player's health at the top and opponent's health at the bottom) */

void gamestart()
{
	 uint8_t         length = 40;
	 int16_t         xdata[length];
	 int16_t         ydata[length];
	 int16_t         zdata[length];
	
 	 //Check for previous death and reset parameters
	 if (health <= 0 || opphealth <= 0)
	 {
		 health = 100;
		 opphealth = 100;
	 }
	                                                        	 
	 while (health > 0 && opphealth > 0)
	 {
	 	//Getting a Random Value from Accelerometer Readings to Start
		if (randominit == false)
	 	{
			oppmoveinit();
			randominit = true;
	 	}

	 	//Fighting Loop 
	 
	 	//Choose Opponent's Move
	 	//Block
	 	if (oppmove == 0)
	 	{
			//Flash Yellow
			fillscreen(0xFF, 0xFF, 0x00);
	       		OSA_TimeDelay(delay);
			clearscreen();	
	 	}
	 	//Jab Opening
	 	else if (oppmove == 1)
	 	{
			//Flash White
			fillscreen(0xFF, 0xFF, 0xFF);
			OSA_TimeDelay(delay);
			clearscreen();
	 	}
	 	//Hook Opening
	 	else if (oppmove == 2)
	 	{
			//Flash Blue
			fillscreen(0x00, 0xFF, 0xFF);
			OSA_TimeDelay(delay);
			clearscreen();
	 	}

	 	//Testing Start Sequence
	 	// SEGGER_RTT_printf(0, "\nGesture Reading in 3...");
	 	//OSA_TimeDelay(1000);
	 	//SEGGER_RTT_printf(0, "\nGesture Reading in 2...");
	 	//OSA_TimeDelay(1000);
	 	//SEGGER_RTT_printf(0, "\nGesture Reading in 1...");
	 	//OSA_TimeDelay(1000);
	 	//SEGGER_RTT_printf(0, "\nGO! GO! GO!");

	 	enableI2Cpins(menuI2cPullupValue);
	 	
	 	for(int i=0; i<length; i++)
	 	{
	 		i2cReadStatus = readSensorRegisterMMA8451Q(kWarpSensorOutputRegisterMMA8451QOUT_X_MSB, 2 /* numberOfBytes */);
		 
	 		if (i2cReadStatus != kWarpStatusOK)
	 		{
		 		SEGGER_RTT_WriteString(0, " ----,");
	 		}
         		else
         		{
                 		//SEGGER_RTT_printf(0, "\nReading from X Sensor %d,", readSensorRegisterValueCombined);
		 		readSensorRegisterValueMSB = deviceMMA8451QState.i2cBuffer[0];
                 		readSensorRegisterValueLSB = deviceMMA8451QState.i2cBuffer[1];
                 		readSensorRegisterValueCombined = ((readSensorRegisterValueMSB & 0xFF) << 6) | (readSensorRegisterValueLSB >> 2);
                 		readSensorRegisterValueCombined = (readSensorRegisterValueCombined ^ (1 << 13)) - (1 << 13);
		 		xdata[i] = readSensorRegisterValueCombined;
         		}
	
			i2cReadStatus = readSensorRegisterMMA8451Q(kWarpSensorOutputRegisterMMA8451QOUT_Y_MSB, 2 /* numberOfBytes */);

			if (i2cReadStatus != kWarpStatusOK)
			{
				SEGGER_RTT_WriteString(0, " ----,");
			}
			else
			{      
				//SEGGER_RTT_printf(0, "\nReading from Y Sensor %d,", readSensorRegisterValueCombined);
				readSensorRegisterValueMSB = deviceMMA8451QState.i2cBuffer[0];
				readSensorRegisterValueLSB = deviceMMA8451QState.i2cBuffer[1];
				readSensorRegisterValueCombined = ((readSensorRegisterValueMSB & 0xFF) << 6) | (readSensorRegisterValueLSB >> 2);
	        		readSensorRegisterValueCombined = (readSensorRegisterValueCombined ^ (1 << 13)) - (1 << 13);
				ydata[i] = readSensorRegisterValueCombined;
			}

			i2cReadStatus = readSensorRegisterMMA8451Q(kWarpSensorOutputRegisterMMA8451QOUT_Z_MSB, 2 /* numberOfBytes */);
	
			if (i2cReadStatus != kWarpStatusOK)
			{
	        		SEGGER_RTT_WriteString(0, " ----,");
			}
			else
			{
	        		//SEGGER_RTT_printf(0, "\nReading from Z Sensor %d,", readSensorRegisterValueCombined);
				readSensorRegisterValueMSB = deviceMMA8451QState.i2cBuffer[0];
				readSensorRegisterValueLSB = deviceMMA8451QState.i2cBuffer[1];
				readSensorRegisterValueCombined = ((readSensorRegisterValueMSB & 0xFF) << 6) | (readSensorRegisterValueLSB >> 2);
				readSensorRegisterValueCombined = (readSensorRegisterValueCombined ^ (1 << 13)) - (1 << 13);
				zdata[i] = readSensorRegisterValueCombined; 
			}

			OSA_TimeDelay(20);   

		}

		disableI2Cpins();
		/*
		//Display Accelerometer Readings in Console
		
		for(int j=0; j<length; j++)
		{	
			SEGGER_RTT_printf(0, "\n%d,", xdata[j]);
			SEGGER_RTT_printf(0, " %d,", ydata[j]);
			SEGGER_RTT_printf(0, " %d,", zdata[j]);
		}
		*/	

		//Jab Check
		bool yjabflag1 = false;
		bool yjabflag2 = false;
		bool yjabflag3 = false;
		bool yjabstrong = false;
		bool jabhookdiff = false;
		int jabthresh = 0;

		for(int j=0; j<length; j++)
		{	
			if (ydata[j] > 3500)
			{
				yjabflag1 = true;
			}
			if (ydata[j] > 7000)
			{
				yjabstrong = true;
			}
			if (ydata[j] < -7000 && yjabflag1 == true)
			{
				yjabflag2 = true;
			}
			if (zdata[j] > 4000 && yjabflag2 == true)
			{
				yjabflag3 = true;
			}
			if (xdata[j] > 2000 || xdata[j] < -2000)
			{
				jabthresh++;
			}
		}

		if (jabthresh > 10)
		{
			jabhookdiff = true;
		}

		//Hook Check
		bool yhookstrongflag = false;
		bool yhookflag1 = false;
		bool xhookflag1 = false;
		bool yhookflag2 = false;
		bool zhookflag1 = false;

		for (int j=0; j<length; j++)
		{
			if (ydata[j] > 5000)
			{
				if (ydata[j] > 8000)
				{
					yhookstrongflag = true;
				}
				yhookflag1 = true;
			}
			if (xdata[j] < -4000)
			{
				xhookflag1 = true;
			}
			if (zdata[j] < -5000)
			{
				zhookflag1 = true;
			}
			if (ydata[j] < -5000 && yhookflag1 == true && zhookflag1 == true && xhookflag1)
			{
				yhookflag2 = true;
			}
		}
	
		//Block Check	
		uint8_t error = 0;
		bool blockcheck = false;

		for (int j=0; j<length; j++)
		{
			if (error <= 30)
			{	
				if (xdata[j] < -2200 || xdata[j] > -1900)
				{
					error++;
				}
				if (ydata[j] < -500 || ydata[j] > 500)
				{
					error++;
				}
				if (zdata[j] < -500 || zdata[j] > 350)
				{
					error++;
				}	
			}
		}

		if (error > 30)
		{
			blockcheck = true;
		}
        
		//Display Opponent Speech
		damagechance = xdata[29] % 2;
	
		//Jab
		if(yjabflag3 == true && jabhookdiff == false && oppmove == 1)
		{
			//Successful Opponent Hit
			if (yjabstrong == true)
			{	
				word(0x12, 0x11, 'j', 0xFF, 0xFF, 0xFF);
				clearscreen();
				fillscreen(0x00, 0xFF, 0x00);
				OSA_TimeDelay(delay);
				clearscreen();
				opphealth = opphealth - 10;
			}
			else
			{
				word(0x12, 0x11, 'p', 0xFF, 0xFF, 0xFF);
				clearscreen();
			}
		}
		else if (yjabflag3 == false && oppmove == 1)
		{
			if (damagechance == 0)
			{
				//fillscreen(0xFF, 0xFF, 0x00);
				OSA_TimeDelay(delay);
				clearscreen();
			}
			else if (damagechance == 1)
			{
				fillscreen(0xFF, 0x00, 0x00);
				OSA_TimeDelay(delay);
				clearscreen();
				health = health - 10;
			}	
		}
	
		//Hook
		if(yhookflag2 == true && jabhookdiff == true && oppmove == 2)
		{
			if (yhookstrongflag == true)
			{
				word(0x12, 0x11, 'h', 0xFF, 0XFF, 0XFF);
				clearscreen();
				fillscreen(0x00, 0xFF, 0x00);
				OSA_TimeDelay(delay);
				clearscreen();
				opphealth = opphealth - 20;
			}
			else
			{
				word(0x12, 0x11, 'p', 0xFF, 0xFF, 0xFF);
				clearscreen();
			}
		}
		else if (yhookflag2 == false && oppmove == 2)
		{
			if (damagechance == 0)
		 	{
				//fillscreen(0xFF, 0xFF, 0x00);
			 	OSA_TimeDelay(delay);
			 	clearscreen();
		 	}
		 	else if (damagechance == 1)
		 	{
			 	fillscreen(0xFF, 0x00, 0x00);
			 	OSA_TimeDelay(delay);
			 	clearscreen();
				health = health - 20;
		 	}
		}
	
		//Block
		if(blockcheck == false && oppmove == 0)
		{
			word(0x12, 0x11, 'n', 0xFF, 0XFF, 0XFF);
			clearscreen();
			fillscreen(0x00, 0xFF, 0x00);
			OSA_TimeDelay(delay);
			clearscreen();
		}
		else if (blockcheck == true && oppmove == 0)
		{
		 	if (damagechance == 0)
		 	{
			 	fillscreen(0xFF, 0x00, 0x00);
			 	OSA_TimeDelay(delay);
			 	clearscreen();
			 	health = health - 10;
		 	}
		 	else if (damagechance == 1)
		 	{
			 	fillscreen(0xFF, 0x00, 0x00);
                         	OSA_TimeDelay(delay);
			 	clearscreen();
				health = health - 20;
		 	}
		}
	
		oppmove = ydata[19] % 3;

		if (oppmove < 0)
		{
			oppmove = oppmove * -1;
		}	
	
		//Display Health
		displayhealth(health, opphealth);	

		//Display Win or Loss if Applicable
		if (health <= 0)
		{
			word(0x12, 0x11, 'f', 0xFF, 0XFF, 0XFF);
			OSA_TimeDelay(delay);
			clearscreen();
		}	
		if (opphealth <= 0)
		{
			word(0x12, 0x11, 'w', 0xFF, 0XFF, 0XFF);
			OSA_TimeDelay(delay);
			clearscreen();
		}

	}

	return;
}

//FUNCTION TO FILL SCREEN A SPECIFIC COLOUR
/*Function simply fills the screen a specific colour which substitutes as the prompt for the user to attack/block */
void fillscreen (uint8_t red, uint8_t green, uint8_t blue)
{
	rectdraw(0x00, 0x00, 0x5F, 0x3F);
	textcol(red, green, blue);
}

//FUNCTION TO DISPLAY THE HEALTH FOR THE PLAYERS
/*This function will display the health of the user and virtual opponent in the form of health bars. When the health is above 50 they will stay green. Between 30 and 50, the health bar is orange. Below 30 the health bar is red. The top health bar will correspond to the player and the bottom health bar will correspond to the virtual opponent*/
void displayhealth(int healthval, int opphealthval)
{
	clearscreen();
	float healthscaled = healthval * 0.86;
	float opphealthscaled = opphealthval * 0.86;
	int playerhealth = healthscaled/1;
	int opponenthealth = opphealthscaled/1;
	
	SEGGER_RTT_printf(0, "\n%d", playerhealth);
	SEGGER_RTT_printf(0, "\n%d", opponenthealth);

	if (playerhealth < 0)
	{
		playerhealth = 0;
	}
	if (opponenthealth < 0)
	{
		opponenthealth = 0;
	}
		
	//Display Player Health
	rectdraw(0x05, 0x0A, 0x05 + playerhealth, 0x1E);
	if (healthval > 50)
	{
		textcol(0x00, 0xFF, 0X00);
	}
	else if (healthval > 30 && healthval <= 50)
	{
		textcol(0xFF, 0xFF, 0x00);
	}
	else
	{
		textcol(0xFF, 0x00, 0x00);
	}

	//Display Opponent Health
	rectdraw(0x05, 0x23, 0x05 + opponenthealth, 0x37);
	if (opphealthval > 50)
	{
		textcol(0x00, 0xFF, 0X00);
	}
	else if (opphealthval > 30 && opphealthval <= 50)
	{
		textcol(0xFF, 0xFF, 0x00);
	}
	else
	{
		textcol(0xFF, 0x00, 0x00);
	}
	OSA_TimeDelay(delay);
	clearscreen(); 

}
