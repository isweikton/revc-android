#include "SDL.h"
#include "SDL_gamecontroller.h"
#include "SDL_joystick.h"
#ifdef _WIN32
#include <shlobj.h>
#include <basetsd.h>
#include <mmsystem.h>
#include <regstr.h>
#include <shellapi.h>
#include <windowsx.h>

DWORD _dwOperatingSystemVersion;
#include "resource.h"
#else
long _dwOperatingSystemVersion;
#ifndef __SWITCH__
#ifndef __APPLE__
#include <sys/sysinfo.h>
#else
#include <mach/mach_host.h>
#include <sys/sysctl.h>
#endif
#endif
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stddef.h>
#endif

#include "common.h"
#if (defined(_MSC_VER))
#include <tchar.h>
#endif /* (defined(_MSC_VER)) */
#include <stdio.h>
#include "rwcore.h"
#include "skeleton.h"
#include "platform.h"
#include "crossplatform.h"

#include "main.h"
#include "FileMgr.h"
#include "Text.h"
#include "Pad.h"
#include "Timer.h"
#include "DMAudio.h"
#include "ControllerConfig.h"
#include "Frontend.h"
#include "Game.h"
#include "PCSave.h"
#include "MemoryCard.h"
#include "Sprite2d.h"
#include "AnimViewer.h"
#include "Font.h"
#include "MemoryMgr.h"

#define MAX_SUBSYSTEMS		(16)

rw::EngineOpenParams openParams;

static RwBool		  ForegroundApp = TRUE;
static RwBool		  WindowIconified = FALSE;
static RwBool		  WindowFocused = TRUE;

static RwBool		  RwInitialised = FALSE;

static RwSubSystemInfo GsubSysInfo[MAX_SUBSYSTEMS];
static RwInt32		GnumSubSystems = 0;
static RwInt32		GcurSel = 0, GcurSelVM = 0;

static RwBool useDefault;

// What is that for anyway?
#ifndef IMPROVED_VIDEOMODE
static RwBool defaultFullscreenRes = TRUE;
#else
static RwBool defaultFullscreenRes = FALSE;
static RwInt32 bestWndMode = -1;
#endif

static psGlobalType PsGlobal;
#define PSGLOBAL(var) (((psGlobalType *)(RsGlobal.ps))->var)

size_t _dwMemAvailPhys;
RwUInt32 gGameState;


bool mouse1 = false;
bool mouse2 = false;

float mousePosX = 0.f;
float mousePosY = 0.f;

TouchInfo touchInfo[10] = {0};

static constexpr SDL_FingerID kInvalidFinger = static_cast<SDL_FingerID>(-1);
static constexpr size_t kMaxFingerSlots = sizeof(touchInfo) / sizeof(touchInfo[0]);
static SDL_FingerID fingerSlots[kMaxFingerSlots] = { kInvalidFinger };

static int
FindFingerSlot(SDL_FingerID id)
{
	for (size_t i = 0; i < kMaxFingerSlots; ++i) {
		if (fingerSlots[i] == id)
			return static_cast<int>(i);
	}
	return -1;
}

static int
AcquireFingerSlot(SDL_FingerID id)
{
	int existing = FindFingerSlot(id);
	if (existing >= 0)
		return existing;

	for (size_t i = 0; i < kMaxFingerSlots; ++i) {
		if (fingerSlots[i] == kInvalidFinger) {
			fingerSlots[i] = id;
			return static_cast<int>(i);
		}
	}
	return -1;
}

static void
ReleaseFingerSlot(SDL_FingerID id)
{
	int idx = FindFingerSlot(id);
	if (idx >= 0)
		fingerSlots[idx] = kInvalidFinger;
}
/*
 *****************************************************************************
 */
void _psCreateFolder(const char *path)
{
#ifdef _WIN32
	HANDLE hfle = CreateFile(path, GENERIC_READ, 
									FILE_SHARE_READ,
									nil,
									OPEN_EXISTING,
									FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL,
									nil);

	if ( hfle == INVALID_HANDLE_VALUE )
		CreateDirectory(path, nil);
	else
		CloseHandle(hfle);
#else
	struct stat info;
	char fullpath[PATH_MAX];
	if (realpath(path, fullpath) == nil) {
		strncpy(fullpath, path, sizeof(fullpath) - 1);
		fullpath[sizeof(fullpath) - 1] = '\0';
	}

	if (lstat(fullpath, &info) != 0) {
		if (errno == ENOENT || (errno != EACCES && !S_ISDIR(info.st_mode))) {
			mkdir(fullpath, 0755);
		}
	}
#endif
}

/*
 *****************************************************************************
 */
const char *_psGetUserFilesFolder()
{
#if defined USE_MY_DOCUMENTS && defined _WIN32
	HKEY hKey = NULL;

	static CHAR szUserFiles[256];

	if ( RegOpenKeyEx(HKEY_CURRENT_USER,
						REGSTR_PATH_SPECIAL_FOLDERS,
						REG_OPTION_RESERVED,
						KEY_READ,
						&hKey) == ERROR_SUCCESS )
	{
		DWORD KeyType;
		DWORD KeycbData = sizeof(szUserFiles);
		if ( RegQueryValueEx(hKey,
							"Personal",
							NULL,
							&KeyType,
							(LPBYTE)szUserFiles,
							&KeycbData) == ERROR_SUCCESS )
		{
			RegCloseKey(hKey);
			strcat(szUserFiles, "\\GTA Vice City User Files");
			_psCreateFolder(szUserFiles);
			return szUserFiles;
		}	

		RegCloseKey(hKey);		
	}
	
	strcpy(szUserFiles, "data");
	return szUserFiles;
#else
	static char szUserFiles[256];
	strcpy(szUserFiles, "userfiles");
	_psCreateFolder(szUserFiles);
	return szUserFiles;
#endif
}

/*
 *****************************************************************************
 */
RwBool
psCameraBeginUpdate(RwCamera *camera)
{
	if ( !RwCameraBeginUpdate(Scene.camera) )
	{
		ForegroundApp = FALSE;
		RsEventHandler(rsACTIVATE, (void *)FALSE);
		return FALSE;
	}
	
	return TRUE;
}

/*
 *****************************************************************************
 */
void
psCameraShowRaster(RwCamera *camera)
{
#ifdef ANDROID
	RwCameraShowRaster(camera, PSGLOBAL(window), rwRASTERFLIPDONTWAIT);
	return;
#endif
#ifdef LEGACY_MENU_OPTIONS
	if (FrontEndMenuManager.m_PrefsVsync || FrontEndMenuManager.m_bMenuActive)
#else
	if (FrontEndMenuManager.m_PrefsFrameLimiter || FrontEndMenuManager.m_bMenuActive)
#endif
		RwCameraShowRaster(camera, PSGLOBAL(window), rwRASTERFLIPWAITVSYNC);
	else
		RwCameraShowRaster(camera, PSGLOBAL(window), rwRASTERFLIPDONTWAIT);

	return;
}

/*
 *****************************************************************************
 */
RwImage *
psGrabScreen(RwCamera *pCamera)
{
#ifndef LIBRW
	RwRaster *pRaster = RwCameraGetRaster(pCamera);
	if (RwImage *pImage = RwImageCreate(pRaster->width, pRaster->height, 32)) {
		RwImageAllocatePixels(pImage);
		RwImageSetFromRaster(pImage, pRaster);
		return pImage;
	}
#else
	rw::Image *image = RwCameraGetRaster(pCamera)->toImage();
	image->removeMask();
	if(image)
		return image;
#endif
	return nil;
}

/*
 *****************************************************************************
 */
#ifdef _WIN32
#pragma comment( lib, "Winmm.lib" ) // Needed for time
RwUInt32
psTimer(void)
{
	RwUInt32 time;

	TIMECAPS TimeCaps;
	
	timeGetDevCaps(&TimeCaps, sizeof(TIMECAPS));
	
	timeBeginPeriod(TimeCaps.wPeriodMin);
	
	time = (RwUInt32) timeGetTime();

	timeEndPeriod(TimeCaps.wPeriodMin);
	
	return time;
}
#else
double
psTimer(void)
{
	struct timespec start; 
#if defined(CLOCK_MONOTONIC_RAW)
	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
#elif defined(CLOCK_MONOTONIC_FAST)
	clock_gettime(CLOCK_MONOTONIC_FAST, &start);
#else
	clock_gettime(CLOCK_MONOTONIC, &start);
#endif
	return start.tv_sec * 1000.0 + start.tv_nsec/1000000.0;
}
#endif     

void
psMouseSetPos(RwV2d *pos)
{

	SDL_WarpMouseInWindow(PSGLOBAL(window), pos->x, pos->y);
	
	PSGLOBAL(lastMousePos.x) = (RwInt32)pos->x;

	PSGLOBAL(lastMousePos.y) = (RwInt32)pos->y;

	return;
}

/*
 *****************************************************************************
 */
RwMemoryFunctions*
psGetMemoryFunctions(void)
{
#ifdef USE_CUSTOM_ALLOCATOR
	return &memFuncs;
#else
	return nil;
#endif
}

/*
 *****************************************************************************
 */
RwBool
psInstallFileSystem(void)
{
	return (TRUE);
}

