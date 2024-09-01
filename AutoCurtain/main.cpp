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

// 전역 변수 선언
volatile int FSR_flag = 0;           // Force Sensing Resistor (FSR) 플래그
volatile int SNflag = 0;             // Sleep/Night 플래그
volatile int i = 0;                  // servoMotion 함수에서 소리 감지 카운터
volatile int Buzzer_flag = 1;        // Buzzer 활성화 플래그
char str2[] = "NIGHT";               // LCD에 표시할 "NIGHT" 문자열
char strl[] = "MORNING";             // LCD에 표시할 "MORNING" 문자열
unsigned int ResOut, F_ResOut;
double F_CDSOut, F_WavOut, F_FrOut, F_ThOut, F_SNOut;
unsigned int WavOut;
double FsrOut;
double ThOut, CDSOut, LmOut, LmOut2, SNOut;

double Var_MAF_Buffer[20] = {0};     // Moving Average Filter 버퍼
double Sound_FIR_BUFF[51] = {0};     // FIR 필터 버퍼
double b[50] = { /* MATLAB에서 가져온 FIR 필터 계수 입력 */ }; // FIR 필터 계수 배열



// 7-Segment Display를 위한 숫자 배열
const char first_num[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};


// 초음파 센서 관련 핀 정의
#define Trigger_ON   PORTB |= (1 << PB0)   // Trigger 핀 HIGH
#define Trigger_OFF  PORTB &= ~(1 << PB0)  // Trigger 핀 LOW
#define Echo         PINB & (1 << PB1)     // Echo 핀 읽기

// USART 함수들
void USART1_TX_vect(unsigned char data)
{
   while(!(UCSR1A & (1<<UDRE1)));   // UDRE = 1 -> 버퍼 비어있음 -> 전송 준비 완료
   UDR1 = data;                      // 데이터 전송
}

unsigned char USART1_RX_vect(void)      // Receive 함수 설정
{
   while(!(UCSR1A & (1<<RXC1)));
   return UDR1;
}

void USART1_TX_INT(int nNum)      // ASCII 코드 변환
{
   USART1_TX_vect((nNum % 10000) / 1000 + 48);      // 천의 자리
   USART1_TX_vect((nNum % 1000) / 100 + 48);       // 백의 자리
   USART1_TX_vect((nNum % 100) / 10 + 48);         // 십의 자리
   USART1_TX_vect((nNum % 10) / 1 + 48);          // 일의 자리
}

// ADC 값을 받아오는 함수
double Get_ADC(unsigned char ch)
{
   ADMUX = ch;                        // 어떤 센서를 받아올지 결정
   ADCSRA |= (1<<ADSC);               // 변환 시작
   while(!(ADCSRA & (1<<ADIF)));      // 변환 완료 대기
   return ADC;
}

// 필터 관련 함수들

// MEM 함수를 이용하여 버퍼를 이동시키고 최신 값을 마지막에 대입
void MEM_Var(double data)
{
   memmove(Var_MAF_Buffer, Var_MAF_Buffer + 1, sizeof(Var_MAF_Buffer) - sizeof(double));
   Var_MAF_Buffer[19] = data;
}

// MAF - 배열로 과거값을 저장하여 평균값을 출력
double MAF_Var(double out)
{
   double sum_data = 0;
   MEM_Var(out);
   for(int j = 0; j < 20; j++)  // 인덱스 변수 j 선언
   {
      sum_data += Var_MAF_Buffer[j];  // 배열의 합을 구해서 크기 20으로 나누어 평균을 구함
   }
   return sum_data / 20.0;
}

// FIR 필터 함수
double FIR(double out)
{  
   int n = 50;
   double FIR_y = 0;
   MEM_Var(out);
   for(int j = 0; j < n; j++)  // 인덱스 변수 j 선언
   {
      Sound_FIR_BUFF[j+1] = b[j] * Var_MAF_Buffer[n-j-1];
      FIR_y += Sound_FIR_BUFF[j+1];
   }
   return FIR_y;
}

