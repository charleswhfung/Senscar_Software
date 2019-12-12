#ifndef _DHT11_H_
#define _DHT11_H_


void delay_init(void);
void delay(uint32_t duration_us);


unsigned char values[5];

void DHT11_init();
unsigned char get_byte();
unsigned char get_data();


#endif
