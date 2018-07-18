//* Includes
#include <p32xxxx.h>
#include "TCPIPConfig.h"			// Include all headers for any enabled TCPIP Stack functions
#include "TCPIP Stack/TCPIP.h"
#include "MainDemo.h"

//* Configuration Bits
#pragma config FNOSC    = PRIPLL    // Oscillator Selection
#pragma config POSCMOD  = HS        // Primary Oscillator
#pragma config FPLLIDIV = DIV_2     // PLL Input Divider (PIC32 Starter Kit: use divide by 2 only)
#pragma config FPLLMUL  = MUL_20    // PLL Multiplier
#pragma config FPLLODIV = DIV_1     // PLL Output Divider
#pragma config FPBDIV   = DIV_1     // Peripheral Clock divisor
#pragma config FWDTEN   = OFF       // Watchdog Timer
#pragma config WDTPS    = PS1       // Watchdog Timer Postscale
#pragma config FCKSM    = CSDCMD    // Clock Switching & Fail Safe Clock Monitor
#pragma config OSCIOFNC = OFF       // CLKO Enable
#pragma config IESO     = OFF       // Internal/External Switch-over
#pragma config FSOSCEN  = ON        // Secondary Oscillator Enable
#pragma config CP       = OFF       // Code Protect
#pragma config BWP      = OFF       // Boot Flash Write Protect
#pragma config PWP      = OFF       // Program Flash Write Protect
#pragma config DEBUG    = ON      	// Debugger Enabled
#pragma config FMIIEN   = OFF, FETHIO = ON // external PHY in RMII/STANDARD configuration
#pragma config ICESEL   = ICS_PGx1	// As required by Page 3 of 34: Cerebot_MX7cK_rm.pdf

//* Defines pinagem
#define SPI_CS 		PORTDbits.RD9
#define ADE_IRQ 	PORTDbits.RD12
#define PHY_RST     PORTAbits.RA6 	
#define LED1     	PORTGbits.RG12 	
#define LED2     	PORTGbits.RG13 	

//* Defines TCP/IP
#define SM_HOME 			0
#define	SM_LISTENING 		1
#define	SM_PROCESS_RECEIVE	2
#define	SM_PROCESS_TRANSMIT	3
#define SERVER_PORT			20000

//* Define tensao nominal da rede
#define VRMS   127000
#define max    5

//* Defines Calibragem
#define PLACA2
//PLACA1
#if defined(PLACA1)
#define NORM_VARMS          4.6869 		// Relação de normalização para VARMS
#define NORM_VBRMS          4.6506 		// Relação de normalização para VBRMS
#define NORM_VCRMS          4.6139 		// Relação de normalização para VCRMS

//PLACA2
#elif defined(PLACA2)       
#define NORM_VARMS          4.6278		// Relação de normalização para VARMS
#define NORM_VBRMS          4.7515		// Relação de normalização para VBRMS
#define NORM_VCRMS          4.6527		// Relação de normalização para VCRMS
#endif

//* Declare AppConfig structure and some other supporting stack variables
APP_CONFIG AppConfig;

//* Variáveis Globais
unsigned char SPI_DUMMY;
unsigned char SPI_DATA_RX[3];
unsigned char j=0;
unsigned char k=0;
unsigned char l=0;
unsigned char aux=0;
static signed int    VARMS;
static signed int    VBRMS;
static signed int    VCRMS;
static signed int    FQA; 
unsigned int count1=0; 
unsigned int count2=0;
unsigned int count3=0;
float temp=0;

//Cria STRUCT VTCD
typedef struct
{ 
  unsigned long date;
  unsigned long time;
  unsigned char type;
  unsigned char i;
} VTCD;

//Cria um objeto VTCD, te tamanho "max", para cada uma das fases
VTCD PhaseA[max];
VTCD PhaseB[max];
VTCD PhaseC[max];

//* Funções
static void InitAppConfig(void);
static void InitializeBoard(void);
void SPI_READ(unsigned char SPI_RDQUANT, unsigned char SPI_ADDRESS);
void SPI_WRITE(unsigned char SPI_WRQUANT, unsigned char SPI_ADDRESS, unsigned char SPI_DATA1, unsigned char SPI_DATA2, unsigned char SPI_DATA3);

