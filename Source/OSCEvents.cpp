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
    
    addStringParameter(Parameter::GLOBAL_SCOPE, "IP", "IP Address", DEFAULT_IP_ADDRESS);
    addIntParameter(Parameter::GLOBAL_SCOPE, "Port", "OSC Port Number", DEFAULT_PORT, 1024, 49151);
    addIntParameter(Parameter::GLOBAL_SCOPE, "Duration_ms", "TTL Pulse Duration", 0, 0, 5000);
    addStringParameter(Parameter::GLOBAL_SCOPE, "Address", "OSC Address", DEFAULT_OSC_ADDRESS);
    addBooleanParameter(Parameter::GLOBAL_SCOPE, "StimOn", "Determines whether events should be generated", true);

    oscModule = std::make_unique<OSCModule>(DEFAULT_IP_ADDRESS, DEFAULT_PORT, DEFAULT_OSC_ADDRESS, this);

}

AudioProcessorEditor *OSCEventsNode::createEditor()
{
    editor = std::make_unique<OSCEventsEditor>(this);
    return editor.get();
}

int OSCEventsNode::getPort() const
{
    return oscModule->m_port;
}

void OSCEventsNode::setPort(int port)
{
    String oscAddress = getOscAddress();
    String ipAddress = getIpAddress();

    oscModule.reset();
    oscModule = std::make_unique<OSCModule>(ipAddress, port, oscAddress, this);
}

String OSCEventsNode::getIpAddress() const
{
    return oscModule->m_ipAddress;
}

void OSCEventsNode::setIpAddress(String address)
{
    String oscAddress = getOscAddress();
    int port = getPort();

    oscModule.reset();
    oscModule = std::make_unique<OSCModule>(address, port, oscAddress, this);
}

void OSCEventsNode::setOscAddress (String address)
{
    String ipAddress = getIpAddress();
    int port = getPort();
    
    oscModule.reset();
    oscModule = std::make_unique<OSCModule>(ipAddress, port, address, this);
}

String OSCEventsNode::getOscAddress() const
{
    return oscModule->m_address;
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
    else if(param->getName().equalsIgnoreCase("IP"))
    {
        String address = param->getValueAsString();
        setIpAddress( address);
    }
    else if(param->getName().equalsIgnoreCase("Address"))
    {
        String address = param->getValueAsString();
        setOscAddress( address);
    }
    else if (param->getName().equalsIgnoreCase("Duration_ms"))
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

        std::cout << "Adding on event at " << startSampleNum << std::endl;
        
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
                std::cout << "Adding off event at " << eventOff->getSampleNumber() << std::endl;
                settings[stream->getStreamId()]->turnoffEvent = eventOff;
            }
                
        }

        streamIndex++;
    }
}

void OSCEventsNode::process(AudioBuffer<float>& buffer)
{

    if (!m_isOn)
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

        std::cout << "Triggering event for message" << std::endl;
        
        triggerEvent(msg.ttlLine, msg.state);
    }

    lock.exit();
   
}

void OSCEventsNode::receiveMessage(const MessageData &message)
{

    lock.enter();

    std::cout << "Pushing message to queue" << std::endl;

    oscModule->m_messageQueue->push(message);
   
    lock.exit();
}


// TODO: Both I/O methods need finishing
void OSCEventsNode::saveCustomParametersToXml(XmlElement *parentElement)
{
    // for (auto stream : getDataStreams())
    // {
    //     auto *moduleXml = parentElement->createNewChildElement("Tracking_Node");
    //     OSCEventsNodeSettings *module = settings[stream->getStreamId()];
    //     for (auto tracker : module->trackers) {
    //         moduleXml->setAttribute("Name", tracker->m_name);
    //         moduleXml->setAttribute("Port", tracker->m_port);
    //         moduleXml->setAttribute("Address", tracker->m_address);
    //     }
    // }
}

void OSCEventsNode::loadCustomParametersFromXml(XmlElement *xml)
{
    // for (auto *moduleXml : xml->getChildIterator())
    // {
    //     if (moduleXml->hasTagName("Tracking_Node"))
    //     {
    //         String name = moduleXml->getStringAttribute("Name", "Tracking source 1");
    //         String address = moduleXml->getStringAttribute("Address", "/red");
    //         String port = moduleXml->getStringAttribute("Port", "27020");

    //         addTracker(name, port, address);
    //     }
    // }
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



OSCServer::OSCServer(String ipAddress, 
    int port, 
    String address, 
    OSCEventsNode *processor)
    : Thread("OscListener Thread"),
       m_ipAddress(ipAddress),
       m_incomingPort(port), 
       m_oscAddress(address),
       m_processor(processor)
{
    m_listeningSocket = new UdpListeningReceiveSocket(
        IpEndpointName(m_ipAddress.getCharPointer(), m_incomingPort),
        this);

    startThread();
}

OSCServer::~OSCServer()
{
    // stop the OSC Listener thread running
    stop();
    stopThread(-1);
    waitForThreadToExit(-1);
    delete m_listeningSocket;
}

void OSCServer::ProcessMessage(const osc::ReceivedMessage& receivedMessage,
    const IpEndpointName&)
{

    std::cout << "Message received on " << receivedMessage.AddressPattern() << std::endl;

    try
    {

		if (String(receivedMessage.AddressPattern()).equalsIgnoreCase(m_oscAddress))
		{
            std::cout << "Num arguments: " << receivedMessage.ArgumentCount() << std::endl;

            osc::ReceivedMessageArgumentStream args = receivedMessage.ArgumentStream();

            int ttlLine = -1;
            int state = true;

            if (receivedMessage.ArgumentCount() > 0)
                args >> ttlLine;

            if (receivedMessage.ArgumentCount() > 1)
                args >> state;

            std::cout << "TTL Line: " << ttlLine << std::endl;
            std::cout << "TTL State: " << state << std::endl;

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
        LOGC("error while parsing message: ", String(receivedMessage.AddressPattern()), ": ", String(e.what()));
    }
}

void OSCServer::run()
{
    sleep(100);
    
    // Start the oscpack OSC Listener Thread
    try
    {
        m_listeningSocket->Run();
        CoreServices::sendStatusMessage("OSC Server running");
		LOGC("OSC Server running");
    }
    catch (const std::exception &e)
    {
        LOGC("Exception in OSCServer::run(): ", String(e.what()));
    }
}

void OSCServer::stop()
{
    // Stop the oscpack OSC Listener Thread
    if (!isThreadRunning())
    {
        return;
    }

    m_listeningSocket->AsynchronousBreak();
}
