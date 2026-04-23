# 🚗 Profile-based Vehicle Personalization System

> **"사용자 인식부터 비상 탈출 시나리오까지, Infineon AURIX 기반의 SDV 초개인화 통합 제어 시스템"**

## 📺 Project Overview
본 프로젝트는 **Infineon AURIX** 환경에서 RFID 사용자 식별과 데이터 플래시(DFLASH) 관리를 결합한 임베디드 시스템입니다. 운전자별 최적화된 차량 환경(시트, 미러, 공조)을 자동으로 구성하고, 화재 등 비상 상황 시 탑승자의 골든타임을 확보하는 안전 시나리오를 수행합니다.

---

## 🏗️ System Architecture & Multi-ECU Role
본 시스템은 3개의 ECU가 **CAN FD(64-byte)** 통신을 통해 유기적으로 데이터를 주고받으며 분산 제어를 수행합니다.

### 1️⃣ AURIX TC275 - Access & Body (Access Control)
* **RFID 인증**: RC522 모듈을 활용하여 사용자 UID 식별 및 프로필 매핑 
* **액추에이터 제어**: DC 엔코더 모터를 통한 시트 포지션/사이드미러 각도 조절 및 서보 모터를 이용한 도어 개폐
* **입력 인터페이스**: 택트 스위치를 통한 실시간 모터 수동 조그 조절 기능

### 2️⃣ AURIX TC375 - Safety & State (Main Controller)
* **시스템 상태 관리**: 전체 시스템의 **FSM(Finite State Machine)** 구현 및 ECU 간 상태 동기화
* **데이터 영구 저장**: DFLASH(FEE)를 활용하여 전원 차단 시에도 사용자별 설정값 유지 및 복원
* **위급 상황 감지**: DS18B20 온도 센서 기반 화재 감지 및 `EMERGENCY` 모드 강제 전이 로직

### 3️⃣ AURIX TC375 - HVAC & HMI (Comfort & UI)
* **공조 시스템(HVAC)**: 실시간 실내 온도와 임계값을 비교하여 팬 속도 자동 제어
* **앰비언트 라이트**: 네오픽셀(WS2812B)을 이용한 상황별 시각적 피드백 제공 (정상/인증실패/비상) 
* **사용자 인터페이스**: LCD와 조이스틱을 활용한 개인화 설정값(온도, 색상 등) 변경 및 저장

---

## 🔄 System State Machine
시스템의 안정성을 위해 다음과 같은 6가지 상태 전이 로직을 설계하였습니다. 

1. **SLEEP**: 저전력 대기 및 사용자 RFID 태그 대기 
2. **SETUP**: 인증 성공 후 DFLASH에서 데이터를 로드하여 하드웨어 복원 구동
3. **ACTIVATED**: 시스템 활성화 및 주행 중 사용자 설정 변경 실시간 모니터링
4. **SHUTDOWN**: 하차 시 현재 설정을 DFLASH에 최종 기록 후 슬립 복귀
5. **EMERGENCY**: 화재 감지 시 즉시 전이하여 도어 개방 및 탈출 환경 조성
6. **DENIED**: 인증 실패 상태로, 시각적 경고 후 슬립 모드 복귀

---

## 🛠️ Tech Stack
### Hardware
* **MCU**: Infineon AURIX TC275, TC375
* **Sensors**: RFID RC522, DS18B20 (Temperature)
* **Actuators**: DC Encoder Motor, Servo Motor, Cooling Fan, Neopixel
* **Peripheral**: I2C LCD, Joystick, QSPI, GTM(PWM), EVADC, STM

### Communication & Protocol
* **CAN FD**: 64-byte 롱 프레임을 활용한 대용량 프로필 데이터 전송
* **SPI / I2C / 1-Wire**: 각종 센서 및 디바이스 통신 표준 적용

---

## 📁 Repository Structure
```
project_root/
├─ Shared/                          ← 공통 인터페이스 원본
├─ Docs/                            ← 공식 문서
├─ Ecu_Safety_State_TC375/          ← ADS 프로젝트 단위
│   ├─ AppSw/
│   ├─ BaseSw/
│   ├─ Shared/                      ← 원본에서 복사
│   ├─ Cpu0_Main.c
│   ├─ Cpu1_Main.c
│   └─ Cpu2_Main.c
├─ Ecu_Access_Body_TC275/
│   └─ (동일 구조)
├─ Ecu_Hvac_Hmi_TC375/
│   └─ (동일 구조)
├─ Integration/
├─ Tools/
│   └─ Sync_Shared.sh
├─ .gitignore
└─ README.md
```

---

## 📅 Development Period

* Date: 2026. 03. 25. ~ 2026. 04. 07.
