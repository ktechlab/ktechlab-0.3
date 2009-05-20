/***************************************************************************
 *   Copyright (C) 2003 by David Saxton                                    *
 *   david@bluehaze.org                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef ECCELL_H
#define ECCELL_H

#include "simplecomponent.h"

class VoltageSource;

/**
@short Electrical cell
Simple electrical cell that simulates a PD and internal resistance
@author David Saxton
*/
class ECCell : public SimpleComponent
{
public:
	ECCell( ICNDocument *icnDocument, bool newItem, const char *id = 0L );
	~ECCell();
	
	static Item* construct( ItemDocument *itemDocument, bool newItem, const char *id );
	static LibraryItem *libraryItem();
	
private:
	void dataChanged();
	virtual void drawShape( QPainter &p );
	VoltageSource *m_voltageSource;
	double voltage;
};

#endif
