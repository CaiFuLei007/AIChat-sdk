
#include "aichat_server.h"
#include "openssl/sha.h"
#include <filesystem>
#include <fstream>

AIChatServer::AIChatServer(const std::string &db_name, const util::Config_info &config)
    : ai_sdk_(db_name),
      curl_(std::make_unique<util::Curl>(config))
{
    srand(time(nullptr));
}

bool AIChatServer::unserialize(const std::string &json, ::Json::Value &value)
{
    ::Json::CharReaderBuilder builder;
    std::unique_ptr<::Json::CharReader> reader(builder.newCharReader());
    return reader->parse(json.c_str(), json.c_str() + json.size(), &value, nullptr);
}

bool AIChatServer::serialize(const ::Json::Value &json, std::string &json_str)
{
    ::Json::StreamWriterBuilder builder;
    builder["indentation"] = "";  // 紧凑格式，无换行缩进
    std::unique_ptr<::Json::StreamWriter> writer(builder.newStreamWriter());
    std::stringstream ss;
    writer->write(json, &ss);
    json_str = ss.str();

    return true;
}

bool AIChatServer::SendVerification(const std::string &email)
{
    std::unique_lock lock(mutex_);

    int info = rand() % 90000 + 10000;
    std::string info_str = std::to_string(info);

    std::string message = 
        "Subject: [AIChat] Verification Code\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "\r\n"
        "您的验证码为：" + info_str + "\r\n"
        "\r\n"
        "该验证码 5 分钟内有效，请勿泄露给他人。\r\n";


    bool ret = curl_->Send({email}, message);
    if(!ret)
    {
        return false;
    }

    verification_code_[email] = info_str;

    ai_sdk_.AddTimerTask(email, 5, [this, email]() {
        std::unique_lock lock(mutex_);
        auto it = verification_code_.find(email);
        if(it == verification_code_.end())
        {
            return ;
        }
        verification_code_.erase(it); 
    });
    return true;
}

void AIChatServer::HandleError(const std::string &message ,  httplib::Response& response)
{
    Json::Value response_json;
    response_json["code"] = -1;
    response_json["msg"] = message;

    std::string response_body;
    serialize(response_json,response_body);
    response.body = response_body;
    return ;
}


void AIChatServer::HandleGetVerification(const httplib::Request &request, httplib::Response &response)
{
    auto body_str = request.body;
    Json::Value body_json;
    if(!unserialize(body_str , body_json))
    {
        HandleError("反序列化错误", response);
        return ;
    }
    std::string email = body_json["email"].asString();
    if(!SendVerification(email))
    {
        HandleError("发送验证码错误", response);
        return ;
    }

    Json::Value response_json;
    response_json["code"] = 0;
    response_json["msg"] = "success";

    std::string response_body;
    serialize(response_json,response_body);
    response.body = response_body;
    return ;
}

void AIChatServer::HandleRegister(const httplib::Request &request, httplib::Response &response)
{
    std::unique_lock lock(mutex_);

    auto body_str = request.body;
    Json::Value body_json;
    if(!unserialize(body_str , body_json))
    {
        HandleError("反序列化错误", response);
        return ;
    }

    std::string email = body_json["email"].asString();
    std::string code = body_json["code"].asString();
    auto it = verification_code_.find(email);
    if(it == verification_code_.end())
    {
        HandleError("验证码超时", response);
        return ;
    }
    if(it->second != code)
    {
        HandleError("验证码错误", response);
        return ;
    }

    std::string password = body_json["password"].asString();
    unsigned char hash_password[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)password.c_str(), password.size(), hash_password);

    // 将二进制哈希转为十六进制字符串，避免 null 字节截断问题
    static const char hex_chars[] = "0123456789abcdef";
    std::string hex_password;
    hex_password.reserve(SHA256_DIGEST_LENGTH * 2);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hex_password.push_back(hex_chars[(hash_password[i] >> 4) & 0x0F]);
        hex_password.push_back(hex_chars[hash_password[i] & 0x0F]);
    }

    std::string uid = ai_sdk_.CreateUser(email , hex_password);

    Json::Value response_json;
    response_json["code"] = 0;
    response_json["msg"] = "success";

    response_json["data"]["uid"] = uid;

    std::string response_body;
    serialize(response_json,response_body);
    response.body = response_body;

    verification_code_.erase(email);
    return ;
}

