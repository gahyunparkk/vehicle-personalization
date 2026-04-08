# 🚗 프로필 기반 차량 설정 개인화 시스템

본 프로젝트는 **Infineon AURIX** 환경에서 RFID 기술과 데이터 플래시(DFLASH) 관리를 결합하여, 운전자별 맞춤형 차량 환경을 자동으로 구성하고 비상시 안전 시나리오를 수행하는 임베디드 제어 시스템입니다.

## 1. 프로젝트 개요
차량 공유 및 다인 운전자 환경에서 **사용자 인식 → 개인 설정 복원 → 주행 종료 후 자동 저장**으로 이어지는 사용자 경험을 구현하고, 화재나 사고 등 비상 상황 발생 시 탑승자의 탈출을 돕는 **안전 시나리오**를 제공합니다.

---

## 2. 주요 기능 (Core Features)

### 2.1. 사용자 인식 및 도어 제어
* **RFID 인증**: 사전에 등록된 RFID 카드를 태그하여 사용자를 식별합니다. (RC522 모듈 활용)
* **자동 잠금 해제**: 인증 성공 시 차량 도어(Servo Motor)가 자동으로 열리며 시스템이 활성화됩니다.
* **유연한 프로필 관리**: 새로운 RFID 카드를 언제든지 등록하여 사용자 프로필을 즉시 추가할 수 있습니다.

### 2.2. DFLASH 기반 프로필 저장 및 호출
* **영구 저장 (FEE)**: 각 RFID 고유 ID에 매핑된 사용자 프로필(시트 포지션, 미러 각도, 에어컨 설정 등)을 DFLASH에 저장합니다.
* **자동 동기화**: 주행 중 사용자가 버튼(Jog)으로 변경한 설정값은 하차 시(Shutdown 상태) DFLASH에 자동으로 업데이트됩니다.
* **스마트 복원**: 다음 탑승 시 해당 사용자의 카드를 태그하면, DFLASH에서 데이터를 불러와 하드웨어를 직전 상태로 즉시 세팅합니다.

### 2.3. 비상 상황 대응 (Safety Scenario)
* **상황 감지**: 화재(DS18B20 온도 센서)나 충돌 등 비상 상황을 실시간으로 감지합니다.
* **비상 프로필 전환**: 비상 상황 발생 시 시스템 상태가 즉시 `EMERGENCY`로 강제 전이됩니다.
* **탈출 환경 조성**: 저장된 비상 전용 프로필을 호출하여 **도어를 전면 개방**하고, 시트 및 미러를 탈출에 가장 용이한 위치로 강제 이동시켜 탑승자의 골든타임을 확보합니다.

---

## 3. 시스템 아키텍처 및 역할 분담

### AURIX TC275 - Access & Body
* **RFID 인증**: RFID 태그의 고유 키값을 읽어 등록된 사용자인지 확인하고, 등록된 사용자면 저장된 프로필을 불러옴.
* **모터 제어**: DC 엔코더 모터로 사이드미러 각도 조절과 시트 포지션 조정을 수행, 서보 모터로 도어 개폐를 수행, 네 개의 택트 스위치로 모터를 작동시킬 수 있음, 모터의 이동 정도는 프로필에 저장

### AURIX TC375 - Safety & State
* **상태 관리**: 전체 시스템의 State machine을 구현, 다른 ECU로부터 수집한 정보를 처리해 현재의 state를 산출, 산출한 state를 다른 ECU로 전송, 프로필 정보를 플래시 메모리에 저장
* **위급 상황 감지**: 온도 센서를 이용해 화재 감지시 비상 프로필로 전환

### AURIX TC375 - HVAC & HMI
* **쾌적한 차내 환경 제공**: 공조기와 앰비언트 라이트, 조작계 제공
* **공조기**: 설정 온도와 현재 온도를 비교하여 자동으로 동작
* **앰비언트 라이트**: 다양한 모드와 색상 제공
* **직관적 인터페이스**: LCD와 조이스틱을 사용해 공조기와 앰비언트 라이트 설정 변경, 프로필 전환 가능
---

## 4. 시스템 상태 전이 (State Machine)

1. **SLEEP**: 저전력 대기 상태, 사용자 RFID 태그 대기.
2. **SETUP**: 인증 성공 후 DFLASH에서 프로필을 로드하여 하드웨어(모터 등)를 구동하는 상태.
3. **ACTIVATED**: 모든 설정 완료 및 주행 상태, 사용자의 실시간 설정 변경을 모니터링.
4. **SHUTDOWN**: 주행 종료 시 현재 상태를 DFLASH에 최종 기록 후 SLEEP으로 복귀.
5. **EMERGENCY**: 고온/충돌 감지 시 즉시 전이, 도어 개방 등 최우선 안전 시퀀스 수행.
6. **DENIED**: 인증 거부 상태. 일정 시간 후 슬립 복귀.