//********************************************************************
//******************************** MAIN ******************************
//********************************************************************
int main(void){

//****** Variáveis locais
  unsigned char FRAME_RXBUF[30];    //****** 
  unsigned char FRAME_TXBUF[120];
  unsigned char FRAME_TXMESS;
  DWORD   	    FRAME_RXQTY;          //****
  unsigned int  MAIN_CONT;
  static TCP_SOCKET	MySocket;
  unsigned char TCPServerState=SM_HOME;  
                
  //****** Configuração dos pinos de I/O
  AD1PCFG=0xffffffff; 	//Todos os pinos digitais
  TRISCbits.TRISC4=1;  	//SPI_SI
  TRISDbits.TRISD0=0;  	//SPI_SO
  TRISDbits.TRISD12=1;  //ADE_IRQ
  TRISDbits.TRISD9=0;  	//SPI_CS
  TRISDbits.TRISD10=0; 	//SPI_SCLK
  TRISAbits.TRISA6=0;	//PHY_RST
  TRISGbits.TRISG12=0;  //LED1
  TRISGbits.TRISG13=0;  //LED2  	

  //****** Polariza alguns pinos de I/O
  LED1=0;
  LED2=0;

  //****** Configuração da comunicação SPI1
  SPI1CON=0; 						//Stops and resets the SPI1. 
  SPI_DUMMY=SPI1BUF;	    		//clears the receive buffer
  SPI1BRG=31;						//use FPB/4 clock frequency
  SPI1STATbits.SPIROV=0;			//clear the Overflow
  SPI1CON=0x00008260;				//SPI ON, 8 bits transfer, SMP=1, Master mode, CKP=1,CKE=0 (com inversão lógica)

  //****** Liga o real time
  SYSKEY = 0xaa996655;    			// write first unlock key to SYSKEY
  SYSKEY = 0x556699aa;    			// write second unlock key to SYSKEY
  RTCCONbits.RTCWREN=1;   			// Destrava o ajuste
  RTCCONbits.ON=1;        			// Liga o RTC
  while(RTCCONbits.RTCCLKON==0){}   // Espera o RTC ser ligado
  RTCCONbits.RTCWREN=0;   			// Trava o ajuste

  //***** Configura o ADE
  SPI_WRITE(1,0x13,0x40,0x00,0x00); //Reseta o software do chip 
  for (MAIN_CONT=0; MAIN_CONT<=2000; MAIN_CONT++) {} //Intervalo necessario apos o reset
  SPI_WRITE(1,0x13,0x00,0x00,0x00); //Registrador OPMODE em operação normal
  SPI_WRITE(1,0x16,0x00,0x00,0x00); //Registrador COMPMODE com NOLOAD=0, SAVAR=0, ABS=0, TERMSEL=000 e CONSEL=00  
  SPI_WRITE(1,0x23,0x00,0x00,0x00); //Registrador GAIN com integrador desativado, PGA1=1, PGA2=1
  SPI_WRITE(1,0x1D,0x06,0x00,0x00); //Registrador SAGCYC ajustado para 6 meio ciclos
  SPI_WRITE(1,0x1E,0x3A,0x00,0x00); //Registrador SAGLVL ajustado para 
  for (MAIN_CONT=0; MAIN_CONT<=2000; MAIN_CONT++) {} 
  
  #if defined(PLACA2)
  SPI_WRITE(2,0x33,0x0D,0xEA,0x00); //Registrador VARMS OFFSET
  SPI_WRITE(2,0x34,0x0E,0x1E,0x00); //Registrador VBRMS OFFSET
  SPI_WRITE(2,0x35,0x0E,0x04,0x00); //Registrador VCRMS OFFSET
  #elif defined(PLACA2)
  SPI_WRITE(2,0x33,0x0D,0xBC,0x00); //Registrador VARMS OFFSET
  SPI_WRITE(2,0x34,0x0E,0x1B,0x00); //Registrador VBRMS OFFSET
  SPI_WRITE(2,0x35,0x0D,0xF0,0x00); //Registrador VCRMS OFFSET
  #endif
  SPI_WRITE(1,0x14,0x00,0x00,0x00); //Freq para o canal A
  SPI_WRITE(3,0x18,0x01,0x00,0x08); //Registrador INTERRUPT MASK com WFSM habilitado 

  //***** Configurações do Timer2 T2 para 100ms
  T2CON=0x8070;                     //Prescaler de 1/256 = 80/256 = 0,3125MHz (3,2us)
  PR2=31250;                        //Carga para comparação = 31250 x 3,2us = 100ms
  IPC2bits.T2IP=6;					//Interrupt priority 6
  IFS0bits.T2IF=0;                  //Interrupt flag
  IEC0bits.T2IE=1;					//Habilita interrupção para o Timer2

  //***** Configurações do Timer3 T3 para 16ms
  T3CON=0x8070;                     //Prescaler de 1/256 = 80/256 = 0,3125MHz (3,2us)
  PR3=5000;                         //Carga para comparação = 5000 x 3,2us = 16ms
  IPC3bits.T3IP=5;					//Interrupt priority 5
  IFS0bits.T3IF=0;                  //Interrupt flag
  IEC0bits.T3IE=0;					//Habilita interrupção para o Timer3 quando ocorrer SAG

  //***** Configurações do Timer4 T4 para 10ms
  T4CON=0x8070;                     //Prescaler de 1/256 = 80/256 = 0,3125MHz (3,2us)
  PR4=3125;                         //Carga para comparação = 3125 x 3,2us = 10ms
  IPC4bits.T4IP=4;					//Interrupt priority 5
  IFS0bits.T4IF=0;                  //Interrupt flag
  IEC0bits.T4IE=0;					//Habilita interrupção para o Timer3 quando ocorrer SAG

  //***** Configurações do Timer5 T5 para 15ms
  T5CON=0x8070;                     //Prescaler de 1/256 = 80/256 = 0,3125MHz (3,2us)
  PR5=4688;                         //Carga para comparação = 3125 x 3,2us = 15ms
  IPC5bits.T5IP=3;					//Interrupt priority 3
  IFS0bits.T5IF=0;                  //Interrupt flag
  IEC0bits.T5IE=0;					//Habilita interrupção para o Timer3 quando ocorrer SAG

  //****** Reseta o PHY Ehernet
  PHY_RST=0;   
  for (MAIN_CONT=0; MAIN_CONT<=100; MAIN_CONT++) {} 
  PHY_RST=1;   
  for (MAIN_CONT=0; MAIN_CONT<=100; MAIN_CONT++) {} 

  //****** Initialize stack-related hardware components 
  TickInit();

  //****** Initialize Stack and application related NV variables into AppConfig.
  InitAppConfig();

  //****** Initialize core stack layers (MAC, ARP, TCP, UDP) 
  StackInit();

  //****** Initialize application specific hardware
  InitializeBoard();
  
  //****** POOLING GERAL
  while(1){    
   
    //****** Máquina de estados TCP Server
    StackTask();

   	switch(TCPServerState){

      case SM_HOME:
        MySocket = TCPOpen(0, TCP_OPEN_SERVER, SERVER_PORT, TCP_PURPOSE_GENERIC_TCP_SERVER); // Tenta obter um SOCKET para o Servidor TCP
	    if(MySocket==INVALID_SOCKET) break; 	   		// Não -> Sai
	   	TCPServerState = SM_LISTENING;					// Sim -> verifica se alguém conecta
	  break;
	
	  case SM_LISTENING:
	    if(!TCPIsConnected(MySocket)) break;           	// Se não obteve a conexão sai
		TCPServerState=SM_PROCESS_RECEIVE;  	        // Se obteve a conexão, realiza os processamentos 
      break;
	
  	  case SM_PROCESS_RECEIVE:

        //***** Se estiver desconectado, executa abaixo
        if(!TCPIsConnected(MySocket)){
          TCPClose(MySocket);
          TCPServerState=SM_HOME;   
          break;
        }    

        //***** Verifica se chegaram dados 
        FRAME_RXQTY=TCPIsGetReady(MySocket);
        if(FRAME_RXQTY!=0){
          TCPGetArray(MySocket,FRAME_RXBUF,FRAME_RXQTY);       	         	//Armazena os dados recebidos 
          
          //Verifica se recebeu a mensagem CDT (Configure date_time) 
          if(FRAME_RXBUF[0]=='C' && FRAME_RXBUF[1]=='D' && FRAME_RXBUF[2]=='T' && FRAME_RXBUF[3] == 0x20){
            SYSKEY = 0xaa996655;    			// write first unlock key to SYSKEY
            SYSKEY = 0x556699aa;    			// write second unlock key to SYSKEY
            RTCCONbits.RTCWREN=1;   			// Destrava o ajuste
            while((RTCCON&0x4)!=0);             // wait for not RTCSYNC
            
            //Ajusta os registradores de data e hora de acordo com o que foi digitado
            RTCDATEbits.YEAR10 = FRAME_RXBUF[10] & 0x0F;
            RTCDATEbits.YEAR01 = FRAME_RXBUF[11] & 0x0F;
            RTCDATEbits.MONTH10 = FRAME_RXBUF[7] & 0x0F;
            RTCDATEbits.MONTH01 = FRAME_RXBUF[8] & 0x0F;
            RTCDATEbits.DAY10 = FRAME_RXBUF[4] & 0x0F;
            RTCDATEbits.DAY01 = FRAME_RXBUF[5] & 0x0F;

            RTCTIMEbits.HR10 = FRAME_RXBUF[13] & 0x0F;
            RTCTIMEbits.HR01 = FRAME_RXBUF[14] & 0x0F;
            RTCTIMEbits.MIN10 = FRAME_RXBUF[16] & 0x0F;
            RTCTIMEbits.MIN01 = FRAME_RXBUF[17] & 0x0F;
            RTCTIMEbits.SEC10 = FRAME_RXBUF[19] & 0x0F;
            RTCTIMEbits.SEC01 = FRAME_RXBUF[20] & 0x0F;

            RTCCONbits.RTCWREN=0;   			// Trava o ajuste            
            FRAME_TXMESS=1;  
            TCPServerState=SM_PROCESS_TRANSMIT;
          }

          //Verifica se recebeu a mensagem VDT (Verify date_time) 
          else if(FRAME_RXBUF[0]=='V' && FRAME_RXBUF[1]=='D' && FRAME_RXBUF[2]=='T') {
            FRAME_TXMESS=2;  
            TCPServerState=SM_PROCESS_TRANSMIT;  
          }

          //Verifica se ocorreu VTCD - Fase A
          else if(FRAME_RXBUF[0]=='F' && FRAME_RXBUF[1]=='A'){
            FRAME_TXMESS=3; 
            TCPServerState=SM_PROCESS_TRANSMIT;
          }

          //Verifica se ocorreu VTCD - Fase B
          else if(FRAME_RXBUF[0]=='F' && FRAME_RXBUF[1]=='B'){
            FRAME_TXMESS=4; 
            TCPServerState=SM_PROCESS_TRANSMIT;
          }

          //Verifica se ocorreu VTCD - Fase C
          else if(FRAME_RXBUF[0]=='F' && FRAME_RXBUF[1]=='C'){
            FRAME_TXMESS=5; 
            TCPServerState=SM_PROCESS_TRANSMIT;
          } 
       }
  
      break;    

	  case SM_PROCESS_TRANSMIT: 
               
        //Se não puder carregar o buffer TX, sai
        if(TCPIsPutReady(MySocket)<20u) break;
        
        // **** VTCD - Fase A ****
        else if(FRAME_TXMESS==3){

        //if(aux==max) aux=0;
        if(j!=0)aux = j-1;
        else aux = j;
      
        //*****SAG
        if(PhaseA[aux].i==2){  
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]=(((PhaseA[aux].date>>12)&0x0f)+0x30);
		FRAME_TXBUF[2]=(((PhaseA[aux].date>>8)&0x0f)+0x30);
		FRAME_TXBUF[3]='/';
		FRAME_TXBUF[4]=(((PhaseA[aux].date>>20)&0x0f)+0x30);
		FRAME_TXBUF[5]=(((PhaseA[aux].date>>16)&0x0f)+0x30); 
		FRAME_TXBUF[6]=' ';
		FRAME_TXBUF[7]=(((PhaseA[aux].time>>28)&0x0f)+0x30);
		FRAME_TXBUF[8]=(((PhaseA[aux].time>>24)&0x0f)+0x30);
		FRAME_TXBUF[9]=':';
		FRAME_TXBUF[10]=(((PhaseA[aux].time>>20)&0x0f)+0x30);
		FRAME_TXBUF[11]=(((PhaseA[aux].time>>16)&0x0f)+0x30);
        FRAME_TXBUF[12]=' '; 
        FRAME_TXBUF[13]='A';
		FRAME_TXBUF[14]=PhaseA[aux].type;
        FRAME_TXBUF[15]='T';
        FRAME_TXBUF[16]=0x0D;
        FRAME_TXBUF[17]=0x0A;
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,18);
        }
        
        //****SWELL
        else if(PhaseA[aux].i==1){
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]=(((PhaseA[aux].date>>12)&0x0f)+0x30);
		FRAME_TXBUF[2]=(((PhaseA[aux].date>>8)&0x0f)+0x30);
		FRAME_TXBUF[3]='/';
		FRAME_TXBUF[4]=(((PhaseA[aux].date>>20)&0x0f)+0x30);
		FRAME_TXBUF[5]=(((PhaseA[aux].date>>16)&0x0f)+0x30); 
		FRAME_TXBUF[6]=' ';
		FRAME_TXBUF[7]=(((PhaseA[aux].time>>28)&0x0f)+0x30);
		FRAME_TXBUF[8]=(((PhaseA[aux].time>>24)&0x0f)+0x30);
		FRAME_TXBUF[9]=':';
		FRAME_TXBUF[10]=(((PhaseA[aux].time>>20)&0x0f)+0x30);
		FRAME_TXBUF[11]=(((PhaseA[aux].time>>16)&0x0f)+0x30);
        FRAME_TXBUF[12]=' '; 
        FRAME_TXBUF[13]='E';
		FRAME_TXBUF[14]=PhaseA[aux].type;  //De acordo com a duracao. EIT, EMT ou ETT
        FRAME_TXBUF[15]='T';
        FRAME_TXBUF[16]=0x0D;
        FRAME_TXBUF[17]=0x0A;
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,18);
        }
        
        //****Operacao Normal
        else{
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]='N';
        FRAME_TXBUF[2]='O';    
        FRAME_TXBUF[3]=0x0D;
        FRAME_TXBUF[4]=0x0A;    
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,5);
        }

        TCPFlush(MySocket);
        TCPServerState=SM_PROCESS_RECEIVE; 

        }//End of VTCD - Fase A ****
        
        // **** VTCD - Fase B ****
        else if(FRAME_TXMESS==4){

        //if(aux==max) aux=0;
        if(k!=0)aux = k-1;
        else aux = k;
      
        //*****SAG
        if(PhaseB[aux].i==2){  
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]=(((PhaseB[aux].date>>12)&0x0f)+0x30);
		FRAME_TXBUF[2]=(((PhaseB[aux].date>>8)&0x0f)+0x30);
		FRAME_TXBUF[3]='/';
		FRAME_TXBUF[4]=(((PhaseB[aux].date>>20)&0x0f)+0x30);
		FRAME_TXBUF[5]=(((PhaseB[aux].date>>16)&0x0f)+0x30); 
		FRAME_TXBUF[6]=' ';
		FRAME_TXBUF[7]=(((PhaseB[aux].time>>28)&0x0f)+0x30);
		FRAME_TXBUF[8]=(((PhaseB[aux].time>>24)&0x0f)+0x30);
		FRAME_TXBUF[9]=':';
		FRAME_TXBUF[10]=(((PhaseB[aux].time>>20)&0x0f)+0x30);
		FRAME_TXBUF[11]=(((PhaseB[aux].time>>16)&0x0f)+0x30);
        FRAME_TXBUF[12]=' '; 
        FRAME_TXBUF[13]='A';
		FRAME_TXBUF[14]=PhaseB[aux].type;
        FRAME_TXBUF[15]='T';
        FRAME_TXBUF[16]=0x0D;
        FRAME_TXBUF[17]=0x0A;
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,18);
        }
        
        //****SWELL
        else if(PhaseB[aux].i==1){
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]=(((PhaseB[aux].date>>12)&0x0f)+0x30);
		FRAME_TXBUF[2]=(((PhaseB[aux].date>>8)&0x0f)+0x30);
		FRAME_TXBUF[3]='/';
		FRAME_TXBUF[4]=(((PhaseB[aux].date>>20)&0x0f)+0x30);
		FRAME_TXBUF[5]=(((PhaseB[aux].date>>16)&0x0f)+0x30); 
		FRAME_TXBUF[6]=' ';
		FRAME_TXBUF[7]=(((PhaseB[aux].time>>28)&0x0f)+0x30);
		FRAME_TXBUF[8]=(((PhaseB[aux].time>>24)&0x0f)+0x30);
		FRAME_TXBUF[9]=':';
		FRAME_TXBUF[10]=(((PhaseB[aux].time>>20)&0x0f)+0x30);
		FRAME_TXBUF[11]=(((PhaseB[aux].time>>16)&0x0f)+0x30);
        FRAME_TXBUF[12]=' '; 
        FRAME_TXBUF[13]='E';
		FRAME_TXBUF[14]=PhaseB[aux].type;  //De acordo com a duracao. EIT, EMT ou ETT
        FRAME_TXBUF[15]='T';
        FRAME_TXBUF[16]=0x0D;
        FRAME_TXBUF[17]=0x0A;
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,18);
        }
        
        //****Operacao Normal
        else{
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]='N';
        FRAME_TXBUF[2]='O';    
        FRAME_TXBUF[3]=0x0D;
        FRAME_TXBUF[4]=0x0A;    
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,5);
        }

        TCPFlush(MySocket);
        TCPServerState=SM_PROCESS_RECEIVE; 

        }//End of VTCD - Fase B ****

