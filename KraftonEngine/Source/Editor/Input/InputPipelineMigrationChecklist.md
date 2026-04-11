# Input Pipeline Migration Checklist (Week06)

Last Updated: 2026-04-11

## 1) Core Routing / Binding

- [x] `InteractionBinding` 도입 (`ReceiverVC`, `TargetWorld`, `Domain`)
- [x] `InputRouter` 기반 단일 VC 라우팅
- [x] 캡처 우선 라우팅(드래그 중 동일 VC 유지)
- [x] 글로벌 단축키(ESC/F8) 우선 처리 경로

## 2) VC 내부 Dispatcher / Context

- [x] VC 내부 컨텍스트 우선순위 소모 체인(First-consume)
- [x] `Command / Gizmo / Selection / Navigation` 분리
- [x] 기존 InputChord 매핑 유지/재사용
- [ ] 컨텍스트별 상세 consume 정책 미세 조정

## 3) Tool / Mode / Controller 구조

- [x] `Tool` 인터페이스 계층 추가
- [x] `Mode` 인터페이스 계층 추가
- [x] `Controller` 도입 및 VC 오케스트레이션 분리
- [x] 기본 툴 이관:
  - [x] Command
  - [x] Gizmo
  - [x] Selection
  - [x] Navigation(기본)
- [ ] Navigation 고급 동작(Orbit/Pan/Dolly 세부 UX) Week5 동등화
- [ ] Tool Global/공용 명령 라우팅 확장

## 4) PIE / EditorOnPIE 동작

- [x] PIE 진입 시 플레이어 액터/카메라 생성 및 초기화
- [x] PIE 종료 시 정리/복원 훅(`OnBeginPIE` / `OnEndPIE`)
- [x] F8 토글( Possessed <-> Ejected )
- [x] `EditorVC -> PIEWorld` (EditorOnPIE) 경로 유지
- [x] PIE 시작 기본 모드: `Possessed` (즉시 플레이)
- [ ] PIE 카메라/입력 체감(감도, 캡처 해제 타이밍) 미세 조정

## 5) Selection / Picking / Gizmo 정합성

- [x] PIE 진입/종료 시 Selection world 리바인딩
- [x] 엔트리 뷰포트 Gizmo 표시 상태 저장/복원
- [ ] ID 버퍼 기반 선택/아웃라인 경계 사례 회귀 점검

## 6) 검증 체크

- [x] Debug x64 빌드 통과
- [x] Release x64 빌드 통과
- [ ] 멀티 뷰포트 시나리오 수동 점검
- [ ] PIE 도중 뷰포트 포커스 전환 점검
- [ ] F8 반복 토글 스트레스 점검
- [ ] 드래그 캡처 중 월드/도메인 일관성 점검

## 7) 다음 소형 패치 우선순위

1. Navigation Orbit/Pan/Dolly 세부 바인딩 정리
2. Global Tool Command 최소 세트 이관(프레임 포커스/카메라 북마크 등)
3. PIE 포커스/캡처 UX 튜닝
