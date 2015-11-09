/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2015 Christian Kreuzberger and Daniel Posch, Alpen-Adria-University
 * Klagenfurt
 *
 * This file is part of amus-ndnSIM, based on ndnSIM. See AUTHORS for complete list of
 * authors and contributors.
 *
 * amus-ndnSIM and ndnSIM are free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later version.
 *
 * amus-ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * amus-ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/ndnSIM/apps/ndn-app.hpp"
#include "ns3/ndnSIM/utils/tracers/ndn-dashplayer-tracer.hpp"


#include <iostream>     // cout, endl
#include <fstream>      // fstream
#include <vector>
#include <string>
#include <algorithm>    // copy
#include <iterator>     // ostream_operator

using namespace std;




namespace ns3 {

void
FileDownloadedTrace(Ptr<ns3::ndn::App> app, shared_ptr<const ndn::Name> interestName,
double downloadSpeed, long milliSeconds)
{
  std::cout << ">DownloadFinished(" << app->GetNode()->GetId() << "," << Simulator::Now().GetMilliSeconds () << "): "<< *interestName <<
     " Speed: " << downloadSpeed/1000.0 << " Kilobit/s in " << milliSeconds << " ms" << std::endl;
}

int
main(int argc, char* argv[])
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::DropTailQueue::MaxPackets", StringValue("500"));

  ns3::Config::SetDefault("ns3::PointToPointNetDevice::Mtu", StringValue("6120")); // 4 KB


  std::string cfg_requestsFileName("requests_full.csv");
  std::string cfg_adaptationLogic("RateBasedAdaptationLogic");
  std::string cfg_numberOfUsers("5000");
  std::string cfg_cdnBandwidth("500Mbps");
  int maxNbConcurrentUsers = 5000;

  std::string cfg_reprFile1("representations.txt");
  std::string cfg_reprFile2("representations.txt");
  std::string cfg_reprFile3("representations.txt");
  std::string cfg_reprFile4("representations.txt");

