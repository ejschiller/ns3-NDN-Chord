/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Jaume Nin <jaume.nin@cttc.cat>
 */

#include "ns3/lte-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/config-store.h"
#include "ns3/lte-rrc-protocol-real.h"
//#include "ns3/gtk-config-store.h"
#include <vector>
#include <cstdio>
#include <iostream>

using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates several eNodeB,
 * attaches one UE per eNodeB starts a flow for each UE to  and from a remote host.
 * It also  starts yet another flow between each UE pair.
 */

NS_LOG_COMPONENT_DEFINE ("EpcFirstExample");

int
main (int argc, char *argv[])
{

  uint16_t numberOfNetworks = 2;
  uint16_t numberOfNodes = 2;
  double simTime = 1.1;
  double distance = 60.0;
  double interPacketInterval = 100;
  bool useCa = false;

  // Command line arguments
  CommandLine cmd;
  cmd.AddValue("numberOfNetworks", "Number of Networks", numberOfNetworks);
  cmd.AddValue("numberOfNodes", "Number of eNodeBs + UE pairs", numberOfNodes);
  cmd.AddValue("simTime", "Total duration of the simulation [s])", simTime);
  cmd.AddValue("distance", "Distance between eNBs [m]", distance);
  cmd.AddValue("interPacketInterval", "Inter packet interval [ms])", interPacketInterval);
  cmd.AddValue("useCa", "Whether to use carrier aggregation.", useCa);
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::LteHelper::UseIdealRrc", BooleanValue(false));

  if (useCa)
   {
     Config::SetDefault ("ns3::LteHelper::UseCa", BooleanValue (useCa));
     Config::SetDefault ("ns3::LteHelper::NumberOfComponentCarriers", UintegerValue (2));
     Config::SetDefault ("ns3::LteHelper::EnbComponentCarrierManager", StringValue ("ns3::RrComponentCarrierManager"));
   }

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  std::vector<Ptr<LteHelper>> IndependentNetworks(numberOfNetworks);
  std::vector<Ptr<PointToPointEpcHelper>> IndependentEpcs(numberOfNetworks);
  std::vector<Ptr<Node>> IndependentPgws(numberOfNetworks);
  std::vector<PointToPointHelper> IndependentLinks(numberOfNetworks);
  std::vector<Ipv4AddressHelper> IndependentAddress(numberOfNetworks);
  std::vector<Ipv4InterfaceContainer> IndependentInterface(numberOfNetworks);
  std::vector<Ipv4Address> IndependentremoteHostAddr(numberOfNetworks);
  std::vector<Ipv4StaticRoutingHelper> Independentipv4RoutingHelper(numberOfNetworks);
  std::vector<NodeContainer> IndependentueNodes(numberOfNetworks);
  std::vector<NodeContainer> IndependentenbNodes(numberOfNetworks);
  std::vector<Ptr<ListPositionAllocator>> IndependentPositionAllocators(numberOfNetworks);
  std::vector<MobilityHelper> IndependentMobilityHelpers(numberOfNetworks); 
  std::vector<NetDeviceContainer> IndependentueDevices(numberOfNetworks);
  std::vector<NetDeviceContainer> IndependentenbDevices(numberOfNetworks);
  std::vector<Ipv4InterfaceContainer> IndependentueIpIfaces(numberOfNetworks);
 
  for(uint16_t nn = 0; nn < numberOfNetworks; nn++) {

	Ptr<LteHelper>& lteHelper = IndependentNetworks[nn];
	lteHelper = CreateObject<LteHelper> ();

        Ptr<PointToPointEpcHelper>& epcHelper = IndependentEpcs[nn];

	// s1-u interface on 2.(nn).0.0, mask defined in src/lte/helper/point-to-point-epc-helper.cc
	std::ostringstream s1ubaseaddress;
	s1ubaseaddress << "2." << nn << ".0.0";

	// x2 interface on 3.(nn).0.0, mask defined in src/lte/helper/point-to-point-epc-helper.cc
	std::ostringstream x2baseaddress;
	x2baseaddress << "3." << nn << ".0.0";

	// ue base interface 4.(nn).0.0, mask defined in src/lte/helper/point-to-point-epc-helper.cc
	std::ostringstream uebaseaddress;
	uebaseaddress << "4." << nn << ".0.0";

	epcHelper = CreateObject<PointToPointEpcHelper> (s1ubaseaddress.str().c_str(), x2baseaddress.str().c_str(), uebaseaddress.str().c_str());
        lteHelper->SetEpcHelper (epcHelper);

	Ptr<Node>& pgw = IndependentPgws[nn];
	pgw = epcHelper->GetPgwNode ();

	PointToPointHelper& p2ph = IndependentLinks[nn];

	p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
	p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  	p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
  	NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);

  	std::ostringstream addressing;
  	Ipv4AddressHelper& ipv4h = IndependentAddress[nn];

	// main interface on 1.(nn).0.0/16
	addressing << "1." << nn << ".0.0";

  	ipv4h.SetBase (addressing.str().c_str(), "255.255.0.0");
	Ipv4InterfaceContainer& internetIpIfaces = IndependentInterface[nn];
  	internetIpIfaces = ipv4h.Assign (internetDevices);
	Ipv4Address& remoteHostAddr = IndependentremoteHostAddr[nn];

  	// interface 0 is localhost, nn is the p2p device
  	remoteHostAddr = internetIpIfaces.GetAddress (nn);
	Ipv4StaticRoutingHelper& ipv4RoutingHelper = Independentipv4RoutingHelper[numberOfNetworks];
	Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  	remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address (addressing.str().c_str()), Ipv4Mask ("255.0.0.0"), nn);

	NodeContainer& ueNodes = IndependentueNodes[nn];
  	NodeContainer& enbNodes = IndependentenbNodes[nn];

  	enbNodes.Create(numberOfNodes);
  	ueNodes.Create(numberOfNodes);

	Ptr<ListPositionAllocator>& positionAlloc = IndependentPositionAllocators[nn];
	positionAlloc = CreateObject<ListPositionAllocator> ();

	for (uint16_t i = 0; i < numberOfNodes; i++) {
      		positionAlloc->Add (Vector(distance * i, 0, 0));
    	}

	MobilityHelper& mobility = IndependentMobilityHelpers[nn];

  	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  	mobility.SetPositionAllocator(positionAlloc);
	mobility.Install(enbNodes);
	mobility.Install(ueNodes);

	// Install Mobility Model
	// Install LTE Devices to the nodes

	NetDeviceContainer& enbLteDevs = IndependentenbDevices[nn];
	enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);

	
	NetDeviceContainer& ueLteDevs = IndependentueDevices[nn];
	ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

	// Install the IP stack on the UEs
	internet.Install (ueNodes);

	// Assign IP address to UEs, and install applications
	Ipv4InterfaceContainer& ueIpIface = IndependentueIpIfaces[nn];
	ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));

	// default route on UEs
	for (uint32_t u = 0; u < ueNodes.GetN (); ++u) {
		Ptr<Node> ueNode = ueNodes.Get (u);
		// Set the default gateway for the UE
		Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
		ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
	}


        for (uint16_t u = 0; u < numberOfNodes; u++) {
		Ptr<LteUeRrcProtocolReal> rrc_protocol;
		Ptr<NetDevice> ueLteDevice = ueLteDevs.Get(u);
		rrc_protocol = ueLteDevice->GetObject<LteUeNetDevice>()->GetRrc()->GetObject<LteUeRrcProtocolReal> ();

		if(rrc_protocol) 
			rrc_protocol->SetenbNodes(&enbNodes);
	
		else 
			NS_LOG (ns3::LOG_WARN, "RRC PROTOCOL EMPTY!");
        }
	

	// Attach one UE per eNodeB
  	for (uint16_t i = 0; i < numberOfNodes; i++) {
        	lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(i));
        }

  }

