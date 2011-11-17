// console.cpp - GUIConsole object

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


static std::vector<std::string> gConsole;

extern "C" void gui_print(const char *fmt, ...)
{
    char buf[512];          // We're going to limit a single request to 512 bytes

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, 512, fmt, ap);
    va_end(ap);

    char *start, *next;
    for (start = next = buf; *next != '\0'; next++)
    {
        if (*next == '\n')
        {
            *next = '\0';
            next++;
            
            std::string line = start;
            gConsole.push_back(line);
            start = next;

            // Handle the normal \n\0 case
            if (*next == '\0')
                return;
        }
    }
    std::string line = start;
    gConsole.push_back(line);
    return;
}

extern "C" void gui_print_overwrite(const char *fmt, ...)
{
    char buf[512];          // We're going to limit a single request to 512 bytes

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, 512, fmt, ap);
    va_end(ap);

    // Pop the last line, and we can continue
    if (!gConsole.empty())   gConsole.pop_back();

    char *start, *next;
    for (start = next = buf; *next != '\0'; next++)
    {
        if (*next == '\n')
        {
            *next = '\0';
            next++;
            
            std::string line = start;
            gConsole.push_back(line);
            start = next;

            // Handle the normal \n\0 case
            if (*next == '\0')
                return;
        }
    }
    std::string line = start;
    gConsole.push_back(line);
    return;
}

GUIConsole::GUIConsole(xml_node<>* node)
{
    xml_attribute<>* attr;
    xml_node<>* child;

    mFont = NULL;
    mCurrentLine = -1;
    memset(&mForegroundColor, 255, sizeof(COLOR));
    memset(&mBackgroundColor, 0, sizeof(COLOR));
    mBackgroundColor.alpha = 255;
    memset(&mScrollColor, 0x08, sizeof(COLOR));
    mScrollColor.alpha = 255;
    mLastCount = 0;
    mSlideout = 0;
    mSlideoutState = 0;

    if (!node)
    {
        mRenderX = 0; mRenderY = 0; mRenderW = gr_fb_width(); mRenderH = gr_fb_height();
        mStubX = 0; mStubY = 0; mStubW = 0; mStubH = 0;
    }
    else
    {
        child = node->first_node("font");
        if (child)
        {
            attr = child->first_attribute("resource");
            if (attr)
                mFont = PageManager::FindResource(attr->value());
        }

        child = node->first_node("color");
        if (child)
        {
            attr = child->first_attribute("foreground");
            if (attr)
            {
                std::string color = attr->value();
                ConvertStrToColor(color, &mForegroundColor);
            }
            attr = child->first_attribute("background");
            if (attr)
            {
                std::string color = attr->value();
                ConvertStrToColor(color, &mBackgroundColor);
            }
            attr = child->first_attribute("scroll");
            if (attr)
            {
                std::string color = attr->value();
                ConvertStrToColor(color, &mScrollColor);
            }
        }

        // Load the placement
        LoadPlacement(node->first_node("placement"), &mRenderX, &mRenderY, &mRenderW, &mRenderH);

        mStubX = mRenderX;    mStubY = mRenderY;    mStubW = mRenderW;    mStubH = mRenderH;

        child = node->first_node("slideout");
        if (child)
        {
            mSlideout = 1;

            attr = child->first_attribute("x");
            if (attr)   mStubX = atol(attr->value());

            attr = child->first_attribute("y");
            if (attr)   mStubY = atol(attr->value());

            attr = child->first_attribute("resource");
            if (attr)   mSlideoutImage = PageManager::FindResource(attr->value());

            if (mSlideoutImage && mSlideoutImage->GetResource())
            {
                mStubW = gr_get_width(mSlideoutImage->GetResource());
                mStubH = gr_get_height(mSlideoutImage->GetResource());
            }
        }
    }

    gr_getFontDetails(mFont, &mFontHeight, NULL);

    SetRenderPos(mRenderX, mRenderY, mRenderW, mRenderH);
    return;
}

int GUIConsole::RenderSlideout(void)
{
    if (!mSlideoutImage || !mSlideoutImage->GetResource())      return -1;

    gr_blit(mSlideoutImage->GetResource(), 0, 0, mStubW, mStubH, mStubX, mStubY);
    return 0;
}

int GUIConsole::RenderConsole(void)
{
    void* fontResource = NULL;
    if (mFont)  fontResource = mFont->GetResource();

    // We fill the background
    gr_color(mBackgroundColor.red, mBackgroundColor.green, mBackgroundColor.blue, 255);
    gr_fill(mRenderX, mRenderY, mRenderW, mRenderH);

    gr_color(mScrollColor.red, mScrollColor.green, mScrollColor.blue, mScrollColor.alpha);
    gr_fill(mRenderX + (mRenderW * 9 / 10), mRenderY, (mRenderW / 10), mRenderH);

    // Render the lines
    gr_color(mForegroundColor.red, mForegroundColor.green, mForegroundColor.blue, mForegroundColor.alpha);

    // Don't try to continue to render without data
    mLastCount = gConsole.size();
    if (mLastCount == 0)        return (mSlideout ? RenderSlideout() : 0);

    // Find the start point
    int start;
    int curLine = mCurrentLine;    // Thread-safing (Another thread updates this value)
    if (curLine == -1)
    {
        start = mLastCount - mMaxRows;
    }
    else
    {
        if (curLine > (int) mLastCount)   curLine = (int) mLastCount;
        if ((int) mMaxRows > curLine)     curLine = (int) mMaxRows;
        start = curLine - mMaxRows;
    }

    unsigned int line;
    for (line = 0; line < mMaxRows; line++)
    {
        if ((start + (int) line) >= 0 && (start + (int) line) < (int) mLastCount)
        {
            gr_text(mRenderX, mStartY + (line * mFontHeight), gConsole[start + line].c_str(), fontResource);
        }
    }
    return (mSlideout ? RenderSlideout() : 0);
}

