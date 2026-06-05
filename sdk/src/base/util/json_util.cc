
#include "base/util/json_util.h"

namespace ai_sdk
{

void JsonUtil::unserialize(const std::string& json ,  ::Json::Value& value)
{
    ::Json::CharReaderBuilder builder;
    std::unique_ptr< ::Json::CharReader > reader(builder.newCharReader());
    reader->parse(json.c_str(), json.c_str() + json.size(), &value, nullptr);

    return ;
}
void JsonUtil::serialize(const ::Json::Value& json , std::string& json_str)
{
    ::Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::unique_ptr< ::Json::StreamWriter > writer(builder.newStreamWriter());
    std::stringstream ss;
    writer->write(json, &ss);
    json_str = ss.str();

    return ;
}


}; // end ai_sdk