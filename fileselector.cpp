// FileSelector.cpp - GUIFileSelector object

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
#include <dirent.h>
#include <ctype.h>

#include <algorithm>

extern "C" {
#include "../common.h"
#include "../roots.h"
#include "../minui/minui.h"
#include "../recovery_ui.h"
}

#include "rapidxml.hpp"
#include "objects.hpp"

GUIFileSelector::GUIFileSelector(xml_node<>* node)
{
    xml_attribute<>* attr;
    xml_node<>* child;

    mStart = mLineSpacing = mIconWidth = mIconHeight = 0;
    mFolderIcon = mFileIcon = mBackground = mFont = NULL;
    mBackgroundX = mBackgroundY = mBackgroundW = mBackgroundH = 0;
    mShowFolders = mShowFiles = mShowNavFolders = 1;
    mUpdate = 0;
    mPathVar = "cwd";
    ConvertStrToColor("black", &mBackgroundColor);
    ConvertStrToColor("white", &mFontColor);
    
    child = node->first_node("icon");
    if (child)
    {
        attr = child->first_attribute("folder");
        if (attr)
            mFolderIcon = PageManager::FindResource(attr->value());
        attr = child->first_attribute("file");
        if (attr)
            mFileIcon = PageManager::FindResource(attr->value());
    }
    child = node->first_node("background");
    if (child)
    {
        attr = child->first_attribute("resource");
        if (attr)
            mBackground = PageManager::FindResource(attr->value());
        attr = child->first_attribute("color");
        if (attr)
        {
            std::string color = attr->value();
            ConvertStrToColor(color, &mBackgroundColor);
        }
    }

    // Handle placement
    child = node->first_node("placement");
    if (child)
    {
        attr = child->first_attribute("x");
        if (attr)   mRenderX = atol(attr->value());

        attr = child->first_attribute("y");
        if (attr)   mRenderY = atol(attr->value());

        attr = child->first_attribute("w");
        if (attr)   mRenderW = atol(attr->value());

        attr = child->first_attribute("h");
        if (attr)   mRenderH = atol(attr->value());
    }
    SetActionPos(mRenderX, mRenderY, mRenderW, mRenderH);

    // Load the font, and possibly override the color
    child = node->first_node("font");
    if (child)
    {
        attr = child->first_attribute("resource");
        if (attr)
            mFont = PageManager::FindResource(attr->value());

        attr = child->first_attribute("color");
        if (attr)
        {
            std::string color = attr->value();
            ConvertStrToColor(color, &mFontColor);
        }

        attr = child->first_attribute("spacing");
        if (attr)
            mLineSpacing = atoi(attr->value());
    }

    child = node->first_node("filter");
    if (child)
    {
        attr = child->first_attribute("extn");
        if (attr)
            mExtn = attr->value();
        attr = child->first_attribute("folders");
        if (attr)
            mShowFolders = atoi(attr->value());
        attr = child->first_attribute("files");
        if (attr)
            mShowFiles = atoi(attr->value());
        attr = child->first_attribute("nav");
        if (attr)
            mShowNavFolders = atoi(attr->value());
    }

    // Handle the path variable
    child = node->first_node("path");
    if (child)
    {
        attr = child->first_attribute("name");
        if (attr)
            mPathVar = attr->value();
        attr = child->first_attribute("default");
        if (attr)
            DataManager::SetValue(mPathVar, attr->value());
    }

    // Handle the result variable
    child = node->first_node("data");
    if (child)
    {
        attr = child->first_attribute("name");
        if (attr)
            mVariable = attr->value();
        attr = child->first_attribute("default");
        if (attr)
            DataManager::SetValue(mVariable, attr->value());
    }

    // Retrieve the line height
    gr_getFontDetails(mFont ? mFont->GetResource() : NULL, &mFontHeight, NULL);
    mLineHeight = mFontHeight;
    if (mFolderIcon && mFolderIcon->GetResource())
    {
        if (gr_get_height(mFolderIcon->GetResource()) > mLineHeight)
            mLineHeight = gr_get_width(mFolderIcon->GetResource());
        mIconWidth = gr_get_width(mFolderIcon->GetResource());
        mIconHeight = gr_get_height(mFolderIcon->GetResource());
    }
    if (mFileIcon && mFileIcon->GetResource())
    {
        if (gr_get_height(mFileIcon->GetResource()) > mLineHeight)
            mLineHeight = gr_get_width(mFileIcon->GetResource());
        mIconWidth = gr_get_width(mFileIcon->GetResource());
        mIconHeight = gr_get_height(mFileIcon->GetResource());
    }
        
    if (mBackground && mBackground->GetResource())
    {
        mBackgroundW = gr_get_width(mBackground->GetResource());
        mBackgroundH = gr_get_height(mBackground->GetResource());
    }

    // Fetch the file/folder list
    std::string value;
    DataManager::GetValue(mPathVar, value);
    GetFileList(value);
}

