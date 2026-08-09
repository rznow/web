/**
 * @file HttpServer.cpp
 * @brief 处理Http请求(HttpRequest),生成Http响应
 *
 * 实现请求解析、响应生成等功能。
 *
 * @author rznow
 * @date 2026-06-10
 */

#include "http/HttpServer.h"
#include "http/HttpResponse.h"
#include "http/HttpRequest.h"
#include "http/MultipartParser.h"
#include "mysql/MySQL.h"
#include "mysql/MySQLPool.h"
#include "JWT.h"
#include "common/UserInfo.h"
#include "common/Post.h"
#include "common/Comment.h"
#include "service/PostService.h"
#include "service/RedisService.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <chrono>

using Clock = std::chrono::steady_clock;

using json = nlohmann::json;
//读文件
std::string readFile(std::string& path)
{
    std::ifstream ifs(path, std::ios::binary);

    if (!ifs.is_open())
    {
        path = "www/404.html";
        return readFile(path);
    }
    //拷贝整个文件
    return std::string(
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>()
    );
}

void saveFile(std::string& path, const std::string& content)
{
    // 保存二进制内容
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(content.data(), content.size());
    ofs.close();
}

std::string getContentType(std::string path)
{
    if(path.ends_with(".html"))
        return "text/html";

    if(path.ends_with(".css"))
        return "text/css";

    if(path.ends_with(".js"))
        return "application/javascript";

    if(path.ends_with(".png"))
        return "image/png";

    if(path.ends_with(".jpg"))
        return "image/jpeg";

    if(path.ends_with(".jpeg"))
        return "image/jpeg";

    return "text/plain";
}

json buildComment(Comment* c)
{
    json obj;

    obj["comment_id"] = c->comment_id;
    obj["post_id"] = c->post_id;
    obj["user_id"] = c->user_id;

    obj["parent_id"] = c->parent_id;
    obj["root_comment_id"] = c->root_comment_id;
    obj["reply_user_id"] = c->reply_user_id;

    obj["author"] = c->author;
    obj["reply_author"] = c->reply_author;
    obj["avatar"] = c->avatar;
    obj["content"] = c->content;
    obj["time"] = c->create_time;

    obj["children"] = json::array();
    for(auto child : c->children)
    {
        obj["children"].push_back(buildComment(child));
    }
    return obj;
}

HttpResponse HttpServer::handleRequest(const HttpRequest& request)
{
    if(request.getMethod() == "GET")
        return handleGet(request);
    else if(request.getMethod() == "POST")
    {
        return handlePost(request);
    }else if(request.getMethod() == "DELETE")
    {
        return handleDel(request);
    }else if(request.getMethod() == "PUT")
    {
        return handlePut(request);
    }
    HttpResponse resp;
    resp.setStatus(405, "Method Not Allowed");
    resp.setHeader("Content-Type", "text/plain");
    resp.setHeader("Connection", "keep-alive");
    resp.setBody("Unsupported HTTP Method");

    return resp;
}

HttpResponse HttpServer::handleGet(const HttpRequest& request)
{
    std::string path = request.getPath();

    if(path == "/profile")
    {
        return profile(request);
    }else if(path == "/siteInfo")
    {
        return siteInfo(request);
    }
    else if(path.starts_with("/posts"))
    {
        if(path.starts_with("/posts?"))
            return posts(request);
        return comments(request);
    }else if(path.starts_with("/post.html"))
    {
        return index(request);
    }else if(path.starts_with("/post"))
    {
        return post(request);
    }

    return index(request);
}
HttpResponse HttpServer::handlePost(const HttpRequest& request)
{
    std::string path = request.getPath();
    if(path == "/login")   return login(request);
    else if(path == "/register") return registerUser(request);
    else if(path.starts_with("/post")) 
    {
        if(path.ends_with("/like")) return post_like(request);
        else if(path.ends_with("/comments")) return commentCreate(request);
        return postCreate(request);
    }else if(path == "/avatar")
    {
        return avatar(request);
    }
    
    HttpResponse resp;
    resp.setStatus(404, "Not Found");
    resp.setHeader("Content-Type", "application/json");
    resp.setHeader("Connection", "keep-alive");
    resp.setBody(R"({"code":404,"msg":"route not found"})");
    return resp;
}

