# ESP-Reader MVP Spec

## 목적

이 문서는 `ESP-Reader`의 1차 MVP를 정의한다.

이번 버전의 목표는 `Markdown 문서를 내부 플래시에서 열고`, `읽기 좋은 페이지 단위로 표시하며`, `슬립/웨이크 이후에도 읽던 흐름을 복원`하는 것이다.

## 제품 정의

- 기기 성격: `읽기 중심 Markdown 문서 뷰어`
- 주요 사용자 가치: `편안한 읽기`, `빠른 재진입`, `낮은 인지 부하`
- 저장소: `내부 플래시`
- 포맷: `Markdown only`

## 반드시 포함할 기능

- 내부 플래시에서 `.md` 파일 목록 표시
- 문서 선택 후 읽기 화면 진입
- 제목/문단/리스트 중심 기본 Markdown 렌더링
- 이전/다음 페이지 이동
- 현재 읽기 위치 저장
- 유휴 시간 초과 시 슬립
- 웨이크 후 마지막 읽기 상태 복원

## 이번 버전에서 제외

- `epub`, `pdf`, `html`
- 네트워크 동기화
- 복수 북마크
- 검색
- 주석, 하이라이트
- 완전한 LaTeX 수식 렌더링
- 이미지 표시

## 사용자 플로우

1. 부팅
2. 내부 플래시 마운트
3. `.md` 목록 표시
4. 문서 선택
5. 페이지 렌더링
6. 페이지 이동
7. 위치 저장
8. 슬립
9. 웨이크 후 복원

## UX 원칙

- 한 화면에 너무 많은 정보를 넣지 않는다.
- 페이지 넘김은 명확하고 예측 가능해야 한다.
- 진행 상태는 작게 표시하되 본문을 방해하지 않는다.
- 설정 변경보다 기본 레이아웃 품질을 우선한다.
- 잔상 최적화보다 표시 안정성을 우선한다.

## 상위 상태

- `Boot`
- `Library`
- `OpenDocument`
- `Reading`
- `PrepareSleep`
- `Sleeping`
- `Error`

## 핵심 데이터

```cpp
struct ReadingSession {
    std::string last_document_path;
    uint32_t last_page_index;
    uint32_t last_block_index;
    uint32_t last_char_offset;
    bool was_reading;
};
```

```cpp
struct DocumentEntry {
    std::string path;
    std::string title;
    size_t file_size;
};
```

## 완료 기준

- `.md` 문서 하나 이상을 처음부터 끝까지 읽을 수 있다.
- 재부팅 또는 웨이크 후 읽던 문서와 위치가 복원된다.
- 목록, 읽기, 슬립 복귀 플로우가 깨지지 않는다.
- 치명적 오류 없이 데모 가능한 수준의 안정성이 확보된다.
