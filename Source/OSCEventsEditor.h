/*
------------------------------------------------------------------

This file is part of the Open Ephys GUI
Copyright (C) 2022 Open Ephys

------------------------------------------------------------------

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OSCEVENTSEDITOR_H
#define OSCEVENTSEDITOR_H

#define MAX_SOURCES 10

#include <VisualizerEditorHeaders.h>

class OSCEventsEditor : public GenericEditor,
						public Button::Listener
{
public:
	/** Constructor */
	OSCEventsEditor(GenericProcessor *parentNode);

	/** Destructor */
	~OSCEventsEditor() {}

	/** Button listener*/
	void buttonClicked(Button* button) override;

private:

	std::unique_ptr<TextButton> stimulationToggleButton;
	std::unique_ptr<Label> stimLabel;

	/** Called by processor when parameters change */
	//void updateCustomView();

	/** Generates an assertion if this class leaks */
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCEventsEditor);
};

#endif // TrackingNodeEDITOR_H_DEFINED