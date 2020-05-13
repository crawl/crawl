# Survey

* 각종 결정 사항들 투표 함 (일주일 정도 투표하고 결정함)
  1. 추출,증산 아이콘 결정하기(4.7) - 투표 완료
  2. 구덩이 함정을 어캐 할까? (4.12) - 투표 완료 (불합리함만 제거-14표(70%), 현상유지-6표(30%), 아예 삭제-0표(0%))
  3. 새로운 스토커 마법 뭘 살릴까? (4.26) - 투표 완료 (1번-10표(55.6%), 2번-6표(33.3%), 3번-2표(11.1%))
  4. 네멜렉스 조베 수정 (4.28) - 투표완료 (1.제물신앙-11표(55%) 2.발동술-18표(90%) 3.어빌창-13표(72%)
     - 카드 순위 1위 xom  2위 Mercenary,alchemist. 3위 Bargain,Sage,Portal,Trowel,Experience,Helm  -> 님들 좀은 징벌카드야...
# Design goals

* 삭제보단 컨텐츠 추가를 중심으로 개발을 할 예정임
  * 컨텐츠 추가에 대해서는 밸런스적인 문제가 보여도 우선 추가를 목표로 함 (선추가 후밸런싱)

* 현재 시스템을 유지하는 선에서 옛날 컨텐츠 복구도 주요 목표 중 하나임

* 일부 개선이나 추가를 하다보면 찬반이 갈릴 수 있는데 이 경우 투표를 진행할 예정임
  * 일주일정도 투표나온 결과로 진행함
  
* 나머지는 부담없이 자유롭게 진행하자

# Dungeon Crawl Stone Soup Korea Fork

[Dungeon Crawl Stone Soup](https://github.com/crawl/crawl/)의 포크

돌죽 0.24버전을 베이스로 시작함. 목표는 위에 디자인 골을 참고

누구나 참여가능한 돌죽 포크를 지향함

* 기여 방법
  1. Pull requests를 통한 기여
     * 이 브랜치를 Fork를 따서 작업후 Pull requests 요청하기
     * 빌드가 실패하는게 아니면 왠만해선 허용합니다.
     
     
  2. 직접 참가하기
     * pull request가 아니라도 어느정도 코딩할줄아는 수준이시면 레포지터리 권한 다 드림
     * 이렇게 권한을 받으면 직접 push해도 상관없음
     
    
  3. 기획 혹은 버그 신고
     * 코딩을 할 줄 몰라도 위에 Issues 메뉴에서 아이디어 제공, 혹은 버그 제보로 참여가능
     

# How to Build
  * 원본영어 판은 [링크](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/INSTALL.txt)를 참조 
  * 한글 해석 [링크](https://gall.dcinside.com/board/view/?id=rlike&no=261405)
    * [링크](https://github.com/kimjoy2002/crawl/issues/18) <- 빌드시 문제 발생하면 참조하기

# Develop Guide
  * 직업 만들기 [영어원문](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/docs/develop/background_creation.txt) [한글번역](https://gall.dcinside.com/board/view/?id=rlike&no=96789)
  * 신 만들기 [영어원문](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/docs/develop/god_creation.txt) [한글번역](https://github.com/kimjoy2002/crawl/issues/116)
  * 몬스터만들기 [영어원문](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/docs/develop/monster_creation.txt)
  * 변이 만들기 [영어원문](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/docs/develop/mutation_creation.txt)
  * 종족 만들기 [영어원문](https://github.com/kimjoy2002/crawl/blob/master/crawl-ref/docs/develop/species_creation.md)
  * 마법 만들기 [한글원문](https://gall.dcinside.com/board/view/?id=rlike&no=318987)

# ChangeLog
  
  [Link](https://github.com/kimjoy2002/crawl/blob/master/CHANGELOG.md)
  
# 웹타일

* 웹타일주소
  *  http://joy1999.codns.com:8081/
  * 3시간마다 자동 업데이트함. push버전을 바로바로 테스트하고싶으면 이걸 이용
  * 플레이용도보단 테스트용도기때문에 위자드모드 가능한 버전도 열어뒀음 
  * 참고로 강제로 패치가 되는데 가끔 패치후 세이브파일이 깨지는 현상이 있음 (무슨 값을 고치면 세이브파일이 깨지는지 아직 잘 모름...)
    * 때문에 실제로 플레이용으로 쓰기엔 좀 위험함. 로그인후 접속시 검은 화면에서 진행이 안되면 세이브파일이 깨진거임
    * 이 경우 새 아이디를 만들어야 함
  * 주의) 기본적으로 서버용 컴퓨터가 아니라서 느림. 플레이용이 아닌 테스트용도로만 사용하세요

# 다운로드

* [![Build Status](http://joy1999.codns.com:8080/buildStatus/icon?job=crawl%2Fcrawl)](http://joy1999.codns.com:8080/job/crawl/job/crawl/)

* 타일판 다운로드
  * http://joy1999.codns.com:8999/download/stone_soup-0.24.1-tiles-win64.zip - 64비트
  * http://joy1999.codns.com:8999/download/stone_soup-0.24.1-tiles-win32.zip - 32비트
  * 매일 밤 새벽3시 자동 업데이트

* 콘솔판 다운로드
  * http://joy1999.codns.com:8999/download/stone_soup-0.24.1-win64.zip - 64비트
  * http://joy1999.codns.com:8999/download/stone_soup-0.24.1-win32.zip - 32비트
  * 매일 밤 새벽3시 자동 업데이트
  
* 이전 다운로드 파일들
  * http://joy1999.codns.com:8999/download/crawl/
  * 매일 자동빌드된 파일들이 올라감 너무 오래되면 삭제
