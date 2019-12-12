
/* DriverLib Includes */
#include "driverlib.h"

/* Standard Includes */
#include <stdint.h>

#include <stdbool.h>
#include "printf.h"
#include <string.h>
#include "dht11.h"
#include <stdlib.h>
//#include <stdio.h>

#define RXBUFFER_SIZE 100
#define NUMBER_SIZE 30
#define MENSAGEM_SIZE 30

#define NUMERO_FUNCOES 18
#define TAMANHO_MAX_FUNCOES 10

//Mapa da flash///////////////////////////////////////////////////////////

#define CONTADOR 0x0003F000        //caso precise para verificar algum reset

#define TELEFONE1 0x0003F001       //primeiro telefone

#define TELEFONE2 0x0003F016       //segundo telefone
//////////////////////////////////////////////////////////////////////////

char data[15];
char dadosFlash=0;

char rxbuff[RXBUFFER_SIZE];
int rxposin=0;                              //Variável de posição do buffer de entrada da serial
int confirma=0;                             //Indica se CMGS foi recebido para poder transmitir um novo pacote
int podeEnviar=1;                           //0 - não pode enviar, 1 - pode enviar
int identifica=0;                           //Variável de estado para identificação de um SMS de entrada.

char num_identificado[NUMBER_SIZE];         //Vetor que guarda o número que foi enviado na mensagem
int pos_identificado=0;                     //Posição do vetor de armazena o número da mensagem recebida
unsigned char flag_numero=0;                //Fica em 1 apenas se identificou o número recebido, deve ser zerado assim que usar o número.
int tamanho_numero=0;                       //tamanho do número identificado

char mensagem_recebida[MENSAGEM_SIZE];
int pos_mensagem=0;
unsigned char flag_mensagem=0;
int tamanho_mensagem=0;

int erro=0;
int cont_erro=0;                            //Se receber 10 erros seguidos vai reiniciar

int limiarAlc = 2000;                       //valor inicial heuristico, em breve será arrumado
int limiarFumo = 2000;                      //valor inicial heuristico, em breve será arrumado

int init_flag=0;                            //Rotina de inicialização, só vai iniciar tudo depois de 30s
int cont_init=0;                            //Conta o tempo de 30s para a inicialização
int flag_erro=0;                            //se receber erro, vai reinicializar o sistema

char telefone1[] = "+5541991832988";        //Telefone Charles
char telefone2[] = "+5541999198205";        //Telefone Eliane
char telefone3[] = "+5541991832988";
//char telefoneBackdoor[] = "+5541992310357";
char telefoneBackdoor[] = "+5541991832988";

#define TEL01 0
#define TEL02 TEL01+1
#define ETA01 TEL02+1
#define ETA02 ETA01+1
#define ETA03 ETA02+1
#define ETA04 ETA03+1
#define ETA05 ETA04+1
#define CAR01 ETA05+1
#define CAR02 CAR01+1
#define CAR03 CAR02+1
#define CAR04 CAR03+1
#define CAR05 CAR04+1
#define GAS CAR05+1
#define ETANOL GAS+1
#define TEMP ETANOL+1
#define UMI TEMP+1
#define CADTEL01 UMI+1
#define CADTEL02 CADTEL01+1




char funcoes_programadas[NUMERO_FUNCOES][TAMANHO_MAX_FUNCOES] =
    { "Tel01",
      "Tel02",
      "Eta01",
      "Eta02",
      "Eta03",
      "Eta04",
      "Eta05",
      "Car01",
      "Car02",
      "Car03",
      "Car04",
      "Car05",
      "Gas",
      "Etanol",
      "Temp",
      "Umi",
      "Cadtel01",
      "Cadtel02"
    };


//int rxposout=0;

void comando(char *comand);
void comando2(char *comand);
void mensagem_inicial();
void mensagem_SMS(char *mensagem, int tel);
void delay_init(void);
void delay(uint32_t duration_us);
//void leituraSensorGas();
//void enviarDados_backdoor(int pos, int v1, int v2, int identificador);
void enviarDados_backdoor(int pos, int temp, int umidade, int sensor1, int sensor2, int identificador);
void mensagem_backdoor(char *mensagem);
void leituraSensores(int backdoor);
int leituraSensorEtanol();
int leituraSensorGas();
int leituraTemperatura();
int leituraUmidade();
void leituraSensoresMensagem(int etanol, int gas);
void mensagem_SMS_Extra(char *mensagem);

/* Statics */
static uint16_t resultsBuffer[8];
static uint8_t timer;
static uint8_t timer500ms;
static uint8_t timer1s;
static uint8_t timer1min;

int cont_tempo=0;//vai contar a quantidade de 100 ms até 1s, ajudando a setar os flags
int cont_min=0;


const eUSCI_UART_Config uartConfig =
{
        EUSCI_A_UART_CLOCKSOURCE_SMCLK,          // SMCLK Clock Source
        78,                                     // BRDIV = 78
        2,                                       // UCxBRF = 2
        0,                                       // UCxBRS = 0
        EUSCI_A_UART_NO_PARITY,                  // No Parity
        EUSCI_A_UART_LSB_FIRST,                  // LSB First
        EUSCI_A_UART_ONE_STOP_BIT,               // One stop bit
        EUSCI_A_UART_MODE,                       // UART mode
        EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION  // Oversampling
};


const eUSCI_UART_Config GSM_Config =
{
    EUSCI_A_UART_CLOCKSOURCE_SMCLK,          // SMCLK Clock Source
    78,                                      // BRDIV = 78
    2,                                       // UCxBRF = 2
    0,                                       // UCxBRS = 0
    EUSCI_A_UART_NO_PARITY,                  // No Parity
    EUSCI_A_UART_LSB_FIRST,                  // LSB First
    EUSCI_A_UART_ONE_STOP_BIT,               // One stop bit
    EUSCI_A_UART_MODE,// UART mode
    EUSCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION  // Oversampling
};

const Timer_A_ContinuousModeConfig continuousModeConfig =

{

        TIMER_A_CLOCKSOURCE_SMCLK,           // SMCK

        TIMER_A_CLOCKSOURCE_DIVIDER_20,      // 3MHz/1

        TIMER_A_TAIE_INTERRUPT_ENABLE,      // Enable Overflow ISR

        TIMER_A_DO_CLEAR                    // Clear Counter

};

int main(void)
{

    volatile uint32_t ii;
    int comp_resultado=0;
    int func=0;//apenas um contador que procura qual função que o usuário quer acessar
    char funcFlash=0xAA;
    int leitura;
    //char *dados;
    char aux[10];                     //Auxiliar para converter números para string


    //int umidade,temperatura,teste;
    //int sensor1, sensor2;



    /* Halting WDT  */
       WDT_A_holdTimer();


    // Setting DCO to 12MHz
       CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_12);


