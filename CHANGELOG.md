# Changelog
작업하면서 변경점은 모두 이 기록에 저장합니다.
할수있으면 영어로도 추가내용을 적기

이 체인지 로그 포맷은 [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)를 기반으로 작성됨,

## [Unreleased]
### Added
- 독물의 추출, 증산 추가
  - Added Fulsome Distillation, Evaporate spell
- 독물의 추출을 위한 나쁜 포션(독, 맹독, 맹물)추가. 추출에서만 나오는 포션이며 자연 드랍되면 버그임
  - Added bad potions (poison, strong poison, water) for Evaporate. only create by Fulsome Distillation
- 가속 마법 복구
  - rollback Haste
- 새로운 메인 타이틀 이미지 추가 (logal-image)
  - Add new title artwork (logal-image)

### Changed
- 오우거 둔기적성 +3으로 변경 (기존 -1)
  - Ogre Maces & Flails aptitude changed to +3 (prev -1)

- 드라코들은 이제 냉혈 변이를 가지지않음
  - All dracos are no longer cold blooded

- 회색을 제외한 드라코 전원 14레벨에 영구 비행 변이를 얻도록 변경
  - All dracos will gain big wing mutation at xl 14 except grey

- 메뉴 컬러링을 위한 태그(fixed_artefact, random_artefact) 추가. 픽다트의 기본 색깔을 노란색으로 변경
  - Added Menu/colouring Prefixes(fixed_artefact, random_artefact). Changed the default color of the fixed artefact to yellow

### Removed

### 0.25 Featrue
- 갑옷 장착시 AC를 보여주는 인터페이스 추가
  - Improve armour AC change descriptions
- 독 중첩단계에 따른 타일 이펙트 추가
  - can see poison level mons and player


[Unreleased]: https://github.com/kimjoy2002/crawl/compare/0.24.1...HEAD
