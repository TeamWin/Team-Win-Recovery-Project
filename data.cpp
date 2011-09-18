/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <linux/input.h>
#include <pthread.h>
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
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include <sstream>

extern "C" {
#include "../common.h"
#include "../roots.h"
#include "../minui/minui.h"
#include "../recovery_ui.h"
#include "../minzip/Zip.h"
#include "../settings_file.h"
}

#include "rapidxml.hpp"
#include "objects.hpp"

extern "C" int get_battery_level(void);

std::map<std::string, std::string> DataManager::mValues;
std::map<std::string, char*> DataManager::mMappedValues;

int DataManager::LoadValues(void)
{
    mValues.insert(std::pair<std::string,std::string>("version", tw_version_val));
    mValues.insert(std::pair<std::string,std::string>("battery", "unavailable"));
    mValues.insert(std::pair<std::string,std::string>("time", "now"));
    mValues.insert(std::pair<std::string,std::string>("cwd", "/sdcard"));
    return 0;
}

int DataManager::GetValue(const std::string varName, std::string& value)
{
    std::map<std::string, std::string>::iterator it;

    // Handle special dynamic cases
    if (varName == "time")
    {
        char tmp[32];

        struct tm *current;
        time_t now;
        now = time(0);
        current = localtime(&now);

        if (current->tm_hour > 12)
            sprintf(tmp, "%d:%02d PM", current->tm_hour - 12, current->tm_min);
        else
            sprintf(tmp, "%d:%02d AM", current->tm_hour == 0 ? 12 : current->tm_hour, current->tm_min);

        value = tmp;
        return 0;
    }
    if (varName == "battery")
    {
        char tmp[16];

        sprintf(tmp, "%i%%", get_battery_level());
        value = tmp;
        return 0;
    }

    it = mValues.find(varName);
    if (it == mValues.end()) return -1;
    value = it->second;
    return 0;
}

int DataManager::SetValue(const std::string varName, std::string value, int persist /* = 0 */)
{
    std::map<std::string, std::string>::iterator it;
    std::map<std::string, char*>::iterator it2;

    it2 = mMappedValues.find(varName);
    if (it2 != mMappedValues.end())
    {
        strcpy(it2->second, value.c_str());
    }
    else
    {
        it = mValues.find(varName);
        if (it == mValues.end())
        {
            mValues.insert(std::pair<std::string,std::string>(varName, value));
        }
        else
        {
            it->second = value;
        }
    }

    // TODO: Persist

//    LOGI("Varaible %s set to '%s'\n", varName.c_str(), value.c_str());
    PageManager::NotifyVarChange(varName, value);
    return 0;
}

int DataManager::SetValue(const std::string varName, int value, int persist /* = 0 */)
{
    std::ostringstream valStr;
    valStr << value;
    return SetValue(varName, valStr.str(), persist);;
}

int DataManager::SetValue(const std::string varName, float value, int persist /* = 0 */)
{
    std::ostringstream valStr;
    valStr << value;
    return SetValue(varName, valStr.str(), persist);;
}

int DataManager::MapValue(const std::string varName, char* value)
{
    mMappedValues.insert(std::pair<std::string,char*>(varName, value));
    return 0;
}

extern "C" void gui_set_progress(float fraction)
{
    DataManager::SetValue("ui_progress", (float) (fraction * 100.0));
    return;
}

extern "C" void gui_update_progress(float portion, int seconds)
{
    DataManager::SetValue("ui_progress_portion", (float) (portion * 100.0));
    DataManager::SetValue("ui_progress_frames", seconds * 30);
    return;
}

extern "C" int gui_map_variable(const char* varName, char* value)
{
    return DataManager::MapValue(varName, value);
}