HttpResponse HttpServer::handleDel(const HttpRequest& request)
{
    //服务器回应
    HttpResponse resp;
    json j;

    if(!request.verify())
    {
        return HttpResponse::JsonResponse({
            {"code", 1003},
            {"msg", "token invalid"}
        });
    }
    UserInfo user = request.getUser();
    int id = std::stoi(request.getPath().substr(7));
    RedisService::getInstance().expireUser(user);
    RedisService::getInstance().updateOnline(user.user_id);
    if(PostService::getInstance().delPost(id))
    {
        j["code"] = 0;
        j["user_id"] = user.user_id;
        j["msg"] = "delete success";
    }else
    {
        j["code"] = 1001;
        j["user_id"] = user.user_id;
        j["msg"] = "delete fail";
    }
    return HttpResponse::JsonResponse(j);
}

HttpResponse HttpServer::handlePut(const HttpRequest& request)
{
    //服务器回应
    HttpResponse resp;
    json j;

    std::string auth = request.getHeader("Authorization");
    if(auth.starts_with("Bearer "))
    {
        auth = auth.substr(7);
    }

    if(!request.verify())
    {
        return HttpResponse::JsonResponse({
            {"code", 1003},
            {"msg", "token invalid"}
        });
    }
    UserInfo user = request.getUser();

    int id = std::stoi(request.getPath().substr(7));
    RedisService::getInstance().expireUser(user);
    RedisService::getInstance().updateOnline(user.user_id);
    json data = json::parse(request.getBody());
    std::string title = data["title"];
    std::string content = data["content"];
    int res = PostService::getInstance().modPost(id, user.user_id, title, content);
    if(res == 0)
    {
        j["code"] = 0;
        j["msg"] = "modify success";
    }else if(res == 1)
    {
        j["code"] = 1004;
        j["msg"] = "no permission to modify the post";
    }else
    {

    }
    std::string body = j.dump();
    resp.setStatus(200, "OK");
    resp.setHeader("Content-Type", "application/json");
    resp.setHeader("Connection", "keep-alive");
    resp.setHeader("Content-Length", std::to_string(body.size()));
    resp.setBody(body);
    // std::cout<<j.dump()<<std::endl<<std::endl;
    return resp;
}

HttpResponse HttpServer::login(const HttpRequest& request)
{
    // std::unordered_map<std::string, std::string> kv;

    // std::string body = request.getBody();

    // size_t start = 0;
    // while(start < body.size())
    // {
    //     size_t eq = body.find('=', start);
    //     size_t amp = body.find('&', start);

    //     if(eq == std::string::npos)
    //         break;

    //     std::string key = body.substr(start, eq - start);

    //     std::string value;
    //     if(amp == std::string::npos)
    //     {
    //         value = body.substr(eq + 1);
    //         kv[key] = value;
    //         break;
    //     }
    //     else
    //     {
    //         value = body.substr(eq + 1, amp - eq - 1);
    //         kv[key] = value;
    //         start = amp + 1;
    //     }
    // }

    // std::string name = kv["username"];
    // std::string password = kv["password"];
    json data = json::parse(request.getBody());
    std::string name = data["username"];
    std::string password = data["password"];


    // std::cout<<" username: "<<name<<std::endl;
    // std::cout<<" password: "<<password<<std::endl;

    int result;
    UserInfo user;

    result = PostService::getInstance().login(user, name, password);
    // {
        // auto mysql = MySQLPool::getInstance().getConnection();
        // result = mysql->loginSQL(name, password, user);
        // MySQL mysql;
        // mysql.connect();
        // result = mysql.loginSQL(name, password);
    // }

    HttpResponse resp;
    json j;
    if(result == 1)
    {
        auto token = JWT::createToken(user);
        resp.setStatus(200, "OK");
        j["code"] = 0;
        j["msg"] = "login success";
        j["token"] = token;
        RedisService::getInstance().updateOnline(user.user_id);
    }
    else if(result == 0)
    {
        resp.setStatus(401, "Unauthorized");
        j["code"] = 1002;
        j["msg"] = "wrong password";
        // body = R"({"code":1002,"msg":"wrong password"})";
    }
    else
    {
        resp.setStatus(404, "Unauthorized");
        j["code"] = 1001;
        j["msg"] = "user not exist";
        // body = R"({"code":1001,"msg":"user not exist"})";
    }
    std::string body = j.dump();
    resp.setBody(body);
    resp.setHeader("Content-Type", "application/json");
    resp.setHeader("Connection", "keep-alive");
    resp.setHeader("Content-Length", std::to_string(body.size()));
    return resp;
}

