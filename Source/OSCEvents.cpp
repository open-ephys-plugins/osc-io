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

#include "OSCEvents.h"
#include "OSCEventsEditor.h"

OSCEventsNode::OSCEventsNode()
    : GenericProcessor ("OSC Events")
{
    int port = DEFAULT_PORT;
    String address = DEFAULT_OSC_ADDRESS;

    while (oscModule == nullptr)
    {
        oscModule = std::make_unique<OSCModule> (port, address, this);

        if (! oscModule->isConnected)
        {
            LOGC ("Tyring new port:", port + 1);
            oscModule.reset (nullptr);
            port++;
        }
    }
}

void OSCEventsNode::registerParameters()
{
    addIntParameter (Parameter::PROCESSOR_SCOPE, "Port", "Port", "OSC Port Number", DEFAULT_PORT, 1024, 49151);
    addStringParameter (Parameter::PROCESSOR_SCOPE, "Address", "Address", "OSC Address", DEFAULT_OSC_ADDRESS);
    addIntParameter (Parameter::PROCESSOR_SCOPE, "Duration", "Duration", "TTL Pulse Duration (ms)", 100, 0, 2000);
    addBooleanParameter (Parameter::PROCESSOR_SCOPE, "StimOn", "Stim", "Determines whether events should be generated", true);

    if (oscModule)
        getParameter ("Port")->currentValue = oscModule->m_port;
}

AudioProcessorEditor* OSCEventsNode::createEditor()
{
    editor = std::make_unique<OSCEventsEditor> (this);
    return editor.get();
}

int OSCEventsNode::getPort() const
{
    if (oscModule)
        return oscModule->m_port;
    else
        return DEFAULT_PORT;
}

void OSCEventsNode::setPort (int port)
{
    String oscAddress = getOscAddress();

    if (getPort() != port)
    {
        oscModule.reset (nullptr);

        oscModule = std::make_unique<OSCModule> (port, oscAddress, this);

        if (! oscModule->isConnected)
        {
            oscModule.reset (nullptr);
            AlertWindow::showMessageBoxAsync (AlertWindow::AlertIconType::WarningIcon,
                                              "OSC Events [" + (String) getNodeId() + "]",
                                              "Unable to bind to port: " + (String) port
                                                  + "\nPlease try a different one!");
        }
    }
}

void OSCEventsNode::setOscAddress (String address)
{
    int port = getPort();

    if (! getOscAddress().equalsIgnoreCase (address))
    {
        oscModule.reset (nullptr);

        oscModule = std::make_unique<OSCModule> (port, address, this);
        if (! oscModule->isConnected)
        {
            oscModule.reset (nullptr);
            AlertWindow::showMessageBoxAsync (AlertWindow::AlertIconType::WarningIcon,
                                              "OSC Events [" + (String) getNodeId() + "]",
                                              "Unable to bind to port: " + (String) port
                                                  + "\nPlease try a different one!");
        }
    }
}

String OSCEventsNode::getOscAddress() const
{
    if (oscModule)
        return oscModule->m_address;
    else
        return DEFAULT_OSC_ADDRESS;
}

void OSCEventsNode::startStimulation()
{
    m_isOn = true;
}

void OSCEventsNode::stopStimulation()
{
    m_isOn = false;
}

int OSCEventsNode::getTTLDuration() const
{
    return m_pulseDurationMs;
}

void OSCEventsNode::setTTLDuration (int dur_ms)
{
    m_pulseDurationMs = dur_ms;
}

void OSCEventsNode::parameterValueChanged (Parameter* param)
{
    auto trackingEditor = (OSCEventsEditor*) getEditor();

    if (param->getName().equalsIgnoreCase ("Port"))
    {
        int port = static_cast<IntParameter*> (param)->getIntValue();
        setPort (port);
    }
    else if (param->getName().equalsIgnoreCase ("Address"))
    {
        String address = param->getValueAsString();
        setOscAddress (address);
    }
    else if (param->getName().equalsIgnoreCase ("Duration"))
    {
        int duration = static_cast<IntParameter*> (param)->getIntValue();
        setTTLDuration (duration);
    }
    else if (param->getName().equalsIgnoreCase ("StimOn"))
    {
        bool isOn = static_cast<BooleanParameter*> (param)->getBoolValue();
        if (isOn)
        {
            startStimulation();
        }
        else
        {
            stopStimulation();
        }
    }
}

void OSCEventsNode::updateSettings()
{
    settings.update (getDataStreams());

    for (auto stream : getDataStreams())
    {
        EventChannel* ttlChan;
        EventChannel::Settings ttlChanSettings {
            EventChannel::Type::TTL,
            "OSC Events stimulation output",
            "Triggers a TTL pulse whenever an incoming message is received",
            "osc.events",
            getDataStream (stream->getStreamId())
        };

        ttlChan = new EventChannel (ttlChanSettings);

        eventChannels.add (ttlChan);
        eventChannels.getLast()->addProcessor (this);
        settings[stream->getStreamId()]->eventChannelPtr = eventChannels.getLast();
    }
}

