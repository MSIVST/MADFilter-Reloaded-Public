 Microsoft Developer Studio Project File - Name="MADFilter" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=MADFilter - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "MADFilter.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MADFilter.mak" CFG="MADFilter - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MADFilter - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "MADFilter - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "MADFilter - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MADFilter_EXPORTS" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /Gi /GX /O2 /I "./memcopy" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MADFilter_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 mad\libmad.lib strmbase.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib ws2_32.lib /nologo /dll /machine:I386 /out:"Release/MADFilter.ax"

!ELSEIF  "$(CFG)" == "MADFilter - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "MADFilter_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /ZI /I "./memcopy" /D "INC_OLE2" /D "STRICT" /D _WIN32_WINNT=0x0400 /D "WIN32" /D "_WIN32" /D "_MT" /D "_DLL" /D _X86_=1 /D WINVER=0x0400 /D DBG=1 /D "DEBUG" /D "_DEBUG" /D try=__try /D except=__except /D leave=__leave /D finally=__finally /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 mad\libmadd.lib strmbasd.lib msvcrtd.lib quartz.lib vfw32.lib winmm.lib version.lib largeint.lib comctl32.lib olepro32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib /nologo /entry:"DllEntryPoint@@12" /dll /pdb:none /machine:I386 /out:"Debug\MADFilter.ax"
# SUBTRACT LINK32 /debug /nodefaultlib

!ENDIF 

# Begin Target

# Name "MADFilter - Win32 Release"
# Name "MADFilter - Win32 Debug"
# Begin Source File

SOURCE=.\memcopy\aag_memcpy.cpp
# End Source File
# Begin Source File

SOURCE=.\memcopy\aag_memcpy.h
# End Source File
# Begin Source File

SOURCE=.\IMADFilterInterfaceImpl.cpp
# End Source File
# Begin Source File

SOURCE=.\ISpecifyPropertyPagesImpl.cpp
# End Source File
# Begin Source File

SOURCE=.\maddecode.cpp
# End Source File
# Begin Source File

SOURCE=.\maddecode.h
# End Source File
# Begin Source File

SOURCE=.\MADFilter.cpp
# End Source File
# Begin Source File

SOURCE=.\MADFilter.def
# End Source File
# Begin Source File

SOURCE=.\MADFilter.h
# End Source File
# Begin Source File

SOURCE=.\res\MADFilter.rc
# End Source File
# Begin Source File

SOURCE=.\MADFilter_CLSIDs.h
# End Source File
# Begin Source File

SOURCE=.\MADFilterInterface.h
# End Source File
# Begin Source File

SOURCE=.\MADFilterProperties.cpp
# End Source File
# Begin Source File

SOURCE=.\MADFilterProperties.h
# End Source File
# Begin Source File

SOURCE=.\memcopy\memcpy_amd.cpp
# End Source File
# Begin Source File

SOURCE=.\memcopy\memcpy_SIMD.cpp
# End Source File
# Begin Source File

SOURCE=.\res\resource.h
# End Source File
# Begin Source File

SOURCE=.\res\version.h
# End Source File
# End Target
# End Project