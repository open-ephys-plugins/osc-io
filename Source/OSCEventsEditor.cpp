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
#include "OSCEventsEditor.h"
#include "OSCEvents.h"
#include <vector>

OSCEventsEditor::OSCEventsEditor (GenericProcessor* parentNode)
    : GenericEditor (parentNode)
{
    desiredWidth = 160;

    ipLabel = std::make_unique<Label> ("IP Label");
    ipLabel->setText ("IP: " + IPAddress::getLocalAddress().toString(), dontSendNotification);
    ipLabel->setFont (FontOptions ("Inter", "Regular", 14.0f));
    ipLabel->setBounds (15, 26, 130, 14);
    addAndMakeVisible (ipLabel.get());

    addTextBoxParameterEditor (Parameter::PROCESSOR_SCOPE, "Port", 15, 42);
    addTextBoxParameterEditor (Parameter::PROCESSOR_SCOPE, "Address", 15, 64);
    addBoundedValueParameterEditor (Parameter::PROCESSOR_SCOPE, "Duration", 15, 86);
    addToggleParameterEditor (Parameter::PROCESSOR_SCOPE, "StimOn", 15, 108);

    for (auto ed : parameterEditors)
    {
        ed->setSize (ed->getWidth(), 17);
    }
}