HttpResponse HttpServer::registerUser(const HttpRequest& request)
{
    // std::unordered_map<std::string, std::string> kv;

    // std::string body = request.getBody();

    // size_t start = 2;
    // while(start < body.size())
    // {
    //     size_t eq = body.find(R"(":")", start);
    //     size_t amp = body.find(R"(",")", start);

    //     if(eq == std::string::npos)
    //         break;

    //     std::string key = body.substr(start, eq - start);

    //     std::string value;
    //     if(amp == std::string::npos)
    //     {
    //         value = body.substr(eq + 3, body.size()-2-eq-3);
    //         kv[key] = value;
    //         break;
    //     }
    //     else
    //     {
    //         value = body.substr(eq + 3, amp - eq - 3);
    //         kv[key] = value;
    //         start = amp + 3;
    //     }
    // }

    // std::string name = kv["username"];
    // std::string password = kv["password"];
    // std::string confirm_password = kv["confirm_password"];
    json data = json::parse(request.getBody());
    std::string name = data.value("username", "");
    std::string password = data.value("password", "");
    // std::cout<<" username: "<<name<<std::endl;
    // std::cout<<" password: "<<password<<std::endl;

    int result;
    HttpResponse resp;

    auto mysql = MySQLPool::getInstance().getConnection();
    result = mysql->registerSQL(name, password);

    json j;
    if(result == 1)
    {
        resp.setStatus(200, "OK");
        // body = R"({"code":0,"msg":"register success"})";
        j["code"] = 0;
        j["msg"] = "register success";
    }
    else if(result == 0)
    {
        resp.setStatus(401, "Unauthorized");
        // body = R"({"code":1003,"msg":"用户已存在"})";
        j["code"] = 1003;
        j["msg"] = "用户已存在";
    }
    std::string body = j.dump();
    resp.setBody(body);
    resp.setHeader("Content-Type", "application/json");
    resp.setHeader("Connection", "keep-alive");
    resp.setHeader("Content-Length", std::to_string(body.size()));
    return resp;

}

HttpResponse HttpServer::avatar(const HttpRequest& request)
{
    MultipartParser parser;
    json j;
    if (!parser.parse(request))
    {
        return HttpResponse::JsonResponse({
            {"code",-1},
            {"msg","上传头像失败"}
        });
    }

    UploadFile avatar = parser.getFile("avatar");

    if (avatar.filename.empty())
    {
        return HttpResponse::JsonResponse({
            {"code",-1},
            {"msg","未找到头像"}
        });
    }

    if(!request.verify())
    {
        return HttpResponse::JsonResponse({
            {"code", 1003},
            {"msg", "token invalid"}
        });
    }
    UserInfo user = request.getUser();
    RedisService::getInstance().updateOnline(user.user_id);
    int user_id = user.user_id;
    // 生成保存文件名
    std::string ext = ".png";
    auto pos = avatar.filename.find_last_of('.');
    if (pos != std::string::npos)
        ext = avatar.filename.substr(pos);

    std::string filename =
        std::to_string(user_id) + ext;

    std::string path =
        "./www/upload/avatar/" + filename;

    saveFile(path, avatar.content);
    // 数据库存储的是访问路径
    std::string avatarUrl =
        "/upload/avatar/" + filename;

    PostService::getInstance().updateAvatar(user_id, avatarUrl);
    j["code"] = 0;
    j["avatar"] = avatarUrl;
    return HttpResponse::JsonResponse(j);
}

