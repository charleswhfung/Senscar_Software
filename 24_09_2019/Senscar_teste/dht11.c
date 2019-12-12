//* DriverLib Includes */
#include "driverlib.h"

#include <dht11.h>

/* Statics */
static volatile unsigned char teste;

void delay_init(void)
{
    Timer32_initModule(TIMER32_0_BASE, TIMER32_PRESCALER_1, TIMER32_32BIT, TIMER32_PERIODIC_MODE);

    Timer32_disableInterrupt(TIMER32_0_BASE);
}

void delay(uint32_t duration_us)
{
    Timer32_haltTimer (TIMER32_0_BASE);
    Timer32_setCount  (TIMER32_0_BASE, 12 * duration_us);// para 12MHz
    Timer32_startTimer(TIMER32_0_BASE, true);

    while (Timer32_getValue(TIMER32_0_BASE) > 0);
}

unsigned char get_byte()
{
   unsigned char s = 0;
   unsigned char value = 0;

   for(s = 0; s < 8; s += 1)
   {
      value <<= 1;
      while(!(GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN4)));
      delay(40);

      if(GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN4))
      {
          value |= 1;
      }

      while(GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN4));
      {
              if(teste == 4)
                  break;
      }
   }
   return value;
}

unsigned char get_data()
{
    //uint8_t chk = 0;
    short chk = 0;
    //unsigned char s = 0;
    unsigned char check_sum = 0;

    //STEP 1: Set pin to output HIGH which represents idle state
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN4); // Set Data pin to output direction
    GPIO_setOutputHighOnPin( GPIO_PORT_P1, GPIO_PIN4);// Set output to on

    //STEP 2: Pull down pin for 18ms(min) to denote START
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN4);// Set output to low
    delay(18 * 1000); // Low for at least 18ms

    //GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN4);// Set output to low

    //STEP 3: Pull HIGH and switch to input mode
    GPIO_setOutputHighOnPin( GPIO_PORT_P1, GPIO_PIN4);// Set output to on
    delay(40);

    GPIO_setAsInputPin(GPIO_PORT_P1, GPIO_PIN4); // Set Data pin to output direction

    chk = GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN4);

    if(chk)
    {
      return 1;
    }
    delay(80);

    chk = GPIO_getInputPinValue(GPIO_PORT_P1, GPIO_PIN4);
    if(!chk)
    {
        return 2;
    }
    delay(40);

    for(teste = 0; teste <= 4; teste += 1)
    {
       values[teste] = get_byte();
    }

    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN4); // Set Data pin to output direction
    GPIO_setOutputHighOnPin( GPIO_PORT_P1, GPIO_PIN4);// Set output to on

   for(teste = 0; teste < 4; teste += 1)
   {
       check_sum += values[teste];
   }

   if(check_sum != values[4])
   {

      return 3;
   }
   else
   {
      return 0;
   }
}

