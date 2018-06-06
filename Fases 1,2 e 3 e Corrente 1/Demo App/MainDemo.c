//* Includes
#include <p32xxxx.h>
#include "TCPIPConfig.h"			// Include all headers for any enabled TCPIP Stack functions
#include "TCPIP Stack/TCPIP.h"
#include "MainDemo.h"
#include "time.h"
#include "stdio.h"

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

//* Defines Calibragem
#define PLACA2
//PLACA1
#if defined(PLACA1)
#define NORM_VARMS          4.6869 		// Rela��o de normaliza��o para VARMS
#define NORM_VBRMS          4.6506 		// Rela��o de normaliza��o para VBRMS
#define NORM_VCRMS          4.6139 		// Rela��o de normaliza��o para VCRMS

//PLACA2
#elif defined(PLACA2)       
#define NORM_VARMS          4.6278		// Rela��o de normaliza��o para VARMS
#define NORM_VBRMS          4.7515		// Rela��o de normaliza��o para VBRMS
#define NORM_VCRMS          4.6527		// Rela��o de normaliza��o para VCRMS
#endif

//* Declare AppConfig structure and some other supporting stack variables
APP_CONFIG AppConfig;

//* Vari�veis Globais
unsigned char SPI_DUMMY;
unsigned char SPI_DATA_RX[3];
static signed int    VARMS;
static signed int    VBRMS;
static signed int    VCRMS;
static signed int    FQA; 
int i=0;


//* Fun��es
static void InitAppConfig(void);
static void InitializeBoard(void);
void SPI_READ(unsigned char SPI_RDQUANT, unsigned char SPI_ADDRESS);
void SPI_WRITE(unsigned char SPI_WRQUANT, unsigned char SPI_ADDRESS, unsigned char SPI_DATA1, unsigned char SPI_DATA2, unsigned char SPI_DATA3);

//********************************************************************
//******************************** MAIN ******************************
//********************************************************************
int main(void){

//****** Vari�veis locais
  unsigned char FRAME_RXBUF[30];    //****** 
  unsigned char FRAME_TXBUF[120];
  unsigned char FRAME_TXMESS;
  DWORD   	    FRAME_RXQTY;          //****
  unsigned int  MAIN_CONT;
  static TCP_SOCKET	MySocket;
  unsigned char TCPServerState=SM_HOME;  
  //unsigned int i;
  //time_t start_t, end_t;
                
  //****** Configura��o dos pinos de I/O
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

  //****** Configura��o da comunica��o SPI1
  SPI1CON=0; 						//Stops and resets the SPI1. 
  SPI_DUMMY=SPI1BUF;	    		//clears the receive buffer
  SPI1BRG=31;						//use FPB/4 clock frequency
  SPI1STATbits.SPIROV=0;			//clear the Overflow
  SPI1CON=0x00008260;				//SPI ON, 8 bits transfer, SMP=1, Master mode, CKP=1,CKE=0 (com invers�o l�gica)

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
  SPI_WRITE(1,0x13,0x00,0x00,0x00); //Registrador OPMODE em opera��o normal
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

  //***** Configura��es do Timer2 T2 para 22ms
  T2CON=0x8070;                     //Prescaler de 1/256 = 80/256 = 0,3125MHz (3,2us)
  PR2=31250;                        //Carga para compara��o = 31250 x 3,2us = 100ms
  IPC2bits.T2IP=6;					//Interrupt priority 6
  IFS0bits.T2IF=0;                  //Interrupt flag
  IEC0bits.T2IE=1;					//Habilita interrup��o para o Timer2

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
 
    if(i==1){
    //inicia tempo
    while(i==1){}
    //para tempo
    //salva data e hora
    
    }
   
    //****** M�quina de estados TCP Server
    StackTask();

   	switch(TCPServerState){

      case SM_HOME:
        MySocket = TCPOpen(0, TCP_OPEN_SERVER, SERVER_PORT, TCP_PURPOSE_GENERIC_TCP_SERVER); // Tenta obter um SOCKET para o Servidor TCP
	    if(MySocket==INVALID_SOCKET) break; 	   		// N�o -> Sai
	   	TCPServerState = SM_LISTENING;					// Sim -> verifica se algu�m conecta
	  break;
	
	  case SM_LISTENING:
	    if(!TCPIsConnected(MySocket)) break;           	// Se n�o obteve a conex�o sai
		TCPServerState=SM_PROCESS_RECEIVE;  	        // Se obteve a conex�o, realiza os processamentos 
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

            //start_t = time(NULL);
            //for(i=0; i<20000000;i++){}
            //end_t = time(NULL);

            //total_t = (double)(end_t - start_t) / CLOCKS_PER_SEC;
            //tempo = difftime(start_t, end_t);

            FRAME_TXMESS=2;  
            TCPServerState=SM_PROCESS_TRANSMIT;  
          }

          //Verifica se ocorreu VTCD - Variacao de Tensao de Curta Duracao
          else if(FRAME_RXBUF[0]=='V' && FRAME_RXBUF[1]=='T' && FRAME_RXBUF[2]=='C' && FRAME_RXBUF[3]=='D'){
            FRAME_TXMESS=3;  
            TCPServerState=SM_PROCESS_TRANSMIT;
          } 
       }
  
      break;    

	  case SM_PROCESS_TRANSMIT: 
               
        //Se n�o puder carregar o buffer TX, sai
        if(TCPIsPutReady(MySocket)<20u) break;
        
        // **** VTCD
        else if(FRAME_TXMESS==3){
        
        //*****SAG
        if(i==1){
        i=0;
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]='S';
		FRAME_TXBUF[2]='A';
        FRAME_TXBUF[3]='G';
        FRAME_TXBUF[4]=0x0D;
        FRAME_TXBUF[5]=0x0A;
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,6);
        }
        
        //****SWELL
        else if(VARMS > 1.1*VRMS){
        FRAME_TXBUF[0]=0x20; 
        FRAME_TXBUF[1]='S';
		FRAME_TXBUF[2]='W';
        FRAME_TXBUF[3]='E';
        FRAME_TXBUF[4]='L';
        FRAME_TXBUF[5]='L';
        FRAME_TXBUF[6]=0x0D;
        FRAME_TXBUF[7]=0x0A;
        //Carrega no buffer os dados a serem transmitidos
        TCPPutArray(MySocket,FRAME_TXBUF,8);
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
        }//End of VTCD
        
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

        //Se for o caso realiza a transmiss�o
        TCPFlush(MySocket);
        TCPServerState=SM_PROCESS_RECEIVE;            
      break; 
    
    }//switch   
  }//loop geral
}//fun��o main

