/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

// Network topology
//
//       n0    n1   n2   n3                     nx
//       |     |    |    |  . . . . . . . . . . |
//       ========================================
//                          LAN
//

#include <fstream>
#include <stdlib.h>
#include <stdint.h>
#include <iostream>
#include <string.h>
#include <vector>
#include <openssl/sha.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/stats-module.h"
#include "ns3/csma-module.h"
#include "ns3/chord-ipv4-helper.h"
#include "ns3/chord-ipv4.h"
#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/internet-apps-module.h"
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ChordRun");

struct CommandHandlerArgument
{
  std::string scriptFile;
  NodeContainer nodeContainer;
  void *chordRun;
};

class ChordRun
{

 public:

    void Start (std::string scriptFile, NodeContainer nodeContainer);
    void Stop ();

    //Chord
    void InsertVNode(Ptr<ChordIpv4> chordApplication, std::string vNodeName);
    void Lookup (Ptr<ChordIpv4> chordApplication, std::string resourceName);

    //DHash
    void Insert (Ptr<ChordIpv4> chordApplication, std::string resourceName, std::string resourceValue);
    void Retrieve (Ptr<ChordIpv4> chordApplication, std::string resourceName);

    //Crash Testing
    void DetachNode(uint16_t nodeNumber);
    void ReAttachNode(uint16_t nodeNumber);
    void CrashChord(Ptr<ChordIpv4> chordApplication);
    void RestartChord(Ptr<ChordIpv4> chordApplication);

    // Call backs by Chord Layer
    void JoinSuccess (std::string vNodeName, uint8_t* key, uint8_t numBytes);
    void LookupSuccess (uint8_t* lookupKey, uint8_t lookupKeyBytes, Ipv4Address ipAddress, uint16_t port);
    void LookupFailure (uint8_t* lookupKey, uint8_t lookupKeyBytes);
    void InsertSuccess (uint8_t* key, uint8_t numBytes, uint8_t* object, uint32_t objectBytes);
    void RetrieveSuccess (uint8_t* key, uint8_t numBytes, uint8_t* object, uint32_t objectBytes);
    void InsertFailure (uint8_t* key, uint8_t numBytes, uint8_t* object, uint32_t objectBytes);
    void RetrieveFailure (uint8_t* key, uint8_t numBytes);
    void VNodeKeyOwnership (std::string vNodeName, uint8_t* key, uint8_t keyBytes, uint8_t* predecessorKey, uint8_t predecessorKeyBytes
			   ,uint8_t* oldPredecessorKey, uint8_t oldPredecessorKeyBytes, Ipv4Address predecessorIp, uint16_t predecessorPort);


    //Statistics
    void TraceRing (std::string vNodeName, uint8_t* key, uint8_t numBytes);
    void VNodeFailure (std::string vNodeName, uint8_t* key, uint8_t numBytes);
    void DumpVNodeInfo ( Ptr<ChordIpv4> chordApplication, std::string vNodeName);
    void DumpDHashInfo (Ptr<ChordIpv4> chordApplication);

