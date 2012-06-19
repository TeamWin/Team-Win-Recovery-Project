// keyboard.cpp - GUIKeyboard object

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
#include "../minuitwrp/minui.h"
#include "../recovery_ui.h"
}

#include "rapidxml.hpp"
#include "objects.hpp"

GUIKeyboard::GUIKeyboard(xml_node<>* node)
	: Conditional(node)
{
	int layoutindex, rowindex, keyindex, Xindex, Yindex, keyHeight, keyWidth;
	char resource[9], layout[7], row[4], key[5];
	xml_attribute<>* attr;
	xml_node<>* child;
	xml_node<>* keylayout;
	xml_node<>* keyrow;

	for (layoutindex=0; layoutindex<MAX_KEYBOARD_LAYOUTS; layoutindex++)
		keyboardImg[layoutindex] = NULL;

	mRendered = false;
	currentLayout = 1;
	mAction = NULL;
	KeyboardHeight = KeyboardWidth = cursorLocation = 0;

	if (!node)  return;

	// Load the action
	mAction = new GUIAction(node);

	// Load the images for the different layouts
	child = node->first_node("layout");
	if (child)
	{
		layoutindex = 1;
		strcpy(resource, "resource1");
		attr = child->first_attribute(resource);
		while (attr && layoutindex < (MAX_KEYBOARD_LAYOUTS + 1)) {
			keyboardImg[layoutindex - 1] = PageManager::FindResource(attr->value());

			layoutindex++;
			resource[8] = (char)(layoutindex + 48);
			attr = child->first_attribute(resource);
		}
	}

	// Check the first image to get height and width
	if (keyboardImg[0] && keyboardImg[0]->GetResource())
	{
		KeyboardWidth = gr_get_width(keyboardImg[0]->GetResource());
		KeyboardHeight = gr_get_height(keyboardImg[0]->GetResource());
	}

	// Get the data variable
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

	// Load all of the layout maps
	layoutindex = 1;
	strcpy(layout, "layout1");
	keylayout = node->first_node(layout);
	while (keylayout)
	{
		if (layoutindex > MAX_KEYBOARD_LAYOUTS) {
			LOGE("Too many layouts defined in keyboard.\n");
			return;
		}

		child = keylayout->first_node("keysize");
		if (child) {
			attr = child->first_attribute("height");
			if (attr)
				keyHeight = atoi(attr->value());
			else
				keyHeight = 0;
			attr = child->first_attribute("width");
			if (attr)
				keyWidth = atoi(attr->value());
			else
				keyWidth = 0;
		}

		rowindex = 1;
		Yindex = 0;
		strcpy(row, "row1");
		keyrow = keylayout->first_node(row);
		while (keyrow)
		{
			if (rowindex > MAX_KEYBOARD_ROWS) {
				LOGE("Too many rows defined in keyboard.\n");
				return;
			}

			Yindex += keyHeight;
			row_heights[layoutindex - 1][rowindex - 1] = Yindex;

			keyindex = 1;
			Xindex = 0;
			strcpy(key, "key01");
			attr = keyrow->first_attribute(key);

			while (attr) {
				string stratt;
				char keyinfo[255];

				if (keyindex > MAX_KEYBOARD_KEYS) {
					LOGE("Too many keys defined in a keyboard row.\n");
					return;
				}

				stratt = attr->value();
				if (strlen(stratt.c_str()) >= 255) {
					LOGE("Key info on layout%i, row%i, key%dd is too long.\n", layoutindex, rowindex, keyindex);
					return;
				}

				strcpy(keyinfo, stratt.c_str());

				if (strlen(keyinfo) == 0) {
					LOGE("No key info on layout%i, row%i, key%dd.\n", layoutindex, rowindex, keyindex);
					return;
				}

				if (strlen(keyinfo) == 1) {
					// This is a single key, simple definition
					keyboard_keys[layoutindex - 1][rowindex - 1][keyindex - 1].key = keyinfo[0];
					Xindex += keyWidth;
					keyboard_keys[layoutindex - 1][rowindex - 1][keyindex - 1].end_x = Xindex - 1;
				} else {
					// This key has extra data
					char* ptr;
					char* offset;
					char* keyitem;
					char foratoi[10];

					ptr = keyinfo;
					offset = keyinfo;
					while (*ptr > 32 && *ptr != ':')
						ptr++;
					if (*ptr != 0)
						*ptr = 0;

					strcpy(foratoi, offset);
					Xindex += atoi(foratoi);
					keyboard_keys[layoutindex - 1][rowindex - 1][keyindex - 1].end_x = Xindex - 1;

					ptr++;
					if (*ptr == 0) {
						// This is an empty area
						keyboard_keys[layoutindex - 1][rowindex - 1][keyindex - 1].key = 0;
					} else if (strlen(ptr) == 1) {
						// This is the character that this key uses
						keyboard_keys[layoutindex - 1][rowindex - 1][keyindex - 1].key = *ptr;
					} else if (*ptr == 'c') {
						// This is an ASCII character code
						keyitem = ptr + 2;
						strcpy(foratoi, keyitem);
						keyboard_keys[layoutindex - 1][rowindex - 1][keyindex - 1].key = atoi(foratoi);
					} else if (*ptr == 'l') {
						// This is a different layout
						keyitem = ptr + 6;
						keyboard_keys[layoutindex - 1][rowindex - 1][keyindex - 1].key = 254;
						strcpy(foratoi, keyitem);
						keyboard_keys[layoutindex - 1][rowindex - 1][keyindex - 1].layout = atoi(foratoi);
					} else if (*ptr == 'a') {
						// This is an action
						keyboard_keys[layoutindex - 1][rowindex - 1][keyindex - 1].key = 253;
					} else
						LOGE("Invalid key info on layout%i, row%i, key%02i.\n", layoutindex, rowindex, keyindex);
				}
				keyindex++;
				sprintf(key, "key%02i", keyindex);
				attr = keyrow->first_attribute(key);
			}
			rowindex++;
			row[3] = (char)(rowindex + 48);
			keyrow = keylayout->first_node(row);
		}
		layoutindex++;
		layout[6] = (char)(layoutindex + 48);
		keylayout = node->first_node(layout);
	}

	int x, y, w, h;
	// Load the placement
	LoadPlacement(node->first_node("placement"), &x, &y, &w, &h);
	SetActionPos(x, y, KeyboardWidth, KeyboardHeight);
	SetRenderPos(x, y, w, h);
	return;
}