  std::string cfg_dashStatsFile("dash-output.txt");


  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
  cmd.AddValue ("adaptationLogic", "Adaptation Strategy used by Client (RateBasedAdaptationLogic,RateAndBufferBasedAdaptationLogic,DASHJSAdaptationLogic)", cfg_adaptationLogic);
  cmd.AddValue ("requestsFileName", "CSV File which lists all the requests (e.g., requests_full.csv)", cfg_requestsFileName);
  cmd.AddValue ("nbUsers", "The number of users which are in the requests file (e.g., 5000)", cfg_numberOfUsers);
  cmd.AddValue ("cdnBandwidthCap", "The Bandwidth of the CDN (e.g., 500Mbps)", cfg_cdnBandwidth);
  cmd.AddValue ("representationsFile1", "CSV File whch lists the available representations for video 1", cfg_reprFile1);
  cmd.AddValue ("representationsFile2", "CSV File whch lists the available representations for video 2", cfg_reprFile2);
  cmd.AddValue ("representationsFile3", "CSV File whch lists the available representations for video 3", cfg_reprFile3);
  cmd.AddValue ("representationsFile4", "CSV File whch lists the available representations for video 4", cfg_reprFile4);
  cmd.AddValue ("dashStatsFile", "Output file for DASH stats of the simulations (e.g., dash-output.txt)", cfg_dashStatsFile);


  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue(cfg_cdnBandwidth));


  maxNbConcurrentUsers = atoi(cfg_numberOfUsers.c_str());

  NS_LOG_UNCOND("NBUsers=" << maxNbConcurrentUsers);

  // Creating nodes
  NodeContainer nodes;
  nodes.Create(2+maxNbConcurrentUsers); // 2+n nodes, connected:{0, ..., n-1} <---> Gateway (n) <----> Server (n+1)

  int server_node_id = maxNbConcurrentUsers+1;
  int gateway_node_id = maxNbConcurrentUsers;

  // Connecting nodes using two links
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(server_node_id), nodes.Get(gateway_node_id));



  FILE* fp = fopen(cfg_requestsFileName.c_str(), "r");
  if (!fp)
  {
    NS_LOG_UNCOND("Could not open requests csv file " << cfg_requestsFileName);
    return -1;
  }

  NS_LOG_UNCOND("Parsing" << cfg_requestsFileName);

  string value;
  int userCnt = 0;

  // get header
  char ch;
  while ((ch = fgetc(fp)) != '\n')
  {
    cout << ch;
  }

  while (!feof(fp) && userCnt < maxNbConcurrentUsers )
  {
    // UserId,StartsAt,StopsAt,VideoId,LinkCapacity,ScreenWidth,ScreenHeight
    int userId;
    float startsAt;
    float stopsAt;
    int videoId;
    int linkCapacity;
    int screenWidth;
    int screenHeight;

    int got = fscanf(fp, "%d,%f,%f,%d,%d,%d,%d",
      &userId, &startsAt, &stopsAt,&videoId, &linkCapacity, &screenWidth, &screenHeight);

    if (got != 7)
    {
      // done
      break;
    }





    PointToPointHelper p2pClient;
    p2pClient.SetChannelAttribute ("Delay", StringValue ("25ms"));

    ostringstream linkCapacityStr;
    linkCapacityStr << linkCapacity << "Kbps";

    p2pClient.SetDeviceAttribute ("DataRate", StringValue (linkCapacityStr.str()));
    p2pClient.SetQueue("ns3::DropTailQueue","MaxPackets",UintegerValue(50));


    // connect node to network
    p2pClient.Install(nodes.Get(gateway_node_id), nodes.Get(userId));

    // install client: Multimedia Consumer
    ns3::ndn::AppHelper consumerHelper("ns3::ndn::FileConsumerCbr::MultimediaConsumer");
    consumerHelper.SetAttribute("AllowUpscale", BooleanValue(true));
    consumerHelper.SetAttribute("AllowDownscale", BooleanValue(false));
    consumerHelper.SetAttribute("ScreenWidth", UintegerValue(screenWidth));
    consumerHelper.SetAttribute("ScreenHeight", UintegerValue(screenHeight));
    consumerHelper.SetAttribute("StartRepresentationId", StringValue("auto"));
    consumerHelper.SetAttribute("MaxBufferedSeconds", UintegerValue(30));
    consumerHelper.SetAttribute("StartUpDelay", StringValue("0.1"));

    consumerHelper.SetAttribute("AdaptationLogic",
      StringValue("dash::player::" + cfg_adaptationLogic));

    ostringstream mpdFileToRequest;
    mpdFileToRequest << "/myprefix" << videoId << "/Fake/Fake.mpd";
    //mpdFileToRequest << "/myprefix0/Fake/Fake.mpd";

    consumerHelper.SetAttribute("MpdFileToRequest",
      StringValue(mpdFileToRequest.str()));


    ApplicationContainer app1 = consumerHelper.Install (nodes.Get(userId));

    app1.Start(Seconds(startsAt));
    app1.Stop(Seconds(stopsAt));


    userCnt++;
  }


  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.setCsSize(0);
  ndnHelper.SetOldContentStore("ns3::ndn::cs::Nocache");
  ndnHelper.InstallAll();



  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/FileDownloadFinished",
                               MakeCallback(&FileDownloadedTrace));



  NS_LOG_UNCOND("Installing strategy choice helper...");

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/myprefix0", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/myprefix1", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/myprefix2", "/localhost/nfd/strategy/best-route");
  ndn::StrategyChoiceHelper::InstallAll("/myprefix3", "/localhost/nfd/strategy/best-route");


  NS_LOG_UNCOND("Installing Producer...");



  ndn::AppHelper producerHelperFake("ns3::ndn::FakeMultimediaServer");

  // Producer will reply to all requests starting with /prefix
  producerHelperFake.SetPrefix("/myprefix0/Fake");
  producerHelperFake.SetAttribute("MetaDataFile", StringValue(cfg_reprFile1));
  producerHelperFake.SetAttribute("MPDFileName", StringValue("Fake.mpd"));

  producerHelperFake.Install(nodes.Get(server_node_id));


  producerHelperFake.SetPrefix("/myprefix1/Fake");
  producerHelperFake.SetAttribute("MetaDataFile", StringValue(cfg_reprFile2));
  producerHelperFake.SetAttribute("MPDFileName", StringValue("Fake.mpd"));

  producerHelperFake.Install(nodes.Get(server_node_id));


  producerHelperFake.SetPrefix("/myprefix2/Fake");
  producerHelperFake.SetAttribute("MetaDataFile", StringValue(cfg_reprFile3));
  producerHelperFake.SetAttribute("MPDFileName", StringValue("Fake.mpd"));

  producerHelperFake.Install(nodes.Get(server_node_id));



  producerHelperFake.SetPrefix("/myprefix3/Fake");
  producerHelperFake.SetAttribute("MetaDataFile", StringValue(cfg_reprFile4));
  producerHelperFake.SetAttribute("MPDFileName", StringValue("Fake.mpd"));

  producerHelperFake.Install(nodes.Get(server_node_id));





  NS_LOG_UNCOND("Installing global routing helper...");
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();

  ndnGlobalRoutingHelper.AddOrigins("/myprefix0", nodes.Get(server_node_id));
  ndnGlobalRoutingHelper.AddOrigins("/myprefix1", nodes.Get(server_node_id));
  ndnGlobalRoutingHelper.AddOrigins("/myprefix2", nodes.Get(server_node_id));
  ndnGlobalRoutingHelper.AddOrigins("/myprefix3", nodes.Get(server_node_id));

  NS_LOG_UNCOND("calling calculateRoutes...");
  ndn::GlobalRoutingHelper::CalculateRoutes();
  NS_LOG_UNCOND("Done calling calculateRoutes");

  Simulator::Stop(Seconds(90720.0));

  ndn::DASHPlayerTracer::InstallAll(cfg_dashStatsFile);
  //ndn::CsTracer::InstallAll("cs-trace.txt", Seconds(1));

  NS_LOG_UNCOND("Starting simulation now...");

  Simulator::Run();
  Simulator::Destroy();

  NS_LOG_UNCOND("Simulation Finished.");

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}