HttpResponse HttpServer::post_like(const HttpRequest& request)
{
    std::string path = request.getPath();

    size_t pos = path.find("post/")+5;
    size_t end = path.find("/like");
    int post_id = std::stoi(path.substr(pos, end-pos));

    std::string auth = request.getHeader("Authorization");

    if(auth.starts_with("Bearer "))
    {
        auth = auth.substr(7);
    }
    HttpResponse resp;
    json j;
    if(!request.verify())
    {
        return HttpResponse::JsonResponse({
            {"code", 1003},
            {"msg", "token invalid"}
        });
    }
    UserInfo user = request.getUser();
    RedisService::getInstance().expireUser(user);
    RedisService::getInstance().updateOnline(user.user_id);
    bool liked = PostService::getInstance().like(post_id, user.user_id);
    int like_count = PostService::getInstance().likes(post_id);
    if(like_count != -1)
    {
        j["code"] = 0;
        j["like_count"] = like_count;
        j["liked"] = liked;
    }else
    {
        j["code"] = 1001;
        j["msg"] = "点赞失败";
    }
    std::string body = j.dump();
    resp.setStatus(200, "OK");
    resp.setHeader("Content-Type", "application/json");
    resp.setHeader("Connection", "keep-alive");
    resp.setHeader("Content-Length", std::to_string(body.size()));
    resp.setBody(body);
    // std::cout<<j.dump()<<std::endl;
    return resp;
}

HttpResponse HttpServer::postCreate(const HttpRequest& request)
{
    json j;

    if(!request.verify())
    {
        return HttpResponse::JsonResponse({
            {"code", 1003},
            {"msg", "token invalid"}
        });
    }
    UserInfo user = request.getUser();
    RedisService::getInstance().expireUser(user);
    RedisService::getInstance().updateOnline(user.user_id);
    Post p;
    p.user_id = user.user_id;
    p.author = user.user_name;

    json data = json::parse(request.getBody());
    p.title = data["title"];
    p.content = data["content"];

    PostService::getInstance().put(p);

    j["code"] = 0;
    j["msg"] = "post success";
    j["user_id"] = user.user_id;
    j["user_name"] = user.user_name;
    return HttpResponse::JsonResponse(j);

}

HttpResponse HttpServer::commentCreate(const HttpRequest& request)
{
    HttpResponse resp;
    json j;

    if(!request.verify())
    {
        return HttpResponse::JsonResponse({
            {"code", 1003},
            {"msg", "token invalid"}
        });
    }
    UserInfo user = request.getUser();
    RedisService::getInstance().expireUser(user);
    RedisService::getInstance().updateOnline(user.user_id);
    Comment c;

    
    std::string path = request.getPath();
    size_t start = path.find("/post/")+6;
    size_t end = path.find("/comments");

    c.post_id = std::stoi(path.substr(start, end));
    c.user_id = user.user_id;
    c.author  = user.user_name;

    json data = json::parse(request.getBody());

    c.content       = data.value("content","");

    c.parent_id     = data.value("parent_id",0);
    c.reply_user_id = data.value("reply_user_id",0);
    c.root_comment_id = data.value("root_comment_id",0);
    // c.print();
    PostService::getInstance().put(c);

    j["code"] = 0;
    j["msg"] = "postComment success";
    j["comment"] = buildComment(&c);
    std::string body = j.dump();
    resp.setStatus(200, "OK");
    resp.setHeader("Content-Type", "application/json");
    resp.setHeader("Connection", "keep-alive");
    resp.setHeader("Content-Length", std::to_string(body.size()));
    resp.setBody(body);
    // std::cout<<j.dump()<<std::endl<<std::endl;
    return resp;

}

