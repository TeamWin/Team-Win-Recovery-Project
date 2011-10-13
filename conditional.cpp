// checkbox.cpp - GUICheckbox object

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

extern "C" {
#include "../common.h"
#include "../minui/minui.h"
#include "../recovery_ui.h"
}

#include "rapidxml.hpp"
#include "objects.hpp"

Conditional::Conditional(xml_node<>* node)
{
    xml_attribute<>* attr;
    xml_node<>* child = (node ? node->first_node("condition") : NULL);

    mCompareOp = "=";

    if (child)
    {
        attr = child->first_attribute("var1");
        if (attr)
            mVar1 = attr->value();

        attr = child->first_attribute("op");
        if (attr)
            mCompareOp = attr->value();

        attr = child->first_attribute("var2");
        if (attr)
            mVar2 = attr->value();
    }
}

Conditional::~Conditional()
{
}

std::string Conditional::GetConditionVariable()
{
    return mVar1;
}

bool Conditional::isConditionTrue()
{
    // This is used to hold the proper value of "true" based on the '!' NOT flag
    bool bTrue = true;

    if (mVar1.empty())  return true;

    if (!mCompareOp.empty() && mCompareOp[0] == '!')
        bTrue = false;

    if (mVar2.empty() && mCompareOp != "modified")
    {
        if (!DataManager::GetStrValue(mVar1).empty())
            return bTrue;
        return !bTrue;
    }

    string var1, var2;
    if (DataManager::GetValue(mVar1, var1))
        var1 = mVar1;
    if (DataManager::GetValue(mVar2, var2))
        var2 = mVar2;

    // This is a special case, we stat the file and that determines our result
    if (var1 == "fileexists")
    {
        struct stat st;
        if (stat(var2.c_str(), &st) == 0)
            var2 = var1;
        else
            var2 = "FAILED";
    }
    if (var1 == "mounted")
    {
        if (isMounted(mVar2))
            var2 = var1;
        else
            var2 = "FAILED";
    }

    if (mCompareOp.find('=') != string::npos && var1 == var2)
        return bTrue;

    if (mCompareOp.find('>') != string::npos && (atof(var1.c_str()) > atof(var2.c_str())))
        return bTrue;

    if (mCompareOp.find('<') != string::npos && (atof(var1.c_str()) < atof(var2.c_str())))
        return bTrue;

    if (mCompareOp == "modified" && var1 != mLastVal)
        return bTrue;

    return !bTrue;
}

bool Conditional::isConditionValid()
{
    return (!mVar1.empty());
}

void Conditional::NotifyPageSet()
{
    if (mCompareOp == "modified")
    {
        string val;
    
        // If this fails, val will not be set, which is perfect
        if (DataManager::GetValue(mVar1, val))
        {
            DataManager::SetValue(mVar1, "");
            DataManager::GetValue(mVar1, val);
        }
        mLastVal = val;
    }
}

bool Conditional::isMounted(string vol)
{
	FILE *fp;
    char tmpOutput[255];

	fp = fopen("/proc/mounts", "rt");
	while (fgets(tmpOutput,255,fp) != NULL)
	{
        char* pos = tmpOutput;
	    while (*pos > 32)   pos++;
        *pos = 0;
	    if (vol == tmpOutput)
        {
            fclose(fp);
            return true;
        }
    }
    fclose(fp);
    return false;
}

