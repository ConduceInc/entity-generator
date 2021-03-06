#include <iostream>
#include <string>
#include <array>

#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/format.hpp>
#include <boost/random.hpp>
#include <boost/generator_iterator.hpp>
#include <boost/program_options.hpp>
#include <curl/curl.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

namespace po = boost::program_options;

const std::array<double, 3> CENTER_OF_US = {{39.8282, -98.5795, 0.}};
const double JAN_01_1996_GMT = 820454400.;

struct CommandLineOptions {
  bool initialize = true;
  int updatePeriod = 1;
  int timeInterval = 15;
  int entityCount = 100;
  bool centerStart = false;
  double stepSize = 0.1;
  bool marchWest = false;
  bool live = false;
  bool ungoverned = true;
  int daysToRun = 1;
  std::string hostname;
  std::string dataset;
  std::string apiKey;
};

struct Entity {
  std::string id;
  std::string kind;
  std::array<double, 3> location;
  double timestamp;
};

std::vector<Entity> entityList;
CommandLineOptions options;

typedef boost::variate_generator<boost::mt19937,
                                 boost::random::uniform_real_distribution<>>
    random_generator;

boost::mt19937 alg_start_lat(1);
boost::random::uniform_real_distribution<> start_lat_range(24., 49.);
random_generator start_lat(alg_start_lat, start_lat_range);

boost::mt19937 alg_start_lon(2);
boost::random::uniform_real_distribution<> start_lon_range(-125., -66.);
random_generator start_lon(alg_start_lon, start_lon_range);

const double getStartDate() {
  static boost::posix_time::ptime date(boost::gregorian::date(1996, 1, 1));
  static boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  return (date - epoch).total_milliseconds() / 1000.0;
}

const double NowGMT() {
  static boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  boost::posix_time::ptime now =
      boost::posix_time::microsec_clock::universal_time();

  return (now - epoch).total_milliseconds() / 1000.0;
}

void updateLocation(std::vector<Entity>::iterator topo,
                    random_generator &walk) {
  double lng = topo->location[0];
  double lat = topo->location[1];
  double newLat = lat + walk();
  double newLng = lng + walk();
  if (options.marchWest) {
    newLng = lng - options.stepSize;
  }

  if (lng > -180 && newLng < -180) {
    newLng += 360;
  } else if (lng < 180 and newLng > 180) {
    newLng -= 360;
  }
  if (lat > -180 && newLat < -180) {
    newLat += 360;
  } else if (lat < 180 and newLat > 180) {
    newLat -= 360;
  }

  topo->location[0] = newLng;
  topo->location[1] = newLat;
}

rapidjson::Value getJsonPath(std::array<double, 3> location,
                             rapidjson::Document::AllocatorType &allocator) {
  rapidjson::Value jsonLoc(rapidjson::kObjectType);
  jsonLoc.AddMember("x", rapidjson::Value(location[0]), allocator);
  jsonLoc.AddMember("y", rapidjson::Value(location[1]), allocator);
  jsonLoc.AddMember("z", rapidjson::Value(location[2]), allocator);
  rapidjson::Value path(rapidjson::kArrayType);
  path.PushBack(jsonLoc, allocator);
  return path;
}

void initializeEntities() {
  for (int i = 0; i < options.entityCount; ++i) {
    Entity newEntity;
    newEntity.id = str(boost::format("live-test-%1%") % i);
    if (options.centerStart) {
      newEntity.location = CENTER_OF_US;
    } else {
      newEntity.location = {{start_lat(), start_lon(), 0.}};
    }
    if (options.live) {
      newEntity.timestamp = NowGMT();
    } else {
      newEntity.timestamp = JAN_01_1996_GMT;
    }
    newEntity.kind = "default";
    entityList.push_back(newEntity);
  }
}

