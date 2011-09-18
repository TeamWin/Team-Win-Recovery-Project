// fill.cpp - GUIFill object

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

GUIFill::GUIFill(xml_node<>* node)
{
    xml_attribute<>* attr;
    xml_node<>* child;

    if (!node)
        return;

    attr = node->first_attribute("color");
    if (!attr)
        return;

    std::string color = attr->value();
    ConvertStrToColor(color, &mColor);

    child = node->first_node("placement");
    if (!child)
        return;

    attr = child->first_attribute("x");
    if (attr)   mRenderX = atol(attr->value());

    attr = child->first_attribute("y");
    if (attr)   mRenderY = atol(attr->value());

    attr = child->first_attribute("w");
    if (attr)   mRenderW = atol(attr->value());

    attr = child->first_attribute("h");
    if (attr)   mRenderH = atol(attr->value());

    return;
}

int GUIFill::Render(void)
{
    gr_color(mColor.red, mColor.green, mColor.blue, mColor.alpha);
    gr_fill(mRenderX, mRenderY, mRenderW, mRenderH);
    return 0;
}

