#include <iostream>
#include <string>
#include <array>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
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

const std::array<double, 3> CENTER_OF_US = {{-98.5795, 39.8282, 0.}};
// const long long JAN_01_1996_GMT = 820454400000;
const long long ONE_WEEK = 1000 * 60 * 60 * 24 * 7;

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
  std::string kind;
  bool insecure = false;
};

struct Entity {
  std::string id;
  std::string kind;
  std::array<double, 3> location;
  long long timestamp;
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
  return (date - epoch).total_milliseconds();
}

const long long NowGMT() {
  static boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  boost::posix_time::ptime now =
      boost::posix_time::microsec_clock::universal_time();

  return (now - epoch).total_milliseconds();
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
  if (lat > -90 && newLat < -90) {
    newLat += 180;
  } else if (lat < 90 and newLat > 90) {
    newLat -= 180;
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
      newEntity.location = {{start_lon(), start_lat(), 0.}};
    }
    if (options.live) {
      newEntity.timestamp = NowGMT();
    } else {
      // newEntity.timestamp = JAN_01_1996_GMT;
      newEntity.timestamp = NowGMT() - ONE_WEEK;
    }
    newEntity.kind = options.kind;
    entityList.push_back(newEntity);
  }
}

const char *updateEntities(random_generator &walk) {
  rapidjson::Document jsonDoc;
  jsonDoc.SetObject();

  rapidjson::Value entities(rapidjson::kArrayType);

  int count = 0;
  for (std::vector<Entity>::iterator entity = entityList.begin();
       entity != entityList.end(); ++entity) {
    updateLocation(entity, walk);
    if (options.live) {
      entity->timestamp = NowGMT();
    } else {
      entity->timestamp += options.timeInterval * 1000;
    }
    rapidjson::Value newEntity(rapidjson::kObjectType);
    newEntity.AddMember("identity",
                        rapidjson::Value(entity->id.c_str(), entity->id.size()),
                        jsonDoc.GetAllocator());
    newEntity.AddMember("timestamp-ms", rapidjson::Value(entity->timestamp),
                        jsonDoc.GetAllocator());
    newEntity.AddMember(
        "endtime-ms",
        rapidjson::Value(entity->timestamp + options.timeInterval * 1000),
        jsonDoc.GetAllocator());
    newEntity.AddMember(
        "kind", rapidjson::Value(entity->kind.c_str(), entity->kind.size()),
        jsonDoc.GetAllocator());
    newEntity.AddMember("path",
                        getJsonPath(entity->location, jsonDoc.GetAllocator()),
                        jsonDoc.GetAllocator());
    entities.PushBack(newEntity, jsonDoc.GetAllocator());
    ++count;
  }
  std::cout << "Updated " << count << " entities" << std::endl;
  jsonDoc.AddMember("entities", entities, jsonDoc.GetAllocator());

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  jsonDoc.Accept(writer);
  const char *jsonStr = buffer.GetString();
  char *strCpy = new char[strlen(jsonStr)];
  strcpy(strCpy, jsonStr);
  return strCpy;
}

