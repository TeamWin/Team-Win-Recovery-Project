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
void fix_perms();
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
void run_script(char *str1,char *str2,char *str3,char *str4,char *str5,char *str6,char *str7, int request_confirm);
void update_tz_environment_variables();
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
	DataManager::SetValue(TW_ACTION_BUSY, 1);
    ourThis->doAction(1);
	DataManager::SetValue(TW_ACTION_BUSY, 0);
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
        // In this case, we just use the default page
        mzCloseZipArchive(&zip);
        gui_changePage(mArg);
    }
    if (fd >= 0)
        close(fd);

#ifdef _SIMULATE_ACTIONS
    for (int i = 0; i < 10; i++)
    {
        usleep(1000000);
        DataManager::SetValue("ui_progress", i * 10);
    }
#else
    install_zip_package(filename.c_str());

    // Now, check if we need to ensure TWRP remains installed...
    struct stat st;
    if (stat("/sbin/installTwrp", &st) == 0)
    {
        DataManager::SetValue("tw_operation", "Configuring TWRP");
        DataManager::SetValue("tw_partition", "");
        ui_print("Configuring TWRP...\n");
        if (__system("/sbin/installTwrp reinstall") < 0)
        {
            ui_print("Unable to configure TWRP with this kernel.\n");
        }
    }
#endif

    // Done
    DataManager::SetValue("ui_progress", 100);
    DataManager::SetValue("ui_progress", 0);

    DataManager::SetValue("tw_operation", "Flash");
    DataManager::SetValue("tw_partition", filename);
    DataManager::SetValue("tw_operation_status", 0);
    DataManager::SetValue("tw_operation_state", 1);
    return;
}

int GUIAction::doAction(int isThreaded)
{
    if (mFunction == "reboot")
    {
        curtainClose();

        sync();
        finish_recovery("s");

        // This is a special method...
        struct stat st;
        if (stat("/sbin/reboot.sh", &st) == 0)
        {
            char cmd[512];
            sprintf(cmd, "/sbin/reboot.sh %s", mArg);
            __system(cmd);
            usleep(3000000);
        }

        if (stat("/sbin/reboot", &st) == 0)
        {
            if (mArg == "recovery")
            {
                __system("/sbin/reboot recovery");
            }
            if (mArg == "poweroff")
            {
                __system("/sbin/reboot poweroff");
            }
            if (mArg == "bootloader")
            {
                __system("/sbin/reboot bootloader");
            }
            usleep(3000000);
        }

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
#ifndef _SIMULATE_ACTIONS
        set_restore_files();
#endif
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
#ifndef _SIMULATE_ACTIONS
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
#endif
    }

    if (mFunction == "umount" || mFunction == "unmount")
    {
#ifndef _SIMULATE_ACTIONS
        if (mArg == "usb")
        {
            usb_storage_disable();
        }
        else
        {
            string cmd = "umount " + mArg;
            __system(cmd.c_str());
        }
#endif
        return 0;
    }
	
	if (mFunction == "restoredefaultsettings")
	{
		DataManager::ResetDefaults();
	}
	
	if (mFunction == "copylog")
	{
#ifndef _SIMULATE_ACTIONS
		ensure_path_mounted("/sdcard");
		__system("cp /tmp/recovery.log /sdcard");
		sync();
		ui_print("Copied recovery log to /sdcard.\n");
#endif
		return 0;
	}
	
	if (mFunction == "addsubtract")
	{
		if (mArg.find("+") != string::npos)
        {
            string varName = mArg.substr(0, mArg.find('+'));
            string string_to_add = mArg.substr(mArg.find('+') + 1, string::npos);
			int amount_to_add = atoi(string_to_add.c_str());
			int value;

			DataManager::GetValue(varName, value);
            DataManager::SetValue(varName, value + amount_to_add);
			return 0;
        }
		if (mArg.find("-") != string::npos)
        {
            string varName = mArg.substr(0, mArg.find('-'));
            string string_to_subtract = mArg.substr(mArg.find('-') + 1, string::npos);
			int amount_to_subtract = atoi(string_to_subtract.c_str());
			int value;

			DataManager::GetValue(varName, value);
			value -= amount_to_subtract;
			if (value <= 0)
				value = 0;
            DataManager::SetValue(varName, value);
			return 0;
        }
	}
	
	if (mFunction == "setguitimezone")
	{
		string SelectedZone;
		DataManager::GetValue(TW_TIME_ZONE_GUISEL, SelectedZone); // read the selected time zone into SelectedZone
		string Zone = SelectedZone.substr(0, SelectedZone.find(';')); // parse to get time zone
		string DSTZone = SelectedZone.substr(SelectedZone.find(';') + 1, string::npos); // parse to get DST component
		
		int dst;
		DataManager::GetValue(TW_TIME_ZONE_GUIDST, dst); // check wether user chose to use DST
		
		string offset;
		DataManager::GetValue(TW_TIME_ZONE_GUIOFFSET, offset); // pull in offset
		
		string NewTimeZone = Zone;
		if (offset != "0")
			NewTimeZone += ":" + offset;
		
		if (dst != 0)
			NewTimeZone += DSTZone;
		
		DataManager::SetValue(TW_TIME_ZONE_VAR, NewTimeZone);
		update_tz_environment_variables();
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

#ifdef _SIMULATE_ACTIONS
            usleep(5000000);
#else
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
			
			if (mArg == "/sdcard") {
				ensure_path_mounted(SDCARD_ROOT);
				mkdir("/sdcard/TWRP", 0777);
				DataManager::Flush();
			}
#endif

            DataManager::SetValue("tw_operation", "Format");
            DataManager::SetValue("tw_partition", mArg);
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 1);
            return 0;
        }
        if (mFunction == "nandroid")
        {
            DataManager::SetValue("ui_progress", 0);

#ifdef _SIMULATE_ACTIONS
            for (int i = 0; i < 5; i++)
            {
                usleep(1000000);
                DataManager::SetValue("ui_progress", i * 20);
            }
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 1);
#else
            if (mArg == "backup")
                nandroid_back_exe();
            else if (mArg == "restore")
                nandroid_rest_exe();
            else
                return -1;
