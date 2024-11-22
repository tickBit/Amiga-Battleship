/*
    Battle ship game for Amiga a'la spaghetti...

    Version 1.1.0

    IMPORTANT:

    The C compiler must comply with the 1999 C standard.
    ----------------------------------------------------

    With VBCC: vc -c99 Battleship.c -o Battleship -lamiga -fpu=68881

    You can adjust the difficulty of the game by increasing or decreasing
    constant DIFFICULTY (and variable error).

 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <dos/dos.h>
#include <exec/memory.h>

#include <datatypes/pictureclass.h>

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

struct BitMap *bitmap = NULL;

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

struct TextAttr Topaz120 = { "topaz.font", 12, 0, 0, };
struct TextAttr myta = {"CGTimes.font", 72, 0, 0};
struct TextFont *myfont, *myfont2;

struct Gadget    *glist, *gads[3];
struct NewGadget ng;
void             *vi;

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
    int width, height;
    struct RastPort rastport;
    struct TextFont *font;
    struct IntuiMessage *Message; 
    struct InputEvent *ie;
    BOOL Done;

    char name[] = "gfx/background.jpg";

    struct Screen *scr = LockPubScreen("Workbench");
    glist = NULL;

    srand(time(NULL));

    if ( (vi = GetVisualInfo(scr, TAG_END)) != NULL )
    {
    Object *Item = NewDTObject (name,
	    DTA_GroupID, GID_PICTURE,
	    PDTA_Remap, TRUE,
        PDTA_Screen, scr,
		PDTA_DestMode, PMODE_V43,
	    TAG_END);

		if (Item)
			{
			struct BitMapHeader *bmhd = NULL;
			struct BitMap *bm = NULL;

            int mx, my;

			DoMethod (Item, DTM_PROCLAYOUT, NULL, TRUE);

			GetDTAttrs (Item,
				PDTA_BitMapHeader, &bmhd,
				PDTA_DestBitMap, &bm,
				TAG_END);

			if (bmhd && bm) {
        
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

				win = OpenWindowTags (NULL,
					WA_Title, "Battle ship game for AmigaOS 3",
					WA_InnerWidth, bmhd->bmh_Width,
					WA_InnerHeight, bmhd->bmh_Height,
                    WA_ReportMouse, TRUE,
                    WA_RMBTrap, TRUE,
                    WA_Gadgets, glist,
                    WA_SmartRefresh, TRUE,
                    WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE,
                    WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | GADGETUP, TAG_END);

				if (win) {

                    bitmap = (struct BitMap *)AllocBitMap(bmhd->bmh_Width,bmhd->bmh_Height,bmhd->bmh_Depth,BMF_CLEAR | BMF_DISPLAYABLE,win->RPort->BitMap);
                    InitRastPort(&rastport);
                    rastport.BitMap = bitmap;

                    GT_RefreshWindow(win, NULL);

                    if (!(myfont = (struct TextFont*)OpenDiskFont(&myta))) {
                        printf("Failed to open CGTimes 72 font\nWill use Topaz 12...\n");
                        myfont = (struct TextFont*)OpenFont(&Topaz120);
                    }

                    myfont2 = (struct TextFont*)OpenFont(&Topaz120);

                    SetFont(&rastport, myfont);
                    SetDrMd(&rastport,0);

                    struct Gadget *gadEvent;

			        ULONG MsgClass;
			        UWORD MsgCode;
			        
                    BOOL allPlaced;

                    Done = FALSE;

                    int winWidth = bmhd->bmh_Width + win->BorderLeft + win->BorderRight + 100;
                    int winHeight = bmhd->bmh_Width + win->BorderBottom + win->BorderTop;

                    int x, y;

                    state = START_SCREEN;
                    
                    /*
                    *  Main event loop
                    */
                    do
                    {
                        
                        BltBitMapRastPort(bm, 0, 0, &rastport, 0, 0, bmhd->bmh_Width, bmhd->bmh_Height, 192);
                        
                        if (state == START_SCREEN) {
                            SetAPen(&rastport, 55);
                            SetFont(&rastport, myfont);
                            Move(&rastport, (800-TextLength(&rastport, "Battleship", 10)) / 2, win->BorderTop+MARGIN);
                            Text(&rastport, "Battleship", 10);

                            SetFont(&rastport, myfont2);
                            SetAPen(&rastport, 41);
                            Move(&rastport, (800-TextLength(&rastport, "Version 1.1.0", 13)) / 2, win->BorderTop+MARGIN + 40);
                            Text(&rastport, "Version 1.1.0", 13);

                            Move(&rastport, (800-TextLength(&rastport, "Click anywhere in the window to continue", 40)) / 2, win->BorderTop+MARGIN + 40 + 80);
                            Text(&rastport, "Click anywhere in the window to continue", 40);
                        }

                        if (state != START_SCREEN) {
                            drawGrid(rastport);
                            drawShips(rastport);
                            drawBoard(rastport);
                        }

                        if (state == PLACE_SHIPS) {
                            SetAPen(&rastport, 66);
                            SetFont(&rastport, myfont);
                            Move(&rastport, 32, MARGIN + 32*16+128);
                            Text(&rastport, "Positioning of ships", 20);
                        }

                        if (state == PLAY) {
                            SetFont(&rastport, myfont);
                            SetAPen(&rastport, 66);
                            Move(&rastport, 128, MARGIN+ 32*16+128);
                            Text(&rastport, "Game on!", 8);

                            SetFont(&rastport, myfont2);
                            
                            SetAPen(&rastport, 84);
                            RectFill(&rastport, 32, MARGIN + 32*16+160-16, 32+32, MARGIN + 32*16+160+32-16);
                            SetAPen(&rastport, 66);
                            Move(&rastport, 32+68, MARGIN + 32*16+160);
                            Text(&rastport, "Human player has hit AI's ship", 30);

                            SetAPen(&rastport, 83);
                            RectFill(&rastport, 32, MARGIN + 32*16+180, 32+32, MARGIN + 32*16+180+32);
                            SetAPen(&rastport, 66);
                            Move(&rastport, 32+68, MARGIN + 32*16+180+16);
                            Text(&rastport, "Human player has missed AI's ship", 33);
                        }
                        if (AIHits == 23 || plyHits == 23) state = GAME_OVER;
                        
                        if (state == GAME_OVER) {
                            SetFont(&rastport, myfont);
                            SetAPen(&rastport, 99);
                            Move(&rastport, 32, MARGIN + 32*16+128+88);
                            if (AIHits == 23) {
                                SetAPen(&rastport, 62);
                                Text(&rastport, "GAME OVER - I WON ;-)", 21);
                            } else {
                                if (plyHits == 23) {
                                    SetAPen(&rastport, 83);
                                    Text(&rastport, "Congratulations! You won!", 25);
                                }
                            }
                        }

                        while((Message = (struct IntuiMessage *)GT_GetIMsg(win->UserPort)) != NULL)
                        {
                            
                            MsgClass = Message->Class;
                            MsgCode = Message->Code;

                            switch(MsgClass)
                            {
                                
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

                                            state = PLAY;
                                            break;
                                        case UNDO_BUTTON:
                                            if (state != PLACE_SHIPS) break;
                                            undoCurrentPositioning();
                                            break;

                                        case NEWGAME_BUTTON:

                                            initGame();

                                            state = PLACE_SHIPS;
                                            break;
                                    
                                    }
                                    break;

                                case IDCMP_MOUSEBUTTONS:
                                     // right mouse button to rotate ship
                                    if (MsgCode == 233) {
                                        
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

                                        break;
                                    }

                                    // right and left mousebutton related...
                                    if (MsgCode == 105 || MsgCode == 104) break;

                                    // grab ship 1 with left mousebutton 
                                    if (MsgCode == 232) {

                                        if (state == START_SCREEN) {
                                            state = PLACE_SHIPS;
                                            break;
                                        }

                                        x = Message->MouseX - win->BorderLeft - MARGIN;
                                        y = Message->MouseY - win->BorderTop - MARGIN;

                                        if (state == PLACE_SHIPS) {

                                        if (x >=  + 512 + 64 && x <=  + 512 + 64 + 32*3 && y >=  + 32 && y <=  + 32 + 32*2) {
                                            if (shipsPlaced[0] == TRUE) break;
                                            angle_ = 0;
                                            shipSelected = 1;

                                            initShip(1);

                                            break;
                                        }

                                        if (x >=  + 512 + 64 && x <=  + 512 + 64 + 32*3 && y >=  + 32 + 32*2 + 32 + 32 && y <=  + 32 + 32*2 + 32 + 32 + 32) {
                                            if (shipsPlaced[1] == TRUE) break;
                                            angle_ = 0;
                                            shipSelected = 2;

                                            initShip(2);
                                            break;
                                        }

                                        if (x >=  + 512 + 64 && x <=  + 512 + 64 + 32*2 && y >=  + 32 + 32*2 + 32 + 32 + 32 + 32 && y <=  + 32 + 32*2 + 32 + 32 + 32*2 + 32 + 32) {
                                            if (shipsPlaced[2] == TRUE) break;
                                            angle_ = 0;
                                            shipSelected = 3;

                                            initShip(3);

                                            break;
                                        }

                                        if (x >=  + 512 + 64 && x <=  + 512 + 64 + 32*4 && y >=  + 32 + 32*2 + 32 + 32 + 32*2 + 32 && y <=  + 32 + 32*2 + 32 + 32 + 32*2 + 32 + 32*4) {
                                            if (shipsPlaced[3] == TRUE) break;
                                            angle_ = 0;
                                            shipSelected = 4;

                                            initShip(4);

                                            break;
                                        }

                                        if (x >=  + 512 + 64 && x <=  + 512 + 64 + 32*5 && y >=  + 32 + 32*2 + 32 + 32 + 32*2 + 32 + 32*4 + 32 && y <=  + 32 + 32*2 + 32 + 32 + 32*2 + 32 + 32*4 + 32 + 32*4 + 32) {
                                            if (shipsPlaced[4] == TRUE) break;
                                            angle_ = 0;
                                            shipSelected = 5;

                                            initShip(5);

                                            break;
                                        }

                                        int bx = (x+7) / 32;
                                        int by = (y+7) / 32;

                                        if (bx < 16 && by < 16) {

                                            if (shipSelected != 0) {
                                                                                            
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
                                        }
                                        }
                                    }
                                    
                                    if (state == PLAY) {
                                        int bx = x / 32;
                                        int by = y / 32;

                                        if (bx >= 0 && bx < 16 & by >= 0 && by < 16) {

                                            if (board[bx + by * 16] == 0) {
                                                // player's miss
                                                board[bx + by * 16] = 3;
                                            } else {
                                                // player's hit
                                                if (board[bx + by *16] == 2) {
                                                    board[bx + by * 16] = 4;
                                                    plyHits++;
                                                } else {
                                                    printf("Choose another square\n");
                                                    break;
                                                }
                                            }
                                            
                                            AImove();

                                        }
                                    }

                                    break;

                                case IDCMP_MOUSEMOVE:
                                    mx = Message->MouseX - win->BorderLeft;
                                    my = Message->MouseY - win->BorderTop;

                                    GT_ReplyIMsg((struct Message *)Message);
                                    break;

                                case IDCMP_CLOSEWINDOW:
                                    Done = TRUE;
                                    break;
                            }               

                    } if (Message != NULL)
                        GT_ReplyIMsg((struct Message *)Message);

                        if (shipSelected != 0 && state == PLACE_SHIPS) {
                            SetAPen(&rastport, 29);

                            switch (shipSelected) {
                                case 1:

                                    for (int j = 0; j < 3; j++) {
                                        for (int i = 0; i < 3; i++) {
                                            if (ship1[i+j*3] == 1) {
                                                RectFill(&rastport, mx + i*32, my + j*32, mx + i*32+32, my+j*32+32);
                                            }
                                        } 
                                    }
                                    break;

                                case 2:

                                    for (int j = 0; j < 3; j++) {
                                        for (int i = 0; i < 3; i++) {
                                            if (ship2[i+j*3] == 1) {
                                                RectFill(&rastport, mx + i*32, my + j*32, mx + i*32+32, my+j*32+32);
                                            }
                                        } 
                                    }
                                    break;

                                case 3:

                                    for (int j = 0; j < 2; j++) {
                                        for (int i = 0; i < 2; i++) {
                                            if (ship3[i+j*2] == 1) {
                                                RectFill(&rastport, mx + i*32, my + j*32, mx + i*32+32, my+j*32+32);
                                            }
                                        } 
                                    }
                                    break;

                                case 4:

                                    for (int j = 0; j < 4; j++) {
                                        for (int i = 0; i < 4; i++) {
                                            if (ship4[i+j*4] == 1) {
                                                RectFill(&rastport, mx + i*32, my + j*32, mx + i*32+32, my+j*32+32);
                                            }
                                        } 
                                    }
                                    break;

                                case 5:

                                    for (int j = 0; j < 5; j++) {
                                        for (int i = 0; i < 5; i++) {
                                            if (ship5[i+j*5] == 1) {
                                                RectFill(&rastport, mx + i*32, my + j*32, mx + i*32+32, my+j*32+32);
                                            }
                                        } 
                                    }
                                    break;
                            }
                        }
                        
                        
                        ClipBlit(&rastport, 0, 0, win->RPort, win->BorderLeft, win->BorderTop, bmhd->bmh_Width, bmhd->bmh_Height, 192);
                        if (state != START_SCREEN) RefreshGList(win->FirstGadget, win, NULL, -1);  // Refresh gadgets


                        WaitTOF();
                        WaitTOF();
                        WaitTOF();
                        WaitTOF();

                    }   while(!Done);
                    } 
                }
                
                CloseWindow(win);
            }
            FreeGadgets(glist);
            }
            if (Item) DisposeDTObject(Item);
            
            
    }
    FreeVisualInfo(vi);
    UnlockPubScreen(NULL, scr);

    cleanup();
}

