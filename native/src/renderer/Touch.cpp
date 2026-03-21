// 1wn_klaymen1n: I really tired of R* coding style,
// anyway i don't think that this project is needed by someone, 
// so let it be in some sort of shitty style :/
// psychobye: its so laggy... -30fps on my device, but i guess it's because not using texture, so whatever
#include "Pad.h"
#include "Rect.h"
#include "common.h"
#include "Font.h"
#include "Pools.h"
#include "Radar.h"
#include "Sprite2d.h"
#include "Touch.h"
#include "Vehicle.h"
#include "Streaming.h"
#include <cmath>
#include <cstdlib>
// #include "android/log.h"

class CPlayerPed;
class CVehicle;
CPlayerPed *FindPlayerPed(void);
CVehicle *FindPlayerVehicle(void);

static void
DrawTouchCircle(float cx, float cy, float radius, const CRGBA &fill, const CRGBA &outline)
{
    const int segments = 12; // maybe its enough for 60fps on mobile?
    const float innerRadius = radius * 0.82f;

    for (int i = 0; i < segments; i++) {
        float a0 = (TWOPI * i) / segments;
        float a1 = (TWOPI * (i + 1)) / segments;
        float x0 = cx + Sin(a0) * radius;
        float y0 = cy - Cos(a0) * radius;
        float x1 = cx + Sin(a1) * radius;
        float y1 = cy - Cos(a1) * radius;
        float ix0 = cx + Sin(a0) * innerRadius;
        float iy0 = cy - Cos(a0) * innerRadius;
        float ix1 = cx + Sin(a1) * innerRadius;
        float iy1 = cy - Cos(a1) * innerRadius;

        CSprite2d::Draw2DPolygon(x0, y0, x1, y1, ix0, iy0, ix1, iy1, outline);
        CSprite2d::Draw2DPolygon(cx, cy, ix0, iy0, cx, cy, ix1, iy1, fill);
    }
}

static void
DrawTouchLabel(float cx, float cy, const char *text, const CRGBA &color, float scale)
{
    wchar wide[16];
    AsciiToUnicode(text, wide);
    CFont::SetBackgroundOff();
    CFont::SetScale(scale, scale * 1.35f);
    CFont::SetCentreOn();
    CFont::SetJustifyOff();
    CFont::SetPropOn();
    CFont::SetFontStyle(FONT_HEADING);
    CFont::SetDropShadowPosition(1);
    CFont::SetDropColor(CRGBA(0, 0, 0, color.a));
    CFont::SetColor(color);
    CFont::PrintString(cx, cy, wide);
}

static bool gTouchUiSpritesLoaded = false;
static CSprite2d gTouchJoyBaseSprite;
static CSprite2d gTouchJoyKnobSprite;
static CSprite2d gTouchExitSprite;
static CSprite2d gTouchFightSprite;
static CSprite2d gTouchSprintSprite;
static CSprite2d gTouchLeftSprite;
static CSprite2d gTouchRightSprite;
static CSprite2d gTouchBrakeSprite;
static CSprite2d gTouchAccelSprite;

static void
LoadTouchSprite(CSprite2d &sprite, const char *relativePath)
{
    sprite.SetTexture(relativePath);
}

static void
EnsureTouchUiSpritesLoaded(void)
{
    if (gTouchUiSpritesLoaded)
        return;

    LoadTouchSprite(gTouchJoyBaseSprite, "mobileui/hud_circle");
    LoadTouchSprite(gTouchJoyKnobSprite, "mobileui/hud_analognub");
    LoadTouchSprite(gTouchExitSprite, "mobileui/hud_car");
    LoadTouchSprite(gTouchFightSprite, "mobileui/punch");
    LoadTouchSprite(gTouchSprintSprite, "mobileui/sprint");
    LoadTouchSprite(gTouchLeftSprite, "mobileui/hud_left");
    LoadTouchSprite(gTouchRightSprite, "mobileui/hud_right");
    LoadTouchSprite(gTouchBrakeSprite, "mobileui/brake");
    LoadTouchSprite(gTouchAccelSprite, "mobileui/accelerate");
    gTouchUiSpritesLoaded = true;
}

static bool
HasTouchSprite(const CSprite2d &sprite)
{
    return sprite.m_pTexture != nil;
}

static void
DrawTouchSprite(CSprite2d &sprite, const CRect &rect, const CRGBA &color)
{
    if (HasTouchSprite(sprite))
        sprite.Draw(rect, color);
}

