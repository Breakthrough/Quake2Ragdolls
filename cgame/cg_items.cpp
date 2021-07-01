/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

//
// cg_screen.c
//

#include "cg_local.h"

void CG_ItemMediaInit (byte bits)
{
	bool doModels = !!(bits & ITEMBIT_MODELS);
	bool doPics = !!(bits & ITEMBIT_PICS);
	bool doSounds = !!(bits & ITEMBIT_SOUNDS);

	if (doModels)
	{
		if (cgMedia.worldModelRegistry != null)
			delete cgMedia.worldModelRegistry;
		if (cgMedia.viewModelRegistry != null)
			delete cgMedia.viewModelRegistry;
	
		cgMedia.worldModelRegistry = new refModel_t*[Items::numItems];
		cgMedia.viewModelRegistry = new refModel_t*[Items::numItems];
	}
		
	if (doSounds)
	{
		if (cgMedia.pickupSoundRegistry != null)
			delete cgMedia.pickupSoundRegistry;
		cgMedia.pickupSoundRegistry = new sfx_t*[Items::numItems];
	}

	if (doPics)
	{
		if (cgMedia.iconRegistry != null)
			delete cgMedia.iconRegistry;
		cgMedia.iconRegistry = new refMaterial_t*[Items::numItems];
	}

	for (int i = 0; i < Items::numItems; ++i)
	{
		var item = &itemlist[i];

		if (item->classname == null)
			continue;

		var splitValues = String(item->precaches).Split();

		if (doSounds)
		{
			if (item->pickup_sound)
				cgMedia.pickupSoundRegistry[i] = cgi.Snd_RegisterSound(item->pickup_sound);
		}

		if (doModels)
		{
			if (item->view_model)
				cgMedia.viewModelRegistry[i] = cgi.R_RegisterModel(item->view_model);
				
			if (item->world_model)
				cgMedia.worldModelRegistry[i] = cgi.R_RegisterModel(item->world_model);
		}

		if (doPics)
		{
			if (item->icon)
				cgMedia.iconRegistry[i] = CG_RegisterPic(item->icon);
		}

		if (splitValues.Count() != 0)
		{
			for (uint32 x = 0; x < splitValues.Count(); ++x)
			{
				if (doModels && splitValues[x].EndsWith("md2"))
					cgi.R_RegisterModel(splitValues[x].CString());
				else if (doModels && splitValues[x].EndsWith("sp2"))
					cgi.R_RegisterModel(splitValues[x].CString());
				else if (doSounds && splitValues[x].EndsWith("wav"))
					cgi.Snd_RegisterSound((char*)splitValues[x].CString());
				else if (doPics && splitValues[x].EndsWith("pcx"))
					cgi.R_RegisterPic(splitValues[x].CString());
			}
		}
	}
}