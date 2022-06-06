/*
 ===============================================================================
 Autors   	: Michal Jemiolek 229902
            : Bartosz Janiszewski 229897
            : Przemyslaw Grubski 229887
 ===============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_dac.h"

#include "joystick.h"
#include "rgb.h"
#include "oled.h"
#include "rotary.h"
#include "acc.h"
#include "pca9532.h"
#include "light.h"
#include "led7seg.h"
#include "eeprom.h"

/*
 * SPP initialization
 * Atributes: none
 * return: none
 */
static void sspInit(void) {
    SSP_CFG_Type SSP_ConfigStruct;
    PINSEL_CFG_Type PinCfg;

    /*
     * Initialize SPI pin connect
     * P0.7 - SCK;
     * P0.8 - MISO
     * P0.9 - MOSI
     * P2.2 - SSEL - used as GPIO
     */
    PinCfg.Funcnum = 2;
    PinCfg.OpenDrain = 0;
    PinCfg.Pinmode = 0;
    PinCfg.Portnum = 0;
    PinCfg.Pinnum = 7;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 8;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 9;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Funcnum = 0;
    PinCfg.Portnum = 2;
    PinCfg.Pinnum = 2;
    PINSEL_ConfigPin(&PinCfg);

    SSP_ConfigStructInit(&SSP_ConfigStruct);

    // Initialize SSP peripheral with parameter given in structure above
    SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

    // Enable SSP peripheral
    SSP_Cmd(LPC_SSP1, ENABLE);
}

/*
 * return int based on rotary output
 * Atributes:
 * uint8_t direction - state of encoder
 * int value - output score based on this value
 * return: int value - decreased, increased or not changed based on direction value
 */
int rotaryMove(uint8_t direction, int value) {
    uint8_t ledSeven = value + '0';

    if (direction != ROTARY_WAIT) {
        if (direction == ROTARY_LEFT) {
            ledSeven--;
        } else if (direction == ROTARY_RIGHT) {
            ledSeven++;
        }

        if (ledSeven < '1') {
            ledSeven = '9';
        } else if (ledSeven > '9') {
            ledSeven = '1';
        }
    }
    value = ledSeven - '0';

    return value;
}

/*
 * set diode to color
 * Atributes:
 * uint8_t number - attribute to change to change Led bulb
 * return: none
 */
void setDiode(uint8_t number) {
    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    if (number == 1) {red = RGB_RED;}
    else if (number == 2) {blue = RGB_BLUE;}
    else if (number == 3) {green = RGB_GREEN;}

    rgb_setLeds(red | green | blue);
}

/*
 * return value based on joystick move
 * Atributes: none
 * return: int (0 or 1) based on joystick move
 */
int joystickController() {
    uint8_t joyStick = 0;
    joyStick = joystick_read();
    if (((JOYSTICK_UP & joyStick) != 0) || ((JOYSTICK_DOWN & joyStick) != 0))
	{
        setDiode(2);
    } else {
    	return 0;
    }
    return 1;
}

/*
 * i2c initialization
 * Atributes: none
 * return: none
 */
static void i2cInit(void) {
 PINSEL_CFG_Type PinCfg;
 /* Initialize I2C2 pin connect */
 /* port 0, pin 10, funkcja 2 - SDA2 dla I2C2 */
 PinCfg.Funcnum = 2;
 PinCfg.Pinnum = 10;
 PinCfg.Portnum = 0;
 PINSEL_ConfigPin(&PinCfg);
 /* port 0, pin 11, funkcja 2 - SCL2 dla I2C2 */
 PinCfg.Pinnum = 11;
 PINSEL_ConfigPin(&PinCfg);
 // Initialize I2C peripheral
 I2C_Init(LPC_I2C2, 100000);
 /* Enable I2C1 operation */
 I2C_Cmd(LPC_I2C2, ENABLE);
}

/*
 * read from accelerometer
 * Atributes:
 * int32_t extAxisX - AxisX value of initial state
 * int32_t extAxisY - AxisY value of initial state
 * int32_t extAxisZ - AxisZ value of initial state
 * return: int (value from 0 to 4) 
 * 0 indicates not enough movement for action 
 * 1-4 indicates indicates enough movement for action
 */
int accelerometerController(int32_t extAxisX, int32_t extAxisY, int32_t extAxisZ) {

    int8_t axisX = 0;
    int8_t axisY = 0;
    int8_t axisZ = 0;

    acc_read(&axisX, &axisY, &axisZ);
    axisX += extAxisX;
    axisY += extAxisY;
    axisZ += extAxisZ;

    if (axisY < -25) {
        return 1;
    } else if (axisY > 25) {
        return 2;
    } else if (axisX > 25) {
        return 3;
    } else if (axisX < -25) {
        return 4;
    } else {
        return 0;
    }
}

/*
 * Activating Led Stripe
 * Atributes: int LEDsNumber - number of diodes to turn on
 * return: none
 */
void setLEDStripe(int LEDsNumber) {
    uint16_t activeLEDs = 0;

    for (int i = 0; i < LEDsNumber; i++) {
        int temp = 15 - i;
        activeLEDs |= (1 << temp);
    }

    pca9532_setLeds(activeLEDs, 0xffff);
}