// **** VTCD - Fase C ****
        else if(FRAME_TXMESS==5){

        //if(aux==max) aux=0;
        if(l!=0)aux = l-1;
        else aux = l;
      
        //*****SAG
        if(PhaseC[aux].i==2){  
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]=(((PhaseC[aux].date>>12)&0x0f)+0x30);
		FRAME_TXBUF[2]=(((PhaseC[aux].date>>8)&0x0f)+0x30);
		FRAME_TXBUF[3]='/';
		FRAME_TXBUF[4]=(((PhaseC[aux].date>>20)&0x0f)+0x30);
		FRAME_TXBUF[5]=(((PhaseC[aux].date>>16)&0x0f)+0x30); 
		FRAME_TXBUF[6]=' ';
		FRAME_TXBUF[7]=(((PhaseC[aux].time>>28)&0x0f)+0x30);
		FRAME_TXBUF[8]=(((PhaseC[aux].time>>24)&0x0f)+0x30);
		FRAME_TXBUF[9]=':';
		FRAME_TXBUF[10]=(((PhaseC[aux].time>>20)&0x0f)+0x30);
		FRAME_TXBUF[11]=(((PhaseC[aux].time>>16)&0x0f)+0x30);
        FRAME_TXBUF[12]=' '; 
        FRAME_TXBUF[13]='A';
		FRAME_TXBUF[14]=PhaseC[aux].type;
        FRAME_TXBUF[15]='T';
        FRAME_TXBUF[16]=0x0D;
        FRAME_TXBUF[17]=0x0A;
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,18);
        }
        
        //****SWELL
        else if(PhaseC[aux].i==1){
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]=(((PhaseC[aux].date>>12)&0x0f)+0x30);
		FRAME_TXBUF[2]=(((PhaseC[aux].date>>8)&0x0f)+0x30);
		FRAME_TXBUF[3]='/';
		FRAME_TXBUF[4]=(((PhaseC[aux].date>>20)&0x0f)+0x30);
		FRAME_TXBUF[5]=(((PhaseC[aux].date>>16)&0x0f)+0x30); 
		FRAME_TXBUF[6]=' ';
		FRAME_TXBUF[7]=(((PhaseC[aux].time>>28)&0x0f)+0x30);
		FRAME_TXBUF[8]=(((PhaseC[aux].time>>24)&0x0f)+0x30);
		FRAME_TXBUF[9]=':';
		FRAME_TXBUF[10]=(((PhaseC[aux].time>>20)&0x0f)+0x30);
		FRAME_TXBUF[11]=(((PhaseC[aux].time>>16)&0x0f)+0x30);
        FRAME_TXBUF[12]=' '; 
        FRAME_TXBUF[13]='E';
		FRAME_TXBUF[14]=PhaseC[aux].type;  //De acordo com a duracao. EIT, EMT ou ETT
        FRAME_TXBUF[15]='T';
        FRAME_TXBUF[16]=0x0D;
        FRAME_TXBUF[17]=0x0A;
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,18);
        }
        
        //****Operacao Normal
        else{
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]='N';
        FRAME_TXBUF[2]='O';    
        FRAME_TXBUF[3]=0x0D;
        FRAME_TXBUF[4]=0x0A;    
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,5);
        }

        TCPFlush(MySocket);
        TCPServerState=SM_PROCESS_RECEIVE; 

        }//End of VTCD - Fase C****

        // **** VDT
        else if(FRAME_TXMESS==2){     
        //RTC_DATE=RTCDATE;
        //RTC_TIME=RTCTIME;
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]=(((RTCDATE>>12)&0x0f)+0x30);
		FRAME_TXBUF[2]=(((RTCDATE>>8)&0x0f)+0x30);
		FRAME_TXBUF[3]='/';
		FRAME_TXBUF[4]=(((RTCDATE>>20)&0x0f)+0x30);
		FRAME_TXBUF[5]=(((RTCDATE>>16)&0x0f)+0x30);
	    FRAME_TXBUF[6]='/';
		FRAME_TXBUF[7]=(((RTCDATE>>28)&0x0f)+0x30);
		FRAME_TXBUF[8]=(((RTCDATE>>24)&0x0f)+0x30); 
		FRAME_TXBUF[9]=' ';
		FRAME_TXBUF[10]=(((RTCTIME>>28)&0x0f)+0x30);
		FRAME_TXBUF[11]=(((RTCTIME>>24)&0x0f)+0x30);
		FRAME_TXBUF[12]=':';
		FRAME_TXBUF[13]=(((RTCTIME>>20)&0x0f)+0x30);
		FRAME_TXBUF[14]=(((RTCTIME>>16)&0x0f)+0x30);
		FRAME_TXBUF[15]=':';
		FRAME_TXBUF[16]=(((RTCTIME>>12)&0x0f)+0x30);
		FRAME_TXBUF[17]=(((RTCTIME>>8)&0x0f)+0x30);
        FRAME_TXBUF[18]=0x0D;
        FRAME_TXBUF[19]=0x0A;   
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,20);        
        }
 
        // ***** CDT 
        else if(FRAME_TXMESS==1){ 
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]='O';
        FRAME_TXBUF[2]='K';    
        FRAME_TXBUF[3]=0x0D;
        FRAME_TXBUF[4]=0x0A;    
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,5);       
        }     

        //Se for o caso realiza a transmissão
        TCPFlush(MySocket);
        TCPServerState=SM_PROCESS_RECEIVE;            
      break; 
    
    }//switch   
  }//loop geral
}//função main

