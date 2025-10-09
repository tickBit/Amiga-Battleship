/*
        Battle ship game - light version with own screen for Amiga a'la spaghetti...

        Motivation for this version: ObtainBestPenA() doesn't work on all RTG systems
        
        Version 1.0.0

        With VBCC: vc -c99 Battleship-light-screen.c -o Battleship-light-screen -lamiga -fpu=68881

        You can adjust the difficulty of the game by increasing or decreasing
        constant DIFFICULTY (and variable error).
        
        I taught myself new things while programming this.
        In my youth I thought I would never use layers.library.
        Now I have.
        
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <dos/dos.h>
#include <exec/memory.h>
#include <exec/types.h>

#include <exec/interrupts.h>
#include <exec/io.h>

#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <graphics/displayinfo.h>
#include <graphics/view.h>
#include <graphics/rastport.h>
#include <graphics/regions.h>
#include <graphics/layers.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>

#include <clib/intuition_protos.h>
#include <clib/utility_protos.h>
#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/macros.h>

#include "battleship-pic-light.c"

#define PLAY_BUTTON     (0)
#define UNDO_BUTTON     (1)
#define NEWGAME_BUTTON  (2)

#define MARGIN 24

#define START_SCREEN    0
#define PLACE_SHIPS     1
#define PLAY            2
#define GAME_OVER       3

#define SCR_DEPTH       4

int state;

struct Library *IntuitionBase = NULL;
struct Library *UtilityBase = NULL;
struct Library *GfxBase = NULL;
struct Library *GadToolsBase = NULL;
struct Library *LayersBase = NULL;

struct Screen *scr = NULL;
struct Window *win = NULL;

int undoShip = 0;
int undoBoard[16*16];
int board[16*16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

const int ship1_[9] = {0,0,1,0,1,1,0,0,1};
const int ship2_[9] = {1,1,1,0,0,0,0,0,0};
const int ship3_[4] = {1,1,1,1};
const int ship4_[16] = {1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0};
const int ship5_[25] = {1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

int ship1[9] = {1,1,1,0,1,0,0,0,0};
int ship2[9] = {1,1,1,0,0,0,0,0,0};
int ship3[4] = {1,1,1,1};
int ship4[16] = {1,1,1,1,1,0,0,0,1,0,0,0,1,0,0,0};
int ship5[25] = {1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

const float DIFFICULTY = 0.15f;

int shipSelected = 0;
int angle_ = 0;
int error = 6;
float errorDelay = 0.0f;

struct RGB8 {
    UBYTE r, g, b;
};

ULONG modeID;
ULONG screenWidth;
ULONG screenHeight;

ULONG penPink = 3;          // (237,119,255)
ULONG penLightPink = 4;     // (254,204,253)
ULONG penBlue = 5;          // (52,100,208)
ULONG penLightBlue = 6;     // (102,203,255)
ULONG penWhite = 7;         // (237,255,255)
ULONG penPinkHit = 8;       // (204,136,255)
ULONG penLightPinkTxt = 9;  // (254,240,255)
ULONG penLightBlueTxt = 10;  // (99,206,255)

ULONG penTitle = 11;
ULONG penTitleTxt = 12;
ULONG penGrid = 13;

ULONG penBG = 14;

UWORD gridMarginX;
UWORD gridMarginY;

struct TextAttr Topaz12 = { "topaz.font", 12, 0, 0 };
struct TextAttr Topaz16 = {"topaz.font", 16, 0, 0};
struct TextFont *myfont, *myfont2;

struct Gadget    *glist = NULL, *gads[3] = { NULL };
struct NewGadget ng;
APTR             vi;

WORD static borderTop;
WORD static borderLeft;


struct EasyStruct myES =
    {
        sizeof(struct EasyStruct),
        0,
        "Info",
        "All ships not placed",
        "Ok",
    };


BOOL shipsPlaced[5] = {FALSE, FALSE, FALSE, FALSE, FALSE};

int AIHits = 0;
int plyHits = 0;

void startPrg()
{
        struct Region *clipRegion;
        struct Region *old, *newRegion;
        struct Rectangle rect;
        
        BOOL clipInstalled = FALSE;
        
        struct BitMapHeader *bmhd;
        
        int width, height;
        struct RastPort *rastport;
        struct TextFont *font;
        struct IntuiMessage *Message; 
        struct InputEvent *ie;
        BOOL Done = FALSE;

        UWORD rBG, gBG, bBG;
        
        LONG Depth;
        
        scr = OpenScreenTags(NULL,
        SA_DisplayID, modeID,
        SA_Width,       screenWidth,
        SA_Height,      screenHeight,
        SA_Depth,       SCR_DEPTH,
        SA_Quiet,       TRUE,
        SA_ShowTitle,   FALSE,
        SA_Exclusive,   TRUE,
        SA_Title,   (ULONG)"Battle Ship Game",
        TAG_END
        );

        if (!scr) {
            printf("[ERROR] Failed to open custom screen with modeID 0x%lx\n", modeID);
            return FALSE;
        }
        
        struct ViewPort *vp = &scr->ViewPort;
        if (!vp) {
            printf("Could not get screen's viewport\n");
            CloseScreen(scr);
            return;
        }
        
        static const struct RGB8 pens[] = {
            {180, 180, 200}, // 0
            {0,0,0}, // 1 text color in gadgets
            {210,210,210}, // gadget border light
            {237, 119, 255}, // 3 - penPink
            {254, 204, 253}, // 4 - penLightPink
            { 52, 100, 208}, // 5 - penBlue
            {102, 203, 255}, // 6 - penLightBlue
            {237, 255, 255}, // 7 - penWhite
            {204, 136, 255}, // 8 - penPinkHit
            {254, 240, 255}, // 9 - penLightPinkTxt
            { 99, 206, 255}, // 10 - penLightBlueTxt
            { 97, 255, 254}, // 11 - penTitle
            {218, 248, 246}, // 12 - penTitleTxt
            {218, 220, 220}, // 13 - penGrid
            {102, 120, 200}, // 14 - penBG
            {180, 180, 200} // 15
        };
        
        for (UWORD i = 0; i < (sizeof(pens)/sizeof(pens[0])); i++) {
            UWORD r4 = (pens[i].r * 15) / 255;
            UWORD g4 = (pens[i].g * 15) / 255;
            UWORD b4 = (pens[i].b * 15) / 255;

            SetRGB4(vp, i, r4, g4, b4);
        }
        
        srand(time(NULL));

        if ( (vi = GetVisualInfo(scr, TAG_END)) != NULL )
        {

            struct Gadget *gad;

            /* GadTools gadgets require this step to be taken */
            gad = CreateContext(&glist);

            /* gadgets */
            ng.ng_TextAttr   = &Topaz12;
            ng.ng_VisualInfo = vi;
            ng.ng_LeftEdge   = MARGIN+24*16+32;
            ng.ng_TopEdge    = MARGIN+24*16+32 + scr->WBorTop + (scr->Font->ta_YSize + 1);
            ng.ng_Width      = 100;
            ng.ng_Height     = 36;
            ng.ng_GadgetText = "PLAY";
            ng.ng_GadgetID   = PLAY_BUTTON;
            ng.ng_Flags      = 0;
            gads[PLAY_BUTTON] = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

            ng.ng_TextAttr   = &Topaz12;
            ng.ng_VisualInfo = vi;
            ng.ng_LeftEdge   = MARGIN+32;
            ng.ng_TopEdge    = MARGIN+24*16+32 + scr->WBorTop + (scr->Font->ta_YSize + 1);
            ng.ng_Width      = 120;
            ng.ng_Height     = 36;
            ng.ng_GadgetText = "Undo ship";
            ng.ng_GadgetID   = UNDO_BUTTON;
            ng.ng_Flags      = 0;
            gads[UNDO_BUTTON] = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

            ng.ng_TextAttr   = &Topaz12;
            ng.ng_VisualInfo = vi;
            ng.ng_LeftEdge   = MARGIN+24*16+32;
            ng.ng_TopEdge    = MARGIN+24*16+32+36+20 + scr->WBorTop + (scr->Font->ta_YSize + 1);
            ng.ng_Width      = 120;
            ng.ng_Height     = 36;
            ng.ng_GadgetText = "New game";
            ng.ng_GadgetID   = NEWGAME_BUTTON;
            ng.ng_Flags      = 0;
            gads[NEWGAME_BUTTON] = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

            if (gad != NULL) {
                
                    win = OpenWindowTags (NULL,
                        WA_Title, "Battle Ship game for AmigaOS 3 - most RTG boards compatible",
                        WA_Top, 0,
                        WA_Left, 0,
                        WA_InnerWidth, 600,
                        WA_InnerHeight, 600,
                        WA_ReportMouse, TRUE,
                        WA_RMBTrap, TRUE,
                        WA_Gadgets, NULL,
                        WA_SmartRefresh, TRUE,
                        WA_CloseGadget, TRUE,
                        WA_Activate, TRUE,
                        WA_CustomScreen, (ULONG)scr,
                        WA_Flags,       WFLG_ACTIVATE | WFLG_DRAGBAR,
                        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_GADGETUP,
                        TAG_END);

                    if (win) {
                        
                        rastport = win->RPort;
                        
                        BOOL gridRegion = FALSE;
                        
                        borderTop = win->BorderTop;
                        
                        gridMarginX = MARGIN + win->BorderLeft;
                        gridMarginY = MARGIN + win->BorderTop;
                                                
                        myfont = (struct TextFont*)OpenFont(&Topaz16);
                        myfont2 = (struct TextFont*)OpenFont(&Topaz12);
                        SetDrMd(rastport,0);
                        
                        struct Gadget *gadEvent;

                        ULONG MsgClass;
                        UWORD code;
                        
                        BOOL allPlaced = FALSE;

                        BOOL prevExists = FALSE;

                        int bx, by, mx, my, pmx, pmy, fillWidth, fillHeight, bpmx, bpmy;

                        state = START_SCREEN;
                                              
                        SetAPen(rastport, penBG);
                        RectFill(rastport,win->BorderLeft,win->BorderTop,600+win->BorderLeft,600+win->BorderTop);   
                        
                        struct BitMap bm;


                        InitBitMap(&bm, 1, 320, 200);  // 1 plane, 320x200
                        bm.Planes[0] = (PLANEPTR)battleshipData;

                        WaitBlit();
                        
                        BltBitMapRastPort(&bm, 0, 0,
                            rastport, 140, 250,
                            320, 200,
                        0xC0);   // COPY
                                               
                        /*
                        *  Main event loop
                        */
                        while(!Done)
                        {

                            
                            if (state == START_SCREEN) {

                                SetAPen(rastport, penTitle);
                                SetFont(rastport, myfont);
                                Move(rastport, (600-TextLength(rastport, "Battle ship game", 16)) / 2, win->BorderTop+MARGIN + borderTop);
                                Text(rastport, "Battle ship game", 16);

                                SetFont(rastport, myfont2);
                                SetAPen(rastport, penTitleTxt);
                                Move(rastport, (600-TextLength(rastport, "Version 1.0.0", 13)) / 2, win->BorderTop+MARGIN + 40) + borderTop;
                                Text(rastport, "Version 1.0.0", 13);

                                Move(rastport, (600-TextLength(rastport, "Click anywhere in the window to continue", 40)) / 2, win->BorderTop+MARGIN + 40 + 80 + borderTop);
                                Text(rastport, "Click anywhere in the window to continue", 40);
                                
                            }
                            
                            
                            if (state != START_SCREEN) {
                                drawGrid(rastport);
                                drawShips(rastport);
                                drawBoard(rastport);
                            }

                            if (state == PLACE_SHIPS) {
                                SetAPen(rastport, penLightPinkTxt);
                                SetFont(rastport, myfont);
                                Move(rastport, 24, MARGIN + 24*16+128 + win->BorderTop + rastport->TxBaseline);
                                Text(rastport, "Positioning of ships", 20);
                            }
                            
                            if ((AIHits == 23 || plyHits == 23) && state != GAME_OVER) {
                                
                                state = GAME_OVER;
                                                WaitBlit();
                                                                                                
                                                rect.MinX = win->BorderLeft + 1;
                                                rect.MinY = win->BorderTop + 1 + MARGIN + 384 + MARGIN + 50;
                                                rect.MaxX = win->BorderLeft + 1 + 384 + MARGIN;
                                                rect.MaxY = gridMarginY + 384 + 70 + 170;
                                                                                                
                                                
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);
                                                    
                                                    if (old) DisposeRegion(old);

                                                }
                                                
                                                WaitBlit();
                                                
                                                // clear "Game on!" text
                                                SetAPen(rastport, penBG);
                                                RectFill(rastport,
                                                    win->BorderLeft, win->BorderTop + MARGIN+24*16+52,
                                                    win->BorderLeft+500, gridMarginY + 384 + 70 + 168);
                                
                                                
                                                
                                               SetFont(rastport, myfont);
                                               Move(rastport, gridMarginX + 32, gridMarginY + 24*16+140);

                                               if (AIHits == 23) {
                                                    SetAPen(rastport, penLightPinkTxt);
                                                    Text(rastport, "GAME OVER - I WON ;-)", 21);
                                                } else {
                                                if (plyHits == 23) {
                                                    SetAPen(rastport, penLightBlueTxt);
                                                    Text(rastport, "Congratulations! You won!", 25);
                                                }
                                }
                                                
                                                
                            }

                            Wait(1 << win->UserPort->mp_SigBit);
                            while ((Message = (struct IntuiMessage *)GT_GetIMsg(win->UserPort)))
                            {
                                
                                ULONG cls =  Message->Class;
                                ULONG code = Message->Code;
                                
                                if (prevExists) {
                                    pmx = mx;
                                    pmy = my;
                                }
                                
                                mx = Message->MouseX;
                                my = Message->MouseY;
                                
                                GT_ReplyIMsg((struct Message *)Message);
                                
                                switch(cls)
                                {
                                    case IDCMP_REFRESHWINDOW:
    
                                        GT_RefreshWindow(win, NULL);
                                        break;

                                    case IDCMP_GADGETUP:
                                        gadEvent = (struct Gadget *)Message->IAddress;


                                        switch (gadEvent->GadgetID) {

                                            case PLAY_BUTTON:
                                                
                                                if (state == PLAY || state == GAME_OVER) break;

                                                allPlaced = TRUE;

                                                for (int i = 0; i < 5; i++) {
                                                    if (shipsPlaced[i] == FALSE) {
                                                        allPlaced = FALSE;
                                                    }
                                                }

                                                if (allPlaced == FALSE) {

                                                    EasyRequest(win, &myES, NULL, "(Variable)", NULL);
                                                    break;
                                                }

                                                placeComputersShips();

                                                // clear "positioning of ships"
                                                /*SetAPen(rastport, penBG);
                                                RectFill(
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop + MARGIN+24*16+MARGIN,
                                                    win->BorderLeft + 384+24, win->BorderTop + MARGIN+24*16+MARGIN+100);
                                                */
                                                state = PLAY;
                                                    
                                                WaitBlit();
                                                
                                                rect.MinX = win->BorderLeft + 1;
                                                rect.MinY = win->BorderTop + 1 + MARGIN + 384;
                                                rect.MaxX = win->BorderLeft + 1 + 384 + MARGIN;
                                                rect.MaxY = win->BorderTop + 1 + MARGIN + 384 + 180;
                                                
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);
                                                    
                                                    if (old) DisposeRegion(old);
                                                }                                                
                                                
                                                SetAPen(rastport, penBG);
                                                RectFill(
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop + MARGIN+24*16+70,
                                                    win->BorderLeft + 400, win->BorderTop + MARGIN+24*16+70 + 120);
                                                
                                                
                                                SetFont(rastport, myfont2);
                                
                                                SetAPen(rastport, penBlue);
                                                RectFill(rastport, 24, MARGIN + 24*16+90-8 + 24, 24+24, MARGIN + 24*16+90+24-8 + 24);
                                                SetAPen(rastport, penLightPinkTxt);
                                                Move(rastport, 24+42, MARGIN + 24*16+90+24+8);
                                                Text(rastport, "Human player has hit AI's ship", 30);

                                                SetAPen(rastport, penLightBlue);
                                                RectFill(rastport, 24, MARGIN + 24*16+110 + 24 + 8, 24+24, MARGIN + 24*16+110+24 + 24 + 8);
                                                SetAPen(rastport, penLightPinkTxt);
                                                Move(rastport, 24+42, MARGIN + 24*16+110+16+24+8);
                                                Text(rastport, "Human player has missed AI's ship", 33);
                                
                                                SetFont(rastport, myfont);
                                                SetAPen(rastport, penLightPinkTxt);
                                                Move(rastport, 220, MARGIN + 24*16+128+40 + win->BorderTop);
                                                Text(rastport, "Game on!", 8);
                                                
                                                WaitBlit();
                                                //
                                                // playing area
                                                //
                                                
                                                rect.MinX = win->BorderLeft+8; rect.MinY = win->BorderTop+8;
                                                rect.MaxX = win->BorderLeft + 500; rect.MaxY = win->BorderTop + MARGIN + 384 + 1;                                
                                                    
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);
                                                    
                                                    if (old) DisposeRegion(old);
                                                }     
                                                    
                                                clipInstalled = TRUE;
                                                                                            
                                                break;
                                            case UNDO_BUTTON:
                                                if (state != PLACE_SHIPS) break;
                                                
                                                WaitBlit();
                                                //
                                                // playing area
                                                //
                                                rect.MinX = win->BorderLeft+8; rect.MinY = win->BorderTop+8;
                                                rect.MaxX = win->BorderLeft + 400; rect.MaxY = win->BorderTop + MARGIN + 384 + 1;                            
                                                    
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);
                                                    
                                                    if (old) DisposeRegion(old);
                                                }     
                                                
                                                SetAPen(rastport, penBG);
                                                RectFill(
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop,
                                                    win->BorderLeft + 400+MARGIN+40, win->BorderTop+384+MARGIN);
                                                
                                                undoCurrentPositioning();
                                                break;

                                            case NEWGAME_BUTTON:
                                                
                                                WaitBlit();
                                                
                                                rect.MinX = win->BorderLeft + 1;
                                                rect.MinY = win->BorderTop + 1 + MARGIN + 384 + 70;
                                                rect.MaxX = win->BorderLeft + 1 + 384 + MARGIN;
                                                rect.MaxY = win->BorderTop + 1 + 24*16+MARGIN+180;
                                                
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);
                                                    
                                                    if (old) DisposeRegion(old);
                                                }
                                                
                                                SetAPen(rastport, penBG);
                                                RectFill(rastport,
                                                    win->BorderLeft, win->BorderTop + MARGIN+24*16+60,
                                                    win->BorderLeft+400, win->BorderTop + MARGIN+24*16+170);
                                                
                                                SetAPen(rastport, penLightPinkTxt);
                                                SetFont(rastport, myfont);
                                                Move(rastport, 24, MARGIN + 24*16+128 + win->BorderTop + rastport->TxBaseline);
                                                Text(rastport, "Positioning of ships", 20);
                                
                                                // playing area
                                                WaitBlit();
                                                rect.MinX = win->BorderLeft+8; rect.MinY = win->BorderTop+8;
                                                rect.MaxX = win->BorderLeft + 500; rect.MaxY = win->BorderTop + MARGIN + 384 + 1;                            
                                                    
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);
                                                    
                                                    if (old) DisposeRegion(old);
                                                }     
                                                
                                                SetAPen(rastport, penBG);
                                                RectFill(
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop,
                                                    win->BorderLeft + 400+MARGIN+40, win->BorderTop + 384+MARGIN);
                                                                                                
                                                initGame();

                                                state = PLACE_SHIPS;
                                                break;
                                        
                                        
                                        }

                                        

                                        break;
                                    
                                    
                                    case IDCMP_CLOSEWINDOW:
                                            Done = TRUE;
                                            break;
                                            
                                    case IDCMP_MOUSEBUTTONS:         
                                        
                                        // right mouse button to rotate ship
                                        if (code == 233) {
                                            
                                            if (state == PLACE_SHIPS) {

                                                switch (shipSelected) {

                                                    case 1:
                                                        rotateShip(ship1, 3, 1);
                                                        break;
                                                    case 2:
                                                        rotateShip(ship2, 3, 2);
                                                        break;

                                                    case 3:
                                                        rotateShip(ship3, 2, 3);
                                                        break;

                                                    case 4:
                                                        rotateShip(ship4, 4, 4);
                                                        break;

                                                    case 5:
                                                        rotateShip(ship5, 5, 5);
                                                        break;

                                                }
                                            
                                            }

                                        }
                                        
                                        // right and left mousebutton related...
                                        if (code == 105 || code == 104) break;

                                        // left mousebutton 
                                        if (code == 232) {
                                                                                        
                                            if (prevExists) {
                                                bpmx = mx;
                                                bpmy = my;
                                            }

                                            if (state == START_SCREEN) {
                                                
                                                // clear start screen..
                                                SetAPen(rastport, penBG);
                                                RectFill(
                                                rastport,
                                                win->BorderLeft, win->BorderTop,
                                                win->BorderLeft + 500, win->BorderTop + 550);
                                            

                                                AddGList(win, glist, 0, -1, NULL);
                                                RefreshGList(win->FirstGadget, win, NULL, -1);

                                                state = PLACE_SHIPS;
                                                
                                                
                                                
                                                break;
                                            
                                            }
                                            
                                            
                                            if (state == PLACE_SHIPS) {
                        
                                                if (shipSelected != 0) {
                                                    int height = 0;
                                                    
                                                    switch (shipSelected) {
                                                        case 1:
                                                            height = 3;
                                                        case 2:
                                                            height = 3;
                                                        case 3:
                                                            height = 2;
                                                        case 4:
                                                            height = 4;
                                                        case 5:
                                                            height = 5;
                                                    }
                                                    
                                                    bx = (mx - gridMarginX) / 24;
                                                    by = (my - gridMarginX) / 24;
                                                    
                                                        SetAPen(rastport, penBG);
                                                        RectFill(rastport,
                                                            bpmx+win->BorderLeft, bpmy+win->BorderTop,
                                                            bpmx+win->BorderLeft + fillWidth+1, bpmy+win->BorderTop + fillHeight+1);
                                            
                                                        switch (shipSelected) {
                                                            case 1:

                                                                if (areaClear(&ship1, bx, by, 3)) {
                                                                    backupPreviousBoard(1);

                                                                    for (int j = 0; j < 3; j++) {
                                                                        for (int i = 0; i < 3; i++) {
                                                                            if (ship1[i + j*3] == 1) {
                                                                                board[bx + i + (by + j)*16] = 1;
                                                                            }
                                                                        }
                                                                    }       
                                                                    shipsPlaced[0] = TRUE;
                                                                    shipSelected = 0;
                                                                }
                                                                break;

                                                            case 2:
                                                                if (areaClear(&ship2, bx, by, 3)) {
                                                                    backupPreviousBoard(2);

                                                                    for (int j = 0; j < 3; j++) {
                                                                        for (int i = 0; i < 3; i++) {
                                                                            if (ship2[i + j*3] == 1) {
                                                                                board[bx + i + (by + j)*16] = 1;
                                                                            }
                                                                        }
                                                                    }
                                                                    shipsPlaced[1] = TRUE;
                                                                    shipSelected = 0;
                                                                }
                                                                break;

                                                            case 3:
                                                            
                                                                if (areaClear(&ship3, bx, by, 2)) {
                                                                    backupPreviousBoard(3);

                                                                    for (int j = 0; j < 2; j++) {
                                                                        for (int i = 0; i < 2; i++) {
                                                                            if (ship3[i + j*2] == 1) {
                                                                                board[bx + i + (by + j)*16] = 1;
                                                                            }
                                                                        }
                                                                    }
                                                                    shipsPlaced[2] = TRUE;
                                                                    shipSelected = 0;
                                                                }
                                                                break;

                                                            case 4:
                                                            
                                                                if (areaClear(&ship4, bx, by, 4)) {
                                                                    backupPreviousBoard(4);

                                                                    for (int j = 0; j < 4; j++) {
                                                                        for (int i = 0; i < 4; i++) {
                                                                            if (ship4[i + j*4] == 1) {
                                                                                board[bx + i + (by + j)*16] = 1;
                                                                            }
                                                                        }
                                                                    }
                                                                    shipsPlaced[3] = TRUE;
                                                                    shipSelected = 0;
                                                                }
                                                                break;

                                                            case 5:
                                                            
                                                                if (areaClear(&ship5, bx, by, 5)) {
                                                                    backupPreviousBoard(5);

                                                                    for (int j = 0; j < 5; j++) {
                                                                        for (int i = 0; i < 5; i++) {
                                                                            if (ship5[i + j*5] == 1) {
                                                                                board[bx + i + (by + j)*16] = 1;
                                                                            }
                                                                        }
                                                                    }
                                                                    shipsPlaced[4] = TRUE;
                                                                    shipSelected = 0;
                                                                }
                                                                break;
                                                        }
                                                    
                                                    break;
                                                }
                                                
                                                
                                                    
                                                if (mx >= gridMarginX + 384 + 30 && mx <= gridMarginX + 30 + 384 + 24*3 && my >= gridMarginY && my <= gridMarginY + 24*3) {
                                                        if (shipsPlaced[0] == TRUE) break;
                                                        angle_ = 0;
                                                        shipSelected = 1;
                                                        initShip(1);

                                                        break;
                                                }

                                                if (mx >= gridMarginX + 384 + 30 && mx <= gridMarginX + 384 + 30 + 24*3 && my >= gridMarginY + 24*3 + 24 && my <= gridMarginY + 24*3 + 24 + 24) {
                                                        if (shipsPlaced[1] == TRUE) break;
                                                        angle_ = 0;
                                                        shipSelected = 2;

                                                        initShip(2);
                                                        break;
                                                }

                                                if (mx >= gridMarginX + 384 + 30 && mx <= gridMarginX + 384 + 30 +  24*2 && my >= gridMarginY + 24*3 + 24 + 24 + 24 && my <= gridMarginY + 24*3 + 24*3 + 24*2) {
                                                        if (shipsPlaced[2] == TRUE) break;
                                                        angle_ = 0;
                                                        shipSelected = 3;

                                                        initShip(3);

                                                        break;
                                                }

                                                if (mx  >= gridMarginX + 384 + 30 && mx <= gridMarginX + 384 + 30 + 24*4 && my >= gridMarginY + 24*3 + 24 + 24*3 && my <= gridMarginY + 24 *3 + 24 + 24*3 + 24*4) {
                                                        if (shipsPlaced[3] == TRUE) break;
                                                        angle_ = 0;
                                                        shipSelected = 4;

                                                        initShip(4);

                                                        break;
                                                }

                                                if (mx + win->BorderLeft >= MARGIN + win->BorderLeft + 384 + 64 && mx + win->BorderLeft <= MARGIN + win->BorderLeft + 384 + 64 + 24*5 && my + win->BorderTop >= MARGIN + win->BorderTop + 24 + 24*2 + 24 + 24 + 24*2 + 24 + 24*4 + 24 && my + win->BorderTop <= MARGIN + win->BorderTop + 24 + 24*2 + 24 + 24 + 24*2 + 24 + 24*4 + 24 + 24*4 + 24) {
                                                        if (shipsPlaced[4] == TRUE) break;
                                                        angle_ = 0;
                                                        shipSelected = 5;

                                                        initShip(5);

                                                        break;
                                                
                                                }
                                            
                                            }
                                            
                                            if (state == PLAY) {
                                                    bx = (mx - gridMarginX) / 24;
                                                    by = (my - gridMarginY) / 24;

                                                    //printf("bx=%d :: by=%d\n", bx, by);
                                                    
                                                    if (bx >= 0 && bx < 16 & by >= 0 && by < 16) {
                                                        
                                                        if (board[bx + by * 16] == 0) {
                                                            // player's miss
                                                            board[bx + by * 16] = 3;
                                                        } else {
                                                            // player's hit
                                                            if (board[bx + by * 16] == 2) {
                                                                board[bx + by * 16] = 4;
                                                                plyHits++;
                                                            } else {
                                                                //printf("Choose another square\n");
                                                                break;
                                                            }
                                                        }
                                                
                                                        AImove();

                                                    }
                                            }
                                            break;
                                        }
                                    case IDCMP_MOUSEMOVE:
                                         
                                        if (shipSelected != 0 && state == PLACE_SHIPS) {
                                            
                                            // as long as these settings are in order, in-game texts can't be printed
                                            if (gridRegion == FALSE) {
                                                    WaitBlit();
                                                    
                                                    rect.MinX = win->BorderLeft+8; rect.MinY = win->BorderTop+8;
                                                    rect.MaxX = win->BorderLeft + 500; rect.MaxY = win->BorderTop + MARGIN + 384 + 1;                            
                                                    
                                                    if ((newRegion = (struct Region *) NewRegion())) {
                                                        OrRectRegion(newRegion, &rect);

                                                        LockLayer(0, win->WLayer);
                                                        old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                        UnlockLayer(win->WLayer);
                                                    
                                                        if (old) DisposeRegion(old);
                                                    }     
                                                    
                                                    gridRegion = TRUE;
                                                    clipInstalled = TRUE;
                                                }
                                            
                                            int height = 0;
                                            
                                                switch (shipSelected) {
                                                        case 1:
                                                            height = 3;
                                                            
                                                            break;
                                                            
                                                        case 2:
                                                            height = 3;
                                                            
                                                            break;
                                                            
                                                        case 3:
                                                            height = 2;
                                                            
                                                            break;
                                                            
                                                        case 4:
                                                            height = 4;
                                                            
                                                            break;
                                                            
                                                        case 5:
                                                            height = 5;
                                                            
                                                            break;
                                                            
                                                    }
                                        
                                            
                                            // bx = (mx + MARGIN + win->BorderLeft + 15) / 24 - 1;
                                            // by = (my + MARGIN + win->BorderTop + height) / 24 - 2;
                                            
                                            //printf("%d %d \n",bx, by);
                                            

                                            if (prevExists) {
                                                
                                                // if (by > 15) fillHeight = (by - 15) * 24;
                                                            SetAPen(rastport, penBG);
                                                            RectFill(
                                                                rastport,
                                                                pmx+win->BorderLeft, pmy+win->BorderTop,
                                                                pmx+win->BorderLeft + fillWidth+1, pmy+win->BorderTop + fillHeight+1);
                                                    
                                            }
                                        }
                                            
                                            prevExists = TRUE;
                                            
                                            SetAPen(rastport, penWhite);

                                            switch (shipSelected) {
                                                case 1:
                                                    
                                                    fillWidth = 3*24;
                                                    fillHeight = 3*24;

                                                    for (int j = 0; j < 3; j++) {
                                                        for (int i = 0; i < 3; i++) {

                                                            if (ship1[i+j*3] == 1) {

                                                                //if (!(mx + i * 24 >= MARGIN + 16 * 24 - win->BorderRight || mx + i * 24 <= win->BorderLeft
                                                                //    || my + j * 24 <= win->BorderTop + MARGIN || my + j*24 >= MARGIN + 16 * 24 + win->BorderTop)) {

                                                                    RectFill(rastport, mx + win->BorderLeft + i*24, my + win->BorderTop + j*24, mx + win->BorderLeft + i*24+24, my + win->BorderTop + j*24+24);
                                                                //}
                                                            }
                                                        } 
                                                    }
                                                    break;

                                                case 2:
                                                
                                                    fillWidth = 3*24;
                                                    fillHeight = 3*24;

                                                    for (int j = 0; j < 3; j++) {
                                                        for (int i = 0; i < 3; i++) {
                                                            if (ship2[i+j*3] == 1) {

                                                                //if (!(mx + i * 24 >= MARGIN + 16 * 24 - win->BorderRight || mx + i * 24 <= win->BorderLeft
                                                                //    || my + j * 24 <= win->BorderTop + MARGIN || my + j*24 >= MARGIN + 16 * 24 + win->BorderTop)) {

                                                                    RectFill(rastport, mx + win->BorderLeft + i*24, my + win->BorderTop + j*24, mx + win->BorderLeft + i*24+24, my + win->BorderTop + j*24+24);
                                                                //}
                                                            }
                                                        } 
                                                    }
                                                    break;

                                                case 3:

                                                    fillWidth = 2*24;
                                                    fillHeight = 2*24;

                                                    for (int j = 0; j < 2; j++) {
                                                        for (int i = 0; i < 2; i++) {
                                                            if (ship3[i+j*2] == 1) {

                                                                //if (!(mx + i * 24 >= MARGIN + 16 * 24 - win->BorderRight || mx + i * 24 <= win->BorderLeft
                                                                //    || my + j * 24 <= win->BorderTop + MARGIN || my + j*24 >= MARGIN + 16 * 24 + win->BorderTop)) {

                                                                    RectFill(rastport, mx + win->BorderLeft + i*24, my + win->BorderTop + j*24, mx + win->BorderLeft + i*24+24, my + win->BorderTop + j*24+24);
                                                                //}
                                                            }
                                                        } 
                                                    }
                                                    break;

                                                case 4:

                                                    fillWidth = 4*24;
                                                    fillHeight = 4*24;

                                                    for (int j = 0; j < 4; j++) {
                                                        for (int i = 0; i < 4; i++) {
                                                            if (ship4[i+j*4] == 1) {

                                                                //if (!(mx + i * 24 >= MARGIN + 16 * 24 - win->BorderRight || mx + i * 24 <= win->BorderLeft
                                                                //    || my + j * 24 <= win->BorderTop + MARGIN || my + j*24 >= MARGIN + 16 * 24 + win->BorderTop)) {

                                                                    RectFill(rastport, mx + win->BorderLeft + i*24, my + win->BorderTop + j*24, mx + win->BorderLeft + i*24+24, my + win->BorderTop + j*24+24);
                                                                //}
                                                            }
                                                        } 
                                                    }
                                                    break;

                                                case 5:

                                                    fillHeight = 5*24;
                                                    fillWidth = 5*24;

                                                    for (int j = 0; j < 5; j++) {
                                                        for (int i = 0; i < 5; i++) {
                                                            if (ship5[i+j*5] == 1) {

                                                                //if (!(mx + i * 24 >= MARGIN + 16 * 24 - win->BorderRight || mx + i * 24 <= win->BorderLeft
                                                                //    || my + j * 24 <= win->BorderTop + MARGIN || my + j*24 >= MARGIN + 16 * 24 + win->BorderTop)) {

                                                                    RectFill(rastport, mx + win->BorderLeft + i*24, my + win->BorderTop + j*24, mx + win->BorderLeft + i*24+24, my + win->BorderTop + j*24+24);
                                                                //}
                                                            }
                                                        } 
                                                    }
                                                    break;
                                                
                                    
                                            }                                                     

                                        
                                    break;
                                }
                                
                                
                            }
                        }
                        
                        WaitBlit();
                        
                        if (clipInstalled) {
                            LockLayer(0, win->WLayer);
                                struct Region *old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                if (old) DisposeRegion(old);
                            UnlockLayer(win->WLayer);
                        }
                        
                        RemoveGList(win, glist, -1);
                        CloseWindow(win);
                    } else {
                        printf("Could not open the window. Is resolution too small?\n");
                    }
                
            } else {
                printf("Couldn't make gadget list\n");
            }
            FreeGadgets(glist);

        } else {
            printf("Couldn't get VisualInfo\n");
        }
            
                
    
    FreeVisualInfo(vi);
    
    CloseScreen(scr);

    return;
}

 int main()
    {
        IntuitionBase       = OpenLibrary("intuition.library", 39);
        GfxBase             = OpenLibrary("graphics.library", 39);
        UtilityBase         = OpenLibrary("utility.library", 39);
        LayersBase          = OpenLibrary("layers.library", 39);
        GadToolsBase        = OpenLibrary("gadtools.library", 39);
        
        if (!IntuitionBase || !GfxBase || !UtilityBase || !LayersBase || !GadToolsBase) {
            
            if (IntuitionBase) CloseLibrary(IntuitionBase); else printf("intuition.library version 39 or newer not found\n");
            if (GfxBase) CloseLibrary(GfxBase); else printf("graphics.library version 39 or newer not found\n");
            if (UtilityBase) CloseLibrary(UtilityBase); else printf("utility.library version 39 or nevwer not found\n");
            if (LayersBase) CloseLibrary(LayersBase); else printf("layers.library version 39 or newer not found\n");
            if (GadToolsBase) CloseLibrary(GadToolsBase); else printf("gadtools.library version 39 or newer not found\n");
            return -1;
        }

        ULONG displayID = INVALID_ID;
        struct DimensionInfo diminfo;
        BOOL found = FALSE;
        ULONG w, h;

        while ((displayID = NextDisplayInfo(displayID)) != INVALID_ID) {
            if (GetDisplayInfoData(NULL, (UBYTE *)&diminfo, sizeof(diminfo), DTAG_DIMS, displayID)) {
                w = diminfo.Nominal.MaxX - diminfo.Nominal.MinX + 1;
                h = diminfo.Nominal.MaxY - diminfo.Nominal.MinY + 1;
                
                if (w > 600 && h > 600) {
                    printf("Found modeID 0x%lx: %lux%lu\n", displayID, w, h);
                    modeID = displayID;
                    screenWidth = w;
                    screenHeight = h;
                    found = TRUE;
                    break;
                }
            }
        }

        if (!found) {
            printf("No screen mode larger than 600x600 was found.\n");
            return 0;
        }
    
        startPrg();

        CloseLibrary(IntuitionBase);
        CloseLibrary(UtilityBase);
        CloseLibrary(GfxBase);
        CloseLibrary(GadToolsBase);
        CloseLibrary(LayersBase);
        
        return 0;

    }


    void drawBoard(struct RastPort *rp) {
        
        for (int j = 0; j < 16; j++) {
            for (int i = 0; i < 16; i++) {

                // player's ship
                SetAPen(rp, penWhite);
                if (board[i + j * 16] == 1) {
                    RectFill(rp, i * 24+gridMarginX, j * 24 + gridMarginY, i * 24 + 24-1 + gridMarginX, j * 24 + 24-1 + gridMarginY);
                }

                // computer's ship
                SetAPen(rp, penBlue);
                if (state == GAME_OVER && AIHits == 23) {
                    if (board[i + j * 16] == 2) {
                        RectFill(rp, i * 24 + gridMarginX, j * 24 + gridMarginY, i * 24 + 24-1 + gridMarginX, j * 24 + 24-1 + gridMarginY);
                    }
                }

                // player's miss
                SetAPen(rp, penLightBlue);
                if (board[i + j * 16] == 3) {
                    RectFill(rp, i * 24+ gridMarginX, j * 24+ gridMarginY, i * 24 + 24-1 + gridMarginX, j * 24 + 24-1+gridMarginY);
                }

                // player's hit
                if (board[i + j * 16] == 4) {
                    SetAPen(rp, penBlue);
                    RectFill(rp, i * 24 + gridMarginX, j * 24 + gridMarginY, i * 24 + 24-1 + gridMarginX, j * 24 + 24-1 + gridMarginY);
                }

                // computer's miss
                if (board[i + j * 16] == 5) {
                    SetAPen(rp, penLightPink);
                    RectFill(rp, i * 24 + gridMarginX, j * 24 + gridMarginY, i * 24 + 24-1 + gridMarginX, j * 24 + 24-1 + gridMarginY);
                }

                // computer's hit
                if (board[i + j * 16] == 6) {
                    SetAPen(rp, penPinkHit);
                    RectFill(rp, i * 24 + gridMarginX, j * 24+gridMarginY, i * 24 + 24-1 + gridMarginX, j * 24 + 24-1 + gridMarginY);
                }

                
            }
        }
    }

    void drawShips(struct RastPort *rp) {

        SetAPen(rp, penPink);

        // draw ship 1
        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 3; i++) {
                if (ship1_[i+j*3] == 1) RectFill(rp, gridMarginX + 384+30+i*24, gridMarginY+j*24-2, gridMarginX + 384+30+i*24+24, gridMarginY+24+j*24);
            }
        }

        // draw ship 2
        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 3; i++) {
                if (ship2_[i+j*3] == 1) RectFill(rp, gridMarginX+384+30+i*24, gridMarginY+j*24 + 24*4, gridMarginX+384+30+i*24+24, gridMarginY + j*24 + 24*4 + 24);
            }
        }

        // draw ship 3
        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 2; i++) {
                if (ship3_[i+j*3] == 1) RectFill(rp, gridMarginX+384+30+i*24, gridMarginY+j*24 + 24*4 + 24*2, gridMarginX+384+30+i*24+24, gridMarginY+j*24 + 24*4 + 24*2 + 24);
            }
        }

        // draw ship 4
        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 4; i++) {
                if (ship4_[i+j*4] == 1) RectFill(rp, gridMarginX+384+30+i*24, gridMarginY+j*24 + 24*4 + 24*2 + 24*3, gridMarginX+384+30+i*24+24, gridMarginY+j*24 + 24*4 + 24*2 + 24*3 + 24);
            }
        }

        // draw ship 5
        for (int j = 0; j < 5; j++) {
            for (int i = 0; i < 5; i++) {
                if (ship5_[i+j*5] == 1) RectFill(rp, gridMarginX+384+30+i*24, gridMarginY+j*24 + 24*4 + 24*2 + 24*3 + 24*5, gridMarginX+384+30+i*24+24, gridMarginY+j*24 + 24*4 + 24*2 + 24*3 + 24*5 + 24);
            }
        }
    }

    void drawGrid(struct RastPort *rp) {

        SetAPen(rp, penGrid);
        
        Move(rp, MARGIN+win->BorderLeft,MARGIN+borderTop);
        Draw(rp, MARGIN+384+win->BorderLeft,MARGIN+borderTop);

        Move(rp, MARGIN+384+win->BorderLeft,MARGIN+borderTop);
        Draw(rp, MARGIN+384+win->BorderLeft,MARGIN+384+borderTop);

        Move(rp, MARGIN+384+win->BorderLeft,MARGIN+384+borderTop);
        Draw(rp, MARGIN+win->BorderLeft, MARGIN+384+borderTop);

        Move(rp, MARGIN+win->BorderLeft, MARGIN+384+borderTop);
        Draw(rp, MARGIN+win->BorderLeft, MARGIN+borderTop);

        for (int j = MARGIN+24; j < MARGIN+384; j+=24) {
            Move(rp, MARGIN+win->BorderLeft, j+borderTop);
            Draw(rp, MARGIN+win->BorderLeft+384, j+borderTop);
        }

        for (int i = MARGIN+24; i < MARGIN+384; i+=24) {
            Move(rp, i+win->BorderLeft, MARGIN+borderTop);
            Draw(rp, i+win->BorderLeft, MARGIN+384+borderTop);
        }
        
    }
    
    void printBoard() {
        for (int j = 0; j < 16; j++) {
            for (int i = 0; i < 16; i++) {
                printf("%d",board[i+j*16]);
            }
            printf("\n");
        }
    }

    void initShip(int n) {
        switch(n) {
            case 1:
                for (int j = 0; j < 3; j++) {
                    for (int i = 0; i < 3; i++) {
                        ship1[i+j*3] = ship1_[i+j*3];
                    }
                }
                break;

            case 2:
                for (int j = 0; j < 3; j++) {
                    for (int i = 0; i < 3; i++) {
                        ship2[i+j*3] = ship2_[i+j*3];
                    }
                }
                break;

            case 3:
                for (int j = 0; j < 2; j++) {
                    for (int i = 0; i < 2; i++) {
                        ship3[i+j*2] = ship3_[i+j*2];
                    }
                }
                break;
            case 4:
                for (int j = 0; j < 4; j++) {
                    for (int i = 0; i < 4; i++) {
                        ship4[i+j*4] = ship4_[i+j*4];
                    }
                }
                break;

            case 5:
                for (int j = 0; j < 5; j++) {
                    for (int i = 0; i < 5; i++) {
                        ship5[i+j*5] = ship5_[i+j*5];
                    }
                }
                break;
        }
    }

    BOOL areaClear(int *ship, int x, int y, int length) {

        for (int j = 0; j < length; j++) {
            for (int i = 0; i < length; i++) {

                if (ship[i+j*length] == 1) {
                    if (x+i < 0 || x+i > 15 || y+j > 15 || y+j < 0) return FALSE;
                    if (board[x+i+(y+j)*16] != 0) return FALSE;
                }
            }
        }

        return TRUE;
    }

    void AImove() {
        
        int AIx, AIy;

        int piecesOfPly[23];

        int k = 0;

        int target, targetPoint;

        BOOL again = FALSE;

        do {
            for (int j = 0; j < 16; j++) {
                for (int i = 0; i < 16 i++) {

                    if (error > 0) {
                    // player's move
                        if (board[i + j*16] == 1) {
                            AIx = i + (rand() * 11341671) % error - (error-1);
                            AIy = j + (rand() * 11341671) % error - (error-1);
                            
                            piecesOfPly[k] = AIx + AIy * 16 + (rand() * 11341671) % error - (error-1);
                            k++;
                        }
                    } else {
                        if (board[i + j*16] == 1) {
                            AIx = i;
                            AIy = j;
                            piecesOfPly[k] = AIx + AIy * 16;
                            k++;
                        }
                    }
                    
                }
            }

            target = (rand() * 11341671) % (k+1);
            
            targetPoint = piecesOfPly[target];

            AIx = (targetPoint % 16);
            AIy = (targetPoint / 16); 

            k = 0;

            if (AIx < 0 || AIx > 15 || AIy < 0 || AIy > 15) {
                again = TRUE;
            
            } else {

                if (board[AIx + AIy * 16] == 1) {
                    board[AIx + AIy *16] = 6;
                    if (plyHits < 23) AIHits++;
                    again = FALSE;

                } else {
                    if (board[AIx + AIy * 16] == 0) {
                        board[AIx + AIy *16] = 5;
                        again = FALSE;

                    } else {
                        if (error > 0) {
                            errorDelay+=DIFFICULTY;
                            if (errorDelay >= 1.0f) {
                                error--;
                                errorDelay = 0.0f;
                                again = TRUE;
                            } else {
                                again = TRUE;
                            }
                        }
                
                    }
                }
            }
        } while(again);
    }

    void placeComputersShips() {

        int *compShip;
        int x, y, i, j;

        // ship 1
        compShip = &ship1;

        j = (rand() * 11337799) % 4 + 1;
        for (i = 1; i < j; i++) {
            rotateShip(compShip, 3, i);
        }
        
        x = (rand() * 11337799) % 13;
        y = (rand() * 11337799) % 13;

        while (!areaClear(compShip, x, y, 3)) {
            x = (rand() * 12345678) % 13;
            y = (rand() * 12345678) % 13;
        }

        for (j = 0; j < 3; j++) {
            for (i = 0; i < 3; i++) {
                if (compShip[i+j*3] == 1) board[x+i+(y+j)*16] = 2;
            }
        }

        // ship 2
        compShip = &ship2;

            
        j = (rand() * 100) % 4 + 1;
        for (i = 1; i < j; i++) {
            rotateShip(compShip, 3, i);
        }
        

        x = (rand() * 11337799) % 13;
        y = (rand() * 11337799) % 13;

        while (!areaClear(compShip, x, y, 3)) {
            x = (rand() * 11337799) % 13;
            y = (rand() * 11337799) % 13;
        }

        
        for (j = 0; j < 3; j++) {
            for (i = 0; i < 3; i++) {
                if (compShip[i+j*3] == 1) board[x+i+(y+j)*16] = 2;
            }
        }

        // ship 3

        compShip = &ship3;

        x = (rand() * 11337799) % 14;
        y = (rand() * 11337799) % 14;

        while (!areaClear(compShip, x, y, 2)) {
            x = (rand() * 11337799) % 14;
            y = (rand() * 11337799) % 14;
        }

        for (j = 0; j < 2; j++) {
            for (i = 0; i < 2; i++) {
                if (compShip[i+j*2] == 1) board[x+i+(y+j)*16] = 2;
            }
        }

        // ship 4

        compShip = &ship4;

        // random rotation of ship
        j = (rand() * 11337799) % 4 + 1;
        for (i = 1; i < j; i++) {
            rotateShip(compShip, 4, i);
        }

        x = (rand() * 11337799) % 12;
        y = (rand() * 11337799) % 12;

        while (!areaClear(compShip, x, y, 4)) {
            x = (rand() * 11337799) % 12;
            y = (rand() * 11337799) % 12;
        }

        for (j = 0; j < 4; j++) {
            for (i = 0; i < 4; i++) {
                if (compShip[i+j*4] == 1) board[x+i+(y+j)*16] = 2;
            }
        }

        // ship 5

        compShip = &ship5;

        // random rotation of ship
        j = (rand() * 11337799) % 4 + 1;
        for (i = 1; i < j; i++) {
            rotateShip(compShip, 5, i);
        }

        x = (rand() * 11337799) % 11;
        y = (rand() * 11337799) % 11;

        while (!areaClear(compShip, x, y, 5)) {
            x = (rand() * 11337799) % 11;
            y = (rand() * 11337799) % 11;
        }

        for (j = 0; j < 5; j++) {
            for (i = 0; i < 5; i++) {
                if (compShip[i+j*5] == 1) board[x+i+(y+j)*16] = 2;
            }
        }

        //printBoard();

        return;
    }

    void initGame() {

        for (int i = 0; i < 5; i++) {
            shipsPlaced[i] = FALSE;
        }

        for (int i = 0; i < 16*16; i++) board[i] = 0;

        shipSelected = 0;
        error = 6;
        errorDelay = 0.0f;
        AIHits = 0;
        plyHits = 0;

    }

    void backupPreviousBoard(int nr) {

        undoShip = nr;
        for (int i = 0; i < 16*16; i++) {
            undoBoard[i] = board[i];
        }
    }

    void undoCurrentPositioning() {

        for (int i = 0; i < 16*16; i++) {
            board[i] = undoBoard[i];
        }

        shipsPlaced[undoShip-1] = FALSE;

    }

    int rotateShip(int *ship, int length, int nr) {

        int rotatedShip[length*length];

        angle_+=1;
        if (angle_ == 5) {
            angle_ = 1;
            initShip(nr);
        }

        switch (angle_) {
            case 1:

                for (int j = 0; j < length; j++) {
                    for (int i = 0; i < length; i++) {
                        if (ship[(length-i-1)+j*length] == 1) rotatedShip[i*length + j] = 1; else rotatedShip[i*length+j] = 0;
                    }
                }
                break;

            case 2:

                for (int j = 0; j < length; j++) {
                    for (int i = 0; i < length; i++) {
                        if (ship[i + j*length] == 1) rotatedShip[j + (length-i-1)*length] = 1; else rotatedShip[j + (length-i-1)*length] = 0;
                    }
                }

                break;
            case 3:
                
                for (int j = 0; j < length; j++) {
                    for (int i = 0; i < length; i++) {
                        if (ship[(length-i-1) +(length-j-1)*length] == 1) rotatedShip[i*length + (length-j-1)] = 1; else rotatedShip[i*length + (length-j-1)] = 0;
                    }
                }
                break;

            case 4:
        
                for (int j = 0; j < length; j++) {
                    for (int i = 0; i < length; i++) {
                        if (ship[i + j*length] == 1) rotatedShip[(length-i-1)*length + j] = 1; else rotatedShip[(length-i-1)*length + j] = 0;
                    }
                }
                
                break;

        }

        /* print ship */
        /*
        printf("\n")
        for (int j = 0; j < length; j++) {
            for (int i = 0; i < length; i++) {
                if (rotatedShip[i + j*length] == 1) printf("1"); else printf("0");
            }
            printf("\n");
        }
        */

        for (int j = 0; j < length; j++) {
            for (int i = 0; i < length; i++) {
                ship[i + j*length] = rotatedShip[i + j*length];
            }
        }
        
    }
