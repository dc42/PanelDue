/*
 * PanelDue.hpp
 *
 * Created: 06/12/2014 14:23:38
 *  Author: David
 */ 

#ifndef PANELDUE_H_
#define PANELDUE_H_

#include "UTFT.hpp"
#include "Display.hpp"

// Global functions in PanelDue.cpp that are called from elsewhere
extern void processReceivedValue(const char id[], const char val[], int index);

// Global data in PanelDue.cpp that is used elsewhere
extern UTFT lcd;
extern DisplayManager mgr;

#endif /* PANELDUE_H_ */