---

## 5. ECU 상세 명세: AURIX TC275 (Access & Body)

Access & Body ECU는 사용자와의 접점(인터페이스)을 관리하며, 차량 도어 및 시트/미러 액추에이터의 물리적 제어를 담당합니다. 소프트웨어는 크게 **AppSW(Application)**와 **BaseSW(Base)** 계층으로 구분됩니다.

### 5.1. Application Software Layer (AppSW)

#### RFID Manager (`App_Manager_Rfid.c/h`)
사용자 인증 시퀀스와 보안 정책(Lockout)을 관리하는 상태 머신 기반 모듈입니다.

* **주요 열거형(Enum) 및 구조체(Struct)**
    * `App_Manager_Rfid_State_t`: RFID 내부 상태 (`IDLE`, `POLLING`, `VERIFY_UID`, `FEEDBACK_SUCCESS/FAIL`, `WAIT_TAG_REMOVED`, `LOCKOUT`)
    * `App_Manager_Rfid_Event_t`: 외부 출력 이벤트 (`NONE`, `SUCCESS`, `FAIL`, `LOCKOUT`)
    * `App_Manager_Rfid_Input_t`: `{ boolean register_flag, const Shared_Profile_Table_t *profile_table }`
    * `App_Manager_Rfid_Output_t`: `{ App_Manager_Rfid_Event_t event, boolean uid_valid, uint8 uid_idx, Mfrc522_Uid uid }`

| 함수명 | 상세 내용 | 입력 파라미터 | 반환값 |
| :--- | :--- | :--- | :--- |
| `App_Manager_Rfid_Init` | 내부 DB, 컨텍스트 및 드라이버 초기화 | `uint32 now_ms` | `void` |
| `App_Manager_Rfid_Run` | 카드 감지 및 UID 검증 로직 실행 | `uint32 now_ms, const App_Manager_Rfid_Input_t *input, App_Manager_Rfid_Output_t *out` | `void` |

#### Door Actuator Manager (`App_Manger_DoorActuator.c/h`)
서보 모터를 이용하여 차량 도어의 물리적 개폐 상태를 추상화하여 제어합니다.

* **주요 열거형(Enum) 및 구조체(Struct)**
    * `DoorState_t`: 도어 상태 (`CLOSED`, `OPEN`, `PARTIAL`)
    * `DoorActuator_t`: `{ ServoInstance_t *servo, sint16 closeAngleDeg, sint16 openAngleDeg, DoorState_t state }`
    * `DoorActuatorConfig_t`: `{ ServoInstance_t *servo, sint16 closeAngleDeg, sint16 openAngleDeg, sint16 initAngleDeg }`

| 함수명 | 상세 내용 | 입력 파라미터 | 반환값 |
| :--- | :--- | :--- | :--- |
| `DoorActuator_Init` | 도어 인스턴스 및 초기 각도 설정 | `DoorActuator_t *d, const DoorActuatorConfig_t *cfg` | `void` |
| `DoorActuator_Open` | 설정된 Open 각도로 서보 구동 | `DoorActuator_t *d` | `void` |
| `DoorActuator_Close` | 설정된 Close 각도로 서보 구동 | `DoorActuator_t *d` | `void` |
| `DoorActuator_SetAngle` | 특정 각도 제어 및 상태 업데이트 | `DoorActuator_t *d, sint16 angleDeg` | `void` |

#### Position Axis Manager (`App_Manger_PositionAxis.c/h`)
인코더 모터를 이용한 시트 및 사이드 미러의 정밀 위치 복원 및 수동 조그를 관리합니다.

* **주요 열거형(Enum) 및 구조체(Struct)**
    * `PositionAxisMode_t`: 축 동작 모드 (`IDLE`, `RESTORE_TO_ZERO`, `RESTORE_TO_TARGET`, `JOG_POSITIVE/NEGATIVE`, `ERROR`)
    * `PositionAxisResult_t`: 동작 결과 (`NONE`, `DONE`, `TIMEOUT`, `WRONG_DIR`, `CANCELED`, `REJECTED`)
    * `PositionAxis_t`: `{ MotorInstance_t *motor, sint32 min/maxTick, PositionAxisMode_t mode, ... }`