// 

//********************************************************************
//***************************** SPI WRITE ****************************
//********************************************************************
void SPI_WRITE(unsigned char SPI_WRQUANT, unsigned char SPI_ADDRESS, unsigned char SPI_DATA1, unsigned char SPI_DATA2, unsigned char SPI_DATA3){

  unsigned char SPI_CONT;

  for (SPI_CONT=0;SPI_CONT<=5;SPI_CONT++) {} 	    //Aguarda tempo para mudar CS
  SPI_CS=1;   										//Coloca CS em nível 1 (invertido)
 
  while(SPI1STATbits.SPITBE==0) {} 					//Verifica se pode transmitir
  SPI1BUF=((SPI_ADDRESS|0x80)^0xFF);  				//Envia o SPI ADDRESS com lógica invertida
  while(SPI1STATbits.SPIRBF==0) {}
  SPI_DUMMY=SPI1BUF;                                //Recebe o dado DUMMY 
  
  while(SPI1STATbits.SPITBE==0) {} 					//Verifica se pode transmitir
  SPI1BUF=(SPI_DATA1^0xFF);  						//Envia o SPI_DATA1 com lógica invertida 
  while(SPI1STATbits.SPIRBF==0) {}
  SPI_DUMMY=SPI1BUF;                                //Recebe o dado DUMMY
 
  if(SPI_WRQUANT>1){
    while(SPI1STATbits.SPITBE==0) {} 				//Verifica se pode transmitir
    SPI1BUF=(SPI_DATA2^0xFF);  						//Envia o SPI_DATA2 com lógica invertida 
    while(SPI1STATbits.SPIRBF==0) {}
    SPI_DUMMY=SPI1BUF;                              //Recebe o dado DUMMY
  }
 
  if(SPI_WRQUANT>2){
    while(SPI1STATbits.SPITBE==0) {} 				//Verifica se pode transmitir
    SPI1BUF=(SPI_DATA3^0xFF);  						//Envia o SPI_DATA3 com lógica invertida 
    while(SPI1STATbits.SPIRBF==0) {}
    SPI_DUMMY=SPI1BUF;                              //Recebe o dado DUMMY 
  }
  
  SPI_CS=0;   										//Coloca CS em nível 0 (invertido)
}

