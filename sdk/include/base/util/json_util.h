
#include <json/json.h>

namespace aichat_sdk
{

class JsonUtil
{
public:
    static void unserialize(const std::string& json ,  ::Json::Value& value);
    static void serialize(const ::Json::Value& json , std::string& json_str);
};



}; // end aichat_sdk