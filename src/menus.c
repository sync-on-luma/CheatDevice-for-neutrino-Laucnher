#include "dbgprintf.h"
#include <stdio.h>
#include <malloc.h>

#include "menus.h"
#include "graphics.h"
#include "cheats.h"
#include "settings.h"
#include "saves.h"
#include "startgame.h"
#include "pad.h"
#include "util.h"

static menuState_t menues[NUMMENUS];
static menuState_t *activeMenu = NULL;
static int initialized = 0;
static char *menuTitleGames = "Cheats";
static char *menuTitleBootMenu = "Launch";

static const char *tempHelpTickerText = NULL;
static int  tempHelpTickerLength = 0;

#define CHUNK_SIZE 1000

int initMenus()
{
    if(!initialized)
    {
        int i;
        memset(menues, 0, sizeof(menuState_t) * NUMMENUS);
        for(i = 0; i < NUMMENUS; i++)
        {
            menues[i].identifier = (menuID_t)i;
            menues[i].items = calloc(CHUNK_SIZE, sizeof(menuItem_t *));
            menues[i].numChunks = 1;
        }

        menues[MENU_GAMES].isSorted = 1;
        menues[MENU_SAVES].isSorted = 1;
        menues[MENU_MAP_VALUES].isSorted = 1;
        menues[MENU_CODES].freeTextWhenRemoved = 1;
        menues[MENU_SAVES].freeTextWhenRemoved = 1;

        activeMenu = &menues[MENU_GAMES];
        activeMenu->text = menuTitleGames;
        initialized = 1;

        return 1;
    }
    return 0;
}

int killMenus()
{
    int i;
    
    if(initialized)
    {
        for(i = 0; i < NUMMENUS; i++)
        {
            activeMenu = &menues[i];
            menuRemoveAllItems();
        }
        
        initialized = 0;
        return 1;
    }
    
    return 0;
}

int menuInsertItem(menuItem_t *item)
{
    if(initialized)
    {
        if(activeMenu->numItems == (activeMenu->numChunks * CHUNK_SIZE))
        {
            activeMenu->numChunks++;
            DPRINTF("increasing menu size to %d chunks\n", activeMenu->numChunks);
            activeMenu->items = realloc(activeMenu->items, CHUNK_SIZE * sizeof(menuItem_t *) * activeMenu->numChunks);
        }


        if(activeMenu->numItems == 0 || !activeMenu->isSorted)
        {
            activeMenu->items[activeMenu->numItems] = item;
        }
        else
        {
            // Insert item while maintaining alphabetical order
            int i;
            for(i = 0; i < activeMenu->numItems; i++)
            {
                if(strcmp(activeMenu->items[i]->text, item->text) > 0)
                    break;
            }

            if(i < activeMenu->numItems)
            {
                // Shift items down by 1 position
                memmove(&activeMenu->items[i + 1], &activeMenu->items[i], sizeof(menuItem_t *) * (activeMenu->numItems - i));
            }

            activeMenu->items[i] = item;
        }

        activeMenu->numItems++;

        return 1;
    }
    return 0;
}

// Remove active item and sets previous item as the new active.
int menuRemoveActiveItem()
{
    if(activeMenu->numItems > 0)
    {
        if(activeMenu->freeTextWhenRemoved && activeMenu->items[activeMenu->currentItem]->text)
            free(activeMenu->items[activeMenu->currentItem]->text);
        
        if(activeMenu->identifier != MENU_GAMES && activeMenu->identifier != MENU_CHEATS && activeMenu->identifier != MENU_BOOT)
            free(activeMenu->items[activeMenu->currentItem]);
        
        activeMenu->numItems--;

        if(activeMenu->numItems > 0)
        {
            // Shift up items after curentItem.
            memmove(&activeMenu->items[activeMenu->currentItem], &activeMenu->items[activeMenu->currentItem + 1], sizeof(menuItem_t *) * (activeMenu->numItems - activeMenu->currentItem));
        }

        if(activeMenu->currentItem > 0)
            activeMenu->currentItem--;
        
        return 1;
    }
    else
        return 0;
}