//********************************************************************
//***************************** SPI READ *****************************
//********************************************************************
void SPI_READ(unsigned char SPI_RDQUANT, unsigned char SPI_ADDRESS){
 
  unsigned char SPI_CONT;

  for (SPI_CONT=0;SPI_CONT<=2;SPI_CONT++) {}  		//Aguarda tempo para mudar CS
  SPI_CS=1;   										//Coloca CS em nível 1 (invertido)
  
  while(SPI1STATbits.SPITBE==0) {} 				    //Verifica se pode transmitir
  SPI1BUF=((SPI_ADDRESS&0x7F)^0xFF);  				//Envia o SPI ADDRESS com lógica invertida
  while(SPI1STATbits.SPIRBF==0) {}
  SPI_DUMMY=SPI1BUF;                                //Recebe o dado DUMMY
   
  while(SPI1STATbits.SPITBE==0) {} 				    //Verifica se pode transmitir
  SPI1BUF=0xFF;										//Envia o byte dummy
  while(SPI1STATbits.SPIRBF==0) {}
  SPI_DATA_RX[0]=(SPI1BUF^0XFFFFFFFF);              //Recebe dado válido e inverte

  if(SPI_RDQUANT>1){
    while(SPI1STATbits.SPITBE==0) {} 			    //Verifica se pode transmitir
  	SPI1BUF=0xFF;									//Envia o byte dummy
  	while(SPI1STATbits.SPIRBF==0) {}
  	SPI_DATA_RX[1]=(SPI1BUF^0XFFFFFFFF);            //Recebe dado válido e inverte
  }
 
  if(SPI_RDQUANT>2){
    while(SPI1STATbits.SPITBE==0) {} 			    //Verifica se pode transmitir
    SPI1BUF=0xFF;									//Envia o byte dummy
    while(SPI1STATbits.SPIRBF==0) {}
    SPI_DATA_RX[2]=(SPI1BUF^0XFFFFFFFF);            //Recebe dado válido e inverte
  }
  
  SPI_CS=0;   										//Coloca CS em nível 0 (invertido)
} 

