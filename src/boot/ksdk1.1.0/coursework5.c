#include <stdlib.h>

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

extern volatile WarpI2CDeviceState      deviceMMA8451QState;

//FUNCTION TO CONFIGURE MMA8451Q ACCELEROMETER SCALE
/*To summarise, this function will set the range of values which the accelerometer can measure up to. It has can measure from +/- 2g, 4g and 8g. The higher the range, the lower the resolution. To set this scale for the accelerometer readings, the chip must be set to be in the standby mode (as opposed to the active mode) which means the Bit 0 in CTRL_REG1 (0x2A) has to be toggled (0 for Standby and 1 for Active). Then the scale is set by writing to Bit 0 (FS0) and Bit 1 (FS1) in XYZ_DATA_CFG register (0x0E). '00' corresponds to the 2g scale, '01' corresponds to 4g and '10' corresponds to 8g */
void configureScale(char scale)
{
	uint16_t        menuI2cPullupValue = 32768;
	enableI2Cpins(menuI2cPullupValue);
	//Configure Accelerometer Scale
	configureSensorMMA8451Q(0x00,/* Payload: Disable FIFO */ 0x01,/* Normal read 8bit, 800Hz, normal, active mode */ menuI2cPullupValue );
	writeSensorRegisterMMA8451Q(0x2A, 0x00, menuI2cPullupValue); //Set Standby Mode
		if (scale == '4')
		{	
			writeSensorRegisterMMA8451Q(0x0E, 0x01, menuI2cPullupValue); //Set 4g Scale (can only change this in standby mode)
		}
		else if (scale == '8')
		{
			writeSensorRegisterMMA8451Q(0x0E, 0x02, menuI2cPullupValue); //Set 8g Scale (can only change this in standby mode)
		}
		else
		{
			writeSensorRegisterMMA8451Q(0x0E, 0x00, menuI2cPullupValue); //Set 2g Scale (can only change this in standby mode)
		}
	writeSensorRegisterMMA8451Q(0x2A, 0x01, menuI2cPullupValue); //Set Active Mode
	disableI2Cpins();
}