/////////////Memoria Flash//////////////////////////////////////////////////////////////////////


       //Faz a leitura da flash, se tiver 0xAA, os telefones foram modificados

       dadosFlash = *(uint8_t*) (CONTADOR);


       if(dadosFlash==0xAA){
           for(ii=0;ii<14;ii++)
           {

               telefone1[ii] = *(uint8_t*) (ii+TELEFONE1);

           }
           telefone1[ii] = '\0';

           for(ii=0;ii<14;ii++)
           {

               telefone2[ii] = *(uint8_t*) (ii+TELEFONE2);

           }
           telefone2[ii] = '\0';
       }


///////////////ADC14////////////////////////////////////////////////////////////////////////////

       // Initializing ADC (MCLK/1/4)
       ADC14_enableModule();
       ADC14_initModule(ADC_CLOCKSOURCE_MCLK, ADC_PREDIVIDER_1, ADC_DIVIDER_4,0);

       // Configuring GPIOs
       GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P5,
               GPIO_PIN5 | GPIO_PIN4 | GPIO_PIN3 | GPIO_PIN2 | GPIO_PIN1 | GPIO_PIN0,
               GPIO_TERTIARY_MODULE_FUNCTION);
       GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4,
               GPIO_PIN7 | GPIO_PIN6,
               GPIO_TERTIARY_MODULE_FUNCTION);

       //Configuring ADC Memory
       ADC14_configureMultiSequenceMode(ADC_MEM0, ADC_MEM7, false);

       ADC14_configureConversionMemory(ADC_MEM0, ADC_VREFPOS_AVCC_VREFNEG_VSS, ADC_INPUT_A0, false);
       ADC14_configureConversionMemory(ADC_MEM1, ADC_VREFPOS_AVCC_VREFNEG_VSS, ADC_INPUT_A1, false);
       ADC14_configureConversionMemory(ADC_MEM2, ADC_VREFPOS_AVCC_VREFNEG_VSS, ADC_INPUT_A2, false);
       ADC14_configureConversionMemory(ADC_MEM3, ADC_VREFPOS_AVCC_VREFNEG_VSS, ADC_INPUT_A3, false);
       ADC14_configureConversionMemory(ADC_MEM4, ADC_VREFPOS_AVCC_VREFNEG_VSS, ADC_INPUT_A4, false);
       ADC14_configureConversionMemory(ADC_MEM5, ADC_VREFPOS_AVCC_VREFNEG_VSS, ADC_INPUT_A5, false);
       ADC14_configureConversionMemory(ADC_MEM6, ADC_VREFPOS_AVCC_VREFNEG_VSS, ADC_INPUT_A6, false);
       ADC14_configureConversionMemory(ADC_MEM7, ADC_VREFPOS_AVCC_VREFNEG_VSS, ADC_INPUT_A7, false);

       /* Enabling the interrupt when a conversion on channel 7 (end of sequence)
       *  is complete and enabling conversions */
       ADC14_enableInterrupt(ADC_INT7);

       /* Enabling Interrupts */
       Interrupt_enableInterrupt(INT_ADC14);
       Interrupt_enableMaster();

       /* Setting up the sample timer to automatically step through the sequence convert.*/
       ADC14_enableSampleTimer(ADC_AUTOMATIC_ITERATION);

       /* Triggering the start of the sample */
       ADC14_enableConversion();
       ADC14_toggleConversionTrigger();


///////////////Timer A////////////////////////////////////////////////////////////////////////////

//Configuração do timer A para noção de tempo, por volta de 100 ms

       timer=0;
       timer500ms=0;
       timer1s=0;
       timer1min=0;


       /* Configuring Continuous Mode */
       Timer_A_configureContinuousMode(TIMER_A0_BASE, &continuousModeConfig);

       /* Enabling interrupts  */
       Interrupt_enableInterrupt(INT_TA0_N);

       /* Starting the Timer_A0 in continuous mode */
       Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_CONTINUOUS_MODE);


///////////////UART////////////////////////////////////////////////////////////////////////////
       /* Selecting P1.2 and P1.3 in UART mode */
       GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P1,
                  GPIO_PIN1 | GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);

       /* Configure pins P3.2 and P3.3 in UART mode.*/
       GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P3,
                  GPIO_PIN2 | GPIO_PIN3, GPIO_PRIMARY_MODULE_FUNCTION);


       /* Configuring UART Module */
       UART_initModule(EUSCI_A0_BASE, &uartConfig);

       /* Configuring UART Module */
       UART_initModule(EUSCI_A2_BASE, &GSM_Config);

       /* Enable UART module */
       UART_enableModule(EUSCI_A0_BASE);

       /* Enable UART module */
       UART_enableModule(EUSCI_A2_BASE);


       /* Enabling interrupts */
       UART_enableInterrupt(EUSCI_A0_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
       Interrupt_enableInterrupt(INT_EUSCIA0);

       /* Enabling interrupts2 */
       UART_enableInterrupt(EUSCI_A2_BASE, EUSCI_A_UART_RECEIVE_INTERRUPT);
       Interrupt_enableInterrupt(INT_EUSCIA2);


///////////////LED////////////////////////////////////////////////////////////////////////////
       GPIO_setAsOutputPin   (GPIO_PORT_P1, GPIO_PIN0);
       GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);