void AIChatServer::HandleLogin(const httplib::Request &request, httplib::Response &response)
{
    auto body_str = request.body;
    Json::Value body_json;
    if(!unserialize(body_str , body_json))
    {
        HandleError("反序列化错误", response);
        return ;
    }

    std::string email = body_json["email"].asString();
    std::string password = body_json["password"].asString();
    unsigned char hash_password[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)password.c_str(), password.size(), hash_password);

    // 将二进制哈希转为十六进制字符串，与注册时保持一致
    static const char hex_chars[] = "0123456789abcdef";
    password.clear();
    password.reserve(SHA256_DIGEST_LENGTH * 2);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        password.push_back(hex_chars[(hash_password[i] >> 4) & 0x0F]);
        password.push_back(hex_chars[hash_password[i] & 0x0F]);
    }

    if(!ai_sdk_.HasUser(email))
    {
        HandleError("用户未进行注册" , response);
        return ;
    }

    auto info = ai_sdk_.GetUser(email , password);
    if(!info)
    {
        HandleError("邮箱或密码错误" , response);
        return ;
    }

    Json::Value response_json;
    response_json["code"] = 0;
    response_json["msg"] = "success";

    response_json["data"]["uid"] = info->uid;
    response_json["data"]["email"] = info->email;
    response_json["data"]["create_time"] = info->create_time;

    std::string response_body;
    serialize(response_json,response_body);
    response.body = response_body;
    return ;
}

void AIChatServer::HandleGetUserSessions(const httplib::Request &request, httplib::Response &response)
{
    std::string uid = request.path_params.at("uid");
    
    auto all_session = ai_sdk_.GetUserAllSession(uid);

    Json::Value sessoins_body;
    for(auto& session : all_session)
    {
        Json::Value tmp;
        tmp["session_id"] = session.session_id;
        tmp["model_name"] = session.model_name;
        tmp["create_time"] = session.create_time;
        tmp["update_time"] = session.update_time;
        if(!session.messages.empty())
            tmp["last_message"] = session.messages.back().content;
        
        sessoins_body.append(tmp);
    }

    Json::Value response_json;
    response_json["code"] = 0;
    response_json["msg"] = "success";

    response_json["data"]["sessions"] = sessoins_body;

    std::string response_body;
    serialize(response_json,response_body);
    response.body = response_body;
    return ;
}

void AIChatServer::HandleGetModels(const httplib::Request &request, httplib::Response &response)
{
    auto models = ai_sdk_.GetAllModels();
    Json::Value model_body;
    for(auto& model : models)
    {
        Json::Value tmp;
        tmp["model_name"] = model.model_name;
        tmp["model_desc"] = model.model_decs;

        model_body.append(tmp);
    }

    Json::Value response_json;
    response_json["code"] = 0;
    response_json["msg"] = "success";

    response_json["data"]["models"] = model_body;

    std::string response_body;
    serialize(response_json,response_body);
    response.body = response_body;
}

void AIChatServer::HandleCreateSession(const httplib::Request &request, httplib::Response &response)
{
    auto body_str = request.body;
    Json::Value body_json;
    if(!unserialize(body_str , body_json))
    {
        HandleError("反序列化错误", response);
        return ;
    }
    std::string uid = body_json["uid"].asString();
    std::string model_name = body_json["model_name"].asString();
    if(uid.empty())
    {
        HandleError("uid 为空", response);
        return ;
    }
    if(model_name.empty())
    {
        HandleError("模型名称为空", response);
        return ;
    }

    std::string session_id = ai_sdk_.CreateSession(uid , model_name);

    Json::Value response_json;
    response_json["code"] = 0;
    response_json["msg"] = "success";

    response_json["data"]["session_id"] = session_id;

    std::string response_body;
    serialize(response_json,response_body);
    response.body = response_body;
}

void AIChatServer::HandleRemoveSession(const httplib::Request &request, httplib::Response &response)
{
    std::string ssid = request.path_params.at("ssid");

    bool ret = ai_sdk_.RemoveSession(ssid);
    if(!ret)
    {
        HandleError("会话不存在", response);
        return ;
    }
    Json::Value response_json;
    response_json["code"] = 0;
    response_json["msg"] = "success";

    std::string response_body;
    serialize(response_json,response_body);
    response.body = response_body;
}

void AIChatServer::HandleGetSessionAllMessage(const httplib::Request &request, httplib::Response &response)
{
    std::string ssid = request.path_params.at("ssid");

    auto session = ai_sdk_.GetSession(ssid);
    if(!session)
    {
        HandleError("会话不存在", response);
        return ;
    }

    Json::Value messages;
    for(auto &message : session->messages)
    {
        Json::Value tmp;
        tmp["mid"] = message.mid;
        tmp["role"] = message.role;
        tmp["content"] = message.content;
        tmp["create_time"] = message.create_time;
        
        messages.append(tmp);
    }

    Json::Value response_json;
    response_json["code"] = 0;
    response_json["msg"] = "success";
    response_json["data"]["session_id"] = session->session_id;
    response_json["data"]["model_name"] = session->model_name;
    response_json["data"]["messages"] = messages;

    std::string response_body;
    serialize(response_json,response_body);
    response.body = response_body;
}