int menuRemoveAllItems()
{
    int i;

    for(i = 0; i < activeMenu->numItems; i++)
    {
        menuItem_t *item = activeMenu->items[i];

        if(activeMenu->freeTextWhenRemoved && item->text)
            free(item->text);
    }

    if((activeMenu->identifier == MENU_CHEATS ||
        activeMenu->identifier == MENU_CODES ||
        activeMenu->identifier == MENU_MAP_VALUES) &&
       activeMenu->items[0])
    {
        free(activeMenu->items[0]);
    }

    activeMenu->currentItem = 0;
    activeMenu->numItems = 0;

    return 1;
}

int menuSetActiveItem(menuItem_t *item)
{
    int i = 0;
    if(!item)
        return 0;

    for(i = 0; i < activeMenu->numItems; i++)
    {
        if(activeMenu->items[i] == item)
        {
            activeMenu->currentItem = i;
            return 1;
        }
    }

    // didn't find the item
    return 0;
}

int menuRenameActiveItem(const char *str)
{
    menuItem_t *node;

    if(!activeMenu)
        return 0;

    node = activeMenu->items[activeMenu->currentItem];

    if(activeMenu->isSorted)
    {
        // Reposition item while maintaining alphabetical order
        int i;
        for(i = 0; i < activeMenu->numItems; i++)
        {
            if(strcmp(activeMenu->items[i]->text, str) > 0)
                break;
        }

        if(abs(i - activeMenu->currentItem) == 1)
        {
            // Rename in place
            free(node->text);
            node->text = strdup(str);

            return 1;
        }

        if(i < activeMenu->currentItem)
        {
            // Shift items down
            memmove(&activeMenu->items[i + 1], &activeMenu->items[i], sizeof(menuItem_t *) * (activeMenu->currentItem - i));
        }
        else if(i > activeMenu->currentItem)
        {
            // Shift items up
            memmove(&activeMenu->items[activeMenu->currentItem], &activeMenu->items[activeMenu->currentItem + 1], sizeof(menuItem_t *) * (i - activeMenu->currentItem - 1));
        }

        if(i == activeMenu->numItems)
        {
            // Renamed item went to end of list
            i--;
        }

        activeMenu->items[i] = node;
        activeMenu->currentItem = i;
    }

    free(node->text);
    node->text = strdup(str);

    return 1;
}

void *menuGetActiveItemExtra()
{
    if(!activeMenu)
        return NULL;

    if(!activeMenu->items[activeMenu->currentItem])
        return NULL;

    return activeMenu->items[activeMenu->currentItem]->extra;
}

const char *menuGetActiveItemText()
{
    if(!activeMenu)
        return NULL;
    
    if(!activeMenu->items[activeMenu->currentItem])
        return NULL;

    return activeMenu->items[activeMenu->currentItem]->text;
}

menuID_t menuGetActive()
{
    return activeMenu->identifier;
}

void *menuGetActiveExtra()
{
    return activeMenu->extra;
}

void menuSetActiveExtra(void *extra)
{
    if(!extra)
        return;

    activeMenu->extra = extra;
}

const char *menuGetActiveText()
{
    return activeMenu->text;
}

char *truncString(char *str, int pos) {
  size_t len = strlen(str);

  if (len > abs(pos)) {
    pos = len - pos;
    pos = -pos;

    if (pos > 0)
      str = str + pos;
    else
      str[len + pos] = 0;
  }

  return str;
}

void menuSetActiveText(const char *text)
{
    if(!text)
        return;

    text = truncString(text, 29);
    activeMenu->text = text;
}

void *menuGetExtra(menuID_t id)
{
    if(id > NUMMENUS-1)
        return NULL;

    return menues[id].extra;
}

