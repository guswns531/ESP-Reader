# ESP-Reader

ESP-Reader는 `ESP32-S3 + e-paper` 기반의 `읽기 경험 중심 Markdown 문서 뷰어` 프로젝트다.

핵심 방향:

- 포맷은 `Markdown only`
- 저장소는 `내부 플래시 중심`
- 목표는 `기능 과시`보다 `편안한 읽기`
- 문서 복원, 페이지 이동, 슬립/웨이크 복귀를 중요하게 본다

## Product Direction

ESP-Reader는 범용 전자책 단말기보다 아래 성격에 가깝다.

- 로컬 문서 리더
- 텍스트 위주 장문 읽기 기기
- 개인 문서, 노트, 기술 문서, 수식 포함 Markdown 뷰어

초기 버전은 `Markdown subset`부터 시작하고, 이후 읽기 UX와 수식 표현을 확장한다.

## Priorities

1. 문서가 안정적으로 열린다.
2. 페이지가 읽기 편하게 구성된다.
3. 읽던 위치가 자연스럽게 복원된다.
4. Markdown 표현 범위를 점진적으로 넓힌다.
5. 수식 표현을 단계적으로 고도화한다.

## Documents

- [docs/spec.md](./docs/spec.md): 1차 MVP 기능 스펙
- [docs/roadmap.md](./docs/roadmap.md): 단계별 제품 확장 계획
- [docs/architecture.md](./docs/architecture.md): 시스템 구조와 모듈 경계
- [docs/format.md](./docs/format.md): Markdown 및 수식 지원 정책

## Initial Scope

1차 목표는 아래와 같다.

- 내부 플래시에서 `.md` 파일 목록 표시
- 파일 선택 후 본문 페이지 렌더링
- 다음/이전 페이지 이동
- 마지막 읽은 위치 저장
- 일정 시간 입력 없으면 슬립
- 웨이크 후 마지막 문서와 위치 복원

## Tech Direction

- Framework: `ESP-IDF`
- Language: `C++`
- App Model: `state machine + event-driven`
- Storage: `internal flash + NVS`
- Display Policy: `stable full refresh first`

## Current Skeleton

현재 구현된 골격은 아래까지 포함한다.

- `Library` 화면 표시
- `Reading` 화면 표시
- ADC 5-key 입력 처리
- 내부 플래시 `SPIFFS` 마운트
- `.md` 문서 스캔 및 목록 표시
- UTF-8 기반 Markdown 샘플 문서 로딩
- 한글 UI 폰트 렌더링
- 한글 본문 줄바꿈 기반 페이지 표시
- 문서별 마지막 페이지 저장/복원
- 목록 선택 시 부분 갱신 기반 강조 이동
- 읽기 화면 페이지 이동 시 부분 갱신
- 엔터 버튼 수동 전체 refresh

키 동작:

- `KEY5`: 읽기 화면에서 라이브러리 복귀
- `KEY4`: 목록에서 위 이동 / 읽기 화면에서 이전 페이지
- `KEY3`: 목록에서 아래 이동 / 읽기 화면에서 다음 페이지
- `KEY2`: 목록에서 문서 열기
- `KEY1`: 수동 전체 refresh

## Current UI

현재 화면 원칙은 아래와 같다.

- 제목은 크게, 두 번 겹쳐 그려 `볼드 느낌`으로 표시
- 선택 항목은 `검정 테두리 박스`로만 강조
- 하단 키 설명은 표시하지 않음
- 오른쪽 상단에만 간단한 조작 힌트 표시
- 한글 UI와 본문은 `u8g2` 기반 폰트 경로 사용
- 본문은 UTF-8 기준 줄바꿈으로 표시
- 읽기 화면 우하단에 `현재 페이지 / 전체 페이지` 표시

## Current Markdown Support

현재 기기에서 구분되어 보이는 요소:

- 제목: `#`, `##`, `###`
- 일반 문단
- 리스트: `-`, `*`, `1.`
- 코드 블록: ````` ``` `````
- 수식 블록: `$$...$$`
- 인라인 수식: `$...$`
- 인용 블록: `> `
- 구분선: `---`
- 인라인 코드: `` `code` ``
- 강조: `**bold**`, `*italic*`

현재 렌더 방식은 완전한 Markdown 엔진이 아니라, `읽기 좋은 구조 구분`을 우선한 표현이다.

현재 샘플 문서:

- `welcome.md`
- `math-notes.md`
- `korean-notes.md`

## Build And Flash

빌드:

```sh
. /Users/hyeonjun/Workspace/WeActStudio.ESP32S3-AorB/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py build
```

플래시:

```sh
. /Users/hyeonjun/Workspace/WeActStudio.ESP32S3-AorB/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -p /dev/cu.usbmodem1101 flash
```

모니터:

```sh
. /Users/hyeonjun/Workspace/WeActStudio.ESP32S3-AorB/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -p /dev/cu.usbmodem1101 monitor
```

## Near-Term Decisions

초기 구현 전에 아래를 확정한다.

- 내부 플래시 파일 시스템 종류
- 기본 폰트와 한글 렌더링 전략
- Markdown parser 범위
- 수식 처리 1차 전략
- 페이지 분할 기준
