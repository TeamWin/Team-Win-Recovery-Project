/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "themes.h"
#include "settings_file.h"

void set_theme(char* tw_theme)
{
	if (strcmp(tw_theme,"0") == 0) // TEAMWIN THEME (default)
	{
		htc.r = 255;
		htc.g = 255;
		htc.b = 255;
		htc.a = 255;

		mtc.r = 0;
		mtc.g = 255;
		mtc.b = 0;
		mtc.a = 255;

		upc.r = 0;
		upc.g = 255;
		upc.b = 0;
		upc.a = 255;

		mihc.r = 0;
		mihc.g = 255;
		mihc.b = 0;
		mihc.a = 255;

		miwhc.r = 0;
		miwhc.g = 0;
		miwhc.b = 0;
		miwhc.a = 0;
		
		mhebc.r = 0;
		mhebc.g = 255;
		mhebc.b = 0;
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"1") == 0) // CM theme
	{
		htc.r = 193;
		htc.g = 193;
		htc.b = 193;
		htc.a = 255;

		mtc.r = 61;
		mtc.g = 233;
		mtc.b = 255;
		mtc.a = 255;

		upc.r = 193;
		upc.g = 193;
		upc.b = 193;
		upc.a = 255;

		mihc.r = 61;
		mihc.g = 233;
		mihc.b = 255;
		mihc.a = 255;

		miwhc.r = 0;
		miwhc.g = 0;
		miwhc.b = 0;
		miwhc.a = 0;
		
		mhebc.r = 61;
		mhebc.g = 233;
		mhebc.b = 255;
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"2") == 0) // RED
	{
		htc.r = 255;
		htc.g = 255;
		htc.b = 255;
		htc.a = 255;

		mtc.r = 255;
		mtc.g = 0;
		mtc.b = 0;
		mtc.a = 255;

		upc.r = 255;
		upc.g = 0;
		upc.b = 0;
		upc.a = 255;

		mihc.r = 255;
		mihc.g = 0;
		mihc.b = 0;
		mihc.a = 255;

		miwhc.r = 0;
		miwhc.g = 0;
		miwhc.b = 0;
		miwhc.a = 0;
		
		mhebc.r = 255;
		mhebc.g = 0;
		mhebc.b = 0;
		mhebc.a = 255;
	}
}