    //Keyboard Handlers
    static void *CommandHandler (void *arg);
    void Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters);
    void ProcessCommandTokens (std::vector<std::string> tokens, Time time);
    void ReadCommandTokens (void);


    pthread_t commandHandlerThreadId;
    struct CommandHandlerArgument th_argument;

  private:
    ChordRun* m_chordRun;
    std::string m_scriptFile;
    NodeContainer m_nodeContainer;
    std::vector<std::string> m_tokens;
    bool m_readyToRead;
    
    //Print
    void PrintCharArray (uint8_t*, uint32_t, std::ostream&);
    void PrintHexArray (uint8_t*, uint32_t, std::ostream&);

};

 void
 ChordRun::Start (std::string scriptFile, NodeContainer nodeContainer)
 {

  NS_LOG_FUNCTION_NOARGS();
  th_argument.scriptFile = scriptFile;
  th_argument.nodeContainer  = nodeContainer;
  th_argument.chordRun = (void *)this;
  this->m_chordRun = this;
  this->m_nodeContainer = nodeContainer;

  m_readyToRead = false;
  //process script-file
  if (scriptFile != "")                                 //Start reading the script file.....if not null
  {
    std::ifstream file;
    file.open (scriptFile.c_str());
    if (file.is_open())
    {
      NS_LOG_INFO ("Reading Script File: " << scriptFile);
      Time time = MilliSeconds (0.0);
      std::string commandLine;
      while (!file.eof())
      {
        std::getline (file, commandLine, '\n');
        std::cout << "Adding Command: " << commandLine << std::endl;
        m_chordRun->Tokenize (commandLine, m_chordRun -> m_tokens, " ");
        if (m_chordRun -> m_tokens.size() == 0)
        {
          NS_LOG_INFO ("Failed to Tokenize");
          continue;
        }
        //check for time command
        std::vector<std::string>::iterator iterator = m_chordRun -> m_tokens.begin();
        if (*iterator == "Time")
        {
          if (m_chordRun -> m_tokens.size() < 2)
          {
            continue;
          }
          iterator++;
          std::istringstream sin (*iterator);
          uint64_t delta;
          sin >> delta;
          time = MilliSeconds( time.GetMilliSeconds() + delta);
          std::cout << "Time Pointer: " << time.GetMilliSeconds() << std::endl;
          m_chordRun -> m_tokens.clear();
          continue;
        }
        NS_LOG_INFO ("Processing...");
        m_chordRun->ProcessCommandTokens (m_chordRun -> m_tokens, MilliSeconds(time.GetMilliSeconds()));
        m_chordRun -> m_tokens.clear();
      }
    }
  }

    Simulator::Schedule (MilliSeconds (200), &ChordRun::ReadCommandTokens, this);

   if (pthread_create (&commandHandlerThreadId, NULL, ChordRun::CommandHandler, &th_argument) != 0)
   {
     perror ("New Thread Creation Failed, Exiting...");
     exit (1);
   }
 }

void
ChordRun::Stop ()
{

  NS_LOG_FUNCTION_NOARGS();
  //Cancel keyboard thread
  pthread_cancel (commandHandlerThreadId);
  //Join keyboard thread
  pthread_join (commandHandlerThreadId, NULL);
}


void*
ChordRun::CommandHandler (void *arg)
{
  struct CommandHandlerArgument th_argument = *((struct CommandHandlerArgument *) arg);
  std::string scriptFile = th_argument.scriptFile;
  NodeContainer nodeContainer = th_argument.nodeContainer;
  ChordRun* chordRun = (ChordRun *)th_argument.chordRun;

  chordRun -> m_chordRun = chordRun;
  chordRun -> m_nodeContainer = nodeContainer;
  chordRun -> m_scriptFile = scriptFile;

  while (1)
  {
    std::string commandLine;
    //read command from keyboard
    std::cout << "\nCommand > ";
    std::getline(std::cin, commandLine, '\n');
    if (chordRun->m_readyToRead == true)
    {
      std::cout << "Simulator busy, please try again..\n";
      continue;
    }

    chordRun->Tokenize (commandLine, chordRun -> m_tokens, " ");

    std::vector<std::string>::iterator iterator = chordRun -> m_tokens.begin();

    if (chordRun -> m_tokens.size() == 0)
    {
      continue;
    }
    //check for quit
    else if (*iterator == "quit")
    {
      break;
    }
    chordRun -> m_readyToRead = true;

    //SINGLE THREADED SIMULATOR WILL CRASH, so let simulator schedule processcommandtokens!
    //chordRun->ProcessCommandTokens (tokens, MilliSeconds (0.));

  }
  Simulator::Stop ();
  pthread_exit (NULL);
}

void
ChordRun::ReadCommandTokens (void)
{
  if (m_readyToRead == true)
  {

    if (m_tokens.size() > 0)
    {
      m_chordRun->ProcessCommandTokens (m_tokens, MilliSeconds (0.0));
    }
    m_tokens.clear();
    m_readyToRead = false;
  }
  Simulator::Schedule (MilliSeconds (200), &ChordRun::ReadCommandTokens, this);

}

