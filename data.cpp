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

#include <string>
#include <utility>
#include <map>
#include <fstream>
#include <sstream>

extern "C"
{
    #include "common.h"
    #include "data.h"
}

#define FILE_VERSION    0x00010000

using namespace std;

class DataManager
{
public:
    static int ResetDefaults();
    static int LoadValues(const string filename);
    static int Flush();

    // Core get routines
    static int GetValue(const string varName, string& value);
    static int GetValue(const string varName, int& value);

    // This is a dangerous function. It will create the value if it doesn't exist so it has a valid c_str
    static string& GetValueRef(const string varName);

    // Helper functions
    static string GetStrValue(const string varName);
    static int GetIntValue(const string varName);

    // Core set routines
    static int SetValue(const string varName, string value, int persist = 0);
    static int SetValue(const string varName, int value, int persist = 0);
    static int SetValue(const string varName, float value, int persist = 0);

    static void DumpValues();

protected:
    typedef pair<string, int> TStrIntPair;
    typedef pair<string, TStrIntPair> TNameValuePair;
    static map<string, TStrIntPair> mValues;
    static string mBackingFile;
    static int mInitialized;

    static map<string, string> mConstValues;

protected:
    static int SaveValues();
    static void SetDefaultValues();

    static int GetMagicValue(string varName, string& value);

};


map<string, DataManager::TStrIntPair>   DataManager::mValues;
map<string, string>                     DataManager::mConstValues;
string                                  DataManager::mBackingFile;
int                                     DataManager::mInitialized = 0;

int DataManager::ResetDefaults()
{
    mValues.clear();
    mConstValues.clear();
    SetDefaultValues();
    return 0;
}

int DataManager::LoadValues(const string filename)
{
    if (!mInitialized)
        SetDefaultValues();

    // Save off the backing file for set operations
    mBackingFile = filename;

    // Read in the file, if possible
    ifstream in(filename.c_str(), ios_base::in);

    int file_version;
    in >> file_version;
    if (file_version != FILE_VERSION)
    {
        return -1;
    }

    while (!in.eof())
    {
        string Name;
        string Value;

        in >> Name;
        in >> Value;

        if (!Name.empty())
        {
            map<string, TStrIntPair>::iterator pos;

            pos = mValues.find(Name);
            if (pos == mValues.end())
                mValues.insert(TNameValuePair(Name, TStrIntPair(Value, 1)));
            else
            {
                pos->second.first = Value;
                pos->second.second = 1;
            }
        }
    }
    return 0;
}

int DataManager::Flush()
{
    return SaveValues();
}

int DataManager::SaveValues()
{
    if (mBackingFile.empty())       return -1;

    ofstream out(mBackingFile.c_str(), ios::out | ios::trunc);
    int file_version = FILE_VERSION;
    out << file_version << endl;

    map<string, TStrIntPair>::iterator iter;
    for (iter = mValues.begin(); iter != mValues.end(); ++iter)
    {
        // Save only the persisted data
        if (iter->second.second != 0)
        {
            out << iter->first << endl;
            out << iter->second.first << endl;
        }
    }
    return 0;
}

int DataManager::GetValue(const string varName, string& value)
{
    if (!mInitialized)
        SetDefaultValues();

    map<string, string>::iterator constPos;
    constPos = mConstValues.find(varName);
    if (constPos != mConstValues.end())
    {
        value = constPos->second;
        return 0;
    }

    map<string, TStrIntPair>::iterator pos;
    pos = mValues.find(varName);
    if (pos == mValues.end())
        return -1;

    value = pos->second.first;
    return 0;
}

int DataManager::GetValue(const string varName, int& value)
{
    string data;

    if (GetValue(varName,data) != 0)
        return -1;

    value = atoi(data.c_str());
    return 0;
}

// This is a dangerous function. It will create the value if it doesn't exist so it has a valid c_str
string& DataManager::GetValueRef(const string varName)
{
    if (!mInitialized)
        SetDefaultValues();

    map<string, string>::iterator constPos;
    constPos = mConstValues.find(varName);
    if (constPos != mConstValues.end())
        return constPos->second;

    map<string, TStrIntPair>::iterator pos;
    pos = mValues.find(varName);
    if (pos == mValues.end())
        pos = (mValues.insert(TNameValuePair(varName, TStrIntPair("", 0)))).first;

    return pos->second.first;
}

// This function will return an empty string if the value doesn't exist
string DataManager::GetStrValue(const string varName)
{
    string retVal;

    GetValue(varName, retVal);
    return retVal;
}

// This function will return 0 if the value doesn't exist
int DataManager::GetIntValue(const string varName)
{
    string retVal;

    GetValue(varName, retVal);
    return atoi(retVal.c_str());
}


int DataManager::SetValue(const string varName, string value, int persist /* = 0 */)
{
    if (!mInitialized)
        SetDefaultValues();

    map<string, string>::iterator constChk;
    constChk = mConstValues.find(varName);
    if (constChk != mConstValues.end())
        return -1;

    map<string, TStrIntPair>::iterator pos;
    pos = mValues.find(varName);
    if (pos == mValues.end())
        pos = (mValues.insert(TNameValuePair(varName, TStrIntPair(value, persist)))).first;
    else
        pos->second.first = value;

    if (pos->second.second != 0)
        SaveValues();

//    PageManager::NotifyVarChange(varName, value);
    return 0;
}

int DataManager::SetValue(const string varName, int value, int persist /* = 0 */)
{
    ostringstream valStr;
    valStr << value;
    return SetValue(varName, valStr.str(), persist);;
}

