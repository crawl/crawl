# please run with -DVERSION="xxx"
# !define VERSION "0.7.0"
!define DCSS "Dungeon Crawl Stone Soup"
SetCompressor /SOLID lzma

!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_KEY "Software\Crawl"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_VALUENAME ""
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_KEY "Software\Crawl"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_VALUENAME ""
!define MULTIUSER_INSTALLMODE_INSTDIR "Crawl"
!include "MultiUser.nsh"
!include "MUI2.nsh"

!ifndef WINARCH
  !define WINARCH "win32"
!endif

Name "${DCSS} ${VERSION}"
Outfile "stone_soup-${VERSION}-${WINARCH}-installer.exe"
XPStyle on
!define MUI_ICON util\crawl.ico

!define MUI_ABORTWARNING

!insertmacro MULTIUSER_PAGE_INSTALLMODE
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section ""
  SetOutPath $INSTDIR
  File /r /x "*~" "build-win\*"
  File /oname=LICENSE.txt ..\..\LICENSE

  # clean after previous versions; crawl-console.exe used to be named crawl.exe
  # but that made sense on Unix but not really on Windows.
  Delete crawl.exe
  # clean after 0.10 or earlier, let's produce error messages instead of
  # incorrect behaviour
  Delete dat\lua\*.lua

  WriteUninstaller $INSTDIR\uninst.exe

  WriteRegStr SHCTX "Software\Crawl" "" $INSTDIR

  CreateDirectory "$SMPROGRAMS\${DCSS}"
  # the order matters -- only the first shortcut is advertised by Win 7
  CreateShortCut "$SMPROGRAMS\${DCSS}\Dungeon Crawl - tiles.lnk" "$INSTDIR\crawl-tiles.exe"
  CreateShortCut "$SMPROGRAMS\${DCSS}\Dungeon Crawl - console.lnk" "$INSTDIR\crawl-console.exe"
  CreateShortCut "$SMPROGRAMS\${DCSS}\Uninstall DCSS.lnk" "$INSTDIR\uninst.exe"

  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "DisplayName" "${DCSS}"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "DisplayVersion" "${VERSION}"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "DisplayIcon" "$INSTDIR\crawl-tiles.exe"
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "NoModify" 1
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl" "NoRepair" 1
SectionEnd

Function .onInit
  !insertmacro MULTIUSER_INIT
FunctionEnd

Section "Uninstall"
  Delete $INSTDIR\uninst.exe
  Delete $INSTDIR\crawl-tiles.exe
  Delete $INSTDIR\crawl-console.exe
  Delete $INSTDIR\version.txt
  Delete $INSTDIR\CREDITS.txt
  Delete $INSTDIR\LICENCE.txt
  RMDir /r $INSTDIR\dat
  RMDir /r $INSTDIR\docs
  RMDir /r $INSTDIR\settings
  # Crafty players may try sticking their saves here, so we remove things from
  # the top dir by hand.
  RMDir $INSTDIR

  Delete "$SMPROGRAMS\${DCSS}\Uninstall DCSS.lnk"
  Delete "$SMPROGRAMS\${DCSS}\Dungeon Crawl - console.lnk"
  Delete "$SMPROGRAMS\${DCSS}\Dungeon Crawl - tiles.lnk"
  RMDir "$SMPROGRAMS\${DCSS}"

  DeleteRegKey SHCTX "Software\Crawl"
  DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\Crawl"
SectionEnd

Function un.onInit
  !insertmacro MULTIUSER_UNINIT
FunctionEnd
