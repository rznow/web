#include "service/PostService.h"
#include "service/RedisService.h"
#include "mysql/MySQL.h"
#include "common/Post.h"
#include "common/Comment.h"
#include "common/PostCache.h"
#include <iostream>
#include <unordered_set>
using namespace std::chrono_literals;

PostService::PostService():
    cache(PostCache::getInstance()),
    pool(MySQLPool::getInstance()){
    // std::cout << "PostService create " << this << std::endl;


    flushThread = std::thread([this]{
        
        // std::cout << "flush thread start " << this << std::endl;

        while(running)
        {
            std::this_thread::sleep_for(30s);
            flush();
        }
    });

    onlineUserThread = std::thread([this]{
        auto &redis = RedisService::getInstance();
        while(running)
        {
            std::this_thread::sleep_for(30s);
            // std::cout<<" 清理下线用户 "<<std::endl;
            redis.clearOfflineUsers();
        }


    });

    initPostIndex();
}

PostService::~PostService(){
    running = false;
    if (flushThread.joinable()) {
        flushThread.join();
    }
}

PostService& PostService::getInstance()
{
    static PostService ps;
    return ps;
}

void PostService::initPostIndex()
{
    auto& redis = RedisService::getInstance();

    if (redis.existPostIndex())
        return;

    auto mysql = pool.getConnection();

    std::unordered_map<std::string, std::string> members;

    mysql->getAllPostIds(members);

    if (members.empty())
    {
        return;
    }

    redis.createPostIndex(members);
}

int PostService::login(UserInfo& user, std::string name, std::string password)
{
    auto mysql = pool.getConnection();
    int result = mysql->loginSQL(name, password, user);

    if(result == 1)
    {
        RedisService::getInstance().setUser(user);
        RedisService::getInstance().updateOnline(user.user_id);
    }
    return result;
}

//MySQL → Redis → PostCache
void PostService::put(Post p)
{
    auto mysql = pool.getConnection();

    mysql->savePost(p);
    
    RedisService::getInstance().setPost(p);
    // RedisService::getInstance().delPostPage();

    PostCache::getInstance().put(p);
}

//MySQL → Redis → PostCache
void PostService::put(Comment& c)
{
    auto mysql = pool.getConnection();

    mysql->saveComment(c);

    auto& redis = RedisService::getInstance();
    redis.addDirty(c.post_id);

    redis.setComment(c);

    PostCache::getInstance().update(c);
}

//PostCache → Redis → MySQL
bool PostService::get(int post_id, Post& p)
{
    auto& redis = RedisService::getInstance();

    if(PostCache::getInstance().get(post_id, p))
    {
        // std::cout<<"Post from postCache!"<<std::endl;
        return true;
    }

    if(redis.getPost(post_id, p))
    {
        // std::cout<<"Post from redis!"<<std::endl;
        PostCache::getInstance().put(p);
        return true;
    }

    auto mysql = pool.getConnection();

    if(mysql->getPost(post_id, p))
    {
        // std::cout<<"Post from mysql!"<<std::endl;
        PostCache::getInstance().put(p);
        redis.inPost(p);

        std::vector<int> likes;
        mysql->getLikes(post_id, likes);

        for(const auto &i:likes)
        {
            redis.addLikeUser(post_id, i);
        }

        return true;
    }
    
    return false;
}

//MySQL → Redis → PostCache
bool PostService::like(int post_id, int user_id)
{
    auto& redis = RedisService::getInstance();
    auto mysql = pool.getConnection();
    bool f = liked(post_id, user_id);
    mysql->like(post_id, user_id, f);
    redis.addDirty(post_id);
    if(f)
    {
        f = false;
        redis.removeLikeUser(post_id, user_id);
        redis.decrLike(post_id);
    }else
    {
        f = true;
        redis.addLikeUser(post_id, user_id);
        redis.incrLike(post_id);
    }
    PostCache::getInstance().update(post_id, f);

    return f;
}

//PostCache → Redis → MySQL
int PostService::likes(int post_id)
{
    auto& redis = RedisService::getInstance();

    int like_count = 0;
    if(PostCache::getInstance().getLikes(post_id, like_count)||
        redis.getLikes(post_id, like_count))
    {
        auto mysql = pool.getConnection();
        mysql->likes(post_id, like_count);
    }

    return like_count;
}


//Redis -> MySQL()
bool PostService::liked(int post_id, int user_id)               //是否点赞
{
    if(RedisService::getInstance().hasLiked(post_id, user_id))
    {
        return true;
    }

    // auto mysql = pool.getConnection();
    // if(mysql->liked(post_id, user_id))
    // {
    //     RedisService::getInstance().addLikeUser(post_id, user_id);
    //     return true;
    // }

    return false;
}

