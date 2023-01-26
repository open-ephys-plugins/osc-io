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

#ifndef OSCEVENTS_H
#define OSCEVENTS_H

#include <ProcessorHeaders.h>

#include <stdio.h>

#define DEFAULT_PORT 27020
#define DEFAULT_OSC_ADDRESS "/ttl"

#include "oscpack/osc/OscOutboundPacketStream.h"
#include "oscpack/ip/IpEndpointName.h"
#include "oscpack/osc/OscReceivedElements.h"
#include "oscpack/osc/OscPacketListener.h"
#include "oscpack/ip/UdpSocket.h"

struct MessageData {
	int ttlLine;
	bool state;
};


/** 
	Stores incoming messages in a queue
*/
class MessageQueue
{
public:

	/** Constructor */
	MessageQueue() { }

	/** Destructor */
	~MessageQueue() { }

	/** Adds a message to the queue */
	void push(const MessageData &message);

	/** Removes a message from the queue*/
	MessageData pop();

	/** True if the queue is empty*/
	bool isEmpty();

	/** Clears the queue*/
	void clear();

	/** Returns the number of messages available*/
	int count();

private:
	Array<MessageData> queue;
};

class OSCEventsNode;

/*
 
	An OSC UDP Server running its own thread

*/
class OSCServer : public osc::OscPacketListener,
			      public Thread
{
public:

	/** Constructor */
	OSCServer(int port, String address, OSCEventsNode* processor);

	/** Destructor*/
	~OSCServer();

	/** Run thread */
	void run();

	/** Stop listening */
	void stop();

	/** Check if server was bound successfully*/
	bool isBound();

protected:
	/** OscPacketListener method*/
	virtual void ProcessMessage(const osc::ReceivedMessage &m, const IpEndpointName &);

private:

	/** Copy constructor */
	OSCServer(OSCServer const &);

	int m_incomingPort;
	String m_oscAddress;

	std::unique_ptr<UdpListeningReceiveSocket> m_listeningSocket;
	OSCEventsNode* m_processor;
};

/** 
	
	Contains a message queue and an OSC server

*/
class OSCModule
{
public:
	
	/** Constructor */
	OSCModule(int port, String address, OSCEventsNode* processor)
		:m_port(port), m_address(address)
	{
		m_messageQueue = std::make_unique<MessageQueue>();
		m_server = std::make_unique<OSCServer>(port, address, processor);
		if(m_server->isBound())
			m_server->startThread();
	}

	/** Destructor */
	~OSCModule() {}

	friend std::ostream &operator<<(std::ostream &, const OSCModule&);

	int m_port = DEFAULT_PORT;
	String m_address = String(DEFAULT_OSC_ADDRESS);

	std::unique_ptr<MessageQueue> m_messageQueue;
	std::unique_ptr<OSCServer> m_server;

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCModule);
};

/** Holds settings for one stream's event channel */
class OSCEventsNodeSettings
{
public:
	/** Constructor -- sets default values*/
	OSCEventsNodeSettings() :
		eventChannelPtr(nullptr), turnoffEvent(nullptr) { }

	/** Destructor*/
	~OSCEventsNodeSettings() { }

	/** Parameters */
	EventChannel* eventChannelPtr;
	TTLEventPtr turnoffEvent; // holds a turnoff event that must be added in a later buffer
};


class OSCEventsNode : public GenericProcessor
{

public:
	/** The class constructor, used to initialize any members. */
	OSCEventsNode();

	/** The class destructor, used to deallocate memory */
	~OSCEventsNode() {}

	/** If the processor has a custom editor, this method must be defined to instantiate it. */
	AudioProcessorEditor *createEditor() override;

	/** Respond to parameter value changes */
	void parameterValueChanged(Parameter *param) override;

	/** Called every time the settings of an upstream plugin are changed.
		Allows the processor to handle variations in the channel configuration or any other parameter
		passed through signal chain. The processor can use this function to modify channel objects that
		will be passed to downstream plugins. */
	void updateSettings() override;

	/** Defines the functionality of the processor.
		The process method is called every time a new data buffer is available.
		Visualizer plugins typically use this method to send data to the canvas for display purposes */
	void process(AudioBuffer<float> &buffer) override;

	/** Saving custom settings to XML. This method is not needed to save the state of
		Parameter objects */
	void saveCustomParametersToXml(XmlElement *parentElement) override;

	/** Load custom settings from XML. This method is not needed to load the state of
		Parameter objects*/
	void loadCustomParametersFromXml(XmlElement *parentElement) override;

	bool startAcquisition() override;

	// receives a message from the osc server
	void receiveMessage(const MessageData &message);

	// Setter-Getters

	int getPort() const;
	void setPort (int port);

	String getOscAddress() const;
	void setOscAddress(String address);

	int getTTLDuration() const;
	void setTTLDuration(int duration_ms);

	/** Enables TTL output*/
	void startStimulation();

	/** Disables TTL output*/
    void stopStimulation();

private:

	CriticalSection lock;

	// Stimulation parameters
	bool m_isOn = true;
	int m_pulseDurationMs = 50;

	std::unique_ptr<OSCModule> oscModule;

	StreamSettings<OSCEventsNodeSettings> settings;

	/** Triggers an event on the specified TTL line*/
	void triggerEvent(int line, bool state);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OSCEventsNode);
};

#endif