/*
 * Clean oled screen
 * Atributes: none
 * return: none
 */
void cleanScreen() {
        oled_clearScreen(OLED_COLOR_WHITE);
}

/*
 * Output text on oled screen
 * Atributes:
 * char *text - output text
 * int x - X axis on screen
 * int y - Y axis on screen
 * return: none
 */
void outputText(char *text, int x, int y) {
        oled_putString(x, y, (uint8_t *) text, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
}

/*
 * initialize timer1
 * Atributes: none
 * return: none
 */
void initTimer1() {
    TIM_TIMERCFG_Type TIM_ConfigStruct;
    TIM_MATCHCFG_Type TIM_MatchConfigStruct;

// Initialize timer 1, prescale count time of 1ms
    TIM_ConfigStruct.PrescaleOption = TIM_PRESCALE_USVAL;
    TIM_ConfigStruct.PrescaleValue = 1000;
    // use channel 0, MR0
    TIM_MatchConfigStruct.MatchChannel = 0;
    // Enable interrupt when MR0 matches the value in TC register
    TIM_MatchConfigStruct.IntOnMatch = FALSE;
    //Enable reset on MR0: TIMER will not reset if MR0 matches it
    TIM_MatchConfigStruct.ResetOnMatch = FALSE;
    //Stop on MR0 if MR0 matches it
    TIM_MatchConfigStruct.StopOnMatch = FALSE;
    //do no thing for external output
    TIM_MatchConfigStruct.ExtMatchOutputType = TIM_EXTMATCH_NOTHING;
    // Set Match value, count value is time (timer * 1000uS =timer mS )
    TIM_MatchConfigStruct.MatchValue = 0xfffffff;

    // Set configuration for Tim_config and Tim_MatchConfig
    TIM_Init(LPC_TIM1, TIM_TIMER_MODE, &TIM_ConfigStruct);
    TIM_ConfigMatch(LPC_TIM1, &TIM_MatchConfigStruct);
    // To start timer 1
    TIM_Cmd(LPC_TIM1, ENABLE);
}


/*
 * Function to change diode color to green randomly
 * Atributes: none
 * return: none
 */
void randomColor()
{
	uint32_t xd = (rand() % 4000) + 1000;
	Timer0_Wait(xd);
	setDiode(3);
}

/*
 * Function to get player input and give score
 * Atributes: int level - indicates level of gameplay
 * return: none
 * Level 1 - 1000 ms
 * Level 2 - 925  ms
 * Level 3 - 850  ms
 * Level 4 - 775  ms
 * Level 5 - 700  ms
 * Level 6 - 625  ms
 * Level 7 - 550  ms
 * Level 8 - 475  ms
 * Level 9 - 400  ms
 */
void reflex(int level)
{
	uint32_t time = 0;
	uint32_t allowedTime = 1000 - (level - 1) * 75;

    int accTemp = 0;

    int8_t axisX = 0;
    int8_t axisY = 0;
    int8_t axisZ = 0;

    acc_read(&axisX, &axisY, &axisZ);
    int8_t AxX = 0 - axisX;
    int8_t AxY = 0 - axisY;
    int8_t AxZ = 0 - axisZ;


        initTimer1();
        cleanScreen();
        outputText("TERAZ", 21, 21);

        while (accTemp == 0 && time <= allowedTime) {
            time = LPC_TIM1->TC;
            accTemp = accelerometerController(AxX, AxY, AxZ);
        }


	if(time <= allowedTime)
	{
		cleanScreen();
		outputText("TWOJ CZAS[ms]: ", 12, 11);
		char buf[10];
		sprintf(buf,"%lu", time);
		outputText(buf, 12, 21);
		Timer0_Wait(1000);
	}
	else
	{
		cleanScreen();
		outputText("SKUCHA :C", 12, 11);
		Timer0_Wait(1000);
	}
}

/*
 * function for setting led stripe based on light
 * Atributes:
 * uint32_t lux - read from light sensor
 * return: none
 */
void light(uint32_t lux)
{
	if(lux>2000) {
		setLEDStripe(7);
	} else if(lux>1200) {
		setLEDStripe(6);
	} else if(lux>600) {
		setLEDStripe(5);
	} else if(lux>150) {
		setLEDStripe(4);
	} else if(lux>70) {
		setLEDStripe(3);
	} else if(lux>30) {
		setLEDStripe(2);
	} else if(lux>10) {
		setLEDStripe(1);
	} else {
		setLEDStripe(0);
	}
}

int main (void) {
	sspInit();
	i2cInit();
	eeprom_init();
	joystick_init();
	oled_init();
	acc_init();
	rgb_init();
	rotary_init();
	led7seg_init();
	pca9532_init();
    light_init();

	int level = 1;
	uint8_t value;

    light_enable();
    light_setRange(LIGHT_RANGE_4000);

    srand(time(NULL));

    while(1) {
		cleanScreen();
		outputText("WYBIERZ", 12, 11);
		outputText("POZIOM", 12, 21);

    	int temp = 0;
        while(temp == 0){
        	temp = joystickController();

			level = rotaryMove(rotary_read(), level);
            value = level + '0';
            led7seg_setChar(value, FALSE);

            light(light_read());
        }
    	randomColor();
    	//getting reaction
    	reflex(level);
    }
}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}