// 

//********************************************************************
//***************************** SPI WRITE ****************************
//********************************************************************
void SPI_WRITE(unsigned char SPI_WRQUANT, unsigned char SPI_ADDRESS, unsigned char SPI_DATA1, unsigned char SPI_DATA2, unsigned char SPI_DATA3){

  unsigned char SPI_CONT;

  for (SPI_CONT=0;SPI_CONT<=5;SPI_CONT++) {} 	    //Aguarda tempo para mudar CS
  SPI_CS=1;   										//Coloca CS em n�vel 1 (invertido)
 
  while(SPI1STATbits.SPITBE==0) {} 					//Verifica se pode transmitir
  SPI1BUF=((SPI_ADDRESS|0x80)^0xFF);  				//Envia o SPI ADDRESS com l�gica invertida
  while(SPI1STATbits.SPIRBF==0) {}
  SPI_DUMMY=SPI1BUF;                                //Recebe o dado DUMMY 
  
  while(SPI1STATbits.SPITBE==0) {} 					//Verifica se pode transmitir
  SPI1BUF=(SPI_DATA1^0xFF);  						//Envia o SPI_DATA1 com l�gica invertida 
  while(SPI1STATbits.SPIRBF==0) {}
  SPI_DUMMY=SPI1BUF;                                //Recebe o dado DUMMY
 
  if(SPI_WRQUANT>1){
    while(SPI1STATbits.SPITBE==0) {} 				//Verifica se pode transmitir
    SPI1BUF=(SPI_DATA2^0xFF);  						//Envia o SPI_DATA2 com l�gica invertida 
    while(SPI1STATbits.SPIRBF==0) {}
    SPI_DUMMY=SPI1BUF;                              //Recebe o dado DUMMY
  }
 
  if(SPI_WRQUANT>2){
    while(SPI1STATbits.SPITBE==0) {} 				//Verifica se pode transmitir
    SPI1BUF=(SPI_DATA3^0xFF);  						//Envia o SPI_DATA3 com l�gica invertida 
    while(SPI1STATbits.SPIRBF==0) {}
    SPI_DUMMY=SPI1BUF;                              //Recebe o dado DUMMY 
  }
  
  SPI_CS=0;   										//Coloca CS em n�vel 0 (invertido)
}