///////////////Ativar Modem////////////////////////////////////////////////////////////////////////////

       GPIO_setAsInputPin(GPIO_PORT_P4, GPIO_PIN6);

       GPIO_setAsOutputPin(GPIO_PORT_P3, GPIO_PIN5);

       delay_init();

      //Ativar o modem

      if(!GPIO_getInputPinValue(GPIO_PORT_P4, GPIO_PIN6)){

          GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN5);
          delay(2000000);

           GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN5);
           delay(2000000);

           GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN5);
           delay(5000000);
      }



      mensagem_SMS("Senscar: DISPOSITIVO INICIALIZADO", 12);
      //mensagem_inicial();

      while(1)
      {

           if(timer){//Rotinas a cada 100ms
               if(flag_erro){
                   reinicializaSistema();
               }
               //ADC14_enableModule();
               //GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);
               timer=0;

           }

           if(timer500ms){//Rotinas a cada 500ms



               timer500ms=0;
           }
           if(timer1s){//Rotinas a cada 1s
               //delay(1*1000*1000);
               if(!init_flag){
                   cont_init++;
                   printf(EUSCI_A0_BASE,"\r\nInicio=%i ",cont_init);
                   if(cont_init==30){
                       init_flag=1;
                       cont_init=0;
                   }
               }
               else{
                   leituraSensores(0);//Vai monitorar os sensores a cada segundo, se ocorrer algum evento estranho irá disparar

               }
               GPIO_toggleOutputOnPin(GPIO_PORT_P1, GPIO_PIN0);
               //leituraSensorGas();

               timer1s=0;
           }
           if(timer1min){//Rotinas a cada 1 min
               //leituraSensorGas();
               //leituraSensores(1);//vai enviar para o backdoor a cada um minuto
               timer1min=0;
           }


           if(flag_numero){
               printf(EUSCI_A0_BASE,"%s","\r\nNumero identificado: ");
               for(ii=0;ii<tamanho_numero;ii++){
                   printf(EUSCI_A0_BASE,"%c",num_identificado[ii]);
               }
               flag_numero=0;

           }

           if(flag_mensagem){//Tem mensagem nova, vou verificar se é alguma mensagem conhecida

              printf(EUSCI_A0_BASE,"%s","\r\nMensagem identificada: ");
                 for(ii=0;ii<tamanho_mensagem;ii++){
                     printf(EUSCI_A0_BASE,"%c",mensagem_recebida[ii]);
                 }
              for(func=0;func<NUMERO_FUNCOES;func++){

                  comp_resultado=strcmp(mensagem_recebida, funcoes_programadas[func]);

                  if(comp_resultado==0){

                      if(func==TEL01){
                          for(ii=0;ii<tamanho_numero;ii++){
                              //printf(EUSCI_A0_BASE,"%c",mensagem_recebida[ii]);
                              telefone1[ii]=num_identificado[ii];
                          }
                          telefone1[ii]='\0';

                          /* Unprotecting Info Bank 0, Sector 0  */
                          MAP_FlashCtl_unprotectSector(FLASH_MAIN_MEMORY_SPACE_BANK1,FLASH_SECTOR31);

                          /* Trying to erase the sector */

                          if(!MAP_FlashCtl_eraseSector(TELEFONE1))
                                  while(1);

                          if(!MAP_FlashCtl_programMemory(telefone1,(void*) TELEFONE1, 15))
                                  while(1);

                          if(!MAP_FlashCtl_programMemory(telefone2,(void*) TELEFONE2, 15))
                                  while(1);

                          if(!MAP_FlashCtl_programMemory(&funcFlash,(void*) CONTADOR, 1))
                                  while(1);

                          /* Setting the sector back to protected  */
                          MAP_FlashCtl_protectSector(FLASH_MAIN_MEMORY_SPACE_BANK1,FLASH_SECTOR31);

                          for(ii=0;ii<14;ii++)
                          {

                              telefone1[ii] = *(uint8_t*) (ii+TELEFONE1);

                          }
                          telefone1[ii] = '\0';

                          for(ii=0;ii<14;ii++)
                          {

                              telefone2[ii] = *(uint8_t*) (ii+TELEFONE2);

                          }
                          telefone2[ii] = '\0';



                          printf(EUSCI_A0_BASE,"%s","\r\n");
                          printf(EUSCI_A0_BASE,"%s",telefone1);
                          mensagem_SMS("Senscar: Telefone primário configurado",12);
                          //mensagem_SMS("Senscar: Telefone primário configurado",2);
                          break;
                      }
                      else if(func==TEL02){
                          for(ii=0;ii<tamanho_numero;ii++){
                              //printf(EUSCI_A0_BASE,"%c",mensagem_recebida[ii]);
                              telefone2[ii]=num_identificado[ii];
                          }
                          telefone2[ii]='\0';

                          /* Unprotecting Info Bank 0, Sector 0  */
                          MAP_FlashCtl_unprotectSector(FLASH_MAIN_MEMORY_SPACE_BANK1,FLASH_SECTOR31);

                          if(!MAP_FlashCtl_eraseSector(TELEFONE1))
                              while(1);

                          if(!MAP_FlashCtl_programMemory(telefone1,(void*) TELEFONE1, 15))
                              while(1);

                          if(!MAP_FlashCtl_programMemory(telefone2,(void*) TELEFONE2, 15))
                              while(1);

                          if(!MAP_FlashCtl_programMemory(&funcFlash,(void*) CONTADOR, 1))
                              while(1);

                          /* Setting the sector back to protected  */
                          MAP_FlashCtl_protectSector(FLASH_MAIN_MEMORY_SPACE_BANK1,FLASH_SECTOR31);

                          for(ii=0;ii<14;ii++)
                          {

                              telefone1[ii] = *(uint8_t*) (ii+TELEFONE1);

                          }
                          telefone1[ii] = '\0';

                          for(ii=0;ii<14;ii++)
                          {

                              telefone2[ii] = *(uint8_t*) (ii+TELEFONE2);

                          }
                          telefone2[ii] = '\0';


                          printf(EUSCI_A0_BASE,"%s","\r\n");
                          printf(EUSCI_A0_BASE,"%s",telefone2);
                          mensagem_SMS("Senscar: Telefone secundário configurado",12);
                          //mensagem_SMS("Senscar: Telefone secundário configurado",2);
                          break;
                      }
                      else if(func==ETA01){
                          limiarAlc = 1489; //Equivale a aproximadamente 0,3V
                          mensagem_SMS("Senscar: Novo limite de etanol configurado",12);
                          //mensagem_SMS("Senscar: Novo limite de etanol configurado",2);
                          break;
                      }
                      else if(func==ETA02){
                          limiarAlc = 2000; //Equivale a aproximadamente 0,4V
                          mensagem_SMS("Senscar: Novo limite de etanol configurado",12);
                          //mensagem_SMS("Senscar: Novo limite de etanol configurado",2);
                          break;
                      }
                      else if(func==ETA03){
                          limiarAlc = 2482; //Equivale a aproximadamente 0,5V
                          mensagem_SMS("Senscar: Novo limite de etanol configurado",12);
                          //mensagem_SMS("Senscar: Novo limite de etanol configurado",2);
                          break;
                      }
                      else if(func==ETA04){
                          limiarAlc = 2978; //Equivale a aproximadamente 0,6V
                          mensagem_SMS("Senscar: Novo limite de etanol configurado",12);
                          //mensagem_SMS("Senscar: Novo limite de etanol configurado",2);
                          break;
                      }

                      else if(func==ETA05){
                          limiarAlc = 3500; //Equivale a aproximadamente 0,7V
                          mensagem_SMS("Senscar: Novo limite de etanol configurado",12);
                          //mensagem_SMS("Senscar: Novo limite de etanol configurado",2);
                          break;
                      }
                      else if(func==CAR01){
                          limiarFumo = 1489; //Equivale a aproximadamente 0,3V
                          mensagem_SMS("Senscar: Novo limite de fumaca configurado",12);
                          //mensagem_SMS("Senscar: Novo limite de etanol configurado",2);
                          break;
                    }
                    else if(func==CAR02){
                          limiarFumo = 2000; //Equivale a aproximadamente 0,4V
                          mensagem_SMS("Senscar: Novo limite de fumaca configurado",12);
                          //mensagem_SMS("Senscar: Novo limite de etanol configurado",2);
                          break;
                    }
                    else if(func==CAR03){
                          limiarFumo = 2482; //Equivale a aproximadamente 0,5V
                          mensagem_SMS("Senscar: Novo limite de fumaca configurado",12);
                          //mensagem_SMS("Senscar: Novo limite de etanol configurado",2);
                          break;
                    }
                    else if(func==CAR04){
                          limiarFumo = 2978; //Equivale a aproximadamente 0,6V
                          mensagem_SMS("Senscar: Novo limite de fumaca configurado",12);
                          //mensagem_SMS("Senscar: Novo limite de etanol configurado",2);
                          break;
                    }

                    else if(func==CAR05){
                          limiarFumo = 3500; //Equivale a aproximadamente 0,7V
                          mensagem_SMS("Senscar: Novo limite de fumaca configurado",12);
                          //mensagem_SMS("Senscar: Novo limite de etanol configurado",2);
                          break;
                    }
                    if(func==GAS){
                        for(ii=0;ii<tamanho_numero;ii++){
                            printf(EUSCI_A0_BASE,"%c",mensagem_recebida[ii]);
                            telefone3[ii]=num_identificado[ii];
                        }
                        telefone3[ii]='\0';
                        printf(EUSCI_A0_BASE,"%s","\r\n");
                        printf(EUSCI_A0_BASE,"%s",telefone3);

                        leitura = leituraSensorGas();
                        sprintf(aux,"%i", leitura);           //Converte gas para string
                        mensagem_SMS_Extra(aux);
                        //leituraSensoresMensagem(leituraSensorEtanol(), leituraSensorGas());
                        //mensagem_SMS("Senscar: Telefone primário configurado",2);
                        break;
                    }
                    if(func==ETANOL){
                        for(ii=0;ii<tamanho_numero;ii++){
                            printf(EUSCI_A0_BASE,"%c",mensagem_recebida[ii]);
                            telefone3[ii]=num_identificado[ii];
                        }
                        telefone3[ii]='\0';
                        printf(EUSCI_A0_BASE,"%s","\r\n");
                        printf(EUSCI_A0_BASE,"%s",telefone3);

                        leitura = leituraSensorEtanol();
                        sprintf(aux,"%i", leitura);           //Converte gas para string
                        mensagem_SMS_Extra(aux);

                        //leituraSensoresMensagem(leituraSensorEtanol(), leituraSensorGas());
                        //mensagem_SMS("Senscar: Telefone primário configurado",2);
                        break;
                    }
                    if(func==TEMP){
                        for(ii=0;ii<tamanho_numero;ii++){
                            printf(EUSCI_A0_BASE,"%c",mensagem_recebida[ii]);
                            telefone3[ii]=num_identificado[ii];
                        }
                        telefone3[ii]='\0';
                        printf(EUSCI_A0_BASE,"%s","\r\n");
                        printf(EUSCI_A0_BASE,"%s",telefone3);


                        leitura = leituraTemperatura();
                        sprintf(aux,"%i", leitura);           //Converte gas para string
                        mensagem_SMS_Extra(aux);

                        //leituraSensoresMensagem(leituraSensorEtanol(), leituraSensorGas());
                        //mensagem_SMS("Senscar: Telefone primário configurado",2);
                        break;
                    }
                    if(func==UMI){
                        for(ii=0;ii<tamanho_numero;ii++){
                            printf(EUSCI_A0_BASE,"%c",mensagem_recebida[ii]);
                            telefone3[ii]=num_identificado[ii];
                        }
                        telefone3[ii]='\0';
                        printf(EUSCI_A0_BASE,"%s","\r\n");
                        printf(EUSCI_A0_BASE,"%s",telefone3);

                        leitura = leituraUmidade();
                        sprintf(aux,"%i", leitura);           //Converte gas para string
                        mensagem_SMS_Extra(aux);
                        //leituraSensoresMensagem(leituraSensorEtanol(), leituraSensorGas());
                        //mensagem_SMS("Senscar: Telefone primário configurado",2);
                        break;
                    }

                    if(func==CADTEL01){
                        for(ii=0;ii<tamanho_numero;ii++){
                            printf(EUSCI_A0_BASE,"%c",mensagem_recebida[ii]);
                            telefone3[ii]=num_identificado[ii];
                        }
                        telefone3[ii]='\0';
                        printf(EUSCI_A0_BASE,"%s","\r\n");
                        printf(EUSCI_A0_BASE,"%s",telefone3);

                        //leitura = leituraUmidade();
                        //sprintf(aux,"%i", leitura);           //Converte gas para string
                        mensagem_SMS_Extra(telefone1);
                        //leituraSensoresMensagem(leituraSensorEtanol(), leituraSensorGas());
                        //mensagem_SMS("Senscar: Telefone primário configurado",2);
                        break;
                    }
                    if(func==CADTEL02){
                        for(ii=0;ii<tamanho_numero;ii++){
                            printf(EUSCI_A0_BASE,"%c",mensagem_recebida[ii]);
                            telefone3[ii]=num_identificado[ii];
                        }
                        telefone3[ii]='\0';
                        printf(EUSCI_A0_BASE,"%s","\r\n");
                        printf(EUSCI_A0_BASE,"%s",telefone3);

                        //leitura = leituraUmidade();
                        //sprintf(aux,"%i", leitura);           //Converte gas para string
                        mensagem_SMS_Extra(telefone2);
                        //leituraSensoresMensagem(leituraSensorEtanol(), leituraSensorGas());
                        //mensagem_SMS("Senscar: Telefone primário configurado",2);
                        break;
                    }




                  }
              }
              if(func==NUMERO_FUNCOES){//Não encontrou nenhuma função
                  mensagem_SMS("Senscar: Mensagem não reconhecida",12);
                  //mensagem_SMS("Senscar: Mensagem não reconhecida",2);
              }

              flag_mensagem=0;

          }


       }


}

