#include <util/delay.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <math.h>

#define sbi(p,m) p |= (1<<m);      // setbit
#define cbi(p,m) p &= -(1<<m);      // clcbit

#define  UBRR 103 //16MHz -> Baud rate 9600, U2X = 0
#define F_CPU 16000000UL
#define AVCC 5.0

void USART1_TX_vect(unsigned char data)   // Transmit 함수 설정
{
   while(!(UCSR1A & (1<<UDRE1)));   //UDRE = 1 -> buffer empty -> ready to write
   UDR1 = data;
}

unsigned char USART1_RX_vect(void)      // Receive 함수 설정
{
   
   while(!(UCSR1A & (1<<RXC1)));
   return UDR1;
}

void USART1_TX_INT(int nNum)      // ascii code 변환
{
   USART1_TX_vect((nNum % 10000) / 1000 + 48);      // 천의 자리
   USART1_TX_vect((nNum % 1000) / 100 + 48);      // 백의 자리
   USART1_TX_vect((nNum % 100) / 10 + 48);         // 십의 자리
   USART1_TX_vect((nNum % 10) / 1 + 48);         // 일의 자리
}

double Get_ADC(unsigned char ch)      // ADC값 받아오기
{
   ADMUX = ch;                        //어떤 센서를 받아올지 결정
   ADCSRA |= (1<<ADSC);
   while(!(ADCSRA & (1<<ADIF)));
   
   return ADC;
}

double Lm35(int sen)
{
	double VLm35 = Get_ADC(sen)*(5.0/1024);
	double Vout;
	double R12 = 30000;
	double R13 = 10000;
	double TL;
	Vout = (1 + (R12/R13))*VLm35;
	TL = 100*Vout;
	
	
	return TL;
}

 
double Thermistor(int sen)       //Thermistor의 센서값을 읽고, 읽어온 데이터를 수치화 하는 함수.
{
   double Vout = Get_ADC(sen) * (5.0/1024);   //analog 값이 0~1023 이고 digital 값이 0~5V 이기 때문.                            
   double T0 = 25 + 273.15;                   
   double R0 = 1000;
   double B = 3650;
   double R10 = 4.7*1000;
   double Rth;
   double Tk,Tc;         //섭씨 화씨
   
   Rth = (AVCC/Vout)*R10-R10;
   Tk = 1/((1/T0)+(log(Rth/R0)/B));//Log를 사용하여 ln을 계산할 수 있다.
   Tc = Tk - 273.15;//섭씨 = 화씨 - 273.15
   
   return Tc;
}

double CDS(int sen)          //CDS의 센서값을 읽고, 읽어온 데이터를 수치화 하는 함수.
{
   double Vout = Get_ADC(sen) * (5.0/1024)*1000; //analog 값이 0~1023 이고 digital 값이 0~5V 이기 때문.
   double R9 = 4.7*1000;
   double Rcds, x;
   
   Rcds = (R9*AVCC*1000)/Vout-R9;
   x = pow(10,(1-(log10(Rcds)-log10(40000))/0.8));//Log10 을 이용하여 밑이 10인 log계산
                                                  //pow 함수를 사용하여 지수를 계산
   return x;
}


ISR(TIMER2_OVF_vect)      // Timer2 인터럽트 -> 제어주기 설정. ADC값 출력
{
   double ThOut,CDSOut,LmOut,LmOut2;
   //주석을 이용하여 하나씩 측정을 하였다. 지금은 thermistor의 값을 읽기 위해 CDS부분을 주석처리하였다.
   //ThOut = Thermistor(0x03);  //Thermistor함수를 사용하여 센서값을 읽고 수치화한 결과
   //USART1_TX_INT((int)ThOut);   USART1_TX_vect(',');//데이터를 전송한다
   
   LmOut = Lm35(0x02);
   USART1_TX_INT((int)LmOut); USART1_TX_vect(',');
   LmOut2 = Get_ADC(0x02)*(5.0/1024)*100;
   USART1_TX_INT((int)LmOut2);
   
   /*CDSOut = CDS(0x01);       //CDS함수를 사용하여 센서값을 읽고 수치화한 결과
   USART1_TX_INT((int)CDSOut);   USART1_TX_vect(',');*/ //데이터를 전송한다.
   USART1_TX_vect(0x0D);

   TCNT2 = 255- 156;
}

void set_GPIO(void)
{
   cbi(DDRF,0);      // 가변저항
   cbi(DDRF,1);      // CDS
   cbi(DDRF,2);      // LM35
   cbi(DDRF,3);      // 서미스터
   
   sbi(DDRD,3);      // TXD1 출력핀으로 설정
   
}

int main(void)
{
   DDRB = 0xff;
   
   set_GPIO();
   
   ADCSRA |= (1<<ADEN);      // ADC enable
   
   TCCR2 = (1<<WGM21)|(1<<WGM20)|(1<<COM21)|(1<<CS22)|(1<<CS20);      // fast PWM mode, 1024 prescale
   TIMSK = (1<<TOIE2);                                       // timer2 overflow interrupt enable
   TCNT2 = 255- 234;
   
   UBRR1L = (unsigned char) UBRR;
   UBRR1H = (unsigned char) (UBRR>>8);
   
   UCSR1B = (1<<TXEN1);                  //Transmitter Enable
   UCSR1C = (1<<UCSZ11)|(1<<UCSZ10);         //비동기, non-parity mode, stop bit : 1 bit data: 8bit
   
   sei();      // 전체 interrupt enable

   while (1)
   {
   }
}