//MAIN FUNCTION LOOP FOR GESTURE SWIPE MODE EXECUTION
/*The function starts of by waiting for the press of the thumb switch or the press of SW3 to return to the Choose Mode Loop (which is disabled for the first run). Once the thumb switch is pressed, the 40 accelerometer readings are taken every 20 milliseconds and saved to x, y and z data arrays. Then the analysis is conducted whereby these data raays are sifted through to see if they trigger specific checkpoints. This will determine which gesture swipe has been completed and display the appropriate identified gesture by writing text to the display. No text is displayed if no gesture is detected. This function then continues to loop and waits for the thumb or reset switch to either carry on with Gesture Swiping or to return to the initial menu. */
void readaccel()
{
	 //Initialisation of Variables
	 uint16_t        readSensorRegisterValueLSB;
	 uint16_t        readSensorRegisterValueMSB;
	 int16_t         readSensorRegisterValueCombined;
	 WarpStatus      i2cReadStatus;
	 uint16_t        menuI2cPullupValue = 32768;
	 uint8_t	 length = 40;
	 int16_t	 xdata[length];
	 int16_t	 ydata[length];
	 int16_t	 zdata[length];
	 bool 		 choosemode = false;
	 bool		 choosemodeenable = false;
	
	 while(1)
	 {
	 	enableI2Cpins(menuI2cPullupValue);
	
	 	//Switch Trigger Loop 
	 	while(1)
	 	{
	 		//Check if Thumb Switch is Pressed - 0 is off and 1 is on
	 		uint32_t thumbread = GPIO_DRV_ReadPinInput( kWarpPinThumbSW );
			if (thumbread == 1)
			{
				break;
			}
			//Allow for one run before enabling check for reset back to Choose Mode Loop
			if (choosemodeenable == true)
			{
				//Check if Choose Mode Switch is Pressed - 1 is off and 0 is on
				uint32_t choosemoderead = GPIO_DRV_ReadPinInput( kWarpPinSW3 );
				//If SW3 pressed
				if (choosemoderead == 0)
				{
					while(1)
					{
						OSA_TimeDelay(50);						
						//Check for release of SW3 before allowing back to Choose Mode Loop (such that user does not accidentally trigger switch again to select the mode)	
						uint32_t choosemoderead2 = GPIO_DRV_ReadPinInput( kWarpPinSW3 );
						if (choosemoderead2 == 1)
						{
							choosemode = true;
							break;
						}
					}
					if (choosemode == true)
					{
						break;
					}
				}
			}
			OSA_TimeDelay(50);
		}
		
		//To get back to Choose Mode Loop 
		if (choosemode == true)
		{
			break;
		}
		choosemodeenable = true;

	 	//Testing Start Sequence	 
	 	//SEGGER_RTT_printf(0, "\nGesture Reading in 3...");
	 	//OSA_TimeDelay(1000);
	 	//SEGGER_RTT_printf(0, "\nGesture Reading in 2...");
	 	//OSA_TimeDelay(1000);
	 	//SEGGER_RTT_printf(0, "\nGesture Reading in 1...");
	 	//OSA_TimeDelay(1000);
	 	//SEGGER_RTT_printf(0, "\nGO! GO! GO!");
	
	 	//This loop reads 40 readings of the accelerometer values (every 20 milliseconds) and saves them to axis specific data arrays
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

		//Display Accelerometer Readings in Console
		/*			
		for(int j=0; j<length; j++)
		{	
			SEGGER_RTT_printf(0, "\n%d,", xdata[j]);
			SEGGER_RTT_printf(0, " %d,", ydata[j]);
			SEGGER_RTT_printf(0, " %d,", zdata[j]);
		}
		*/
	
		//Checkpoint Tests for Gestures from Accelerometer Readings
	
		//Gesture Up Check	
		bool xupflag = false;
		bool yupflag1 = false;
		bool zupflag = false;
		bool yupflag2 = false;
		bool uprightdiff = false;
		int uprightcount = 0;
		bool updowndiff = false;
		int updowncount = 0;

		for(int j=0; j<length; j++)
		{
			if (xdata[j] < -200)
			{
				xupflag = true;
			}
			if (ydata[j] < -1500)
			{
				yupflag1 = true;
			}
			if (zdata[j] < 1000 && xupflag == true && yupflag1 == true)
			{
				zupflag = true;
			}
			if (ydata[j] > 1400 && zupflag == true)
			{
				yupflag2 = true;
			}
			if (xdata[j] > 1000 || xdata[j] < -1000)
			{
				uprightcount++;
			}
			if (zdata[j] < 0)
			{
				updowncount++;
			}
		}
		//Clause to help differentiate some up cases from the right
		if (uprightcount > 10)
		{
			uprightdiff = true;
		}
		//Clause to help differentiate some up cases from down
		if (updowncount > 4)
		{
			updowndiff = true;
		}

		//Gesture Down Check
		bool xdownflag = false;
		bool ydownflag1 = false;
		bool ydownflag2 = false;
		bool zdownflag = false;

		for(int j=0; j<length; j++)
		{
			if (zdata[j] < 1500)
			{
				zdownflag = true;
			}
			if (ydata[j] > 50)
			{
				ydownflag1 = true;
			}
			if (xdata[j] > -250 && zdownflag == true && ydownflag1 == true)
			{
				xdownflag = true;
			}
			if (ydata[j] < -1700 && xdownflag == true)
			{
				ydownflag2 = true;
			}	
		}
	
		//Gesture Left Check
		bool xleftflag = false;
		bool yleftflag = false;
		bool zleftflag1 = false;
		bool zleftflag2 = false;
		bool leftrightdiff = false;
		int leftrightcount = 0;

		for(int j=0; j<length; j++)
		{
			if (zdata[j] > 1500)
			{
				zleftflag1 = true;
			}
			if (ydata[j] > 200 && zleftflag1 == true)
			{
				yleftflag = true;	
			}
			if (xdata[j] < -2500 && yleftflag == true && zleftflag1 == true)
			{
				xleftflag = true;
			}
			if (zdata[j] > 800 && xleftflag == true)
			{
				zleftflag2 = true;
			}
		}	
		
		//Clause to help differentiate between left and right commands
		for(int j=24; j<35; j++)
		{
			if (zdata[j] < 0)
			{
				leftrightcount++;
			}

		}

		if (leftrightcount >= 5)
		{
			leftrightdiff = true;
		}
	
		//Gesture Right Check
		bool xrightflag1 = false;
		bool xrightflag2 = false;
		bool yrightflag1 = false;
		bool yrightflag2 = false;
		bool zrightflag = false;
					
		for(int j=0; j<length; j++)
		{
			if (ydata[j] < 300)
			{
				yrightflag1 = true;
			}
			if (xdata[j] > -700)
			{
				xrightflag1 = true;
			}
			if (xdata[j] < -2000 && yrightflag1 == true && xrightflag1 == true)
			{
				xrightflag2 = true;
			}
			if (zdata[j] < 300 && yrightflag1 == true && xrightflag1 == true)
			{
				zrightflag = true;
			}
			if (ydata[j] > 400 && zrightflag == true)
			{
				yrightflag2 = true;
			}
		}

		//Display Result
		//Up
		if (yupflag2 == true && uprightdiff == false && updowndiff == true)
        	{
	        	word(0x18, 0x11, 'u', 0xFF, 0xFF, 0xFF);
		}
		//Down
		else if (ydownflag2 == true && updowndiff == false)
		{
			word(0x12, 0x11, 'd', 0xFF, 0xFF, 0xFF);
		}
		//Left
		else if (zleftflag2 == true && leftrightdiff == false)
		{
			word(0x12, 0x11, 'l', 0xFF, 0xFF, 0xFF);
		}
		//Right
		else if (yrightflag2 == true && leftrightdiff == true)
		{
			word(0x12, 0x11, 'r', 0xFF, 0xFF, 0xFF);
		}
	}
	return;
}	

