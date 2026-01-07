// 1sh0zer: I really tired of R* coding style, 
// anyway i don't think that this project is needed by someone, 
// so let it be in some sort of shitty style :/
#include "Pad.h"
#include "Rect.h"
#include "common.h"
#include "Sprite2d.h"
#include "Touch.h"
#include <cmath>
// #include "android/log.h"

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
    touchButtons.emplace_back(0.15f, 0.60f, 0.3f, 0.70f, BtnType::JOY,   CRGBA(255, 255, 255, 255));
    touchButtons.emplace_back(0.0f, 0.0f, 0.05f, 0.05f, BtnType::STICK, CRGBA(0,   0,   255, 255));
    touchButtons.emplace_back(0.70f, 0.60f, 0.75f, 0.65f, BtnType::JUMP,   CRGBA(255, 255, 255, 255));
    touchButtons.emplace_back(0.85f, 0.75f, 0.90f, 0.80f, BtnType::CAR,  CRGBA(255, 255, 255, 255));
    
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
    if(!touchButtons.empty())
    {
        for(Btn& btn: touchButtons)
        {
            btn.wasTouchedLastFrame = btn.touched;
            checkFinger(btn);
            btn.updateRect();
            if(btn.type != BtnType::STICK)
            {
                if(btn.touched)
                    btn.color = CRGBA(0, 0, 255, 255);
                else
                    btn.color = CRGBA(255, 255, 255, 255);
            }
            if(btn.type != BtnType::LOOK)
                CSprite2d::DrawRect(btn.rect, btn.color);
        }
        UpdateStick();
        UpdateLook();
    }
}

void CTouch::UpdateLook()
{
    float smoothing = 0.2f;
    if(look_finger != -1 && touchInfo[look_finger].pressed)
    {
        float lookX = touchInfo[look_finger].dx * 15;  //this magic numbers here cause i'm a wizard
        float lookY = touchInfo[look_finger].dy * 15;

    
        smoothedLookX = smoothedLookX * (1.0f - smoothing) + lookX * smoothing;
        smoothedLookY = smoothedLookY * (1.0f - smoothing) + lookY * smoothing;
    }
    else
    {
        look_finger = -1;
        lookAxisX = lookAxisY = 0;
        smoothedLookX += (0.0f - smoothedLookX) * smoothing;
        smoothedLookY += (0.0f - smoothedLookY) * smoothing;
    }
    
    lookAxisX = smoothedLookX;
    lookAxisY = -smoothedLookY;
}

void CTouch::UpdateStick()
{
    if (!stickBtn || !joyBtn) // I lost my mind
        return;

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
    for(int i = 0; i < 10; i++)
    {
        const TouchInfo &touch = touchInfo[i];
        if(!touch.pressed)
            continue;
        
        if(touch.x >= btn.rect.left && touch.x <= btn.rect.right &&
           touch.y >= btn.rect.top && touch.y <= btn.rect.bottom)
        {
            if(btn.type == BtnType::STICK || btn.type == BtnType::JOY)
            {
                move_finger = i;
                break;
            }
            else if(btn.type == BtnType::LOOK)
            {
                look_finger = i;
                break;
            }
            btn.touched = true;
            btn.fingerID = i;
            break;
        }
    }
}
