# ESP-Reader Architecture

## 시스템 목표

아키텍처 목표는 아래 3가지다.

- 읽기 경험을 우선하는 단순한 구조
- 상태 복원과 디바이스 제어의 명확한 분리
- Markdown 확장과 수식 전략 변경에 버틸 수 있는 경계 설정

## 레이어

### 1. Platform Layer

- e-paper panel driver
- GPIO / ADC input
- flash filesystem
- NVS session storage
- sleep / wake integration

역할:

- 하드웨어 직접 제어
- ESP-IDF API 캡슐화

### 2. Core Layer

- app state machine
- event queue
- document manager
- session manager
- layout engine

역할:

- 앱 흐름 제어
- 문서 상태와 읽기 상태 유지
- 렌더링에 필요한 중간 데이터 생성

### 3. Presentation Layer

- library screen
- reading screen
- error screen
- settings screen

역할:

- 화면 레이아웃 정의
- 본문과 상태 정보 배치

## 주요 모듈

### AppController

- 전역 상태머신 소유
- 상태 전이 처리
- 이벤트 분배

### InputManager

- ADC 키값 샘플링
- 디바운스
- 버튼 이벤트 변환

### StorageManager

- 내부 플래시 파일 시스템 마운트
- 문서 목록 스캔
- 파일 읽기

### SessionManager

- 마지막 문서와 위치 저장
- 부팅 후 세션 복원

### DocumentParser

- Markdown을 내부 블록 구조로 변환
- 초기 버전은 최소 subset만 지원

### LayoutEngine

- 블록을 페이지 단위 레이아웃으로 변환
- 줄바꿈, 여백, 문단 간격 처리

### RenderEngine

- 페이지 모델을 framebuffer에 그림
- 제목, 본문, 하단 상태 표시

### PowerManager

- 유휴 시간 추적
- 슬립 요청
- 웨이크 후 복귀 시점 연결

## 데이터 흐름

1. `InputManager`가 버튼 이벤트를 생성한다.
2. `AppController`가 상태와 이벤트를 보고 액션을 결정한다.
3. `StorageManager`가 문서를 읽는다.
4. `DocumentParser`가 문서를 블록 단위로 파싱한다.
5. `LayoutEngine`이 현재 설정 기준으로 페이지를 생성한다.
6. `RenderEngine`이 화면에 표시한다.
7. `SessionManager`가 읽기 위치를 저장한다.

## 렌더링 모델

초기 렌더링 경로는 아래처럼 단순하게 유지한다.

- raw markdown
- parsed blocks
- laid out page
- rendered framebuffer

이 구조를 지키면 나중에 수식 블록이나 코드 블록이 추가되어도 `parser -> layout -> render` 순서를 유지할 수 있다.

## 수식 대응 전략

수식은 초기에 별도 레이어가 아니라 `special block / inline token`으로 취급한다.

이유:

- 수식만 별도 시스템으로 분리하면 초기 구조가 과도하게 커진다.
- 나중에 온디바이스 렌더링 또는 전처리 방식을 선택할 여지를 남길 수 있다.

## 설계 원칙

- 상태 전이와 그리기 코드를 직접 섞지 않는다.
- 파일 시스템 접근과 페이지 계산을 분리한다.
- 문서 파싱 결과를 재사용 가능하게 만든다.
- 문서 표현 범위를 늘려도 읽기 UX 설정 구조는 유지한다.
