/*
        Battle ship game for Amiga a'la spaghetti...

        Version 1.4.0

        IMPORTANT:

        Can be (probably) ONLY be compiled with VBCC
        --------------------------------------------

        With VBCC: vc -c99 Battleship-AmigaOS3.c -o Battleship -lamiga -fpu=688881

        While compiling, you'll get a lot of warning from VBCC.
        If necessary, increase stack (i.e. stack 20000 in AmigaDOS).

        You can adjust the difficulty of the game by increasing or decreasing
        constant DIFFICULTY (and variable error).
        
        I taught myself new things while programming this.
        In my youth I thought I would never use layers.library.
        Now I have.
        
        This version probably still has bugs!
        
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <dos/dos.h>
#include <exec/memory.h>
#include <exec/types.h>

#include <exec/interrupts.h>
#include <exec/io.h>

#include <datatypes/pictureclass.h>
#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/regions.h>   // <-- tämä on oleellinen
#include <graphics/layers.h>
#include <intuition/intuition.h>
#include <intuition/icclass.h>
#include <intuition/gadgetclass.h>
#include <libraries/gadtools.h>
#include <utility/hooks.h>

#include <clib/amigaguide_protos.h>
#include <clib/datatypes_protos.h>
#include <clib/intuition_protos.h>
#include <clib/utility_protos.h>
#include <clib/alib_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/macros.h>

#define PLAY_BUTTON     (0)
#define UNDO_BUTTON     (1)
#define NEWGAME_BUTTON  (2)

#define MARGIN 32

    /*
        Below should be found from include files, but just in case...
    */

#define PDTA_DestMode		(DTA_Dummy + 251)
#define PMODE_V43 (1)	/* Extended mode */

#define START_SCREEN    0
#define PLACE_SHIPS     1
#define PLAY            2
#define GAME_OVER       3

int state;

struct Library *IntuitionBase = NULL;
struct Library *DataTypesBase = NULL;
struct Library *UtilityBase = NULL;
struct Library *GfxBase = NULL;
struct Library *GadToolsBase = NULL;
struct Library *DiskfontBase = NULL;
struct Library *LayersBase = NULL;

struct Window *win = NULL;

struct BitMap *gBitMap;
Object *dto;

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

ULONG penPink;          // (237,119,255)
ULONG penLightPink;     // (254,204,253)
ULONG penBlue;          // (52,100,208)
ULONG penLightBlue;     // (102,203,255)
ULONG penWhite;         // (237,255,255)
ULONG penPinkHit;       // (204,136,255)
ULONG penLightPinkTxt;  // (254,240,255)
ULONG penLightBlueTxt;  // (99,206,255)

ULONG penTitle;
ULONG penTitleTxt;
ULONG penGrid;

struct TextAttr Topaz120 = { "topaz.font", 12, 0, 0, };
struct TextAttr myta = {"CGTimes.font", 72, 0, 0};
struct TextFont *myfont, *myfont2;

struct Gadget    *glist, *gads[3];
struct NewGadget ng;
void             *vi;

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

struct BackFillInfo
{
	struct Hook            Hook;
	struct Screen         *Screen;
	Object                *PictureObject;
	struct BitMapHeader   *BitMapHeader;
	struct BitMap         *BitMap;
	WORD                   CopyWidth;
	WORD                   CopyHeight;
};

struct BackFillMsg
{
	struct Layer    *Layer;
	struct Rectangle Bounds;
	LONG             OffsetX;
	LONG             OffsetY;
};

BOOL shipsPlaced[5] = {FALSE, FALSE, FALSE, FALSE, FALSE};

int AIHits = 0;
int plyHits = 0;

struct BackFillInfo BF1, *Backfill;

ULONG RGB32(ULONG val)
{
    return ((ULONG)val * 0x01010101UL);
}


// these settings might ONLY work with VBCC compiler!
void __saveds MyBackfillFunc(
        __reg("a0") struct Hook *Hook,
        __reg("a2") struct RastPort *rp,
        __reg("a1") struct BackFillMessage *bfa)

    {
        
        struct BackFillInfo *bfi = (struct BackFillInfo *)Hook->h_Data;

        //if (bfi == NULL || gBitMap == NULL) return;
        
        int dx = (rp->Layer->Width  - bfi->BitMapHeader->bmh_Width)  / 2;
        //int dy = bfi->Screen->BarHeight + 1;


        /* Piirrä vain rajatulle alueelle */
    BltBitMapRastPort(
        gBitMap,
        0, 0,      /* lähde alku */
        rp,
        dx, borderTop,
        800, 800,
        0xC0       /* COPY */
    );
}