void
ChordRun::ProcessCommandTokens (std::vector<std::string> tokens, Time time)
{
  NS_LOG_INFO ("Processing Command Token...");
  //Process tokens
  std::vector<std::string>::iterator iterator = tokens.begin();

  std::istringstream sin (*iterator);
  uint16_t nodeNumber;
  sin >> nodeNumber;
std::cout<<nodeNumber;
  //this command can be in script file
  if (*iterator == "quit")
  {
    NS_LOG_INFO ("Scheduling Command quit...");
    Simulator::Stop (MilliSeconds(time.GetMilliSeconds()));
    return;
  }
  else if (tokens.size() < 2)
  {
        //std::cout<<"stop  ";
    return;
  }
  Ptr<ChordIpv4> chordApplication = m_nodeContainer.Get(nodeNumber)->GetApplication(0)->GetObject<ChordIpv4> ();

  iterator++;
  if (*iterator == "InsertVNode")
  {
std::cout<<tokens.size()<<std::endl;
    if (tokens.size() < 3)
    { 
std::cout<<"stop  ";
      return;
    }
    //extract node name
    iterator++;
    std::string vNodeName = std::string(*iterator);
    NS_LOG_INFO ("Scheduling Command InsertVNode...");
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordRun::InsertVNode, this, chordApplication, vNodeName);
    return;
  }
  else if (*iterator == "DumpVNodeInfo")
  {
    if (tokens.size() < 3)
    { 
      return;
    }
    //extract node name
    iterator++;
    std::string vNodeName = std::string(*iterator);
    NS_LOG_INFO ("Scheduling Command DumpVNodeInfo...");
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordRun::DumpVNodeInfo, this,chordApplication,vNodeName);
  }

  else if (*iterator == "DumpDHashInfo")
  {
    NS_LOG_INFO ("Scheduling Command DumpDHashInfo...");
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordRun::DumpDHashInfo, this, chordApplication);
  }

  else if (*iterator == "TraceRing")
  {
    if (tokens.size() < 3)
    { 
      return;
    }
    //extract node name
    iterator++;
    std::string vNodeName = std::string(*iterator);
    NS_LOG_INFO ("Scheduling Command TraceRing...");
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordIpv4::FireTraceRing, chordApplication, vNodeName);
  }
  else if (*iterator == "Lookup")
  {
    if (tokens.size() < 3)
    {
      return;
    }
    //extract node resourceName
    iterator++;
    std::string resourceName = std::string(*iterator);
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordRun::Lookup, this, chordApplication, resourceName);
    return;
  }
  else if (*iterator == "Retrieve")
  {
    if (tokens.size() < 3)
    {
      return;
    }
    iterator++;
    std::string resourceName = std::string(*iterator);
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordRun::Retrieve, this, chordApplication, resourceName);
  }
  else if (*iterator == "RemoveVNode")
  {
    if (tokens.size() < 3)
    {
      return;
    }
    //extract node resourceName
    iterator++;
    std::string vNodeName = std::string(*iterator);
    NS_LOG_INFO ("Scheduling Command RemoveVNode...");
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordIpv4::RemoveVNode, chordApplication, vNodeName);
  }
  else if (*iterator == "Detach")
  {
    NS_LOG_INFO ("Scheduling Command Detach...");
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordRun::DetachNode, this, nodeNumber);
  }
  else if (*iterator == "ReAttach")
  {
    NS_LOG_INFO ("Scheduling Command ReAttach...");	
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordRun::ReAttachNode, this, nodeNumber);
  }
  else if (*iterator == "Crash")
  {
    NS_LOG_INFO ("Scheduling Command Crash");
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordRun::CrashChord, this, chordApplication);
  }
  else if (*iterator == "Restart")
  {
    NS_LOG_INFO ("Scheduling Command Restart...");
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordRun::RestartChord, this, chordApplication);
  }
  else if (*iterator == "FixFinger")
  {
    iterator++;
    std::string vNodeName = std::string (*iterator);
    NS_LOG_INFO ("Scheduling Command FixFinger...");
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordIpv4::FixFingers, chordApplication, vNodeName);
  }
  else if (*iterator == "Insert")
  {
    if (tokens.size() < 4)
    {
      return;
    }
    iterator++;
    std::string resourceName = std::string(*iterator);

    iterator++;
    std::string resourceValue = std::string (*iterator);
    NS_LOG_INFO ("Scheduling Command Insert...");
    Simulator::Schedule (MilliSeconds(time.GetMilliSeconds()), &ChordRun::Insert, this, chordApplication, resourceName, resourceValue);
  }
  else
  {
    std::cout << "Unrecognized command\n";
  }
}

void
ChordRun::InsertVNode(Ptr<ChordIpv4> chordApplication, std::string vNodeName)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  unsigned char* md = (unsigned char*) malloc (20);
  const unsigned char* message = (const unsigned char*) vNodeName.c_str();
  SHA1 (message , vNodeName.length() , md);

  NS_LOG_INFO ("Scheduling Command InsertVNode...");
  chordApplication->InsertVNode(vNodeName, md, 20);
  free (md);
}

