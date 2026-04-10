# ESP-Reader

ESP-Reader는 내부 플래시 기반의 Markdown 문서 뷰어입니다.

- 현재 버전은 SPIFFS에서 `.md` 파일을 읽습니다.
- ADC 5-key 입력으로 목록과 페이지를 이동합니다.
- 읽기 경험 중심 구조를 먼저 만들고 있습니다.

## 현재 상태

이 문서는 실제 내부 플래시에서 로드된 샘플 문서입니다.

```cpp
// later:
// mount storage
// parse markdown
// restore session
```

다음 단계는 세션 복원과 더 나은 레이아웃 엔진입니다.