int GUIConsole::Render(void)
{
    if (mSlideout && mSlideoutState == 0)
    {
        return RenderSlideout();
    }
    return RenderConsole();
}

int GUIConsole::Update(void)
{
    if (mSlideout && mSlideoutState != 1)
    {
        if (mSlideoutState > 1)
        {
            // We need a full update
            mSlideoutState -= 2;
            return 2;
        }
        return 0;
    }

    if (mCurrentLine == -1 && mLastCount != gConsole.size())
    {
        // We can use Render, and return for just a flip
        Render();
        return 1;
    }
    else if (mLastTouchY >= 0)
    {
        // They're still touching, so re-render
        Render();
        return 1;
    }
    return 0;
}

int GUIConsole::SetRenderPos(int x, int y, int w, int h)
{
    // Adjust the stub position accordingly
    mStubX += (x - mRenderX);
    mStubY += (y - mRenderY);

    mRenderX = x;
    mRenderY = y;
    if (w || h)
    {
        mRenderW = w;
        mRenderH = h;
    }

    int height = mRenderH - (mSlideout ? mStubH : 0);

    // Calculate the max rows
    mMaxRows = height / mFontHeight;

    // Adjust so we always fit to bottom
    mStartY = mRenderY + (height % mFontHeight);

    if (mSlideout && mSlideoutState == 0)
        SetActionPos(mRenderX, mRenderY, mRenderW, mRenderH);
    else
        SetActionPos(mStubX, mStubY, mStubW, mStubH);
    return 0;
}

// IsInRegion - Checks if the request is handled by this object
//  Return 0 if this object handles the request, 1 if not
int GUIConsole::IsInRegion(int x, int y)
{
    if (mSlideout && mSlideoutState == 1)
    {
        return (x < mRenderX || x > mRenderX + mRenderW || y < mRenderY || y > mRenderY + mRenderH) ? 0 : 1;
    }
    return (x < mStubX || x > mStubX + mStubW || y < mStubY || y > mStubY + mStubH) ? 0 : 1;
}

// NotifyTouch - Notify of a touch event
//  Return 0 on success, >0 to ignore remainder of touch, and <0 on error
int GUIConsole::NotifyTouch(TOUCH_STATE state, int x, int y)
{
    if (mSlideout && mSlideoutState == 0)
    {
        if (state == TOUCH_START)
        {
            mSlideoutState = 3;
            SetActionPos(mRenderX, mRenderY, mRenderW, mRenderH);
            return 1;
        }
    }
    else if (mSlideout)
    {
        // Are we sliding it back in?
        if (state == TOUCH_START)
        {
            if (x > mStubX && x < (mStubX + mStubW) && y > mStubY && y < (mStubY + mStubH))
            {
                mSlideoutState = 2;
                SetActionPos(mStubX, mStubY, mStubW, mStubH);
                return 1;
            }
        }
    }

    // If we don't have enough lines to scroll, throw this away.
    if (mLastCount < mMaxRows)   return 1;

    // We are scrolling!!!
    switch (state)
    {
    case TOUCH_START:
        mLastTouchX = x;
        mLastTouchY = y;
        if ((x - mRenderX) > ((9 * mRenderW) / 10))
            mSlideMultiplier = 10;
        else
            mSlideMultiplier = 1;
        break;

    case TOUCH_DRAG:
        // This handles tapping
        if (x == mLastTouchX && y == mLastTouchY)   break;
        mLastTouchX = -1;

        if (y > mLastTouchY + 5)
        {
            mLastTouchY = y;
            if (mCurrentLine == -1)
                mCurrentLine = mLastCount - mMaxRows;
            else if (mCurrentLine > mSlideMultiplier)
                mCurrentLine -= mSlideMultiplier;
            else
                mCurrentLine = mMaxRows;

            if (mCurrentLine < (int) mMaxRows)
                mCurrentLine = mMaxRows;
        }
        else if (y < mLastTouchY - 5)
        {
            mLastTouchY = y;
            if (mCurrentLine >= 0)
            {
                mCurrentLine += mSlideMultiplier;
                if (mCurrentLine >= (int) mLastCount)
                    mCurrentLine = -1;
            }
        }
        break;

    case TOUCH_RELEASE:
        // On a tap, we jump to the tail
        if (mLastTouchX >= 0)
            mCurrentLine = -1;

        mLastTouchY = -1;
        break;
    }
    return 0;
}