//FUNCTION TO DRAW RECTANGLE ON SCREEN
/*This function will draw a rectangle at any specified location on the screen according to the row and column addresses given */
void rectdraw(uint8_t startcol, uint8_t startrow, uint8_t endcol, uint8_t endrow)
{
	writeCommand(kSSD1331CommandDRAWRECT);
	writeCommand(startcol);
	writeCommand(startrow);
	writeCommand(endcol);
	writeCommand(endrow);
}	

//FUNCTION TO CHOOSE COLOUR OF RECTANGLE - TO FOLLOW AFTER RECTDRAW FUNCTION ABOVE
/*This function continues from the rectdraw function to determine the colour of specified rectangle. These functions were split just to prevent too many variables being passed in a single function and to better differentiate the two main definitions for the rectangle */
void textcol(uint8_t red, uint8_t green, uint8_t blue)
{
	writeCommand(red);
	writeCommand(green);
	writeCommand(blue);
	writeCommand(red);
	writeCommand(green);
	writeCommand(blue);	
}

//FUNCTION WITH CASE STATEMENT SUCH TO DRAW INDIVIDUAL LETTERS OF PRE-DECIDED SIZE AT SPECIFIED LOCATION ON SCREEN
/*This function contains the required make up of rectangles such to write each letter (which is designed to to fit in a 11 x 16 rectangle with the exception of the letter W) at the specified location passed in the function. This case statement only includes the letters needed for the used words in this project as opposed to the whole alphabet */
void text(uint8_t startcol, uint8_t startrow, char character, uint8_t red, uint8_t green, uint8_t blue)
{
	switch(character)
	{
		case 'D':
			rectdraw(startcol, startrow, startcol + 2, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 3, startrow, startcol + 7, startrow + 2);
			textcol(red, green, blue);
			rectdraw(startcol + 6, startrow + 1, startcol + 8, startrow + 2);
			textcol(red, green, blue);
			rectdraw(startcol + 7, startrow + 2, startcol + 9, startrow + 3);
			textcol(red, green, blue);
			rectdraw(startcol + 8, startrow + 3, startcol + 10, startrow + 12);
			textcol(red, green, blue);
			rectdraw(startcol + 7, startrow + 12, startcol + 9, startrow + 13);
			textcol(red, green, blue);
			rectdraw(startcol + 6, startrow + 13, startcol + 8, startrow + 14);
			textcol(red, green, blue);
			rectdraw(startcol + 3, startrow + 13, startcol + 7, startrow + 15);
			textcol(red, green, blue);
			break;

		case 'E':
			rectdraw(startcol, startrow, startcol + 2, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 2, startrow, startcol + 10, startrow + 2);
			textcol(red, green, blue);
			rectdraw(startcol + 2, startrow + 7, startcol + 8, startrow + 9);
			textcol(red, green, blue);
			rectdraw(startcol + 2, startrow + 13, startcol + 10, startrow + 15);
			textcol(red, green, blue);
			break;

		case 'F':
			rectdraw(startcol, startrow, startcol + 2, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 2, startrow, startcol + 10, startrow + 2);
			textcol(red, green, blue);
			rectdraw(startcol + 2, startrow + 7, startcol + 8, startrow + 9);
			textcol(red, green, blue);
			break;
		
		case 'G':
			rectdraw(startcol, startrow + 2, startcol + 2, startrow + 13);
			textcol(red, green, blue);
			rectdraw(startcol + 2, startrow, startcol + 9, startrow + 2);
			textcol(red, green, blue);
			rectdraw(startcol + 7, startrow + 1, startcol + 10, startrow + 3);
			textcol(red, green, blue);
			rectdraw(startcol + 1, startrow + 1, startcol + 3, startrow + 3);
			textcol(red, green, blue);
			rectdraw(startcol + 6, startrow + 9, startcol + 10, startrow + 10);
			textcol(red, green, blue);
			rectdraw(startcol + 8, startrow + 9, startcol + 10, startrow + 14);
			textcol(red, green, blue);
			rectdraw(startcol + 2, startrow + 13, startcol + 9, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 1, startrow + 12, startcol + 3, startrow + 14);
			textcol(red, green, blue);
			break;	
		
		case 'H':
			rectdraw(startcol, startrow, startcol + 2, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 8, startrow, startcol + 10, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 2, startrow + 6, startcol + 8, startrow + 9);
			textcol(red, green, blue);
			break;

		case 'I':
			rectdraw(startcol, startrow, startcol + 10, startrow + 2);
			textcol(red, green, blue);
		        rectdraw(startcol, startrow + 13, startcol + 10, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 4, startrow + 2, startcol + 7, startrow + 15);
			textcol(red, green, blue);
			break;

		case 'L':
			rectdraw(startcol, startrow, startcol + 2, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 2, startrow + 13, startcol + 10, startrow + 15);
			textcol(red, green, blue);	
			break;

		case 'N':
			 rectdraw(startcol, startrow, startcol + 1, startrow + 15);
			 textcol(red, green, blue);
			 rectdraw(startcol + 9, startrow, startcol + 10, startrow + 15);
			 textcol(red, green, blue);
			 rectdraw(startcol + 2, startrow, startcol + 2, startrow + 4);
			 textcol(red, green, blue);
			 rectdraw(startcol + 3, startrow + 2, startcol + 3, startrow + 6);
			 textcol(red, green, blue);
			 rectdraw(startcol + 4, startrow + 4, startcol + 4, startrow + 8);
			 textcol(red, green, blue);
			 rectdraw(startcol + 5, startrow + 7, startcol + 5, startrow + 11);
			 textcol(red, green, blue);
			 rectdraw(startcol + 6, startrow + 9, startcol + 6, startrow + 13);
			 textcol(red, green, blue);
			 rectdraw(startcol + 7, startrow + 11, startcol + 7, startrow + 15);
			 textcol(red, green, blue);
			 rectdraw(startcol + 8, startrow + 13, startcol + 8, startrow + 15);
			 textcol(red, green, blue);
			 break;

		case 'O':
			 rectdraw(startcol + 2, startrow, startcol + 8, startrow + 1);
			 textcol(red, green, blue);
			 rectdraw(startcol + 1, startrow + 1, startcol + 3, startrow + 2);
			 textcol(red, green, blue);
			 rectdraw(startcol, startrow + 2, startcol + 2, startrow + 3);
			 textcol(red, green, blue);
			 rectdraw(startcol, startrow + 2, startcol + 1, startrow + 12);
			 textcol(red, green, blue);
			 rectdraw(startcol, startrow + 12, startcol + 2, startrow + 13);
			 textcol(red, green, blue);
			 rectdraw(startcol + 1, startrow + 13, startcol + 3, startrow + 14);
			 textcol(red, green, blue);
			 rectdraw(startcol + 2, startrow + 14, startcol + 8, startrow + 15);
			 textcol(red, green, blue);
			 rectdraw(startcol + 7, startrow + 1, startcol + 9, startrow + 2);
			 textcol(red, green, blue);
			 rectdraw(startcol + 8, startrow + 2, startcol + 10, startrow + 3);
			 textcol(red, green, blue);
			 rectdraw(startcol + 9, startrow + 4, startcol + 10, startrow + 13);
			 textcol(red, green, blue);
			 rectdraw(startcol + 8, startrow + 12, startcol + 10, startrow + 13);
			 textcol(red, green, blue);
			 rectdraw(startcol + 7, startrow + 13, startcol + 9, startrow + 14);
			 textcol(red, green, blue);
			 break;

		case 'P':
			 rectdraw(startcol, startrow, startcol + 2, startrow + 15);
			 textcol(red, green, blue);
			 rectdraw(startcol + 3, startrow, startcol + 8, startrow + 1);
			 textcol(red, green, blue);
			 rectdraw(startcol + 7, startrow + 1, startcol + 9, startrow + 2);
			 textcol(red, green, blue);
			 rectdraw(startcol + 8, startrow + 2, startcol + 10, startrow + 3);
			 textcol(red, green, blue);
			 rectdraw(startcol + 9, startrow + 4, startcol + 10, startrow + 4);
			 textcol(red, green, blue);
			 rectdraw(startcol + 8, startrow + 5, startcol + 10, startrow + 6);
			 textcol(red, green, blue);
			 rectdraw(startcol + 7, startrow + 6, startcol + 9, startrow + 7);
			 textcol(red, green, blue);
			 rectdraw(startcol + 3, startrow + 7, startcol + 8, startrow + 8);
			 textcol(red, green, blue);
			 break;

		case 'R':
			 rectdraw(startcol, startrow, startcol + 2, startrow + 15);
			 textcol(red, green, blue);
			 rectdraw(startcol + 3, startrow, startcol + 8, startrow + 1);
			 textcol(red, green, blue);
			 rectdraw(startcol + 7, startrow + 1, startcol + 9, startrow + 2);
			 textcol(red, green, blue);
			 rectdraw(startcol + 8, startrow + 2, startcol + 10, startrow + 3);
			 textcol(red, green, blue);
			 rectdraw(startcol + 9, startrow + 4, startcol + 10, startrow + 4);
			 textcol(red, green, blue);
			 rectdraw(startcol + 8, startrow + 5, startcol + 10, startrow + 6);
			 textcol(red, green, blue);
			 rectdraw(startcol + 7, startrow + 6, startcol + 9, startrow + 7);
			 textcol(red, green, blue);
			 rectdraw(startcol + 3, startrow + 7, startcol + 8, startrow + 8);
			 textcol(red, green, blue);
			 rectdraw(startcol + 3, startrow + 9, startcol + 5, startrow + 10);
			 textcol(red, green, blue);
			 rectdraw(startcol + 4, startrow + 10, startcol + 6, startrow + 11);
			 textcol(red, green, blue);
			 rectdraw(startcol + 5, startrow + 11, startcol + 7, startrow + 12);
			 textcol(red, green, blue);
			 rectdraw(startcol + 6, startrow + 12, startcol + 8, startrow + 13);
			 textcol(red, green, blue);
			 rectdraw(startcol + 7, startrow + 13, startcol + 9, startrow + 14);
			 textcol(red, green, blue);
			 rectdraw(startcol + 8, startrow + 14, startcol + 10, startrow + 15);
			 textcol(red, green, blue);
			 break;

		case 'S':
			 rectdraw(startcol + 2, startrow, startcol + 9, startrow);
			 textcol(red, green, blue);
			 rectdraw(startcol + 1, startrow + 1, startcol + 10, startrow + 1);
			 textcol(red, green, blue);
			 rectdraw(startcol, startrow + 2, startcol + 10, startrow + 2);
			 textcol(red, green, blue);
			 rectdraw(startcol, startrow + 3, startcol + 3, startrow + 5);
			 textcol(red, green, blue);
			 rectdraw(startcol, startrow + 6, startcol + 8, startrow + 6);
			 textcol(red, green, blue);
			 rectdraw(startcol, startrow + 7, startcol + 9, startrow + 7);
			 textcol(red, green, blue);
			 rectdraw(startcol + 1, startrow + 8, startcol + 10, startrow + 8);
			 textcol(red, green, blue);
			 rectdraw(startcol + 2, startrow + 9, startcol + 10, startrow + 9);
			 textcol(red, green, blue);
			 rectdraw(startcol + 7, startrow + 10, startcol + 10, startrow + 12);
			 textcol(red, green, blue);
			 rectdraw(startcol, startrow + 13, startcol + 10, startrow + 13);
			 textcol(red, green, blue);
			 rectdraw(startcol, startrow + 14, startcol + 9, startrow + 14);
			 textcol(red, green, blue);
			 rectdraw(startcol + 1, startrow + 15, startcol + 8, startrow + 15);
			 textcol(red, green, blue);
			 break;

		case 'T':
			 rectdraw(startcol, startrow, startcol + 10, startrow + 2);
			 textcol(red, green, blue);
			 rectdraw(startcol + 4, startrow + 3, startcol + 6, startrow + 15);
			 textcol(red, green, blue);
			 break;
		
		case 'U':
			 rectdraw(startcol, startrow, startcol + 2, startrow + 15);
			 textcol(red, green, blue);
			 rectdraw(startcol + 3, startrow + 13, startcol + 7, startrow + 15);
			 textcol(red, green, blue);
			 rectdraw(startcol + 8, startrow, startcol + 10, startrow + 15);
                         textcol(red, green, blue);			
			 break;

		case 'W':
			rectdraw(startcol, startrow, startcol + 2, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 13, startrow, startcol + 15, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 3, startrow + 11, startcol + 3, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 4, startrow + 10, startcol + 4, startrow + 14);
			textcol(red, green, blue);
			rectdraw(startcol + 5, startrow + 9, startcol + 5, startrow + 13);
			textcol(red, green, blue);
			rectdraw(startcol + 6, startrow + 8, startcol + 6, startrow + 12);
			textcol(red, green, blue);
			rectdraw(startcol + 7, startrow + 8, startcol + 7, startrow + 11);
			textcol(red, green, blue);
			rectdraw(startcol + 8, startrow + 8, startcol + 8, startrow + 12);
			textcol(red, green, blue);
			rectdraw(startcol + 9, startrow + 9, startcol + 9, startrow + 13);
			textcol(red, green, blue);
			rectdraw(startcol + 10, startrow + 10, startcol + 10, startrow + 14);
			textcol(red, green, blue);
			rectdraw(startcol + 11, startrow + 11, startcol + 11, startrow + 15);
			textcol(red, green, blue);
			rectdraw(startcol + 12, startrow + 12, startcol + 12, startrow + 15);
			textcol(red, green, blue);
			break;
		default:
			break;
	}		
}