#endif

            return 0;
        }
		if (mFunction == "fixpermissions")
		{
			DataManager::SetValue("ui_progress", 0);
			DataManager::SetValue("tw_operation", "FixingPermissions");
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 0);
			LOGI("fix permissions started!\n");
#ifdef _SIMULATE_ACTIONS
            usleep(10000000);
#else
			fix_perms();
#endif
			LOGI("fix permissions DONE!\n");
			DataManager::SetValue("ui_progress", 100);
			DataManager::SetValue("ui_progress", 0);
			DataManager::SetValue("tw_operation", "FixingPermissions");
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 1);
			return 0;
		}
		if (mFunction == "partitionsd")
		{
			DataManager::SetValue("ui_progress", 0);
			DataManager::SetValue("tw_operation", "partitionsd");
            DataManager::SetValue("tw_operation_status", 0);
            DataManager::SetValue("tw_operation_state", 0);

#ifdef _SIMULATE_ACTIONS
            usleep(10000000);
#else
            // Below seen in Koush's recovery
			char sddevice[256];
			Volume *vol = volume_for_path("/sdcard");
			strcpy(sddevice, vol->device);
			// Just need block not whole partition
			sddevice[strlen("/dev/block/mmcblkX")] = NULL;

			char es[64];
			std::string ext_format;
			int ext, swap;
			DataManager::GetValue("tw_sdext_size", ext);
			DataManager::GetValue("tw_swap_size", swap);
			DataManager::GetValue("tw_sdpart_file_system", ext_format);
			sprintf(es, "/sbin/sdparted -es %dM -ss %dM -efs %s -s > /cache/part.log",ext,swap,ext_format.c_str());
			LOGI("\nrunning script: %s\n", es);
			run_script("\nContinue partitioning?",
				   "\nPartitioning sdcard : ",
				   es,
				   "\nunable to execute parted!\n(%s)\n",
				   "\nOops... something went wrong!\nPlease check the recovery log!\n",
				   "\nPartitioning complete!\n\n",
				   "\nPartitioning aborted!\n\n", 0);
			
			// recreate TWRP folder and rewrite settings - these will be gone after sdcard is partitioned
			ensure_path_mounted(SDCARD_ROOT);
			mkdir("/sdcard/TWRP", 0777);
			DataManager::Flush();
#endif
			DataManager::SetValue("ui_progress", 100);
			DataManager::SetValue("ui_progress", 0);
			DataManager::SetValue("tw_operation", "partitionsd");
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