void AIChatServer::HandleSendMessage(const httplib::Request &request, httplib::Response &response)
{
    std::string ssid = request.path_params.at("ssid");

    auto session = ai_sdk_.GetSession(ssid);
    if(!session)
    {
        HandleError("会话不存在", response);
        return ;
    }
    auto body_str = request.body;
    Json::Value body_json;
    if(!unserialize(body_str , body_json))
    {
        HandleError("反序列化错误", response);
        return ;
    }
    std::string content = body_json["content"].asString();
    if(content.empty())
    {
        HandleError("消息为空", response);
        return;
    }
    
    auto ret = ai_sdk_.SendMessage(ssid , content);
    if(ret.empty())
    {
        HandleError("模型未返回消息", response);
        return ;
    }
    Json::Value response_json;
    response_json["code"] = 0;
    response_json["msg"] = "success";
    response_json["data"]["content"] = ret;

    std::string response_body;
    serialize(response_json,response_body);
    response.body = response_body;
}

void AIChatServer::HandleSendMessageStream(const httplib::Request &request, httplib::Response &response)
{
    std::string ssid = request.path_params.at("ssid");

    auto session = ai_sdk_.GetSession(ssid);
    if(!session)
    {
        HandleError("会话不存在", response);
        return ;
    }
    auto body_str = request.body;
    Json::Value body_json;
    if(!unserialize(body_str , body_json))
    {
        HandleError("反序列化错误", response);
        return ;
    }
    std::string content = body_json["content"].asString();

    response.set_header("Content-Type", "text/event-stream");
    response.set_header("Cache-Control", "no-cache");
    response.set_header("Connection", "keep-alive");

    response.set_chunked_content_provider("text/event-stream" , [&, ssid , content](size_t offset, httplib::DataSink &sink){

        auto message_callback = [&](const std::string& data , bool finish)
        {
            std::cout << data << '\n';
            if(!finish && data.empty())
                return true;
            Json::Value data_json;
            std::string body;
            data_json["content"] = data;
            data_json["finish"] = finish;
            serialize(data_json, body);

            std::string sse_data = "data: " + body + "\n\n";

            sink.write(sse_data.c_str() , sse_data.size());

            if(finish)
            {
                sink.done();
                return false;
            }
            return true;
        };

        message_callback("" , false);
        ai_sdk_.SendMessageStream(ssid , content , message_callback);

        return false;
    });
    return ;
}


void AIChatServer::SetRouter()
{
    // 用户相关
    server_.Post("/api/verification", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleGetVerification(req, res); 
    });
    
    server_.Post("/api/register", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleRegister(req, res);
    });
    
    server_.Post("/api/login", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleLogin(req, res);
    });

    // 模型
    server_.Get("/api/models", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleGetModels(req, res);
    });

    // 会话
    server_.Get("/api/users/:uid/sessions", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleGetUserSessions(req, res);
    });
    
    server_.Post("/api/sessions", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleCreateSession(req, res);
    });
    
    server_.Delete("/api/sessions/:ssid", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleRemoveSession(req, res);
    });

    // 消息（待实现的 3 个）
    server_.Get("/api/sessions/:ssid/messages", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleGetSessionAllMessage(req, res);
    });
    
    server_.Post("/api/sessions/:ssid/messages", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleSendMessage(req, res);
    });
    
    server_.Post("/api/sessions/:ssid/messages/stream", [this](const httplib::Request& req, httplib::Response& res) {
        this->HandleSendMessageStream(req, res);
    });


    return ;
}

void AIChatServer::RegisterModels(const std::vector<ai_sdk::Config>& configs)
{
    for(auto& config : configs)
    {
        ai_sdk_.RegisterModel(config);
    }
}

void AIChatServer::SetWebRoot(const std::string &mount_point, const std::string &web_dir)
{
    // 解析真实路径，去除 .. 等相对路径
    namespace fs = std::filesystem;
    web_dir_ = fs::canonical(fs::path(web_dir)).string();
    mount_point_ = mount_point;
}

void AIChatServer::Run(const std::string &ip , uint16_t port)
{
    SetRouter();

    // 在 API 路由注册之后再挂载静态文件目录，确保 API 路由优先匹配
    if (!web_dir_.empty()) {
        server_.set_mount_point(mount_point_, web_dir_);
    }

    std::thread th([&,ip , port]{
        server_.listen(ip , port);
    });

    th.detach();
}