int main(int argc,char **argv)
{

	/* The program might need to ask for later versions of these libraries... */

   	if(IntuitionBase == NULL)
    {
        IntuitionBase = OpenLibrary("intuition.library",39);
        if(IntuitionBase == NULL)
            return -1;
    }

    if (DataTypesBase == NULL)
    {
        DataTypesBase = OpenLibrary("datatypes.library",39);
        if(DataTypesBase == NULL) {
            
            if (IntuitionBase != NULL) CloseLibrary(IntuitionBase);

            return -1;
        }
    }

    if (UtilityBase == NULL)
    {
        UtilityBase = OpenLibrary("utility.library",39);
        if(UtilityBase == NULL) {
            
            if (DataTypesBase != NULL) CloseLibrary(DataTypesBase);
            if (IntuitionBase != NULL) CloseLibrary(IntuitionBase);

            return -1;
        }
    }

	if (GfxBase == NULL) {
		GfxBase = OpenLibrary("graphics.library",39);
		if(GfxBase == NULL) {

            if (UtilityBase!= NULL) CloseLibrary(UtilityBase);
            if (DataTypesBase!= NULL) CloseLibrary(DataTypesBase);
            if (IntuitionBase!= NULL) CloseLibrary(IntuitionBase);

            return -1;
        }

	}

    if (GadToolsBase == NULL) {
        GadToolsBase = OpenLibrary("gadtools.library",39);
        if (GadToolsBase == NULL) {

            if (UtilityBase!= NULL) CloseLibrary(UtilityBase);
            if (DataTypesBase!= NULL) CloseLibrary(DataTypesBase);
            if (IntuitionBase!= NULL) CloseLibrary(IntuitionBase);
            if (GfxBase!=NULL) CloseLibrary(GfxBase);

            return -1;
        }
    }

    if (DiskfontBase == NULL) {
        DiskfontBase = OpenLibrary("diskfont.library",39);
        if (DiskfontBase == NULL) {

            if (UtilityBase!= NULL) CloseLibrary(UtilityBase);
            if (DataTypesBase!= NULL) CloseLibrary(DataTypesBase);
            if (IntuitionBase!= NULL) CloseLibrary(IntuitionBase);
            if (GfxBase!=NULL) CloseLibrary(GfxBase);
            if (DiskfontBase!=NULL) CloseLibrary(DiskfontBase);

            return -1;
        }
    }

	startPrg();


	return 0;

}

