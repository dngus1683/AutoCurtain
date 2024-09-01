# 자동 커튼 만들기 프로젝트
<img src="https://github.com/user-attachments/assets/9934f54e-90ad-4981-84a9-0f77828d80b9" width="960" height="400"/>

## 목차
1. [개요](#개요)
2. [서론](#서론)
3. [실험 환경 구성](#실험-환경-구성)
   - [H/W 시스템 구조](#H/W-시스템-구조)
4. [실험 결과](#실험-결과)
   - [CDS](#cds)
   - [HC-SR04 초음파 센서](#hc-sr04-초음파-센서)
   - [FSR-400 압력센서](#fsr-400-압력센서)
   - [Thermister 온도센서](#thermister-온도센서)
   - [CZN-15E 사운드 센서](#czn-15e-사운드-센서)
   - [ADC 가변저항](#adc-가변저항)

## 개요
로봇학실험3 강의에서는 MATLAB과 Atmega128의 활용, 필터링, 센서 제어 등을 배웠다. 이들을 종합하여 하나의 결과물을 만드는 것이 Final Term Project의 목표였다.
- **기간**: 2020.03-2020.06
- **팀원 & 역할 분담**:
  - [장우현](https://github.com/dngus1683): FSR-400 압력센서, CZN-15E 사운드 센서, CDS.
  - [허정범](https://github.com/okpocandy): HC-SR04 초음파 센서, Thermister 온도센서, ADC 가변저항  
- **사용 언어**: C
- **사용 툴**: AtmelStudio
- **실습 환경**: ATMega128
  
## 서론
여름과 겨울철 실내 냉난방이 필수적인 현대사회에서, 에너지 절약에 대한 문제가 중요하게 다뤄지고 있다. 이에 따라 냉난방 전자제품을 사용하지 않으면서도 비슷한 효과를 내는 방법들이 제안되고 있다. 이번 프로젝트에서 우리 팀이 제안한 방법은 '자동 커튼' 시스템이다.

커튼은 창문 밖 햇빛을 차단하여 실내 온도 상승을 막고, 겨울에는 바깥의 냉기를 물리적으로 차단하여 실내 온도 하강을 막아주는 역할을 한다. 그러나 커튼을 사용할 때마다 사용자가 직접 조작해야 하는 불편함이 있다. 이를 해결하기 위해 Atmega128을 접목한 자동 커튼 시스템을 개발하였다.


## 실험 환경 구성

### H/W 시스템 구조
- H/W Outline

  ![image](https://github.com/user-attachments/assets/58bfe4c3-e63c-4d32-bc22-1586d10c3a6a)

- System Architecture

  ![image](https://github.com/user-attachments/assets/97f42b43-7441-4636-8b47-25cd7325646d)

## 실험 결과

### CDS
CDS 센서는 주변 빛의 밝기 변화로 저항값이 변하면서 생기는 전압값으로 조도를 측정하는 센서다. CDS 센서는 주변 밝기가 일정해도 센서값이 진동하는 형태를 보인다. 이를 개선하기 위해 FIR 필터를 사용하여 신호를 선형적으로 변환하였으며, 특정 조도 값을 기준으로 DC 모터를 구동하여 커튼을 내리는 기능을 구현하였다.

![image](https://github.com/user-attachments/assets/495e6684-9d18-43bd-9bac-b9d0bf44357a)

### HC-SR04 초음파 센서
초음파 센서는 센서에서 보낸 초음파가 물체에 반사되어 돌아오는 시간을 측정하여 거리를 계산하는 센서다. 이 센서를 활용하여 커튼이 일정 위치에 도달했을 때, Buzzer를 울려 성공적으로 커튼이 내려왔음을 알린다. FIR 필터를 사용하여 노이즈 제거와 함께 안정적인 신호 처리를 수행하였다.

![image](https://github.com/user-attachments/assets/3a531852-1995-4b04-9d2f-abea9c06f87b)

### FSR-400 압력센서
FSR-400 압력 센서는 압력의 변화로 발생하는 전압값을 측정하여 시스템의 모드를 제어하는 역할을 한다. 이 센서를 통해 사용자가 커튼을 올리거나 내리는 모드를 설정할 수 있다. MAF 필터를 사용하여 신호의 갑작스런 변화를 완화하고 안정적인 모드 전환을 구현하였다.

![image](https://github.com/user-attachments/assets/9f9fb1f6-903c-4d66-b4d5-f4ef74486f17)

### Thermister 온도센서
Thermister 온도 센서는 온도가 상승할수록 저항이 감소하는 센서로, 주변 온도를 측정한다. 필터링된 온도 값은 7-segments를 통해 실내 온도를 사용자에게 알려준다. MAF 필터를 적용하여 신호를 안정적으로 출력하도록 하였다.

![image](https://github.com/user-attachments/assets/37393417-5e54-497d-9138-980497c039bc)

### CZN-15E 사운드 센서
CZN-15E 사운드 센서는 소리의 크기를 측정하는 센서다. 이 센서는 밤 모드에서 휴대폰의 기상 알람 소리를 감지하여 커튼을 올리고, Servo 모터를 통해 "아침입니다."라는 문구를 표시하는 역할을 한다. FIR 필터를 적용하여 신호의 불규칙성을 줄이고, 모션 제어를 원활하게 하였다.

![image](https://github.com/user-attachments/assets/338a6264-caca-4468-b0cf-59dcf294ec2d)

### ADC 가변저항
가변저항은 저항값을 임의로 변경할 수 있는 부품으로, 사용자가 커튼을 수동으로 조작할 때 사용된다. 필터링된 신호를 통해 커튼을 올리거나 내리는 동작을 수행할 수 있으며, LED의 밝기를 조절하여 저항값의 변화를 시각적으로 확인할 수 있다.

![image](https://github.com/user-attachments/assets/d9562cab-2438-40ff-86d3-a25cb462c1cd)