void reinicializaSistema(){

    //Desativar o modem

      GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN5);
      delay(2000000);

      GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN5);
      delay(2000000);

      GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN5);
      delay(5000000);

      //Ativa o modem

      GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN5);
      delay(2000000);

      GPIO_setOutputHighOnPin(GPIO_PORT_P3, GPIO_PIN5);
      delay(2000000);

      GPIO_setOutputLowOnPin(GPIO_PORT_P3, GPIO_PIN5);
      delay(5000000);


      //Reinicializa todas as variáveis, é como se tivesse começado tudo de novo
        cont_init=0;
        init_flag=0;
        flag_erro=0;//se receber erro, vai reinicializar o sistema
        //wtd_counter=0;
        rxposin=0;                              //Variável de posição do buffer de entrada da serial
        confirma=0;                             //Indica se CMGS foi recebido para poder transmitir um novo pacote
        erro=0;
        podeEnviar=1;                           //0 - não pode enviar, 1 - pode enviar
        identifica=0;                           //Variável de estado para identificação de um SMS de entrada.


        pos_identificado=0;                     //Posição do vetor de armazena o número da mensagem recebida
        flag_numero=0;                //Fica em 1 apenas se identificou o número recebido, deve ser zerado assim que usar o número.
        tamanho_numero=0;                       //tamanho do número identificado


        pos_mensagem=0;
        flag_mensagem=0;
        tamanho_mensagem=0;
        //cont_mensagens=0;                       //contador de mensagens enviadas, deve enviar no máximo cinco mensagens a cada meia hora.


        //mensagem_SMS("Senscar: DISPOSITIVO INICIALIZADO", 12);
        //mensagem_inicial();

}