//PostCache -> Redis -> Mysql
std::vector<Post> PostService::getPosts(size_t page, size_t size)
{
    std::vector<Post> posts;
    std::vector<int> posts_id;
    posts_id.reserve(size);

    auto &redis = RedisService::getInstance();
    
    if(redis.getPostPage(page, size, posts_id))
    {
        posts.resize(posts_id.size());
        std::vector<int> missPos;
        // getPosts(posts_id, posts, missPos);
        redis.getPostsPipeline(posts_id, posts, missPos);
        std::vector<int> missIds;
        for(auto pos : missPos)
        {
            missIds.push_back(posts_id[pos]);
        }
        if(missIds.size() > 0)
        {
            auto mysql = pool.getConnection();
            mysql->getPosts(missIds, posts, missPos);
            for(auto i: missPos)
            {
                redis.inPost(posts[i]);
            }
        }
    }else
    {
        auto mysql = pool.getConnection();
        mysql->getPosts(posts, size, (page - 1) * size);
        RedisService::getInstance().setPosts(posts);
    }
    for(auto &p:posts)
        PostCache::getInstance().put(p);

    return posts;
}

bool PostService::getPosts(
    const std::vector<int>& ids,
    std::vector<Post>& posts,
    std::vector<int>& missPos) const
{
    auto &redis = RedisService::getInstance();
    auto &cache = PostCache::getInstance();
    for(size_t i = 0; i < ids.size(); i++)
    {
        Post p;
        if(cache.get(ids[i], p) || redis.getPost(ids[i], p))
        {
            posts[i] = std::move(p);
        }else   missPos.emplace_back(i);
    }
    return true;
}

//Redis -> Mysql
std::vector<int> PostService::getRootComments(size_t post_id, size_t page, size_t size)
{
    std::vector<int> comments_id;
    if(getPostCommentCount(post_id) <= 0)
    {
        return comments_id;
    }
    comments_id.reserve(size);
    auto& redis = RedisService::getInstance();
    if(redis.getComments(post_id, page, size, comments_id))
    {
        return comments_id;
    }

    {
        auto mysql = pool.getConnection();

        mysql->getRootComments(
            comments_id,
            post_id,
            size,
            (page-1)*size);
    }
    return comments_id;
}
// std::vector<Comment> PostService::getRootComments(size_t post_id, size_t page, size_t size)
// {
//     auto& redis = RedisService::getInstance();

//     std::vector<Comment> comments;
//     std::vector<int> comments_id;
//     if(getPostCommentCount(post_id) <= 0)
//     {
//         return comments;
//     }
//     comments_id.reserve(size);

//     if(redis.getComments(post_id, page, size, comments_id))
//     {
//         comments.resize(comments_id.size());
//         std::vector<int> missCom;
//         getComments(comments_id, comments, missCom);
//         std::vector<int> missIds;
//         for(auto com : missCom)
//         {
//             missIds.push_back(comments_id[com]);
//         }
//         if(missIds.size() > 0)
//         {
//             auto mysql = pool.getConnection();
//             mysql->getComments(missIds, comments, missCom);
//         }
//     }else
//     {
//         auto mysql = pool.getConnection();

//         mysql->getRootComments(
//             comments,
//             post_id,
//             size,
//             (page-1)*size);

//         redis.setComments(post_id, page, size, comments);
//     }
//     return comments;
// }

bool PostService::getComments(
    const std::vector<int>& ids,
    std::vector<Comment>& comments,
    std::vector<int>& missCom) const
{
    auto &redis = RedisService::getInstance();
    for(size_t i = 0; i < ids.size(); i++)
    {
        Comment c;
        if(redis.getComment(ids[i], c))
        {
            comments[i] = std::move(c);
        }else   missCom.emplace_back(i);
    }
    return true;
}

//Redis -> Mysql
std::vector<Comment> PostService::getComments(size_t post_id, std::vector<int>& rootComments)
{
    auto& redis = RedisService::getInstance();

    std::vector<Comment> comments;
    std::vector<int> comments_id;
    if(getPostCommentCount(post_id) <= 0)
    {
        return comments;
    }
    if(redis.existPostCommentIndex(post_id))
    {
        for(auto &i:rootComments)
        {
            comments_id.push_back(i);
            redis.getChildComments(i, comments_id);   
        }
        comments.resize(comments_id.size());
        std::vector<int> missCom;
        // getComments(comments_id, comments, missCom);
        redis.getCommentsPipeline(comments_id, comments, missCom);
        std::vector<int> missIds;
        for(auto com : missCom)
        {
            missIds.push_back(comments_id[com]);
        }
        if(missIds.size() > 0)
        {
            auto mysql = pool.getConnection();
            mysql->getComments(missIds, comments, missCom);
            for(auto i:missCom) 
            {
                redis.setComment(comments[i]);
            }
        }

    }else{
        auto mysql = pool.getConnection();

        mysql->getComments(comments, post_id, rootComments);

        redis.setComments(post_id, comments);
    }
    return comments;
}

//MySQL → Redis → PostCache
bool PostService::delPost(size_t post_id)
{
    auto mysql = pool.getConnection();

    bool res = mysql->delPost(post_id);

    RedisService::getInstance().delPost(post_id);
    // RedisService::getInstance().delPostPage();

    PostCache::getInstance().erase(post_id);

    return res;
}

