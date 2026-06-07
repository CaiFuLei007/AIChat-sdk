
#include "provider/chatgpt_provider.h"
#include "base/util/mylog.h"
#include "base/util/json_util.h"

#include <httplib.h>
#include <json/json.h>

namespace ai_sdk
{

bool ChatGPTProvider::Init(Config config)
{
    // 初始化模型 : endpoint , apikey , model , path
    if(config.apikey.empty())
    {
        ERR("APIKEY EMPTY");
        return false;
    }

    if(config.end_point.empty())
    {
        config.end_point = "https://api.openai.com";
    }
    if(config.model.empty())
    {
        config.model = "gpt-5.4";
    }
    if(config.path.empty())
    {
        config.path = "/v1/responses";
    }
    
    config_ = std::move(config);
    is_avaiable_ = true;
    return true;
}

std::string ChatGPTProvider::BuildResponseBody(const std::vector<Message> &messages , bool stream)
{
    Json::Value messages_json;
    for(auto& message :  messages)
    {
        Json::Value tmp;
        tmp["content"] = message.content;
        tmp["role"] = message.role;

        messages_json.append(std::move(tmp));
    }

    Json::Value body_obj;
    body_obj["input"] = messages_json;
    body_obj["model"] = config_.model;
    body_obj["max_output_tokens"] = config_.max_token;
    body_obj["stream"] = stream;
    body_obj["temperature"] = config_.temperature;
    body_obj["top_p"] = config_.top_p;

    std::string body_str;
    JsonUtil::serialize(body_obj , body_str);

    return body_str;
}

std::string ChatGPTProvider::SendMessage(const std::vector<Message>& messages)
{
    // 发送请求 , 全量返回
    // 1. 组装 之前的所有消息
    // 2. 设置参数 : model max_output_tokens stream top_p temperature , 设置请求头
    // 3. 创建 client 发送消息
    // 4. 对请求结果进行解析 : response["output"][0]["content"][0]["text"]
    // 5. 返回结果

    std::string body = BuildResponseBody(messages , false);

    httplib::Headers headers = {
        {"Accept" , "application/json"} , 
        {"Authorization" , "Bearer " + config_.apikey}
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
    
    // 对请求结果进行解析 : response["output"][0]["content"][0]["text"]
    if(!response_body.isObject() || !response_body.isMember("output"))
    {
        WARN("RESPONSE NO OUTPUTS");
        return "";
    }
    auto outputs = response_body["output"];
    if(!outputs.isArray() || outputs.size() == 0)
    {
        WARN("OUTPUTS IS EMPTY");
        return "";
    }
    auto output = outputs[0];
    if(!output.isObject() || !output.isMember("content"))
    {
        WARN("OUTPUT NO CONTENT");
        return "";
    }
    auto content = output["content"];
    if(!content.isArray() || content.size() == 0)
    {
        WARN("CONTENT IS EMPTY");
        return "";
    }
    auto item = content[0];
    if(!item.isObject() || !item.isMember("text"))
    {
        WARN("CONTENT NO TEXT");
        return "";
    }
    return item["text"].asString();
}


std::string ChatGPTProvider::SendMessageStream(const std::vector<Message> &messages , MessageCallback message_cb)
{
    // 发送请求 , 流式返回
    // 1. 组装 之前的所有消息
    // 2. 设置参数 : model max_tokens stream top_p temperature , 设置请求头
    // 3. 创建请求 httplib::Request 设置 请求行, 请求头 , 请求体 , 以及对应的 回调
    // 4. 创建 client 发送消息
    // 5. 流式响应 : SSE 格式 : 每条数据使用 \n\n进行分割,  
    //                          event : 存储数据类型 , data : 存储数据, 获取 data["delta"]
    // 5. 返回结果


    std::string body = BuildResponseBody(messages , true);
    httplib::Headers headers = {
        {"Accept" , "application/json"} , 
        {"Authorization" , "Bearer " + config_.apikey} , 
        {"Content-Type" , "application/json"}
    };

    httplib::Request request;
    request.method = "POST";
    request.path = config_.path;
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
            //  event : 存储数据类型 , data : 存储数据, 获取 data["delta"]
            std::string line = buffer.substr(0 , pos);
            buffer.erase(0 , pos + 2);
            std::string prefix_event = "event: ";
            std::string prefix_data = "data: ";
            if(line.compare(0 , prefix_event.size() , prefix_event) != 0)
            {
                continue;
            }
            line.erase(0 , prefix_event.size());
            size_t event_end = line.find('\n');
            if(event_end == std::string::npos)
            {
                continue;
            }
            std::string event = line.substr(0 , event_end);
            line.erase(0 , event_end + 1);
            if(event == "response.completed")
            {
                finish = true;
                message_cb("" , true);
                return true;
            }
            if(event != "response.output_text.done" && event != "response.output_text.delta")
            {
                continue;
            }
            if(line.compare(0 , prefix_data.size() , prefix_data) != 0)
            {
                continue;
            }
            line.erase(0 , prefix_data.size());

            // 解析数据
            Json::Value response ;
            JsonUtil::unserialize(line , response);
            if(!response.isObject())
            {
                WARN("RESPONSE PARSE FAILED");
                continue;
            }

            if(event == "response.output_text.done")
            {
                if(response.isMember("text"))
                {
                    full_message = response["text"].asString();
                }
                return true;
            }
            else if(event == "response.output_text.delta")
            {
                if(response.isMember("delta"))
                {
                    std::string delta = response["delta"].asString();
                    message_cb(delta , false);
                }
                return true;
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