static float
AndroidRadarTouchLeft(void)
{
    return Max(0.0f, (SCREEN_SCALE_X(RADAR_LEFT) - SCREEN_SCALE_X(10.0f)) / SCREEN_WIDTH);
}

static float
AndroidRadarTouchTop(void)
{
    return Max(0.0f, (SCREEN_SCALE_Y(ANDROID_RADAR_TOP_OFFSET) - SCREEN_SCALE_Y(10.0f)) / SCREEN_HEIGHT);
}

static float
AndroidRadarTouchSizeX(void)
{
    return (SCREEN_SCALE_X(RADAR_WIDTH) + SCREEN_SCALE_X(26.0f)) / SCREEN_WIDTH;
}

static float
AndroidRadarTouchSizeY(void)
{
    return (SCREEN_SCALE_Y(RADAR_HEIGHT) + SCREEN_SCALE_Y(26.0f)) / SCREEN_HEIGHT;
}

static bool
IsDrivingTouchMode(void)
{
    CVehicle *vehicle = FindPlayerVehicle();
    return vehicle != nil && vehicle->pDriver == (CPed*)FindPlayerPed();
}

static bool
IsVehicleActionAvailable(void)
{
    if (IsDrivingTouchMode())
        return true;

    CPlayerPed *player = FindPlayerPed();
    if (player == nil || player->bInVehicle)
        return false;

    const CVector playerPos = player->GetPosition();
    CVehiclePool *vehiclePool = CPools::GetVehiclePool();
    if (vehiclePool == nil)
        return false;

    for (int i = vehiclePool->GetSize() - 1; i >= 0; i--) {
		CVehicle *vehicle = vehiclePool->GetSlot(i);
		if (vehicle == nil || !vehicle->CanPedEnterCar())
			continue;
		if ((vehicle->GetPosition() - playerPos).MagnitudeSqr2D() < 36.0f){
#ifdef ANDROID
			CStreaming::RequestModel(vehicle->GetModelIndex(), STREAMFLAGS_DONT_REMOVE);
#endif
			return true;
		}
	}
	return false;
}

static bool
IsButtonVisible(BtnType type)
{
    bool driving = IsDrivingTouchMode();

    switch (type) {
    case BtnType::LOOK:
    case BtnType::PAUSE:
        return true;
    case BtnType::JOY:
    case BtnType::STICK:
        return !driving;
    case BtnType::CAR_LEFT:
    case BtnType::CAR_RIGHT:
    case BtnType::CAR_BRAKE:
    case BtnType::CAR_ACCEL:
        return driving;
    case BtnType::CAR:
        return IsVehicleActionAvailable();
    case BtnType::FIGHT:
    case BtnType::JUMP:
        return !driving;
    default:
        return true;
    }
}

Btn::Btn(float x1, float y1, float x2, float y2, BtnType type = BtnType::DEFAULT, CRGBA color = CRGBA(255, 255, 255, 255), bool preserveAspect = true)
    :x1(x1), y1(y1), x2(x2), y2(y2), type(type), color(color), preserveAspect(preserveAspect)
{
    y2 = y1 + (x2 - x1) * (SCREEN_WIDTH / SCREEN_HEIGHT);
    rect = makeBtnRect(x1, y1, x2, y2);
}

void CTouch::Init()
{
    touchButtons.clear();
    
    stickBtn = nullptr;
    joyBtn = nullptr; //Clean this in case of re-init
    
    touchButtons.emplace_back(0.5f, 0.0f, 1.0f, 1.0f, BtnType::LOOK, CRGBA(255, 0, 0, 255), false);
    touchButtons.emplace_back(0.09f, 0.56f, 0.25f, 0.72f, BtnType::JOY,   CRGBA(255, 255, 255, 255));
    touchButtons.emplace_back(0.0f, 0.0f, 0.07f, 0.07f, BtnType::STICK, CRGBA(0,   0,   255, 255));
    touchButtons.emplace_back(0.060f, 0.770f, 0.140f, 0.850f, BtnType::CAR_LEFT,  CRGBA(255, 255, 255, 255));
    touchButtons.emplace_back(0.170f, 0.770f, 0.250f, 0.850f, BtnType::CAR_RIGHT, CRGBA(255, 255, 255, 255));
    touchButtons.emplace_back(0.910f, 0.435f, 0.990f, 0.515f, BtnType::CAR,   CRGBA(255, 255, 255, 255));
    touchButtons.emplace_back(0.810f, 0.770f, 0.890f, 0.850f, BtnType::CAR_BRAKE, CRGBA(255, 255, 255, 255));
    touchButtons.emplace_back(0.910f, 0.770f, 0.990f, 0.850f, BtnType::CAR_ACCEL, CRGBA(255, 255, 255, 255));
    touchButtons.emplace_back(0.910f, 0.635f, 0.990f, 0.715f, BtnType::FIGHT, CRGBA(255, 255, 255, 255));
    touchButtons.emplace_back(0.910f, 0.790f, 0.990f, 0.870f, BtnType::JUMP,  CRGBA(255, 255, 255, 255));
    touchButtons.emplace_back(AndroidRadarTouchLeft(), AndroidRadarTouchTop(),
        AndroidRadarTouchLeft() + AndroidRadarTouchSizeX(), AndroidRadarTouchTop() + AndroidRadarTouchSizeY(),
        BtnType::PAUSE, CRGBA(255, 255, 255, 255), false);
    
    for (Btn& btn : touchButtons) {
        if (btn.type == BtnType::STICK) stickBtn = &btn;
        else if(btn.type == BtnType::JOY) joyBtn = &btn;
    }
    move_finger = look_finger = -1;
    moveAxisX = moveAxisY = 0;
    lookAxisX = lookAxisY = 0;
}