/*********************************************************************
************ Function: static void InitializeBoard(void) *************
*********************************************************************/
static void InitializeBoard(void)
{
		INTEnableSystemMultiVectoredInt();     		// Enable multi-vectored interrupts
		SYSTEMConfigPerformance(GetSystemClock()); // Enable optimal performance
		mOSCSetPBDIV(OSC_PB_DIV_1);				// Use 1:1 CPU Core:Peripheral clocks
}

/*********************************************************************
****************** Function: void InitAppConfig(void)***************** 
*********************************************************************/
static ROM BYTE SerializedMACAddress[6] = {MY_DEFAULT_MAC_BYTE1, MY_DEFAULT_MAC_BYTE2, MY_DEFAULT_MAC_BYTE3, MY_DEFAULT_MAC_BYTE4, MY_DEFAULT_MAC_BYTE5, MY_DEFAULT_MAC_BYTE6};
//#pragma romdata

static void InitAppConfig(void)
{

	while(1)
	{
		// Start out zeroing all AppConfig bytes to ensure all fields are 
		// deterministic for checksum generation
		memset((void*)&AppConfig, 0x00, sizeof(AppConfig));
		
		AppConfig.Flags.bIsDHCPEnabled = TRUE;
		AppConfig.Flags.bInConfigMode = TRUE;
		memcpypgm2ram((void*)&AppConfig.MyMACAddr, (ROM void*)SerializedMACAddress, sizeof(AppConfig.MyMACAddr));
//		{
//			_prog_addressT MACAddressAddress;
//			MACAddressAddress.next = 0x157F8;
//			_memcpy_p2d24((char*)&AppConfig.MyMACAddr, MACAddressAddress, sizeof(AppConfig.MyMACAddr));
//		}
		AppConfig.MyIPAddr.Val = MY_DEFAULT_IP_ADDR_BYTE1 | MY_DEFAULT_IP_ADDR_BYTE2<<8ul | MY_DEFAULT_IP_ADDR_BYTE3<<16ul | MY_DEFAULT_IP_ADDR_BYTE4<<24ul;
		AppConfig.DefaultIPAddr.Val = AppConfig.MyIPAddr.Val;
		AppConfig.MyMask.Val = MY_DEFAULT_MASK_BYTE1 | MY_DEFAULT_MASK_BYTE2<<8ul | MY_DEFAULT_MASK_BYTE3<<16ul | MY_DEFAULT_MASK_BYTE4<<24ul;
		AppConfig.DefaultMask.Val = AppConfig.MyMask.Val;
		AppConfig.MyGateway.Val = MY_DEFAULT_GATE_BYTE1 | MY_DEFAULT_GATE_BYTE2<<8ul | MY_DEFAULT_GATE_BYTE3<<16ul | MY_DEFAULT_GATE_BYTE4<<24ul;
		AppConfig.PrimaryDNSServer.Val = MY_DEFAULT_PRIMARY_DNS_BYTE1 | MY_DEFAULT_PRIMARY_DNS_BYTE2<<8ul  | MY_DEFAULT_PRIMARY_DNS_BYTE3<<16ul  | MY_DEFAULT_PRIMARY_DNS_BYTE4<<24ul;
		AppConfig.SecondaryDNSServer.Val = MY_DEFAULT_SECONDARY_DNS_BYTE1 | MY_DEFAULT_SECONDARY_DNS_BYTE2<<8ul  | MY_DEFAULT_SECONDARY_DNS_BYTE3<<16ul  | MY_DEFAULT_SECONDARY_DNS_BYTE4<<24ul;
	
		break;
	}
}

