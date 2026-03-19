#pragma once
#include "common.h"
#include <vector>

#ifdef ANDROID
void AndroidShowSettingsMenu();
#endif

enum BtnType{
    DEFAULT,
    STICK,
    JOY,
    CAR,
    CAR_LEFT,
    CAR_RIGHT,
    CAR_BRAKE,
    CAR_ACCEL,
    JUMP,
    FIGHT,
    LOOK,
    PAUSE
};

class Btn
{
public:
    Btn(float x1, float y1, float x2, float y2, BtnType type, CRGBA color, bool preserveAspect);
    float x1, y1, x2, y2;
    CRect rect;
    CRGBA color;
    BtnType type;
    bool preserveAspect;
    bool touched = false;
    int fingerID;
    bool wasTouchedLastFrame;
    
    CRect makeBtnRect(float normX1, float normY1, float normX2, float normY2) {
        float aspect = SCREEN_WIDTH / SCREEN_HEIGHT;
        float virtualW = SCREEN_HEIGHT * aspect;
        float offsetX = (SCREEN_WIDTH - virtualW) * 0.5f;

        if(preserveAspect)
             y2 = y1 + (x2 - x1) * (SCREEN_WIDTH / float(SCREEN_HEIGHT));
        
        float x1v = normX1 * virtualW;
        float x2v = normX2 * virtualW;
        float y1v = normY1 * SCREEN_HEIGHT;
        float y2v = normY2 * SCREEN_HEIGHT;

        return CRect(offsetX + x1v, y1v, offsetX + x2v,y2v);
    }
    
    std::pair<float,float>getCenter(){
        float x = x1 + (x2 - x1) / 2;
        float y = y1 + (y2 - y1) / 2;
        return std::make_pair(x, y);
    }
    
    void setCenter(float x, float y){
        float halfX = (x2 - x1) / 2;
        float halfY = (y2 - y1) / 2;
        
        x1 = x - halfX;
        x2 = x + halfX;
        y1 = y - halfY;
        y2 = y + halfY;
    }
    void setCenter(const std::pair<float, float> &center)
    {
        setCenter(center.first, center.second);
    }
    
    void updateRect(){rect = makeBtnRect(x1, y1, x2, y2);}
};

class CTouch
{
public:
    void Init();
    void Draw();
    void UpdateStick();
    void UpdateLook();
    void checkFinger(Btn& btn);
    void updateButton(Btn& btn);
    
    bool getButtonJustDown(BtnType type){
        for(const Btn& btn : touchButtons)
        {
            if(btn.type == type && btn.touched && !btn.wasTouchedLastFrame)
            {
#ifdef ANDROID
                if (type == BtnType::PAUSE) {
                    AndroidShowSettingsMenu();
                    return false;
                }
#endif
                return true;
            }
        }
        return false;
    }
    
    bool getButton(BtnType type){
        for(const Btn& btn : touchButtons)
        {
            if(btn.type == type && btn.touched)
                return true;
        }
        return false;
    }
    
    int16 moveAxisX, moveAxisY;
    int16 lookAxisX, lookAxisY; //Oh god..
    float smoothedLookX = 0.0f;
    float smoothedLookY = 0.0f;

private:
    std::vector<Btn> touchButtons;
    Btn* stickBtn = nullptr;
    Btn* joyBtn = nullptr;
    
    int move_finger, look_finger; //five fingers in my ass
};