void CTouch::Draw()
{
    EnsureTouchUiSpritesLoaded();
    if(!touchButtons.empty())
    {
        for(Btn& btn: touchButtons)
        {
            btn.wasTouchedLastFrame = btn.touched;
            checkFinger(btn);
            btn.updateRect();
            if(!IsButtonVisible(btn.type))
                continue;
            if(btn.type != BtnType::STICK)
            {
                if(btn.touched)
                    btn.color = CRGBA(92, 187, 255, 210);
                else
                    btn.color = CRGBA(255, 255, 255, 120);
            }
            if(btn.type == BtnType::LOOK || btn.type == BtnType::PAUSE)
                continue;

            float cx = (btn.rect.left + btn.rect.right) * 0.5f;
            float cy = (btn.rect.top + btn.rect.bottom) * 0.5f;
            float radius = Min(btn.rect.right - btn.rect.left, btn.rect.bottom - btn.rect.top) * 0.5f;

            if (btn.type == BtnType::JOY) {
                if (move_finger == -1)
                    continue;
                if (HasTouchSprite(gTouchJoyBaseSprite))
                    DrawTouchSprite(gTouchJoyBaseSprite, btn.rect, CRGBA(255, 255, 255, btn.touched ? 210 : 170));
                else
                    DrawTouchCircle(cx, cy, radius * 1.05f, CRGBA(18, 22, 30, 90), CRGBA(255, 255, 255, 105));
            } else if (btn.type == BtnType::STICK) {
                if (move_finger == -1)
                    continue;
                if (HasTouchSprite(gTouchJoyKnobSprite)) {
                    float knobInsetX = (btn.rect.right - btn.rect.left) * 0.18f;
                    float knobInsetY = (btn.rect.bottom - btn.rect.top) * 0.18f;
                    CRect knobRect(
                        btn.rect.left + knobInsetX,
                        btn.rect.top + knobInsetY,
                        btn.rect.right - knobInsetX,
                        btn.rect.bottom - knobInsetY
                    );
                    DrawTouchSprite(gTouchJoyKnobSprite, knobRect, CRGBA(255, 255, 255, btn.touched ? 235 : 195));
                } else
                    DrawTouchCircle(cx, cy, radius, CRGBA(92, 187, 255, btn.touched ? 220 : 170), CRGBA(255, 255, 255, 140));
            } else {
                CSprite2d *sprite = nil;
                switch (btn.type) {
                case BtnType::JUMP:      sprite = &gTouchSprintSprite; break;
                case BtnType::FIGHT:     sprite = &gTouchFightSprite; break;
                case BtnType::CAR:       sprite = &gTouchExitSprite; break;
                case BtnType::CAR_LEFT:  sprite = &gTouchLeftSprite; break;
                case BtnType::CAR_RIGHT: sprite = &gTouchRightSprite; break;
                case BtnType::CAR_BRAKE: sprite = &gTouchBrakeSprite; break;
                case BtnType::CAR_ACCEL: sprite = &gTouchAccelSprite; break;
                default: break;
                }

                if (sprite != nil && HasTouchSprite(*sprite)) {
                    CRect drawRect = btn.rect;
                    float insetX = (btn.rect.right - btn.rect.left) * 0.06f;
                    float insetY = (btn.rect.bottom - btn.rect.top) * 0.06f;
                    drawRect.left += insetX;
                    drawRect.right -= insetX;
                    drawRect.top += insetY;
                    drawRect.bottom -= insetY;

                    if ((btn.type == BtnType::JUMP) && HasTouchSprite(gTouchJoyBaseSprite)) {
                        DrawTouchSprite(gTouchJoyBaseSprite, drawRect, CRGBA(255, 255, 255, btn.touched ? 200 : 155));
                        float fgInsetX = (drawRect.right - drawRect.left) * 0.08f;
                        float fgInsetY = (drawRect.bottom - drawRect.top) * 0.08f;
                        drawRect.left += fgInsetX;
                        drawRect.right -= fgInsetX;
                        drawRect.top += fgInsetY;
                        drawRect.bottom -= fgInsetY;
                    }
                    DrawTouchSprite(*sprite, drawRect, CRGBA(255, 255, 255, btn.touched ? 230 : 185));
                } else {
                    DrawTouchCircle(cx, cy, radius, CRGBA(18, 22, 30, btn.touched ? 180 : 120), btn.color);
                    if (btn.type == BtnType::JUMP)
                        DrawTouchLabel(cx, cy + radius * 0.18f, "J", CRGBA(255, 255, 255, 230), SCREEN_SCALE_X(0.55f));
                    else if (btn.type == BtnType::FIGHT)
                        DrawTouchLabel(cx, cy + radius * 0.18f, "F", CRGBA(255, 255, 255, 230), SCREEN_SCALE_X(0.55f));
                    else if (btn.type == BtnType::CAR)
                        DrawTouchLabel(cx, cy + radius * 0.18f, "E", CRGBA(255, 255, 255, 230), SCREEN_SCALE_X(0.55f));
                    else if (btn.type == BtnType::CAR_LEFT)
                        DrawTouchLabel(cx, cy + radius * 0.18f, "<", CRGBA(255, 255, 255, 230), SCREEN_SCALE_X(0.55f));
                    else if (btn.type == BtnType::CAR_RIGHT)
                        DrawTouchLabel(cx, cy + radius * 0.18f, ">", CRGBA(255, 255, 255, 230), SCREEN_SCALE_X(0.55f));
                    else if (btn.type == BtnType::CAR_BRAKE)
                        DrawTouchLabel(cx, cy + radius * 0.18f, "-", CRGBA(255, 255, 255, 230), SCREEN_SCALE_X(0.55f));
                    else if (btn.type == BtnType::CAR_ACCEL)
                        DrawTouchLabel(cx, cy + radius * 0.18f, "+", CRGBA(255, 255, 255, 230), SCREEN_SCALE_X(0.55f));
                    }
            }
        }
        UpdateStick();
        UpdateLook();
    }
}