/*
 *****************************************************************************
 */
RwBool
psNativeTextureSupport(void)
{
	return true;
}

/*
 *****************************************************************************
 */
#ifdef UNDER_CE
#define CMDSTR	LPWSTR
#else
#define CMDSTR	LPSTR
#endif

static void _psInitializeVibration() {}
static void _psHandleVibration() 
{
	CPad* pad = CPad::GetPad(0);

	if (pad->ShakeDur < CTimer::GetTimeStepInMilliseconds())
		pad->ShakeDur = 0;
	else
		pad->ShakeDur -= CTimer::GetTimeStepInMilliseconds();
	if (pad->ShakeDur == 0) pad->ShakeFreq = 0;

	uint16 freq_low = pad->ShakeFreq == 0.0 ? 160.0f : pad->ShakeFreq * 3;
	uint16 freq_high = pad->ShakeFreq == 0.0 ? 320.0f : pad->ShakeFreq * 3;
	if(pad->ShakeFreq > 0)
		SDL_JoystickRumble(PSGLOBAL(joy1),freq_low,freq_high, 1);

}

RwBool
psInitialize(void)
{
 	PsGlobal.lastMousePos.x = PsGlobal.lastMousePos.y = 0.0f;
	PsGlobal.lastTouchPos.x = PsGlobal.lastTouchPos.y = 0.0f;
 
 	RsGlobal.ps = &PsGlobal;
 	
 	PsGlobal.fullScreen = FALSE;
 	PsGlobal.cursorIsInWindow = FALSE;
 	WindowFocused = TRUE;
 	WindowIconified = FALSE;
 	
 	PsGlobal.joy1 = NULL;
 	PsGlobal.joy2 = NULL;
	for (size_t i = 0; i < kMaxFingerSlots; ++i)
		fingerSlots[i] = kInvalidFinger;
    CFileMgr::Initialise();
	
#ifdef PS2_MENU
	CPad::Initialise();
	CPad::GetPad(0)->Mode = 0;

	CGame::frenchGame = false;
	CGame::germanGame = false;
	CGame::nastyGame = true;
	CMenuManager::m_PrefsAllowNastyGame = true;

#ifndef _WIN32
	// Mandatory for Linux(Unix? Posix?) to set lang. to environment lang.
	setlocale(LC_ALL, "");	

	char *systemLang, *keyboardLang;

	systemLang = setlocale (LC_ALL, NULL);
	keyboardLang = setlocale (LC_CTYPE, NULL);
	
	short lang;
	lang = !strncmp(systemLang, "fr_",3) ? LANG_FRENCH :
					!strncmp(systemLang, "de_",3) ? LANG_GERMAN :
					!strncmp(systemLang, "en_",3) ? LANG_ENGLISH :
					!strncmp(systemLang, "it_",3) ? LANG_ITALIAN :
					!strncmp(systemLang, "es_",3) ? LANG_SPANISH :
					LANG_OTHER;
#else
	WORD lang	= PRIMARYLANGID(GetSystemDefaultLCID());
#endif

	if ( lang  == LANG_ITALIAN )
		CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_ITALIAN;
	else if ( lang  == LANG_SPANISH )
		CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_SPANISH;
	else if ( lang  == LANG_GERMAN )
	{
		CGame::germanGame = true;
		CGame::nastyGame = false;
		CMenuManager::m_PrefsAllowNastyGame = false;
		CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_GERMAN;
	}
	else if ( lang  == LANG_FRENCH )
	{
		CGame::frenchGame = true;
		CGame::nastyGame = false;
		CMenuManager::m_PrefsAllowNastyGame = false;
		CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_FRENCH;
	}
	else
		CMenuManager::m_PrefsLanguage = CMenuManager::LANGUAGE_AMERICAN;

	FrontEndMenuManager.InitialiseMenuContentsAfterLoadingGame();

	TheMemoryCard.Init();
#else
	//C_PcSave::SetSaveDirectory(_psGetUserFilesFolder());//FIXME
	
	InitialiseLanguage();

#endif

	_psInitializeVibration();
	
	gGameState = GS_START_UP;
	TRACE("gGameState = GS_START_UP");
#ifdef _WIN32
	OSVERSIONINFO verInfo;
	verInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	
	GetVersionEx(&verInfo);
	
	_dwOperatingSystemVersion = OS_WIN95;
	
	if ( verInfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
	{
		if ( verInfo.dwMajorVersion == 4 )
		{
			debug("Operating System is WinNT\n");
			_dwOperatingSystemVersion = OS_WINNT;
		}
		else if ( verInfo.dwMajorVersion == 5 )
		{
			debug("Operating System is Win2000\n");
			_dwOperatingSystemVersion = OS_WIN2000;
		}
		else if ( verInfo.dwMajorVersion > 5 )
		{
			debug("Operating System is WinXP or greater\n");
			_dwOperatingSystemVersion = OS_WINXP;
		}
	}
	else if ( verInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS )
	{
		if ( verInfo.dwMajorVersion > 4 || verInfo.dwMajorVersion == 4 && verInfo.dwMinorVersion != 0 )
		{
			debug("Operating System is Win98\n");
			_dwOperatingSystemVersion = OS_WIN98;
		}
		else
		{
			debug("Operating System is Win95\n");
			_dwOperatingSystemVersion = OS_WIN95;
		}
	}
#else
	_dwOperatingSystemVersion = OS_WINXP; // To fool other classes
#endif

	
#ifndef PS2_MENU
	FrontEndMenuManager.LoadSettings();
#endif


#ifdef _WIN32
	MEMORYSTATUS memstats;
	GlobalMemoryStatus(&memstats);

	_dwMemAvailPhys = memstats.dwAvailPhys;

	debug("Physical memory size %u\n", memstats.dwTotalPhys);
	debug("Available physical memory %u\n", memstats.dwAvailPhys);
#elif defined (__APPLE__)
	uint64_t size = 0;
	uint64_t page_size = 0;
	size_t uint64_len = sizeof(uint64_t);
	size_t ull_len = sizeof(unsigned long long);
	sysctl((int[]){CTL_HW, HW_PAGESIZE}, 2, &page_size, &ull_len, NULL, 0);
	sysctl((int[]){CTL_HW, HW_MEMSIZE}, 2, &size, &uint64_len, NULL, 0);
	vm_statistics_data_t vm_stat;
	mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vm_stat, &count);
	_dwMemAvailPhys = (uint64_t)(vm_stat.free_count * page_size);
	debug("Physical memory size %llu\n", _dwMemAvailPhys);
	debug("Available physical memory %llu\n", size);
#elif defined (__SWITCH__)
	svcGetInfo(&_dwMemAvailPhys, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
	debug("Physical memory size %llu\n", _dwMemAvailPhys);
#else
#ifndef __APPLE__
 	struct sysinfo systemInfo;
	sysinfo(&systemInfo);
	_dwMemAvailPhys = systemInfo.freeram;
	debug("Physical memory size %u\n", systemInfo.totalram);
	debug("Available physical memory %u\n", systemInfo.freeram);
#else
	uint64_t size = 0;
	uint64_t page_size = 0;
	size_t uint64_len = sizeof(uint64_t);
	size_t ull_len = sizeof(unsigned long long);
	sysctl((int[]){CTL_HW, HW_PAGESIZE}, 2, &page_size, &ull_len, NULL, 0);
	sysctl((int[]){CTL_HW, HW_MEMSIZE}, 2, &size, &uint64_len, NULL, 0);
	vm_statistics_data_t vm_stat;
	mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
	host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vm_stat, &count);
	_dwMemAvailPhys = (uint64_t)(vm_stat.free_count * page_size);
	debug("Physical memory size %llu\n", _dwMemAvailPhys);
	debug("Available physical memory %llu\n", size);
#endif
	_dwOperatingSystemVersion = OS_WINXP; // To fool other classes
#endif
  
  TheText.Unload();
	return TRUE;
}


/*
 *****************************************************************************
 */
void
psTerminate(void)
{
	return;
}

static RwChar **_VMList;

/*
 *****************************************************************************
 */
RwInt32 _psGetNumVideModes()
{
	return RwEngineGetNumVideoModes();
}
 
/*
 *****************************************************************************
 */
RwBool _psFreeVideoModeList()
{
	RwInt32 numModes;
	RwInt32 i;
	
	numModes = _psGetNumVideModes();
	
	if ( _VMList == nil )
		return TRUE;
	
	for ( i = 0; i < numModes; i++ )
	{
		RwFree(_VMList[i]);
	}
	
	RwFree(_VMList);
	
	_VMList = nil;
	
	return TRUE;
}

/*
 *****************************************************************************
 */
RwChar **_psGetVideoModeList()
{
	RwInt32 numModes;
	RwInt32 i;
	
	if ( _VMList != nil )
	{
		return _VMList;
	}
	
	numModes = RwEngineGetNumVideoModes();
	
	_VMList = (RwChar **)RwCalloc(numModes, sizeof(RwChar*));
	
	for ( i = 0; i < numModes; i++	)
	{
		RwVideoMode			vm;
		
		RwEngineGetVideoModeInfo(&vm, i);
		
		if ( vm.flags & rwVIDEOMODEEXCLUSIVE )
		{
			_VMList[i] = (RwChar*)RwCalloc(100, sizeof(RwChar));
			rwsprintf(_VMList[i],"%d X %d X %d", vm.width, vm.height, vm.depth);
		}
		else
			_VMList[i] = nil;
	}
	
	return _VMList;
}