void getAVRMS(void){
  
  //* Aguarda a fase A passar pelo zero
  SPI_READ(3,0x1A); 	
  SPI_WRITE(3,0x18,0x00,0x02,0x00); //Ativa a interrupcao para quando ocorrer Zero crossing in the voltage channel of Phase A 
  while(ADE_IRQ==0){}     		    //Se passou reseta o indicador de interrupção(registrador RSTATUS) e os registradores de potência  
  SPI_READ(3,0x1A);                 //Eh enviado pelo registrador RSTATUS (0x1A) a instrucao que reseta o indicador de amostra recebida (retorna IRQ para 0) 
   
  //Calcula a tensão RMS para a fase A
  SPI_READ(3,0x0D); //AVRMS  
  VARMS=((65536*SPI_DATA_RX[0])+(256*SPI_DATA_RX[1])+SPI_DATA_RX[2])/NORM_VARMS;
}

void getBVRMS(void){

  //* Aguarda a fase B passar pelo zero
  SPI_WRITE(3,0x18,0x00,0x04,0x00); 
  while(ADE_IRQ==0){}     		    //Se passou reseta o indicador de interrupção(registrador RSTATUS) e os registradores de potência  
  SPI_READ(3,0x1A); 					

  //Calcula a tensão RMS para a fase B
  SPI_READ(3,0x0E); //BVRMS  
  VBRMS=((65536*SPI_DATA_RX[0])+(256*SPI_DATA_RX[1])+SPI_DATA_RX[2])/NORM_VBRMS;  
}

void getCVRMS(void){

  //* Aguarda a fase C passar pelo zero
  SPI_WRITE(3,0x18,0x00,0x08,0x00); 
  while(ADE_IRQ==0){}     		    //Se passou reseta o indicador de interrupção(registrador RSTATUS) e os registradores de potência  
  SPI_READ(3,0x1A); 					
  
  //Calcula a tensão RMS para a fase C
  SPI_READ(3,0x0F); //CVRMS
  VCRMS=((65536*SPI_DATA_RX[0])+(256*SPI_DATA_RX[1])+SPI_DATA_RX[2])/NORM_VCRMS;
}

