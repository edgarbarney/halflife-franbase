
#pragma once

#include <VGUI_Panel.h>
#include <VGUI_Frame.h>
#include <VGUI_TextPanel.h>
#include <VGUI_Label.h>
#include <VGUI_Button.h>
#include <VGUI_ActionSignal.h>
#include <VGUI_InputSignal.h>
#include <VGUI_Scheme.h>
#include <VGUI_Image.h>
#include <VGUI_FileInputStream.h>
#include <VGUI_BitmapTGA.h>
#include <VGUI_DesktopIcon.h>
#include <VGUI_App.h>
#include <VGUI_MiniApp.h>
#include <VGUI_LineBorder.h>
#include <VGUI_String.h>
#include <VGUI_ScrollPanel.h>
#include <VGUI_ScrollBar.h>
#include <VGUI_Slider.h>

// custom scheme handling
#include "vgui_SchemeManager.h"

#define TF_DEFS_ONLY
#define PC_RANDOM 10
#define PC_LASTCLASS 10
#define PC_UNDEFINED 0
#define MENU_DEFAULT 1
#define MENU_TEAM 2
#define MENU_CLASS 3
#define MENU_MAPBRIEFING 4
#define MENU_INTRO 5
#define MENU_CLASSHELP 6
#define MENU_CLASSHELP2 7
#define MENU_REPEATHELP 8
#define MENU_SPECHELP 9
#define MENU_CUSTOM 10 //AJH New Customizable menu HUD system
using namespace vgui;

class Cursor;
class ScorePanel;
class SpectatorPanel;
class CCommandMenu;
class CommandLabel;
class CommandButton;
class BuildButton;
class ClassButton;
class CMenuPanel;
class DragNDropPanel;
class CTransparentPanel;
class CClassMenuPanel;
class CTeamMenuPanel;
class CCustomMenu; //AJH new customizable menu system
class TeamFortressViewport;

char* GetVGUITGAName(const char* pszName);
BitmapTGA* LoadTGAForRes(const char* pImageName);
void ScaleColors(int& r, int& g, int& b, int a);
extern const char* sTFClassSelection[];
extern const char* sInventorySelection[]; // AJH Inventory selection system
extern int sTFValidClassInts[];
extern const char* sLocalisedClasses[];
extern const char* sLocalisedInventory[]; // AJH Inventory selection system
extern int iTeamColors[5][3];
extern int iNumberOfTeamColors;
extern TeamFortressViewport* gViewPort;


// Command Menu positions
#define MAX_MENUS 80
#define MAX_BUTTONS 100

#define BUTTON_SIZE_Y YRES(30)
#define CMENU_SIZE_X XRES(160)

#define SUBMENU_SIZE_X (CMENU_SIZE_X / 8)
#define SUBMENU_SIZE_Y (BUTTON_SIZE_Y / 6)

#define CMENU_TOP (BUTTON_SIZE_Y * 4)

//#define MAX_TEAMNAME_SIZE		64
#define MAX_BUTTON_SIZE 32

// Map Briefing Window
#define MAPBRIEF_INDENT 30

// Team Menu
#define TMENU_INDENT_X (30 * ((float)ScreenHeight / 640))
#define TMENU_HEADER 100
#define TMENU_SIZE_X (ScreenWidth - (TMENU_INDENT_X * 2))
#define TMENU_SIZE_Y (TMENU_HEADER + BUTTON_SIZE_Y * 7)
#define TMENU_PLAYER_INDENT (((float)TMENU_SIZE_X / 3) * 2)
#define TMENU_INDENT_Y (((float)ScreenHeight - TMENU_SIZE_Y) / 2)

// Class Menu
#define CLMENU_INDENT_X (30 * ((float)ScreenHeight / 640))
#define CLMENU_HEADER 100
#define CLMENU_SIZE_X (ScreenWidth - (CLMENU_INDENT_X * 2))
#define CLMENU_SIZE_Y (CLMENU_HEADER + BUTTON_SIZE_Y * 11)
#define CLMENU_PLAYER_INDENT (((float)CLMENU_SIZE_X / 3) * 2)
#define CLMENU_INDENT_Y (((float)ScreenHeight - CLMENU_SIZE_Y) / 2)

// Arrows
enum
{
	ARROW_UP,
	ARROW_DOWN,
	ARROW_LEFT,
	ARROW_RIGHT,
};

//==============================================================================
// VIEWPORT PIECES
//============================================================
// Wrapper for an Image Label without a background
class CImageLabel : public Label
{
public:
	BitmapTGA* m_pTGA;

public:
	void LoadImage(const char* pImageName);
	CImageLabel(const char* pImageName, int x, int y);
	CImageLabel(const char* pImageName, int x, int y, int wide, int tall);

	virtual int getImageTall();
	virtual int getImageWide();

	void paintBackground() override
	{
		// Do nothing, so the background's left transparent.
	}
};

// Command Label
// Overridden label so we can darken it when submenus open
class CommandLabel : public Label
{
private:
	int m_iState;

public:
	CommandLabel(const char* text, int x, int y, int wide, int tall) : Label(text, x, y, wide, tall)
	{
		m_iState = 0;
	}

	void PushUp()
	{
		m_iState = 0;
		repaint();
	}

	void PushDown()
	{
		m_iState = 1;
		repaint();
	}
};

//============================================================
// Command Buttons
class CommandButton : public Button
{
private:
	int m_iPlayerClass;
	bool m_bFlat;

	// Submenus under this button
	CCommandMenu* m_pSubMenu;
	CCommandMenu* m_pParentMenu;
	CommandLabel* m_pSubLabel;

	char m_sMainText[MAX_BUTTON_SIZE];
	char m_cBoundKey;

	SchemeHandle_t m_hTextScheme;

	void RecalculateText();

public:
	bool m_bNoHighlight;

public:
	CommandButton(const char* text, int x, int y, int wide, int tall, bool bNoHighlight, bool bFlat);
	// Constructors
	CommandButton(const char* text, int x, int y, int wide, int tall, bool bNoHighlight = false);
	CommandButton(int iPlayerClass, const char* text, int x, int y, int wide, int tall, bool bFlat);

	void Init();

	// Menu Handling
	void AddSubMenu(CCommandMenu* pNewMenu);
	void AddSubLabel(CommandLabel* pSubLabel)
	{
		m_pSubLabel = pSubLabel;
	}

	virtual bool IsNotValid()
	{
		return false;
	}

	void UpdateSubMenus(int iAdjustment);
	int GetPlayerClass() { return m_iPlayerClass; };
	CCommandMenu* GetSubMenu() { return m_pSubMenu; };

	CCommandMenu* getParentMenu();
	void setParentMenu(CCommandMenu* pParentMenu);