/*
 *****************************************************************************
 */
void _psSelectScreenVM(RwInt32 videoMode)
{
 	RwTexDictionarySetCurrent( nil );
 	
	FrontEndMenuManager.UnloadTextures();
	
	if (!_psSetVideoMode(RwEngineGetCurrentSubSystem(), videoMode))
	{
		RsGlobal.quit = TRUE;

		printf("ERROR: Failed to select new screen resolution\n");
	}
	else
		FrontEndMenuManager.LoadAllTextures();
}

/*
 *****************************************************************************
 */

RwBool IsForegroundApp()
{
	return !!ForegroundApp;
}

/*
 *****************************************************************************
 */
RwBool
psSelectDevice()
{
	RwVideoMode			vm;
	RwInt32				subSysNum;
	RwInt32				AutoRenderer = 0;
	

	RwBool modeFound = FALSE;
	
	if ( !useDefault )
	{
		GnumSubSystems = RwEngineGetNumSubSystems();
		if ( !GnumSubSystems )
		{
			return FALSE;
		}
		
		/* Just to be sure ... */
		GnumSubSystems = (GnumSubSystems > MAX_SUBSYSTEMS) ? MAX_SUBSYSTEMS : GnumSubSystems;
		
		/* Get the names of all the sub systems */
		for (subSysNum = 0; subSysNum < GnumSubSystems; subSysNum++)
		{
			RwEngineGetSubSystemInfo(&GsubSysInfo[subSysNum], subSysNum);
		}
		
		/* Get the default selection */
		GcurSel = RwEngineGetCurrentSubSystem();
#ifdef IMPROVED_VIDEOMODE
		if(FrontEndMenuManager.m_nPrefsSubsystem < GnumSubSystems)
			GcurSel = FrontEndMenuManager.m_nPrefsSubsystem;
#endif
	}
	
	/* Set the driver to use the correct sub system */
	if (!RwEngineSetSubSystem(GcurSel))
	{
		return FALSE;
	}

#ifdef IMPROVED_VIDEOMODE
	FrontEndMenuManager.m_nPrefsSubsystem = GcurSel;
#endif

#ifndef IMPROVED_VIDEOMODE
	if ( !useDefault )
	{
		if ( _psGetVideoModeList()[FrontEndMenuManager.m_nDisplayVideoMode] && FrontEndMenuManager.m_nDisplayVideoMode )
		{
			FrontEndMenuManager.m_nPrefsVideoMode = FrontEndMenuManager.m_nDisplayVideoMode;
			GcurSelVM = FrontEndMenuManager.m_nDisplayVideoMode;
		}
		else
		{
#ifdef DEFAULT_NATIVE_RESOLUTION
			// get the native video mode
			HDC hDevice = GetDC(NULL);
			int w = GetDeviceCaps(hDevice, HORZRES);
			int h = GetDeviceCaps(hDevice, VERTRES);
			int d = GetDeviceCaps(hDevice, BITSPIXEL);
#else
			const int w = 640;
			const int h = 480;
			const int d = 16;
#endif
			while ( !modeFound && GcurSelVM < RwEngineGetNumVideoModes() )
			{
				RwEngineGetVideoModeInfo(&vm, GcurSelVM);
				if ( defaultFullscreenRes	&& vm.width	 != w 
											|| vm.height != h
											|| vm.depth	 != d
											|| !(vm.flags & rwVIDEOMODEEXCLUSIVE) )
					++GcurSelVM;
				else
					modeFound = TRUE;
			}
			
			if ( !modeFound )
			{
#ifdef DEFAULT_NATIVE_RESOLUTION
				GcurSelVM = 1;
#else
				printf("WARNING: Cannot find 640x480 video mode, selecting device cancelled\n");
				return FALSE;
#endif
			}
		}
	}
#else
	if ( !useDefault )
	{
		bool noPrefs = FrontEndMenuManager.m_nPrefsWidth == 0 ||
		   FrontEndMenuManager.m_nPrefsHeight == 0 ||
		   FrontEndMenuManager.m_nPrefsDepth == 0;
		   
		bool fitsPrefs = vm.width <= FrontEndMenuManager.m_nPrefsWidth || 
		vm.height <= FrontEndMenuManager.m_nPrefsHeight ||
		vm.depth <= FrontEndMenuManager.m_nPrefsDepth;
		
		if(noPrefs || !fitsPrefs) { //help me
			// Defaults if nothing specified or we are bigger
			SDL_DisplayMode mode;
			SDL_GetDesktopDisplayMode(0, &mode);
			FrontEndMenuManager.m_nPrefsWidth = mode.w;
			FrontEndMenuManager.m_nPrefsHeight = mode.h;
			debug("Resolution set to default w:%d h:%d\n", mode.w, mode.h);
			FrontEndMenuManager.m_nPrefsDepth = 32;
			FrontEndMenuManager.m_nPrefsWindowed = 0;
		}

		// Find the videomode that best fits what we got from the settings file
		RwInt32 bestFsMode = -1;
		RwInt32 bestWidth = -1;
		RwInt32 bestHeight = -1;
		RwInt32 bestDepth = -1;
		for(GcurSelVM = 0; GcurSelVM < RwEngineGetNumVideoModes(); GcurSelVM++){
			RwEngineGetVideoModeInfo(&vm, GcurSelVM);
			if (!(vm.flags & rwVIDEOMODEEXCLUSIVE)){
				bestWndMode = GcurSelVM;
			} else {
				// try the largest one that isn't larger than what we wanted
				if(vm.width >= bestWidth &&
				   vm.height >= bestHeight &&
				   vm.depth >= bestDepth)
				{
					// if(vm.width <= FrontEndMenuManager.m_nPrefsWidth && vm.height <= FrontEndMenuManager.m_nPrefsHeight && vm.depth <= FrontEndMenuManager.m_nPrefsDepth)
					// {
						bestWidth = vm.width;
						bestHeight = vm.height;
						bestDepth = vm.depth;
						bestFsMode = GcurSelVM;
						debug("Found resolution is w:%d h:%d\n", bestWidth, bestHeight);
					// }
					// else{
					// 	debug("Prefs resolution is smaller, than we have!\n");
					// }
				}
			}
		}

		if(bestFsMode < 0){
			debug("WARNING: Cannot find desired video mode, selecting device cancelled\n %d", bestFsMode);
			return FALSE;
		}
		GcurSelVM = bestFsMode;

		FrontEndMenuManager.m_nDisplayVideoMode = GcurSelVM;
		FrontEndMenuManager.m_nPrefsVideoMode = FrontEndMenuManager.m_nDisplayVideoMode;

		FrontEndMenuManager.m_nSelectedScreenMode = FrontEndMenuManager.m_nPrefsWindowed;
	}
#endif

	RwEngineGetVideoModeInfo(&vm, GcurSelVM);

#ifdef IMPROVED_VIDEOMODE
	if (FrontEndMenuManager.m_nPrefsWindowed)
		GcurSelVM = bestWndMode;

	// Now GcurSelVM is 0 but vm has sizes(and fullscreen flag) of the video mode we want, that's why we changed the rwVIDEOMODEEXCLUSIVE conditions below
	FrontEndMenuManager.m_nPrefsWidth = vm.width;
	FrontEndMenuManager.m_nPrefsHeight = vm.height;
	FrontEndMenuManager.m_nPrefsDepth = vm.depth;
#endif

#ifndef PS2_MENU
	FrontEndMenuManager.m_nCurrOption = 0;
#endif
	
	/* Set up the video mode and set the apps window
	* dimensions to match */
	if (!RwEngineSetVideoMode(GcurSelVM))
	{
		return FALSE;
	}
	/*
	TODO
	if (vm.flags & rwVIDEOMODEEXCLUSIVE)
	{
		debug("%dx%dx%d", vm.width, vm.height, vm.depth);
		
		UINT refresh = GetBestRefreshRate(vm.width, vm.height, vm.depth);
		
		if ( refresh != (UINT)-1 )
		{
			debug("refresh %d", refresh);
			RwD3D8EngineSetRefreshRate((RwUInt32)refresh);
		}
	}
	*/
#ifndef IMPROVED_VIDEOMODE
	if (vm.flags & rwVIDEOMODEEXCLUSIVE)
	{
		// RsGlobal.maximumWidth = vm.width;
		// RsGlobal.maximumHeight = vm.height;
		// RsGlobal.width = vm.width;
		// RsGlobal.height = vm.height;
		
		//PSGLOBAL(fullScreen) = TRUE;
	}
#else
		RsGlobal.maximumWidth = FrontEndMenuManager.m_nPrefsWidth;
		RsGlobal.maximumHeight = FrontEndMenuManager.m_nPrefsHeight;
		RsGlobal.width = FrontEndMenuManager.m_nPrefsWidth;
		RsGlobal.height = FrontEndMenuManager.m_nPrefsHeight;
		
		PSGLOBAL(fullScreen) = !FrontEndMenuManager.m_nPrefsWindowed;
#endif

#ifdef MULTISAMPLING
	RwD3D8EngineSetMultiSamplingLevels(1 << FrontEndMenuManager.m_nPrefsMSAALevel);
#endif	
	return TRUE;
}