void OSCEventsNode::triggerEvent (int ttlLine, bool state)
{
    int streamIndex = 0;

    for (auto stream : getDataStreams())
    {
        int64 startSampleNum = getFirstSampleNumberForBlock (stream->getStreamId());
        int nSamples = getNumSamplesInBlock (stream->getStreamId());

        if (m_pulseDurationMs > 0)
            state = true; // all events are "ON" events if pulse duration is set

        // Create and Send ON event
        TTLEventPtr event = TTLEvent::createTTLEvent (eventChannels[streamIndex],
                                                      startSampleNum,
                                                      ttlLine,
                                                      state);

        LOGD ("Adding on event at ", startSampleNum);

        addEvent (event, 0);

        if (m_pulseDurationMs > 0)
        {
            // Create OFF event
            int eventDurationSamp = static_cast<int> (ceil (m_pulseDurationMs / 1000.0f * stream->getSampleRate()));

            TTLEventPtr eventOff = TTLEvent::createTTLEvent (settings[stream->getStreamId()]->eventChannelPtr,
                                                             startSampleNum + eventDurationSamp,
                                                             ttlLine,
                                                             false);

            // Add or schedule turning-off event
            // We don't care whether there are other turning-offs scheduled to occur either in
            // this buffer or later. The abilities to change event duration during acquisition and for
            // events to be longer than the timeout period create a lot of possibilities and edge cases,
            // but overwriting turnoffEvent unconditionally guarantees that this and all previously
            // turned-on events will be turned off by this "turning-off" if they're not already off.
            if (eventDurationSamp < nSamples)
            {
                addEvent (eventOff, eventDurationSamp);
            }

            else
            {
                LOGD ("Adding off event at ", eventOff->getSampleNumber());
                settings[stream->getStreamId()]->turnoffEvent = eventOff;
            }
        }

        streamIndex++;
    }
}

void OSCEventsNode::process (AudioBuffer<float>& buffer)
{
    if (! m_isOn || ! oscModule)
        return;

    // turn off event from previous buffer if necessary
    for (auto stream : getDataStreams())
    {
        auto settingsModule = settings[stream->getStreamId()];

        if (! settingsModule->turnoffEvent)
            continue;

        int startSampleNum = getFirstSampleNumberForBlock (stream->getStreamId());
        int nSamples = getNumSamplesInBlock (stream->getStreamId());
        int turnoffOffset = jmax (0, (int) (settingsModule->turnoffEvent->getSampleNumber() - startSampleNum));

        if (turnoffOffset < nSamples)
        {
            addEvent (settingsModule->turnoffEvent, turnoffOffset);
            settingsModule->turnoffEvent = nullptr;
        }
    }

    lock.enter();

    for (int i = 0; i < oscModule->m_messageQueue->count(); i++)
    {
        MessageData msg = oscModule->m_messageQueue->pop();

        LOGD ("Triggering event for message");

        triggerEvent (msg.ttlLine, msg.state);
    }

    lock.exit();
}

bool OSCEventsNode::startAcquisition()
{
    if (oscModule)
    {
        LOGC ("[OSC Events] Clearing message queue before starting acquisition")

        lock.enter();
        oscModule->m_messageQueue->clear();
        lock.exit();

        LOGD ("Message QUEUE SIZE: ", oscModule->m_messageQueue->count());
    }

    return true;
}

void OSCEventsNode::receiveMessage (const MessageData& message)
{
    lock.enter();

    if (CoreServices::getAcquisitionStatus())
        oscModule->m_messageQueue->push (message);

    lock.exit();
}

void MessageQueue::push (const MessageData& message)
{
    queue.add (message);
}

MessageData MessageQueue::pop()
{
    return queue.removeAndReturn (0);
}

bool MessageQueue::isEmpty()
{
    return queue.size() == 0;
}

void MessageQueue::clear()
{
    queue.clear();
}

int MessageQueue::count()
{
    return queue.size();
}

OSCModule::OSCModule (int port, String address, OSCEventsNode* processor)
    : m_port (port), m_address (address), m_processor (processor), isConnected (false)
{
    m_messageQueue = std::make_unique<MessageQueue>();

    if (! connect (port))
    {
        LOGE ("Failed to bind to port: ", port);
        return;
    }

    LOGC ("Creating OSC receiver - Port:", port, " Address:", address);

    isConnected = true;
    addListener (this, address);
}

void OSCModule::oscMessageReceived (const juce::OSCMessage& message)
{
    int ttlLine = -1;
    int state = true;

    if (message.size() == 2)
    {
        if (message[0].isInt32())
            ttlLine = message[0].getInt32();
        else if (message[0].isString())
            ttlLine = message[0].getString().getIntValue();

        if (message[1].isInt32())
            state = message[1].getInt32();
        else if (message[1].isString())
            state = message[1].getString().getIntValue();

        if (ttlLine >= 0 && ttlLine < 256
            && (state == 0 || state == 1))
        {
            MessageData messageData;

            messageData.ttlLine = ttlLine;
            messageData.state = bool (state);

            m_processor->receiveMessage (messageData);

            return;
        }
    }

    LOGE ("Invalid message received from OSC server");
}