GUIKeyboard::~GUIKeyboard()
{
	int layoutindex;

	for (layoutindex=0; layoutindex<MAX_KEYBOARD_LAYOUTS; layoutindex++)
		if (keyboardImg[layoutindex])   delete keyboardImg[layoutindex];
}

int GUIKeyboard::Render(void)
{
	if (!isConditionTrue())
	{
		mRendered = false;
		return 0;
	}

	int ret = 0;

	if (keyboardImg[currentLayout - 1] && keyboardImg[currentLayout - 1]->GetResource())
		gr_blit(keyboardImg[currentLayout - 1]->GetResource(), 0, 0, KeyboardWidth, KeyboardHeight, mRenderX, mRenderY);

	mRendered = true;
	return ret;
}

int GUIKeyboard::Update(void)
{
	if (!isConditionTrue())	 return (mRendered ? 2 : 0);
	if (!mRendered)			 return 2;

	return 0;
}

int GUIKeyboard::SetRenderPos(int x, int y, int w, int h)
{
	mRenderX = x;
	mRenderY = y;
	if (w || h)
	{
		mRenderW = KeyboardWidth;
		mRenderH = KeyboardHeight;
	}

	if (mAction)		mAction->SetActionPos(mRenderX, mRenderY, mRenderW, mRenderH);
	SetActionPos(mRenderX, mRenderY, mRenderW, mRenderH);
	return 0;
}

int GUIKeyboard::NotifyTouch(TOUCH_STATE state, int x, int y)
{
	static int startSelection = -1;

	if (!isConditionTrue())	 return -1;

	switch (state)
	{
	case TOUCH_START:
		startSelection = -1;
		break;
	case TOUCH_DRAG:
		startSelection = -1;
		break;
	case TOUCH_RELEASE:
		if (startSelection == 0)
			return 0;

		unsigned int indexy, indexx, rely, relx, rowIndex;

		rely = y - mRenderY;
		relx = x - mRenderX;

		// Find the correct row
		for (indexy=0; indexy<MAX_KEYBOARD_ROWS; indexy++) {
			if (row_heights[currentLayout - 1][indexy] > rely) {
				rowIndex = indexy;
				indexy = MAX_KEYBOARD_ROWS;
			}
		}

		// Find the correct key (column)
		for (indexx=0; indexx<MAX_KEYBOARD_KEYS; indexx++) {
			if (keyboard_keys[currentLayout - 1][rowIndex][indexx].end_x > relx) {
				// This is the key that was pressed!
				if ((int)keyboard_keys[currentLayout - 1][rowIndex][indexx].key == 8) {
					//Backspace
					string variableValue;

					DataManager::GetValue(mVariable, variableValue);
					if (variableValue.size() > 0) {
						variableValue.resize(variableValue.size() - 1);
						DataManager::SetValue(mVariable, variableValue);
					}
				} else if ((int)keyboard_keys[currentLayout - 1][rowIndex][indexx].key < 253 && (int)keyboard_keys[currentLayout - 1][rowIndex][indexx].key > 0) {
					// Regular key
					string variableValue;

					DataManager::GetValue(mVariable, variableValue);
					variableValue += keyboard_keys[currentLayout - 1][rowIndex][indexx].key;
					DataManager::SetValue(mVariable, variableValue);
				} else if ((int)keyboard_keys[currentLayout - 1][rowIndex][indexx].key == 254) {
					// Switch layouts
					currentLayout = keyboard_keys[currentLayout - 1][rowIndex][indexx].layout;
					mRendered = false;
				} else if ((int)keyboard_keys[currentLayout - 1][rowIndex][indexx].key == 253) {
					// Action
					return (mAction ? mAction->NotifyTouch(state, x, y) : 1);
				}
				indexx = MAX_KEYBOARD_KEYS;
			}
		}
		break;
	}

	return 0;
}