void parseCommandLine(int argc, char *argv[]) {
  po::options_description desc("topology-generator is a utility for sending "
                               "network fluoroscope data (topologies) to "
                               "sluice.\n\nConfiguration options");
  desc.add_options()("help", "Print the list of command line options")(
      "kind", po::value<std::string>(&options.kind)->default_value("default"),
      "Data kind to assign to entities")(
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
      "Generate updates as quickly as possible")(
      "insecure", po::bool_switch(&options.insecure)->default_value(false),
      "Disables SSL verifications");

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

struct string {
  char *ptr;
  size_t len;
};

size_t writefunc(void *ptr, size_t size, size_t nmemb, std::string *s) {
    s->append((char*) ptr, size * nmemb);
    return size * nmemb;
}

size_t headerfunc(void* ptr, size_t size, size_t nitems, std::map<std::string, std::string>* m) {
    std::vector<std::string> strs;
    std::string data((char*)ptr, size * nitems);
    boost::algorithm::split(strs, data, boost::is_any_of(":"));
    if (strs.size() > 1) {
        (*m)[boost::algorithm::trim_copy(strs[0])] = boost::algorithm::trim_copy(strs[1]);
    }

    return size * nitems;
}

void waitForCompletion(std::string& job, CURL* curl) {
    if (!job.empty()) {
        char errorBuffer[CURL_ERROR_SIZE];
        std::cout << "Waiting for " << job << std::endl;
        std::string jobUrl = "https://" + options.hostname + "/conduce/api" + job;

        while (true) {
            std::string result;
            std::map<std::string, std::string> headers;
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
            curl_easy_setopt(curl, CURLOPT_URL, jobUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, 0);
            curl_easy_setopt(curl, CURLOPT_POST, 0);
            curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);

            CURLcode res;
            errorBuffer[0] = 0;
            res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cout << "libcurl: " << res << std::endl;
                std::cout << errorBuffer << std::endl;
                exit(1);
            }
            long code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            if (code != 200) {
                std::cout << "Warning: bad response code received: " << code << std::endl;
            } else {
                rapidjson::Document d;
                d.Parse(result.c_str());
                if (d.HasMember("response")) {
                    if (d["response"].GetInt() != 200) {
                        std::cout << "add_data failed with code " << d["response"].GetInt() << "\n\n";
                        std::cout << d["result"].GetString() << std::endl;
                        exit(1);
                    }
                    return;
                }
                usleep(1000);
            }
        }
    }
}

int main(int argc, char *argv[]) {

  boost::mt19937 alg_walk(0);
  boost::random::uniform_real_distribution<> walk_range(-1 * options.stepSize,
                                                        options.stepSize);

  random_generator walk(alg_walk, walk_range);

  parseCommandLine(argc, argv);
  std::string CONDUCE_ADD_DATA_URL =
      "https://" + options.hostname + "/conduce/api/datasets/add_datav2/";
  std::string addDataUrl = CONDUCE_ADD_DATA_URL + options.dataset;
  CURL *curl = curl_easy_init();
  char errorBuffer[CURL_ERROR_SIZE];
  std::string s;
  std::map<std::string, std::string> headers;
  struct curl_slist *entityHeader = NULL;
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, addDataUrl.c_str());
    entityHeader =
        curl_slist_append(entityHeader, "Content-Type: application/json");
    std::string keyHeader = "Authorization: Bearer " + options.apiKey;
    entityHeader = curl_slist_append(entityHeader, keyHeader.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, entityHeader);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    if (options.insecure) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerfunc);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
  }

  initializeEntities();

  const int UPDATE_COUNT =
      (60 / options.timeInterval) * 60 * 24 * options.daysToRun;
  for (int count = 0; count < UPDATE_COUNT; ++count) {
    // double before = NowGMT();
    s = std::string();
    headers.clear();
    const char *entitiesStr = updateEntities(walk);
    if (strlen(entitiesStr) == 0) {
      std::cout << "Zero length string" << std::endl;
      return 1;
    }
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
    curl_easy_setopt(curl, CURLOPT_URL, addDataUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, entitiesStr);
    curl_easy_setopt(curl, CURLOPT_POST, 1);
    std::cout << "request data: " << entitiesStr << std::endl;

    CURLcode res;
    errorBuffer[0] = 0;

    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      std::cout << "libcurl: " << res << std::endl;
      std::cout << errorBuffer << std::endl;
      return 1;
    }
    delete[] entitiesStr;
    if (headers.find("Location") == headers.end()) {
        std::cout << "No Location header found in response" << std::endl;
        return 1;
    }
    waitForCompletion(headers["Location"], curl);

    if (!options.ungoverned) {
      sleep(options.updatePeriod);
    }
  }

  curl_easy_cleanup(curl);
  return 0;
}