//FUNCTION TO CLEAR SCREEN
/*Function ensures that screen is clear before attempting to display any writing or features on the OLED screen */
void clearscreen()
{
	writeCommand(kSSD1331CommandCLEAR);
	writeCommand(0x00);
	writeCommand(0x00);
	writeCommand(0x5F);
	writeCommand(0x3F);
}

//FUNCTION TO WRITE A SPECIFIC WORD ON THE OLED SCREEN
/*Function makes use of the text function such to make up the desired words used in both the Gesture Swipe and Game Modes */ 
void word(uint8_t startcol, uint8_t startrow, char gesture, uint8_t red, uint8_t green, uint8_t blue)
{
	//Clear Screen
	clearscreen(); 
	
	switch (gesture)
	{
		//Down
		case 'd':
			 text(startcol, startrow, 'D', red, green, blue);
			 text(startcol + 12, startrow, 'O', red, green, blue);
			 text(startcol + 24, startrow, 'W', red, green, blue);
			 text(startcol + 42, startrow, 'N', red, green, blue);
		 	 break;
		//Up
		case 'u':
			 text(startcol, startrow, 'U', red, green, blue);
			 text(startcol + 12, startrow, 'P', red, green, blue);
			 break;
		//Left
		case 'l':
			 text(startcol, startrow, 'L', red, green, blue);
			 text(startcol + 12, startrow, 'E', red, green, blue);
			 text(startcol + 24, startrow, 'F', red, green, blue);
			 text(startcol + 36, startrow, 'T', red, green, blue);
			 break;
		//Right
		case 'r':
			 text(startcol, startrow, 'R', red, green, blue);
			 text(startcol+ 12, startrow, 'I', red, green, blue);
			 text(startcol + 24, startrow, 'G', red, green, blue);
			 text(startcol + 36, startrow, 'H', red, green, blue);
			 text(startcol + 48, startrow, 'T', red, green, blue);
			 break;
		//Jab
		case 'j':
			 text(startcol + 12, startrow, 'P', red, green, blue);
			 text(startcol + 24, startrow, 'O', red, green, blue);
			 text(startcol + 36, startrow, 'W', red, green, blue);
			 break;
		//Hook
		case 'h':
			 text(startcol + 12, startrow, 'O', red, green, blue);
			 text(startcol + 24, startrow, 'W', red, green, blue);
			 text(startcol + 42, startrow, 'W', red, green, blue);
			 break;
		//Block
		case 'n':
			 text(startcol + 12, startrow, 'N', red, green, blue);
			 text(startcol + 24, startrow, 'O', red, green, blue);
			 text(startcol + 36, startrow, 'O', red, green, blue);
			 break;
		//Win
		case 'w':
			 text(startcol + 12, startrow, 'W', red, green, blue);
			 text(startcol + 30, startrow, 'I', red, green, blue);
			 text(startcol + 42, startrow, 'N', red, green, blue);
			 break;
		//Lose
		case 'f':
			 text(startcol + 12, startrow, 'L', red, green, blue);
			 text(startcol + 24, startrow, 'O', red, green, blue);
			 text(startcol + 36, startrow, 'S', red, green, blue);
			 text(startcol + 48, startrow, 'S', red, green, blue);
			 break;
		//Weak
		case 'p':
			 text(startcol + 12, startrow, 'P', red, green, blue);
			 text(startcol + 24, startrow, 'O', red, green, blue);
			 text(startcol + 36, startrow, 'O', red, green, blue);
			 text(startcol + 48, startrow, 'R', red, green, blue);
			 break;
		default:
			 break;
	}
	
	OSA_TimeDelay(2000); //Delay to Allow Viewing of Remark
	clearscreen();

}