void getFREQ(void){

  SPI_READ(2,0x10); //Le o registrador FREQ
  FQA=((256*SPI_DATA_RX[0])+SPI_DATA_RX[1])/0.016; //Aquisita a freq para a fase A 

}

/*********************************************************************
**************************** Timer 2 Interrupt ***********************
*********************************************************************/
void __ISR(_TIMER_2_VECTOR,ipl6) _T2Interrupt(void){ 

  //Limpa o flag da interrupção (devia estar no início, mas pode atrapalhar o cálculo fasorial)
  IFS0bits.T2IF=0;

  //Realiza a aquisição das frequencias

  //Se já contou próximo de 100ms (> 4 periodos do sinal(66ms)), executa abaixo
  getFREQ();

  //Realiza a aquisição das tensões RMS
  getAVRMS();
  
  if(VARMS < 0.9*VRMS){
     IEC0bits.T3IE=1; //Inicia a interrupção do Timer3 --> VTCD detectada
     PhaseA[j].i=2; //2 = SAG
  }
  if(VARMS > 1.1*VRMS){
     IEC0bits.T3IE=1; //Inicia a interrupção do Timer3 --> VTCD detectada
     PhaseA[j].i=1; //1 = SWELL 
  }
  
  getBVRMS();
  
  if(VBRMS < 0.9*VRMS){
     IEC0bits.T4IE=1; //Inicia a interrupção do Timer4 --> VTCD detectada
     PhaseB[k].i=2; //2 = SAG
  }
  if(VBRMS > 1.1*VRMS){
     IEC0bits.T4IE=1; //Inicia a interrupção do Timer4 --> VTCD detectada
     PhaseB[k].i=1; //1 = SWELL 
  }
  
  getCVRMS();
  
  if(VCRMS < 0.9*VRMS){
     IEC0bits.T5IE=1; //Inicia a interrupção do Timer5 --> VTCD detectada
     PhaseC[l].i=2; //2 = SAG
  }
  if(VCRMS > 1.1*VRMS){
     IEC0bits.T5IE=1; //Inicia a interrupção do Timer5 --> VTCD detectada
     PhaseC[l].i=1; //1 = SWELL 
  }

}

/*********************************************************************
**************************** Timer 3 Interrupt ***********************
******************************** PHASE A *****************************
*********************************************************************/
void __ISR(_TIMER_3_VECTOR,ipl5) _T3Interrupt(void){ 

  //Limpa o flag da interrupção 
  IFS0bits.T3IF=0;
  
  count1++;

  if(VARMS >= 0.9*VRMS && VARMS <= 1.1*VRMS){

     IEC0bits.T3IE=0;     //Desabilita a interrupcao do Timer3
     temp = 0.016*count1;  //Calcula a duração da VTCD

     if(temp < 0.5){PhaseA[j].type=0x49;}                   //Instantaneo
     else if(temp >= 0.5 && temp < 3){PhaseA[j].type=0x4D;} //Momentaneo
     else {PhaseA[j].type=0x54;}                            //Temporario

     PhaseA[j].time=RTCTIME;   //Salva o momento da ocorrencia
     PhaseA[j].date=RTCDATE;   //Salva a data da ocorrencia
     count1=0;                 //Zera o contador
     j++;
     
     if(j==max){
       j=0;
     }
  }
}

/*********************************************************************
**************************** Timer 4 Interrupt ***********************
******************************** PHASE B *****************************
*********************************************************************/
void __ISR(_TIMER_4_VECTOR,ipl4) _T4Interrupt(void){ 

  //Limpa o flag da interrupção 
  IFS0bits.T4IF=0;

  count2++;

  if(VBRMS >= 0.9*VRMS && VBRMS <= 1.1*VRMS){

     IEC0bits.T4IE=0;     //Desabilita a interrupcao do Timer4
     temp = 0.01*count2;  //Calcula a duração da VTCD

     if(temp < 0.2){PhaseB[k].type=0x49;}                   //Instantaneo
     else if(temp >= 0.2 && temp < 3){PhaseB[k].type=0x4D;} //Momentaneo
     else {PhaseB[k].type=0x54;}                            //Temporario

     PhaseB[k].time=RTCTIME;   //Salva o momento da ocorrencia
     PhaseB[k].date=RTCDATE;   //Salva a data da ocorrencia
     count2=0;                 //Zera o contador
     k++;
     
     if(k==max){
       k=0;
     }
  }
}

/*********************************************************************
**************************** Timer 5 Interrupt ***********************
******************************** PHASE C *****************************
*********************************************************************/
void __ISR(_TIMER_5_VECTOR,ipl3) _T5Interrupt(void){ 

  //Limpa o flag da interrupção 
  IFS0bits.T5IF=0;

  count3++;

  if(VCRMS >= 0.9*VRMS && VCRMS <= 1.1*VRMS){

     IEC0bits.T5IE=0;     //Desabilita a interrupcao do Timer5
     temp = 0.01*count3;  //Calcula a duração da VTCD

     if(temp < 0.5){PhaseC[l].type=0x49;}                   //Instantaneo
     else if(temp >= 0.5 && temp < 3){PhaseC[l].type=0x4D;} //Momentaneo
     else {PhaseC[l].type=0x54;}                            //Temporario

     PhaseC[l].time=RTCTIME;   //Salva o momento da ocorrencia
     PhaseC[l].date=RTCDATE;   //Salva a data da ocorrencia
     count3=0;                 //Zera o contador
     l++;
     
     if(l==max){
       l=0;
     }
  } 
}