//MySQL → Redis → PostCache
void PostService::modifyView(size_t post_id,size_t user_id)
{
    // auto mysql = pool.getConnection();

    // mysql->view(post_id);
    auto& redis = RedisService::getInstance();
    if(!redis.existPostView(post_id, user_id))
    {
        redis.addDirty(post_id);

        redis.incrView(post_id);

        PostCache::getInstance().update(post_id);

        redis.setPostView(post_id, user_id);
    }
    
}

//MySQL → Redis → PostCache
int PostService::modPost(size_t post_id, size_t user_id, std::string& title, std::string& content)
{
    auto mysql = pool.getConnection();

    if(checkPost(post_id, user_id))
    {
        if(mysql->modPost(post_id, title, content))
        {
            RedisService::getInstance().updatePost(post_id, title, content);
            PostCache::getInstance().update(post_id, title, content);
            return 0;
        }
        return 2; //修改失败
    }
    return 1;  //帖子并非当前用户所有
}

//PostCache → Redis → MySQL
bool PostService::checkPost(size_t post_id, size_t user_id)         //判断帖子是否为本人的
{
    /*
    
    PostCache
    
    */
    if(RedisService::getInstance().checkPost(post_id, user_id))
    {
        return true;
    }

    auto mysql = pool.getConnection();
    return mysql->checkPost(post_id, user_id);
}

void PostService::flush()       //定时更新点赞和浏览
{
    auto& redis = RedisService::getInstance();
    auto mysql = pool.getConnection();
    std::string dirtyPost;
    std::unordered_set<std::string> s;
    int idx = 0;
    while(redis.getDirty(dirtyPost)&&idx++<50)
    {
        if(s.find(dirtyPost) != s.end()) continue;
        s.emplace(dirtyPost);
        int post_id = std::stoi(dirtyPost);
        std::unordered_map<std::string, std::string> fields;
        redis.getViewLikeComment(post_id, fields);
        std::unordered_map<std::string, std::string> mysql_fields;
        for(auto &[key, value] : fields)
        {
            mysql_fields[key+"_count"] = value;
        }
        mysql->load(post_id, mysql_fields);
    }
}

int PostService::getPostCount(size_t user_id)
{
    auto &redis = RedisService::getInstance();

    int count = 0;
    if(redis.getPostCount(user_id, count))
    {
        return count;
    }

    auto mysql = pool.getConnection();
    if(mysql->getPostCount(user_id, count))
    {
        redis.setPostCount(user_id, count);
    }
    return count;
}

int PostService::getPostCommentCount(size_t post_id)
{
    auto &redis = RedisService::getInstance();

    int count = 0;
    Post p;
    if(PostCache::getInstance().get(post_id, p))
    {
        return p.comment_count;
    }

    if(redis.getPost(post_id, p))
    {
        return p.comment_count;
    }

    auto mysql = pool.getConnection();
    if(mysql->getPost(post_id, p))
    {
        PostCache::getInstance().put(p);
        redis.inPost(p);
        return p.comment_count;
    }
    return count;
}

int PostService::getCommentCount(size_t user_id)
{
    auto &redis = RedisService::getInstance();

    int count = 0;
    if(redis.getCommentCount(user_id, count))
    {
        return count;
    }

    auto mysql = pool.getConnection();
    if(mysql->getCommentCount(user_id, count))
    {
        redis.setCommentCount(user_id, count);
    }
    return count;
}

int PostService::getLikeCount(size_t user_id)
{
    auto &redis = RedisService::getInstance();

    int count = 0;
    if(redis.getLikeCount(user_id, count))
    {
        return count;
    }

    auto mysql = pool.getConnection();
    if(mysql->getLikeCount(user_id, count))
    {
        redis.setLikeCount(user_id, count);
    }
    return count;
}

// std::string PostService::getCreateTime(size_t user_id)
// {
//     auto &redis = RedisService::getInstance();

//     std::string time;

//     if(redis.getCreateTime(user_id, time))
//         return time;
    
//     auto mysql = pool.getConnection();
//     if(mysql->getCreateTime(user_id, time))
//     {
//         redis.setCreateTime(user_id, time);
//     }
//     return time;
// }

//MySQL -> Redis
bool PostService::updateAvatar(int user_id, const std::string& avatarUrl)
{
    auto mysql = pool.getConnection();

    mysql->modAvatar(user_id, avatarUrl);

    RedisService::getInstance().delUser(user_id);

    return true;
}
//Redis -> MySQL
std::string PostService::getAvatar(int user_id)
{
    std::string avatar = "/images/default_avatar.png";
    if(RedisService::getInstance().getAvatar(user_id, avatar))
    {
        return avatar;
    }

    auto mysql = pool.getConnection();
    mysql->getAvatar(user_id, avatar);

    return avatar;
}

bool PostService::getUserStat(int user_id, UserStat& stat)
{
    auto &redis = RedisService::getInstance();

    if(redis.getUserStat(user_id, stat))
    {
        return true;
    }

    auto mysql = pool.getConnection();
    mysql->getUserStat(user_id, stat);

    redis.setUserStat(user_id, stat);

    return true;
}