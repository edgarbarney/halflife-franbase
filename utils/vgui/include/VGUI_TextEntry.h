//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#pragma once

#include<VGUI.h>
#include<VGUI_Panel.h>
#include<VGUI_InputSignal.h>

namespace vgui
{

enum MouseCode;
enum KeyCode;
class ActionSignal;

class VGUIAPI TextEntry : public Panel , public InputSignal
{
public:
	TextEntry(const char* text,int x,int y,int wide,int tall);
public:
	virtual void setText(const char* text,int textLen);
	virtual void getText(int offset,char* buf,int bufLen);
	virtual void resetCursorBlink();
	virtual void doGotoLeft();
	virtual void doGotoRight();
	virtual void doGotoFirstOfLine();
	virtual void doGotoEndOfLine();
	virtual void doInsertChar(char ch);
	virtual void doBackspace();
	virtual void doDelete();
	virtual void doSelectNone();
	virtual void doCopySelected();
	virtual void doPaste();
	virtual void doPasteSelected();
	virtual void doDeleteSelected();
	virtual void addActionSignal(ActionSignal* s);
	virtual void setFont(Font* font);
	virtual void setTextHidden(bool bHideText);
protected:
	void paintBackground() override;
	virtual void setCharAt(char ch,int index);
protected:
	virtual void fireActionSignal();
	virtual bool getSelectedRange(int& cx0,int& cx1);
	virtual bool getSelectedPixelRange(int& cx0,int& cx1);
	virtual int  cursorToPixelSpace(int cursorPos);
	virtual void selectCheck();
protected: //InputSignal
	void cursorMoved(int x,int y,Panel* panel) override;
	void cursorEntered(Panel* panel) override;
	void cursorExited(Panel* panel) override;
	void mousePressed(MouseCode code,Panel* panel) override;
	void mouseDoublePressed(MouseCode code,Panel* panel) override;
	void mouseReleased(MouseCode code,Panel* panel) override;
	void mouseWheeled(int delta,Panel* panel) override;
	void keyPressed(KeyCode code,Panel* panel) override;
	void keyTyped(KeyCode code,Panel* panel) override;
	void keyReleased(KeyCode code,Panel* panel) override; 
	void keyFocusTicked(Panel* panel) override;
protected:
	Dar<char>          _lineDar;
	int                _cursorPos;
	bool               _cursorBlink;
	bool               _hideText;
	long               _cursorNextBlinkTime;
	int                _cursorBlinkRate;
	int                _select[2];
	Dar<ActionSignal*> _actionSignalDar;
	Font*              _font;
};

}