int cleanup() {

    if (bitmap) FreeBitMap((struct BitMap *)bitmap);
    if (DataTypesBase) CloseLibrary(DataTypesBase);
    if (IntuitionBase) CloseLibrary(IntuitionBase);
    if (UtilityBase) CloseLibrary(UtilityBase);
	if (GfxBase) CloseLibrary(GfxBase);
	
    return;

}

void drawBoard(struct RastPort rp) {

    for (int j = 0; j < 16; j++) {
        for (int i = 0; i < 16; i++) {

            // player's ship
            if (board[i + j * 16] == 1) {
                SetAPen(&rp, 101);
                RectFill(&rp, MARGIN + i * 32, MARGIN + j * 32, MARGIN + i * 32 + 32-1, MARGIN + j * 32 + 32-1);
            }

            // computer's ship
            if (state == GAME_OVER && AIHits == 23) {
                if (board[i + j * 16] == 2) {
                    SetAPen(&rp, 98);
                    RectFill(&rp, MARGIN + i * 32, MARGIN + j * 32, MARGIN + i * 32 + 32-1, MARGIN + j * 32 + 32-1);
                }
            }

            // player's miss
            if (board[i + j * 16] == 3) {
                SetAPen(&rp, 83);
                RectFill(&rp, MARGIN + i * 32, MARGIN + j * 32, MARGIN + i * 32 + 32-1, MARGIN + j * 32 + 32-1);
            }

            // player's hit
            if (board[i + j * 16] == 4) {
                SetAPen(&rp, 84);
                RectFill(&rp, MARGIN + i * 32, MARGIN + j * 32, MARGIN + i * 32 + 32-1, MARGIN + j * 32 + 32-1);
            }

            // computer's miss
            if (board[i + j * 16] == 5) {
                SetAPen(&rp, 88);
                RectFill(&rp, MARGIN + i * 32, MARGIN + j * 32, MARGIN + i * 32 + 32-1, MARGIN + j * 32 + 32-1);
            }

            // computer's hit
            if (board[i + j * 16] == 6) {
                SetAPen(&rp, 74);
                RectFill(&rp, MARGIN + i * 32, MARGIN + j * 32, MARGIN + i * 32 + 32-1, MARGIN + j * 32 + 32-1);
            }

            
        }
    }
}