int menuSetActive(menuID_t id)
{
    if( id > NUMMENUS-1 )
        return 0;

    if(id == MENU_CHEATS && cheatsGetNumGames() == 0)
        return 0;

    activeMenu = &menues[id];

    if(id == MENU_BOOT)
    {
        activeMenu->text = menuTitleBootMenu;
        menuRemoveAllItems();
        settingsLoadBootMenu();
    }
    
    else if (id == MENU_GAMES)
    {
        activeMenu->text = menuTitleGames;
    }
    return 1;
}

void menuSetCallback(menuCallbackType_t id, void (*cb)(const menuItem_t*))
{
    if(!initialized)
        return;

    activeMenu->callbacks[id] = cb;
}

void menuSetHelpTickerText(const char *text)
{
    if(!initialized || !text)
        return;

    activeMenu->helpTickerText = text;
    activeMenu->helpTickerLength = graphicsGetWidth(text);
}

void menuSetTempHelpTickerText(const char *text)
{
    if(!initialized || !text)
        return;

    tempHelpTickerText = text;
    tempHelpTickerLength = graphicsGetWidth(text);
}

void menuClearTempHelpTickerText()
{
    tempHelpTickerText = NULL;
    tempHelpTickerLength = 0;
}

int menuUp()
{
    if(activeMenu->currentItem > 0)
    {
        activeMenu->currentItem--;
        return 1;
    }
    else
        return 0;
}

int menuDown()
{
    if(activeMenu->numItems > 0)
    {
        if(activeMenu->currentItem < activeMenu->numItems - 1)
        {
            activeMenu->currentItem++;
            return 1;
        }
    }

    return 0;
}

int menuUpRepeat(int n)
{
    int i;
    for(i = 0; i < n; i++)
        menuUp();

    return 1;
}

int menuDownRepeat(int n)
{
    int i;
    for(i = 0; i < n; i++)
        menuDown();

    return 1;
}

int menuUpAlpha()
{
    int idx;
    char firstChar;

    if(activeMenu->currentItem > 0)
    {
        activeMenu->currentItem--;
        idx = activeMenu->currentItem;

        firstChar = activeMenu->items[idx]->text[0];

        while(idx > 0 && (activeMenu->items[idx - 1]->text[0] == firstChar))
            idx--;

        activeMenu->currentItem = idx;
        return 1;
    }

    return 0;
}

int menuDownAlpha()
{
    int idx;
    char firstChar;

    if(activeMenu->numItems > 0)
    {
        if(activeMenu->currentItem < activeMenu->numItems - 1)
        {
            activeMenu->currentItem++;
            idx = activeMenu->currentItem;

            firstChar = activeMenu->items[idx]->text[0];

            while(idx < (activeMenu->numItems - 1) && (activeMenu->items[idx]->text[0] == firstChar))
                idx++;

            activeMenu->currentItem = idx;
            return 1;
        }
    }

    return 0;
}

int menuGoToTop()
{
    activeMenu->currentItem = 0;
    return 1;
}

int menuGoToBottom()
{
    if(activeMenu->numItems > 0)
    {
        activeMenu->currentItem = activeMenu->numItems - 1;
        return 1;
    }

    return 0;
}

int menuGoToNextHeader()
{
    if(activeMenu->numItems > 0)
    {
        if(activeMenu->currentItem < activeMenu->numItems - 1)
        {
            activeMenu->currentItem++;
            unsigned int index = activeMenu->currentItem;
            menuItem_t *item = activeMenu->items[index];
            while((index < (activeMenu->numItems - 1)) && item->type != MENU_ITEM_HEADER)
            {
                index++;
                item = activeMenu->items[index];
            }

            activeMenu->currentItem = index;
            return 1;
        }
    }

    return 0;
}

