#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/format.hpp>
#include <boost/generator_iterator.hpp>
#include <boost/program_options.hpp>
#include <boost/random.hpp>
#include <curl/curl.h>

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

namespace po = boost::program_options;

const std::array<double, 3> CENTER_OF_US = {{-98.5795, 39.8282, 0.}};

struct CommandLineOptions {
  bool initialize = true;
  int timeInterval = 1;
  int entityCount = 100;
  bool centerStart = false;
  double stepSize = 0.1;
  bool marchWest = false;
  bool live = false;
  bool ungoverned = false;
  int daysToRun = 1;
  uint64_t startTime = 0;
  uint64_t endtimeOffset = 0;
  std::string hostname;
  std::string dataset;
  std::string apiKey;
  std::string kind;
  bool insecure = false;
  bool testPattern = false;
  bool disableSslVerifyPeer = false;
};

struct Entity {
  std::string id;
  std::string kind;
  std::array<double, 3> location;
  std::array<double, 3> initialLocation;
  uint64_t timestamp;
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

const long long nowUTC() {
  static boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  boost::posix_time::ptime now =
      boost::posix_time::microsec_clock::universal_time();

  return (now - epoch).total_milliseconds();
}

const std::string getTimeString() {
  return boost::posix_time::to_iso_string(
      boost::posix_time::second_clock::universal_time());
}

void moveToNextTestLocation(double &lng, double &lat, const double initialLng,
                            const double initialLat) {
  if (lng == initialLng && lat == initialLat) {
    lng = initialLng + options.stepSize;
  } else if (lng == initialLng + options.stepSize && lat == initialLat) {
    lat = initialLat + options.stepSize;
  } else if (lng == initialLng + options.stepSize &&
             lat == initialLat + options.stepSize) {
    lng = initialLng;
  } else if (lng == initialLng && lat == initialLat + options.stepSize) {
    lat = initialLat;
  } else {
    lat = initialLat;
    lng = initialLng;
  }
}

void updateLocation(std::vector<Entity>::iterator topo,
                    random_generator &walk) {
  double lng = topo->location[0];
  double lat = topo->location[1];
  double newLat = lat;
  double newLng = lng;

  if (options.testPattern) {
    moveToNextTestLocation(newLng, newLat, topo->initialLocation[0],
                           topo->initialLocation[1]);
  } else {
    newLat += walk();
    newLng += walk();
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

std::array<double, 3> getGridLocation(int index, int entityCount) {
  int side = ceil(sqrt(static_cast<double>(entityCount)));
  int row = index / side;
  int col = index % side;
  return {{static_cast<double>(col) * options.stepSize * 3,
           static_cast<double>(row) * options.stepSize * 3, 0}};
}

void initializeEntities() {
  for (int i = 0; i < options.entityCount; ++i) {
    Entity newEntity;
    newEntity.id = str(boost::format("live-test-%1%") % i);
    if (options.testPattern) {
      newEntity.location = getGridLocation(i, options.entityCount);
    } else if (options.centerStart) {
      newEntity.location = CENTER_OF_US;
    } else {
      newEntity.location = {{start_lon(), start_lat(), 0.}};
    }
    newEntity.initialLocation = newEntity.location;
    if (options.live) {
      newEntity.timestamp = nowUTC();
    } else {
      newEntity.timestamp = options.startTime;
    }
    newEntity.kind = options.kind;
    entityList.push_back(newEntity);
  }
}

const std::string updateEntities(random_generator &walk) {
  rapidjson::Document jsonDoc;
  jsonDoc.SetObject();

  rapidjson::Value entities(rapidjson::kArrayType);

  int count = 0;
  for (std::vector<Entity>::iterator entity = entityList.begin();
       entity != entityList.end(); ++entity) {
    updateLocation(entity, walk);
    if (options.live) {
      entity->timestamp = nowUTC();
    } else {
      entity->timestamp += options.timeInterval * 1000;
    }
    rapidjson::Value newEntity(rapidjson::kObjectType);
    newEntity.AddMember("identity",
                        rapidjson::Value(entity->id.c_str(), entity->id.size()),
                        jsonDoc.GetAllocator());
    newEntity.AddMember("timestamp_ms", rapidjson::Value(entity->timestamp),
                        jsonDoc.GetAllocator());
    newEntity.AddMember(
        "endtime_ms",
        rapidjson::Value(entity->timestamp + options.endtimeOffset),
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
  std::cout << getTimeString() << ": Updating " << count << " entities"
            << std::endl;
  jsonDoc.AddMember("entities", entities, jsonDoc.GetAllocator());

  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  jsonDoc.Accept(writer);
  std::string jsonStr = buffer.GetString();
  return jsonStr;
}

void parseCommandLine(int argc, char *argv[]) {
  po::options_description desc(
      "entity-generator is a utility for sending data to Conduce."
      "\n\nConfiguration options");
  desc.add_options()("help", "Print the list of command line options")(
      "kind", po::value<std::string>(&options.kind)->default_value("default"),
      "Data kind to assign to entities")(
      "host",
      po::value<std::string>(&options.hostname)
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
      "center-start",
      po::value<bool>(&options.centerStart)->default_value(false),
      "Originate all nodes at the center of the United States.")(
      "march-west", po::value<bool>(&options.marchWest)->default_value(false),
      "Cause all nodes to move westward every interval")(
      "live", po::bool_switch(&options.live)->default_value(false),
      "Generate topology updates now rather than at a fixed start date and "
      "interval")(
      "disable-ssl-verify-peer",
      po::bool_switch(&options.disableSslVerifyPeer)->default_value(false),
      "Disable SSL peer verification (local env only)")(
      "step-size", po::value<double>(&options.stepSize)->default_value(0.1),
      "Distance to move nodes every time interval (decimal degrees)")(
      "time-interval", po::value<int>(&options.timeInterval)->default_value(1),
      "Seconds between topology updates")(
      "endtime-offset",
      po::value<uint64_t>(&options.endtimeOffset)->default_value(0),
      "Duration after which entity expires (ms)")(
      "start-time", po::value<uint64_t>(&options.startTime)->default_value(0),
      "The timestamp at which the first entity sample should occur (ms)")(
      "ungoverned", po::value<bool>(&options.ungoverned)->default_value(false),
      "Generate updates as quickly as possible")(
      "insecure", po::bool_switch(&options.insecure)->default_value(false),
      "Disables SSL verifications")(
      "test-pattern",
      po::bool_switch(&options.testPattern)->default_value(false),
      "Override random motion entity behavior and generate a test pattern.");

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

// A function that dumps response data from a curl request into the provided
// std::string
size_t writefunc(void *ptr, size_t size, size_t nmemb, std::string *s) {
  s->append((char *)ptr, size * nmemb);
  // std::cout << *s << std::endl;
  return size * nmemb;
}

// A function that dumps response headers from a curl request into the provided
// map
// The map will store header keys and their associated values
size_t headerfunc(void *ptr, size_t size, size_t nitems,
                  std::map<std::string, std::string> *m) {
  std::vector<std::string> strs;
  std::string data((char *)ptr, size * nitems);
  boost::algorithm::split(strs, data, boost::is_any_of(":"));
  if (strs.size() > 1) {
    (*m)[boost::algorithm::trim_copy(strs[0])] =
        boost::algorithm::trim_copy(strs[1]);
  }

  return size * nitems;
}

// Waits for an asynchronous job to complete by querying the provided jobs URI
void waitForCompletion(std::string &jobUri, CURL *curl) {
  if (!jobUri.empty()) {
    char errorBuffer[CURL_ERROR_SIZE];
    std::cout << getTimeString() << ": Waiting for " << jobUri << std::endl;

    // The location header from an asynchronous call gives the relative URI, so
    // we need to prepend the host and /conduce/api
    std::string jobUrl = "https://" + options.hostname + jobUri;

    while (true) {
      // Reset various CURL fields that get modified by the add_data requests.
      // Querying an asynchronous job status is a GET request so we want to hold
      // on to the result data
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
      while (res != CURLE_OK) {
        long responseCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        std::cout << getTimeString() << ": libcurl: " << res << std::endl;
        std::cout << getTimeString() << responseCode << ": " << errorBuffer
                  << std::endl;
        if (responseCode / 100 == 5) {
          res = curl_easy_perform(curl);
        } else {
          break;
        }
      }
      long code = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
      // A successful query for an asynchronous job should yield a 200 response
      // But sometimes asynchronous jobs can take a little long to update their
      // status
      //(Not likely in this program)
      // Async job status is stored in redis with an expiration time of 5
      // minutes.
      // Every time the progress is updated, the 5 minute expiration is
      // refreshed.
      // If a job takes a really long time and the underlying code doesn't do a
      // good job of
      // keeping the progress updated, the redis key can vanish, causing the
      // query for status to fail.
      // Due to the asynchronous nature of the task, it is very difficult for
      // the end user to tell if the job failed (likely due to a server bug)
      // or if it just hasn't updated its progress and it's taking a long time
      // (also a server bug, but a different kind)
      // Given that, we'll just be extra safe and print a warning.  If the user
      // sees nothing but warnings for a long time, it's probably a job failure.
      // Note that if the job fails without killing the whole server process, it
      // should finish and report a failing response status.
      if (code != 200) {
        std::cout << getTimeString()
                  << ": Warning: bad response code received: " << code
                  << std::endl;
      } else {
        // The response for a job status query is a json structure containing at
        // least a 'progress' field (floating point value between 0.0 and 1.0
        // indicating percentage complete)
        // When the job is complete, it will also contain a 'response' field
        // containing an http response code (200 is success for an add_data
        // call)
        // and a 'result' field containing a string with any useful response
        // output (like an error message)
        // We don't really care about anything in a successful response, so
        // we're only processing errors here and breaking from the loop on
        // success
        rapidjson::Document d;
        d.Parse(result.c_str());
        if (d.HasMember("response")) {
          if (d["response"].GetInt() != 200) {
            std::cout << getTimeString() << ": add_data failed with code "
                      << d["response"].GetInt() << "\n\n";
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

  parseCommandLine(argc, argv);
  std::string CONDUCE_ADD_DATA_URL =
      "https://" + options.hostname + "/conduce/api/v1/datasets/add-data/";
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
    entityHeader = curl_slist_append(entityHeader, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, entityHeader);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    if (options.insecure) {
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerfunc);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
    if (options.disableSslVerifyPeer) {
      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
    }
    if (const char *cainfo = std::getenv("REQUESTS_CA_BUNDLE")) {
      curl_easy_setopt(curl, CURLOPT_CAINFO, cainfo);
    }
  }

  initializeEntities();

  boost::mt19937 alg_walk(0);
  boost::random::uniform_real_distribution<> walk_range(-1 * options.stepSize,
                                                        options.stepSize);

  random_generator walk(alg_walk, walk_range);

  const int UPDATE_COUNT = 3600 * 24 * options.daysToRun / options.timeInterval;
  const int START_TIME = nowUTC();
  int updateTime = START_TIME;
  for (int count = 0; count < UPDATE_COUNT; ++count) {
    s = std::string();
    headers.clear();
    const std::string entitiesStr = updateEntities(walk);
    if (strlen(entitiesStr.c_str()) == 0) {
      std::cout << getTimeString() << ": Zero length string" << std::endl;
      continue;
    }
    std::cout << getTimeString() << ": " << addDataUrl << std::endl;
    // Reset all of the curl fields for the next add_data call
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);
    curl_easy_setopt(curl, CURLOPT_URL, addDataUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, entitiesStr.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1);

    CURLcode res;
    errorBuffer[0] = 0;

    res = curl_easy_perform(curl);
    while (res != CURLE_OK) {
      long responseCode = 0;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
      std::cout << getTimeString() << ": libcurl: " << res << std::endl;
      std::cout << getTimeString() << responseCode << ": " << errorBuffer
                << std::endl;
      if (responseCode / 100 == 5) {
        res = curl_easy_perform(curl);
      } else {
        break;
      }
    }
    // std::cout << "request data: " << entitiesStr.c_str() << std::endl;
    // delete[] entitiesStr;
    // The location header contains the URI to query for status updates for the
    // asyncrhonous job
    /*for (auto it = headers.begin(); it != headers.end(); ++it) {
      std::cout << it->first + ": " + it->second << std::endl;
    }*/
    if (headers.find("Location") == headers.end()) {
      std::cout << getTimeString() << ": No Location header found in response"
                << std::endl;
      continue;
    }
    waitForCompletion(headers["Location"], curl);

    if (!options.ungoverned) {
      updateTime += options.timeInterval * 1000;
      int sleepTime = updateTime - nowUTC();
      ;
      if (sleepTime > 0) {
        std::cout << getTimeString() << ": Sleeping for " << sleepTime
                  << " milliseconds" << std::endl;
        usleep(sleepTime * 1000);
      } else {
        std::cout << getTimeString() << ": Behind real-time by " << sleepTime
                  << " milliseconds" << std::endl;
      }
    }
  }

  curl_easy_cleanup(curl);
  return 0;
}
