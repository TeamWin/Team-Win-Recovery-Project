// data.h - Base classes for data manager of GUI

#ifndef _DATA_HEADER
#define _DATA_HEADER

int DataManager_ResetDefaults();
int DataManager_LoadValues(const char* filename);
const char* DataManager_GetStrValue(const char* varName);
int DataManager_GetIntValue(const char* varName);

int DataManager_SetStrValue(const char* varName, char* value);
int DataManager_SetIntValue(const char* varName, int value);
int DataManager_SetFloatValue(const char* varName, float value);

int DataManager_ToggleIntValue(const char* varName);

void DataManager_DumpValues();

#endif  // _DATA_HEADER