/*

  uint16_t otherPort = 3000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;

  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      ++ulPort;
      ++otherPort;
      PacketSinkHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
      PacketSinkHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
      PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), otherPort));
      serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get(u)));
      serverApps.Add (ulPacketSinkHelper.Install (remoteHost));
      serverApps.Add (packetSinkHelper.Install (ueNodes.Get(u)));

      UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
      dlClient.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
      dlClient.SetAttribute ("MaxPackets", UintegerValue(1000000));

      UdpClientHelper ulClient (remoteHostAddr, ulPort);
      ulClient.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
      ulClient.SetAttribute ("MaxPackets", UintegerValue(1000000));

      UdpClientHelper client (ueIpIface.GetAddress (u), otherPort);
      client.SetAttribute ("Interval", TimeValue (MilliSeconds(interPacketInterval)));
      client.SetAttribute ("MaxPackets", UintegerValue(1000000));

      clientApps.Add (dlClient.Install (remoteHost));
      clientApps.Add (ulClient.Install (ueNodes.Get(u)));
      if (u+1 < ueNodes.GetN ())
        {
          clientApps.Add (client.Install (ueNodes.Get(u+1)));
        }
      else
        {
          clientApps.Add (client.Install (ueNodes.Get(0)));
        }
    }

*/

  //serverApps.Start (Seconds (0.01));
  //clientApps.Start (Seconds (0.01));


  for(uint16_t nn = 0; nn < numberOfNetworks; nn++) {
	Ptr<LteHelper>& lteHelper = IndependentNetworks[nn];
	lteHelper->EnableTraces ();
  }

  // Uncomment to enable PCAP tracing
  //p2ph.EnablePcapAll("lena-epc-first");

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();

  /*GtkConfigStore config;
  config.ConfigureAttributes();*/

  Simulator::Destroy();
  return 0;

}