void drawShips(struct RastPort rp) {

    SetAPen(&rp, 129);

    // draw ship 1
    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
            if (ship1_[i+j*3] == 1) RectFill(&rp, MARGIN+512+64+i*32, MARGIN+32+j*32, MARGIN+512+64+i*32+32, MARGIN+32+32+j*32);
        }
    }

    // draw ship 2
    for (int j = 0; j < 3; j++) {
        for (int i = 0; i < 3; i++) {
            if (ship2_[i+j*3] == 1) RectFill(&rp, MARGIN+512+64+i*32, MARGIN+32+j*32 + 64+32+32, MARGIN+512+64+i*32+32, MARGIN+32+32+j*32 + 64+32+32);
        }
    }

    // draw ship 3
    for (int j = 0; j < 2; j++) {
        for (int i = 0; i < 2; i++) {
            if (ship3_[i+j*3] == 1) RectFill(&rp, MARGIN+512+64+i*32, MARGIN+32+j*32 + 64+32+64+32, MARGIN+512+64+i*32+32, MARGIN+32+32+j*32 + 64+32+64+32);
        }
    }

    // draw ship 4
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            if (ship4_[i+j*4] == 1) RectFill(&rp, MARGIN+512+64+i*32, MARGIN+32+j*32 + 64+32+64+32+32+32+32, MARGIN+512+64+i*32+32, MARGIN+32+32+j*32 + 64+32+64+32+32+32+32);
        }
    }

    // draw ship 5
    for (int j = 0; j < 5; j++) {
        for (int i = 0; i < 5; i++) {
            if (ship5_[i+j*5] == 1) RectFill(&rp, MARGIN+512+64+i*32, MARGIN+32+j*32 + 64+32+64+32+32+32+32*4+32+32, MARGIN+512+64+i*32+32, MARGIN+32+32+j*32 + 64+32+64+32+32+32+32*4+32+32);
        }
    }
}

void drawGrid(struct RastPort rp) {

    SetAPen(&rp, 100);
    
    Move(&rp, MARGIN,MARGIN);
    Draw(&rp, MARGIN+512,MARGIN);

    Move(&rp, MARGIN+512,MARGIN);
    Draw(&rp, MARGIN+512,MARGIN+512);

    Move(&rp, MARGIN+512,MARGIN+512);
    Draw(&rp, MARGIN, MARGIN+512);

    Move(&rp, MARGIN, MARGIN+512);
    Draw(&rp, MARGIN, MARGIN);

    for (int j = MARGIN+32; j < MARGIN+512; j+=32) {
        Move(&rp, MARGIN, j);
        Draw(&rp, MARGIN+512, j);
    }

    for (int i = MARGIN+32; i < MARGIN+512; i+=32) {
        Move(&rp, i, MARGIN);
        Draw(&rp, i, MARGIN+512);
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