	// Overloaded vgui functions
	void paint() override;
	virtual void setText(const char* text);
	void paintBackground() override;

	void cursorEntered();
	void cursorExited();

	void setBoundKey(char boundKey);
	char getBoundKey();
};

class ColorButton : public CommandButton
{
private:
	Color* ArmedColor;
	Color* UnArmedColor;

	Color* ArmedBorderColor;
	Color* UnArmedBorderColor;

public:
	ColorButton(const char* text, int x, int y, int wide, int tall, bool bNoHighlight, bool bFlat) : CommandButton(text, x, y, wide, tall, bNoHighlight, bFlat)
	{
		ArmedColor = nullptr;
		UnArmedColor = nullptr;
		ArmedBorderColor = nullptr;
		UnArmedBorderColor = nullptr;
	}


	void paintBackground() override
	{
		int r, g, b, a;
		Color bgcolor;

		if (isArmed())
		{
			// Highlight background
			/*	getBgColor(bgcolor);
			bgcolor.getColor(r, g, b, a);
			drawSetColor( r,g,b,a );
			drawFilledRect(0,0,_size[0],_size[1]);*/

			if (ArmedBorderColor != nullptr)
			{
				ArmedBorderColor->getColor(r, g, b, a);
				drawSetColor(r, g, b, a);
				drawOutlinedRect(0, 0, _size[0], _size[1]);
			}
		}
		else
		{
			if (UnArmedBorderColor != nullptr)
			{
				UnArmedBorderColor->getColor(r, g, b, a);
				drawSetColor(r, g, b, a);
				drawOutlinedRect(0, 0, _size[0], _size[1]);
			}
		}
	}
	void paint() override
	{
		int r, g, b, a;
		if (isArmed())
		{
			if (ArmedColor != nullptr)
			{
				ArmedColor->getColor(r, g, b, a);
				setFgColor(r, g, b, a);
			}
			else
				setFgColor(Scheme::sc_secondary2);
		}
		else
		{
			if (UnArmedColor != nullptr)
			{
				UnArmedColor->getColor(r, g, b, a);
				setFgColor(r, g, b, a);
			}
			else
				setFgColor(Scheme::sc_primary1);
		}

		Button::paint();
	}

	void setArmedColor(int r, int g, int b, int a)
	{
		ArmedColor = new Color(r, g, b, a);
	}

	void setUnArmedColor(int r, int g, int b, int a)
	{
		UnArmedColor = new Color(r, g, b, a);
	}

	void setArmedBorderColor(int r, int g, int b, int a)
	{
		ArmedBorderColor = new Color(r, g, b, a);
	}

	void setUnArmedBorderColor(int r, int g, int b, int a)
	{
		UnArmedBorderColor = new Color(r, g, b, a);
	}
};

class SpectButton : public CommandButton
{
private:
public:
	SpectButton(int iPlayerClass, const char* text, int x, int y, int wide, int tall) : CommandButton(text, x, y, wide, tall, false)
	{
		Init();

		setText(text);
	}

	void paintBackground() override
	{
		if (isArmed())
		{
			drawSetColor(143, 143, 54, 125);
			drawFilledRect(5, 0, _size[0] - 5, _size[1]);
		}
	}

	void paint() override
	{

		if (isArmed())
		{
			setFgColor(194, 202, 54, 0);
		}
		else
		{
			setFgColor(143, 143, 54, 15);
		}

		Button::paint();
	}
};
//============================================================
// Command Menus
class CCommandMenu : public Panel
{
private:
	CCommandMenu* m_pParentMenu;
	int m_iXOffset;
	int m_iYOffset;

	// Buttons in this menu
	CommandButton* m_aButtons[MAX_BUTTONS];
	int m_iButtons;

	// opens menu from top to bottom (false = default), or from bottom to top (true)?
	bool m_iDirection;

public:
	CCommandMenu(CCommandMenu* pParentMenu, int x, int y, int wide, int tall) : Panel(x, y, wide, tall)
	{
		m_pParentMenu = pParentMenu;
		m_iXOffset = x;
		m_iYOffset = y;
		m_iButtons = 0;
		m_iDirection = false;
	}


	CCommandMenu(CCommandMenu* pParentMenu, bool direction, int x, int y, int wide, int tall) : Panel(x, y, wide, tall)
	{
		m_pParentMenu = pParentMenu;
		m_iXOffset = x;
		m_iYOffset = y;
		m_iButtons = 0;
		m_iDirection = direction;
	}

	float m_flButtonSizeY;
	bool m_iSpectCmdMenu;
	void AddButton(CommandButton* pButton);
	bool RecalculateVisibles(int iNewYPos, bool bHideAll);
	void RecalculatePositions(int iYOffset);
	void MakeVisible(CCommandMenu* pChildMenu);

	CCommandMenu* GetParentMenu() { return m_pParentMenu; };
	int GetXOffset() { return m_iXOffset; };
	int GetYOffset() { return m_iYOffset; };
	bool GetDirection() { return m_iDirection; };
	int GetNumButtons() { return m_iButtons; };
	CommandButton* FindButtonWithSubmenu(CCommandMenu* pSubMenu);

	void ClearButtonsOfArmedState();

	void RemoveAllButtons();


	bool KeyInput(int keyNum);

	void paintBackground() override;
};

//==============================================================================
// Command menu root button (drop down box style)

class DropDownButton : public ColorButton
{
private:
	CImageLabel* m_pOpenButton;

public:
	DropDownButton(const char* text, int x, int y, int wide, int tall, bool bNoHighlight, bool bFlat) : ColorButton(text, x, y, wide, tall, bNoHighlight, bFlat)
	{
		// Put a > to show it's a submenu
		m_pOpenButton = new CImageLabel("arrowup", XRES(CMENU_SIZE_X - 2), YRES(BUTTON_SIZE_Y - 2));
		m_pOpenButton->setParent(this);

		int textwide, texttall;
		getSize(textwide, texttall);

		// Reposition
		m_pOpenButton->setPos(textwide - (m_pOpenButton->getImageWide() + 6), -2 /*(tall - m_pOpenButton->getImageTall()*2) / 2*/);
		m_pOpenButton->setVisible(true);
	}

	void setVisible(bool state) override
	{
		m_pOpenButton->setVisible(state);
		ColorButton::setVisible(state);
	}
};

//==============================================================================
// Button with image instead of text