void _InputInitialiseJoys()
{
	PSGLOBAL(joy1) = NULL;
	PSGLOBAL(joy2) = NULL;
	
	printf("SDL_NumJoysticks : %d\n", SDL_NumJoysticks());
	for(int i = 0; i < SDL_NumJoysticks(); i++)
	{
		if(SDL_IsGameController(i)){
			if(PSGLOBAL(joy1) == NULL)
				PSGLOBAL(joy1) = SDL_JoystickOpen(i);
			else if (PSGLOBAL(joy2) == NULL)
				PSGLOBAL(joy2) = SDL_JoystickOpen(i);
			else
				break;
			
		}
	}
	if (PSGLOBAL(joy1) != NULL) {
		int count;
		count = SDL_JoystickNumButtons(PSGLOBAL(joy1));
// #ifdef DETECT_JOYSTICK_MENU
// 		strcpy(gSelectedJoystickName, glfwGetJoystickName(PSGLOBAL(joy1)));
// #endif
		ControlsManager.InitDefaultControlConfigJoyPad(count);
	}
}


int lastCursorMode = SDL_DISABLE;
int keymap[SDL_NUM_SCANCODES];
bool lshiftStatus = false;
bool rshiftStatus = false;


static void
initkeymap(void)
{
    for (int i = 0; i < SDL_NUM_SCANCODES; ++i)
        keymap[i] = rsNULL;

    keymap[SDL_SCANCODE_SPACE] = ' ';
    keymap[SDL_SCANCODE_APOSTROPHE] = '\'';
    keymap[SDL_SCANCODE_COMMA] = ',';
    keymap[SDL_SCANCODE_MINUS] = '-';
    keymap[SDL_SCANCODE_PERIOD] = '.';
    keymap[SDL_SCANCODE_SLASH] = '/';
    keymap[SDL_SCANCODE_0] = '0';
    keymap[SDL_SCANCODE_1] = '1';
    keymap[SDL_SCANCODE_2] = '2';
    keymap[SDL_SCANCODE_3] = '3';
    keymap[SDL_SCANCODE_4] = '4';
    keymap[SDL_SCANCODE_5] = '5';
    keymap[SDL_SCANCODE_6] = '6';
    keymap[SDL_SCANCODE_7] = '7';
    keymap[SDL_SCANCODE_8] = '8';
    keymap[SDL_SCANCODE_9] = '9';
    keymap[SDL_SCANCODE_SEMICOLON] = ';';
    keymap[SDL_SCANCODE_EQUALS] = '=';
    keymap[SDL_SCANCODE_A] = 'A';
    keymap[SDL_SCANCODE_B] = 'B';
    keymap[SDL_SCANCODE_C] = 'C';
    keymap[SDL_SCANCODE_D] = 'D';
    keymap[SDL_SCANCODE_E] = 'E';
    keymap[SDL_SCANCODE_F] = 'F';
    keymap[SDL_SCANCODE_G] = 'G';
    keymap[SDL_SCANCODE_H] = 'H';
    keymap[SDL_SCANCODE_I] = 'I';
    keymap[SDL_SCANCODE_J] = 'J';
    keymap[SDL_SCANCODE_K] = 'K';
    keymap[SDL_SCANCODE_L] = 'L';
    keymap[SDL_SCANCODE_M] = 'M';
    keymap[SDL_SCANCODE_N] = 'N';
    keymap[SDL_SCANCODE_O] = 'O';
    keymap[SDL_SCANCODE_P] = 'P';
    keymap[SDL_SCANCODE_Q] = 'Q';
    keymap[SDL_SCANCODE_R] = 'R';
    keymap[SDL_SCANCODE_S] = 'S';
    keymap[SDL_SCANCODE_T] = 'T';
    keymap[SDL_SCANCODE_U] = 'U';
    keymap[SDL_SCANCODE_V] = 'V';
    keymap[SDL_SCANCODE_W] = 'W';
    keymap[SDL_SCANCODE_X] = 'X';
    keymap[SDL_SCANCODE_Y] = 'Y';
    keymap[SDL_SCANCODE_Z] = 'Z';
    keymap[SDL_SCANCODE_LEFTBRACKET] = '[';
    keymap[SDL_SCANCODE_BACKSLASH] = '\\';
    keymap[SDL_SCANCODE_RIGHTBRACKET] = ']';
    keymap[SDL_SCANCODE_GRAVE] = '`';
    keymap[SDL_SCANCODE_ESCAPE] = rsESC;
    keymap[SDL_SCANCODE_RETURN] = rsENTER;
    keymap[SDL_SCANCODE_TAB] = rsTAB;
    keymap[SDL_SCANCODE_BACKSPACE] = rsBACKSP;
    keymap[SDL_SCANCODE_INSERT] = rsINS;
    keymap[SDL_SCANCODE_DELETE] = rsDEL;
    keymap[SDL_SCANCODE_RIGHT] = rsRIGHT;
    keymap[SDL_SCANCODE_LEFT] = rsLEFT;
    keymap[SDL_SCANCODE_DOWN] = rsDOWN;
    keymap[SDL_SCANCODE_UP] = rsUP;
    keymap[SDL_SCANCODE_PAGEUP] = rsPGUP;
    keymap[SDL_SCANCODE_PAGEDOWN] = rsPGDN;
    keymap[SDL_SCANCODE_HOME] = rsHOME;
    keymap[SDL_SCANCODE_END] = rsEND;
    keymap[SDL_SCANCODE_CAPSLOCK] = rsCAPSLK;
    keymap[SDL_SCANCODE_SCROLLLOCK] = rsSCROLL;
    keymap[SDL_SCANCODE_NUMLOCKCLEAR] = rsNUMLOCK;
    keymap[SDL_SCANCODE_PRINTSCREEN] = rsNULL;
    keymap[SDL_SCANCODE_PAUSE] = rsPAUSE;

    keymap[SDL_SCANCODE_F1] = rsF1;
    keymap[SDL_SCANCODE_F2] = rsF2;
    keymap[SDL_SCANCODE_F3] = rsF3;
    keymap[SDL_SCANCODE_F4] = rsF4;
    keymap[SDL_SCANCODE_F5] = rsF5;
    keymap[SDL_SCANCODE_F6] = rsF6;
    keymap[SDL_SCANCODE_F7] = rsF7;
    keymap[SDL_SCANCODE_F8] = rsF8;
    keymap[SDL_SCANCODE_F9] = rsF9;
    keymap[SDL_SCANCODE_F10] = rsF10;
    keymap[SDL_SCANCODE_F11] = rsF11;
    keymap[SDL_SCANCODE_F12] = rsF12;
}

void SDL_Active()
{
	Uint32 flags = SDL_GetWindowFlags(PSGLOBAL(window));;
	if((flags & SDL_WINDOW_HIDDEN))
	{
		WindowFocused = FALSE;
	}
	else{
		WindowFocused = TRUE;
	}
}

void Joy_Events(SDL_Event *event)
{
	RsPadButtonStatus bs;
	bs.padID = 0;
	switch(event->type)
	{
		case SDL_CONTROLLERDEVICEADDED:
			_InputInitialiseJoys();
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
			_InputInitialiseJoys();
			break;
		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			if (PSGLOBAL(joy1) && event->cdevice.which == SDL_JoystickInstanceID(PSGLOBAL(joy1))) {
				memcpy(&ControlsManager.m_OldState, &ControlsManager.m_NewState, sizeof(ControlsManager.m_NewState));
				if(event->type == SDL_JOYBUTTONUP){
					RsPadEventHandler(rsPADBUTTONUP,   (void *)&bs);	
					ControlsManager.m_NewState.mappedButtons[event->jbutton.button] = false;	
				}
				else{
					RsPadEventHandler(rsPADBUTTONDOWN, (void *)&bs);
					ControlsManager.m_NewState.mappedButtons[event->jbutton.button] = true;
				}
			}
			break;
	}
}