void startPrg()
{
        struct Region *clipRegion;
        struct Region *old, *newRegion;
        struct Rectangle rect;
        
        struct Hook backfillHook;
        
        BOOL clipInstalled = FALSE;
        
        int width, height;
        struct RastPort *rastport;
        struct TextFont *font;
        struct IntuiMessage *Message; 
        struct InputEvent *ie;
        BOOL Done;

        char name[] = "gfx/background.jpg";

        struct Screen *scr = LockPubScreen("Workbench");
        glist = NULL;
        
        LONG Depth;
        
        borderTop = scr->BarHeight+1;
        
        penPink = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(237), RGB32(119), RGB32(255),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        penLightPink = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(254), RGB32(204), RGB32(253),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        penBlue = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(52), RGB32(100), RGB32(208),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        penLightBlue = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(102), RGB32(203), RGB32(255),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        penWhite = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(237), RGB32(255), RGB32(255),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        penPinkHit = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(204), RGB32(136), RGB32(255),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        penLightPinkTxt = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(254), RGB32(240), RGB32(255),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        penLightBlueTxt = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(99), RGB32(206), RGB32(255),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        penTitle = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(97), RGB32(255), RGB32(254),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        penTitleTxt = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(218), RGB32(248), RGB32(246),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        penGrid = ObtainBestPenA(scr->ViewPort.ColorMap,
                        RGB32(218), RGB32(220), RGB32(220),
                        OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        
        srand(time(NULL));

        if ( (vi = GetVisualInfo(scr, TAG_END)) != NULL )
        {

            struct Gadget *gad;

            /* GadTools gadgets require this step to be taken */
            gad = CreateContext(&glist);

            /* gadgets */
            ng.ng_TextAttr   = &Topaz120;
            ng.ng_VisualInfo = vi;
            ng.ng_LeftEdge   = MARGIN+32*16+64;
            ng.ng_TopEdge    = MARGIN+32*16+32 + scr->WBorTop + (scr->Font->ta_YSize + 1);
            ng.ng_Width      = 100;
            ng.ng_Height     = 36;
            ng.ng_GadgetText = "PLAY";
            ng.ng_GadgetID   = PLAY_BUTTON;
            ng.ng_Flags      = 0;
            gads[PLAY_BUTTON] = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

            ng.ng_TextAttr   = &Topaz120;
            ng.ng_VisualInfo = vi;
            ng.ng_LeftEdge   = MARGIN+32;
            ng.ng_TopEdge    = MARGIN+32*16+32 + scr->WBorTop + (scr->Font->ta_YSize + 1);
            ng.ng_Width      = 160;
            ng.ng_Height     = 36;
            ng.ng_GadgetText = "Undo ship";
            ng.ng_GadgetID   = UNDO_BUTTON;
            ng.ng_Flags      = 0;
            gads[UNDO_BUTTON] = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

            ng.ng_TextAttr   = &Topaz120;
            ng.ng_VisualInfo = vi;
            ng.ng_LeftEdge   = MARGIN+32*16+64;
            ng.ng_TopEdge    = MARGIN+32*16+32+36+20 + scr->WBorTop + (scr->Font->ta_YSize + 1);
            ng.ng_Width      = 160;
            ng.ng_Height     = 36;
            ng.ng_GadgetText = "New game";
            ng.ng_GadgetID   = NEWGAME_BUTTON;
            ng.ng_Flags      = 0;
            gads[NEWGAME_BUTTON] = gad = CreateGadget(BUTTON_KIND, gad, &ng, TAG_END);

            if (gad != NULL) {

                 /* Alusta backfill hook */
                backfillHook.h_Entry = (HOOKFUNC)MyBackfillFunc;
                backfillHook.h_SubEntry = NULL;
                backfillHook.h_Data = NULL;
                
                
                if (LoadPicture(Backfill, name, scr))
                {
                    win = OpenWindowTags (NULL,
                        WA_Title, "Battle ship game for AmigaOS 3",
                        WA_Top, 0,
                        WA_Left, 80,
                        WA_InnerWidth, 800,
                        WA_InnerHeight, 800,
                        WA_ReportMouse, TRUE,
                        WA_RMBTrap, TRUE,
                        WA_Gadgets, NULL,
                        WA_SmartRefresh, TRUE,
                        WA_CloseGadget, TRUE,
                        WA_BackFill,(ULONG)&backfillHook,
                        WA_Flags,       WFLG_ACTIVATE | WFLG_DRAGBAR,
                        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_REFRESHWINDOW | IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_GADGETUP,
                        TAG_END);

                    if (win) {
    
                        BOOL gridRegion = FALSE;
                
                        rastport = win->RPort;
                        borderTop = win->BorderTop;
                        GT_RefreshWindow(win, NULL);

                        if (!(myfont = (struct TextFont*)OpenDiskFont(&myta))) {
                            printf("Failed to open CGTimes 72 font\nWill use Topaz 12...\n");
                            myfont = (struct TextFont*)OpenFont(&Topaz120);
                        }

                        myfont2 = (struct TextFont*)OpenFont(&Topaz120);
                        SetDrMd(rastport,0);
                        
                        struct Gadget *gadEvent;

                        ULONG MsgClass;
                        UWORD code;
                        
                        BOOL allPlaced;

                        Done = FALSE;
                        BOOL prevExists = FALSE;

                        int bx, by, mx, my, pmx, pmy, fillWidth, fillHeight, bpmx, bpmy;

                        state = START_SCREEN;
                        
                        
                        WaitTOF();
                        
                        /*
                        *  Main event loop
                        */
                        while(!Done)
                        {

                            
                            if (state == START_SCREEN) {

                                SetAPen(rastport, penTitle);
                                SetFont(rastport, myfont);
                                Move(rastport, (800-TextLength(rastport, "Battle ship", 11)) / 2, win->BorderTop+MARGIN + borderTop);
                                Text(rastport, "Battle ship", 11);

                                SetFont(rastport, myfont2);
                                SetAPen(rastport, penTitleTxt);
                                Move(rastport, (800-TextLength(rastport, "Version 1.4.0", 13)) / 2, win->BorderTop+MARGIN + 40) + borderTop;
                                Text(rastport, "Version 1.4.0", 13);

                                Move(rastport, (800-TextLength(rastport, "Click anywhere in the window to continue", 40)) / 2, win->BorderTop+MARGIN + 40 + 80 + borderTop);
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
                                Move(rastport, 32, MARGIN + 32*16+128 + win->BorderTop + rastport->TxBaseline);
                                Text(rastport, "Positioning of ships", 20);
                            }

                            if (state == PLAY) {
                                SetFont(rastport, myfont);
                                SetAPen(rastport, penLightPinkTxt);
                                Move(rastport, 128, MARGIN + 32*16+128+32);
                                Text(rastport, "Game on!", 8);

                                SetFont(rastport, myfont2);
                                
                                SetAPen(rastport, penBlue);
                                RectFill(rastport, 32, MARGIN + 32*16+160-16 + 32, 32+32, MARGIN + 32*16+160+32-16 + 32);
                                SetAPen(rastport, penLightPinkTxt);
                                Move(rastport, 32+68, MARGIN + 32*16+160+32);
                                Text(rastport, "Human player has hit AI's ship", 30);

                                SetAPen(rastport, penLightBlue);
                                RectFill(rastport, 32, MARGIN + 32*16+180 + 32, 32+32, MARGIN + 32*16+180+32 + 32);
                                SetAPen(rastport, penLightPinkTxt);
                                Move(rastport, 32+68, MARGIN + 32*16+180+16+32);
                                Text(rastport, "Human player has missed AI's ship", 33);
                                
                                // as long as these settings are in order, in-game texts can't be printed
                                            if (gridRegion == FALSE) {
                                                    rect.MinX = win->BorderLeft+8; rect.MinY = win->BorderTop+8;
                                                    rect.MaxX = win->BorderLeft + 700; rect.MaxY = win->BorderTop + MARGIN + 512 + 1;                            
                                                    
                                                    
                                                    WaitBlit();
                                                    
                                                    if ((newRegion = (struct Region *) NewRegion())) {
                                                        OrRectRegion(newRegion, &rect);

                                                        LockLayer(0, win->WLayer);
                                                        old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                        UnlockLayer(win->WLayer);

                                                        if (old) DisposeRegion(old);

                                                        clipRegion = newRegion; /* talletetaan, jotta voidaan vapauttaa lopuksi */
                                                    }     
                                                    
                                                    gridRegion = TRUE;
                                                    clipInstalled = TRUE;
                                                }
                                            
                            }
                            
                            if ((AIHits == 23 || plyHits == 23) && state != GAME_OVER) {
                                
                                state = GAME_OVER;
                                                                                                
                                                rect.MinX = win->BorderLeft + 1;
                                                rect.MinY = win->BorderTop + 1 + MARGIN + 512 + 56;
                                                rect.MaxX = win->BorderLeft + 1 + 512 + MARGIN;
                                                rect.MaxY = win->BorderTop + 1 + MARGIN + 512 + 140;
                                                
                                                WaitBlit();
                                                
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *) InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);

                                                    if (old) DisposeRegion(old);

                                                    clipRegion = newRegion; /* talletetaan, jotta voidaan vapauttaa lopuksi */
                                                }
                                                
                                                
                                                
                                                // clear "Game on!" text
                                                BltBitMapRastPort(gBitMap,
                                                    0,MARGIN+32*16+70,
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop + MARGIN+32*16+70,
                                                    780, 180,
                                                0xC0);
                                
                                
                                                rect.MinX = win->BorderLeft + 1;
                                                rect.MinY = win->BorderTop + 1 + MARGIN + 512 + 126;
                                                rect.MaxX = win->BorderLeft + 1 + 512 + MARGIN + 250;
                                                rect.MaxY = win->BorderTop + 1 + MARGIN + 512 + 250;
                                                
                                                WaitBlit();
                                                
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *) InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);

                                                    if (old) DisposeRegion(old);

                                                    clipRegion = newRegion; /* talletetaan, jotta voidaan vapauttaa lopuksi */
                                                }
                                                
                                                // clear the screen a bit...
                                                BltBitMapRastPort(gBitMap,
                                                    0,MARGIN+32*16+126 + 1,
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop + MARGIN+32*16+126 + 1,
                                                    780, 180,
                                                0xC0);
                                                
                                                
                                                // when the following is FALSE, eventually the Clip Region will change..
                                                gridRegion = FALSE;
                                                
                                                
                            }

                            if (state == GAME_OVER) {
                                SetFont(rastport, myfont);
                                Move(rastport, 32, MARGIN + 32*16+128+88);
                                
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

                                                    EasyRequest(NULL, &myES, NULL, "(Variable)", NULL);
                                                    break;
                                                }

                                                placeComputersShips();

                                                // clear "positioning of ships"
                                                BltBitMapRastPort(gBitMap,
                                                    0,MARGIN+32*16+128,
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop + MARGIN+32*16+128,
                                                    512+32, 128,
                                                    0xC0);
                                                
                                                state = PLAY;
                                                
                                                rect.MinX = win->BorderLeft + 1;
                                                rect.MinY = win->BorderTop + 1 + MARGIN + 512;
                                                rect.MaxX = win->BorderLeft + 1 + 512 + MARGIN;
                                                rect.MaxY = win->BorderTop + 1 + MARGIN + 512 + 250;
                                                
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *) InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);

                                                    if (old) DisposeRegion(old);

                                                    clipRegion = newRegion; /* talletetaan, jotta voidaan vapauttaa lopuksi */
                                                }
                                                
                                                gridRegion = FALSE;
                                                
                                                BltBitMapRastPort(gBitMap,
                                                    0,MARGIN+32*16+70,
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop + MARGIN+32*16+70,
                                                    600, 120,
                                                    0xC0);
                                                                                                
                                                break;
                                            case UNDO_BUTTON:
                                                if (state != PLACE_SHIPS) break;
                                                
                                                // playing area
                                                rect.MinX = win->BorderLeft+8; rect.MinY = win->BorderTop+8;
                                                rect.MaxX = win->BorderLeft + 700; rect.MaxY = win->BorderTop + MARGIN + 512 + 1;                            
                                                    
                                                    
                                                WaitBlit();
                                                    
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);

                                                    if (old) DisposeRegion(old);

                                                    clipRegion = newRegion; /* talletetaan, jotta voidaan vapauttaa lopuksi */
                                                }     
                                                
                                                BltBitMapRastPort(gBitMap,
                                                    0,0,
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop,
                                                    700+MARGIN+40, 512+MARGIN,
                                                0xC0);
                                                
                                                undoCurrentPositioning();
                                                break;

                                            case NEWGAME_BUTTON:
                                                
                                                rect.MinX = win->BorderLeft + 1;
                                                rect.MinY = win->BorderTop + 1 + MARGIN + 512 + 56;
                                                rect.MaxX = win->BorderLeft + 1 + 512 + MARGIN;
                                                rect.MaxY = win->BorderTop + 1 + MARGIN + 512 + 190;
                                                
                                                WaitBlit();
                                                
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *) InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);

                                                    if (old) DisposeRegion(old);

                                                    clipRegion = newRegion; /* talletetaan, jotta voidaan vapauttaa lopuksi */
                                                }
                                                
                                                // clear "Game on!" text
                                                BltBitMapRastPort(gBitMap,
                                                    0,MARGIN+32*16+70,
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop + MARGIN+32*16+70,
                                                    600, 190,
                                                0xC0);

                                                
                                                // clear "game over" text
                                                rect.MinX = win->BorderLeft + 1;
                                                rect.MinY = win->BorderTop + 1 + MARGIN + 512 + 126;
                                                rect.MaxX = win->BorderLeft + 1 + 512 + MARGIN + 250;
                                                rect.MaxY = win->BorderTop + 1 + MARGIN + 512 + 250;
                                                
                                                WaitBlit();
                                                
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *) InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);

                                                    if (old) DisposeRegion(old);

                                                    clipRegion = newRegion; /* talletetaan, jotta voidaan vapauttaa lopuksi */
                                                }
                                                
                                                // clear the screen a bit...
                                                BltBitMapRastPort(gBitMap,
                                                    0,MARGIN+32*16+126 + 1,
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop + MARGIN+32*16+126 + 1,
                                                    780, 180,
                                                0xC0);
                                                
                                                SetAPen(rastport, penLightPinkTxt);
                                                SetFont(rastport, myfont);
                                                Move(rastport, 32, MARGIN + 32*16+128 + win->BorderTop + rastport->TxBaseline);
                                                Text(rastport, "Positioning of ships", 20);
                                                
                                                // playing area
                                                rect.MinX = win->BorderLeft+8; rect.MinY = win->BorderTop+8;
                                                rect.MaxX = win->BorderLeft + 700; rect.MaxY = win->BorderTop + MARGIN + 512 + 1;                            
                                                    
                                                    
                                                WaitBlit();
                                                    
                                                if ((newRegion = (struct Region *) NewRegion())) {
                                                    OrRectRegion(newRegion, &rect);

                                                    LockLayer(0, win->WLayer);
                                                    old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                    UnlockLayer(win->WLayer);

                                                    if (old) DisposeRegion(old);

                                                    clipRegion = newRegion; /* talletetaan, jotta voidaan vapauttaa lopuksi */
                                                }     
                                                
                                                BltBitMapRastPort(gBitMap,
                                                    0,0,
                                                    rastport,
                                                    win->BorderLeft, win->BorderTop,
                                                    700+MARGIN+40, 512+MARGIN,
                                                0xC0);
                                                
                                                gridRegion = TRUE;
                                                
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

                                            
                                                BltBitMapRastPort(gBitMap,
                                                0, 0,
                                                rastport,
                                                win->BorderLeft, win->BorderTop,
                                                800, 350,
                                                0xC0);
                                            

                                                AddGList(win, &glist, 0, -1, NULL);
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
                                                    
                                                    bx = (mx + MARGIN + win->BorderLeft) / 32 - 1;
                                                    by = (my + MARGIN + win->BorderTop) / 32 - 2;
                                                    
                                                        BltBitMapRastPort(gBitMap,
                                                            bpmx, bpmy,
                                                            rastport,
                                                            bpmx+win->BorderLeft, bpmy+win->BorderTop,
                                                            fillWidth+1, fillHeight+1,
                                                            0xC0);
                                            
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
                                                
                                                
                                                    
                                                if (mx + win->BorderLeft >= MARGIN + win->BorderLeft + 512 + 64 + 32 && mx + win->BorderLeft + MARGIN <= MARGIN + win->BorderLeft + 512 + 64 + 32 + 32*3 && my + win->BorderTop >= win->BorderTop + MARGIN && my + win->BorderTop <= MARGIN + win->BorderTop + 32*3) {
                                                        if (shipsPlaced[0] == TRUE) break;
                                                        angle_ = 0;
                                                        shipSelected = 1;
                                                        initShip(1);

                                                        break;
                                                }

                                                if (mx + win->BorderLeft >= MARGIN + win->BorderLeft + 512 + 64 && mx + win->BorderLeft <= MARGIN + win->BorderLeft + 512 + 64 + 32*3 && my + win->BorderTop >= MARGIN + 32 + 32*2 + 32 + 32 + 32 && my + win->BorderTop <= MARGIN + 32 + 32*2 + 32 + 32 + 32 + 32) {
                                                        if (shipsPlaced[1] == TRUE) break;
                                                        angle_ = 0;
                                                        shipSelected = 2;

                                                        initShip(2);
                                                        break;
                                                }

                                                if (mx + win->BorderLeft >= MARGIN + win->BorderLeft + 512 + 64 && mx + win->BorderLeft <= MARGIN + win->BorderLeft + 512 + 64 + 32*2 && my + win->BorderTop >= MARGIN + win->BorderTop + 32 + 32*2 + 32 + 32 + 32 + 32 && my + win->BorderTop <= MARGIN + win->BorderTop + 32 + 32*2 + 32 + 32 + 32*2 + 32 + 32) {
                                                        if (shipsPlaced[2] == TRUE) break;
                                                        angle_ = 0;
                                                        shipSelected = 3;

                                                        initShip(3);

                                                        break;
                                                }

                                                if (mx + win->BorderLeft >= MARGIN + win->BorderLeft + 512 + 64 && mx + win->BorderLeft <= MARGIN + win->BorderLeft + 512 + 64 + 32*4 && my + win->BorderTop >= MARGIN + win->BorderTop + 32 + 32*2 + 32 + 32 + 32*2 + 32 && my + win->BorderTop <= MARGIN + 32 + 32*2 + 32 + 32 + 32*2 + 32 + 32*4) {
                                                        if (shipsPlaced[3] == TRUE) break;
                                                        angle_ = 0;
                                                        shipSelected = 4;

                                                        initShip(4);

                                                        break;
                                                }

                                                if (mx + win->BorderLeft >= MARGIN + win->BorderLeft + 512 + 64 && mx + win->BorderLeft <= MARGIN + win->BorderLeft + 512 + 64 + 32*5 && my + win->BorderTop >= MARGIN + win->BorderTop + 32 + 32*2 + 32 + 32 + 32*2 + 32 + 32*4 + 32 && my + win->BorderTop <= MARGIN + win->BorderTop + 32 + 32*2 + 32 + 32 + 32*2 + 32 + 32*4 + 32 + 32*4 + 32) {
                                                        if (shipsPlaced[4] == TRUE) break;
                                                        angle_ = 0;
                                                        shipSelected = 5;

                                                        initShip(5);

                                                        break;
                                                
                                                }
                                            
                                            }
                                            
                                            if (state == PLAY) {
                                                bx = (mx + MARGIN + win->BorderLeft) / 32 - 2;
                                                    by = (my + MARGIN + win->BorderTop) / 32 - 4;

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
                                                    rect.MinX = win->BorderLeft+8; rect.MinY = win->BorderTop+8;
                                                    rect.MaxX = win->BorderLeft + 700; rect.MaxY = win->BorderTop + MARGIN + 512 + 1;                            
                                                    
                                                    
                                                    WaitBlit();
                                                    
                                                    if ((newRegion = (struct Region *) NewRegion())) {
                                                        OrRectRegion(newRegion, &rect);

                                                        LockLayer(0, win->WLayer);
                                                        old = (struct Region *)InstallClipRegion(win->WLayer, newRegion);
                                                        UnlockLayer(win->WLayer);

                                                        if (old) DisposeRegion(old);

                                                        clipRegion = newRegion; /* talletetaan, jotta voidaan vapauttaa lopuksi */
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
                                        
                                            
                                            // bx = (mx + MARGIN + win->BorderLeft + 15) / 32 - 1;
                                            // by = (my + MARGIN + win->BorderTop + height) / 32 - 2;
                                            
                                            //printf("%d %d \n",bx, by);
                                            

                                            if (prevExists) {
                                                
                                                // if (by > 15) fillHeight = (by - 15) * 32;
                                                    
                                                            BltBitMapRastPort(gBitMap,
                                                                pmx, pmy,
                                                                rastport,
                                                                pmx+win->BorderLeft, pmy+win->BorderTop,
                                                                fillWidth+1, fillHeight+1,
                                                                0xC0);
                                                    
                                            }
                                        }
                                            
                                            prevExists = TRUE;
                                            
                                            SetAPen(rastport, penWhite);

                                            switch (shipSelected) {
                                                case 1:
                                                    
                                                    fillWidth = 3*32;
                                                    fillHeight = 3*32;

                                                    for (int j = 0; j < 3; j++) {
                                                        for (int i = 0; i < 3; i++) {

                                                            if (ship1[i+j*3] == 1) {

                                                                //if (!(mx + i * 32 >= MARGIN + 16 * 32 - win->BorderRight || mx + i * 32 <= win->BorderLeft
                                                                //    || my + j * 32 <= win->BorderTop + MARGIN || my + j*32 >= MARGIN + 16 * 32 + win->BorderTop)) {

                                                                    RectFill(rastport, mx + win->BorderLeft + i*32, my + win->BorderTop + j*32, mx + win->BorderLeft + i*32+32, my + win->BorderTop + j*32+32);
                                                                //}
                                                            }
                                                        } 
                                                    }
                                                    break;

                                                case 2:
                                                
                                                    fillWidth = 3*32;
                                                    fillHeight = 3*32;

                                                    for (int j = 0; j < 3; j++) {
                                                        for (int i = 0; i < 3; i++) {
                                                            if (ship2[i+j*3] == 1) {

                                                                //if (!(mx + i * 32 >= MARGIN + 16 * 32 - win->BorderRight || mx + i * 32 <= win->BorderLeft
                                                                //    || my + j * 32 <= win->BorderTop + MARGIN || my + j*32 >= MARGIN + 16 * 32 + win->BorderTop)) {

                                                                    RectFill(rastport, mx + win->BorderLeft + i*32, my + win->BorderTop + j*32, mx + win->BorderLeft + i*32+32, my + win->BorderTop + j*32+32);
                                                                //}
                                                            }
                                                        } 
                                                    }
                                                    break;

                                                case 3:

                                                    fillWidth = 2*32;
                                                    fillHeight = 2*32;

                                                    for (int j = 0; j < 2; j++) {
                                                        for (int i = 0; i < 2; i++) {
                                                            if (ship3[i+j*2] == 1) {

                                                                //if (!(mx + i * 32 >= MARGIN + 16 * 32 - win->BorderRight || mx + i * 32 <= win->BorderLeft
                                                                //    || my + j * 32 <= win->BorderTop + MARGIN || my + j*32 >= MARGIN + 16 * 32 + win->BorderTop)) {

                                                                    RectFill(rastport, mx + win->BorderLeft + i*32, my + win->BorderTop + j*32, mx + win->BorderLeft + i*32+32, my + win->BorderTop + j*32+32);
                                                                //}
                                                            }
                                                        } 
                                                    }
                                                    break;

                                                case 4:

                                                    fillWidth = 4*32;
                                                    fillHeight = 4*32;

                                                    for (int j = 0; j < 4; j++) {
                                                        for (int i = 0; i < 4; i++) {
                                                            if (ship4[i+j*4] == 1) {

                                                                //if (!(mx + i * 32 >= MARGIN + 16 * 32 - win->BorderRight || mx + i * 32 <= win->BorderLeft
                                                                //    || my + j * 32 <= win->BorderTop + MARGIN || my + j*32 >= MARGIN + 16 * 32 + win->BorderTop)) {

                                                                    RectFill(rastport, mx + win->BorderLeft + i*32, my + win->BorderTop + j*32, mx + win->BorderLeft + i*32+32, my + win->BorderTop + j*32+32);
                                                                //}
                                                            }
                                                        } 
                                                    }
                                                    break;

                                                case 5:

                                                    fillHeight = 5*32;
                                                    fillWidth = 5*32;

                                                    for (int j = 0; j < 5; j++) {
                                                        for (int i = 0; i < 5; i++) {
                                                            if (ship5[i+j*5] == 1) {

                                                                //if (!(mx + i * 32 >= MARGIN + 16 * 32 - win->BorderRight || mx + i * 32 <= win->BorderLeft
                                                                //    || my + j * 32 <= win->BorderTop + MARGIN || my + j*32 >= MARGIN + 16 * 32 + win->BorderTop)) {

                                                                    RectFill(rastport, mx + win->BorderLeft + i*32, my + win->BorderTop + j*32, mx + win->BorderLeft + i*32+32, my + win->BorderTop + j*32+32);
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

                        /* Poistetaan ClipRegion turvallisesti */
                        if (clipInstalled) {
                            
                                LockLayer(0, win->WLayer);
                                {
                                    struct Region *old = (struct Region *)InstallClipRegion(win->WLayer, NULL);
                                    if (old) DisposeRegion(old);
                                }
                                UnlockLayer(win->WLayer);
                            } else if (clipRegion) {
                            DisposeRegion(clipRegion);
                        }
                        
                        CloseWindow(win);
                    }
                        
                    
                    
                    
                    DisposeDTObject(dto);
                }
                
            }
                
            FreeGadgets(glist);

        }
            
                
    
    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, scr);

    cleanup();
}

 int main(int argc,char **argv)
    {
        DataTypesBase       = OpenLibrary("datatypes.library", 39);
        IntuitionBase       = OpenLibrary("intuition.library", 39);
        GfxBase             = OpenLibrary("graphics.library", 39);
        UtilityBase         = OpenLibrary("utility.library", 39);
        LayersBase          = OpenLibrary("layers.library", 39);
        GadToolsBase        = OpenLibrary("gadtools.library", 39);
        DiskfontBase        = OpenLibrary("diskfont.library", 39);
        
        if (!DataTypesBase || ! IntuitionBase || !GfxBase || !UtilityBase || !LayersBase || !GadToolsBase || !DiskfontBase) {
            
            if (DataTypesBase) CloseLibrary(DataTypesBase); else printf("datatypes.library version 39 not found\n");
            if (IntuitionBase) CloseLibrary(IntuitionBase); else printf("intuition.library version 39 not found\n");
            if (GfxBase) CloseLibrary(GfxBase); else printf("graphics.library version 39 not found\n");
            if (UtilityBase) CloseLibrary(UtilityBase); else printf("utility.library version 39 not found\n");
            if (LayersBase) CloseLibrary(LayersBase); else printf("layers.library version 39 not found\n");
            if (GadToolsBase) CloseLibrary(GadToolsBase); else printf("gadtools.library version 39 not found\n");
            if (DiskfontBase) CloseLibrary(DiskfontBase); else printf("diskfont.library version 39 not found\n");
            
            return -1;
        }

        startPrg();


        return 0;

    }


