#include "dbgprintf.h"
#include <stdio.h>
#include <string.h>

#include "libraries/ini.h"
#include "settings.h"
#include "util.h"
#include "menus.h"
#include "graphics.h"
#include "cheats.h"
#include "startgame.h"

typedef struct settings
{
    char *databaseReadOnlyPath;
    char *databaseReadWritePath;
    char *databasePath; // deprecated
    char *bootPaths[5];
} settings_t;

static int initialized = 0;
static settings_t settings;
static int settingsDirty = 0;

#ifdef _DTL_T10000
static char *settingsPath = "host:CheatDevicePS2.ini";
#else
static char *settingsPath = "CheatDevicePS2.ini";
#endif

static char *defaultBootPaths[] = {
    "mass:BOOT/BOOT.ELF",
    "mass:BOOT/ESR.ELF",
#ifdef SUPPORT_SYSTEM_2X6
    "mc0:boot.bin", // security dongle bootfile
#else
    "mc0:BOOT/BOOT.ELF",
#endif
    "mc1:BOOT/BOOT.ELF",
    "rom:OSDSYS"
};

static const char *HELP_TICKER = \
    "{CROSS} Select    "
    "{CIRCLE} Main Menu";

static void getINIString(struct ini_info *ini, char **dst, const char *keyName, const char *defaultValue)
{
    if(!dst || !keyName)
        return;

    const char *value = ini ? ini_get(ini, "CheatDevicePS2", keyName) : NULL;
    if(value && strlen(value) > 0)
        *dst = strdup(value);
    else
        *dst = defaultValue ? strdup(defaultValue) : NULL;

    DPRINTF("%s: %s = %s\n", __FUNCTION__, keyName, *dst);
}

static void migrateOldDatabaseSetting()
{
    // Originally the path to a single read/write database was set with the
    // "database" setting. If a game, cheat, or code line was created, modified,
    // or deleted, the entire database was rewritten.
    //
    // Now there can be a read-only database in addition to a read/write one,
    // specified by the "databaseReadOnly" and "databaseReadWrite" settings,
    // respectively. If a game, cheat, or code line is created, modified, or
    // deleted, it is written to the read/write database rather than rewriting
    // the whole database. The read-only database won't be written to by
    // CheatDevice.

    if(settings.databasePath &&
      (settings.databaseReadOnlyPath || settings.databaseReadWritePath))
    {
        // Settings file has an entry for "databaseReadWrite" and/or
        // "databaseReadOnly" in addition to "database".
        displayError(
            "Warning: The \"database\" key in the settings file\n"
            " is no longer used. The database specified by \n"
            "\"databaseReadOnly\" and/or \"databaseReadWrite\" will \n"
            "be used instead."
        );
    }
    else if(settings.databasePath)
    {
        // Settings file only has an entry for "database".
        char msg[512];
        snprintf(msg, sizeof(msg),
            "The \"database\" key in the settings file is no longer used.\n"
            "Please specify if\n"
            "\"%s\"\n"
            "should be used as a read-only or read/write database.\n"
            "Read-only: Changes will be saved to another database file\n"
            "Read/Write: Changes will be saved to this database file",
            settings.databasePath
        );

        const char *items[] = {
            "Use as read-only database",
            "Use as read-write database",
            "Cancel"
        };
        
        int ret = displayPromptMenu(items, 3, msg);

        if(ret == 0)
        {
            settings.databaseReadOnlyPath = settings.databasePath;
            settings.databasePath = NULL;
            settingsDirty = 1;
        }
        else if(ret == 1)
        {
            settings.databaseReadWritePath = settings.databasePath;
            settings.databasePath = NULL;
            settingsDirty = 1;
        }
        else
        {
            displayError("Database will not be loaded.");
        }
    }
}

int initSettings()
{
    if(initialized)
        return 0;

    DPRINTF("\n ** Initializing Settings **\n");

    struct ini_info *ini;
    if (!(ini = ini_load(settingsPath))) {
        ini = ini_load("CHTD.INI");
    }
    if (ini) {
        getINIString(ini, &settings.databasePath, "database", NULL);
        getINIString(ini, &settings.databaseReadOnlyPath, "databaseReadOnly", NULL);
        getINIString(ini, &settings.databaseReadWritePath, "databaseReadWrite", NULL);
        getINIString(ini, &settings.bootPaths[0], "boot1", defaultBootPaths[0]);
        getINIString(ini, &settings.bootPaths[1], "boot2", defaultBootPaths[1]);
        getINIString(ini, &settings.bootPaths[2], "boot3", defaultBootPaths[2]);
    }

    migrateOldDatabaseSetting();

    if(settings.databaseReadOnlyPath &&
       settings.databaseReadWritePath &&
       strcasecmp(settings.databaseReadOnlyPath,
                  settings.databaseReadWritePath) == 0)
    {
        // Same path specified for read-only and read/write database. We don't
        // want to load the same database twice, so only keep it as the
        // read/write database.
        settings.databaseReadOnlyPath = NULL;
        settingsDirty = 1;
    }

    DPRINTF("settings.databaseReadOnlyPath:  %p -> %s\n", settings.databaseReadOnlyPath, settings.databaseReadOnlyPath);
    DPRINTF("settings.databaseReadWritePath: %p -> %s\n", settings.databaseReadWritePath, settings.databaseReadWritePath);

    if(ini)
        ini_free(ini);

    initialized = 1;
    return 1;
}

