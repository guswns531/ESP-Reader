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

- [spec.md](./spec.md): 1차 MVP 기능 스펙
- [roadmap.md](./roadmap.md): 단계별 제품 확장 계획
- [architecture.md](./architecture.md): 시스템 구조와 모듈 경계
- [format.md](./format.md): Markdown 및 수식 지원 정책

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

## Near-Term Decisions

초기 구현 전에 아래를 확정한다.

- 내부 플래시 파일 시스템 종류
- 기본 폰트와 한글 렌더링 전략
- Markdown parser 범위
- 수식 처리 1차 전략
- 페이지 분할 기준
