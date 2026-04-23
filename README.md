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
<img width="775" height="439" alt="Image" src="https://github.com/user-attachments/assets/42c17ea6-9853-4030-94e9-a7dccd16f829" />

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

## 📍 Hardware Pin Mapping

<details>
<summary>🔍 1. Infineon AURIX TC275 ShieldBuddy (Access / Body)</summary>

| 모듈 | 기능 | Arduino 핀 | MCU 핀 | 라벨 | 모드 | 비고 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **MFRC522** | RFID CS(SDA) | D42 | P11.10 | BASE_COM_QSPI1_CS | Output | |
| | RFID SCLK(SCK) | D47 | P11.6 | BASE_COM_QSPI1_SCLK | Output | |
| | RFID MOSI | D41 | P11.9 | BASE_COM_QSPI1_MTSR | Output | |
| | RFID MISO | D46 | P11.3 | BASE_COM_QSPI1_MRST | Input | |
| **Encoder A** | Encoder | D39 | P00.7 | MOTOR_A_ENC | Input | 시트/미러 제어 |
| | 모터 PWM | D3 | P02.1 | MOTOR_A_PWM | Output | TOM1_9_TOUT1 |
| | 모터 Brake | D9 | P02.7 | MOTOR_A_BRAKE | Output | |
| | 모터 Direction | D12 | P10.1 | MOTOR_A_DIR | Output | |
| **Encoder B** | Encoder | D4 | P10.4 | MOTOR_B_ENC | Input | 시트/미러 제어 |
| | 모터 PWM | D11 | P10.3 | MOTOR_B_PWM | Output | TOM0_3_TOUT105 |
| | 모터 Brake | D8 | P02.6 | MOTOR_B_BRAKE | Output | |
| | 모터 Direction | D13 | P10.2 | MOTOR_B_DIR | Output | |
| **SG90** | 서보모터 PWM | D5 | P02.3 | SERVO_PWM | Output | TOM1_11_TOUT3 |
| **Switch** | 매뉴얼 조그 (4ea) | D6, D7, D22, D23 | - | SW_JOG | Input | 사이드미러/시트 조절 |
| **CAN** | CAN TX / RX | - | P20.8 / P20.7 | CAN_TX / RX | Comm. | 트랜시버 연결 필수 |
| **LED** | 상태 표시 RED / YEL | D29 / D28 | P00.2 / P00.9 | STATUS_LED | Output | 인증 실패 / 성공 |

</details>

<details>
<summary>🔍 2. Infineon AURIX TC375 Lite Kit (HVAC / HMI)</summary>

| 모듈 | 기능 | Arduino 핀 | MCU 핀 | 라벨 | 모드 | 비고 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Joystick** | 조이스틱 X축 | A5 | P40.9 | - | Analog | EVADC 8.7 |
| | 조이스틱 Y축 | A4 | P40.8 | - | Analog | EVADC 8.6 |
| | 조이스틱 스위치 | A3 | P40.7 | JOYSW | Input | |
| **DC Fan** | 냉난방 팬 PWM | D3 | P02.1 | - | Output | TOM0_9_TOUT1 |
| **Red LED** | 난방 표시 LED | D12 | P10.1 | REDLED | Output | Active-low |
| **Blue LED** | 냉방 표시 LED | D13 | P10.2 | BLUELED | Output | Active-low |
| **Neopixel** | 앰비언트 MOSI | D11 | P10.3 | - | QSPI | WS2812B 연동 |
| **LCD1602** | LCD SCL / SDA | SCL / SDA | P13.1 / P13.2 | MCP_SCL / SDA | I2C | Open-drain |
| **CAN** | CAN TX / RX | - | P20.8 / P20.7 | CAN_TX / RX | Comm. | |

</details>

<details>
<summary>🔍 3. Infineon AURIX TC375 Lite Kit (Safety / State Control)</summary>

| 모듈 | 기능 | Arduino 핀 | MCU 핀 | 라벨 | 모드 | 비고 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Buzzer** | 버저 PWM | D10 | P10.5 | - | Output | TOM0_2_TOUT107 |
| **SW-420** | 충격센서 DO | D11 | P10.3 | SHOCK_IN | Input | ERU 인터럽트 |
| **DS18B20** | 온도 센서 DAT | - | P02.1 | TEMP_DATA | OpenDrain | 4.7kΩ Pull-up 필수 |
| **CAN** | CAN TX / RX | - | P20.8 / P20.7 | CAN_TX / RX | Comm. | |

</details>

---

## 📡 CAN FD Message Specification

분산 제어 환경에서의 ECU 간 데이터 동기화 및 상태 제어를 위해 정의된 CAN FD 메시지 구조입니다.

<details>
<summary>🔍 CAN Message List (Communication Matrix)</summary>

| 메시지명 | CAN ID | 송신 | 수신 | Payload | DLC | 주기 | 전송 조건 | 비고 |
| :--- | :---: | :---: | :---: | :--- | :---: | :---: | :---: | :--- |
| **MSG_SS_STATE** | 0x100 | SS | AB, HH | system_state (1B) | 1 | 10ms | Time | 시스템 기준 상태 브로드캐스트 |
| **MSG_AB_AUTH_PROFILE_IDX** | 0x200 | AB | SS, HH | auth_profile_idx (1B) | 1 | - | Event | RFID 인증 결과 전달 (성공: idx, 실패: invalid) |
| **MSG_HH_PROFILE_IDX** | 0x201 | HH | SS, AB | selected_profile_idx (1B) | 1 | - | Event | HMI에서 선택/변경된 profile idx 전달 |
| **MSG_SS_TEMP** | 0x300 | SS | HH | temperature (1B) | 1 | 1000ms | Time | 실시간 온도 정수값 전송 |
| **MSG_SS_PROFILE_TABLE** | 0x400 | SS | AB, HH | profile_table (40B) | 14 | - | Event | SETUP 시 또는 테이블 변경 시 송신 |
| **MSG_AB_PROFILE_TABLE** | 0x401 | AB | HH, SS | profile_table (40B) | 14 | 100ms | Time | 인증 결과 반영 후 필요 시 송신 |
| **MSG_HH_PROFILE_TABLE** | 0x402 | HH | AB, SS | profile_table (40B) | 14 | - | Event | HMI 수정 반영 후 필요 시 송신 |

</details>

### 💡 통신 설계 핵심
- **Multi-Rate Scheduling**: 시스템 상태(10ms), 온도(1000ms), 프로필 업데이트(100ms) 등 데이터의 중요도와 변화 주기에 따라 전송 레이트를 최적화했습니다.
- **Event-Driven Update**: RFID 인증이나 HMI 조작과 같은 사용자 이벤트 발생 시 즉각적으로 데이터를 전송하여 시스템 응답성을 높였습니다.
- **Large Payload (CAN FD)**: 40B 크기의 대용량 프로필 테이블을 전송하기 위해 CAN FD 프레임(DLC 14)을 활용하여 통신 효율을 극대화했습니다.

---

## 📅 Development Period

* Date: 2026. 03. 25. ~ 2026. 04. 07.
