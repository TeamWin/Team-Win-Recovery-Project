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
void wipe_data(int confirm);
void wipe_battery_stats(void);
void wipe_rotate_data(void);
int usb_storage_enable(void);
int usb_storage_disable(void);
int __system(const char *command);
};

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

    // This handles notifying the condition system of page start
    if (varName.empty() && isConditionValid())
        NotifyPageSet();

    if ((GetConditionVariable() == varName || varName.empty()) && isConditionValid() && isConditionTrue())
        doAction();

    return 0;
}

void* GUIAction::thread_start(void *cookie)
{
    GUIAction* ourThis = (GUIAction*) cookie;

    LOGI("GUIAction thread has been started.\n");
    ourThis->doAction(1);
    LOGI("GUIAction thread is terminating.\n");
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
    if (twrp != NULL)
    {
        unlink("/tmp/twrp.zip");
        fd = creat("/tmp/twrp.zip", 0666);
    }
    if (fd >= 0 && twrp != NULL && 
        mzExtractZipEntryToFile(&zip, twrp, fd) && 
        !PageManager::LoadPackage("install", "/tmp/twrp.zip"))
    {
        mzCloseZipArchive(&zip);
        PageManager::SelectPackage("install");
        gui_changePage("main");
    }
    else
    {
        // In this case, we just use 
        mzCloseZipArchive(&zip);
        gui_changePage(mArg);
    }
    if (fd >= 0)
        close(fd);

    install_zip_package(filename.c_str());
    DataManager::SetValue("ui_progress", 100);
    DataManager::SetValue("ui_progress", 0);

    DataManager::SetValue("tw_operation", "Flash");
    DataManager::SetValue("tw_partition", filename);
    DataManager::SetValue("tw_operation_status", 0);
    DataManager::SetValue("tw_operation_state", 1);
    return;
}

#ifndef _SIMULATE_ACTIONS
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
        return 0;
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

    if (mFunction == "set")
    {
        if (mArg.find('=') != string::npos)
        {
            string varName = mArg.substr(0, mArg.find('='));
            string value = mArg.substr(mArg.find('=') + 1, string::npos);

            DataManager::GetValue(value, value);
            DataManager::SetValue(varName, value);
        }
        else
            DataManager::SetValue(mArg, "1");
        return 0;
    }
    if (mFunction == "clear")
    {
        DataManager::SetValue(mArg, "0");
        return 0;
    }

    if (mFunction == "mount")
    {
        if (mArg == "usb")
        {
            usb_storage_enable();
        }
        else
        {
            string cmd = "mount " + mArg;
            __system(cmd.c_str());
        }
        return 0;
    }

    if (mFunction == "umount" || mFunction == "unmount")
    {
        if (mArg == "usb")
        {
            usb_storage_disable();
        }
        else
        {
            string cmd = "umount " + mArg;
            __system(cmd.c_str());
        }
        return 0;
    }

    if (isThreaded)
    {
        if (mFunction == "flash")
        {
            std::string filename;
            DataManager::GetValue("tw_filename", filename);

            DataManager::SetValue("tw_operation", "Flashing");
            DataManager::SetValue("tw_partition", filename);
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 0);

            flash_zip(filename);
            return 0;
        }
        if (mFunction == "wipe")
        {
            DataManager::SetValue("tw_operation", "Format");
            DataManager::SetValue("tw_partition", mArg);
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 0);

            if (mArg == "data")
                wipe_data(0);
            else if (mArg == "battery")
                wipe_battery_stats();
            else if (mArg == "rotate")
                wipe_rotate_data();
            else if (mArg == "dalvik")
                wipe_dalvik_cache();
            else
                erase_volume(mArg.c_str());

            DataManager::SetValue("tw_operation", "Format");
            DataManager::SetValue("tw_partition", mArg);
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 1);
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

#else // _SIMULATE_ACTIONS