int menuGoToPreviousHeader()
{
    if(activeMenu->currentItem > 0)
    {
        activeMenu->currentItem--;
        unsigned int index = activeMenu->currentItem;
        menuItem_t *item = activeMenu->items[index];

        while(index > 0 && item->type != MENU_ITEM_HEADER)
        {
            index--; 
            item = activeMenu->items[index];
        }

        activeMenu->currentItem = index;
        return 1;
    }

    return 0;
}

int menuProcessInputCallbacks(u32 padPressed)
{
    if(!activeMenu)
        return 0;

    const menuItem_t *current = activeMenu->items[activeMenu->currentItem];

    if((padPressed & PAD_SQUARE) &&
        activeMenu->callbacks[MENU_CALLBACK_PRESSED_SQUARE])
    {
        activeMenu->callbacks[MENU_CALLBACK_PRESSED_SQUARE](current);
        return 1;
    }
    else if((padPressed & PAD_CROSS) &&
        activeMenu->callbacks[MENU_CALLBACK_PRESSED_CROSS])
    {
        activeMenu->callbacks[MENU_CALLBACK_PRESSED_CROSS](current);
        return 1;
    }

    return 0;
}

static float windowPosition;

static void drawScrollBar()
{
    if(activeMenu->numItems < 15)
        return;

    // Based on notes from http://csdgn.org/article/scrollbar
    // All menu items
    float contentSize = activeMenu->numItems * 22;
    // Currently displayed menu items
    float windowSize = 14 * 22;
    // Size of track the grip moves over
    float trackSize = windowSize;
    float windowContentRatio = windowSize / contentSize;
    float gripSize = trackSize * windowContentRatio;
    
    if(gripSize < 20)
        gripSize = 20;
    else if(gripSize > trackSize)
        gripSize = trackSize;
    
    float windowScrollAreaSize = contentSize - windowSize;
    float windowPositionRatio = windowPosition / windowScrollAreaSize;
    float trackScrollAreaSize = trackSize - gripSize;
    float gripPositionOnTrack = trackScrollAreaSize * windowPositionRatio;

    // Draw track w/ 2px padding around grip
    graphicsDrawQuad(graphicsGetDisplayWidth() - 40, 76, 10, trackSize + 4, COLOR_BLUE);
    // Draw grip
    graphicsDrawQuad(graphicsGetDisplayWidth() - 38, 78 + gripPositionOnTrack, 6, gripSize, COLOR_WHITE);
}

typedef enum tickerState {
    tickerStateWait,
    tickerStateMoveLeft,
    tickerStateMoveRight,
} tickerState_t;

#define TICKER_WAIT_FRAMES    90
#define TICKER_START_X        20

static void drawHelpTicker()
{
    static int helpTickerX = TICKER_START_X;
    static int waitFrames = TICKER_WAIT_FRAMES;
    static tickerState_t state = tickerStateWait;
    static const char *textPrev = NULL;
    const char *text;
    int length;
    
    if(tempHelpTickerText)
    {
        // Use temporary help ticker
        text = tempHelpTickerText;
        length = tempHelpTickerLength;
    }
    else if(activeMenu->helpTickerText)
    {
        // Use primary help ticker
        text = activeMenu->helpTickerText;
        length = activeMenu->helpTickerLength;
    }
    else
    {
        // No help ticker is available
        return;
    }

    if(text != textPrev)
    {
        // Help ticker text changed
        helpTickerX = TICKER_START_X;
        waitFrames = TICKER_WAIT_FRAMES;
        state = tickerStateWait;
        textPrev = text;  
    }

    // Update state
    if(state == tickerStateWait)
        waitFrames--;
    else if(state == tickerStateMoveLeft)
        helpTickerX -= 2;
    else if(state == tickerStateMoveRight)
        helpTickerX += 2;

    // Transition to next state
    if(state == tickerStateWait && waitFrames == 0)
    {
        if((length + 2*TICKER_START_X) <= graphicsGetDisplayWidth())
        {
            // Text is small enough to fit on the screen, so it doesn't need to
            // be moved.
            waitFrames = TICKER_WAIT_FRAMES;
        }
        else if(helpTickerX >= TICKER_START_X)
            state = tickerStateMoveLeft;
        else
            state = tickerStateMoveRight;
    }
    else if((state != tickerStateWait) &&
            ((helpTickerX <= -1 *(length - graphicsGetDisplayWidth() + TICKER_START_X)) ||
            (helpTickerX >= TICKER_START_X)))
    {
        // Text has moved as far to the left or right as we allow. Wait a few
        // frames before moving it again.
        state = tickerStateWait;
        waitFrames = TICKER_WAIT_FRAMES;
    }

    graphicsDrawText(helpTickerX, 405, COLOR_WHITE, text);
}

