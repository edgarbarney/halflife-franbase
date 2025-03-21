//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include<VGUI.h>
#include<VGUI_Panel.h>
#include<VGUI_Dar.h>

namespace vgui
{

class IntChangeSignal;
class Button;
class Slider;

class VGUIAPI ScrollBar : public Panel
{
public:
	ScrollBar(int x,int y,int wide,int tall,bool vertical);
public:
	virtual void    setValue(int value);
	virtual int     getValue();
	virtual void    addIntChangeSignal(IntChangeSignal* s); 
	virtual void    setRange(int min,int max);
	virtual void    setRangeWindow(int rangeWindow);
	virtual void    setRangeWindowEnabled(bool state);
	void    setSize(int wide,int tall) override;
	virtual bool    isVertical();
	virtual bool    hasFullRange();
	virtual void    setButton(Button* button,int index);
	virtual Button* getButton(int index);
	virtual void    setSlider(Slider* slider);
	virtual Slider* getSlider();
	virtual void 	doButtonPressed(int buttonIndex);
	virtual void    setButtonPressedScrollValue(int value);
	virtual void    validate();
public: //bullshit public 
	virtual void fireIntChangeSignal();
protected:
	void performLayout() override;
protected:
	Button* _button[2];
	Slider* _slider;
	Dar<IntChangeSignal*> _intChangeSignalDar;
	int     _buttonPressedScrollValue;
};

}
