
#include "provider/gemini_provider.h"
#include "base/util/mylog.h"
#include "base/util/json_util.h"

#include <httplib.h>
#include <json/json.h>

namespace ai_sdk
{

bool GeminiProvider::Init(Config config)
{
    // 初始化模型 : endpoint , apikey , model , path
    if(config.apikey.empty())
    {
        ERR("APIKEY EMPTY");
        return false;
    }

    if(config.end_point.empty())
    {
        config.end_point = "https://generativelanguage.googleapis.com";
    }
    if(config.model.empty())
    {
        config.model = "gemini-3.5-flash";
    }
    if(config.path.empty())
    {
        config.path = "/v1beta/models/gemini-3.5-flash:generateContent";
    }
    if(config.streampath.empty())
    {
        config.streampath = "/v1beta/models/gemini-3.5-flash:streamGenerateContent?alt=sse";
    }
    
    config_ = std::move(config);
    is_avaiable_ = true;
    return true;
}

std::string GeminiProvider::BuildResponseBody(const std::vector<Message> &messages)
{
    Json::Value messages_json;
    for(auto& message :  messages)
    {
        Json::Value tmp;
        Json::Value parts_arr;
        Json::Value part;
        part["text"] = message.content;
        parts_arr.append(std::move(part));
        tmp["parts"] = parts_arr;
        tmp["role"] = message.role;

        messages_json.append(std::move(tmp));
    }

    Json::Value generation_config;
    generation_config["maxOutputTokens"] = config_.max_token;
    generation_config["temperature"] = config_.temperature;
    generation_config["topP"] = config_.top_p;

    Json::Value body_obj;
    body_obj["contents"] = messages_json;
    body_obj["generationConfig"] = generation_config;

    std::string body_str;
    JsonUtil::serialize(body_obj , body_str);

    return body_str;
}

std::string GeminiProvider::SendMessage(const std::vector<Message>& messages)
{
    // 发送请求 , 全量返回
    // 1. 组装 之前的所有消息
    // 2. 设置参数 : model max_tokens stream top_p temperature , 设置请求头
    // 3. 创建 client 发送消息
    // 4. 对请求结果进行解析 : response["candidates"][0]["content"]["parts"][0]["text"]
    // 5. 返回结果

    std::string body = BuildResponseBody(messages);

    httplib::Headers headers = {
        {"Accept" , "application/json"} ,
        {"x-goog-api-key" , config_.apikey}
    };

    httplib::Client client(config_.end_point);
    auto ret = client.Post(config_.path , headers , body, "application/json");
    if(!ret)
    {
        WARN("CLIENT POST FIAL : {}" , httplib::to_string(ret.error()));
        return "";
    }

    Json::Value response_body;
    JsonUtil::unserialize(ret->body , response_body);
    
    // 对请求结果进行解析 : response["candidates"][0]["content"]["parts"][0]["text"]
    if(!response_body.isObject() || !response_body.isMember("candidates"))
    {
        WARN("RESPONSE NO CHOICES");
        return "";
    }
    auto candidates = response_body["candidates"];
    if(!candidates.isArray() || candidates.size() == 0)
    {
        WARN("CANDIDATES IS EMPTY");
        return "";
    }
    auto candidate = candidates[0];
    if(!candidate.isObject() || !candidate.isMember("content"))
    {
        WARN("CANDIDATE NO CONTENT");
        return "";
    }
    auto content = candidate["content"];
    if(!content.isObject() || !content.isMember("parts"))
    {
        WARN("CONTENT NO PARTS");
        return "";
    }
    auto parts = content["parts"];
    if(!parts.isArray() || parts.size() == 0)
    {
        WARN("PARTS EMPTY");
        return "";
    }
    auto part = parts[0];
    if(!part.isObject() || !part.isMember("text"))
    {
        WARN("PART NO TEXT");
        return "";
    }
    return part["text"].asString();
}


std::string GeminiProvider::SendMessageStream(const std::vector<Message> &messages , MessageCallback message_cb)
{
    // 发送请求 , 流式返回
    // 1. 组装 之前的所有消息
    // 2. 设置参数 : model max_tokens stream top_p temperature , 设置请求头
    // 3. 创建请求 httplib::Request 设置 请求行, 请求头 , 请求体 , 以及对应的 回调
    // 4. 创建 client 发送消息
    // 5. 流式响应 : SSE 格式 : 每条数据使用 \n\n进行分割, 获取 data["candidates"][0]["content"]["parts"][0]["text"]
    //                                                     如果 data["candidates"][0] 中包含 "finishReason" 字段说明是最后一条数据
    // 6. 返回结果


    std::string body = BuildResponseBody(messages);
    httplib::Headers headers = {
        {"Accept" , "application/json"} , 
        {"x-goog-api-key" , config_.apikey} , 
        {"Content-Type" , "application/json"}
    };

    httplib::Request request;
    request.method = "POST";
    request.path = config_.streampath;
    request.headers = std::move(headers);
    request.body = body;


    bool finish = false;
    bool is_error = false;
    std::string error_msg;
    std::string buffer;
    std::string full_message;

    request.response_handler = [&](const httplib::Response &response)
    {
        if(response.status != 200)
        {
            is_error = true;
            error_msg = "RESPONSE STATUS : " + std::to_string(response.status);
            return false;
        }

        return true;
    };

    request.content_receiver = [&](const char *data, size_t data_length, size_t offset, size_t total_length)->bool
    {
        //  \r\n\r\n , \n\n
        for(size_t i = 0; i < data_length; i++)
        {
            if(data[i] != '\r') 
                buffer += data[i];
        }

        size_t pos;
        // 开始进行解析
        while((pos = buffer.find("\n\n")) != std::string::npos)
        {
            std::string line = buffer.substr(0 , pos);
            buffer.erase(0 , pos + 2);
            std::string prefix = "data: ";
            if(line.compare(0 , prefix.size() , prefix) != 0)
            {
                continue;
            }
            line.erase(0 , prefix.size());

            
            // 5. 流式响应 : SSE 格式 : 每条数据使用 \n\n进行分割, 获取 data["candidates"][0]["content"]["parts"][0]["text"]
            //                                                     如果 data["candidates"][0] 中包含 "finishReason" 字段说明是最后一条数据
            Json::Value response ;
            JsonUtil::unserialize(line , response);
            if(!response.isObject() || !response.isMember("candidates"))
            {
                WARN("RESPONSE NO CANDIDATES");
                continue;
            }
            auto candidates = response["candidates"];
            if(!candidates.isArray() || candidates.size() == 0)
            {
                WARN("CANDIDATES IS EMPTY");
                continue;
            }
            auto candidate = candidates[0];
            if(!candidate.isObject() || !candidate.isMember("content"))
            {
                if(candidate.isObject() && candidate.isMember("finishReason"))
                {
                    finish = true;
                    message_cb("" , true);
                }
                continue;
            }
            auto content = candidate["content"];
            if(!content.isObject() || !content.isMember("parts"))
            {
                WARN("CONTENT NO PARTS");
                continue;
            }
            auto parts = content["parts"];
            if(!parts.isArray() || parts.size() == 0)
            {
                WARN("PARTS EMPTY");
                continue;
            }
            auto part = parts[0];
            if(!part.isObject() || !part.isMember("text"))
            {
                WARN("PART NO TEXT");
                continue;
            }

            std::string text = part["text"].asString();
            full_message += text;
            message_cb(text , false);

            if(candidate.isMember("finishReason"))
            {
                finish = true;
                message_cb("" , true);
            }
        }
        return true;
    };

    httplib::Client client(config_.end_point);
    bool ret = client.send(request);
    if(!ret)
    {
        ERR("CLIENT SEND FALSI : {} " , error_msg);
        return "";
    }


    return full_message;
}

} // end ai_sdk