void SDL_Events(SDL_Event *event)
{
	Joy_Events(event);
	switch (event->type) 
	{
		case SDL_QUIT:
			RsGlobal.quit = true;
			break;
		case SDL_MOUSEMOTION:
			if (FrontEndMenuManager.m_bMenuActive && WindowFocused)
			{
				if(SDL_GetRelativeMouseMode())
					SDL_SetRelativeMouseMode(SDL_FALSE);
				int winw, winh;
				SDL_GetWindowSize(PSGLOBAL(window), &winw, &winh);
				
				int xpos, ypos;
				SDL_GetMouseState(&xpos, &ypos);
				FrontEndMenuManager.m_nMouseTempPosX = xpos * (RsGlobal.maximumWidth / winw);
				FrontEndMenuManager.m_nMouseTempPosY = ypos * (RsGlobal.maximumHeight / winh);
				break;
			}
			else
			{
				if(!WindowFocused)
					break;
				if(!SDL_GetRelativeMouseMode())
					SDL_SetRelativeMouseMode(SDL_TRUE);
				static int xposabs;
				static int yposabs;
				xposabs+= event->motion.xrel;
				yposabs+= event->motion.yrel;
				
				mousePosX = xposabs;
				mousePosY = yposabs;
				}
		case SDL_MOUSEBUTTONDOWN:
			if(event->button.button == SDL_BUTTON_LEFT)
				mouse1 = true;
			else if(event->button.button == SDL_BUTTON_RIGHT)
				mouse2 = true;
			break;
		case SDL_MOUSEBUTTONUP:
			if(event->button.button == SDL_BUTTON_LEFT)
					mouse1 = false;

			else if(event->button.button == SDL_BUTTON_RIGHT)
					mouse2 = false;
			break;
		case SDL_KEYDOWN:
        if (event->key.keysym.scancode >= 0 && event->key.keysym.scancode < SDL_NUM_SCANCODES) 
		{
			RsKeyCodes ks = (RsKeyCodes)keymap[event->key.keysym.scancode];

			if (event->key.keysym.scancode == SDL_SCANCODE_LSHIFT)
				lshiftStatus = true;
			if (event->key.keysym.scancode == SDL_SCANCODE_RSHIFT)
				rshiftStatus = true;

			RsKeyboardEventHandler(rsKEYDOWN, &ks);
        }
        break;
		case SDL_KEYUP:
                if (event->key.keysym.scancode >= 0 && event->key.keysym.scancode < SDL_NUM_SCANCODES) 
				{
					RsKeyCodes ks = (RsKeyCodes)keymap[event->key.keysym.scancode];
					
					if (event->key.keysym.scancode == SDL_SCANCODE_LSHIFT)
						lshiftStatus = false;
					if (event->key.keysym.scancode == SDL_SCANCODE_RSHIFT)
						rshiftStatus = false;
					
					RsKeyboardEventHandler(rsKEYUP, &ks);
				}
                break;
		case SDL_MOUSEWHEEL:
			PSGLOBAL(mouseWheel) = event->wheel.y;
			break;
		
		case SDL_WINDOWEVENT:
			switch(event->window.event)
			{
			case SDL_WINDOWEVENT_RESIZED:
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				int w,h;
				SDL_GetWindowSize(PSGLOBAL(window),&w ,&h );
				if (RwInitialised && gGameState == GS_PLAYING_GAME)
				{
					RsEventHandler(rsIDLE, (void *)TRUE);
				}
				if (RwInitialised && w > 0 && h > 0) {
				RwRect r;

				// TODO fix artifacts of resizing with mouse
				RsGlobal.maximumHeight = h;
				RsGlobal.maximumWidth = w;

				r.x = 0;
				r.y = 0;
				r.w = w;
				r.h = h;

				RsEventHandler(rsCAMERASIZE, &r);
				}
				break;
			}
			break;
		case SDL_FINGERDOWN:{
			int slot = AcquireFingerSlot(event->tfinger.fingerId);
			if (slot >= 0) {
				if (FrontEndMenuManager.m_bMenuActive)
				{
					FrontEndMenuManager.m_nTouchTempPosX = event->tfinger.x * (float)RsGlobal.maximumWidth;
					FrontEndMenuManager.m_nTouchTempPosY = event->tfinger.y * (float)RsGlobal.maximumHeight;
				}
				touchInfo[slot].pressed = true;
				touchInfo[slot].x = event->tfinger.x * (float)RsGlobal.maximumWidth;
				touchInfo[slot].y = event->tfinger.y * (float)RsGlobal.maximumHeight;
			}
			break;
		}
		case SDL_FINGERUP:{
			int slot = FindFingerSlot(event->tfinger.fingerId);
			if (slot >= 0) {
				touchInfo[slot].pressed = false;
				touchInfo[slot].x = 0.0f;
				touchInfo[slot].y = 0.0f;
				touchInfo[slot].dx = 0.0f;
				touchInfo[slot].dy = 0.0f;
				ReleaseFingerSlot(event->tfinger.fingerId);
			}
			break;
		}
		case SDL_FINGERMOTION:{
			int slot = FindFingerSlot(event->tfinger.fingerId);
			if (slot >= 0) {
				touchInfo[slot].x  += event->tfinger.dx * (float)RsGlobal.maximumWidth;
				touchInfo[slot].y  += event->tfinger.dy * (float)RsGlobal.maximumHeight;
				touchInfo[slot].dx  = event->tfinger.dx * (float)RsGlobal.maximumWidth;
				touchInfo[slot].dy  = event->tfinger.dy * (float)RsGlobal.maximumHeight;
			}
			break;
		}
	}
}

// R* calls that in ControllerConfig, idk why
void
_InputTranslateShiftKeyUpDown(RsKeyCodes *rs) {
	RsKeyboardEventHandler(lshiftStatus ? rsKEYDOWN : rsKEYUP, &(*rs = rsLSHIFT));
	RsKeyboardEventHandler(rshiftStatus ? rsKEYDOWN : rsKEYUP, &(*rs = rsRSHIFT));
}

/*
 *****************************************************************************
 */
void psPostRWinit(void)
{
	RwVideoMode vm;
	RwEngineGetVideoModeInfo(&vm, GcurSelVM);

	/* FIXME
	glfwSetCursorEnterCallback(PSGLOBAL(window), cursorEnterCB);
	glfwSetWindowIconifyCallback(PSGLOBAL(window), windowIconifyCB);
	glfwSetJoystickCallback(joysChangeCB);
*/

	_InputInitialiseMouse(false);
	if(!(vm.flags & rwVIDEOMODEEXCLUSIVE))
		SDL_SetWindowSize(PSGLOBAL(window), RsGlobal.maximumWidth, RsGlobal.maximumHeight);
	// Make sure all keys are released
	CPad::GetPad(0)->Clear(true);
	CPad::GetPad(1)->Clear(true);
}

/*
 *****************************************************************************
 */
RwBool _psSetVideoMode(RwInt32 subSystem, RwInt32 videoMode)
{
	RwInitialised = FALSE;
	
	RsEventHandler(rsRWTERMINATE, nil);
	
	GcurSel = subSystem;
	GcurSelVM = videoMode;
	
	useDefault = TRUE;
	
	if ( RsEventHandler(rsRWINITIALIZE, &openParams) == rsEVENTERROR )
		return FALSE;

	RwInitialised = TRUE;
	useDefault = FALSE;
	
	RwRect r;
	
	r.x = 0;
	r.y = 0;
	r.w = RsGlobal.maximumWidth;
	r.h = RsGlobal.maximumHeight;

	RsEventHandler(rsCAMERASIZE, &r);
	
	psPostRWinit();
	
	return TRUE;
}

void HandleExit()
{
#ifdef _WIN32
	MSG message;
	while ( PeekMessage(&message, nil, 0U, 0U, PM_REMOVE|PM_NOYIELD) )
	{
		if( message.message == WM_QUIT )
		{
			RsGlobal.quit = TRUE;
		}
		else
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
	}
#else
	// We now handle terminate message always, why handle on some cases?
	return;
#endif
}

/*
 *****************************************************************************
 */
