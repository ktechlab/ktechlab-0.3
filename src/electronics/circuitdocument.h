/***************************************************************************
 *   Copyright (C) 2003-2005 by David Saxton                               *
 *   david@bluehaze.org                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef CIRCUITDOCUMENT_H
#define CIRCUITDOCUMENT_H

#include <set>

#include "circuiticndocument.h"

class Circuit;
class Component;
class Element;
class KTechlab;
class Pin;
class QTimer;
class Switch;
class Wire;
class KActionMenu;

typedef std::set<Circuit*> CircuitList;
typedef QValueList<Component*> ComponentList;
typedef std::set<Element*> ElementList;
typedef std::set<Pin *> PinList;
typedef QValueList<Switch*> SwitchList;
typedef std::set<Wire *> WireList;

class Circuitoid {

public:
	bool addPin(Pin *node) {
		if(node) return pinList.insert(node).second;

		return false;
	}

	void addElement(Element *ele) {
		if(ele) elementList.insert(ele);
	}

	unsigned numElements() const { return elementList.size(); }

	ElementList::const_iterator getElementsEnd() const { return elementList.end(); }
	ElementList::const_iterator getElementsBegin() const { return elementList.begin(); }

	PinList::const_iterator getPinsEnd() const { return pinList.end(); }
	PinList::const_iterator getPinsBegin() const { return pinList.begin(); }

	PinList getPinList() { return pinList; }

private:

	PinList pinList;
	ElementList elementList;
};

/**
CircuitDocument handles allocation of the components displayed in the ICNDocument
to various Circuits, where the simulation can be performed, and displays the
information from those simulations back on the ICNDocument
@short Circuit view
@author David Saxton
*/

class CircuitDocument : public CircuitICNDocument {
	Q_OBJECT

public:
	CircuitDocument(const QString &caption, const char *name = 0L);
	~CircuitDocument();

	virtual View *createView(ViewContainer *viewContainer, uint viewAreaId, const char *name = 0l);

	void calculateConnectorCurrents();
	/**
	 * Count the number of ExternalConnection components in the CNItemList
	 */
	int countExtCon(const ItemList &cnItemList) const;

	virtual void update();

public slots:
	/**
	 * Creates a subcircuit from the currently selected components
	 */
	void createSubcircuit();
	void displayEquations();
	void setOrientation0();
	void setOrientation90();
	void setOrientation180();
	void setOrientation270();
	void rotateCounterClockwise();
	void rotateClockwise();
	void flipHorizontally();
	void flipVertically();
	/**
	 * Enables / disables / selects various actions depending on what is
	 * selected or not.
	 * @param plugContextMenu If true, then will insert actions into contextmenu
	 */
	virtual void slotInitItemActions();
	void requestAssignCircuits();
	void componentAdded(Item *item);
	void componentRemoved(Item *item);
	void connectorAdded(ElectronicConnector *connector);
	virtual void slotUpdateConfiguration();

protected:
	virtual void itemAdded(Item *item);
	virtual void fillContextMenu(const QPoint &pos);
	virtual bool isValidItem(Item *item);
	virtual bool isValidItem(const QString &itemId);

	KActionMenu *m_pOrientationAction;

private slots:
	void assignCircuits();

private:
	/**
	 * If the given circuitoid can be a LogicCircuit, then it will be added to
	 * m_logicCircuits, and return true. Else returns false.
	 */
	bool tryAsLogicCircuit(Circuitoid *circuitoid);
	/**
	 * Creates a circuit from the circuitoid
	 */
	Circuit *createCircuit(Circuitoid *circuitoid);

	/**
	 * @param node Current node (will be added, then tested for further
	 * connections).
	 * @param nodeList List of nodes in current partition.
	 * @param unassignedNodes The pool of all nodes in the CircuitDocument
	 * waiting for assignment.
	 * @param onlyGroundDependent if true, then the partition will not use
	 * circuit-dependent pins to include new pins while growing the
	 * partition.
	 */
	void getPartition(Pin *pin, PinList *pinList, PinList *unassignedPins, bool onlyGroundDependent = false);
	/**
	 * Takes the nodeList (generated by getPartition), splits it at ground nodes,
	 * and creates circuits from each split.
	 */
	void splitIntoCircuits(PinList *pinList);
	/**
	 * Construct a circuit from the given node, stopping at the groundnodes
	 */
	void recursivePinAdd(Pin *pin, Circuitoid *circuitoid, PinList *unassignedPins);

	void deleteCircuits();

	QTimer *m_updateCircuitsTmr;
	CircuitList m_circuitList;
	ComponentList m_toSimulateList;
	ComponentList m_componentList; // List is built up during call to assignCircuits

// hmm, we have one of these in circuit too....
	PinList m_pinList;
	WireList m_wireList;
	SwitchList m_switchList;
};

#endif