void leituraSensoresMensagem(int etanol, int gas){
    char *dados;
    char aux[10];                     //Auxiliar para converter números para string
    char aux2[30]="Leitura dos sensores: Etanol: ";
    char aux3[]=" Gas: ";
    sprintf(aux,"%i", etanol);        //Converte etanol para string
    dados = strcat (aux2,aux);        //Concatena com Etanol
    dados = strcat (dados,aux3);      //Concatena a segunda string
    sprintf(aux,"%i", gas);           //Converte etanol para string
    dados = strcat (dados,aux);       //Concatena com Gas
    mensagem_SMS(dados,12);

}




int leituraSensorEtanol(){
    int sensor1;
    int temperatura;

    MAP_ADC14_toggleConversionTrigger();

    sensor1 = (int)(resultsBuffer[0]);
    temperatura = leituraTemperatura();

    if(temperatura<0){
        sensor1 = sensor1*1.35;
    }
    else if(temperatura<10){
        sensor1 = sensor1*1.15;
    }
    else if(temperatura<20){
        sensor1 = sensor1*1.00;
    }
    else if(temperatura<30){
        sensor1 = sensor1*0.90;
    }
    else if(temperatura<40){
        sensor1 = sensor1*0.85;
    }
    else if(temperatura>=40){
        sensor1 = sensor1*0.80;
    }

    printf(EUSCI_A0_BASE,"sensor1=%i \r\n",sensor1);

    return sensor1;
}

int leituraSensorGas(){
    int sensor2;
    int temperatura;
    MAP_ADC14_toggleConversionTrigger();

    sensor2 = (int)(resultsBuffer[1]);

    if(temperatura<0){
        sensor2 = sensor2*1.35;
    }
    else if(temperatura<10){
        sensor2 = sensor2*1.15;
    }
    else if(temperatura<20){
        sensor2 = sensor2*1.00;
    }
    else if(temperatura<30){
        sensor2 = sensor2*0.90;
    }
    else if(temperatura<40){
        sensor2 = sensor2*0.85;
    }
    else if(temperatura>=40){
        sensor2 = sensor2*0.80;
    }

    printf(EUSCI_A0_BASE,"sensor2=%i \r\n",sensor2);

    return sensor2;
}

int leituraTemperatura(){
    int temperatura,teste;

    teste = get_data();

    temperatura = (int)(values[2]);

    printf(EUSCI_A0_BASE,"Temp=%i \r\n", temperatura);

    return temperatura;

}

int leituraUmidade(){
    int umidade,teste;

    teste = get_data();

    umidade = (int)(values[0]);

    printf(EUSCI_A0_BASE,"Humidity=%i \r\n", umidade);

    return umidade;

}






void leituraSensores(int backdoor){
    int umidade,temperatura,teste;
    int sensor1, sensor2;

    teste = get_data();

    umidade = (int)(values[0]);
    temperatura = (int)(values[2]);

    printf(EUSCI_A0_BASE,"teste=%i, Humidity=%i, Temp=%i \r\n",teste, umidade, temperatura);

    MAP_ADC14_toggleConversionTrigger();

    sensor1 = (int)(resultsBuffer[0]);
    sensor2 = (int)(resultsBuffer[1]);

    if(temperatura<0){
        sensor1 = sensor1*1.35;
        sensor2 = sensor2*1.35;
    }
    else if(temperatura<10){
        sensor1 = sensor1*1.15;
        sensor2 = sensor2*1.15;

    }
    else if(temperatura<20){
        sensor1 = sensor1*1.00;
        sensor2 = sensor2*1.00;
    }
    else if(temperatura<30){
        sensor1 = sensor1*0.90;
        sensor2 = sensor2*0.90;
    }
    else if(temperatura<40){
        sensor1 = sensor1*0.85;
        sensor2 = sensor2*0.85;
    }
    else if(temperatura>=40){
        sensor1 = sensor1*0.80;
        sensor2 = sensor2*0.80;
    }





    printf(EUSCI_A0_BASE,"sensor1=%i, sensor2=%i, Temp=%i \r\n",sensor1, sensor2);

    if(sensor1>limiarAlc){
        mensagem_SMS("Senscar: Foi detectado nivel alto de etanol",12);
        //backdoor=1;//vai enviar os dados para o backdoor
    }
    if(sensor2>limiarFumo){
        mensagem_SMS("Senscar: Foi detectado nivel alto de gas carbonico",12);
        //backdoor=1;//vai enviar os dados para o backdoor
    }

/*
    if(backdoor){//Se for requisitado, envia os dados para o backdoor
        enviarDados_backdoor(1, temperatura, umidade, sensor1, sensor2,1);
    }
*/
}


/*
void leituraSensorGas(){
    int i=0;
    int disparoAlc=0;
    int disparoFumo=0;
    //char mensagem[100]="Senscar: Foi detectado nivel alto de etanol no ";
    //char sensor[100];

    ADC14_enableModule();//Realiza a leitura do sensor
    printf(EUSCI_A0_BASE,"%s","\r\n");
    printf(EUSCI_A0_BASE,"%s","Leitura dos Sensores:");
    //Verifica se alguma leitura passou do limiar programado
    for(i=0;i<6;i++){
        if(resultsBuffer[i]>limiarAlc){
            disparoAlc=1;
        }

    }
    if(disparoAlc){
        mensagem_SMS("Senscar: Foi detectado nivel alto de etanol",12);
        //mensagem_SMS("Senscar: Foi detectado nivel alto de etanol",2);
        disparoAlc=0;
    }
    for(i=5;i<7;i++){
        if(resultsBuffer[i]>limiarFumo){
            disparoFumo=1;
        }

    }
    if(disparoFumo){
        mensagem_SMS("Senscar: Foi detectado nivel alto de gas carbonico",12);
        //mensagem_SMS("Senscar: Foi detectado nivel alto de gas carbonico",2);
        disparoFumo=0;
    }
}

*/




void TA0_N_IRQHandler(void){

    Timer_A_clearInterruptFlag(TIMER_A0_BASE);

    timer = 1;
    cont_tempo++;
    if(cont_tempo==5){
        timer500ms=1;
    }
    if(cont_tempo==10){
        timer1s=1;
        cont_tempo=0;
        cont_min++;
    }
    if(cont_min==60){
        cont_min=0;
        timer1min=1;

    }
}


void ADC14_IRQHandler(void)

{

    uint64_t status;



        status = ADC14_getEnabledInterruptStatus();

        ADC14_clearInterruptFlag(status);



        if(status & ADC_INT7)

        {

            ADC14_getMultiSequenceResult(resultsBuffer);



            ADC14_disableModule();

        }

}





/* EUSCI A0 UART ISR - Echoes data back to PC host */
void EUSCIA0_IRQHandler(void)
{
    uint32_t status = MAP_UART_getEnabledInterruptStatus(EUSCI_A0_BASE);

    //MAP_UART_clearInterruptFlag(EUSCI_A0_BASE, status);

    //printf(EUSCI_A0_BASE, "%s","IRQ\n\r");

    if(status & EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG)
    {
        MAP_UART_transmitData(EUSCI_A0_BASE, 'A');

    }

    MAP_UART_clearInterruptFlag(EUSCI_A0_BASE, status);

}