GUIFileSelector::~GUIFileSelector()
{
}

int GUIFileSelector::Render(void)
{
    // First step, fill background
    gr_color(mBackgroundColor.red, mBackgroundColor.green, mBackgroundColor.blue, 255);
    gr_fill(mRenderX, mRenderY, mRenderW, mRenderH);

    // Next, render the background resource (if it exists)
    if (mBackground && mBackground->GetResource())
    {
        mBackgroundX = mRenderX + ((mRenderW - mBackgroundW) / 2);
        mBackgroundY = mRenderY + ((mRenderH - mBackgroundH) / 2);
        gr_blit(mBackground->GetResource(), 0, 0, mBackgroundW, mBackgroundH, mBackgroundX, mBackgroundY);
    }

    // Now, we need the lines (icon + text)
    gr_color(mFontColor.red, mFontColor.green, mFontColor.blue, mFontColor.alpha);

    // This tells us how many lines we can actually render
    int lines = mRenderH / (mLineHeight + mLineSpacing);
    int line;

    int folderSize = mShowFolders ? mFolderList.size() : 0;
    int fileSize = mShowFiles ? mFileList.size() : 0;

    if (folderSize + fileSize < lines)  lines = folderSize + fileSize;

    void* fontResource = NULL;
    if (mFont)  fontResource = mFont->GetResource();

    int yPos = mRenderY + (mLineSpacing / 2);
    for (line = 0; line < lines; line++)
    {
        Resource* icon;
        std::string label;

        if (line + mStart < folderSize)
        {
            icon = mFolderIcon;
            label = mFolderList.at(line + mStart).d_name;
        }
        else
        {
            icon = mFileIcon;
            label = mFileList.at((line + mStart) - folderSize).d_name;
        }

        if (icon && icon->GetResource())
        {
            gr_blit(icon->GetResource(), 0, 0, mIconWidth, mIconHeight, mRenderX, yPos);
        }
        gr_text(mRenderX + mIconWidth + 5, yPos, label.c_str(), fontResource);

        // Move the yPos
        yPos += mLineHeight + mLineSpacing;
    }

    mUpdate = 0;
    return 0;
}

int GUIFileSelector::Update(void)
{
    if (mUpdate)
    {
        mUpdate = 0;
        if (Render() == 0)  return 1;
    }
    return 0;
}

int GUIFileSelector::GetSelection(int x, int y)
{
    // We only care about y position
    return (y - mRenderY) / (mLineHeight + mLineSpacing);
}