int killSettings()
{
    if(!initialized)
        return 0;

    DPRINTF(" ** Killing Settings Manager **\n");

    free(settings.databasePath);
    free(settings.bootPaths[0]);
    free(settings.bootPaths[1]);
    free(settings.bootPaths[2]);

    return 1;
}

int settingsSave(char *error, int errorLen)
{
    if(!initialized)
        return 0;

    if(!settingsDirty)
        return 1;

    FILE *iniFile = fopen(settingsPath, "w");
    if(!iniFile)
    {
        if(error)
            snprintf(error, errorLen, "Failed to open \"%s\" for writing.", settingsPath);

        DPRINTF("Error saving %s\n", settingsPath);
        return 0;
    }

    fputs("[CheatDevicePS2]\n", iniFile);
    fprintf(iniFile, "databaseReadOnly = %s\n",
        settings.databaseReadOnlyPath ? settings.databaseReadOnlyPath : "");
    fprintf(iniFile, "databaseReadWrite = %s\n",
        settings.databaseReadWritePath ? settings.databaseReadWritePath : "");
    fprintf(iniFile, "boot1 = %s\n", settings.bootPaths[0]);
    fprintf(iniFile, "boot2 = %s\n", settings.bootPaths[1]);
    fprintf(iniFile, "boot3 = %s\n", settings.bootPaths[2]);

    fclose(iniFile);

    return 1;
}

char* settingsGetReadOnlyDatabasePath()
{
    if(!initialized)
        return NULL;

    return settings.databaseReadOnlyPath;
}

void settingsSetReadOnlyDatabasePath(const char *path)
{
    if(!initialized || !path)
        return;

    if(settings.databaseReadOnlyPath)
        free(settings.databaseReadOnlyPath);
    settings.databaseReadOnlyPath = strdup(path);
    settingsDirty = 1;
}

char* settingsGetReadWriteDatabasePath()
{
    if(!initialized)
        return NULL;

    return settings.databaseReadWritePath;
}

void settingsSetReadWriteDatabasePath(const char *path)
{
    if(!initialized || !path)
        return;

    if(settings.databaseReadWritePath)
        free(settings.databaseReadWritePath);
    settings.databaseReadWritePath = strdup(path);
    settingsDirty = 1;
}

// Get string array containing numPaths boot paths.
char** settingsGetBootPaths(int *numPaths)
{
    if(!initialized)
        return NULL;

    *numPaths = 3;
    return settings.bootPaths;
}

/*static void onDisplayContextMenu(const menuItem_t *selected)
{
    const char *items[] = {"Edit Path", "Cancel"};
    int ret = displayPromptMenu(items, 2, "Boot Options");

    if(ret == 0)
        settingsRenameBootPath();
}*/

static void onBootPathSelected(const menuItem_t *selected)
{
    const char *path = selected->path;
    startgameExecute(path);
}

void settingsLoadBootMenu()
{
    if(!initialized)
        return;

    int numPaths;
    char **paths = settingsGetBootPaths(&numPaths);

    menuItem_t *items = calloc(numPaths + 1, sizeof(menuItem_t));
    
    const char *text[] = {"Start Game", "Retrun To Dashboard", "Reboot Console"};
    int i;
    for(i = 0; i < numPaths; i++)
    {
        items[i].type = MENU_ITEM_NORMAL;
        items[i].text = text[i];
        items[i].path = paths[i];
        items[i].extra = (void *)&paths[i];
        menuInsertItem(&items[i]);
    }

    menuSetCallback(MENU_CALLBACK_AFTER_DRAW, cheatsDrawStats);
   // menuSetCallback(MENU_CALLBACK_PRESSED_SQUARE, onDisplayContextMenu);
    menuSetCallback(MENU_CALLBACK_PRESSED_CROSS, onBootPathSelected);
    menuSetHelpTickerText(HELP_TICKER);
}

/*void settingsRenameBootPath()
{
    if(!initialized)
        return;

    if(menuGetActive() != MENU_BOOT)
        return;

    char **bootPath = (char **)menuGetActiveItemExtra();
    if(!bootPath)
        return;

    char newPath[80];
    if(displayTextEditMenu(newPath, sizeof(newPath), menuGetActiveItemText(), "Edit Boot Path") == 0)
        return;

    free(*bootPath);
    *bootPath = strdup(newPath);
    menuRenameActiveItem(newPath);
    menuRemoveAllItems();
    settingsLoadBootMenu();
    settingsDirty = 1;
}*/
