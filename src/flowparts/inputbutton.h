/***************************************************************************
 *   Copyright (C) 2003 by David Saxton                                    *
 *   david@bluehaze.org                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef INPUTBUTTON_H
#define INPUTBUTTON_H

#include "flowpart.h"

/**
@short InputButton - does debouncing of input
@author David Saxton
*/
class InputButton : public FlowPart
{
public:
	InputButton( ICNDocument *icnDocument, bool newItem, const char *id = 0);
	~InputButton();
	
	static Item* construct( ItemDocument *itemDocument, bool newItem, const char *id);
	static LibraryItem *libraryItem();
	
	virtual void generateMicrobe( FlowCode *code);

protected:
	void dataChanged();
};

#endif