int cleanup() {

        CloseLibrary(DataTypesBase);
        CloseLibrary(IntuitionBase);
        CloseLibrary(UtilityBase);
        CloseLibrary(GfxBase);
        CloseLibrary(DiskfontBase);
        CloseLibrary(GadToolsBase);
        CloseLibrary(LayersBase);
        
        return;

    }

    void drawBoard(struct RastPort *rp) {
        
        for (int j = 0; j < 16; j++) {
            for (int i = 0; i < 16; i++) {

                // player's ship
                SetAPen(rp, penWhite);
                if (board[i + j * 16] == 1) {
                    RectFill(rp, MARGIN + i * 32+borderLeft, MARGIN + j * 32+borderTop, MARGIN + i * 32 + 32-1+borderLeft, MARGIN + j * 32 + 32-1+borderTop);
                }

                // computer's ship
                SetAPen(rp, penBlue);
                if (state == GAME_OVER && AIHits == 23) {
                    if (board[i + j * 16] == 2) {
                        RectFill(rp, MARGIN + i * 32+borderLeft, MARGIN + j * 32+borderTop, MARGIN + i * 32 + 32-1+borderLeft, MARGIN + j * 32 + 32-1+borderTop);
                    }
                }

                // player's miss
                SetAPen(rp, penLightBlue);
                if (board[i + j * 16] == 3) {
                    RectFill(rp, MARGIN + i * 32+borderLeft, MARGIN + j * 32+borderTop, MARGIN + i * 32 + 32-1+borderLeft, MARGIN + j * 32 + 32-1+borderTop);
                }

                // player's hit
                if (board[i + j * 16] == 4) {
                    SetAPen(rp, penBlue);
                    RectFill(rp, MARGIN + i * 32+borderLeft, MARGIN + j * 32+borderTop, MARGIN + i * 32 + 32-1+borderLeft, MARGIN + j * 32 + 32-1+borderTop);
                }

                // computer's miss
                if (board[i + j * 16] == 5) {
                    SetAPen(rp, penLightPink);
                    RectFill(rp, MARGIN + i * 32+borderLeft, MARGIN + j * 32+borderTop, MARGIN + i * 32 + 32-1+borderLeft, MARGIN + j * 32 + 32-1+borderTop);
                }

                // computer's hit
                if (board[i + j * 16] == 6) {
                    SetAPen(rp, penPinkHit);
                    RectFill(rp, MARGIN + i * 32+borderLeft, MARGIN + j * 32+borderTop, MARGIN + i * 32 + 32-1+borderLeft, MARGIN + j * 32 + 32-1+borderTop);
                }

                
            }
        }
    }

    void drawShips(struct RastPort *rp) {

        SetAPen(rp, penPink);

        // draw ship 1
        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 3; i++) {
                if (ship1_[i+j*3] == 1) RectFill(rp, MARGIN+512+64+i*32, MARGIN+32+j*32-2, MARGIN+512+64+i*32+32, MARGIN+32+32+j*32-2);
            }
        }

        // draw ship 2
        for (int j = 0; j < 3; j++) {
            for (int i = 0; i < 3; i++) {
                if (ship2_[i+j*3] == 1) RectFill(rp, MARGIN+512+64+i*32, MARGIN+32+j*32 + 64+32+32, MARGIN+512+64+i*32+32, MARGIN+32+32+j*32 + 64+32+32);
            }
        }

        // draw ship 3
        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 2; i++) {
                if (ship3_[i+j*3] == 1) RectFill(rp, MARGIN+512+64+i*32, MARGIN+32+j*32 + 64+32+64+32, MARGIN+512+64+i*32+32, MARGIN+32+32+j*32 + 64+32+64+32);
            }
        }

        // draw ship 4
        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 4; i++) {
                if (ship4_[i+j*4] == 1) RectFill(rp, MARGIN+512+64+i*32, MARGIN+32+j*32 + 64+32+64+32+32+32+32, MARGIN+512+64+i*32+32, MARGIN+32+32+j*32 + 64+32+64+32+32+32+32);
            }
        }

        // draw ship 5
        for (int j = 0; j < 5; j++) {
            for (int i = 0; i < 5; i++) {
                if (ship5_[i+j*5] == 1) RectFill(rp, MARGIN+512+64+i*32, MARGIN+32+j*32 + 64+32+64+32+32+32+32*4+32+32, MARGIN+512+64+i*32+32, MARGIN+32+32+j*32 + 64+32+64+32+32+32+32*4+32+32);
            }
        }
    }

    void drawGrid(struct RastPort *rp) {

        SetAPen(rp, penGrid);
        
        Move(rp, MARGIN,MARGIN+borderTop);
        Draw(rp, MARGIN+512,MARGIN+borderTop);

        Move(rp, MARGIN+512,MARGIN+borderTop);
        Draw(rp, MARGIN+512,MARGIN+512+borderTop);

        Move(rp, MARGIN+512,MARGIN+512+borderTop);
        Draw(rp, MARGIN, MARGIN+512+borderTop);

        Move(rp, MARGIN, MARGIN+512+borderTop);
        Draw(rp, MARGIN, MARGIN+borderTop);

        for (int j = MARGIN+32; j < MARGIN+512; j+=32) {
            Move(rp, MARGIN, j+borderTop);
            Draw(rp, MARGIN+512, j+borderTop);
        }

        for (int i = MARGIN+32; i < MARGIN+512; i+=32) {
            Move(rp, i, MARGIN+borderTop);
            Draw(rp, i, MARGIN+512+borderTop);
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


    /* ---------------------------------------------------------------------- */
    /* Load a picture via datatypes                                           */
    /* ---------------------------------------------------------------------- */
    BOOL LoadPicture(struct BackFillInfo *bfi, STRPTR filename, struct Screen *scr)
    {
        dto = NewDTObject(filename,
                                DTA_SourceType       ,DTST_FILE,
                                                DTA_GroupID          ,GID_PICTURE,
                                                PDTA_Remap           ,TRUE,
                                                //PDTA_DestBitMap      ,(ULONG)&gBitMap,
                                                PDTA_DestMode        ,PMODE_V43,
                                                TAG_DONE);

        if (!dto) return FALSE;

        DoMethod (dto, DTM_PROCLAYOUT, NULL, TRUE);

        struct BitMapHeader *bmhd;

        GetDTAttrs (dto,
                    PDTA_BitMapHeader, &bmhd,
                    PDTA_BitMap, &gBitMap,
                    TAG_END);
        
        if (!bmhd || !gBitMap) return FALSE;
        
        bfi->Screen = scr;
        bfi->BitMapHeader = bmhd;
        bfi->CopyWidth  = bmhd->bmh_Width;
        bfi->CopyHeight = bmhd->bmh_Height;
        
        return TRUE;
    }