const char *updateEntities() {
  boost::mt19937 alg_walk(0);
  boost::random::uniform_real_distribution<> walk_range(-1 * options.stepSize,
                                                        options.stepSize);

  random_generator walk(alg_walk, walk_range);

  // std::vector<Slaw> topoSlaws;
  rapidjson::Document jsonDoc;
  jsonDoc.SetObject();

  rapidjson::Value entities(rapidjson::kArrayType);

  for (std::vector<Entity>::iterator entity = entityList.begin();
       entity != entityList.end(); ++entity) {
    updateLocation(entity, walk);
    if (options.live) {
      entity->timestamp = NowGMT();
    } else {
      entity->timestamp += options.timeInterval;
    }
    rapidjson::Value newEntity(rapidjson::kObjectType);
    newEntity.AddMember("identity",
                        rapidjson::Value(entity->id.c_str(), entity->id.size()),
                        jsonDoc.GetAllocator());
    newEntity.AddMember("timestamp_ms", rapidjson::Value(entity->timestamp),
                        jsonDoc.GetAllocator());
    newEntity.AddMember(
        "kind", rapidjson::Value(entity->kind.c_str(), entity->kind.size()),
        jsonDoc.GetAllocator());
    newEntity.AddMember("path",
                        getJsonPath(entity->location, jsonDoc.GetAllocator()),
                        jsonDoc.GetAllocator());
    // topoSlaws.push_back();
    entities.PushBack(newEntity, jsonDoc.GetAllocator());
  }
  jsonDoc.AddMember("entities", entities, jsonDoc.GetAllocator());

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  jsonDoc.Accept(writer);
  return buffer.GetString();
}

void parseCommandLine(int argc, char *argv[]) {
  po::options_description desc("topology-generator is a utility for sending "
                               "network fluoroscope data (topologies) to "
                               "sluice.\n\nConfiguration options");
  desc.add_options()("help", "Print the list of command line options")(
      "host", po::value<std::string>(&options.hostname)
                  ->default_value("dev-app.conduce.com"),
      "Resolvable name or IP address of Conduce server")(
      "dataset-id", po::value<std::string>(&options.dataset),
      "Dataset unique identifier")(
      "api-key", po::value<std::string>(&options.apiKey),
      "API key used validate data upload to dataset")(
      "entity-count", po::value<int>(&options.entityCount)->default_value(100),
      "Number of entities to generate topologies for")(
      "days", po::value<int>(&options.daysToRun)->default_value(1),
      "Days of topologies to send to pool server")(
      "initialize-topology",
      po::value<bool>(&options.initialize)->default_value(true),
      "Add topologies to sluice.  Disable to only send topology updates.")(
      "center-start",
      po::value<bool>(&options.centerStart)->default_value(false),
      "Originate all nodes at the center of the United States.")(
      "march-west", po::value<bool>(&options.marchWest)->default_value(false),
      "Cause all nodes to move westward every interval")(
      "live", po::value<bool>(&options.live)->default_value(false),
      "Generate topology updates now rather than at a fixed start date and "
      "interval")(
      "step-size", po::value<double>(&options.stepSize)->default_value(0.1),
      "Distance to move nodes every time interval (decimal degrees)")(
      "time-interval", po::value<int>(&options.timeInterval)->default_value(15),
      "Seconds between topology updates")(
      "update-period", po::value<int>(&options.updatePeriod)->default_value(1),
      "Period on which updates are sent")(
      "ungoverned", po::value<bool>(&options.ungoverned)->default_value(true),
      "Generate updates as quickly as possible");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    exit(0);
  }

  bool abort = false;

  if (!vm.count("dataset-id")) {
    std::cerr << "A dataset ID must be provided." << std::endl;
    std::cerr << "entity-generator --dataset-id=ID" << std::endl;
    abort = true;
  }
  if (!vm.count("api-key")) {
    std::cerr << "An API key must be provided." << std::endl;
    std::cerr << "entity-generator --api-key=TOKEN" << std::endl;
    abort = true;
  }

  if (abort) {
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  parseCommandLine(argc, argv);
  std::string CONDUCE_CREATE_DATASET_URL =
      "https://" + options.hostname + "/conduce/api/datasets/create";
  CURL *curl = curl_easy_init();
  struct curl_slist *createHeader = NULL;
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, CONDUCE_CREATE_DATASET_URL.c_str());
    createHeader =
        curl_slist_append(createHeader, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, createHeader);
    curl_easy_perform(curl);
  }

  return 0;
}