void EUSCIA2_IRQHandler(void)
{
    uint32_t status = MAP_UART_getEnabledInterruptStatus(EUSCI_A2_BASE);

    MAP_UART_clearInterruptFlag(EUSCI_A2_BASE, status);

    if(status & EUSCI_A_UART_RECEIVE_INTERRUPT_FLAG)
    {
        //MAP_UART_transmitData(EUSCI_A1_BASE, MAP_UART_receiveData(EUSCI_A2_BASE));
        //MAP_GPIO_setOutputHighOnPin(GPIO_PORT_P2, GPIO_PIN1);
        rxbuff[rxposin] = UART_receiveData(EUSCI_A2_BASE);
        if(confirma==0 && rxbuff[rxposin]=='+'){
            confirma=1;
        }
        else if(confirma==1 && rxbuff[rxposin]=='C'){
            confirma=2;
        }
        else if(confirma==2 && rxbuff[rxposin]=='M'){
            confirma=3;
        }
        else if(confirma==3 && rxbuff[rxposin]=='G'){
            confirma=4;
        }
        else if(confirma==4 && rxbuff[rxposin]=='S'){
            confirma=5;
        }
        else{
            confirma=0;
        }
        if(confirma==5){
            MAP_UART_transmitData(EUSCI_A0_BASE, 'C');
            MAP_UART_transmitData(EUSCI_A0_BASE, 'O');
            MAP_UART_transmitData(EUSCI_A0_BASE, 'N');
            MAP_UART_transmitData(EUSCI_A0_BASE, 'F');
            MAP_UART_transmitData(EUSCI_A0_BASE, 'I');
            MAP_UART_transmitData(EUSCI_A0_BASE, 'R');
            MAP_UART_transmitData(EUSCI_A0_BASE, 'M');
            MAP_UART_transmitData(EUSCI_A0_BASE, 'A');
            podeEnviar=1;
            confirma=0;
            cont_erro=0;
        }



        if(erro==0 && rxbuff[rxposin]=='E'){
            erro=1;
        }
        else if(erro==1 && rxbuff[rxposin]=='R'){
            erro=2;
        }
        else if(erro==2 && rxbuff[rxposin]=='R'){
            erro=3;
        }
        else if(erro==3 && rxbuff[rxposin]=='O'){
            erro=4;
        }
        else if(erro==4 && rxbuff[rxposin]=='R'){
            erro=5;
        }
        if(erro==5){
            MAP_UART_transmitData(EUSCI_A0_BASE, 'E');
            MAP_UART_transmitData(EUSCI_A0_BASE, 'R');
            MAP_UART_transmitData(EUSCI_A0_BASE, 'R');
            MAP_UART_transmitData(EUSCI_A0_BASE, 'O');
            erro=0;
            //flag_erro=1;

            podeEnviar=1;
            confirma=0;
            cont_erro++;
            if(cont_erro==10){
                printf(EUSCI_A0_BASE, "%s","RESETA!!!!\r\n");
                flag_erro=1;
                cont_erro=0;
            }
        }






        if(identifica==0 && rxbuff[rxposin]=='+'){
            identifica=1;
            rxposin=0;
        }
        else if(identifica==1 && rxbuff[rxposin]=='C'){
            identifica=2;
        }
        else if(identifica==2 && rxbuff[rxposin]=='M'){
            identifica=3;
        }
        else if(identifica==3 && rxbuff[rxposin]=='T'){
            identifica=4;
        }
        else if(identifica==4 && rxbuff[rxposin]==':'){
            identifica=5;
        }
        else if(identifica==5 && rxbuff[rxposin]==' '){
            identifica=6;
        }
        else if(identifica==6 && rxbuff[rxposin]=='"'){
            identifica=7;
        }
        else if(identifica==7){
            if(rxbuff[rxposin]=='"'){
                identifica=8;
                tamanho_numero = pos_identificado;
                pos_identificado=0;
                flag_numero=1;
            }
            else{
                num_identificado[pos_identificado]=rxbuff[rxposin];
                pos_identificado++;
            }


        }

        else if(identifica==8 && rxbuff[rxposin]==10){
            identifica=9;
        }

        else if(identifica==9){
            if(rxbuff[rxposin]==13){
                identifica=0;
                mensagem_recebida[pos_mensagem]='\0';
                tamanho_mensagem = pos_mensagem;
                pos_mensagem=0;
                flag_mensagem=1;
            }
            else{
                mensagem_recebida[pos_mensagem]=rxbuff[rxposin];
                pos_mensagem++;
            }

        }

        rxposin++;
        if(rxposin>=RXBUFFER_SIZE){
            rxposin=0;
        }
        MAP_UART_transmitData(EUSCI_A0_BASE, UART_receiveData(EUSCI_A2_BASE));


    }

    //MAP_GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_PIN1);
}


/*
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
*/

/*
void enviarDados_backdoor(int pos, int v1, int v2, int identificador){

    //Formato
    //|F|POS|A|V1|A|V2|A|F|
    // 1  1  1 5  1  5 1 1

    //enviarDados_backdoor(1, temperatura,umidade,1);
    //enviarDados_backdoor(2, sensor1, sensor2,1);

  char *dados;
  char aux[10];                     //Auxiliar para converter números para string
  char aux2[20]="F";                //Começa com a letra F, mas vai guardar os dados da mensagem por ponteiro
  char aux3[]="A";                  //Auxiliar que possui a letra A
  char aux4[]="F";
  sprintf(aux,"%i", pos);           //Converte pos para string
  dados = strcat (aux2,aux);        //Concatena F com pos
  dados = strcat (dados,aux3);      //Concatena com 'A'
  sprintf(aux,"%i", v1);
  dados = strcat (dados,aux);       //Concatena com v1
  dados = strcat (dados,aux3);      //Concatena com 'A'
  sprintf(aux,"%i", v2);
  dados = strcat (dados,aux);       //Concatena com v2
  dados = strcat (dados,aux3);      //Concatena com 'A'
  sprintf(aux,"%i", identificador);
  dados = strcat (dados,aux);       //Concatena com identificador
  dados = strcat (dados,aux3);      //Concatena com 'A'
  dados = strcat (dados,aux4);      //Concatena com 'F'
  printf(EUSCI_A0_BASE,"\r\nValor convertido: %s",dados);

  mensagem_backdoor(dados);
}*/