//********************************************************************
//***************************** SPI READ *****************************
//********************************************************************
void SPI_READ(unsigned char SPI_RDQUANT, unsigned char SPI_ADDRESS){
 
  unsigned char SPI_CONT;

  for (SPI_CONT=0;SPI_CONT<=2;SPI_CONT++) {}  		//Aguarda tempo para mudar CS
  SPI_CS=1;   										//Coloca CS em n�vel 1 (invertido)
  
  while(SPI1STATbits.SPITBE==0) {} 				    //Verifica se pode transmitir
  SPI1BUF=((SPI_ADDRESS&0x7F)^0xFF);  				//Envia o SPI ADDRESS com l�gica invertida
  while(SPI1STATbits.SPIRBF==0) {}
  SPI_DUMMY=SPI1BUF;                                //Recebe o dado DUMMY
   
  while(SPI1STATbits.SPITBE==0) {} 				    //Verifica se pode transmitir
  SPI1BUF=0xFF;										//Envia o byte dummy
  while(SPI1STATbits.SPIRBF==0) {}
  SPI_DATA_RX[0]=(SPI1BUF^0XFFFFFFFF);              //Recebe dado v�lido e inverte

  if(SPI_RDQUANT>1){
    while(SPI1STATbits.SPITBE==0) {} 			    //Verifica se pode transmitir
  	SPI1BUF=0xFF;									//Envia o byte dummy
  	while(SPI1STATbits.SPIRBF==0) {}
  	SPI_DATA_RX[1]=(SPI1BUF^0XFFFFFFFF);            //Recebe dado v�lido e inverte
  }
 
  if(SPI_RDQUANT>2){
    while(SPI1STATbits.SPITBE==0) {} 			    //Verifica se pode transmitir
    SPI1BUF=0xFF;									//Envia o byte dummy
    while(SPI1STATbits.SPIRBF==0) {}
    SPI_DATA_RX[2]=(SPI1BUF^0XFFFFFFFF);            //Recebe dado v�lido e inverte
  }
  
  SPI_CS=0;   										//Coloca CS em n�vel 0 (invertido)
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
  while(ADE_IRQ==0){}     		    //Se passou reseta o indicador de interrup��o(registrador RSTATUS) e os registradores de pot�ncia  
  SPI_READ(3,0x1A);                 //Eh enviado pelo registrador RSTATUS (0x1A) a instrucao que reseta o indicador de amostra recebida (retorna IRQ para 0) 
   
  //Calcula a tens�o RMS para a fase A
  SPI_READ(3,0x0D); //AVRMS  
  VARMS=((65536*SPI_DATA_RX[0])+(256*SPI_DATA_RX[1])+SPI_DATA_RX[2])/NORM_VARMS;
}

void getBVRMS(void){

  //* Aguarda a fase B passar pelo zero
  SPI_WRITE(3,0x18,0x00,0x04,0x00); 
  while(ADE_IRQ==0){}     		    //Se passou reseta o indicador de interrup��o(registrador RSTATUS) e os registradores de pot�ncia  
  SPI_READ(3,0x1A); 					

  //Calcula a tens�o RMS para a fase B
  SPI_READ(3,0x0E); //BVRMS  
  VBRMS=((65536*SPI_DATA_RX[0])+(256*SPI_DATA_RX[1])+SPI_DATA_RX[2])/NORM_VBRMS;  
}

void getCVRMS(void){

  //* Aguarda a fase C passar pelo zero
  SPI_WRITE(3,0x18,0x00,0x08,0x00); 
  while(ADE_IRQ==0){}     		    //Se passou reseta o indicador de interrup��o(registrador RSTATUS) e os registradores de pot�ncia  
  SPI_READ(3,0x1A); 					
  
  //Calcula a tens�o RMS para a fase C
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

  //Limpa o flag da interrup��o (devia estar no in�cio, mas pode atrapalhar o c�lculo fasorial)
  IFS0bits.T2IF=0;

  //Realiza a aquisi��o das frequencias

  //Se j� contou pr�ximo de 100ms (> 4 periodos do sinal(66ms)), executa abaixo
  getFREQ();

  //Realiza a aquisi��o das tens�es RMS
  getAVRMS();
  
  if(VARMS < 0.9*VRMS){
    i = 1;  
  }
  
  getBVRMS();
  getCVRMS();
}