class CImageButton : public ColorButton
{
private:
	CImageLabel* m_pOpenButton;

public:
	CImageButton(const char* text, int x, int y, int wide, int tall, bool bNoHighlight, bool bFlat) : ColorButton(" ", x, y, wide, tall, bNoHighlight, bFlat)
	{
		m_pOpenButton = new CImageLabel(text, 1, 1, wide - 2, tall - 2);
		m_pOpenButton->setParent(this);

		// Reposition
		//	m_pOpenButton->setPos( x+1,y+1 );
		//	m_pOpenButton->setSize(wide-2,tall-2);

		m_pOpenButton->setVisible(true);
	}

	void setVisible(bool state) override
	{
		m_pOpenButton->setVisible(state);
		ColorButton::setVisible(state);
	}
};

//==============================================================================
class TeamFortressViewport : public Panel
{
private:
	vgui::Cursor* _cursorNone;
	vgui::Cursor* _cursorArrow;

	bool m_iInitialized;

	CCommandMenu* m_pCommandMenus[MAX_MENUS];
	CCommandMenu* m_pCurrentCommandMenu;
	float m_flMenuOpenTime;
	float m_flScoreBoardLastUpdated;
	float m_flSpectatorPanelLastUpdated;
	int m_iNumMenus;
	int m_iCurrentTeamNumber;
	int m_iCurrentPlayerClass;
	int m_iUser1;
	int m_iUser2;
	int m_iUser3;

	// VGUI Menus
	void CreateTeamMenu();
	CMenuPanel* ShowTeamMenu();
	void CreateClassMenu();
	CMenuPanel* ShowClassMenu();
	void CreateSpectatorMenu();
	CMenuPanel* ShowCustomMenu(); //AJH new customizable menu system
	void CreateCustomMenu();	  //AJH new customizable menu system

	// Scheme handler
	CSchemeManager m_SchemeManager;

	// MOTD
	bool m_iGotAllMOTD;
	char m_szMOTD[MAX_MOTD_LENGTH];

	//  Command Menu Team buttons
	CommandButton* m_pTeamButtons[6];
	CommandButton* m_pDisguiseButtons[5];
	BuildButton* m_pBuildButtons[3];
	BuildButton* m_pBuildActiveButtons[3];

	bool m_iAllowSpectators;

	// Data for specific sections of the Command Menu
	int m_iValidClasses[5];
	bool m_iIsFeigning;
	int m_iIsSettingDetpack;
	int m_iNumberOfTeams;
	int m_iBuildState;
	bool m_iRandomPC;
	char m_sTeamNames[5][MAX_TEAMNAME_SIZE];

	// Localisation strings
	char m_sDetpackStrings[3][MAX_BUTTON_SIZE];

	char m_sMapName[64];

	// helper function to update the player menu entries
	void UpdatePlayerMenu(int menuIndex);

public:
	TeamFortressViewport(int x, int y, int wide, int tall);
	void Initialize();

	int CreateCommandMenu(const char* menuFile, bool direction, int yOffset, bool flatDesign, float flButtonSizeX, float flButtonSizeY, int xOffset);
	void CreateScoreBoard();
	CommandButton* CreateCustomButton(char* pButtonText, char* pButtonName, int iYOffset);
	CCommandMenu* CreateDisguiseSubmenu(CommandButton* pButton, CCommandMenu* pParentMenu, const char* commandText, int iYOffset, int iXOffset = 0);

	void UpdateCursorState();
	void UpdateCommandMenu(int menuIndex);
	void UpdateOnPlayerInfo();
	void UpdateHighlights();
	void UpdateSpectatorPanel();

	bool KeyInput(bool down, int keynum, const char* pszCurrentBinding);
	void InputPlayerSpecial();
	void GetAllPlayersInfo();
	void DeathMsg(int killer, int victim);

	void ShowCommandMenu(int menuIndex);
	void InputSignalHideCommandMenu();
	void HideCommandMenu();
	void SetCurrentCommandMenu(CCommandMenu* pNewMenu);
	void SetCurrentMenu(CMenuPanel* pMenu);

	void ShowScoreBoard();
	void HideScoreBoard();
	bool IsScoreBoardVisible();

	bool AllowedToPrintText();

	void ShowVGUIMenu(int iMenu);
	void HideVGUIMenu();
	void HideTopMenu();

	CMenuPanel* CreateTextWindow(int iTextToShow);

	CCommandMenu* CreateSubMenu(CommandButton* pButton, CCommandMenu* pParentMenu, int iYOffset, int iXOffset = 0);

	// Data Handlers
	int GetValidClasses(int iTeam) { return m_iValidClasses[iTeam]; };
	int GetNumberOfTeams() { return m_iNumberOfTeams; };
	bool GetIsFeigning() { return m_iIsFeigning; };
	int GetIsSettingDetpack() { return m_iIsSettingDetpack; };
	int GetBuildState() { return m_iBuildState; };
	bool IsRandomPC() { return m_iRandomPC; };
	char* GetTeamName(int iTeam) { return m_sTeamNames[iTeam]; };
	bool GetAllowSpectators() { return m_iAllowSpectators; };