// LM35 온도 센서 함수
double Lm35(int sen)
{
   double VLm35 = Get_ADC(sen) * (5.0/1024);
   double Vout;
   double R12 = 30000;
   double R13 = 10000;
   double TL;
   Vout = (1 + (R12/R13)) * VLm35;
   TL = 100 * Vout;
   
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


// CDS - 조도 센서
double CDS(int sen)          //CDS의 센서값을 읽고, 읽어온 데이터를 수치화 하는 함수.
{
   double Vout = Get_ADC(sen) * (5.0/1024); //analog 값이 0~1023 이고 digital 값이 0~5V 이기 때문.
   double R9 = 4.7*1000;   // CDS에 연결된 저항 값
   double Rcds, x;         // CDS 저항 값, 수치화된 조도
   
   Rcds = (R9*AVCC)/Vout-R9;
   x = pow(10,(1-(log10(Rcds)-log10(40000))/0.8)); // Log10 을 이용하여 밑이 10인 log계산
                                                   // pow 함수를 사용하여 지수를 계산
   return x;
}


// DC모터 종합 제어 함수
void CDSMotion(double Rout, double Cout)
{  
   // FSR 플래그가 1이면 물체가 감지된 상태
   if(FSR_flag == 1)
   {
      // SNflag가 1이면 자고 있는 상태이며, 커튼을 올림
      if(SNflag == 1)
      {
         PORTD = 0b00000001; // 커튼을 올린다.
      }
   }
   // 물체가 없을 경우, 조도와 가변저항으로 커튼을 조절
   else 
   {
      if(Rout < 340)
      {
         PORTD = 0b00000001; // 커튼을 올린다.
      }
      else if(Rout > 680)
      {
         PORTD = 0b00000010; // 커튼을 내린다.
      }
      else
      {
         if(Cout > 600)
         {
            PORTD = 0b00000010; // 커튼을 내린다.
         }
         else
         {
            PORTD = 0b00000000; // 모터 정지
         }
      }
   }
}

// UltraWave - 초음파 센서
unsigned int getEcho(void)
{
   Trigger_ON; _delay_us(10); Trigger_OFF;      // Trigger에 10us 신호 출력, 이후 초음파 센서 자체적으로 8cycle의 초음파 출력.
   while(Echo==0x00); TCCR3B=0x02; TCNT3=0x00;  // 초음파가 도착한 후, echo rising, 이때부터 echo가 low로 떨어질 때까지 카운팅할 카운터3 셋팅.
   while(Echo!=0x00); TCCR3B=0x08;              // echo가 low로 떨어졌을 때, 타이머3 off.
   return (TCNT3/58);                           // 카운팅한 값 == 센서부터 물체까지 빛의 속도로 걸린 시간.
                                                // 1cm에 58us가 걸리므로 물체와의 거리 == (카운팅한 값 = TCNT3)/58.
}

//buzzer
void WaveMotion(unsigned int out)//초음파 센서로 거리를 인식하여 일정 거리 안에 물체가 들어오면 소리를 낸다.
{
   if(out < 20)   //거리가 20cm안에 물체가 인식이 되면 buzzer tlag를 1로하여 소리를 낸다.
   {
      if (Buzzer_flag==1)
      {
         PORTB |= 0x10; 
         _delay_ms (10);
         Buzzer_flag = 0;
      }
      else
      {
         PORTB &= ~0x10;
      }
   }
   else
   {
      PORTB &= ~0x10;
      Buzzer_flag = 1;
   }   
}

// LCD 제어 함수
void Lcd(double data)
{
   if(data > 50)  // 센서를 누르면 올라감 (data가 증가)
   {
      lcd_str(str2);  // "NIGHT" 문자열 LCD에 출력
      lcd_iw(0x01); 
      FSR_flag = 1;     // FSR 플래그 설정
   }
   else
   {
      lcd_str(strl);    // "MORNING" 문자열 LCD에 출력
      lcd_iw(0x01);
      FSR_flag = 0;     // FSR 플래그 해제
   }
}

// 서미스터 수치화 함수
double Thermistor(int sen)
{
   double Vout = Get_ADC(sen) * (5.0/1024);  // ADC값은 0~1023. 이를 5v이내의 값으로 얻기 위한 식이다.
   double TO = 25 + 273.15;                  // T 초기값
   double Re = 1000;                         // R 초기값
   double B = 3650;                          // Beta Value
   double R10 = 4.7 * 1000；                 // 서미스터에 연결된 저항 값 
   double Rth;                               // 서미스터 저항 값
   double Tk, Tc;                            // 수치화된 켈빈 온도, 섭씨 온도
   
   Rth = (AVCC/Vout)*R10-R10;
   Tk = 1 / ((1/T0)+(Log(Rth/R0)/B)):
   Tc = Tk - 273.5;

   return Tc;
}

// 7-Segment Display 제어 함수
void seven_segment(double data) // 서미스터의 온도를 7-세그먼트로 출력
{
   char temp; 
   int second; 
   int first;

   second = (int)(data / 10); // 앞자리 (십의 자리)
   first = (int)(data - (second * 10)); // 뒷자리 (일의 자리)

   switch(second)
   {
      case 0: PORTE |= 0b11100000; break;
      case 1: 
         temp = PINE & 0b11011111; 
         temp |= 0b11000000; 
         PORTE = temp;
         break;
      case 2: 
         temp = PINE & 0b10011111; 
         temp |= 0b10000000; 
         PORTE = temp;
         break;
      case 3: 
         temp = PINE & 0b00011111; 
         PORTE = temp;
         break;
   }
   PORTC = first_num[first]; // 일의 자리 숫자 출력
   _delay_ms(20); // 디스플레이 안정화 대기
}

// Servo 모터 제어 함수
unsigned int set_servo(double angle)// 모터의 각도를 조절하는 함수
{
   // 0도: 0.5ms, 90도: 1.5ms, 180도: 2.5ms
   double width;
   double duty;
   
   width = angle / 90.0 + 0.5;

   duty = (width / 20) * 100; // 서보모터 주기가 20ms임을 고려
   
   OCR1A = (duty / 100) * ICR1; // PWM 신호 설정
   
   return OCR1A;
}

// Servo 모션 제어 함수
void servoMotion(double out)// 알람 소리가 계속 울리면 서보모터를 작동
{
   if(SNflag == 0) // 소리가 없는 상태 (조용)
   {
      set_servo(0); // 서보모터를 0도로 설정
      if(out > 100)
      {
         i++;
         if (i == 30)
         {
            SNflag = 1; // 벨소리가 계속 울리면 SNflag를 1로 설정
         }
      }
   }
   else if(SNflag == 1) // 소리가 있는 상태
   {
      set_servo(90); // 서보모터를 90도로 설정
      if(out < 70)
      {
         i--;
         if(i == 0)
         {
            SNflag = 0; // 소리가 멈추면 SNflag를 0으로 설정
         }
      }
   }
}

// LED 밝기 조절 함수
void Led(double data) // LED 밝기 조절
{
   if(data <= 340)
   {
      PORTA = 0xff; // LED 전체 켜기
   }
   else if(data <= 680)
   {
      PORTA = 0x00; // LED 끄기
      _delay_ms (100);
      PORTA = 0xff; // LED 다시 켜기
   }
   else
   {
      PORTA = 0x00; // LED 끄기
   }
}


// Timer2 오버플로우 인터럽트 서비스 루틴
ISR(TIMER2_OVF_vect)      // Timer2 인터럽트 -> 제어주기 설정. ADC값 출력
{
   // 센서 출력 변수 선언
   double ThOut, CDSOut, LmOut, LmOut2;
   
   // Thermistor 값 읽기 및 전송 (주석 처리 해제 시 사용)
   // ThOut = Thermistor(0x03);  // Thermistor 함수를 사용하여 센서값을 읽고 수치화한 결과
   // USART1_TX_INT((int)ThOut);   // 데이터 전송
   // USART1_TX_vect(',');         // 구분자 전송
   
   // LM35 온도 센서 값 읽기 및 전송
   LmOut = Lm35(0x02);
   USART1_TX_INT((int)LmOut); 
   USART1_TX_vect(',');
   LmOut2 = Get_ADC(0x02) * (5.0 / 1024) * 100;
   USART1_TX_INT((int)LmOut2);
   
   // CDS 센서 값 읽기 및 전송
   CDSOut = CDS(0x01);       // CDS 함수를 사용하여 센서값을 읽고 수치화한 결과
   F_CDSOut = FIR(CDSOut);   // FIR 필터 적용
   USART1_TX_INT((int)F_CDSOut); 
   USART1_TX_vect(','); // 데이터 전송
   CDSMotion(F_ResOut, F_CDSOut); // DC모터 제어 함수 호출
   
   // 초음파 센서 값 읽기 및 전송
   WavOut = getEcho();
   F_WavOut = FIR(WavOut);
   USART1_TX_INT((int)F_WavOut); 
   USART1_TX_vect(','); 
   WaveMotion(F_WavOut); // 일정 거리 안에 물체가 있으면 Buzzer 제어
   
   // FSR 센서 값 읽기 및 전송
   FsrOut = Get_ADC(0x02);
   F_ResOut = MAF_Var(FsrOut);
   USART1_TX_INT((int)F_ResOut); 
   USART1_TX_vect(',');
   Lcd(F_ResOut); // LCD로 상태 표시
   
   // Thermistor 값 읽기 및 전송
   ThOut = Thermistor(0x03);
   F_ThOut = MAF_Var(ThOut);   // MAF 필터 적용
   USART1_TX_INT((int)F_ThOut); 
   USART1_TX_vect(',');
   seven_segment(F_ThOut); // 7-Segment Display에 온도 표시
   
   // 소리 센서 값 읽기 및 전송
   SNOut = Get_ADC(0x05);
   F_SNOut = FIR(SNOut);
   USART1_TX_INT((int)F_SNOut); 
   USART1_TX_vect(',');
   servoMotion(F_SNOut); // Servo 모터 제어
   
   // 가변저항 값 읽기 및 전송
   ResOut = Get_ADC(0x00);
   F_ResOut = MAF_Var(ResOut);
   USART1_TX_INT((int)F_ResOut); 
   USART1_TX_vect(',');
   Led(F_ResOut); // LED 밝기 조절
   
   USART1_TX_vect(0x0D); // 캐리지 리턴 전송
   
   TCNT2 = 255 - 156; // Timer2 재설정
}

// GPIO 설정 함수
void set_GPIO(void)
{
   cbi(DDRF,0);      // 가변저항 입력 설정
   cbi(DDRF,1);      // CDS 입력 설정
   cbi(DDRF,2);      // LM35 입력 설정
   cbi(DDRF,3);      // Thermistor 입력 설정
   
   sbi(DDRD,3);      // TXD1 출력핀으로 설정
}

int main(void)
{
   // 포트 설정
   DDRF = 0x00; // DDRF = 0: 입력, 1: 출력
   DDRB = 0xff; // DDRB = 1: 출력
   DDRA = 0xff; // LED 포트 출력 설정
   DDRD = 0xff; // LCD 상위 4비트 출력 설정
   DDRE = 0xf7; // LCD RS RW E 하위 3비트 출력 설정
   DDRC = 0xff; // 7-Segment Display 포트 출력 설정

   // 초기 포트 상태 설정
   PORTD = 0x00;
   PORTA = 0x00; // LED 초기값 OFF
   PORTE = 0xff;

   // ADC 설정
   ADCSRA = 0b10000111; // ADC enable, 프리스케일 128

   // Timer1 설정 (Servo 제어용)
   TCCR1A = (1<<COM1A1) | (1<<WGM11) | (1<<COM1C1); // Fast PWM, 비반전 모드
   TCCR1B = (1<<WGM13) | (1<<WGM12) | (1<<CS11) | (1<<CS10); // Fast PWM, 분주비 64
   ICR1 = 4999; // Top 값 설정
   OCR1A = 125; // 초기 PWM 듀티 사이클 설정 (0도)
   
   // Timer2 설정
   TCCR2 = (1<<WGM21) | (1<<WGM20) | (1<<COM21) | (1<<CS22) | (1<<CS20); // Fast PWM, 프리스케일 1024
   TIMSK = (1<<TOIE2); // Timer2 오버플로우 인터럽트 활성화
   TCNT2 = 255 - 234; // Timer2 초기값 설정

   // Timer3 설정 (초음파 센서 타이밍용)
   TCCR3A = 0x00; // CTC 모드 설정
   TCCR3B = 0x80; // CTC 모드, 프리스케일 1

   // USART1 설정
   UBRR1L = (unsigned char) UBRR;
   UBRR1H = (unsigned char) (UBRR >> 8);
   UCSR1B = (1<<TXEN1);                  // Transmitter Enable
   UCSR1C = (1<<UCSZ11) | (1<<UCSZ10);    // 비동기, 데이터 비트 8비트

   init_lcd(); // LCD 초기화
   sei();      // 전역 인터럽트 활성화

   while (1)
   {
      // 메인 루프는 비어있으며, 모든 동작은 인터럽트에서 처리됨
   }
}
