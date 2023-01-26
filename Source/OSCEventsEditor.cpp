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
#include <vector>
#include "OSCEventsEditor.h"
#include "OSCEvents.h"

OSCEventsEditor::OSCEventsEditor(GenericProcessor *parentNode)
    : GenericEditor(parentNode)
{
    desiredWidth = 250;

    ipLabel = std::make_unique<Label>("IP Label", "IP");
    ipLabel->setFont(Font("Silkscreen", "Regular", 12.0f));
    ipLabel->setColour(Label::textColourId, Colours::darkgrey);
    ipLabel->setBounds(15, 25, 40, 20);
    addAndMakeVisible(ipLabel.get());

    ipAddrLabel = std::make_unique<TextEditor>("IP Address");
    ipAddrLabel->setText(IPAddress::getLocalAddress().toString(), false);
    ipAddrLabel->setReadOnly(true);
    ipAddrLabel->setCaretVisible(false);
    ipAddrLabel->setTooltip("This machine's assigned address. Use this in your OSC sender client.");
    ipAddrLabel->applyFontToAllText(Font("CP Mono", "Plain", 15.0f));
    ipAddrLabel->applyColourToAllText(Colours::lightgrey);
    ipAddrLabel->setColour(TextEditor::backgroundColourId, Colours::grey);
    ipAddrLabel->setBounds(15, 45, 115, 18);
    addAndMakeVisible(ipAddrLabel.get());

    addTextBoxParameterEditor("Port", 145, 25);
    addTextBoxParameterEditor("Address", 15, 75);
    addTextBoxParameterEditor("Duration", 105, 75);
    
     // Stimulate (toggle)
    stimLabel = std::make_unique<Label>("Stim Label", "STIM");
    stimLabel->setFont(Font("Silkscreen", "Bold", 12.0f));
    stimLabel->setColour(Label::textColourId, Colours::darkgrey);
    stimLabel->setBounds(200, 75, 40, 20);
    addAndMakeVisible(stimLabel.get());

    stimulationToggleButton = std::make_unique<TextButton>("Stimulate Button");
    stimulationToggleButton->setBounds(200, 95, 40, 18);
    stimulationToggleButton->addListener(this);
    stimulationToggleButton->setClickingTogglesState(true); // makes the button toggle its state when clicked
    stimulationToggleButton->setButtonText("ON");
    stimulationToggleButton->setColour(TextButton::buttonOnColourId, Colours::yellow);
    stimulationToggleButton->setToggleState(true, dontSendNotification);
    addAndMakeVisible(stimulationToggleButton.get()); // makes the button a child component of the editor and makes it visible
}


void OSCEventsEditor::buttonClicked(Button *btn)
{
    OSCEventsNode *processor = (OSCEventsNode *) getProcessor();

    if (btn == stimulationToggleButton.get())
    {
        if (btn->getToggleState()==true)
        {
            processor->getParameter("StimOn")->setNextValue(true);
            btn->setButtonText(String("ON"));
        }
        else
        {
            processor->getParameter("StimOn")->setNextValue(false);
            btn->setButtonText(String("OFF"));
        }
    }
}

/*void OSCEventsEditor::updateCustomView()
{
    OSCEventsNode *processor = (OSCEventsNode *)getProcessor();

    auto portParam = processor->getParameter("Port");
    int port = processor->getPort();

    if(port == 0)
        port = DEFAULT_PORT;

    portParam->currentValue = port;

    auto addressParam = processor->getParameter("Address");
    String addr = processor->getOscAddress();

    if(addr.isEmpty())
        addr = DEFAULT_OSC_ADDRESS;

    addressParam->currentValue = addr;

    auto ipAddressParam = processor->getParameter("Address");
    addr = processor->getIpAddress();

    if (addr.isEmpty())
        addr = DEFAULT_OSC_ADDRESS;

    ipAddressParam->currentValue = addr;

    updateView();
}*/