	// Message Handlers
	bool MsgFunc_ValClass(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_TeamNames(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_Feign(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_Detpack(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_VGUIMenu(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_MOTD(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_BuildSt(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_RandomPC(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_ServerName(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_ScoreInfo(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_TeamScore(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_TeamInfo(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_Spectator(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_AllowSpec(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_SpecFade(const char* pszName, int iSize, void* pbuf);
	bool MsgFunc_ResetFade(const char* pszName, int iSize, void* pbuf);

	// Input
	bool SlotInput(int iSlot);

	void paintBackground() override;

	CSchemeManager* GetSchemeManager() { return &m_SchemeManager; }
	ScorePanel* GetScoreBoard() { return m_pScoreBoard; }

	void* operator new(size_t stAllocateBlock);

public:
	// VGUI Menus
	CMenuPanel* m_pCurrentMenu;
	CTeamMenuPanel* m_pTeamMenu;
	CCustomMenu* m_pCustomMenu; //AJH new customizable menu system
	int m_StandardMenu;			// indexs in m_pCommandMenus
	int m_SpectatorOptionsMenu;
	int m_SpectatorCameraMenu;
	int m_PlayerMenu; // a list of current player
	CClassMenuPanel* m_pClassMenu;
	ScorePanel* m_pScoreBoard;
	SpectatorPanel* m_pSpectatorPanel;
	char m_szServerName[MAX_SERVERNAME_LENGTH];
};

//============================================================
// Command Menu Button Handlers
#define MAX_COMMAND_SIZE 256

class CMenuHandler_StringCommand : public ActionSignal
{
protected:
	char m_pszCommand[MAX_COMMAND_SIZE];
	bool m_iCloseVGUIMenu;

public:
	CMenuHandler_StringCommand(const char* pszCommand)
	{
		strncpy(m_pszCommand, pszCommand, MAX_COMMAND_SIZE);
		m_pszCommand[MAX_COMMAND_SIZE - 1] = '\0';
		m_iCloseVGUIMenu = false;
	}

	CMenuHandler_StringCommand(const char* pszCommand, bool iClose)
	{
		strncpy(m_pszCommand, pszCommand, MAX_COMMAND_SIZE);
		m_pszCommand[MAX_COMMAND_SIZE - 1] = '\0';
		m_iCloseVGUIMenu = true;
	}

	void actionPerformed(Panel* panel) override
	{
		gEngfuncs.pfnClientCmd(m_pszCommand);

		if (m_iCloseVGUIMenu)
			gViewPort->HideTopMenu();
		else
			gViewPort->HideCommandMenu();
	}
};

// This works the same as CMenuHandler_StringCommand, except it watches the string command
// for specific commands, and modifies client vars based upon them.
class CMenuHandler_StringCommandWatch : public CMenuHandler_StringCommand
{
private:
public:
	CMenuHandler_StringCommandWatch(const char* pszCommand) : CMenuHandler_StringCommand(pszCommand)
	{
	}

	CMenuHandler_StringCommandWatch(const char* pszCommand, bool iClose) : CMenuHandler_StringCommand(pszCommand, iClose)
	{
	}

	void actionPerformed(Panel* panel) override
	{
		CMenuHandler_StringCommand::actionPerformed(panel);

		// Try to guess the player's new team (it'll be corrected if it's wrong)
		if (strcmp(m_pszCommand, "jointeam 1") == 0)
			g_iTeamNumber = 1;
		else if (strcmp(m_pszCommand, "jointeam 2") == 0)
			g_iTeamNumber = 2;
		else if (strcmp(m_pszCommand, "jointeam 3") == 0)
			g_iTeamNumber = 3;
		else if (strcmp(m_pszCommand, "jointeam 4") == 0)
			g_iTeamNumber = 4;
	}
};

// Used instead of CMenuHandler_StringCommand for Class Selection buttons.
// Checks the state of hud_classautokill and kills the player if set
class CMenuHandler_StringCommandClassSelect : public CMenuHandler_StringCommand
{
private:
public:
	CMenuHandler_StringCommandClassSelect(const char* pszCommand) : CMenuHandler_StringCommand(pszCommand)
	{
	}

	CMenuHandler_StringCommandClassSelect(const char* pszCommand, int iClose) : CMenuHandler_StringCommand(pszCommand, iClose != 0)
	{
	}

	void actionPerformed(Panel* panel) override;
};

class CMenuHandler_PopupSubMenuInput : public InputSignal
{
private:
	CCommandMenu* m_pSubMenu;
	Button* m_pButton;

public:
	CMenuHandler_PopupSubMenuInput(Button* pButton, CCommandMenu* pSubMenu)
	{
		m_pSubMenu = pSubMenu;
		m_pButton = pButton;
	}

	void cursorMoved(int x, int y, Panel* panel) override
	{
		//gViewPort->SetCurrentCommandMenu( m_pSubMenu );
	}

	void cursorEntered(Panel* panel) override
	{
		gViewPort->SetCurrentCommandMenu(m_pSubMenu);

		if (m_pButton != nullptr)
			m_pButton->setArmed(true);
	};
	void cursorExited(Panel* Panel) override{};
	void mousePressed(MouseCode code, Panel* panel) override{};
	void mouseDoublePressed(MouseCode code, Panel* panel) override{};
	void mouseReleased(MouseCode code, Panel* panel) override{};
	void mouseWheeled(int delta, Panel* panel) override{};
	void keyPressed(KeyCode code, Panel* panel) override{};
	void keyTyped(KeyCode code, Panel* panel) override{};
	void keyReleased(KeyCode code, Panel* panel) override{};
	void keyFocusTicked(Panel* panel) override{};
};

class CMenuHandler_LabelInput : public InputSignal
{
private:
	ActionSignal* m_pActionSignal;

public:
	CMenuHandler_LabelInput(ActionSignal* pSignal)
	{
		m_pActionSignal = pSignal;
	}

	void mousePressed(MouseCode code, Panel* panel) override
	{
		m_pActionSignal->actionPerformed(panel);
	}

	void mouseReleased(MouseCode code, Panel* panel) override{};
	void cursorEntered(Panel* panel) override{};
	void cursorExited(Panel* Panel) override{};
	void cursorMoved(int x, int y, Panel* panel) override{};
	void mouseDoublePressed(MouseCode code, Panel* panel) override{};
	void mouseWheeled(int delta, Panel* panel) override{};
	void keyPressed(KeyCode code, Panel* panel) override{};
	void keyTyped(KeyCode code, Panel* panel) override{};
	void keyReleased(KeyCode code, Panel* panel) override{};
	void keyFocusTicked(Panel* panel) override{};
};

#define HIDE_TEXTWINDOW 0
#define SHOW_MAPBRIEFING 1
#define SHOW_CLASSDESC 2
#define SHOW_MOTD 3
#define SHOW_SPECHELP 4

class CMenuHandler_TextWindow : public ActionSignal
{
private:
	int m_iState;

public:
	CMenuHandler_TextWindow(int iState)
	{
		m_iState = iState;
	}

	void actionPerformed(Panel* panel) override
	{
		if (m_iState == HIDE_TEXTWINDOW)
		{
			gViewPort->HideTopMenu();
		}
		else
		{
			gViewPort->HideCommandMenu();
			gViewPort->ShowVGUIMenu(m_iState);
		}
	}
};

class CMenuHandler_ToggleCvar : public ActionSignal
{
private:
	struct cvar_s* m_cvar;

public:
	CMenuHandler_ToggleCvar(char* cvarname)
	{
		m_cvar = gEngfuncs.pfnGetCvarPointer(cvarname);
	}

	void actionPerformed(Panel* panel) override
	{
		if (m_cvar->value != 0.0f)
			m_cvar->value = 0.0f;
		else
			m_cvar->value = 1.0f;

		// hide the menu
		gViewPort->HideCommandMenu();

		gViewPort->UpdateSpectatorPanel();
	}
};



class CMenuHandler_SpectateFollow : public ActionSignal
{
protected:
	char m_szplayer[MAX_COMMAND_SIZE];

public:
	CMenuHandler_SpectateFollow(char* player)
	{
		strncpy(m_szplayer, player, MAX_COMMAND_SIZE);
		m_szplayer[MAX_COMMAND_SIZE - 1] = '\0';
	}

	void actionPerformed(Panel* panel) override
	{
		gHUD.m_Spectator.FindPlayer(m_szplayer);
		gViewPort->HideCommandMenu();
	}
};



class CDragNDropHandler : public InputSignal
{
private:
	DragNDropPanel* m_pPanel;
	bool m_bDragging;
	int m_iaDragOrgPos[2];
	int m_iaDragStart[2];

public:
	CDragNDropHandler(DragNDropPanel* pPanel)
	{
		m_pPanel = pPanel;
		m_bDragging = false;
	}

	void cursorMoved(int x, int y, Panel* panel) override;
	void mousePressed(MouseCode code, Panel* panel) override;
	void mouseReleased(MouseCode code, Panel* panel) override;

	void mouseDoublePressed(MouseCode code, Panel* panel) override{};
	void cursorEntered(Panel* panel) override{};
	void cursorExited(Panel* panel) override{};
	void mouseWheeled(int delta, Panel* panel) override{};
	void keyPressed(KeyCode code, Panel* panel) override{};
	void keyTyped(KeyCode code, Panel* panel) override{};
	void keyReleased(KeyCode code, Panel* panel) override{};
	void keyFocusTicked(Panel* panel) override{};
};

class CHandler_MenuButtonOver : public InputSignal
{
private:
	int m_iButton;
	CMenuPanel* m_pMenuPanel;

public:
	CHandler_MenuButtonOver(CMenuPanel* pPanel, int iButton)
	{
		m_iButton = iButton;
		m_pMenuPanel = pPanel;
	}

	void cursorEntered(Panel* panel) override;

	void cursorMoved(int x, int y, Panel* panel) override{};
	void mousePressed(MouseCode code, Panel* panel) override{};
	void mouseReleased(MouseCode code, Panel* panel) override{};
	void mouseDoublePressed(MouseCode code, Panel* panel) override{};
	void cursorExited(Panel* panel) override{};
	void mouseWheeled(int delta, Panel* panel) override{};
	void keyPressed(KeyCode code, Panel* panel) override{};
	void keyTyped(KeyCode code, Panel* panel) override{};
	void keyReleased(KeyCode code, Panel* panel) override{};
	void keyFocusTicked(Panel* panel) override{};
};

class CHandler_ButtonHighlight : public InputSignal
{
private:
	Button* m_pButton;

public:
	CHandler_ButtonHighlight(Button* pButton)
	{
		m_pButton = pButton;
	}

	void cursorEntered(Panel* panel) override
	{
		m_pButton->setArmed(true);
	};
	void cursorExited(Panel* Panel) override
	{
		m_pButton->setArmed(false);
	};
	void mousePressed(MouseCode code, Panel* panel) override{};
	void mouseReleased(MouseCode code, Panel* panel) override{};
	void cursorMoved(int x, int y, Panel* panel) override{};
	void mouseDoublePressed(MouseCode code, Panel* panel) override{};
	void mouseWheeled(int delta, Panel* panel) override{};
	void keyPressed(KeyCode code, Panel* panel) override{};
	void keyTyped(KeyCode code, Panel* panel) override{};
	void keyReleased(KeyCode code, Panel* panel) override{};
	void keyFocusTicked(Panel* panel) override{};
};

//-----------------------------------------------------------------------------
// Purpose: Special handler for highlighting of command menu buttons
//-----------------------------------------------------------------------------
class CHandler_CommandButtonHighlight : public CHandler_ButtonHighlight
{
private:
	CommandButton* m_pCommandButton;

public:
	CHandler_CommandButtonHighlight(CommandButton* pButton) : CHandler_ButtonHighlight(pButton)
	{
		m_pCommandButton = pButton;
	}

	void cursorEntered(Panel* panel) override
	{
		m_pCommandButton->cursorEntered();
	}

	void cursorExited(Panel* panel) override
	{
		m_pCommandButton->cursorExited();
	}
};


//================================================================
// Overidden Command Buttons for special visibilities
class ClassButton : public CommandButton
{
protected:
	int m_iPlayerClass;

public:
	ClassButton(int iClass, const char* text, int x, int y, int wide, int tall, bool bNoHighlight) : CommandButton(text, x, y, wide, tall, bNoHighlight)
	{
		m_iPlayerClass = iClass;
	}

	bool IsNotValid() override;
};

class TeamButton : public CommandButton
{
private:
	int m_iTeamNumber;

public:
	TeamButton(int iTeam, const char* text, int x, int y, int wide, int tall) : CommandButton(text, x, y, wide, tall)
	{
		m_iTeamNumber = iTeam;
	}

	bool IsNotValid() override
	{
		int iTeams = gViewPort->GetNumberOfTeams();
		// Never valid if there's only 1 team
		if (iTeams == 1)
			return true;

		// Auto Team's always visible
		if (m_iTeamNumber == 5)
			return false;

		if (iTeams >= m_iTeamNumber && m_iTeamNumber != g_iTeamNumber)
			return false;

		return true;
	}
};

class FeignButton : public CommandButton
{
private:
	bool m_iFeignState;

public:
	FeignButton(bool iState, const char* text, int x, int y, int wide, int tall) : CommandButton(text, x, y, wide, tall)
	{
		m_iFeignState = iState;
	}

	bool IsNotValid() override
	{
		// Only visible for spies

		if (m_iFeignState == gViewPort->GetIsFeigning())
			return false;
		return true;
	}
};

class SpectateButton : public CommandButton
{
public:
	SpectateButton(const char* text, int x, int y, int wide, int tall, bool bNoHighlight) : CommandButton(text, x, y, wide, tall, bNoHighlight)
	{
	}

	bool IsNotValid() override
	{
		// Only visible if the server allows it
		if (gViewPort->GetAllowSpectators())
			return false;

		return true;
	}
};

#define DISGUISE_TEAM1 (1 << 0)
#define DISGUISE_TEAM2 (1 << 1)
#define DISGUISE_TEAM3 (1 << 2)
#define DISGUISE_TEAM4 (1 << 3)

class DisguiseButton : public CommandButton
{
private:
	int m_iValidTeamsBits;
	int m_iThisTeam;

public:
	DisguiseButton(int iValidTeamNumsBits, const char* text, int x, int y, int wide, int tall) : CommandButton(text, x, y, wide, tall, false)
	{
		m_iValidTeamsBits = iValidTeamNumsBits;
	}

	bool IsNotValid() override
	{

		// if it's not tied to a specific team, then always show (for spies)
		if (m_iValidTeamsBits == 0)
			return false;

		// if we're tied to a team make sure we can change to that team
		int iTmp = 1 << (gViewPort->GetNumberOfTeams() - 1);
		if ((m_iValidTeamsBits & iTmp) != 0)
			return false;
		return true;
	}
};

class InventoryButton : public CommandButton //AJH inventory system
{
private:
	int m_iItem;

public:
	InventoryButton(int item, const char* text, int x, int y, int wide, int tall, bool bFlat) : CommandButton(text, x, y, wide, tall, bFlat)
	{
		m_iItem = item;
	}

	bool IsNotValid() override
	{
		// Always show the main 'Inventory' button (if it is specified in commandmenu.txt
		if (m_iItem < 0 || m_iItem >= MAX_ITEMS) // m_iItem >= MAX_ITEMS shouldn't ever be true
			return false;
		//Only visible if the item is in the players inventory
		if (g_iInventory[m_iItem] > 0)
			return false;
		else
			return true;
	}
};

class DetpackButton : public CommandButton
{
private:
	int m_iDetpackState;

public:
	DetpackButton(int iState, const char* text, int x, int y, int wide, int tall) : CommandButton(text, x, y, wide, tall)
	{
		m_iDetpackState = iState;
	}

	bool IsNotValid() override
	{

		if (m_iDetpackState == gViewPort->GetIsSettingDetpack())
			return false;

		return true;
	}
};

extern int iBuildingCosts[];
#define BUILDSTATE_HASBUILDING (1 << 0) // Data is building ID (1 = Dispenser, 2 = Sentry, 3 = Entry Teleporter, 4 = Exit Teleporter)
#define BUILDSTATE_BUILDING (1 << 1)
#define BUILDSTATE_BASE (1 << 2)
#define BUILDSTATE_CANBUILD (1 << 3) // Data is building ID (1 = Dispenser, 2 = Sentry, 3 = Entry Teleporter, 4 = Exit Teleporter)

class BuildButton : public CommandButton
{
private:
	int m_iBuildState;
	int m_iBuildData;

public:
	enum Buildings
	{
		DISPENSER = 0,
		SENTRYGUN = 1,
		ENTRY_TELEPORTER = 2,
		EXIT_TELEPORTER = 3
	};

	BuildButton(int iState, int iData, const char* text, int x, int y, int wide, int tall) : CommandButton(text, x, y, wide, tall)
	{
		m_iBuildState = iState;
		m_iBuildData = iData;
	}

	bool IsNotValid() override
	{
		return false;
	}
};

#define MAX_MAPNAME 256

class MapButton : public CommandButton
{
private:
	char m_szMapName[MAX_MAPNAME];

public:
	MapButton(const char* pMapName, const char* text, int x, int y, int wide, int tall) : CommandButton(text, x, y, wide, tall)
	{
		sprintf(m_szMapName, "maps/%s.bsp", pMapName);
	}

	bool IsNotValid() override
	{
		const char* level = gEngfuncs.pfnGetLevelName();
		if (level == nullptr)
			return true;

		// Does it match the current map name?
		if (strcmp(m_szMapName, level) != 0)
			return true;

		return false;
	}
};

//-----------------------------------------------------------------------------
// Purpose: CommandButton which is only displayed if the player is on team X
//-----------------------------------------------------------------------------
class TeamOnlyCommandButton : public CommandButton
{
private:
	int m_iTeamNum;

public:
	TeamOnlyCommandButton(int iTeamNum, const char* text, int x, int y, int wide, int tall, bool flat) : CommandButton(text, x, y, wide, tall, false, flat), m_iTeamNum(iTeamNum) {}

	bool IsNotValid() override
	{
		if (g_iTeamNumber != m_iTeamNum)
			return true;

		return CommandButton::IsNotValid();
	}
};

//-----------------------------------------------------------------------------
// Purpose: CommandButton which is only displayed if the player is on team X
//-----------------------------------------------------------------------------
class ToggleCommandButton : public CommandButton, public InputSignal
{
private:
	struct cvar_s* m_cvar;
	CImageLabel* pLabelOn;
	CImageLabel* pLabelOff;


public:
	ToggleCommandButton(const char* cvarname, const char* text, int x, int y, int wide, int tall, bool flat) : CommandButton(text, x, y, wide, tall, false, flat)
	{
		m_cvar = gEngfuncs.pfnGetCvarPointer(cvarname);

		// Put a > to show it's a submenu
		pLabelOn = new CImageLabel("checked", 0, 0);
		pLabelOn->setParent(this);
		pLabelOn->addInputSignal(this);

		pLabelOff = new CImageLabel("unchecked", 0, 0);
		pLabelOff->setParent(this);
		pLabelOff->setEnabled(true);
		pLabelOff->addInputSignal(this);

		int textwide, texttall;
		getTextSize(textwide, texttall);

		// Reposition
		pLabelOn->setPos(textwide, (tall - pLabelOn->getTall()) / 2);

		pLabelOff->setPos(textwide, (tall - pLabelOff->getTall()) / 2);

		// Set text color to orange
		setFgColor(Scheme::sc_primary1);
	}

	void cursorEntered(Panel* panel) override
	{
		CommandButton::cursorEntered();
	}

	void cursorExited(Panel* panel) override
	{
		CommandButton::cursorExited();
	}

	void mousePressed(MouseCode code, Panel* panel) override
	{
		doClick();
	};

	void cursorMoved(int x, int y, Panel* panel) override{};

	void mouseDoublePressed(MouseCode code, Panel* panel) override{};
	void mouseReleased(MouseCode code, Panel* panel) override{};
	void mouseWheeled(int delta, Panel* panel) override{};
	void keyPressed(KeyCode code, Panel* panel) override{};
	void keyTyped(KeyCode code, Panel* panel) override{};
	void keyReleased(KeyCode code, Panel* panel) override{};
	void keyFocusTicked(Panel* panel) override{};

	void paint() override
	{
		if (m_cvar == nullptr)
		{
			pLabelOff->setVisible(false);
			pLabelOn->setVisible(false);
		}
		else if (m_cvar->value != 0.0f)
		{
			pLabelOff->setVisible(false);
			pLabelOn->setVisible(true);
		}
		else
		{
			pLabelOff->setVisible(true);
			pLabelOn->setVisible(false);
		}

		CommandButton::paint();
	}
};
class SpectToggleButton : public CommandButton, public InputSignal
{
private:
	struct cvar_s* m_cvar;
	CImageLabel* pLabelOn;

public:
	SpectToggleButton(const char* cvarname, const char* text, int x, int y, int wide, int tall, bool flat) : CommandButton(text, x, y, wide, tall, false, flat)
	{
		m_cvar = gEngfuncs.pfnGetCvarPointer(cvarname);

		// Put a > to show it's a submenu
		pLabelOn = new CImageLabel("checked", 0, 0);
		pLabelOn->setParent(this);
		pLabelOn->addInputSignal(this);


		int textwide, texttall;
		getTextSize(textwide, texttall);

		// Reposition
		pLabelOn->setPos(textwide, (tall - pLabelOn->getTall()) / 2);
	}

	void cursorEntered(Panel* panel) override
	{
		CommandButton::cursorEntered();
	}

	void cursorExited(Panel* panel) override
	{
		CommandButton::cursorExited();
	}

	void mousePressed(MouseCode code, Panel* panel) override
	{
		doClick();
	};

	void cursorMoved(int x, int y, Panel* panel) override{};

	void mouseDoublePressed(MouseCode code, Panel* panel) override{};
	void mouseReleased(MouseCode code, Panel* panel) override{};
	void mouseWheeled(int delta, Panel* panel) override{};
	void keyPressed(KeyCode code, Panel* panel) override{};
	void keyTyped(KeyCode code, Panel* panel) override{};
	void keyReleased(KeyCode code, Panel* panel) override{};
	void keyFocusTicked(Panel* panel) override{};

	void paintBackground() override
	{
		if (isArmed())
		{
			drawSetColor(143, 143, 54, 125);
			drawFilledRect(5, 0, _size[0] - 5, _size[1]);
		}
	}

	void paint() override
	{
		if (isArmed())
		{
			setFgColor(194, 202, 54, 0);
		}
		else
		{
			setFgColor(143, 143, 54, 15);
		}

		if (m_cvar == nullptr)
		{
			pLabelOn->setVisible(false);
		}
		else if (m_cvar->value != 0.0f)
		{
			pLabelOn->setVisible(true);
		}
		else
		{
			pLabelOn->setVisible(false);
		}

		Button::paint();
	}
};

/*
class SpectToggleButton : public ToggleCommandButton
{
private:
	struct cvar_s * m_cvar;
	CImageLabel *	pLabelOn;
	CImageLabel *	pLabelOff;
	
public:

	SpectToggleButton( const char* cvarname, const char* text,int x,int y,int wide,int tall, bool flat ) : 
	  ToggleCommandButton( cvarname, text, x, y, wide, tall, flat, true )
	 {
		m_cvar = gEngfuncs.pfnGetCvarPointer( cvarname );

			// Put a > to show it's a submenu
		pLabelOn = new CImageLabel( "checked", 0, 0 );
		pLabelOn->setParent(this);
		pLabelOn->addInputSignal(this);
				
		pLabelOff = new CImageLabel( "unchecked", 0, 0 );
		pLabelOff->setParent(this);
		pLabelOff->setEnabled(true);
		pLabelOff->addInputSignal(this);

		int textwide, texttall;
		getTextSize( textwide, texttall);
	
		// Reposition
		pLabelOn->setPos( textwide, (tall - pLabelOn->getTall()) / 2 );

		pLabelOff->setPos( textwide, (tall - pLabelOff->getTall()) / 2 );
		
		// Set text color to orange
		setFgColor(Scheme::sc_primary1);
	}
		
	virtual void paintBackground()
	{
		if ( isArmed())
		{
			drawSetColor( 143,143, 54, 125 ); 
			drawFilledRect( 5, 0,_size[0] - 5,_size[1]);
		}
	}

	virtual void paint()
	{
	
		if ( isArmed() )
		{ 
			setFgColor( 194, 202, 54, 0 );
		}
		else
		{
			setFgColor( 143, 143, 54, 15 );
		}

			if ( !m_cvar )
		{
			pLabelOff->setVisible(false);
			pLabelOn->setVisible(false);
		} 
		else if ( m_cvar->value )
		{
			pLabelOff->setVisible(false);
			pLabelOn->setVisible(true);
		}
		else
		{
			pLabelOff->setVisible(true);
			pLabelOn->setVisible(false);
		}

		Button::paint();
	}
};
*/
//============================================================
// Panel that can be dragged around
class DragNDropPanel : public Panel
{
private:
	bool m_bBeingDragged;
	LineBorder* m_pBorder;

public:
	DragNDropPanel(int x, int y, int wide, int tall) : Panel(x, y, wide, tall)
	{
		m_bBeingDragged = false;

		// Create the Drag Handler
		addInputSignal(new CDragNDropHandler(this));

		// Create the border (for dragging)
		m_pBorder = new LineBorder();
	}

	virtual void setDragged(bool bState)
	{
		m_bBeingDragged = bState;

		if (m_bBeingDragged)
			setBorder(m_pBorder);
		else
			setBorder(nullptr);
	}
};

//================================================================
// Panel that draws itself with a transparent black background
class CTransparentPanel : public Panel
{
private:
	int m_iTransparency;

public:
	CTransparentPanel(int iTrans, int x, int y, int wide, int tall) : Panel(x, y, wide, tall)
	{
		m_iTransparency = iTrans;
	}

	void paintBackground() override
	{
		if (m_iTransparency != 0)
		{
			// Transparent black background
			drawSetColor(0, 0, 0, m_iTransparency);
			drawFilledRect(0, 0, _size[0], _size[1]);
		}
	}
};

//================================================================
// Menu Panel that supports buffering of menus
class CMenuPanel : public CTransparentPanel
{
private:
	CMenuPanel* m_pNextMenu;
	int m_iMenuID;
	bool m_iRemoveMe;
	bool m_iIsActive;
	float m_flOpenTime;

public:
	CMenuPanel(bool iRemoveMe, int x, int y, int wide, int tall) : CTransparentPanel(100, x, y, wide, tall)
	{
		Reset();
		m_iRemoveMe = iRemoveMe;
	}

	CMenuPanel(int iTrans, bool iRemoveMe, int x, int y, int wide, int tall) : CTransparentPanel(iTrans, x, y, wide, tall)
	{
		Reset();
		m_iRemoveMe = iRemoveMe;
	}

	virtual void Reset()
	{
		m_pNextMenu = nullptr;
		m_iIsActive = false;
		m_flOpenTime = 0;
	}

	void SetNextMenu(CMenuPanel* pNextPanel)
	{
		if (m_pNextMenu != nullptr)
			m_pNextMenu->SetNextMenu(pNextPanel);
		else
			m_pNextMenu = pNextPanel;
	}

	void SetMenuID(int iID)
	{
		m_iMenuID = iID;
	}

	void SetActive(bool iState)
	{
		m_iIsActive = iState;
	}

	virtual void Open()
	{
		setVisible(true);

		// Note the open time, so we can delay input for a bit
		m_flOpenTime = gHUD.m_flTime;
	}

	virtual void Close()
	{
		setVisible(false);
		m_iIsActive = false;

		if (m_iRemoveMe)
			gViewPort->removeChild(this);

		// This MenuPanel has now been deleted. Don't append code here.
	}

	bool ShouldBeRemoved() { return m_iRemoveMe; };
	CMenuPanel* GetNextMenu() { return m_pNextMenu; };
	int GetMenuID() { return m_iMenuID; };
	bool IsActive() { return m_iIsActive; };
	float GetOpenTime() { return m_flOpenTime; };

	// Numeric input
	virtual bool SlotInput(int iSlot) { return false; };
	virtual void SetActiveInfo(int iInput){};
};

//================================================================
// Custom drawn scroll bars
class CTFScrollButton : public CommandButton
{
private:
	BitmapTGA* m_pTGA;

public:
	CTFScrollButton(int iArrow, const char* text, int x, int y, int wide, int tall);

	void paint() override;
	void paintBackground() override;
};

// Custom drawn slider bar
class CTFSlider : public Slider
{
public:
	CTFSlider(int x, int y, int wide, int tall, bool vertical) : Slider(x, y, wide, tall, vertical){};

	void paintBackground() override;
};

// Custom drawn scrollpanel
class CTFScrollPanel : public ScrollPanel
{
public:
	CTFScrollPanel(int x, int y, int wide, int tall);
};

//================================================================
// Menu Panels that take key input
//============================================================
class CClassMenuPanel : public CMenuPanel
{
private:
	CTransparentPanel* m_pClassInfoPanel[PC_LASTCLASS];
	Label* m_pPlayers[PC_LASTCLASS];
	ClassButton* m_pButtons[PC_LASTCLASS];
	CommandButton* m_pCancelButton;
	ScrollPanel* m_pScrollPanel;

	CImageLabel* m_pClassImages[MAX_TEAMS][PC_LASTCLASS];

	int m_iCurrentInfo;

	enum
	{
		STRLENMAX_PLAYERSONTEAM = 128
	};
	char m_sPlayersOnTeamString[STRLENMAX_PLAYERSONTEAM];

public:
	CClassMenuPanel(int iTrans, bool iRemoveMe, int x, int y, int wide, int tall);

	bool SlotInput(int iSlot) override;
	void Open() override;
	virtual void Update();
	void SetActiveInfo(int iInput) override;
	virtual void Initialize();

	void Reset() override
	{
		CMenuPanel::Reset();
		m_iCurrentInfo = 0;
	}
};

class CCustomMenu : public CMenuPanel //AJH Custom Menu HUD
{
private:
	CTransparentPanel* m_pClassInfoPanel[PC_LASTCLASS];
	Label* m_pPlayers[PC_LASTCLASS];
	ClassButton* m_pButtons[PC_LASTCLASS];
	CommandButton* m_pCancelButton;
	ScrollPanel* m_pScrollPanel;

	CImageLabel* m_pClassImages[MAX_TEAMS][PC_LASTCLASS];

	int m_iCurrentInfo;

	enum
	{
		STRLENMAX_PLAYERSONTEAM = 128
	};
	char m_sPlayersOnTeamString[STRLENMAX_PLAYERSONTEAM];

public:
	CCustomMenu(int iTrans, int iRemoveMe, int x, int y, int wide, int tall);

	bool SlotInput(int iSlot) override;
	void Open() override;
	virtual void Update();
	void SetActiveInfo(int iInput) override;
	virtual void Initialize();

	void Reset() override
	{
		CMenuPanel::Reset();
		m_iCurrentInfo = 0;
	}
};

class CTeamMenuPanel : public CMenuPanel
{
public:
	ScrollPanel* m_pScrollPanel;
	CTransparentPanel* m_pTeamWindow;
	Label* m_pMapTitle;
	TextPanel* m_pBriefing;
	TextPanel* m_pTeamInfoPanel[6];
	CommandButton* m_pButtons[6];
	bool m_bUpdatedMapName;
	CommandButton* m_pCancelButton;
	CommandButton* m_pSpectateButton;

	int m_iCurrentInfo;

public:
	CTeamMenuPanel(int iTrans, bool iRemoveMe, int x, int y, int wide, int tall);

	bool SlotInput(int iSlot) override;
	void Open() override;
	virtual void Update();
	void SetActiveInfo(int iInput) override;
	void paintBackground() override;

	virtual void Initialize();

	void Reset() override
	{
		CMenuPanel::Reset();
		m_iCurrentInfo = 0;
	}
};

//=========================================================
// Specific Menus to handle old HUD sections
class CHealthPanel : public DragNDropPanel
{
private:
	BitmapTGA* m_pHealthTGA;
	Label* m_pHealthLabel;

public:
	CHealthPanel(int x, int y, int wide, int tall) : DragNDropPanel(x, y, wide, tall)
	{
		// Load the Health icon
		FileInputStream* fis = new FileInputStream(GetVGUITGAName("%d_hud_health"), false);
		m_pHealthTGA = new BitmapTGA(fis, true);
		fis->close();

		// Create the Health Label
		int iXSize, iYSize;
		m_pHealthTGA->getSize(iXSize, iYSize);
		m_pHealthLabel = new Label("", 0, 0, iXSize, iYSize);
		m_pHealthLabel->setImage(m_pHealthTGA);
		m_pHealthLabel->setParent(this);

		// Set panel dimension
		// Shouldn't be needed once Billy's fized setImage not recalculating the size
		//setSize( iXSize + 100, gHUD.m_iFontHeight + 10 );
		//m_pHealthLabel->setPos( 10, (getTall() - iYSize) / 2 );
	}

	void paintBackground() override
	{
	}

	void paint() override
	{
		// Get the paint color
		int r, g, b, a;
		// Has health changed? Flash the health #
		if (gHUD.m_Health.m_fFade != 0.0f)
		{
			gHUD.m_Health.m_fFade -= (gHUD.m_flTimeDelta * 20);
			if (gHUD.m_Health.m_fFade <= 0)
			{
				a = MIN_ALPHA;
				gHUD.m_Health.m_fFade = 0;
			}

			// Fade the health number back to dim
			a = MIN_ALPHA + (gHUD.m_Health.m_fFade / FADE_TIME) * 128;
		}
		else
			a = MIN_ALPHA;

		gHUD.m_Health.GetPainColor(r, g, b);
		ScaleColors(r, g, b, a);

		// If health is getting low, make it bright red
		if (gHUD.m_Health.m_iHealth <= 15)
			a = 255;

		int iXSize, iYSize, iXPos, iYPos;
		m_pHealthTGA->getSize(iXSize, iYSize);
		m_pHealthTGA->getPos(iXPos, iYPos);

		// Paint the player's health
		int x = gHUD.DrawHudNumber(iXPos + iXSize + 5, iYPos + 5, DHN_3DIGITS | DHN_DRAWZERO, gHUD.m_Health.m_iHealth, r, g, b);

		// Draw the vertical line
		int HealthWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;
		x += HealthWidth / 2;
		FillRGBA(x, iYPos + 5, HealthWidth / 10, gHUD.m_iFontHeight, 255, 160, 0, a);
	}
};