void
ChordRun::Lookup (Ptr<ChordIpv4> chordApplication, std::string resourceName)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  unsigned char* md = (unsigned char*) malloc (20);
  const unsigned char* message = (const unsigned char*) resourceName.c_str();
  SHA1 (message , resourceName.length() , md);
  NS_LOG_INFO ("Scheduling Command Lookup...");
  chordApplication->LookupKey(md, 20);
  free (md);
}

void
ChordRun::Insert (Ptr<ChordIpv4> chordApplication, std::string resourceName, std::string resourceValue)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  NS_LOG_INFO ("Insert ResourceName : "<< resourceName );
  NS_LOG_INFO ("Insert Resourcevalue : "<< resourceValue);
  unsigned char* md = (unsigned char*) malloc (20);
  const unsigned char* message = (const unsigned char*) resourceName.c_str();
  SHA1 (message , resourceName.length() , md);
  unsigned char* value = (unsigned char *)(resourceValue.c_str());
std::cout<<"value"<<value<<std::endl;
std::cout<<"lentgh"<<resourceValue.length()<<std::endl;
std::cout<<"1"<<std::endl;
  chordApplication->Insert(md, 20, value, resourceValue.length());
  free (md);
}

void
ChordRun::Retrieve (Ptr<ChordIpv4> chordApplication, std::string resourceName)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  unsigned char* md = (unsigned char*) malloc (20);
  const unsigned char* message = (const unsigned char*) resourceName.c_str();
  SHA1 (message , resourceName.length() , md);
  chordApplication->Retrieve (md, 20);
  free (md);
}

void
ChordRun::DetachNode(uint16_t nodeNumber)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  Ptr<NetDevice> netDevice = m_nodeContainer.Get(nodeNumber)->GetDevice(1);
  Ptr<CsmaChannel> channel = netDevice->GetChannel()->GetObject<CsmaChannel> ();
  channel->Detach(nodeNumber);
}

void
ChordRun::ReAttachNode(uint16_t nodeNumber)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  Ptr<NetDevice> netDevice = m_nodeContainer.Get(nodeNumber)->GetDevice(1);
  Ptr<CsmaChannel> channel = netDevice->GetChannel()->GetObject<CsmaChannel> ();
  if (channel->Reattach(nodeNumber) == false)
    std::cout << "Reattach success" << std::endl;
  else
    std::cout << "Reattach failed" << std::endl;
}

void
ChordRun::CrashChord(Ptr<ChordIpv4> chordApplication)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  /* This code used to work in ns-3.6 release */
  //chordApplication -> Stop(Seconds(0.0));
}

void
ChordRun::RestartChord(Ptr<ChordIpv4> chordApplication)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  /* This code used to work in ns-3.6 release */
  //chordApplication -> Start(Seconds(0.0));
}
void
ChordRun::DumpVNodeInfo(Ptr<ChordIpv4> chordApplication,std::string vNodeName)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  chordApplication->DumpVNodeInfo (vNodeName, std::cout);
}

void
ChordRun::DumpDHashInfo (Ptr<ChordIpv4> chordApplication)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  chordApplication->DumpDHashInfo (std::cout);
}

void
ChordRun::JoinSuccess (std::string vNodeName, uint8_t* key, uint8_t numBytes)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  std::cout << "VNode: " << vNodeName << " Joined successfully" << std::endl;
  PrintHexArray (key, numBytes, std::cout);
}

void
ChordRun::LookupSuccess (uint8_t* lookupKey, uint8_t lookupKeyBytes, Ipv4Address ipAddress, uint16_t port)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  std::cout << "Lookup Success Ip: " << ipAddress << " Port: " << port << std::endl;
  PrintHexArray (lookupKey, lookupKeyBytes, std::cout);
}

void
ChordRun::LookupFailure (uint8_t* lookupKey, uint8_t lookupKeyBytes)
{ 
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  std::cout << "Key Lookup failed" << std::endl;
  PrintHexArray (lookupKey, lookupKeyBytes, std::cout);
}

