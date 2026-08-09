#pragma once 

#include "redis/RedisPool.h"
#include "common/Post.h"
#include "common/UserInfo.h"
#include "common/Comment.h"
#include <unordered_map>

class RedisService
{
    private:
        RedisPool& pool;

        RedisService();
        ~RedisService();
        
    public:
        RedisService(const RedisService&) = delete;
        RedisService& operator=(const RedisService&) = delete;
        static RedisService& getInstance();

        bool setUser(UserInfo& u) const;
        bool updateOnline(int user_id) const;
        bool clearOfflineUsers();     //定时更新在线用户
        bool expireUser(UserInfo& u) const;
        bool delUser(int user_id) const;
        bool setCreateTime(int user_id, const std::string& time) const;
        // bool getCreateTime(int user_id, std::string& time) const;
        bool getAvatar(int user_id, std::string& avatar) const;

        bool createPostIndex(std::unordered_map<std::string, std::string>& members);
        bool setPost(const Post& p) const;
        bool inPost(const Post& p) const;
        void delPostPage() const;
        bool getPostPage(
            int page, int size, 
            std::vector<int>& posts_id) const;
        bool setPosts(const std::vector<Post>& posts) const;

        bool getPost(int post_id, Post& p) const;
        bool getPosts(
            int page, int size, 
            std::vector<Post>& posts) const;
        bool getPostsPipeline(
            const std::vector<int>& ids,
            std::vector<Post>& posts,
            std::vector<int>& missPos);

        bool getPostCount(int user_id, int& count) const;
        bool setPostCount(int user_id, const int count) const;
        bool getCommentCount(int user_id, int& count) const;
        bool setCommentCount(int user_id, const int count) const;
        bool getLikeCount(int user_id, int& count) const;
        bool setLikeCount(int user_id, const int count) const;

        bool setComment(const Comment& c) const;
        bool setComments(int post_id, const std::vector<Comment>& comments) const;
        bool setComments(
            int post_id, int page, int size, 
            const std::vector<Comment>& comments) const;
        bool getComment(int comment_id, Comment& c) const;
        bool getComments(int post_id, std::vector<int>& comments_id) const;
        bool getComments(
            int post_id, int page, int size, 
            std::vector<int>& comments_id) const;
        bool getCommentsPipeline(
            const std::vector<int>& ids,
            std::vector<Comment>& comments,
            std::vector<int>& missPos);
        bool getChildComments(
            int comment_id,
            std::vector<int>& comments_id) const;

        bool existPostIndex() const;
        bool existPostCommentIndex(int post_id) const;

        bool updatePost(
            int post_id, 
            const std::string& title,
            const std::string& content) const;

        bool checkPost(
            int post_id, 
            int user_id) const;

        bool existPostLikes(int post_id) const;

        bool delPost(int post_id) const;

        bool incrLike(int post_id) const;

        bool decrLike(int post_id) const;

        bool getLikes(int post_id, int& like_count) const;

        bool incrComment(int post_id) const;

        bool decrComment(int post_id) const;

        bool incrView(int post_id) const;

        bool addLikeUser(int post_id, int user_id) const;

        bool removeLikeUser(int post_id, int user_id) const;

        bool hasLiked(int post_id, int user_id) const;

        bool addDirty(int post_id) const;

        bool getDirty(std::string& post_id) const;

        bool getViewLikeComment(int post_id, std::unordered_map<std::string, std::string>& fields) const;

        bool setPostView(int post_id, int user_id);

        bool existPostView(int post_id, int user_id);

        size_t getOnlineCount();

        size_t getPostCount();

        bool getUserStat(int user_id, UserStat& stat) const;

        bool setUserStat(int user_id, const UserStat& stat) const;
};