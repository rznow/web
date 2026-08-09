#ifndef POSTSERVICE_H
#define POSTSERVICE_H

#include "common/PostCache.h"
#include "common/UserInfo.h"
#include "mysql/MySQLPool.h"
#include <thread>
class Post;
class Comment;

class PostService
{
    private:
        PostCache& cache;
        MySQLPool& pool;
        bool running = true;
        std::thread flushThread;
        std::thread onlineUserThread;

        PostService();
        ~PostService();
        
    public:
        PostService(const PostService&) = delete;
        PostService& operator=(const PostService&) = delete;
        static PostService& getInstance();
        void initPostIndex();
        int login(UserInfo& user, std::string name, std::string password);
        void put(Post p);
        void put(Comment& c);
        bool get(int post_id, Post& p);
        bool like(int post_id, int user_id);
        int likes(int post_id);
        bool liked(int post_id, int user_id);

        std::vector<Post> getPosts(size_t page, size_t size); 
        bool getPosts(
            const std::vector<int>& ids,
            std::vector<Post>& posts,
            std::vector<int>& missPos) const;

        std::vector<int> getRootComments(
            size_t post_id, 
            size_t page, 
            size_t size);
        // std::vector<Comment> getRootComments(
        //     size_t post_id, 
        //     size_t page, 
        //     size_t size);
        bool getComments(
            const std::vector<int>& ids,
            std::vector<Comment>& comments,
            std::vector<int>& missCom) const;
        
        std::vector<Comment> getComments(size_t post_id, std::vector<int>& rootComments);  
        bool delPost(size_t post_id);
        void modifyView(size_t post_id, size_t user_id);
        int modPost(size_t post_id, 
            size_t user_id, 
            std::string& title, 
            std::string& content);
        bool checkPost(size_t post_id, size_t user_id);
        void flush();       //定时更新点赞和浏览
        int getPostCount(size_t user_id);
        int getPostCommentCount(size_t post_id);
        int getCommentCount(size_t user_id);
        int getLikeCount(size_t user_id);
        // std::string getCreateTime(size_t user_id);
        bool updateAvatar(int user_id, const std::string& avatarUrl);
        std::string getAvatar(int user_id);
        // std::string getAvatar(int user_id);    //获取头像
        bool getUserStat(int user_id, UserStat& stat);
};




#endif