void CTouch::UpdateLook()
{
    bool driving = IsDrivingTouchMode();
    float activeSmoothing = driving ? 0.42f : 0.36f;
    float releaseSmoothing = driving ? 0.46f : 0.28f;
    if(look_finger != -1 && touchInfo[look_finger].pressed)
    {
        float normX = touchInfo[look_finger].dx / Max(1.0f, float(SCREEN_WIDTH));
        float normY = touchInfo[look_finger].dy / Max(1.0f, float(SCREEN_HEIGHT));
        float lookX = normX * (driving ? 260000.0f : 210000.0f);
        float lookY = normY * (driving ? 110000.0f : 125000.0f);
        float deadzone = driving ? 0.18f : 0.10f;

        if(Abs(lookX) < deadzone)
            lookX = 0.0f;
        if(Abs(lookY) < deadzone)
            lookY = 0.0f;

        if(lookX != 0.0f){
            float xdir = lookX < 0.0f ? -1.0f : 1.0f;
            float absX = Abs(lookX);
            if (driving)
                lookX = xdir * Min(127.0f, absX * 2.10f + Pow(absX, 1.03f) * 0.24f);
            else
                lookX = xdir * Min(127.0f, absX * 1.55f + Pow(absX, 1.16f) * 0.11f);
        }
        if(lookY != 0.0f){
            float ydir = lookY < 0.0f ? -1.0f : 1.0f;
            float absY = Abs(lookY);
            if (driving)
                lookY = ydir * Min(92.0f, absY * 1.05f + Pow(absY, 1.02f) * 0.08f);
            else
                lookY = ydir * Min(108.0f, absY * 1.05f + Pow(absY, 1.14f) * 0.08f);
        }

        smoothedLookX = smoothedLookX * (1.0f - activeSmoothing) + lookX * activeSmoothing;
        smoothedLookY = smoothedLookY * (1.0f - activeSmoothing) + lookY * activeSmoothing;

        // SDL touch motion keeps the last non-zero delta until the next motion event.
        // Consume it once so holding a finger still does not keep rotating the camera.
        touchInfo[look_finger].dx = 0.0f;
        touchInfo[look_finger].dy = 0.0f;
    }
    else
    {
        look_finger = -1;
        lookAxisX = lookAxisY = 0;
        smoothedLookX += (0.0f - smoothedLookX) * releaseSmoothing;
        smoothedLookY += (0.0f - smoothedLookY) * releaseSmoothing;
    }
    
    lookAxisX = Clamp((int32)smoothedLookX, -127, 127);
    lookAxisY = Clamp((int32)smoothedLookY, -127, 127);
}

