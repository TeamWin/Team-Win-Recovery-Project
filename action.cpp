// image.cpp - GUIImage object

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include <string>
#include <sstream>

extern "C" {
#include "../common.h"
#include "../roots.h"
#include "../minui/minui.h"
#include "../recovery_ui.h"

int install_zip_package(const char* zip_path_filename);
int erase_volume(const char* path);
void wipe_dalvik_cache(void);
int nandroid_back_exe(void);
void set_restore_files(void);
int nandroid_rest_exe(void);
}


#include "rapidxml.hpp"
#include "objects.hpp"


void curtainClose(void);

GUIAction::GUIAction(xml_node<>* node)
    : Conditional(node)
{
    xml_node<>* child;
    xml_attribute<>* attr;

    mKey = 0;

    if (!node)  return;

    // First, get the action
    child = node->first_node("action");
    if (!child) return;

    attr = child->first_attribute("function");
    if (!attr)  return;

    mFunction = attr->value();
    mArg = child->value();

    // Now, let's get either the key or region
    child = node->first_node("touch");
    if (child)
    {
        attr = child->first_attribute("key");
        if (attr)
        {
            std::string key = attr->value();
    
            if (key == "home")          mKey = KEY_HOME;
            else if (key == "menu")     mKey = KEY_MENU;
            else if (key == "back")     mKey = KEY_BACK;
            else if (key == "search")   mKey = KEY_SEARCH;
            else if (key == "voldown")  mKey = KEY_VOLUMEDOWN;
            else if (key == "volup")    mKey = KEY_VOLUMEUP;
            else if (key == "power")    mKey = KEY_POWER;
            else                        mKey = atol(key.c_str());
        }
        else
        {
            attr = child->first_attribute("x");
            if (!attr)  return;
            mActionX = atol(attr->value());
            attr = child->first_attribute("y");
            if (!attr)  return;
            mActionY = atol(attr->value());
            attr = child->first_attribute("w");
            if (!attr)  return;
            mActionW = atol(attr->value());
            attr = child->first_attribute("h");
            if (!attr)  return;
            mActionH = atol(attr->value());
        }
    }
}

int GUIAction::NotifyTouch(TOUCH_STATE state, int x, int y)
{
    if (state == TOUCH_RELEASE)
        doAction();

    return 0;
}

int GUIAction::NotifyKey(int key)
{
    if (!mKey || key != mKey)    return 1;

    doAction();
    return 0;
}

int GUIAction::NotifyVarChange(std::string varName, std::string value)
{
    if (varName.empty() && !isConditionValid() && !mKey && !mActionW)
        doAction();

    if (isConditionValid() && isConditionTrue())
        doAction();

    return 0;
}

void* GUIAction::thread_start(void *cookie)
{
    GUIAction* ourThis = (GUIAction*) cookie;

    ourThis->doAction(1);
    return NULL;
}

void GUIAction::flash_zip(std::string filename)
{
    DataManager::SetValue("ui_progress", 0);

    if (filename.empty())
    {
        LOGE("No file specified.\n");
        return;
    }

    // We're going to jump to this page first, like a loading page
    gui_changePage(mArg);

    int fd = -1;
    ZipArchive zip;

    if (mzOpenZipArchive(filename.c_str(), &zip))
    {
        LOGE("Unable to open zip file.\n");
        return;
    }

    const ZipEntry* twrp = mzFindZipEntry(&zip, "META-INF/teamwin/twrp.zip");
    if (twrp == NULL)
        goto legacy;

    unlink("/tmp/twrp.zip");
    fd = creat("/tmp/twrp.zip", 0666);
    if (fd < 0)
        goto legacy;

    if (!mzExtractZipEntryToFile(&zip, twrp, fd))
        goto legacy;

    close(fd);

    if (PageManager::LoadPackage("install", "/tmp/twrp.zip"))
        goto legacy;

    mzCloseZipArchive(&zip);

    PageManager::SelectPackage("install");
    gui_changePage("main");

    install_zip_package(filename.c_str());
    DataManager::SetValue("ui_progress", 100);
    DataManager::SetValue("ui_progress", 0);

    return;

legacy:
    // In this case, we just use 
    mzCloseZipArchive(&zip);
    if (fd != -1)   close(fd);
    gui_changePage(mArg);
    install_zip_package(filename.c_str());
    DataManager::SetValue("ui_progress", 100);
    DataManager::SetValue("ui_progress", 0);
    return;
}

int GUIAction::doAction(int isThreaded)
{
    if (mFunction == "reboot")
    {
        curtainClose();

        sync();
        finish_recovery("s");

        if (mArg == "recovery")
        {
            // Reboot to recovery
            ensure_path_unmounted("/sdcard");
            __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, (void*) "recovery");
        }
        if (mArg == "poweroff")
        {
            // Power off
            __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
        }
        if (mArg == "bootloader")
        {
            __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, (void*) "bootloader");
        }

        reboot(RB_AUTOBOOT);
        return -1;
    }
    if (mFunction == "home")
    {
        PageManager::SelectPackage("TWRP");
        gui_changePage("main");
    }

    if (mFunction == "page")
        return gui_changePage(mArg);

    if (mFunction == "reload")
        return PageManager::ReloadPackage("TWRP", "/sdcard/TWRP/theme/ui.zip");

    if (mFunction == "readBackup")
    {
        set_restore_files();
        return 0;
    }

    if (isThreaded)
    {
        if (mFunction == "flash")
        {
            std::string filename;
            DataManager::GetValue("tw_filename", filename);
            flash_zip(filename);
        }
        if (mFunction == "wipe")
        {
            if (mArg != "dalvik")
                erase_volume(mArg.c_str());
            else
                wipe_dalvik_cache();
            return 0;
        }
        if (mFunction == "nandroid")
        {
            DataManager::SetValue("ui_progress", 0);

            if (mArg == "backup")
                nandroid_back_exe();
            else if (mArg == "restore")
                nandroid_rest_exe();
            else
                return -1;

            DataManager::SetValue("ui_progress", 100);
            DataManager::SetValue("ui_progress", 0);
            return 0;
        }
    }
    else
    {
        pthread_t t;
        pthread_create(&t, NULL, thread_start, this);
        return 0;
    }
    return -1;
}