void InitialiseLanguage()
{
#ifndef _WIN32
	// Mandatory for Linux(Unix? Posix?) to set lang. to environment lang.
	setlocale(LC_ALL, "");	

	char *systemLang, *keyboardLang;

	systemLang = setlocale (LC_ALL, NULL);
	keyboardLang = setlocale (LC_CTYPE, NULL);
	
	short primUserLCID, primSystemLCID;
	primUserLCID = primSystemLCID = !strncmp(systemLang, "fr_",3) ? LANG_FRENCH :
					!strncmp(systemLang, "de_",3) ? LANG_GERMAN :
					!strncmp(systemLang, "en_",3) ? LANG_ENGLISH :
					!strncmp(systemLang, "it_",3) ? LANG_ITALIAN :
					!strncmp(systemLang, "es_",3) ? LANG_SPANISH :
					LANG_OTHER;

	short primLayout = !strncmp(keyboardLang, "fr_",3) ? LANG_FRENCH : (!strncmp(keyboardLang, "de_",3) ? LANG_GERMAN : LANG_ENGLISH);

	short subUserLCID, subSystemLCID;
	subUserLCID = subSystemLCID = !strncmp(systemLang, "en_AU",5) ? SUBLANG_ENGLISH_AUS : SUBLANG_OTHER;
	short subLayout = !strncmp(keyboardLang, "en_AU",5) ? SUBLANG_ENGLISH_AUS : SUBLANG_OTHER;

#else
	WORD primUserLCID	= PRIMARYLANGID(GetSystemDefaultLCID());
	WORD primSystemLCID = PRIMARYLANGID(GetUserDefaultLCID());
	WORD primLayout		= PRIMARYLANGID((DWORD)GetKeyboardLayout(0));
	
	WORD subUserLCID	= SUBLANGID(GetSystemDefaultLCID());
	WORD subSystemLCID	= SUBLANGID(GetUserDefaultLCID());
	WORD subLayout		= SUBLANGID((DWORD)GetKeyboardLayout(0));
#endif
	if (   primUserLCID	  == LANG_GERMAN
		|| primSystemLCID == LANG_GERMAN
		|| primLayout	  == LANG_GERMAN )
	{
		CGame::nastyGame = false;
		FrontEndMenuManager.m_PrefsAllowNastyGame = false;
		CGame::germanGame = true;
	}
	
	if (   primUserLCID	  == LANG_FRENCH
		|| primSystemLCID == LANG_FRENCH
		|| primLayout	  == LANG_FRENCH )
	{
		CGame::nastyGame = false;
		FrontEndMenuManager.m_PrefsAllowNastyGame = false;
		CGame::frenchGame = true;
	}
	
	if (   subUserLCID	 == SUBLANG_ENGLISH_AUS
		|| subSystemLCID == SUBLANG_ENGLISH_AUS
		|| subLayout	 == SUBLANG_ENGLISH_AUS )
		CGame::noProstitutes = true;

#ifdef NASTY_GAME
	CGame::nastyGame = true;
	FrontEndMenuManager.m_PrefsAllowNastyGame = true;
	CGame::noProstitutes = false;
#endif
	
	int32 lang;
	
	switch ( primSystemLCID )
	{
		case LANG_GERMAN:
		{
			lang = LANG_GERMAN;
			break;
		}
		case LANG_FRENCH:
		{
			lang = LANG_FRENCH;
			break;
		}
		case LANG_SPANISH:
		{
			lang = LANG_SPANISH;
			break;
		}
		case LANG_ITALIAN:
		{
			lang = LANG_ITALIAN;
			break;
		}
		default:
		{
			lang = ( subSystemLCID == SUBLANG_ENGLISH_AUS ) ? -99 : LANG_ENGLISH;
			break;
		}
	}
	
	FrontEndMenuManager.OS_Language = primUserLCID;

	switch ( lang )
	{
		case LANG_GERMAN:
		{
			FrontEndMenuManager.m_PrefsLanguage = CMenuManager::LANGUAGE_GERMAN;
			break;
		}
		case LANG_SPANISH:
		{
			FrontEndMenuManager.m_PrefsLanguage = CMenuManager::LANGUAGE_SPANISH;
			break;
		}
		case LANG_FRENCH:
		{
			FrontEndMenuManager.m_PrefsLanguage = CMenuManager::LANGUAGE_FRENCH;
			break;
		}
		case LANG_ITALIAN:
		{
			FrontEndMenuManager.m_PrefsLanguage = CMenuManager::LANGUAGE_ITALIAN;
			break;
		}
		default:
		{
			FrontEndMenuManager.m_PrefsLanguage = CMenuManager::LANGUAGE_AMERICAN;
			break;
		}
	}

#ifndef _WIN32
	// TODO this is needed for strcasecmp to work correctly across all languages, but can these cause other problems??
	setlocale(LC_CTYPE, "C");
	setlocale(LC_COLLATE, "C");
	setlocale(LC_NUMERIC, "C");
#endif

	TheText.Unload();
	TheText.Load();
}

#ifndef _WIN32
void terminateHandler(int sig, siginfo_t *info, void *ucontext) {
	RsGlobal.quit = TRUE;
}

#ifdef FLUSHABLE_STREAMING
void dummyHandler(int sig){
	// Don't kill the app pls
}
#endif
#endif

long _InputInitialiseMouse(bool exclusive)
{
	// Disabled = keep cursor centered and hide
	lastCursorMode = exclusive ? SDL_ENABLE : SDL_DISABLE;
	SDL_ShowCursor(lastCursorMode);
	return 0;
}

void _InputShutdownMouse()
{
	// Not needed
}

bool _InputMouseNeedsExclusive()
{
// 1wn_klaymen1n: The fuck is this? I think it's unneeded. FIXME?
// 	// That was the cause of infamous mouse bug on Win.
// 	
// 	RwVideoMode vm;
// 	RwEngineGetVideoModeInfo(&vm, GcurSelVM);
// 
// 	// If windowed, free the cursor on menu(where this func. is called and DISABLED-HIDDEN transition is done accordingly)
// 	// If it's fullscreen, be sure that it didn't stuck on HIDDEN.
// 	return !(vm.flags & rwVIDEOMODEEXCLUSIVE) || lastCursorMode == SDL_ENABLE;
	return false;
}

/*
 *****************************************************************************
 */
#ifdef _WIN32
int PASCAL
WinMain(HINSTANCE instance,
	HINSTANCE prevInstance	__RWUNUSED__,
	CMDSTR cmdLine,
	int cmdShow)
{

	RwInt32 argc;
	RwChar** argv;
	SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, nil, SPIF_SENDCHANGE);

#ifndef MASTER
	if (strstr(cmdLine, "-console"))
	{
		AllocConsole();
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}
#endif