void CTouch::UpdateStick()
{
    if (!stickBtn || !joyBtn) // I lost my mind
        return;

    if (IsDrivingTouchMode()) {
        move_finger = -1;
        stickBtn->setCenter(joyBtn->getCenter());
        moveAxisX = moveAxisY = 0;
        return;
    }

    float stickX;
    float stickY;
    
    float joyCenterX = (joyBtn->rect.left + joyBtn->rect.right) / 2.0f;
    float joyCenterY = (joyBtn->rect.top + joyBtn->rect.bottom) / 2.0f;
    
    if(move_finger == -1)
    {
        stickBtn->setCenter(joyBtn->getCenter());
        moveAxisX = moveAxisY = 0;
        return;
    }
    if (!touchInfo[move_finger].pressed)
    {   
        move_finger = -1;
        return;
    }
    else
    {
        float dx = touchInfo[move_finger].x - joyCenterX;
        float dy = touchInfo[move_finger].y - joyCenterY;
        float distance = std::sqrt(dx * dx + dy * dy);
    
        const float STICK_RADIUS = 150.0f;
    
        if (distance > STICK_RADIUS) {
            float scale = STICK_RADIUS / distance;
            dx *= scale;
            dy *= scale;
        }
    
        float newStickX = joyCenterX + dx;
        float newStickY = joyCenterY + dy;
        
        float aspect   = SCREEN_WIDTH / SCREEN_HEIGHT;
        float virtualW = SCREEN_HEIGHT * aspect;
        float offsetX  = (SCREEN_WIDTH - virtualW) * 0.5f;

        float normX = (newStickX - offsetX) / virtualW;
        
        stickBtn->setCenter((newStickX - offsetX) / virtualW,  newStickY / SCREEN_HEIGHT);
        
        auto stickCenter = stickBtn->getCenter();
        float stickCenterX = stickCenter.first * SCREEN_WIDTH;
        float stickCenterY = stickCenter.second * SCREEN_HEIGHT;

        
        float moveX = (joyCenterX - stickCenterX) / STICK_RADIUS;
        float moveY = (joyCenterY - stickCenterY) / STICK_RADIUS;
        
        moveAxisX = -moveX * 128;
        moveAxisY = -moveY * 128;
    }
}

//Remember this!
// float left;     // x min
// float bottom;   // y max
// float right;    // x max
// float top;      // y min
void CTouch::checkFinger(Btn& btn)
{
    btn.touched = false;
    if(!IsButtonVisible(btn.type))
        return;
    for(int i = 0; i < 10; i++)
    {
        const TouchInfo &touch = touchInfo[i];
        if(!touch.pressed)
            continue;
        
        if(touch.x >= btn.rect.left && touch.x <= btn.rect.right &&
           touch.y >= btn.rect.top && touch.y <= btn.rect.bottom)
        {
            if(btn.type == BtnType::LOOK)
            {
                bool overlapsActionButton = false;
                for(const Btn &other : touchButtons)
                {
                    if(other.type == BtnType::LOOK || !IsButtonVisible(other.type))
                        continue;
                    if(touch.x >= other.rect.left && touch.x <= other.rect.right &&
                       touch.y >= other.rect.top && touch.y <= other.rect.bottom){
                        overlapsActionButton = true;
                        break;
                    }
                }
                if(overlapsActionButton)
                    continue;
            }
            if(btn.type == BtnType::STICK || btn.type == BtnType::JOY)
            {
                btn.touched = true;
                btn.fingerID = i;
                move_finger = i;
                break;
            }
            else if(btn.type == BtnType::LOOK)
            {
                btn.touched = true;
                btn.fingerID = i;
                look_finger = i;
                break;
            }
            btn.touched = true;
            btn.fingerID = i;
            break;
        }
    }
}
