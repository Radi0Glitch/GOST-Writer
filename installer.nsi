; GOST-Writer NSIS Installer Script
Unicode True
!include "LogicLib.nsh"
!include "MUI2.nsh"

Name "GOST-Writer"
OutFile "..\GOST-Writer-Setup-x86_64.exe"
Icon "gw-logo.ico"
UninstallIcon "gw-logo.ico"
InstallDir "$PROGRAMFILES64\GOST-Writer"
InstallDirRegKey HKLM "Software\GOST-Writer" "Install_Dir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\gost-writer.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "English"

Section "GOST-Writer" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"

  File "gost-writer.exe"
  File "SDL3.dll"
  File "SDL3_ttf.dll"
  File "README.md"

  SetOutPath "$INSTDIR\fonts"
  File "fonts\*.*"

  SetOutPath "$INSTDIR\packaging\icons"
  File /nonfatal "packaging\icons\gw-logo-64.bmp"

  SetOutPath "$INSTDIR\templates\gost"
  File /nonfatal "templates\gost\reference-a4.docx"

  SetOutPath "$INSTDIR\tools\pandoc\windows-x86_64"
  File /nonfatal "tools\pandoc\windows-x86_64\pandoc.exe"

  SetOutPath "$INSTDIR\tools\typst\windows-x86_64"
  File /nonfatal "tools\typst\windows-x86_64\typst.exe"

  SetOutPath "$INSTDIR"
  WriteRegStr HKLM "Software\GOST-Writer" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GOST-Writer" "DisplayName" "GOST-Writer"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GOST-Writer" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GOST-Writer" "DisplayIcon" '"$INSTDIR\gost-writer.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GOST-Writer" "Publisher" "GOST-Writer"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GOST-Writer" "DisplayVersion" "1.0.0"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GOST-Writer" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GOST-Writer" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\GOST-Writer"
  CreateShortcut "$SMPROGRAMS\GOST-Writer\GOST-Writer.lnk" "$INSTDIR\gost-writer.exe" "" "$INSTDIR\gost-writer.exe" 0
  CreateShortcut "$SMPROGRAMS\GOST-Writer\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortcut "$DESKTOP\GOST-Writer.lnk" "$INSTDIR\gost-writer.exe" "" "$INSTDIR\gost-writer.exe" 0

  ; --- File associations for .md / .markdown ---
  WriteRegStr HKLM "Software\Classes\GOSTWriter.Markdown" "" "Документ ГОСТ (GOST-Writer)"
  WriteRegStr HKLM "Software\Classes\GOSTWriter.Markdown\DefaultIcon" "" '"$INSTDIR\gost-writer.exe",0'
  WriteRegStr HKLM "Software\Classes\GOSTWriter.Markdown\shell\open\command" "" '"$INSTDIR\gost-writer.exe" "%1"'
  WriteRegStr HKLM "Software\Classes\.md\OpenWithProgids" "GOSTWriter.Markdown" ""
  WriteRegStr HKLM "Software\Classes\.markdown\OpenWithProgids" "GOSTWriter.Markdown" ""
  ReadRegStr $0 HKLM "Software\Classes\.md" ""
  ${If} $0 == "" 
  ${OrIf} $0 == "GOSTWriter.Markdown"
    WriteRegStr HKLM "Software\Classes\.md" "" "GOSTWriter.Markdown"
  ${EndIf}
  ReadRegStr $0 HKLM "Software\Classes\.markdown" ""
  ${If} $0 == ""
  ${OrIf} $0 == "GOSTWriter.Markdown"
    WriteRegStr HKLM "Software\Classes\.markdown" "" "GOSTWriter.Markdown"
  ${EndIf}
  WriteRegStr HKLM "Software\GOST-Writer" "md_owner" "1"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

Section "Uninstall"
  ReadRegStr $0 HKLM "Software\Classes\.md" ""
  ${If} $0 == "GOSTWriter.Markdown"
    DeleteRegValue HKLM "Software\Classes\.md" ""
  ${EndIf}
  ReadRegStr $0 HKLM "Software\Classes\.markdown" ""
  ${If} $0 == "GOSTWriter.Markdown"
    DeleteRegValue HKLM "Software\Classes\.markdown" ""
  ${EndIf}
  DeleteRegValue HKLM "Software\Classes\.md\OpenWithProgids" "GOSTWriter.Markdown"
  DeleteRegValue HKLM "Software\Classes\.markdown\OpenWithProgids" "GOSTWriter.Markdown"
  DeleteRegKey HKLM "Software\Classes\GOSTWriter.Markdown"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'

  Delete "$INSTDIR\gost-writer.exe"
  Delete "$INSTDIR\SDL3.dll"
  Delete "$INSTDIR\SDL3_ttf.dll"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\uninstall.exe"

  RMDir /r "$INSTDIR\fonts"
  RMDir /r "$INSTDIR\packaging"
  RMDir /r "$INSTDIR\templates"
  RMDir /r "$INSTDIR\tools"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\GOST-Writer\GOST-Writer.lnk"
  Delete "$SMPROGRAMS\GOST-Writer\Uninstall.lnk"
  RMDir "$SMPROGRAMS\GOST-Writer"
  Delete "$DESKTOP\GOST-Writer.lnk"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\GOST-Writer"
  DeleteRegKey HKLM "Software\GOST-Writer"
SectionEnd