HttpResponse HttpServer::index(const HttpRequest& request)
{
    std::string path = request.getPath();
    size_t pos = path.find('?');
    path = path.substr(0, pos);
    if(path == "/") path = "/index.html";
    path = "www" + path;

    // std::cout<<"Path:\t"<<path<<std::endl;
    std::string body = readFile(path);

    //服务器回应
    HttpResponse resp;
    if(path == "www/404.html")
    {
        resp.setStatus(404, "Not Found");
    }else
    {
        resp.setStatus(200, "OK");
    }
    
    resp.setHeader("Content-Type", getContentType(path));
    resp.setHeader("Connection", "keep-alive");
    resp.setHeader("Content-Length", std::to_string(body.size()));
    resp.setBody(body);

    return resp; 
}

HttpResponse HttpServer::profile(const HttpRequest& request)
{
    //服务器回应
    HttpResponse resp;
    json j;
    std::string auth = request.getHeader("Authorization");

    if(auth.starts_with("Bearer "))
    {
        auth = auth.substr(7);
    }

    if(!request.verify())
    {
        return HttpResponse::JsonResponse({
            {"code", 1003},
            {"msg", "token invalid"}
        });
    }
    UserInfo user = request.getUser();
    RedisService::getInstance().updateOnline(user.user_id);
    j["code"]           = 0;
    j["user_id"]        = user.user_id;
    j["user_name"]      = user.user_name;
    j["avatar"]         = PostService::getInstance().getAvatar(user.user_id);

    UserStat stat;
    PostService::getInstance().getUserStat(user.user_id, stat);
    j["register_time"]  = stat.create_time;
    j["post_count"]     = stat.post_count;
    j["comment_count"]  = stat.comment_count;
    j["like_count"]     = stat.like_count;

    // j["register_time"]  = PostService::getInstance().getCreateTime(user.user_id);
    // j["post_count"]     = PostService::getInstance().getPostCount(user.user_id);
    // j["comment_count"]  = PostService::getInstance().getCommentCount(user.user_id);
    // j["like_count"]     = PostService::getInstance().getLikeCount(user.user_id);

    user.avatar = j["avatar"];
    RedisService::getInstance().setUser(user);

    
    return HttpResponse::JsonResponse(j);
}

HttpResponse HttpServer::siteInfo(const HttpRequest& request)
{
    //服务器回应
    json j;

    j["code"]               = 0;
    j["online_count"]       = RedisService::getInstance().getOnlineCount();
    j["post_count"]         = RedisService::getInstance().getPostCount();

    return HttpResponse::JsonResponse(j);
}

HttpResponse HttpServer::posts(const HttpRequest& request)
{
    ///posts?page=1&size=10
    std::string path = request.getPath();
    size_t pos = 7;

    size_t start = path.find('=')+1;
    size_t end = path.find('&');
    size_t page = std::stoi(path.substr(start, end-start));
    pos = end + 1;
    start = path.find('=', pos)+1;
    size_t size = std::stoi(path.substr(start));

    // std::cout<<"page:\t"<<page<<std::endl;
    // std::cout<<"size:\t"<<size<<std::endl;

    HttpResponse resp;
    json j;

    std::vector<Post> posts = PostService::getInstance().getPosts(page, size);

    j["code"] = 0;
    json post_array = json::array();
    for(auto &i: posts)
    {
        post_array.push_back({
            {"post_id",         i.post_id},
            {"user_id",         i.user_id},
            {"author",          i.author},
            {"title",           i.title},
            {"content",         i.content},
            {"like_count",      i.like_count},
            {"comment_count",   i.comment_count},
            {"view_count",      i.view_count},
            
            
            {"time", i.create_time}
        });
    }
    j["posts"] = post_array;

    return HttpResponse::JsonResponse(j);
}