int DataManager::SetValue(const string varName, float value, int persist /* = 0 */)
{
    ostringstream valStr;
    valStr << value;
    return SetValue(varName, valStr.str(), persist);;
}

void DataManager::DumpValues()
{
    map<string, TStrIntPair>::iterator iter;
    ui_print("Data Manager internal dump - Values with leading X are persisted.\n");
    for (iter = mValues.begin(); iter != mValues.end(); ++iter)
    {
        ui_print("%c %s=%s\n", iter->second.second ? 'X' : ' ', iter->first.c_str(), iter->second.first.c_str());
    }
}

void DataManager::SetDefaultValues()
{
    mInitialized = 1;

    mConstValues.insert(make_pair("TW_VERSION", "VERSION"));
    mConstValues.insert(make_pair("TW_NAN_SYSTEM", "NAN_SYSTEM"));
    mConstValues.insert(make_pair("TW_NAN_DATA", "NAN_DATA"));
    mConstValues.insert(make_pair("TW_NAN_BOOT", "NAN_BOOT"));
    mConstValues.insert(make_pair("TW_NAN_RECOVERY", "NAN_RECOVERY"));
    mConstValues.insert(make_pair("TW_NAN_CACHE", "NAN_CACHE"));
    mConstValues.insert(make_pair("TW_NAN_WIMAX", "NAN_WIMAX"));
    mConstValues.insert(make_pair("TW_NAN_ANDSEC", "NAN_ANDSEC"));
    mConstValues.insert(make_pair("TW_NAN_SDEXT", "NAND_SDEXT"));
    mConstValues.insert(make_pair("TW_REBOOT_AFTER_FLASH", "REBOOT_AFTER_FLASH"));
    mConstValues.insert(make_pair("TW_SIGNED_ZIP", "SIGNED_ZIP"));
    mConstValues.insert(make_pair("TW_COLOR_THEME", "COLOR_THEME"));
    mConstValues.insert(make_pair("TW_USE_COMPRESSION", "NAN_USE_COMPRESSION"));
    mConstValues.insert(make_pair("TW_SHOW_SPAM", "SHOW_SPAM"));
    mConstValues.insert(make_pair("TW_TIME_ZONE", "TIME_ZONE"));
    mConstValues.insert(make_pair("TW_ZIP_LOCATION", "ZIP_LOCATION"));
    mConstValues.insert(make_pair("TW_FORCE_MD5_CHECK", "FORCE_MD5"));
    mConstValues.insert(make_pair("TW_SORT_FILES_BY_DATE", "SORT_BY_DATE"));
    mConstValues.insert(make_pair("TW_SINGLE_ZIP_MODE", "SINGLE_ZIP_MODE"));
	mConstValues.insert(make_pair("TW_RM_RF", "RM_RF"));

    mConstValues.insert(make_pair(TW_VERSION_VAR, "1.1.1"));
    mValues.insert(make_pair(TW_NANDROID_SYSTEM_VAR, make_pair("1", 1)));
    mValues.insert(make_pair(TW_NANDROID_DATA_VAR, make_pair("1", 1)));
    mValues.insert(make_pair(TW_NANDROID_BOOT_VAR, make_pair("1", 1)));
    mValues.insert(make_pair(TW_NANDROID_RECOVERY_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_NANDROID_CACHE_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_NANDROID_WIMAX_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_NANDROID_ANDSEC_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_NANDROID_SDEXT_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_REBOOT_AFTER_FLASH_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_SIGNED_ZIP_VERIFY_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_FORCE_MD5_CHECK_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_COLOR_THEME_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_USE_COMPRESSION_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_SHOW_SPAM_VAR, make_pair("0", 1)));
    mValues.insert(make_pair(TW_TIME_ZONE_VAR, make_pair("CST6CDT", 1)));
    mValues.insert(make_pair(TW_ZIP_LOCATION_VAR, make_pair("/sdcard", 1)));
    mValues.insert(make_pair(TW_SORT_FILES_BY_DATE_VAR, make_pair("0", 1)));
	mValues.insert(make_pair(TW_RM_RF_VAR, make_pair("0", 1)));
}

extern "C" int DataManager_ResetDefaults()
{
    return DataManager::ResetDefaults();
}


extern "C" int DataManager_LoadValues(const char* filename)
{
    return DataManager::LoadValues(filename);
}

extern "C" int DataManager_Flush()
{
    return DataManager::Flush();
}

extern "C" int DataManager_GetValue(const char* varName, char* value)
{
    int ret;
    string str;

    ret = DataManager::GetValue(varName, str);
    if (ret == 0)
        strcpy(value, str.c_str());
    return ret;
}

extern "C" const char* DataManager_GetStrValue(const char* varName)
{
    string& str = DataManager::GetValueRef(varName);
    return str.c_str();
}

extern "C" int DataManager_GetIntValue(const char* varName)
{
    return DataManager::GetIntValue(varName);
}

extern "C" int DataManager_SetStrValue(const char* varName, char* value)
{
    return DataManager::SetValue(varName, value, 0);
}

extern "C" int DataManager_SetIntValue(const char* varName, int value)
{
    return DataManager::SetValue(varName, value, 0);
}

extern "C" int DataManager_SetFloatValue(const char* varName, float value)
{
    return DataManager::SetValue(varName, value, 0);
}

extern "C" int DataManager_ToggleIntValue(const char* varName)
{
    if (DataManager::GetIntValue(varName))
        return DataManager::SetValue(varName, 0);
    else
        return DataManager::SetValue(varName, 1);
}

extern "C" void DataManager_DumpValues()
{
    return DataManager::DumpValues();
}