static void drawTitle()
{
    if(activeMenu->text)
        graphicsDrawText(30, 47, COLOR_WHITE, activeMenu->text);
}

static void drawMenuItems()
{
    int yItems = 14;
    int yAbove = 7;
    int y = 76;
    int idx;

    /* Find top of viewport */
    if((activeMenu->numItems - activeMenu->currentItem) >= yAbove || (activeMenu->numItems < 8))
        idx = activeMenu->currentItem;
    else
        idx = activeMenu->numItems - yAbove;

    while(yAbove-- > 0 && idx > 0)
        idx--;

    windowPosition = idx * 22;
    
    /* Render visible items */
    int i;
    for(i = 0; i < yItems; ++i)
    {
        if(idx < activeMenu->numItems)
        {
            menuItem_t *item = activeMenu->items[idx];

            if(item->type == MENU_ITEM_NORMAL || item->type == MENU_ITEM_HAMBURGER_BUTTON)
            {
                cheatsCheat_t *cheat = item ? (cheatsCheat_t *)item->extra : NULL;
                if(activeMenu->identifier == MENU_CHEATS &&
                   cheat->enabled)
                {
                    graphicsDrawText(50, y, COLOR_YELLOW, item->text);

                    if(cheat->type == CHEAT_VALUE_MAPPED)
                    {
                        cheatsGame_t *game = activeMenu->extra;
                        // Too much indirection...
                        const char *selection = getNthString(game->valueMaps[cheat->valueMapIndex].keys, cheat->valueMapChoice);
                        graphicsDrawTextRightJustified(graphicsGetDisplayWidth() - 60, y, COLOR_YELLOW, selection);
                    }
                }
                else if(activeMenu->identifier == MENU_GAMES &&
                        cheatsIsActiveGame((cheatsGame_t *) item->extra) &&
                        cheatsGetNumEnabledCheats() > 0)
                {
                    graphicsDrawText(50, y, COLOR_YELLOW, item->text);
                }
                else
                    graphicsDrawText(50, y, COLOR_WHITE, item->text);
            }
            else
            {
                graphicsDrawText(50, y, COLOR_YELLOW, item->text);
            }

            if(item->type == MENU_ITEM_HAMBURGER_BUTTON)
            {
                graphicsDrawHamburger(graphicsGetDisplayWidth() - 50, y);
            }

            if(idx == activeMenu->currentItem)
                graphicsDrawPointer(28, y+5);

            y += 22;
            idx++;
        }
    }
}

int menuRender()
{
    if(!initialized)
        return 0;
    
    if(activeMenu->identifier == MENU_MAIN)
    {
        return 1;
    }

    drawTitle();
    drawMenuItems();
    drawHelpTicker();
    drawScrollBar();

    if(activeMenu->callbacks[MENU_CALLBACK_AFTER_DRAW])
    {
        const menuItem_t *current = activeMenu->items[activeMenu->currentItem];
        activeMenu->callbacks[MENU_CALLBACK_AFTER_DRAW](current);
    }

    return 1;
}
