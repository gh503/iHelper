; installer.nsi
!include MUI2.nsh

!define MUI_ABORTWARNING
!define MUI_ICON "installer.ico"
!define MUI_UNICON "uninstaller.ico"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "license.rtf"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

Section "Main"
    SetOutPath $INSTDIR
    File /r "..\..\..\..\targets\${INSTALL_DIR_NAME}\*"
SectionEnd