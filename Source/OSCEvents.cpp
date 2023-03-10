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
    : GenericProcessor("OSC Events")
{

    addIntParameter(Parameter::GLOBAL_SCOPE, "Port", "OSC Port Number", DEFAULT_PORT, 1024, 49151);
    addIntParameter(Parameter::GLOBAL_SCOPE, "Duration", "TTL Pulse Duration (ms)", 50, 0, 5000);
    addStringParameter(Parameter::GLOBAL_SCOPE, "Address", "OSC Address", DEFAULT_OSC_ADDRESS);
    addBooleanParameter(Parameter::GLOBAL_SCOPE, "StimOn", "Determines whether events should be generated", true);

}

AudioProcessorEditor *OSCEventsNode::createEditor()
{
    editor = std::make_unique<OSCEventsEditor>(this);
    return editor.get();
}

int OSCEventsNode::getPort() const
{
    if(oscModule)
        return oscModule->m_port;
    else
        return DEFAULT_PORT;
}

void OSCEventsNode::setPort(int port)
{
    String oscAddress = getOscAddress();

    if(getPort() != port)
    {
        oscModule.reset(nullptr);
        
        oscModule = std::make_unique<OSCModule>(port, oscAddress, this);

        if(!oscModule->m_server->isBound())
        {
            oscModule.reset(nullptr);
            AlertWindow::showMessageBoxAsync(AlertWindow::AlertIconType::WarningIcon,
                                             "OSC Events [" + (String)getNodeId() + "]",
                                             "Unable to bind to port: " + (String)port
                                             + "\nPlease try a different one!");
        }
    }
}

void OSCEventsNode::setOscAddress (String address)
{
    int port = getPort();
    
    if(!getOscAddress().equalsIgnoreCase(address))
    {
        oscModule.reset(nullptr);
        
        oscModule = std::make_unique<OSCModule>(port, address, this);
        if(!oscModule->m_server->isBound())
        {
            oscModule.reset(nullptr);
            AlertWindow::showMessageBoxAsync(AlertWindow::AlertIconType::WarningIcon,
                                             "OSC Events [" + (String)getNodeId() + "]",
                                             "Unable to bind to port: " + (String)port
                                             + "\nPlease try a different one!");
        }
    }
}