void
ChordRun::VNodeKeyOwnership (std::string vNodeName, uint8_t* key, uint8_t keyBytes, uint8_t* predecessorKey, uint8_t predecessorKeyBytes, uint8_t* oldPredecessorKey, uint8_t oldPredecessorKeyBytes, Ipv4Address predecessorIp, uint16_t predecessorPort)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  std::cout << "VNode: " << vNodeName << " Key Space Ownership change reported" << std::endl;
  std::cout << "New predecessor Ip: " << predecessorIp << " Port: " << predecessorPort << std::endl;
}


void
ChordRun::VNodeFailure (std::string vNodeName, uint8_t* key, uint8_t numBytes)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  std::cout << "VNode: " << vNodeName << " Failed" << std::endl;
}

void
ChordRun::InsertSuccess (uint8_t* key, uint8_t numBytes, uint8_t* object, uint32_t objectBytes)
{ 
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  std::cout << "Insert Success!";
  PrintHexArray (key, numBytes, std::cout);
  PrintCharArray (object, objectBytes, std::cout);
}

void
ChordRun::RetrieveSuccess (uint8_t* key, uint8_t numBytes, uint8_t* object, uint32_t objectBytes)
{ 
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  std::cout << "Retrieve Success!";
  PrintHexArray (key, numBytes, std::cout);
  PrintCharArray (object, objectBytes, std::cout);
}

void
ChordRun::InsertFailure (uint8_t* key, uint8_t numBytes, uint8_t* object, uint32_t objectBytes)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  std::cout << "Insert Failure Reported...";
  PrintHexArray (key, numBytes, std::cout);
  PrintCharArray (object, objectBytes, std::cout);
}

void
ChordRun::RetrieveFailure (uint8_t* key, uint8_t keyBytes)
{
  NS_LOG_FUNCTION_NOARGS();
  std::cout << "\nCurrent Simulation Time: " << Simulator::Now ().GetMilliSeconds() << std::endl;
  std::cout << "Retrieve Failure Reported...";
  PrintHexArray (key, keyBytes, std::cout);
}


void
ChordRun::TraceRing (std::string vNodeName, uint8_t* key, uint8_t numBytes)
{
  std::cout << "<" << vNodeName << ">" << std::endl;
}


void 
ChordRun::Tokenize(const std::string& str,
    std::vector<std::string>& tokens,
    const std::string& delimiters)
{
  // Skip delimiters at beginning.
  std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
  // Find first "non-delimiter".
  std::string::size_type pos = str.find_first_of(delimiters, lastPos);

  while (std::string::npos != pos || std::string::npos != lastPos)
  {
    // Found a token, add it to the vector.
    tokens.push_back(str.substr(lastPos, pos - lastPos));
    // Skip delimiters.  Note the "not_of"
    lastPos = str.find_first_not_of(delimiters, pos);
    // Find next "non-delimiter"
    pos = str.find_first_of(delimiters, lastPos);
  }
}

void
ChordRun::PrintCharArray (uint8_t* array, uint32_t size, std::ostream &os)
{
  os << "Char Array: ";
  for (uint32_t i = 0; i<size; i++)
    os << array[i];
  os << "\n";
}