void enviarDados_backdoor(int pos, int temp, int umidade, int sensor1, int sensor2, int identificador){

    //Formato
    //|F|POS|A|V1|A|V2|A|F|
    // 1  1  1 5  1  5 1 1


    //|F|POS|A|TEMP|A|UMI|A|S1|A|S2|A|IDENT|A|F|

    //enviarDados_backdoor(1, temperatura,umidade,1);
    //enviarDados_backdoor(2, sensor1, sensor2,1);

  char *dados;
  char aux[10];                     //Auxiliar para converter números para string
  char aux2[20]="F";                //Começa com a letra F, mas vai guardar os dados da mensagem por ponteiro
  char aux3[]="A";                  //Auxiliar que possui a letra A
  char aux4[]="F";
  sprintf(aux,"%i", pos);           //Converte pos para string
  dados = strcat (aux2,aux);        //Concatena F com pos
  dados = strcat (dados,aux3);      //Concatena com 'A'
  sprintf(aux,"%i", temp);
  dados = strcat (dados,aux);       //Concatena com temperatura
  dados = strcat (dados,aux3);      //Concatena com 'A'
  sprintf(aux,"%i", umidade);
  dados = strcat (dados,aux);       //Concatena com umidade
  dados = strcat (dados,aux3);      //Concatena com 'A'
  sprintf(aux,"%i", sensor1);
  dados = strcat (dados,aux);       //Concatena com sensor1
  dados = strcat (dados,aux3);      //Concatena com 'A'
  sprintf(aux,"%i", sensor2);
  dados = strcat (dados,aux);       //Concatena com sensor2
  dados = strcat (dados,aux3);      //Concatena com 'A'
  sprintf(aux,"%i", identificador);
  dados = strcat (dados,aux);       //Concatena com identificador
  dados = strcat (dados,aux3);      //Concatena com 'A'
  dados = strcat (dados,aux4);      //Concatena com 'F'
  printf(EUSCI_A0_BASE,"\r\nValor convertido: %s",dados);

  mensagem_backdoor(dados);
}


/*
void comando(char *comand){

    //char *mensagem;
    //strcat(comand,"\n\r");
    //mensagem = strcat(comand," teste");

    //printf(EUSCI_A0_BASE, "%s",mensagem);
    printf(EUSCI_A0_BASE, "%s",comand);

}

void comando2(char *comand){

    //char *mensagem;
    //strcat(comand,"\n\r");
    //mensagem = strcat(comand," teste");

    //printf(EUSCI_A0_BASE, "%s",mensagem);
    printf(EUSCI_A0_BASE, "%s",comand);
    printf(EUSCI_A2_BASE, "%s",comand);


}
*/
void mensagem_backdoor(char *mensagem){
    long ii;

    while(!podeEnviar){//Se não pode enviar, espere!!!!
        if(flag_erro){//deu erro, esquece tudo e volta para o começo
            reinicializaSistema();
            return;
        }
    }
    podeEnviar=0;       //Estou enviando

    printf(EUSCI_A0_BASE, "%s","TESTE\r\n");

    printf(EUSCI_A2_BASE, "%s","AT\r\n");
    for(ii=0;ii<5000;ii++){}

    printf(EUSCI_A2_BASE, "%s","AT+CMGF=1\r\n");
    for(ii=0;ii<5000;ii++){}

    printf(EUSCI_A2_BASE, "%s","ATE0\r\n");
    for(ii=0;ii<50000;ii++){}


    printf(EUSCI_A2_BASE, "%s","AT+CNMI=1,2,0,0,0\r\n");
    for(ii=0;ii<50000;ii++){}

    printf(EUSCI_A2_BASE, "%s","AT+CMGF=1\r\n");
    for(ii=0;ii<50000;ii++){}

    //comando ("ATE0");
    UART_transmitData(EUSCI_A2_BASE, 'A');
    UART_transmitData(EUSCI_A2_BASE, 'T');
    UART_transmitData(EUSCI_A2_BASE, 'E');
    UART_transmitData(EUSCI_A2_BASE, '0');
    UART_transmitData(EUSCI_A2_BASE, '\r');
    UART_transmitData(EUSCI_A2_BASE, '\n');

    printf(EUSCI_A2_BASE, "%s","ATE0\r\n");
    for(ii=0;ii<50000;ii++){}


    printf(EUSCI_A2_BASE, "%s","AT+CNMI=1,2,0,0,0\r\n");
    for(ii=0;ii<50000;ii++){}


    printf(EUSCI_A2_BASE, "%s","AT+CMGS=");
    UART_transmitData(EUSCI_A2_BASE, 0x22);

    printf(EUSCI_A2_BASE, telefoneBackdoor);
    UART_transmitData(EUSCI_A2_BASE, 0x22);
    printf(EUSCI_A2_BASE, "\r\n");
    for(ii=0;ii<5000000;ii++){}


    printf(EUSCI_A2_BASE, mensagem);
    for(ii=0;ii<5000000;ii++){}




    UART_transmitData(EUSCI_A2_BASE, 0x1A);
    UART_transmitData(EUSCI_A2_BASE, 0x0D);
    UART_transmitData(EUSCI_A2_BASE, 0x0A);


    printf(EUSCI_A0_BASE, "FIM\r\n");


}


void mensagem_inicial(){
    long ii;

    while(!podeEnviar){//Se não pode enviar, espere!!!!
        if(flag_erro){//deu erro, esquece tudo e volta para o começo
            reinicializaSistema();
            return;
        }
    }
    podeEnviar=0;       //Estou enviando

    printf(EUSCI_A0_BASE, "%s","TESTE\r\n");

    printf(EUSCI_A2_BASE, "%s","AT\r\n");
    for(ii=0;ii<5000;ii++){}

    printf(EUSCI_A2_BASE, "%s","AT+CMGF=1\r\n");
    for(ii=0;ii<5000;ii++){}

    printf(EUSCI_A2_BASE, "%s","ATE0\r\n");
    for(ii=0;ii<50000;ii++){}


    printf(EUSCI_A2_BASE, "%s","AT+CNMI=1,2,0,0,0\r\n");
    for(ii=0;ii<50000;ii++){}

    printf(EUSCI_A2_BASE, "%s","AT+CMGF=1\r\n");
    for(ii=0;ii<50000;ii++){}

    //comando ("ATE0");
    UART_transmitData(EUSCI_A2_BASE, 'A');
    UART_transmitData(EUSCI_A2_BASE, 'T');
    UART_transmitData(EUSCI_A2_BASE, 'E');
    UART_transmitData(EUSCI_A2_BASE, '0');
    UART_transmitData(EUSCI_A2_BASE, '\r');
    UART_transmitData(EUSCI_A2_BASE, '\n');

    printf(EUSCI_A2_BASE, "%s","ATE0\r\n");
    for(ii=0;ii<50000;ii++){}


    printf(EUSCI_A2_BASE, "%s","AT+CNMI=1,2,0,0,0\r\n");
    for(ii=0;ii<50000;ii++){}


    printf(EUSCI_A2_BASE, "%s","AT+CMGS=");
    UART_transmitData(EUSCI_A2_BASE, 0x22);

    printf(EUSCI_A2_BASE, "+5541991832988");
    UART_transmitData(EUSCI_A2_BASE, 0x22);
    printf(EUSCI_A2_BASE, "\r\n");
    for(ii=0;ii<5000000;ii++){}


    printf(EUSCI_A2_BASE, "Senscar: DISPOSITIVO INICIALIZADO");
    for(ii=0;ii<5000000;ii++){}




    UART_transmitData(EUSCI_A2_BASE, 0x1A);
    UART_transmitData(EUSCI_A2_BASE, 0x0D);
    UART_transmitData(EUSCI_A2_BASE, 0x0A);


    printf(EUSCI_A0_BASE, "FIM\r\n");


}

