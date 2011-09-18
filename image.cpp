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

GUIImage::GUIImage(xml_node<>* node)
{
    xml_attribute<>* attr;
    xml_node<>* child;

    mImage = NULL;

    if (!node)
        return;

    child = node->first_node("image");
    if (child)
    {
        attr = child->first_attribute("resource");
        if (attr)
            mImage = PageManager::FindResource(attr->value());
    }

    child = node->first_node("placement");
    if (child)
    {
        attr = child->first_attribute("x");
        if (attr)   mRenderX = atol(attr->value());

        attr = child->first_attribute("y");
        if (attr)   mRenderY = atol(attr->value());
    }

    if (mImage && mImage->GetResource())
    {
        mRenderW = gr_get_width(mImage->GetResource());
        mRenderH = gr_get_height(mImage->GetResource());
    }

    return;
}

int GUIImage::Render(void)
{
    if (!mImage || !mImage->GetResource())      return -1;
    gr_blit(mImage->GetResource(), 0, 0, mRenderW, mRenderH, mRenderX, mRenderY);
    return 0;
}

int GUIImage::SetRenderPos(int x, int y, int w, int h)
{
    if (w || h)     return -1;
    mRenderX = x;
    mRenderY = y;
    return 0;
}