| 함수명 | 상세 내용 | 입력 파라미터 | 반환값 |
| :--- | :--- | :--- | :--- |
| `PositionAxis_Init` | 모터 핸들 및 제어 파라미터 초기화 | `PositionAxis_t *a, const PositionAxisConfig_t *cfg` | `void` |
| `PositionAxis_Task1ms` | 1ms 주기 모터 제어 및 FSM 처리 | `PositionAxis_t *a` | `void` |
| `PositionAxis_StartRestore` | 0점 경유 후 목표 위치 복원 시작 | `PositionAxis_t *a, sint32 targetTick` | `boolean` |
| `PositionAxis_Stop` | 모든 모터 동작 즉시 중단 | `PositionAxis_t *a` | `void` |

---

### 5.2. Base Software Layer (BaseSW)

#### QSPI Driver (`Base_Com_QSPI1.c/h`)
RFID 모듈과의 SPI 통신을 담당하는 저수준 드라이버입니다.

| 함수명 | 상세 내용 | 입력 파라미터 | 반환값 |
| :--- | :--- | :--- | :--- |
| `Base_Com_Qspi1_Init` | QSPI1 모듈 및 채널(100kbps) 초기화 | `void` | `void` |
| `Base_Com_Qspi1_Transfer` | 멀티 바이트 데이터 동기 송수신 | `const uint8 *tx, uint8 *rx, uint32 len` | `void` |

#### MFRC522 Driver (`Base_Driver_Mfrc522.c/h`)
RFID 리더 칩의 레지스터 제어 및 카드 통신 프로토콜을 수행합니다.

| 함수명 | 상세 내용 | 입력 파라미터 | 반환값 |
| :--- | :--- | :--- | :--- |
| `Base_Driver_Mfrc522_Init` | 칩 소프트 리셋 및 안테나 활성화 | `void` | `boolean` |
| `Base_Driver_Mfrc522_ReadUid` | 카드 감지 및 UID(2바이트) 획득 | `Mfrc522_Uid *outUid` | `Mfrc522_Status` |

#### Encoder Motor Driver (`Base_EncoderMotor.c/h`)
GTM PWM을 이용한 속도 제어 및 GPIO 폴링 기반 인코더 카운팅을 수행합니다.

| 함수명 | 상세 내용 | 입력 파라미터 | 반환값 |
| :--- | :--- | :--- | :--- |
| `Motor_Init` | PWM 및 인코더/브레이크 핀 초기화 | `MotorInstance_t *m, const MotorConfig_t *cfg` | `void` |
| `Motor_MoveStart` | 정밀 보정 기능을 포함한 상대 이동 시작 | `MotorInstance_t *m, sint32 ticks, uint16 duty, uint32 timeoutMs, sint32 toleranceTicks` | `boolean` |

#### System Timer Driver (`Driver_Stm.c/h`)
1ms 인터럽트를 통해 멀티레이트 스케줄링 플래그를 생성합니다.

| 함수명 | 상세 내용 | 입력 파라미터 | 반환값 |
| :--- | :--- | :--- | :--- |
| `Driver_Stm_Init` | 1ms 주기 STM 인터럽트 활성화 | `void` | `void` |
| `STM_Int0Handler` | 1/10/100/1000ms 주기 플래그 생성 | `void` | `void (ISR)` |

#### MultiCAN+ FD Driver (`MULTICAN_FD.c/h`)
64바이트 롱 프레임을 지원하는 CAN FD 통신 드라이버입니다.

| 함수명 | 상세 내용 | 입력 파라미터 | 반환값 |
| :--- | :--- | :--- | :--- |
| `initMultican` | CAN FD 노드 및 메시지 오브젝트 초기화 | `void` | `void` |
| `transmitCanMessage` | 64바이트 데이터 전송 (Busy-wait) | `uint32 txId, uint32 *pData` | `void` |

---

### 5.3. System Entry Point (`App.c`)
각 주기별 태스크를 실제 호출하고 전체 시퀀스를 총괄합니다.

* **스케줄링 구성**:
    * `AppTask1ms`: 인코더 추적 및 버튼 조그 제어 (고속).
    * `AppTask10ms`: RFID 인증 로직 및 상태 전이 관리 (중속).
    * `AppTask100ms`: CAN 통신 처리 및 상태 LED 관리 (저속).

**Development Date**: 2026. 03. 25.  ~ 2026. 04. 07.

**Confluence Link**: https://autoever-fibonacci.atlassian.net/wiki/external/OWY4MjM0MzE0ODU2NGYyMDljNmMyYjdlOWQ1YzY3MWU