#else
int
main(int argc, char *argv[])
{
#endif
	RwV2d pos;
	RwInt32 i;

#ifdef USE_CUSTOM_ALLOCATOR
	InitMemoryMgr();
#endif

#if !defined(_WIN32) && !defined(__SWITCH__)
	struct sigaction act;
	act.sa_sigaction = terminateHandler;
	act.sa_flags = SA_SIGINFO;
	sigaction(SIGTERM, &act, NULL);
#ifdef FLUSHABLE_STREAMING
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = dummyHandler;
	sa.sa_flags = 0;
	sigaction(SIGUSR1, &sa, NULL);
#endif
#endif
	
	for(i=1; i < argc; i++)
	{
		if(strcmp(argv[i], "--dir") == 0 && i + 1 < argc)
		{
			const char *gamePath = argv[i+1];
			setenv("GAMEFILES", gamePath, 1);
		}
	}
	
	/* 
	 * Initialize the platform independent data.
	 * This will in turn initialize the platform specific data...
	 */
	if( RsEventHandler(rsINITIALIZE, nil) == rsEVENTERROR )
	{
		return FALSE;
	}

	for(i=1; i<argc; i++)
	{
		RsEventHandler(rsPREINITCOMMANDLINE, argv[i]);
	}
#ifdef _WIN32
	/*
	 * Get proper command line params, cmdLine passed to us does not
	 * work properly under all circumstances...
	 */
	cmdLine = GetCommandLine();

	/*
	 * Parse command line into standard (argv, argc) parameters...
	 */
	argv = CommandLineToArgv(cmdLine, &argc);


	/* 
	 * Parse command line parameters (except program name) one at 
	 * a time BEFORE RenderWare initialization...
	 */
#endif
	/*
	 * Parameters to be used in RwEngineOpen / rsRWINITIALISE event
	 */

	openParams.width = RsGlobal.maximumWidth;
	openParams.height = RsGlobal.maximumHeight;
	openParams.windowtitle = RsGlobal.appName;
	openParams.window = &PSGLOBAL(window);
	
	ControlsManager.MakeControllerActionsBlank();
	ControlsManager.InitDefaultControlConfiguration();

	/* 
	 * Initialize the 3D (RenderWare) components of the app...
	 */
	if( rsEVENTERROR == RsEventHandler(rsRWINITIALIZE, &openParams) )
	{
		RsEventHandler(rsTERMINATE, nil);

		return 0;
	}

#ifdef _WIN32
	HWND wnd = glfwGetWin32Window(PSGLOBAL(window));

	HICON icon = LoadIcon(instance, MAKEINTRESOURCE(IDI_MAIN_ICON));

	SendMessage(wnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
	SendMessage(wnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
#endif

	psPostRWinit();

	ControlsManager.InitDefaultControlConfigMouse(MousePointerStateHelper.GetMouseSetUp());

	//glfwSetWindowPos(PSGLOBAL(window), 0, 0);

	/* 
	 * Parse command line parameters (except program name) one at 
	 * a time AFTER RenderWare initialization...
	 */
	for(i=1; i<argc; i++)
	{
		RsEventHandler(rsCOMMANDLINE, argv[i]);
	}

	/* 
	 * Force a camera resize event...
	 */
	{
		RwRect r;

		r.x = 0;
		r.y = 0;
		r.w = RsGlobal.maximumWidth;
		r.h = RsGlobal.maximumHeight;

		RsEventHandler(rsCAMERASIZE, &r);
	}
#ifdef _WIN32
	SystemParametersInfo(SPI_SETPOWEROFFACTIVE, FALSE, nil, SPIF_SENDCHANGE);
	SystemParametersInfo(SPI_SETLOWPOWERACTIVE, FALSE, nil, SPIF_SENDCHANGE);
	

	STICKYKEYS SavedStickyKeys;
	SavedStickyKeys.cbSize = sizeof(STICKYKEYS);
	
	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &SavedStickyKeys, SPIF_SENDCHANGE);
	
	STICKYKEYS NewStickyKeys;
	NewStickyKeys.cbSize = sizeof(STICKYKEYS);
	NewStickyKeys.dwFlags = SKF_TWOKEYSOFF;
	
	SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &NewStickyKeys, SPIF_SENDCHANGE);
#endif

	{
		CFileMgr::SetDirMyDocuments();
		
#ifdef LOAD_INI_SETTINGS
		// At this point InitDefaultControlConfigJoyPad must have set all bindings to default and ms_padButtonsInited to number of detected buttons.
		// We will load stored bindings below, but let's cache ms_padButtonsInited before LoadINIControllerSettings and LoadSettings clears it,
		// so we can add new joy bindings **on top of** stored bindings.
		int connectedPadButtons = ControlsManager.ms_padButtonsInited;
#endif

		int32 gta3set = CFileMgr::OpenFile("gta_vc.set", "r");
		
		if ( gta3set )
		{
			ControlsManager.LoadSettings(gta3set);
			CFileMgr::CloseFile(gta3set);
		}
		
		CFileMgr::SetDir("");

#ifdef LOAD_INI_SETTINGS
		LoadINIControllerSettings();
		if (connectedPadButtons != 0)
			ControlsManager.InitDefaultControlConfigJoyPad(connectedPadButtons); // add (connected-saved) amount of new button assignments on top of ours

		// these have 2 purposes: creating .ini at the start, and adding newly introduced settings to old .ini at the start
		SaveINISettings();
		SaveINIControllerSettings();
#endif
	}
	
#ifdef _WIN32
	SetErrorMode(SEM_FAILCRITICALERRORS);
#endif

#ifdef PS2_MENU
	int32 r = TheMemoryCard.CheckCardStateAtGameStartUp(CARD_ONE);
	if (   r == CMemoryCard::ERR_DIRNOENTRY  || r == CMemoryCard::ERR_NOFORMAT
		&& r != CMemoryCard::ERR_OPENNOENTRY && r != CMemoryCard::ERR_NONE )
	{
		LoadingScreen(nil, nil, "loadsc0");
		
		TheText.Unload();
		TheText.Load();
		
		CFont::Initialise();
		
		FrontEndMenuManager.DrawMemoryCardStartUpMenus();
	}
#endif
	if(SDL_InitSubSystem( SDL_INIT_JOYSTICK ))
	{
		printf("Failed to initialize SDL GameController API: %s\n", SDL_GetError());
	}
	SDL_GameControllerAddMappingsFromFile( "gamecontrollerdb.txt" );
	_InputInitialiseJoys();
	initkeymap();

	while ( TRUE )
	{
		RwInitialised = TRUE;
		
		/* 
		* Set the initial mouse position...
		*/
		pos.x = RsGlobal.maximumWidth * 0.5f;
		pos.y = RsGlobal.maximumHeight * 0.5f;

		RsMouseSetPos(&pos);
		
		/*
		* Enter the message processing loop...
		*/

#ifndef MASTER
		if (gbModelViewer) {
			// This is TheModelViewer in LCS
			LoadingScreen("Loading the ModelViewer", NULL, GetRandomSplashScreen());
			CAnimViewer::Initialise();
			CTimer::Update();
#ifndef PS2_MENU
			FrontEndMenuManager.m_bGameNotLoaded = false;
#endif
		}
#endif

#ifdef PS2_MENU
		if (TheMemoryCard.m_bWantToLoad)
			LoadSplash(GetLevelSplashScreen(CGame::currLevel));
		
		TheMemoryCard.m_bWantToLoad = false;
		
		CTimer::Update();
		
		while( !RsGlobal.quit && !(FrontEndMenuManager.m_bWantToRestart || TheMemoryCard.b_FoundRecentSavedGameWantToLoad) && !SDL_WindowShouldClose(PSGLOBAL(window)) )
#else
		while( !RsGlobal.quit && !FrontEndMenuManager.m_bWantToRestart /*&& !SDL_WindowShouldClose(PSGLOBAL(window))*/)
#endif
		{
#ifndef LIBRW_SDL2			
			//glfwPollEvents();
#else	
			SDL_Active();
			SDL_Event e;
			while (SDL_PollEvent(&e) != 0){
				SDL_Events(&e);
			}
#endif			
#ifdef GET_KEYBOARD_INPUT_FROM_X11
			checkKeyPresses();
#endif
#ifndef MASTER
			if (gbModelViewer) {
				// This is TheModelViewerCore in LCS
				TheModelViewer();
			} else
#endif
			if ( ForegroundApp )
			{
				switch ( gGameState )
				{
					case GS_START_UP:
					{
#ifdef NO_MOVIES
						gGameState = GS_INIT_ONCE;
#else
						gGameState = GS_INIT_LOGO_MPEG;
#endif
						TRACE("gGameState = GS_INIT_ONCE");
						break;
					}

				    case GS_INIT_LOGO_MPEG:
					{
					    //if (!startupDeactivate)
						//    PlayMovieInWindow(cmdShow, "movies\\Logo.mpg");
					    gGameState = GS_LOGO_MPEG;
					    TRACE("gGameState = GS_LOGO_MPEG;");
					    break;
				    }

				    case GS_LOGO_MPEG:
					{
//					    CPad::UpdatePads();

//					    if (startupDeactivate || ControlsManager.GetJoyButtonJustDown() != 0)
						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetLeftMouseJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetEnterJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetCharJustDown(' '))
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetAltJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetTabJustDown())
//						    ++gGameState;

					    break;
				    }

				    case GS_INIT_INTRO_MPEG:
					{
//#ifndef NO_MOVIES
//					    CloseClip();
//					    CoUninitialize();
//#endif
//
//					    if (CMenuManager::OS_Language == LANG_FRENCH || CMenuManager::OS_Language == LANG_GERMAN)
//						    PlayMovieInWindow(cmdShow, "movies\\GTAtitlesGER.mpg");
//					    else
//						    PlayMovieInWindow(cmdShow, "movies\\GTAtitles.mpg");

					    gGameState = GS_INTRO_MPEG;
					    TRACE("gGameState = GS_INTRO_MPEG;");
					    break;
				    }

				    case GS_INTRO_MPEG:
					{
//					    CPad::UpdatePads();
//
//					    if (startupDeactivate || ControlsManager.GetJoyButtonJustDown() != 0)
						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetLeftMouseJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetEnterJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetCharJustDown(' '))
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetAltJustDown())
//						    ++gGameState;
//					    else if (CPad::GetPad(0)->GetTabJustDown())
//						    ++gGameState;

					    break;
				    }

					case GS_INIT_ONCE:
					{
						//CoUninitialize();
						
#ifdef PS2_MENU
						extern char version_name[64];
						if ( CGame::frenchGame || CGame::germanGame )
							LoadingScreen(NULL, version_name, "loadsc24");
						else
							LoadingScreen(NULL, version_name, "loadsc0");
						
						printf("Into TheGame!!!\n");
#else				
						LoadingScreen(nil, nil, "loadsc0");
						// LoadingScreen(nil, nil, "loadsc0"); // duplicate
#endif
						if ( !CGame::InitialiseOnceAfterRW() )
							RsGlobal.quit = TRUE;
						
#ifdef PS2_MENU
						gGameState = GS_INIT_PLAYING_GAME;
#else
						gGameState = GS_INIT_FRONTEND;
						TRACE("gGameState = GS_INIT_FRONTEND;");
#endif
						break;
					}
#ifndef PS2_MENU
					case GS_INIT_FRONTEND:
					{
						LoadingScreen(nil, nil, "loadsc0");
						// LoadingScreen(nil, nil, "loadsc0"); // duplicate
						
						FrontEndMenuManager.m_bGameNotLoaded = true;
						
						FrontEndMenuManager.m_bStartUpFrontEndRequested = true;
#ifdef ANDROID
						FrontEndMenuManager.m_nCurrScreen = MENUPAGE_NEW_GAME;
						FrontEndMenuManager.m_nCurrOption = 0;
						FrontEndMenuManager.DoSettingsBeforeStartingAGame();
#endif
						
						if ( defaultFullscreenRes )
						{
							defaultFullscreenRes = FALSE;
							FrontEndMenuManager.m_nPrefsVideoMode = GcurSelVM;
							FrontEndMenuManager.m_nDisplayVideoMode = GcurSelVM;
						}
						
						gGameState = GS_FRONTEND;
						TRACE("gGameState = GS_FRONTEND;");
						break;
					}
					
					case GS_FRONTEND:
					{
						if(!WindowIconified)
							RsEventHandler(rsFRONTENDIDLE, nil);

#ifdef PS2_MENU
						if ( !FrontEndMenuManager.m_bMenuActive || TheMemoryCard.m_bWantToLoad )
#else
						if ( !FrontEndMenuManager.m_bMenuActive || FrontEndMenuManager.m_bWantToLoad )
#endif
						{
							gGameState = GS_INIT_PLAYING_GAME;
							TRACE("gGameState = GS_INIT_PLAYING_GAME;");
						}

#ifdef PS2_MENU
						if (TheMemoryCard.m_bWantToLoad )
#else
						if ( FrontEndMenuManager.m_bWantToLoad )
#endif
						{
							InitialiseGame();
							FrontEndMenuManager.m_bGameNotLoaded = false;
							gGameState = GS_PLAYING_GAME;
							TRACE("gGameState = GS_PLAYING_GAME;");
						}
						break;
					}
#endif
					
					case GS_INIT_PLAYING_GAME:
					{
#ifdef PS2_MENU
						CGame::Initialise("DATA\\GTA3.DAT");
						
						//LoadingScreen("Starting Game", NULL, GetRandomSplashScreen());
					
						if (   TheMemoryCard.CheckCardInserted(CARD_ONE) == CMemoryCard::NO_ERR_SUCCESS
							&& TheMemoryCard.ChangeDirectory(CARD_ONE, TheMemoryCard.Cards[CARD_ONE].dir)
							&& TheMemoryCard.FindMostRecentFileName(CARD_ONE, TheMemoryCard.MostRecentFile) == true
							&& TheMemoryCard.CheckDataNotCorrupt(TheMemoryCard.MostRecentFile))
						{
							strcpy(TheMemoryCard.LoadFileName, TheMemoryCard.MostRecentFile);
							TheMemoryCard.b_FoundRecentSavedGameWantToLoad = true;
					
							if (CMenuManager::m_PrefsLanguage != TheMemoryCard.GetLanguageToLoad())
							{
								CMenuManager::m_PrefsLanguage = TheMemoryCard.GetLanguageToLoad();
								TheText.Unload();
								TheText.Load();
							}
					
							CGame::currLevel = (eLevelName)TheMemoryCard.GetLevelToLoad();
						}
#else
						InitialiseGame();

						FrontEndMenuManager.m_bGameNotLoaded = false;
#endif
						gGameState = GS_PLAYING_GAME;
						TRACE("gGameState = GS_PLAYING_GAME;");
						break;
					}
					
					case GS_PLAYING_GAME:
					{
#ifdef ANDROID
						if ( RwInitialised )
							RsEventHandler(rsIDLE, (void *)TRUE);
#else
						float ms = (float)CTimer::GetCurrentTimeInCycles() / (float)CTimer::GetCyclesPerMillisecond();
						if ( RwInitialised )
						{
							if (!FrontEndMenuManager.m_PrefsFrameLimiter || (1000.0f / (float)RsGlobal.maxFPS) < ms)
								RsEventHandler(rsIDLE, (void *)TRUE);
						}
#endif
						break;
					}
				}
			}
			else
			{
				if ( RwCameraBeginUpdate(Scene.camera) )
				{
					RwCameraEndUpdate(Scene.camera);
					ForegroundApp = TRUE;
					RsEventHandler(rsACTIVATE, (void *)TRUE);
				}
				
			}
		}

		
		/* 
		* About to shut down - block resize events again...
		*/
		RwInitialised = FALSE;
		
		FrontEndMenuManager.UnloadTextures();
