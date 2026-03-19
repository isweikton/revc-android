#include <jni.h>
#include <unistd.h>
#include <SDL_main.h>
#include <SDL_hints.h>
#include <SDL_system.h>
#include <android/log.h>
#include "android.h"
#include "common.h"
#include "Clock.h"
#include "renderer/Weather.h"
#include "PlayerInfo.h"
#include "PlayerPed.h"
#include "Streaming.h"
#include "ModelInfo.h"
#include "Frontend.h"
#include "General.h"
#include "Renderer.h"
#include "PostFX.h"
#include "IniFile.h"
#include "WaterLevel.h"
#define JNI_WRAPPER extern "C" JNIEXPORT
#define JNIWRAPPER JNI_WRAPPER
FILE* logfile = nullptr;

extern void InitCrashHandler();
extern void AndroidShowSettingsMenu();

extern "C" JNIEXPORT void JNICALL
Java_com_wn_1klaymen1n_revc_LauncherActivity_setenv(JNIEnv *env, jobject obj, jstring value)
{
    __android_log_print(ANDROID_LOG_DEBUG,"REVC-DEBUG", "Java_com_wn_1klaymen1n_revc_LauncherActivity_setenv %s",env->GetStringUTFChars(value, NULL));
    const char *gameFiles = env->GetStringUTFChars(value, NULL);
    setenv("GAMEFILES", gameFiles, 1);
    chdir(gameFiles);
    env->ReleaseStringUTFChars(value, gameFiles);
}

JNI_WRAPPER int LaunchAndroid(){
    InitCrashHandler();
    char logPath[256];
    snprintf(logPath, sizeof(logPath), "%s/gamelog.txt", getenv("GAMEFILES"));
    logfile = fopen(logPath, "w+");
    setvbuf(logfile, nullptr, _IONBF, 0);
    __android_log_print(ANDROID_LOG_DEBUG, "REVC", "TRYING TO START");
    int argc = 0;
    char *argv[1] = { nullptr };
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
int result = SDL_main(argc, argv);
    return result;
}

JNIWRAPPER void
Java_com_wn_1klaymen1n_revc_NativeBridge_setTime(JNIEnv*, jobject, jint hour, jint minute)
{
	uint8 h = (uint8)(hour < 0 ? 0 : hour % 24);
	uint8 m = (uint8)(minute < 0 ? 0 : minute % 60);
	CClock::SetGameClock(h, m);
}

JNIWRAPPER void
Java_com_wn_1klaymen1n_revc_NativeBridge_setWeather(JNIEnv*, jobject, jint weatherId)
{
	int16 id = (int16)Clamp(weatherId, 0, WEATHER_TOTAL - 1);
	CWeather::ForcedWeatherType = id;
	CWeather::OldWeatherType = id;
	CWeather::NewWeatherType = id;
	CWeather::WeatherTypeInList = id;
	CWeather::ForceWeatherNow(id);
}

JNIWRAPPER jboolean
Java_com_wn_1klaymen1n_revc_NativeBridge_setPlayerModel(JNIEnv*, jobject, jint modelId)
{
	CPlayerPed *ped = FindPlayerPed();
	if (ped == nil)
		return JNI_FALSE;
	int32 id = Clamp(modelId, 0, MODELINFOSIZE - 1);
	CStreaming::RequestModel(id, STREAMFLAGS_DONT_REMOVE);
	CStreaming::LoadAllRequestedModels(false);
	ped->SetModelIndex(id);
	return JNI_TRUE;
}

JNIWRAPPER void
Java_com_wn_1klaymen1n_revc_NativeBridge_setBrightness(JNIEnv*, jobject, jint value)
{
	int v = Clamp(value, 32, 512);
	FrontEndMenuManager.m_PrefsBrightness = v;
	CDraw::SetFOV(FrontEndMenuManager.m_PrefsBrightness); // force redraw path for some devices
}

JNIWRAPPER void
Java_com_wn_1klaymen1n_revc_NativeBridge_setMaxFps(JNIEnv*, jobject, jint fps)
{
	int target = Clamp(fps, 30, ANDROID_MAX_FPS_CAP);
	RsGlobal.maxFPS = target;
	FrontEndMenuManager.m_PrefsFrameLimiter = 0;
}

JNIWRAPPER void
Java_com_wn_1klaymen1n_revc_NativeBridge_setDrawDistance(JNIEnv*, jobject, jfloat lod)
{
	float clamped = Clamp(lod, 0.7f, 2.2f);
	FrontEndMenuManager.m_PrefsLOD = clamped;
	CRenderer::ms_lodDistScale = clamped;
}

JNIWRAPPER void
Java_com_wn_1klaymen1n_revc_NativeBridge_setDensity(JNIEnv*, jobject, jfloat ped, jfloat car)
{
	CIniFile::PedNumberMultiplier = Clamp(ped, 0.2f, 1.2f);
	CIniFile::CarNumberMultiplier = Clamp(car, 0.2f, 1.2f);
}

JNIWRAPPER void
Java_com_wn_1klaymen1n_revc_NativeBridge_togglePostFx(JNIEnv*, jobject, jboolean enabled)
{
	if (enabled) {
		CPostFX::EffectSwitch = CPostFX::POSTFX_SIMPLE;
		CPostFX::BlurOn = false;
	} else {
		CPostFX::EffectSwitch = CPostFX::POSTFX_OFF;
		CPostFX::BlurOn = false;
	}
}

JNIWRAPPER void
Java_com_wn_1klaymen1n_revc_NativeBridge_toggleWater(JNIEnv*, jobject, jboolean enabled)
{
	gbDontRenderWater = !enabled;
}

void AndroidShowSettingsMenu()
{
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) {
        return;
    }
    jclass sdlActivityClass = env->GetObjectClass(activity);
    if (sdlActivityClass == nullptr) {
        env->DeleteLocalRef(activity);
        return;
    }
    jmethodID openMethod = env->GetStaticMethodID(
        sdlActivityClass,
        "openSettingsFromNative",
        "()V");
    if (openMethod != nullptr) {
        env->CallStaticVoidMethod(sdlActivityClass, openMethod);
    }
    env->DeleteLocalRef(sdlActivityClass);
    env->DeleteLocalRef(activity);
}
void AndroidSetLoadingOverlay(const char *title, const char *subtitle, bool visible)
{
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());
    if (env == nullptr || activity == nullptr) {
        return;
    }

    jclass activityClass = env->GetObjectClass(activity);
    if (activityClass == nullptr) {
        env->DeleteLocalRef(activity);
        return;
    }

    jmethodID method = env->GetStaticMethodID(
        activityClass,
        "setNativeLoadingOverlay",
        "(Ljava/lang/String;Ljava/lang/String;Z)V");

    if (method != nullptr) {
        jstring jTitle = title ? env->NewStringUTF(title) : nullptr;
        jstring jSubtitle = subtitle ? env->NewStringUTF(subtitle) : nullptr;
        env->CallStaticVoidMethod(activityClass, method, jTitle, jSubtitle, visible ? JNI_TRUE : JNI_FALSE);
        if (jTitle) env->DeleteLocalRef(jTitle);
        if (jSubtitle) env->DeleteLocalRef(jSubtitle);
    }

    env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(activity);
}