int GUIAction::doAction(int isThreaded)
{
    if (mFunction == "reboot")
    {
        ui_print("Reboot requested to %s.\n", mArg.c_str());
        return 0;
    }
    if (mFunction == "home")
    {
        PageManager::SelectPackage("TWRP");
        gui_changePage("main");
        return 0;
    }

    if (mFunction == "page")
        return gui_changePage(mArg);

    if (mFunction == "reload")
        return PageManager::ReloadPackage("TWRP", "/sdcard/TWRP/theme/ui.zip");

    if (mFunction == "readBackup")
    {
        //set_restore_files();
        ui_print("Simulating backup contains all data available.\n");
        DataManager::SetValue(TW_RESTORE_SYSTEM_VAR, 1);
        DataManager::SetValue(TW_RESTORE_DATA_VAR, 1);
        DataManager::SetValue(TW_RESTORE_CACHE_VAR, 1);
        DataManager::SetValue(TW_RESTORE_RECOVERY_VAR, 1);
        DataManager::SetValue(TW_RESTORE_WIMAX_VAR, 1);
        DataManager::SetValue(TW_RESTORE_BOOT_VAR, 1);
        DataManager::SetValue(TW_RESTORE_ANDSEC_VAR, 1);
        DataManager::SetValue(TW_RESTORE_SDEXT_VAR, 1);
        return 0;
    }

    if (mFunction == "set")
    {
        if (mArg.find('=') != string::npos)
        {
            string varName = mArg.substr(0, mArg.find('='));
            string value = mArg.substr(mArg.find('=') + 1, string::npos);

            DataManager::GetValue(value, value);
            DataManager::SetValue(varName, value);
        }
        else
            DataManager::SetValue(mArg, "1");
        return 0;
    }
    if (mFunction == "clear")
    {
        DataManager::SetValue(mArg, "0");
        return 0;
    }

    if (mFunction == "mount")
    {
        if (mArg == "usb")
            ui_print("Mounted usb.\n");
        else
            ui_print("Mounted %s.\n", mArg.c_str());
        return 0;
    }

    if (mFunction == "umount" || mFunction == "unmount")
    {
        if (mArg == "usb")
            ui_print("Unmounted usb.\n");
        else
            ui_print("Unmounted %s.\n", mArg.c_str());
        return 0;
    }

    if (isThreaded)
    {
        if (mFunction == "flash")
        {
            DataManager::SetValue("ui_progress", 0);

            std::string filename;
            DataManager::GetValue("tw_filename", filename);

            DataManager::SetValue("tw_operation", "Flashing");
            DataManager::SetValue("tw_partition", filename);
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 0);

            // We're going to jump to this page first, like a loading page
            gui_changePage(mArg);

            ui_print("Simulating 10-second zip install of file %s...\n", filename.c_str());

            DataManager::SetValue("ui_progress_portion", 100);
            DataManager::SetValue("ui_progress_frames", 300);

            usleep(10000000);

            DataManager::SetValue("ui_progress", 100);
            DataManager::SetValue("ui_progress", 0);

            DataManager::SetValue("tw_operation", "Done");
            DataManager::SetValue("tw_partition", filename);
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 1);
            return 0;
        }
        if (mFunction == "wipe")
        {
            DataManager::SetValue("ui_progress", 0);

            DataManager::SetValue("tw_operation", "Format");
            DataManager::SetValue("tw_partition", mArg);
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 0);

            ui_print("Simulating 5-second wipe of %s\n", mArg.c_str());
            DataManager::SetValue("ui_progress_portion", 100);
            DataManager::SetValue("ui_progress_frames", 150);
            usleep(5000000);

            DataManager::SetValue("ui_progress", 100);

            DataManager::SetValue("tw_operation", "Format");
            DataManager::SetValue("tw_partition", mArg);
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 1);
            return 0;
        }
        if (mFunction == "nandroid")
        {
            DataManager::SetValue("ui_progress", 0);

            DataManager::SetValue("tw_operation", mArg);
            DataManager::SetValue("tw_partition", "system");
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 0);

            ui_print("Simulating 10-second nandroid %s of system\n", mArg.c_str());
            DataManager::SetValue("ui_progress_portion", 100);
            DataManager::SetValue("ui_progress_frames", 300);
            usleep(10000000);

            DataManager::SetValue("ui_progress", 100);

            DataManager::SetValue("tw_operation", "Completed");
            DataManager::SetValue("tw_partition", mArg);
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 1);
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

#endif