#ifdef PS2_MENU	
		if ( !(FrontEndMenuManager.m_bWantToRestart || TheMemoryCard.b_FoundRecentSavedGameWantToLoad))
			break;
#else
		if ( !FrontEndMenuManager.m_bWantToRestart )
			break;
#endif
		
		CPad::ResetCheats();
		CPad::StopPadsShaking();
		
		DMAudio.ChangeMusicMode(MUSICMODE_DISABLE);
		
#ifdef PS2_MENU
		CGame::ShutDownForRestart();
#endif
		
		CTimer::Stop();
		
#ifdef PS2_MENU
		if (FrontEndMenuManager.m_bWantToRestart || TheMemoryCard.b_FoundRecentSavedGameWantToLoad)
		{
			if (TheMemoryCard.b_FoundRecentSavedGameWantToLoad)
			{
				FrontEndMenuManager.m_bWantToRestart = true;
				TheMemoryCard.m_bWantToLoad = true;
			}

			CGame::InitialiseWhenRestarting();
			DMAudio.ChangeMusicMode(MUSICMODE_GAME);
			FrontEndMenuManager.m_bWantToRestart = false;
			
			continue;
		}
		
		CGame::ShutDown();	
		CTimer::Stop();
		
		break;
#else
		if ( FrontEndMenuManager.m_bWantToLoad )
		{
			CGame::ShutDownForRestart();
			CGame::InitialiseWhenRestarting();
			DMAudio.ChangeMusicMode(MUSICMODE_GAME);
			LoadSplash(GetLevelSplashScreen(CGame::currLevel));
			FrontEndMenuManager.m_bWantToLoad = false;
		}
		else
		{
#ifndef MASTER
			if( gbModelViewer )
				CAnimViewer::Shutdown();
			else
#endif
			if ( gGameState == GS_PLAYING_GAME )
				CGame::ShutDown();
			
			CTimer::Stop();
			
			if ( FrontEndMenuManager.m_bFirstTime == true )
			{
				gGameState = GS_INIT_FRONTEND;
				TRACE("gGameState = GS_INIT_FRONTEND;");
			}
			else
			{
				gGameState = GS_INIT_PLAYING_GAME;
				TRACE("gGameState = GS_INIT_PLAYING_GAME;");
			}
		}
		
		FrontEndMenuManager.m_bFirstTime = false;
		FrontEndMenuManager.m_bWantToRestart = false;
#endif
	}
	
#ifndef MASTER
	if ( gbModelViewer )
		CAnimViewer::Shutdown();
	else
#endif
	if ( gGameState == GS_PLAYING_GAME )
		CGame::ShutDown();

	DMAudio.Terminate();
	
	_psFreeVideoModeList();


	/*
	 * Tidy up the 3D (RenderWare) components of the application...
	 */
	RsEventHandler(rsRWTERMINATE, nil);

	/*
	 * Free the platform dependent data...
	 */
	RsEventHandler(rsTERMINATE, nil);

#ifdef _WIN32
	/* 
	 * Free the argv strings...
	 */
	free(argv);
	
	SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &SavedStickyKeys, SPIF_SENDCHANGE);
	SystemParametersInfo(SPI_SETPOWEROFFACTIVE, TRUE, nil, SPIF_SENDCHANGE);
	SystemParametersInfo(SPI_SETLOWPOWERACTIVE, TRUE, nil, SPIF_SENDCHANGE);
	SetErrorMode(0);
#endif

	return 0;
}
/*
 *****************************************************************************
 */

RwV2d leftStickPos;
RwV2d rightStickPos;

void CapturePad(RwInt32 padID)
{
	
	SDL_Joystick *glfwPad = NULL;

	if( padID == 0 )
		glfwPad = PSGLOBAL(joy1);
	else if( padID == 1)
		glfwPad = PSGLOBAL(joy2);
	else
		assert("invalid padID");
	
	if ( glfwPad == NULL )
		return;
	
	ControlsManager.m_NewState.isGamepad = SDL_IsGameController(SDL_JoystickInstanceID(glfwPad));
	
// 	if (ControlsManager.m_bFirstCapture == false) {
// 		memcpy(&ControlsManager.m_OldState, &ControlsManager.m_NewState, sizeof(ControlsManager.m_NewState));
// 	} else {
// 		ControlsManager.m_NewState.mappedButtons[15] = ControlsManager.m_NewState.mappedButtons[16] = 0;
// 	}
// 
// 	if (ControlsManager.m_bFirstCapture == true) {
// 		memcpy(&ControlsManager.m_OldState, &ControlsManager.m_NewState, sizeof(ControlsManager.m_NewState));
// 		
// 		ControlsManager.m_bFirstCapture = false;
// 	}

	RsPadButtonStatus bs;
	bs.padID = padID;
	{
		if (CPad::m_bMapPadOneToPadTwo)
			bs.padID = 1;
		
		RsPadEventHandler(rsPADBUTTONUP,   (void *)&bs);
		RsPadEventHandler(rsPADBUTTONDOWN, (void *)&bs);
	}
	
		
	float lt =  SDL_JoystickGetAxis(glfwPad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / (float)SHRT_MAX;
	float rt = SDL_JoystickGetAxis(glfwPad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / (float)SHRT_MAX;
		
	if (lt != 0.0f)
		ControlsManager.m_NewState.mappedButtons[15] = lt > -0.8f;

	if (rt != 0.0f)
		ControlsManager.m_NewState.mappedButtons[16] = rt > -0.8f;

	leftStickPos.y = SDL_JoystickGetAxis(glfwPad, SDL_CONTROLLER_AXIS_LEFTY) / (float)SHRT_MAX;
		
	leftStickPos.x = SDL_JoystickGetAxis(glfwPad, SDL_CONTROLLER_AXIS_LEFTX) / (float)SHRT_MAX;
 
	rightStickPos.x =  SDL_JoystickGetAxis(glfwPad, SDL_CONTROLLER_AXIS_RIGHTX) / (float)SHRT_MAX;
			
	rightStickPos.y = SDL_JoystickGetAxis(glfwPad, SDL_CONTROLLER_AXIS_RIGHTY) / (float)SHRT_MAX;
		
	{
		if (CPad::m_bMapPadOneToPadTwo)
			bs.padID = 1;
		CPad *pad = CPad::GetPad(bs.padID);

		if ( Abs(leftStickPos.x)  > 0.3f )
			pad->PCTempJoyState.LeftStickX	= (int32)(leftStickPos.x  * 128.0f);
				
		if ( Abs(leftStickPos.y)  > 0.3f )
			pad->PCTempJoyState.LeftStickY	= (int32)(leftStickPos.y  * 128.0f);
				
		if ( Abs(rightStickPos.x) > 0.3f )
			pad->PCTempJoyState.RightStickX = (int32)(rightStickPos.x * 128.0f);

		if ( Abs(rightStickPos.y) > 0.3f )
			pad->PCTempJoyState.RightStickY = (int32)(rightStickPos.y * 128.0f);
	}
	_psHandleVibration();

	return;
}