void mensagem_SMS(char *mensagem, int tel){
    long ii;
    int i=0;
    int qtd_tel;
    if(tel==12){//telefone 1 e 2
        if(strcmp(telefone1, telefone2)==0){//Verifica se o telefone 1 e 2 são iguais, se sim, só manda uma mensagem.
            qtd_tel=1;
        }
        else{//dois telefones diferentes, enviar mensagem para cada um.
            qtd_tel=2;
        }
    }
    else{//telefone 1 ou telefone 2
        qtd_tel=1;
    }

    for(i=0;i<qtd_tel;i++){
        while(!podeEnviar){//Se não pode enviar, espere!!!!
            if(flag_erro){//deu erro, esquece tudo e volta para o começo
                reinicializaSistema();
                return;
            }
        }
            podeEnviar=0;       //Estou enviando

            for(ii=0;ii<500000;ii++){}//Um tempinho para terminar a mensagem anterior.

            printf(EUSCI_A0_BASE, "%s","\r\nTESTE\r\n");

            printf(EUSCI_A2_BASE, "%s","AT\r\n");
            for(ii=0;ii<5000;ii++){}

            printf(EUSCI_A2_BASE, "%s","AT+CMGF=1\r\n");
            for(ii=0;ii<5000;ii++){}

            printf(EUSCI_A2_BASE, "%s","ATE0\r\n");
            for(ii=0;ii<50000;ii++){}


            printf(EUSCI_A2_BASE, "%s","AT+CNMI=1,2,0,0,0\r\n");
            for(ii=0;ii<50000;ii++){}

            printf(EUSCI_A2_BASE, "%s","AT+CMGF=1\r\n");
            for(ii=0;ii<50000;ii++){}

            //comando ("ATE0");
            UART_transmitData(EUSCI_A2_BASE, 'A');
            UART_transmitData(EUSCI_A2_BASE, 'T');
            UART_transmitData(EUSCI_A2_BASE, 'E');
            UART_transmitData(EUSCI_A2_BASE, '0');
            UART_transmitData(EUSCI_A2_BASE, '\r');
            UART_transmitData(EUSCI_A2_BASE, '\n');

            printf(EUSCI_A2_BASE, "%s","ATE0\r\n");
            for(ii=0;ii<50000;ii++){}


            printf(EUSCI_A2_BASE, "%s","AT+CNMI=1,2,0,0,0\r\n");
            for(ii=0;ii<50000;ii++){}


            printf(EUSCI_A2_BASE, "%s","AT+CMGS=");
            UART_transmitData(EUSCI_A2_BASE, 0x22);

            if(i==0 && qtd_tel==2){//Dois números diferentes, primeiro número
                printf(EUSCI_A2_BASE, telefone1);
            }
            else if(i==1 && qtd_tel==2){//Dois números diferentes, segundo número
                printf(EUSCI_A2_BASE, telefone2);
            }
            else if(i==0 && qtd_tel==1){//Dois números iguais, apenas o primeiro número
                printf(EUSCI_A2_BASE, telefone1);
            }
            else if(i==0 && qtd_tel==1 && tel==1){//Apenas o telefone 1
                printf(EUSCI_A2_BASE, telefone1);
            }
            else if(i==0 && qtd_tel==1 && tel==2){//Apenas o telefone 2
                printf(EUSCI_A2_BASE, telefone2);
            }

            else{
                printf(EUSCI_A0_BASE, "\r\nTelefone invalido");
                printf(EUSCI_A2_BASE, telefone1);//No caso de telefone invalido, envia para o telefone 1
            }

            /*
            //printf(EUSCI_A2_BASE, "+5541991832988");
            if(tel==1){
                printf(EUSCI_A2_BASE, telefone1);
            }
            else if(tel==2){
                printf(EUSCI_A2_BASE, telefone2);
            }
            else{//default por hora
                printf(EUSCI_A2_BASE, "+5541991832988");

            }*/


            UART_transmitData(EUSCI_A2_BASE, 0x22);
            printf(EUSCI_A2_BASE, "\r\n");
            for(ii=0;ii<5000000;ii++){}


            printf(EUSCI_A2_BASE, mensagem);
            for(ii=0;ii<5000000;ii++){}

            UART_transmitData(EUSCI_A2_BASE, 0x1A);
            UART_transmitData(EUSCI_A2_BASE, 0x0D);
            UART_transmitData(EUSCI_A2_BASE, 0x0A);


            printf(EUSCI_A0_BASE, "FIM\r\n");

            for(ii=0;ii<500000;ii++){}//Tempo para transmitir para não embaralhar os pacotes



    }

}

void mensagem_SMS_Extra(char *mensagem){
    long ii;

    while(!podeEnviar){//Se não pode enviar, espere!!!!
        if(flag_erro){//deu erro, esquece tudo e volta para o começo
            reinicializaSistema();
            return;
        }
    }
    podeEnviar=0;       //Estou enviando

    printf(EUSCI_A0_BASE, "%s","TESTE\r\n");

    printf(EUSCI_A2_BASE, "%s","AT\r\n");
    for(ii=0;ii<5000;ii++){}

    printf(EUSCI_A2_BASE, "%s","AT+CMGF=1\r\n");
    for(ii=0;ii<5000;ii++){}

    printf(EUSCI_A2_BASE, "%s","ATE0\r\n");
    for(ii=0;ii<50000;ii++){}


    printf(EUSCI_A2_BASE, "%s","AT+CNMI=1,2,0,0,0\r\n");
    for(ii=0;ii<50000;ii++){}

    printf(EUSCI_A2_BASE, "%s","AT+CMGF=1\r\n");
    for(ii=0;ii<50000;ii++){}

    //comando ("ATE0");
    UART_transmitData(EUSCI_A2_BASE, 'A');
    UART_transmitData(EUSCI_A2_BASE, 'T');
    UART_transmitData(EUSCI_A2_BASE, 'E');
    UART_transmitData(EUSCI_A2_BASE, '0');
    UART_transmitData(EUSCI_A2_BASE, '\r');
    UART_transmitData(EUSCI_A2_BASE, '\n');

    printf(EUSCI_A2_BASE, "%s","ATE0\r\n");
    for(ii=0;ii<50000;ii++){}


    printf(EUSCI_A2_BASE, "%s","AT+CNMI=1,2,0,0,0\r\n");
    for(ii=0;ii<50000;ii++){}


    printf(EUSCI_A2_BASE, "%s","AT+CMGS=");
    UART_transmitData(EUSCI_A2_BASE, 0x22);

    printf(EUSCI_A2_BASE, telefone3);
    UART_transmitData(EUSCI_A2_BASE, 0x22);
    printf(EUSCI_A2_BASE, "\r\n");
    for(ii=0;ii<5000000;ii++){}


    printf(EUSCI_A2_BASE, mensagem);
    for(ii=0;ii<5000000;ii++){}


    UART_transmitData(EUSCI_A2_BASE, 0x1A);
    UART_transmitData(EUSCI_A2_BASE, 0x0D);
    UART_transmitData(EUSCI_A2_BASE, 0x0A);


    printf(EUSCI_A0_BASE, "FIM\r\n");


}



