int GUIFileSelector::NotifyTouch(TOUCH_STATE state, int x, int y)
{
    static int startSelection = -1;
    static int startY = 0;
    int selection = 0;

    switch (state)
    {
    case TOUCH_START:
        startSelection = GetSelection(x,y);
        startY = y;
        break;

    case TOUCH_DRAG:
        // Check if we dragged out of the selection window
        selection = GetSelection(x,y);
        if (startSelection != selection)
        {
            startSelection = -1;

            // Handle scrolling
            if (y > (int) (startY + (mLineHeight + mLineSpacing)))
            {
                if (mStart)     mStart--;
                mUpdate = 1;
                startY = y;
            }
            else if (y < (int) (startY - (mLineHeight + mLineSpacing)))
            {
                int folderSize = mShowFolders ? mFolderList.size() : 0;
                int fileSize = mShowFiles ? mFileList.size() : 0;
                int lines = mRenderH / (mLineHeight + mLineSpacing);

                if (mStart + lines < folderSize + fileSize)     mStart++;
                mUpdate = 1;
                startY = y;
            }
        }
        break;

    case TOUCH_RELEASE:
        if (startSelection >= 0)
        {
            // We've selected an item!
            std::string str;

            int folderSize = mShowFolders ? mFolderList.size() : 0;
            int fileSize = mShowFiles ? mFileList.size() : 0;

            // Move the selection to the proper place in the array
            startSelection += mStart;

            if (startSelection < folderSize + fileSize)
            {
                if (startSelection < folderSize)
                {
                    std::string oldcwd;
                    std::string cwd;

                    str = mFolderList.at(startSelection).d_name;
                    DataManager::GetValue(mPathVar, cwd);

                    oldcwd = cwd;
                    // Ignore requests to do nothing
                    if (str == ".")     return 0;
                    if (str == "..")
                    {
                        if (cwd != "/")
                        {
                            size_t found;
                            found = cwd.find_last_of('/');
                            cwd = cwd.substr(0,found);

                            if (cwd.length() < 2)   cwd = "/";
                        }
                    }
                    else
                    {
                        // Add a slash if we're not the root folder
                        if (cwd != "/")     cwd += "/";
                        cwd += str;
                    }

                    if (mShowNavFolders == 0 && mShowFiles == 0)
                    {
                        // This is a "folder" selection
                        LOGI("Selecting folder: %s\n", cwd.c_str());
                        DataManager::SetValue(mVariable, cwd);
                    }
                    else
                    {
                        LOGI("Changing folder to: %s\n", cwd.c_str());
                        DataManager::SetValue(mPathVar, cwd);
                        if (GetFileList(cwd) != 0)
                        {
                            LOGE("Unable to change folders.\n");
                            DataManager::SetValue(mPathVar, oldcwd);
                            GetFileList(oldcwd);
                        }
                        mStart = 0;
                        mUpdate = 1;
                    }
                }
                else if (!mVariable.empty())
                {
                    str = mFileList.at(startSelection - folderSize).d_name;

                    std::string cwd;
                    DataManager::GetValue(mPathVar, cwd);
                    if (cwd != "/")     cwd += "/";
                    DataManager::SetValue(mVariable, cwd + str);
                }
            }
        }
        break;
    }
    return 0;
}

int GUIFileSelector::NotifyVarChange(std::string varName, std::string value)
{
    if (varName.empty())
    {
        // Always clear the data variable so we know to use it
        DataManager::SetValue(mVariable, "");
    }
    if (varName == mPathVar)
    {
        GetFileList(value);
        mStart = 0;
        mUpdate = 1;
        return 0;
    }
    return 0;
}

int GUIFileSelector::SetRenderPos(int x, int y, int w /* = 0 */, int h /* = 0 */)
{
    mRenderX = x;
    mRenderY = y;
    if (w || h)
    {
        mRenderW = w;
        mRenderH = h;
    }
    SetActionPos(mRenderX, mRenderY, mRenderW, mRenderH);
    mUpdate = 1;
    return 0;
}

struct convert {
   void operator()(char& c) { c = toupper((unsigned char)c); } // converts to upper case for case insensitive comparisons
};


bool GUIFileSelector::fileSort(struct dirent d1, struct dirent d2)
{
    std::string d1_str = d1.d_name;
    for_each(d1_str.begin(), d1_str.end(), convert());
    std::string d2_str = d2.d_name;
    for_each(d2_str.begin(), d2_str.end(), convert());

    return d1_str < d2_str;
}

int GUIFileSelector::GetFileList(const std::string folder)
{
    DIR* d;
    struct dirent* de;

    // Clear all data
    mFolderList.clear();
    mFileList.clear();

    d = opendir(folder.c_str());
    if (d == NULL)
    {
        LOGE("error opening %s\n", folder.c_str());
        return -1;
    }

    while ((de = readdir(d)) != NULL)
    {
        std::string entry = de->d_name;

        if (de->d_type == DT_DIR)
        {
            if (mShowNavFolders || (entry != "." && entry != ".."))
                mFolderList.push_back(*de);
        }
        else if (de->d_type == DT_REG)
        {
            if (mExtn.empty() || (entry.length() > mExtn.length() && entry.substr(entry.length() - mExtn.length()) == mExtn))
            {
                mFileList.push_back(*de);
            }
        }
    }
    closedir(d);
    std::sort(mFolderList.begin(), mFolderList.end(), fileSort);
    std::sort(mFileList.begin(), mFileList.end(), fileSort);
    return 0;
}

void GUIFileSelector::SetPageFocus(int inFocus)
{
    if (inFocus)
    {
        std::string value;
        DataManager::GetValue(mPathVar, value);
        GetFileList(value);
    }
}