HttpResponse HttpServer::comments(const HttpRequest& request)
{
    ///posts/5/comments?page=1&size=10
    std::string path = request.getPath();
    size_t pos = 0;
    size_t start = path.find("posts/")+6;
    size_t end = path.find("/comments");
    size_t post_id = std::stoi(path.substr(start, end-start));

    start = path.find("page=")+5;
    end = path.find('&');
    size_t page = std::stoi(path.substr(start, end-start));
    start = path.find("size=", pos)+5;
    size_t size = std::stoi(path.substr(start));

    // std::cout<<"post_id:\t"<<post_id<<std::endl;
    // std::cout<<"page:\t\t"<<page<<std::endl;
    // std::cout<<"size:\t\t"<<size<<std::endl;

    HttpResponse resp;
    json j;
    // auto t1 = Clock::now();
    std::vector<int> rootComments = PostService::getInstance().getRootComments(post_id, page, size);
    // auto t2 = Clock::now();
    std::vector<Comment> comments = PostService::getInstance().getComments(post_id, rootComments);
    // auto t3 = Clock::now();

    std::unordered_map<int, Comment*> mp;

    std::vector<Comment*> roots;
    for(auto& c : comments)
    {
        mp[c.comment_id] = &c;
        if(c.parent_id == 0) 
        {
            roots.push_back(&c);
        }
    }

    for(auto& c : comments)
    {
        auto it = mp.find(c.parent_id);

        if(it != mp.end())
        {
            it->second->children.push_back(&c);
        }
    }
    // auto t4 = Clock::now();

    j["code"] = 0;
    json comment_array = json::array();
    for(auto &i: roots)
    {
        comment_array.push_back(buildComment(i));
    }

    j["comments"] = comment_array;
    // auto t5 = Clock::now();
    // std::string s=j.dump();

    // auto t6=Clock::now();
    // std::cout<<"rc: "<< t2-t1<<std::endl;
    // std::cout<<"c:  "<< t3-t2<<std::endl;
    // std::cout<<"pre: "<< t4-t3<<std::endl;
    // std::cout<<"btree: "<< t5-t4<<std::endl;
    // std::cout<<"json: "<< t6-t5<<std::endl;
    
    return HttpResponse::JsonResponse(j);
}

HttpResponse HttpServer::post(const HttpRequest& request)
{
    std::string path = request.getPath();

    size_t pos = path.find('=')+1;
    int id = std::stoi(path.substr(pos));

    HttpResponse resp;
    json j;
    Post p;

    if(!request.verify())
    {
        j["msg"] = "token invalid";
    }

    UserInfo user = request.getUser();
    RedisService::getInstance().updateOnline(user.user_id);
    if(PostService::getInstance().get(id, p))
    {
        j["code"] = 0;
        j["post"]["post_id"]        = p.post_id;
        j["post"]["user_id"]        = p.user_id;
        j["post"]["author"]         = p.author;
        j["post"]["title"]          = p.title;
        j["post"]["content"]        = p.content;
        j["post"]["like_count"]     = p.like_count;
        if(user.user_id!=0&&p.user_id != user.user_id)
        {
            PostService::getInstance().modifyView(id, user.user_id);
            j["post"]["view_count"] = p.view_count+1;
        }else 
        j["post"]["view_count"]     = p.view_count;
        j["post"]["comment_count"]  = p.comment_count;
        j["post"]["time"]           = p.create_time;
        j["post"]["liked"]          = PostService::getInstance().liked(id, user.user_id);
    }else
    {
        
        j["code"] = 1001;
        j["msg"] = "post not found";
    }
    RedisService::getInstance().expireUser(user);
    return HttpResponse::JsonResponse(j);
}