void
ChordRun::PrintHexArray (uint8_t* array, uint32_t size, std::ostream &os)
{
  os << "Bytes: " << (uint16_t) size << "\n";
  os << "Array: \n";
  os << "[ ";
  for (uint8_t j=0;j<size;j++)
  {
    os << std::hex << "0x" <<(uint16_t) array[j] << " ";
  }
  os << std::dec << "]\n";
}

 int 
 main (int argc, char *argv[])
 {
   uint16_t nodes=100;
   argc=3;
   
   uint16_t bootStrapNodeNum=10;
   std::string scriptFile = "";
   if (argc < 3)
   {
     std::cout << "Usage: chord-run <nodes> <bootstrapNodeNumber> <OPTIONAL: script-file>. Please input number of nodes to simulate and bootstrap node number\n";
    exit (1);
   }
   else
   {
    //nodes = atoi(argv[1]);
    //bootStrapNodeNum = atoi(argv[2]);
    if (argc == 4)
    {
      //scriptFile = argv[3];
    }
    std::cout << "Number of nodes to simulate: " << (uint16_t) nodes << "\n";
   }

   LogComponentEnable ("ChordRun", LOG_LEVEL_ALL);
   LogComponentEnable("ChordIpv4Application", LOG_LEVEL_ERROR);
   //LogComponentEnable("UdpSocketImpl", LOG_LEVEL_ALL);
   //LogComponentEnable("Packet", LOG_LEVEL_ALL);
   //LogComponentEnable("Socket", LOG_LEVEL_ALL);
   //LogComponentEnable("ChordMessage", LOG_LEVEL_ALL);
   LogComponentEnable("ChordIdentifier", LOG_LEVEL_ERROR);
   LogComponentEnable("ChordTransaction", LOG_LEVEL_ERROR);
   LogComponentEnable("ChordVNode", LOG_LEVEL_ERROR);
   LogComponentEnable("ChordNodeTable", LOG_LEVEL_ERROR);
   LogComponentEnable("DHashIpv4", LOG_LEVEL_ERROR);
   LogComponentEnable("DHashConnection", LOG_LEVEL_ERROR);
   //LogComponentEnable("TcpSocketImpl", LOG_LEVEL_ALL);
   //LogComponentEnable("TcpL4Protocol", LOG_LEVEL_ALL);

   //
   // Allow the user to override any of the defaults and the above Bind() at
   // run-time, via command-line arguments
   //
   CommandLine cmd;
   //cmd.Parse (argc, argv);


   //
   // Explicitly create the nodes required by the topology (shown above).
   //

   NS_LOG_INFO ("Creating nodes.");
   NodeContainer nodeContainer;
   nodeContainer.Create (nodes);

   InternetStackHelper internet;
   internet.Install (nodeContainer);

   NS_LOG_INFO ("Create channels.");
   //
   // Explicitly create the channels required by the topology (shown above).
   //
   CsmaHelper csma;
   csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
   csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
   csma.SetDeviceAttribute ("Mtu", UintegerValue (1400));
   NetDeviceContainer d = csma.Install (nodeContainer);

   Ipv4AddressHelper ipv4;
   //
   // We've got the "hardware" in place.  Now we need to add IP addresses.
   //
   NS_LOG_INFO ("Assign IP Addresses.");
   ipv4.SetBase ("10.1.1.0", "255.255.255.0");
   Ipv4InterfaceContainer i = ipv4.Assign (d);

   NS_LOG_INFO ("Create Applications.");
   //
   //Create a command handler thread
   //
   ChordRun chordRun;
   //
   // Create a ChordIpv4 application on all nodes. Insertion of vnodes controlled by user via keyboard.
   //

   uint16_t port = 2000;
   for (int j=0; j<nodes; j++)
   {
     ChordIpv4Helper server (i.GetAddress(bootStrapNodeNum), port, i.GetAddress(j), port, port+1, port+2);
     ApplicationContainer apps = server.Install (nodeContainer.Get(j));
     apps.Start(Seconds (0.0));
     Ptr<ChordIpv4> chordApplication = nodeContainer.Get(j)->GetApplication(0)->GetObject<ChordIpv4> ();
     chordApplication->SetJoinSuccessCallback (MakeCallback(&ChordRun::JoinSuccess, &chordRun));
     chordApplication->SetLookupSuccessCallback (MakeCallback(&ChordRun::LookupSuccess, &chordRun));
     chordApplication->SetLookupFailureCallback (MakeCallback(&ChordRun::LookupFailure, &chordRun));
     chordApplication->SetTraceRingCallback (MakeCallback(&ChordRun::TraceRing, &chordRun));
     chordApplication->SetVNodeFailureCallback(MakeCallback(&ChordRun::VNodeFailure, &chordRun));
     chordApplication->SetVNodeKeyOwnershipCallback(MakeCallback(&ChordRun::VNodeKeyOwnership, &chordRun));
     //DHash configuration:: Needs to be done once but can be overwritten...
     chordApplication->SetInsertSuccessCallback (MakeCallback(&ChordRun::InsertSuccess, &chordRun));
     chordApplication->SetRetrieveSuccessCallback (MakeCallback(&ChordRun::RetrieveSuccess, &chordRun));
     chordApplication->SetInsertFailureCallback (MakeCallback(&ChordRun::InsertFailure, &chordRun));
     chordApplication->SetRetrieveFailureCallback (MakeCallback(&ChordRun::RetrieveFailure, &chordRun));
   }

   //Start Chord-Run 
   chordRun.Start(scriptFile,nodeContainer);
   //
   // Now, do the actual simulation.
   //
   NS_LOG_INFO ("Run Simulation.");
   Simulator::Run ();
   chordRun.Stop ();
   Simulator::Destroy ();
   NS_LOG_INFO ("Done.");
   return 0;

 }



