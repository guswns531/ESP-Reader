# ESP-Reader Format Policy

## 목표

ESP-Reader는 `Markdown only`를 원칙으로 하되, 모든 Markdown을 즉시 지원하지 않는다.

핵심 기준은 아래와 같다.

- 읽기 경험을 해치지 않는 범위에서 확장
- 긴 문서에서 의미가 큰 구조를 우선 지원
- 수식은 단계적으로 품질을 높임

## v0.1 지원 범위

- plain paragraph
- heading 1-3
- unordered list
- ordered list
- blank line separation

렌더링 원칙:

- 문서 구조가 구분되어 보이면 된다.
- HTML 수준의 정밀한 재현은 목표가 아니다.

## v0.2 지원 범위

- emphasis
- strong
- inline code
- fenced code block
- thematic break
- block quote

## 후속 지원 후보

- table
- checkbox list
- link display
- image placeholder
- footnote marker

초기에는 `클릭 가능한 인터랙션`보다 `읽을 수 있는 구조 표현`을 우선한다.

## 수식 정책

### Stage 1

- `$...$`, `$$...$$` 구문을 인식한다.
- 렌더링이 안 되더라도 수식 경계를 잃지 않게 표시한다.

### Stage 2

- 자주 쓰는 inline math를 읽기 가능한 수준으로 단순 표현한다.
- block math는 별도 블록으로 배치한다.

### Stage 3

- 전처리 또는 온디바이스 렌더링 중 하나를 선택해 고도화한다.

## 한글과 줄바꿈

읽기 편함이 중요하므로 아래를 우선한다.

- 한글/영문 혼합 문단에서 지나치게 어색한 줄바꿈을 줄인다.
- 코드 블록과 일반 문단의 줄바꿈 정책을 분리한다.
- 제목, 문단, 리스트의 간격을 다르게 둔다.

## 의도적으로 미루는 것

- 완전한 CommonMark 호환
- HTML embedded block
- CSS 스타일 재현
- 복잡한 표 레이아웃
- 고품질 TeX 수식 전체 구현