String OSCEventsNode::getOscAddress() const
{
    if(oscModule)
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

void OSCEventsNode::setTTLDuration(int dur_ms)
{
    m_pulseDurationMs = dur_ms;
}


void OSCEventsNode::parameterValueChanged(Parameter *param)
{
    auto trackingEditor = (OSCEventsEditor*) getEditor();

    if (param->getName().equalsIgnoreCase("Port"))
    {
        int port = static_cast<IntParameter*>(param)->getIntValue();
        setPort(port);
    }
    else if(param->getName().equalsIgnoreCase("Address"))
    {
        String address = param->getValueAsString();
        setOscAddress( address);
    }
    else if (param->getName().equalsIgnoreCase("Duration"))
    {
        int duration = static_cast<IntParameter*>(param)->getIntValue();
        setTTLDuration(duration);
    }
    else if (param->getName().equalsIgnoreCase("StimOn"))
    {
        bool isOn = static_cast<BooleanParameter*>(param)->getBoolValue();
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

    settings.update(getDataStreams());

    for (auto stream : getDataStreams())
    {        
        EventChannel* ttlChan;
        EventChannel::Settings ttlChanSettings{
            EventChannel::Type::TTL,
            "OSC Events stimulation output",
            "Triggers a TTL pulse whenever an incoming message is received",
            "osc.events",
            getDataStream(stream->getStreamId())
        };

        ttlChan = new EventChannel(ttlChanSettings);

        eventChannels.add(ttlChan);
        eventChannels.getLast()->addProcessor(processorInfo.get());
        settings[stream->getStreamId()]->eventChannelPtr = eventChannels.getLast();
    }

    parameterValueChanged(getParameter("Duration"));
    parameterValueChanged(getParameter("StimOn"));

    int port = static_cast<IntParameter*>(getParameter("Port"))->getIntValue();
    String address = getParameter("Address")->getValueAsString();
    
    while(oscModule == nullptr)
    {
        oscModule = std::make_unique<OSCModule>(port, address, this);

        if(!oscModule->m_server->isBound())
        {
            LOGC("Tyring new port:", port + 1);
            oscModule.reset(nullptr);
            port++;
        }
    }

    getParameter("Port")->currentValue = oscModule->m_port;
    getEditor()->updateView();
}

void OSCEventsNode::triggerEvent(int ttlLine, bool state)
{   

    int streamIndex = 0;
    
    for (auto stream : getDataStreams())
    {     
        int64 startSampleNum = getFirstSampleNumberForBlock(stream->getStreamId());
        int nSamples = getNumSamplesInBlock(stream->getStreamId());

        if (m_pulseDurationMs > 0)
            state = true; // all events are "ON" events if pulse duration is set

        // Create and Send ON event
        TTLEventPtr event = TTLEvent::createTTLEvent(eventChannels[streamIndex],
                                                     startSampleNum,
                                                     ttlLine,
                                                     state);

        LOGD("Adding on event at ", startSampleNum);
        
        addEvent(event, 0);

        if (m_pulseDurationMs > 0)
        {
            // Create OFF event
            int eventDurationSamp = static_cast<int>(ceil(m_pulseDurationMs / 1000.0f * stream->getSampleRate()));

            TTLEventPtr eventOff = TTLEvent::createTTLEvent(settings[stream->getStreamId()]->eventChannelPtr,
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
                addEvent(eventOff, eventDurationSamp);
            }
                
            else
            {
                LOGD("Adding off event at ", eventOff->getSampleNumber());
                settings[stream->getStreamId()]->turnoffEvent = eventOff;
            }
                
        }

        streamIndex++;
    }
}

void OSCEventsNode::process(AudioBuffer<float>& buffer)
{

    if (!m_isOn || !oscModule)
        return;

    // turn off event from previous buffer if necessary
    for (auto stream : getDataStreams())
    {

        auto settingsModule = settings[stream->getStreamId()];

        if (!settingsModule->turnoffEvent)
            continue;
        
        int startSampleNum = getFirstSampleNumberForBlock(stream->getStreamId());
        int nSamples = getNumSamplesInBlock(stream->getStreamId());
        int turnoffOffset = jmax(0, (int)(settingsModule->turnoffEvent->getSampleNumber() - startSampleNum));

        if (turnoffOffset < nSamples)
        {
            addEvent(settingsModule->turnoffEvent, turnoffOffset);
            settingsModule->turnoffEvent = nullptr;
        }

    }

    lock.enter();

    for (int i = 0; i < oscModule->m_messageQueue->count(); i++)
    {
        MessageData msg = oscModule->m_messageQueue->pop();

        LOGD("Triggering event for message");
        
        triggerEvent(msg.ttlLine, msg.state);
    }

    lock.exit();
   
}

bool OSCEventsNode::startAcquisition()
{
    if(oscModule)
    {
        LOGC("[OSC Events] Clearing message queue before starting acquisition")

        lock.enter();
        oscModule->m_messageQueue->clear();
        lock.exit();

        LOGD("Message QUEUE SIZE: ", oscModule->m_messageQueue->count());
    }

    return true;
}

void OSCEventsNode::receiveMessage(const MessageData &message)
{

    lock.enter();

    LOGD("Pushing message to queue");

    if(CoreServices::getAcquisitionStatus())
        oscModule->m_messageQueue->push(message);

    LOGD("Message QUEUE SIZE: ", oscModule->m_messageQueue->count());
   
    lock.exit();
}


void MessageQueue::push(const MessageData &message)
{
    queue.add(message);
}

MessageData MessageQueue::pop()
{
    return queue.removeAndReturn(0);
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



OSCServer::OSCServer(int port, 
    String address, 
    OSCEventsNode *processor)
    : Thread("OscListener Thread"),
       m_incomingPort(port), 
       m_oscAddress(address),
       m_processor(processor)
{
    LOGC("Creating OSC server - Port:", port, " Address:", address);

    try
    {
        m_listeningSocket = std::make_unique<UdpListeningReceiveSocket>(
            IpEndpointName(IpEndpointName::ANY_ADDRESS, m_incomingPort),
            this);

        CoreServices::sendStatusMessage("OSC Server ready!");
        LOGC("OSC Server started!");
    }
    catch (const std::exception &e)
    {
        CoreServices::sendStatusMessage("OSC Server failed to start!");
        LOGE("Exception in creating OSC Server: ", String(e.what()));
    }

    // startThread();
}

OSCServer::~OSCServer()
{
    // stop the OSC Listener thread running
    stop();
    stopThread(-1);
    waitForThreadToExit(-1);
}

void OSCServer::ProcessMessage(const osc::ReceivedMessage& receivedMessage,
    const IpEndpointName&)
{

    LOGD("Message received on ", receivedMessage.AddressPattern());

    try
    {

		if (String(receivedMessage.AddressPattern()).equalsIgnoreCase(m_oscAddress))
		{
            LOGD("Num arguments: ", receivedMessage.ArgumentCount());

            osc::ReceivedMessageArgumentStream args = receivedMessage.ArgumentStream();

            int ttlLine = -1;
            int state = true;

            if (receivedMessage.ArgumentCount() > 0)
                args >> ttlLine;

            if (receivedMessage.ArgumentCount() > 1)
                args >> state;

            LOGD("TTL Line: ", ttlLine);
            LOGD("TTL State: ", state);

            if (ttlLine >= 0)
            {
                MessageData messageData;

                messageData.ttlLine = ttlLine;
                messageData.state = bool(state);

                m_processor->receiveMessage(messageData);
            }
		}
        
    }
    catch (osc::Exception &e)
    {
        // any parsing errors such as unexpected argument types, or
        // missing arguments get thrown as exceptions.
        LOGE("error while parsing message: ", String(receivedMessage.AddressPattern()), ": ", String(e.what()));
    }
}

void OSCServer::run()
{    
    // Start the oscpack OSC Listener Thread
    // TODO (FIX): Hits assertion in the JUCE::Thread class bec6ause listener's
    // 'Run()' method is throwing expection in some cases.
    if(m_listeningSocket)
            m_listeningSocket->Run();
}

bool OSCServer::isBound()
{
    if(m_listeningSocket)
        return m_listeningSocket->IsBound();
    else
        return false;
}

void OSCServer::stop()
{
    // Stop the oscpack OSC Listener Thread
    if (!isThreadRunning())
    {
        return;
    }

    if(m_listeningSocket)
        m_listeningSocket->AsynchronousBreak();
}

